#include "san_lte_redundancy/mwan3_ubus_monitor.hpp"

#include <cstddef>
#include <cstring>
#include <stdexcept>

namespace san_lte_redundancy {

namespace {

// Identity offset between an externally-owned ubus_subscriber and the
// surrounding Mwan3UbusMonitor instance. We can't use offsetof on a
// non-standard-layout class with virtual bases, but Mwan3UbusMonitor
// is plain-data + std members, so the compile-time offset is stable.
inline Mwan3UbusMonitor* subscriberOwner(struct ubus_subscriber* sub) {
    // Inline the calculation so no intermediate non-const pointer remains
    // (silences cppcheck constVariablePointer).
    return reinterpret_cast<Mwan3UbusMonitor*>(
        reinterpret_cast<std::byte*>(sub)
        - offsetof(Mwan3UbusMonitor, subscriber_));
}

}  // namespace

Mwan3UbusMonitor::Mwan3UbusMonitor(rclcpp::Logger logger)
    : logger_(logger),
      ubus_ctx_(nullptr),
      subscriber_{},
      subscribed_(false),
      wan_lte_up_(false),
      running_(true)
{
    ubus_ctx_ = ubus_connect(nullptr);
    if (ubus_ctx_ == nullptr) {
        // On a non-OpenWrt host (e.g. CI) ubus may not be available.
        // We still construct successfully so the role manager can run
        // in mock mode; isLteUp() will return the latched flag.
        RCLCPP_WARN(logger_, "ubus_connect failed - operating in offline mode");
        return;
    }

    subscriber_.cb = &Mwan3UbusMonitor::onHotplugEventTrampoline;
    if (ubus_register_subscriber(ubus_ctx_, &subscriber_) != UBUS_STATUS_OK) {
        RCLCPP_ERROR(logger_, "ubus_register_subscriber failed");
        ubus_free(ubus_ctx_);
        ubus_ctx_ = nullptr;
        return;
    }

    uint32_t obj_id = 0;
    if (ubus_lookup_id(ubus_ctx_, "mwan3", &obj_id) == UBUS_STATUS_OK) {
        if (ubus_subscribe(ubus_ctx_, &subscriber_, obj_id) == UBUS_STATUS_OK) {
            subscribed_ = true;
        } else {
            RCLCPP_WARN(logger_, "ubus_subscribe to mwan3 failed");
        }
    } else {
        RCLCPP_WARN(logger_,
                    "mwan3 ubus object not found, using sync polling only");
    }

    uloop_init();
    ubus_add_uloop(ubus_ctx_);
    uloop_thread_ = std::thread([this] { uloopThreadMain(); });

    refreshLteStatus();
}

Mwan3UbusMonitor::~Mwan3UbusMonitor() {
    running_ = false;
    uloop_end();
    if (uloop_thread_.joinable()) {
        uloop_thread_.join();
    }
    if (ubus_ctx_ != nullptr) {
        ubus_free(ubus_ctx_);
        ubus_ctx_ = nullptr;
    }
}

void Mwan3UbusMonitor::onLteStatusChange(StatusCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    status_callback_ = std::move(cb);
}

void Mwan3UbusMonitor::uloopThreadMain() {
    // uloop_run blocks until uloop_end is called from another thread
    // (~destructor). On older libubox builds it returns periodically;
    // we wrap in a loop so the thread doesn't exit prematurely.
    while (running_) {
        uloop_run();
    }
}

bool Mwan3UbusMonitor::isLteUp() {
    return wan_lte_up_.load();
}

void Mwan3UbusMonitor::refreshLteStatus() {
    if (ubus_ctx_ == nullptr) {
        return;
    }
    uint32_t obj_id = 0;
    if (ubus_lookup_id(ubus_ctx_, "mwan3", &obj_id) != UBUS_STATUS_OK) {
        return;
    }

    struct blob_buf b;
    std::memset(&b, 0, sizeof(b));
    blob_buf_init(&b, 0);

    struct Ctx { std::atomic<bool>* flag; };
    Ctx ctx{&wan_lte_up_};

    auto data_cb = +[](struct ubus_request* req, int /*type*/,
                       struct blob_attr* msg) {
        if (msg == nullptr) return;
        auto* c = static_cast<Ctx*>(req->priv);

        static const struct blobmsg_policy top_policy[] = {
            { "interfaces", BLOBMSG_TYPE_TABLE },
        };
        struct blob_attr* top[1] = {nullptr};
        blobmsg_parse(top_policy, 1, top,
                      blob_data(msg), blob_len(msg));
        if (top[0] == nullptr) return;

        struct blob_attr* iface;
        unsigned int rem;
        blobmsg_for_each_attr(iface, top[0], rem) {
            if (std::strcmp(blobmsg_name(iface), "wan_lte") != 0) {
                continue;
            }
            static const struct blobmsg_policy sub_policy[] = {
                { "status", BLOBMSG_TYPE_STRING },
            };
            struct blob_attr* sub[1] = {nullptr};
            blobmsg_parse(sub_policy, 1, sub,
                          blobmsg_data(iface), blobmsg_data_len(iface));
            if (sub[0] != nullptr) {
                const std::string status = blobmsg_get_string(sub[0]);
                c->flag->store(status == "online");
            }
        }
    };

    ubus_invoke(ubus_ctx_, obj_id, "status", b.head, data_cb, &ctx, 1000);
    blob_buf_free(&b);
}

bool Mwan3UbusMonitor::reloadMwan3Service() {
    if (ubus_ctx_ == nullptr) {
        // Offline mode (tests on non-OpenWrt host) - treat as success.
        return true;
    }
    uint32_t obj_id = 0;
    if (ubus_lookup_id(ubus_ctx_, "service", &obj_id) != UBUS_STATUS_OK) {
        RCLCPP_ERROR(logger_, "procd 'service' ubus object missing");
        return false;
    }
    struct blob_buf b;
    std::memset(&b, 0, sizeof(b));
    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "name", "mwan3");
    blobmsg_add_string(&b, "action", "reload");

