#include <rtsp_server.hpp>
#include <string> // Required for std::string and std::to_string
#include <vector> // Required for std::vector

// Define the custom GstRTSPMediaFactory type
#define GST_TYPE_APP_SRC_DYNAMIC_MEDIA_FACTORY (app_src_dynamic_media_factory_get_type())
G_DECLARE_FINAL_TYPE(AppSrcDynamicMediaFactory, app_src_dynamic_media_factory, APP_SRC_DYNAMIC, MEDIA_FACTORY, GstRTSPMediaFactory)

// --- AppSrcDynamicMediaFactory Class Structure ---
struct _AppSrcDynamicMediaFactory {
    GstRTSPMediaFactory parent;
    rtsp_server::RTSPServerNode* node_instance; // Pointer to RTSPServerNode instance
    gchar *appsrc_name; // e.g. "imagesrc", "imagesrc1"
};

struct _AppSrcDynamicMediaFactoryClass {
    GstRTSPMediaFactoryClass parent_class;
};

G_DEFINE_TYPE(AppSrcDynamicMediaFactory, app_src_dynamic_media_factory, GST_TYPE_RTSP_MEDIA_FACTORY);

// --- Static Functions for AppSrcDynamicMediaFactory ---

static gchar *app_src_dynamic_media_factory_create_launch_string(GstRTSPMediaFactory *factory) {
    AppSrcDynamicMediaFactory *self = (AppSrcDynamicMediaFactory *)factory;
    rtsp_server::RTSPServerNode* node = self->node_instance;

    // Use standardized fixed parameters for Phase 1
    const int width = 640;
    const int height = 360;
    const uint32_t bitrate_bps = 1500000; // 1.5 Mbps baseline
    const std::string encoder_name = "mpph264enc"; // Optimized for RK3588

    // Standard fixed pipeline optimized for low latency and late-joiners
    // config-interval=-1: Critical for clients joining an already running stream
    std::string pipeline_str =
        "( appsrc name=" + std::string(self->appsrc_name) + " is-live=true block=false do-timestamp=true format=time " +
        "max-bytes=0 max-buffers=30 ! " +
        "queue leaky=downstream max-size-buffers=30 ! " +
        "videoconvert ! videoscale ! video/x-raw,format=NV12,width=" + std::to_string(width) + ",height=" + std::to_string(height) + " ! " +
        "queue leaky=downstream max-size-buffers=30 ! " +
        encoder_name + " bps=" + std::to_string(bitrate_bps) + " rc-mode=vbr gop=15 ! " +
        "h264parse config-interval=-1 ! rtph264pay name=pay0 pt=96 )";

    if (node) {
        RCLCPP_INFO(node->get_logger(), "[RTSP Server] Using FIXED Pipeline for %s: %s",
                    self->appsrc_name, pipeline_str.c_str());
    }

    return g_strdup(pipeline_str.c_str());
}

static GstElement *app_src_dynamic_media_factory_create_element(GstRTSPMediaFactory *factory, const GstRTSPUrl *url) {
    gchar *launch_string = app_src_dynamic_media_factory_create_launch_string(factory);
    GstElement *pipeline = gst_parse_launch(launch_string, NULL);
    g_free(launch_string);
    return pipeline;
}

static void app_src_dynamic_media_factory_init(AppSrcDynamicMediaFactory *factory) {
    factory->node_instance = nullptr;
    factory->appsrc_name = nullptr;
}

static void app_src_dynamic_media_factory_dispose(GObject *object) {
    AppSrcDynamicMediaFactory *self = (AppSrcDynamicMediaFactory *)object;
    g_free(self->appsrc_name); // Free the appsrc_name allocated with g_strdup
    G_OBJECT_CLASS(app_src_dynamic_media_factory_parent_class)->dispose(object);
}

static void app_src_dynamic_media_factory_class_init(AppSrcDynamicMediaFactoryClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = app_src_dynamic_media_factory_dispose;

    GstRTSPMediaFactoryClass *parent_class = GST_RTSP_MEDIA_FACTORY_CLASS(klass);
    parent_class->create_element = app_src_dynamic_media_factory_create_element;
}

