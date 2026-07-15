#include "san_hub_comm/gstreamer_pipeline.hpp"

#include <cstring>
#include <utility>

// Header only pulled <rclcpp/logger.hpp> (type), but RCLCPP_ERROR /
// RCLCPP_WARN / RCLCPP_INFO are macros defined in <rclcpp/logging.hpp>.
#include <rclcpp/logging.hpp>

namespace san_hub_comm {

namespace {

// Convert GstMessageType to our coarser PipelineEvent classification.
// WARNING is a soft event - the pipeline keeps playing. ERROR is
// treated as fatal here; the relay node can choose to rebuild.
PipelineEvent classifyError(GstMessage* msg, std::string* detail_out) {
    GError* err = nullptr;
    gchar* dbg = nullptr;
    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
        gst_message_parse_error(msg, &err, &dbg);
    } else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_WARNING) {
        gst_message_parse_warning(msg, &err, &dbg);
    }
    std::string text;
    if (err && err->message) text = err->message;
    if (dbg) {
        if (!text.empty()) text += " | ";
        text += dbg;
    }
    if (detail_out) *detail_out = text;
    if (err) g_error_free(err);
    if (dbg) g_free(dbg);
    return (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
        ? PipelineEvent::ERROR_FATAL
        : PipelineEvent::WARNING;
}

}  // namespace

GStreamerPipeline::GStreamerPipeline(rclcpp::Logger logger,
                                     const std::string& description)
    : logger_(logger), description_(description)
{
    if (!gst_is_initialized()) {
        gst_init(nullptr, nullptr);
    }
    if (!build(description_)) {
        // Build failed - pipeline_ stays null. play() will refuse.
        RCLCPP_ERROR(logger_, "pipeline construction failed");
    }
}

GStreamerPipeline::~GStreamerPipeline() {
    teardown();
}

bool GStreamerPipeline::build(const std::string& description) {
    GError* err = nullptr;
    GstElement* p = gst_parse_launch(description.c_str(), &err);
    if (p == nullptr) {
        RCLCPP_ERROR(logger_, "gst_parse_launch failed: %s",
                     err ? err->message : "(no detail)");
        if (err) g_error_free(err);
        return false;
    }
    if (err) {
        // gst_parse_launch returns a *partial* pipeline when it
        // encounters an unknown element ("no element \"foo\"") — it
        // skips the bad element, builds the rest, and signals the
        // problem via the GError out-param. Such a pipeline is
        // misconfigured and will never function correctly, so refuse
        // it here instead of accepting a zombie. The test
        // PipelineWrapperTest.BadDescriptionDoesNotCrash relies on
        // this strict semantics.
        RCLCPP_ERROR(logger_,
                     "gst_parse_launch warning treated as fatal: %s",
                     err->message);
        g_error_free(err);
        gst_object_unref(p);
        return false;
    }
    pipeline_ = p;

    GstBus* bus = gst_element_get_bus(pipeline_);
    if (bus) {
        gst_bus_set_sync_handler(
            bus, &GStreamerPipeline::onBusMessageTrampoline, this, nullptr);
        gst_object_unref(bus);
    }
    return true;
}

void GStreamerPipeline::teardown() {
    if (pipeline_ == nullptr) return;
    gst_element_set_state(pipeline_, GST_STATE_NULL);
    gst_object_unref(pipeline_);
    pipeline_ = nullptr;
    playing_ = false;
}

bool GStreamerPipeline::play() {
    if (pipeline_ == nullptr) return false;
    GstStateChangeReturn rc =
        gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (rc == GST_STATE_CHANGE_FAILURE) {
        RCLCPP_ERROR(logger_, "set_state(PLAYING) failed");
        return false;
    }
    playing_ = true;
    return true;
}

bool GStreamerPipeline::stop() {
    if (pipeline_ == nullptr) return true;
    GstStateChangeReturn rc =
        gst_element_set_state(pipeline_, GST_STATE_NULL);
    if (rc == GST_STATE_CHANGE_FAILURE) {
        RCLCPP_ERROR(logger_, "set_state(NULL) failed");
        return false;
    }
    playing_ = false;
    // Force the state change to complete before returning so callers
    // can rely on "stopped means stopped" for the 5 s cleanup KPP.
    gst_element_get_state(pipeline_, nullptr, nullptr,
                          5 * GST_SECOND);
    return true;
}

bool GStreamerPipeline::rebuild(const std::string& description) {
    teardown();
    description_ = description;
    if (!build(description_)) return false;
    return play();
}

void GStreamerPipeline::setEventCallback(EventCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    event_cb_ = std::move(cb);
}

GstBusSyncReply GStreamerPipeline::onBusMessageTrampoline(GstBus* bus,
                                                          GstMessage* msg,
                                                          gpointer user)
{
    auto* self = static_cast<GStreamerPipeline*>(user);
    return self->onBusMessage(bus, msg);
}

GstBusSyncReply GStreamerPipeline::onBusMessage(GstBus*,
                                                GstMessage* msg)
{
    if (msg == nullptr) return GST_BUS_PASS;
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_EOS:
            fireEvent(PipelineEvent::EOS, "end of stream");
            break;
        case GST_MESSAGE_STATE_CHANGED: {
            // Only forward the top-level pipeline transition - element
            // state changes are too chatty for the relay node.
            if (GST_MESSAGE_SRC(msg) != GST_OBJECT(pipeline_)) break;
            GstState newstate;
            gst_message_parse_state_changed(msg, nullptr, &newstate, nullptr);
            if (newstate == GST_STATE_PLAYING) {
                fireEvent(PipelineEvent::PLAYING, "");
            } else if (newstate == GST_STATE_PAUSED) {
                fireEvent(PipelineEvent::PAUSED, "");
            }
            break;
        }
        case GST_MESSAGE_ERROR:
        case GST_MESSAGE_WARNING: {
            std::string detail;
            auto kind = classifyError(msg, &detail);
            fireEvent(kind, detail);
            break;
        }
        default:
            break;
    }
    return GST_BUS_PASS;
}

void GStreamerPipeline::fireEvent(PipelineEvent ev,
                                  const std::string& detail)
{
    EventCallback cb;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        cb = event_cb_;
    }
    if (cb) cb(ev, detail);
}

}  // namespace san_hub_comm