    int rc = ubus_invoke(ubus_ctx_, obj_id, "event", b.head,
                         nullptr, nullptr, 1000);
    blob_buf_free(&b);

    if (rc != UBUS_STATUS_OK) {
        RCLCPP_ERROR(logger_, "ubus service.event reload mwan3 failed: %d", rc);
        return false;
    }
    return true;
}

void Mwan3UbusMonitor::injectStatusEventForTest(const std::string& interface,
                                                bool is_up)
{
    if (interface == "wan_lte") {
        wan_lte_up_.store(is_up);
    }
    dispatchStatusEvent(interface, is_up);
}

void Mwan3UbusMonitor::dispatchStatusEvent(const std::string& interface,
                                           bool is_up)
{
    StatusCallback cb;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        cb = status_callback_;
    }
    if (cb) {
        cb(interface, is_up);
    }
}

int Mwan3UbusMonitor::onHotplugEventTrampoline(struct ubus_context* /*ctx*/,
                                               struct ubus_object* obj,
                                               struct ubus_request_data*,
                                               const char* method,
                                               struct blob_attr* msg)
{
    if (method == nullptr || msg == nullptr) {
        return 0;
    }
    if (std::strcmp(method, "interface_status") != 0) {
        return 0;
    }

    // The C ubus subscriber struct embeds its `obj` inline; back-derive
    // the C++ wrapper instance.
    auto* sub = reinterpret_cast<struct ubus_subscriber*>(
        reinterpret_cast<std::byte*>(obj) -
        offsetof(struct ubus_subscriber, obj));
    auto* self = subscriberOwner(sub);

    static const struct blobmsg_policy policy[] = {
        { "interface", BLOBMSG_TYPE_STRING },
        { "status",    BLOBMSG_TYPE_STRING },
    };
    struct blob_attr* tb[2] = {nullptr, nullptr};
    blobmsg_parse(policy, 2, tb, blob_data(msg), blob_len(msg));
    if (tb[0] == nullptr || tb[1] == nullptr) {
        return 0;
    }

    const std::string iface = blobmsg_get_string(tb[0]);
    const std::string status = blobmsg_get_string(tb[1]);
    const bool is_up = (status == "online");

    if (iface == "wan_lte") {
        self->wan_lte_up_.store(is_up);
    }
    self->dispatchStatusEvent(iface, is_up);
    return 0;
}

}  // namespace san_lte_redundancy