// Helper function to create an instance of our factory
static AppSrcDynamicMediaFactory *app_src_dynamic_media_factory_new(rtsp_server::RTSPServerNode* node, const gchar *appsrc_name_val) {
    AppSrcDynamicMediaFactory *factory = (AppSrcDynamicMediaFactory *)g_object_new(GST_TYPE_APP_SRC_DYNAMIC_MEDIA_FACTORY, NULL);
    factory->node_instance = node;
    factory->appsrc_name = g_strdup(appsrc_name_val); // Duplicate string for ownership
    return factory;
}

namespace rtsp_server {

RTSPServerNode::RTSPServerNode(const rclcpp::NodeOptions & options) 
: rclcpp_lifecycle::LifecycleNode("rtsp_server", options)
{
    RCLCPP_INFO(this->get_logger(), "RTSPServerNode constructed. Waiting for configuration.");
}

RTSPServerNode::~RTSPServerNode() {
    // Ensure everything is stopped
    if (main_loop_ && g_main_loop_is_running(main_loop_)) {
        g_main_loop_quit(main_loop_);
    }
    if (gst_thread_.joinable()) {
        gst_thread_.join();
    }
    
    if (main_loop_) { g_main_loop_unref(main_loop_); main_loop_ = nullptr; }
    if (server_) { g_object_unref(server_); server_ = nullptr; }
    if (mounts_) { g_object_unref(mounts_); mounts_ = nullptr; }
    // Factories are usually owned by mounts, but if we hold refs, release them
    if (factory_cam0_) { factory_cam0_ = nullptr; } 
    if (factory_cam1_) { factory_cam1_ = nullptr; }

    RCLCPP_INFO(this->get_logger(), "rtsp_server destroyed");
}

CallbackReturn RTSPServerNode::on_configure(const rclcpp_lifecycle::State &) {
    RCLCPP_INFO(this->get_logger(), "Configuring RTSPServerNode...");

    InitRosCommon();
    
    // Initialize GStreamer
    // Suppress GStreamer logs (redirects to stdout/stderr which may fill syslog)
    gst_debug_set_default_threshold(GST_LEVEL_ERROR);
    gst_init(NULL, NULL);

    // Create Main Loop
    main_loop_ = g_main_loop_new(NULL, FALSE);

    // Initialize RTSP Server
    server_ = gst_rtsp_server_new();
    mounts_ = gst_rtsp_server_get_mount_points(server_); 
    
    // Set service port from parameter
    std::string port_str;
    {
        std::lock_guard<std::mutex> lock(params_mutex_);
        port_str = current_config_.rtsp_port;
    }
    g_object_set(server_, "service", port_str.c_str(), NULL);
    g_object_set(server_, "address", "0.0.0.0", NULL);

    // Initialize RTSP mount points (factories)
    init_rtsp_server();

    // Add timeout for session cleanup
    g_timeout_add_seconds(2, (GSourceFunc)session_cleanup, this);

    RCLCPP_INFO(this->get_logger(), "RTSPServerNode configured.");
    return CallbackReturn::SUCCESS;
}

CallbackReturn RTSPServerNode::on_activate(const rclcpp_lifecycle::State &) {
    RCLCPP_INFO(this->get_logger(), "Activating RTSPServerNode...");

    // Attach server to the main context (starts listening)
    m_server_source_id = gst_rtsp_server_attach(server_, NULL);
    if (m_server_source_id == 0) {
        RCLCPP_ERROR(this->get_logger(), "Failed to attach RTSP server.");
        return CallbackReturn::FAILURE;
    }

    // Start GMainLoop in a separate thread
    gst_thread_ = std::thread(&RTSPServerNode::gst_loop_thread, this);

    RCLCPP_INFO(this->get_logger(), "RTSPServerNode activated. RTSP Server running.");
    return CallbackReturn::SUCCESS;
}

CallbackReturn RTSPServerNode::on_deactivate(const rclcpp_lifecycle::State &) {
    RCLCPP_INFO(this->get_logger(), "Deactivating RTSPServerNode...");

    // Stop listening
    if (m_server_source_id > 0) {
        g_source_remove(m_server_source_id);
        m_server_source_id = 0;
    }

    // Stop GStreamer Main Loop
    if (main_loop_ && g_main_loop_is_running(main_loop_)) {
        g_main_loop_quit(main_loop_);
    }

    // Wait for thread to finish
    if (gst_thread_.joinable()) {
        gst_thread_.join();
    }

    RCLCPP_INFO(this->get_logger(), "RTSPServerNode deactivated.");
    return CallbackReturn::SUCCESS;
}

CallbackReturn RTSPServerNode::on_cleanup(const rclcpp_lifecycle::State &) {
    RCLCPP_INFO(this->get_logger(), "Cleaning up RTSPServerNode...");

    if (main_loop_) { g_main_loop_unref(main_loop_); main_loop_ = nullptr; }
    if (server_) { g_object_unref(server_); server_ = nullptr; }
    if (mounts_) { g_object_unref(mounts_); mounts_ = nullptr; }

    {
        std::lock_guard<std::mutex> lock(appsrc_mutex_);
        if (appsrc_) { gst_object_unref(appsrc_); appsrc_ = nullptr; }
    }
    {
        std::lock_guard<std::mutex> lock(appsrc_mutex2_);
        if (appsrc2_) { gst_object_unref(appsrc2_); appsrc2_ = nullptr; }
    }
    
    // Release ROS resources
    m_sub_image.reset();
    m_sub_image2.reset();
    stream_control_sub_.reset();
    rtsp_status_pub_.reset();
    status_timer_.reset();

    RCLCPP_INFO(this->get_logger(), "RTSPServerNode cleaned up.");
    return CallbackReturn::SUCCESS;
}

CallbackReturn RTSPServerNode::on_shutdown(const rclcpp_lifecycle::State & state) {
    RCLCPP_INFO(this->get_logger(), "Shutting down RTSPServerNode...");
    return on_cleanup(state);
}

CallbackReturn RTSPServerNode::on_error(const rclcpp_lifecycle::State & state) {
    RCLCPP_ERROR(this->get_logger(), "Error detected in RTSPServerNode. Cleaning up...");
    return on_cleanup(state);
}

// =========================================================================
//  InitRosCommon
// =========================================================================
void RTSPServerNode::InitRosCommon() { 
    rclcpp::QoS qos_profile(rclcpp::KeepLast(10));
    qos_profile.reliability(rclcpp::ReliabilityPolicy::BestEffort);
    qos_profile.durability(rclcpp::DurabilityPolicy::Volatile);

    // cam0 구독: human_detector 결과 영상
    m_sub_image = this->create_subscription<sensor_msgs::msg::Image>(
        "/human_detector/human/image_raw", qos_profile,
        std::bind(&RTSPServerNode::CallbackImage, this, std::placeholders::_1));

    // cam1 구독: /camera/image_raw2
    m_sub_image2 = this->create_subscription<sensor_msgs::msg::Image>(
        "/camera/image_raw2", qos_profile,
        std::bind(&RTSPServerNode::CallbackImage2, this, std::placeholders::_1));

    // Stream control subscription
    stream_control_sub_ = this->create_subscription<combat_robot_msgs::msg::StreamControlCommand>(
        "/stream_control_command", rclcpp::QoS(10),
        std::bind(&RTSPServerNode::onStreamControlCommand, this, std::placeholders::_1));

    // Declare and get parameters for stream settings
    this->declare_parameter<std::string>("rtsp_port", "8554");
    this->declare_parameter<std::string>("video_encoder", "mpph264enc");

    // Initialize struct members with parameter values
    std::lock_guard<std::mutex> lock(params_mutex_);
    current_config_.rtsp_port = this->get_parameter("rtsp_port").as_string();
    current_config_.encoder = this->get_parameter("video_encoder").as_string();

    // Create publisher for RTSP server status
    rtsp_status_pub_ = this->create_publisher<std_msgs::msg::UInt8>("/rtsp_status", 10);

    // Create a timer to publish status periodically (e.g., 1Hz)
    status_timer_ = this->create_wall_timer(
        std::chrono::seconds(1),
        std::bind(&RTSPServerNode::publish_rtsp_status, this));

    RCLCPP_INFO(this->get_logger(), "Initial RTSP server parameters:");
    RCLCPP_INFO(this->get_logger(), "  Port: %s", current_config_.rtsp_port.c_str());
    RCLCPP_INFO(this->get_logger(), "  Encoder: %s", current_config_.encoder.c_str());
}

void RTSPServerNode::publish_rtsp_status() {
    std_msgs::msg::UInt8 msg;
    if (this->get_current_state().id() == lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE &&
        stream_requested_.load()) {
        msg.data = 1;
    } else {
        msg.data = 0;
    }
    rtsp_status_pub_->publish(msg);
}

// =========================================================================
// GStreamer main loop thread
// =========================================================================
void RTSPServerNode::gst_loop_thread(){
    // This runs the GMainLoop which handles RTSP events
    g_main_loop_run(main_loop_);
}

// =========================================================================
//  Create RTSP server and mount points
// =========================================================================
void RTSPServerNode::init_rtsp_server(){
    // Ensure mounts_ is initialized. It's initialized in the constructor now.
    // server_ and mounts_ are already set up in the constructor.

    std::string port;
    {
        std::lock_guard<std::mutex> lock(params_mutex_);
        port = current_config_.rtsp_port;
    }

    // Create a dynamic factory for cam0
    factory_cam0_ = (GstRTSPMediaFactory*)app_src_dynamic_media_factory_new(this, "imagesrc");
    gst_rtsp_media_factory_set_shared(factory_cam0_, TRUE);
    gst_rtsp_media_factory_set_suspend_mode(factory_cam0_, GST_RTSP_SUSPEND_MODE_NONE);
    gst_rtsp_media_factory_set_eos_shutdown(factory_cam0_, FALSE);
    g_signal_connect(factory_cam0_, "media-configure", (GCallback)media_configure, this);
    gst_rtsp_mount_points_add_factory(mounts_, "/cam0", factory_cam0_);
    
    // Create a dynamic factory for cam1
    factory_cam1_ = (GstRTSPMediaFactory*)app_src_dynamic_media_factory_new(this, "imagesrc1");
    gst_rtsp_media_factory_set_shared(factory_cam1_, TRUE);
    gst_rtsp_media_factory_set_suspend_mode(factory_cam1_, GST_RTSP_SUSPEND_MODE_NONE);
    gst_rtsp_media_factory_set_eos_shutdown(factory_cam1_, FALSE);
    g_signal_connect(factory_cam1_, "media-configure", (GCallback)media_configure, this);
    gst_rtsp_mount_points_add_factory(mounts_, "/cam1", factory_cam1_);
    
    RCLCPP_INFO(this->get_logger(),
        "RTSP server is running on rtsp://<ip_address>:%s/cam0 and /cam1 (initial setup)",
        port.c_str());
}

// =========================================================================
//  Stream Control Callback
// =========================================================================
void RTSPServerNode::onStreamControlCommand(const combat_robot_msgs::msg::StreamControlCommand::SharedPtr msg) {
    switch (msg->stream_command) {
        case combat_robot_msgs::msg::StreamControlCommand::STREAM_START:
            stream_requested_.store(true);
            RCLCPP_INFO(this->get_logger(), "Stream command: START");
            break;
        case combat_robot_msgs::msg::StreamControlCommand::STREAM_STOP:
            stream_requested_.store(false);
            RCLCPP_INFO(this->get_logger(), "Stream command: STOP");
            break;
        default:
            break;
    }
}

// =========================================================================
//  Check and Recreate Pipelines (Timer Callback)
// =========================================================================
void RTSPServerNode::checkAndRecreatePipelines() {
    if (!pipeline_recreate_needed_.load()) {
        return;
    }

    // Cooldown: Don't recreate more than once every 500ms
    static rclcpp::Time last_recreate_time(0, 0, this->get_clock()->get_clock_type());
    if ((this->now() - last_recreate_time).seconds() < 0.5) {
        return;
    }

    std::lock_guard<std::mutex> lock(pipeline_recreate_mutex_); // Ensure only one recreation at a time
    if (!pipeline_recreate_needed_.load()) { // Re-check after acquiring lock
        return;
    }
    
    last_recreate_time = this->now();

    RCLCPP_WARN(this->get_logger(), "Recreating RTSP pipelines due to parameter change...");

    // [Step 1] Remove old factories
    if (mounts_) {
        gst_rtsp_mount_points_remove_factory(mounts_, "/cam0");
        gst_rtsp_mount_points_remove_factory(mounts_, "/cam1");
    }

    // [Step 2] Create and mount new factories
    if (factory_cam0_) { factory_cam0_ = nullptr; }
    if (factory_cam1_) { factory_cam1_ = nullptr; }

    factory_cam0_ = (GstRTSPMediaFactory*)app_src_dynamic_media_factory_new(this, "imagesrc");
    gst_rtsp_media_factory_set_shared(factory_cam0_, TRUE);
    gst_rtsp_media_factory_set_suspend_mode(factory_cam0_, GST_RTSP_SUSPEND_MODE_NONE);
    gst_rtsp_media_factory_set_eos_shutdown(factory_cam0_, FALSE);
    g_signal_connect(factory_cam0_, "media-configure", (GCallback)media_configure, this);
    gst_rtsp_mount_points_add_factory(mounts_, "/cam0", factory_cam0_);

    factory_cam1_ = (GstRTSPMediaFactory*)app_src_dynamic_media_factory_new(this, "imagesrc1");
    gst_rtsp_media_factory_set_shared(factory_cam1_, TRUE);
    gst_rtsp_media_factory_set_suspend_mode(factory_cam1_, GST_RTSP_SUSPEND_MODE_NONE);
    gst_rtsp_media_factory_set_eos_shutdown(factory_cam1_, FALSE);
    g_signal_connect(factory_cam1_, "media-configure", (GCallback)media_configure, this);
    gst_rtsp_mount_points_add_factory(mounts_, "/cam1", factory_cam1_);
    
    // [Step 3] Force Disconnect: Remove all existing sessions to force clients to reconnect to the NEW pipeline
    if (server_) {
        GstRTSPSessionPool *pool = gst_rtsp_server_get_session_pool(server_);
        if (pool) {
            gst_rtsp_session_pool_filter(pool, 
                [](GstRTSPSessionPool * /*pool*/, GstRTSPSession * /*session*/, gpointer /*user_data*/) -> GstRTSPFilterResult {
                    return GST_RTSP_FILTER_REMOVE;
                }, 
                nullptr);
            g_object_unref(pool);
            RCLCPP_INFO(this->get_logger(), "Forced cleanup of all RTSP sessions (After factory swap).");
        }
    }

    pipeline_recreate_needed_.store(false);
    
    // [Debug] Verify the new pipeline configuration
    // if (factory_cam0_) {
    //     gchar* launch_str = app_src_dynamic_media_factory_create_launch_string(factory_cam0_);
    //     if (launch_str) {
    //         RCLCPP_WARN(this->get_logger(), "New Pipeline Config: %s", launch_str);
    //         g_free(launch_str);
    //     }
    // }

    RCLCPP_INFO(this->get_logger(), "RTSP pipelines recreated successfully.");
}
// =========================================================================
// Add URL in RTSP server 
// =========================================================================
void RTSPServerNode::rtsp_server_add_url(const char *url, const char *sPipeline){
    GstRTSPMountPoints *mounts;
    GstRTSPMediaFactory *factory;

    mounts = gst_rtsp_server_get_mount_points(server_);
    factory = gst_rtsp_media_factory_new();

    // set up pipeline
    gst_rtsp_media_factory_set_launch(factory, sPipeline);

    // set up appsrc callback
    g_signal_connect(factory, "media-configure", (GCallback)media_configure, this);

    // Shared pipeline for multiple clients
    gst_rtsp_media_factory_set_shared(factory, TRUE);

    // Latency configuration
    gst_rtsp_media_factory_set_latency(factory, 500);  

    gst_rtsp_mount_points_add_factory(mounts, url, factory);
    g_object_unref(mounts);
}

// media가 준비될 때: appsrc 포인터 확보
void RTSPServerNode::media_configure(GstRTSPMediaFactory * /*factory*/, GstRTSPMedia *media, gpointer user_data){
    RTSPServerNode *node = static_cast<RTSPServerNode*>(user_data);

    // get pipeline
    GstElement *pipeline = gst_rtsp_media_get_element(media);

    // cam0: imagesrc
    {
        std::lock_guard<std::mutex> lock(node->appsrc_mutex_);
        GstElement *new_appsrc = gst_bin_get_by_name(GST_BIN(pipeline), "imagesrc");
        if (new_appsrc) {
            if (node->appsrc_) gst_object_unref(node->appsrc_);
            node->appsrc_ = new_appsrc;
            gst_util_set_object_arg(G_OBJECT(node->appsrc_), "format", "time");
            g_object_set(G_OBJECT(node->appsrc_), 
                            "stream-type", GST_APP_STREAM_TYPE_STREAM,
                            "format", GST_FORMAT_TIME,
                            "is-live", TRUE, NULL);
            
            // Set caps immediately using last known image dimensions
            int width = node->last_image_width_.load();
            int height = node->last_image_height_.load();
            GstCaps *caps = gst_caps_new_simple("video/x-raw",
                "format", G_TYPE_STRING, "RGB",
                "width", G_TYPE_INT, width,
                "height", G_TYPE_INT, height,
                "framerate", GST_TYPE_FRACTION, 30, 1,
                nullptr);
            gst_app_src_set_caps(GST_APP_SRC(node->appsrc_), caps);
            gst_caps_unref(caps);
            node->caps_set_ = true;
            RCLCPP_INFO(node->get_logger(), "appsrc(imagesrc) configured for /cam0 with caps %dx%d", width, height);
        }
    }

    // cam1: imagesrc1
    {
        std::lock_guard<std::mutex> lock(node->appsrc_mutex2_);
        GstElement *new_appsrc2 = gst_bin_get_by_name(GST_BIN(pipeline), "imagesrc1");
        if (new_appsrc2) {
            if (node->appsrc2_) gst_object_unref(node->appsrc2_);
            node->appsrc2_ = new_appsrc2;
            gst_util_set_object_arg(G_OBJECT(node->appsrc2_), "format", "time");
            g_object_set(G_OBJECT(node->appsrc2_), 
                            "stream-type", GST_APP_STREAM_TYPE_STREAM,
                            "format", GST_FORMAT_TIME,
                            "is-live", TRUE, NULL);
            
            // Set caps immediately using last known image dimensions
            int width = node->last_image_width2_.load();
            int height = node->last_image_height2_.load();
            GstCaps *caps = gst_caps_new_simple("video/x-raw",
                "format", G_TYPE_STRING, "RGB",
                "width", G_TYPE_INT, width,
                "height", G_TYPE_INT, height,
                "framerate", GST_TYPE_FRACTION, 30, 1,
                nullptr);
            gst_app_src_set_caps(GST_APP_SRC(node->appsrc2_), caps);
            gst_caps_unref(caps);
            node->caps_set2_ = true;
            RCLCPP_INFO(node->get_logger(), "appsrc(imagesrc1) configured for /cam1 with caps %dx%d", width, height);
        }
    }

    gst_object_unref(pipeline);
}

// RTSP media unprepared (연결 종료 시에도 포인터는 유지하되 상태만 리셋 가능)
void RTSPServerNode::on_media_unprepared(GstRTSPMedia * /*media*/, gpointer user_data) {
    // [Note] In shared mode, we keep appsrc_ pointers to handle subsequent clients.
    // They will be cleaned up in the destructor or when factory is replaced.
    RCLCPP_DEBUG(static_cast<RTSPServerNode*>(user_data)->get_logger(), "RTSP media unprepared (Keep pointers for shared mode)");
}

gboolean RTSPServerNode::session_cleanup(gpointer user_data){
    RTSPServerNode *node = static_cast<RTSPServerNode*>(user_data);
    GstRTSPSessionPool *pool;
    int num;

    pool = gst_rtsp_server_get_session_pool(node->server_);
    num = gst_rtsp_session_pool_cleanup(pool);
    g_object_unref(pool);

    if (num > 0) {
        RCLCPP_INFO(node->get_logger(), "Sessions cleaned: %d", num);
    }
    return TRUE;
}

// =========================================================================
// ROS2 image callbacks
// =========================================================================
void RTSPServerNode::CallbackImage(const sensor_msgs::msg::Image::ConstSharedPtr& msg){
    // Lifecycle check
    if (this->get_current_state().id() != lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE) {
        return;
    }

    cv_bridge::CvImagePtr cv_ptr;
    try {
        // Request RGB8 directly from cv_bridge to optimize conversion
        cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::RGB8);
    } catch (cv_bridge::Exception& e) {
        // Throttle error logging to prevent disk I/O overload
        RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 1000, 
                              "cv_bridge exception(cam0): %s", e.what());
        return;
    }
    push_image_to_appsrc(cv_ptr->image, msg->header.stamp);
}

void RTSPServerNode::CallbackImage2(const sensor_msgs::msg::Image::ConstSharedPtr& msg){
    // Lifecycle check
    if (this->get_current_state().id() != lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE) {
        return;
    }

    cv_bridge::CvImagePtr cv_ptr;
    try {
        cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::RGB8);
    } catch (cv_bridge::Exception& e) {
        RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
                              "cv_bridge exception(cam1): %s", e.what());
        return;
    }
    push_image_to_appsrc2(cv_ptr->image, msg->header.stamp);
}

// =========================================================================
// push to appsrc (cam0)
// =========================================================================
void RTSPServerNode::push_image_to_appsrc(const cv::Mat& rgb_image, const rclcpp::Time& stamp) {
    if (rgb_image.empty()) return;
    
    // Store last known dimensions for media_configure to use
    last_image_width_.store(rgb_image.cols);
    last_image_height_.store(rgb_image.rows);
    
    // RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
    //                      "[cam0] Received ROS Image: %dx%d", rgb_image.cols, rgb_image.rows);

    std::lock_guard<std::mutex> lock(appsrc_mutex_);
    if (!appsrc_) {
        RCLCPP_DEBUG(this->get_logger(), "[cam0] appsrc not yet configured, dropping frame.");
        return;
    }

    // caps set using original image size.
    // 'videoscale' in pipeline handles resizing.
    if (!caps_set_) {
        GstCaps *caps = gst_caps_new_simple("video/x-raw",
                               "format", G_TYPE_STRING, "RGB",
                               "width", G_TYPE_INT, rgb_image.cols,
                               "height", G_TYPE_INT, rgb_image.rows,
                               "framerate", GST_TYPE_FRACTION, 30, 1,
                               nullptr);
        gst_app_src_set_caps(GST_APP_SRC(appsrc_), caps);
        gst_caps_unref(caps);
        caps_set_ = true;
    }

    GstBuffer *buffer = gst_buffer_new_allocate(NULL, rgb_image.total() * rgb_image.elemSize(), NULL);

    // Manual timestamp calculation removed. Relying on appsrc's do-timestamp=true property.
    // This ensures timestamps are generated based on the running time of the pipeline clock.
    // GST_BUFFER_PTS(buffer) = ...;
    // GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;

    gst_buffer_fill(buffer, 0, rgb_image.data, rgb_image.total() * rgb_image.elemSize());
    // GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_LIVE); // appsrc is-live property handles this hint

    GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc_), buffer);
    if (ret != GST_FLOW_OK) {
        if (ret == GST_FLOW_FLUSHING) {
            RCLCPP_DEBUG(this->get_logger(), "[cam0] push FLUSHING -> dropping");
        } else {
            RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000, 
                                 "[cam0] push error: %d", ret);
        }
        return;
    } else {
        // RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
        //                      "[cam0] Pushed buffer to pipeline successfully");
    }
}

// =========================================================================
// push to appsrc (cam1)
// =========================================================================
void RTSPServerNode::push_image_to_appsrc2(const cv::Mat& rgb_image, const rclcpp::Time& stamp) {
    if (rgb_image.empty()) return;
    
    // Store last known dimensions for media_configure to use
    last_image_width2_.store(rgb_image.cols);
    last_image_height2_.store(rgb_image.rows);
    
    std::lock_guard<std::mutex> lock(appsrc_mutex2_);
    if (!appsrc2_) {
        RCLCPP_DEBUG(this->get_logger(), "[cam1] appsrc not yet configured, dropping frame.");
        return;
    }

    if (!caps_set2_) {
        GstCaps *caps = gst_caps_new_simple("video/x-raw",
                               "format", G_TYPE_STRING, "RGB",
                               "width", G_TYPE_INT, rgb_image.cols,
                               "height", G_TYPE_INT, rgb_image.rows,
                               "framerate", GST_TYPE_FRACTION, 30, 1,
                               nullptr);
        gst_app_src_set_caps(GST_APP_SRC(appsrc2_), caps);
        gst_caps_unref(caps);
        caps_set2_ = true;
    }

    GstBuffer *buffer = gst_buffer_new_allocate(NULL, rgb_image.total() * rgb_image.elemSize(), NULL);

    // Manual timestamp calculation removed. Relying on appsrc's do-timestamp=true property.
    // GstClockTime pts = (stamp.nanoseconds() - first_frame_stamp2_.nanoseconds());
    // GST_BUFFER_PTS(buffer) = pts;
    // GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;

    gst_buffer_fill(buffer, 0, rgb_image.data, rgb_image.total() * rgb_image.elemSize());
    // GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_LIVE);

    GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc2_), buffer);
    if (ret != GST_FLOW_OK) {
        if (ret == GST_FLOW_FLUSHING) {
            RCLCPP_DEBUG(this->get_logger(), "[cam1] push FLUSHING -> dropping");
        } else {
            RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000, 
                                  "[cam1] push error: %d", ret);
        }
        return;
    }
}

} // namespace rtsp_server

#include <rclcpp_components/register_node_macro.hpp>
RCLCPP_COMPONENTS_REGISTER_NODE(rtsp_server::RTSPServerNode)
