#if __has_include(<cv_bridge/cv_bridge.hpp>)
#include <cv_bridge/cv_bridge.hpp>
#else
#include <cv_bridge/cv_bridge.h>
#endif

#include "camera_driver.hpp"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <vector>
#include <string>

namespace camera_interface {

// ------------------------ rpicam-vid 스폰 ------------------------
bool CameraDriver::SpawnRpiCamVid() {
    // Use parameters from m_image_info (loaded from YAML)
    std::string mode_str = std::to_string(m_image_info.image_width) + ":" + std::to_string(m_image_info.image_height);
    std::vector<std::string> args_str = {
        "/usr/local/bin/rpicam-vid",
        "--mode", mode_str,
        "--framerate", std::to_string(m_image_info.framerate),
        "--codec", "h264",
        "--libav-format", "h264",
        "--inline",
        "--nopreview",
        "-t", "0",
        "-o", "-"
    };

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        RCLCPP_ERROR(this->get_logger(), "pipe() failed");
        return false;
    }
    pid_t pid = fork();
    if (pid == -1) {
        RCLCPP_ERROR(this->get_logger(), "fork() failed");
        close(pipefd[0]); close(pipefd[1]);
        return false;
    }
    if (pid == 0) {
        // child: stdout -> pipe write end
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]); close(pipefd[1]);

        std::vector<const char*> argv;
        for(const auto& s : args_str) { argv.push_back(s.c_str()); }
        argv.push_back(nullptr);

        execvp(argv[0], const_cast<char* const*>(argv.data()));
        _exit(127); // exec 실패 시
    }
    // parent

    std::string cmd_log_str;
    for(const auto& s : args_str) {
        cmd_log_str += s + " ";
    }
    RCLCPP_INFO(this->get_logger(), "Executing rpicam-vid command: %s", cmd_log_str.c_str());

    close(pipefd[1]);
    m_rpicam_fd = pipefd[0];
    m_rpicam_pid = pid;
    return true;
}

// ------------------------ 생성/소멸 ------------------------
CameraDriver::CameraDriver(const rclcpp::NodeOptions& options)
 : rclcpp_lifecycle::LifecycleNode("camera_driver", options)
{
    // Constructor handles parameter declaration only
    InitRosParam();
    RCLCPP_INFO(this->get_logger(), "CameraDriver constructed. Waiting for configuration.");
}

CameraDriver::~CameraDriver() {
    DestroyGStreamer();
}

// ------------------------ Lifecycle Callbacks ------------------------

CallbackReturn CameraDriver::on_configure(const rclcpp_lifecycle::State &)
{
    RCLCPP_INFO(this->get_logger(), "Configuring CameraDriver...");

    // Get parameter values (already declared in constructor, just reading them if needed again or if logic depends on them)
    // Note: In ROS 2, parameters are usually read when declared or via get_parameter.
    // Since InitRosParam declared them, we can assume they are available.
    
    // Create Lifecycle Publisher
    // Topic: out/image_raw
    rclcpp::QoS qos_profile(10);
    qos_profile.reliability(rclcpp::ReliabilityPolicy::BestEffort);
    qos_profile.durability(rclcpp::DurabilityPolicy::Volatile);

    m_image_publisher = this->create_publisher<sensor_msgs::msg::Image>("out/image_raw", qos_profile);

    if (!InitGStreamer()) {
        RCLCPP_ERROR(this->get_logger(), "Failed to initialize GStreamer pipeline.");
        return CallbackReturn::FAILURE;
    }

    RCLCPP_INFO(this->get_logger(), "CameraDriver configured.");
    return CallbackReturn::SUCCESS;
}

CallbackReturn CameraDriver::on_activate(const rclcpp_lifecycle::State &) {
    RCLCPP_INFO(this->get_logger(), "Activating CameraDriver...");

    // 1. Activate Publisher
    m_image_publisher->on_activate();

    // 2. Start Hardware/GStreamer
    if (m_pipeline) {
        GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            RCLCPP_ERROR(this->get_logger(), "Failed to set GStreamer pipeline to PLAYING state.");
            return CallbackReturn::FAILURE;
        }
    } else {
        RCLCPP_ERROR(this->get_logger(), "Pipeline not initialized!");
        return CallbackReturn::FAILURE;
    }

    // 3. Start Monitoring Timer
    m_monitor_timer = this->create_wall_timer(
        std::chrono::milliseconds(500),
        std::bind(&CameraDriver::MonitorGStreamer, this));

    RCLCPP_INFO(this->get_logger(), "CameraDriver activated.");
    return CallbackReturn::SUCCESS;
}

CallbackReturn CameraDriver::on_deactivate(const rclcpp_lifecycle::State &) {
    RCLCPP_INFO(this->get_logger(), "Deactivating CameraDriver...");

    // 1. Stop Monitoring Timer
    if (m_monitor_timer) {
        m_monitor_timer->cancel();
        m_monitor_timer.reset();
    }

    // 2. Stop Hardware/GStreamer
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_READY);
    }

    // 3. Deactivate Publisher
    m_image_publisher->on_deactivate();

    RCLCPP_INFO(this->get_logger(), "CameraDriver deactivated.");
    return CallbackReturn::SUCCESS;
}

CallbackReturn CameraDriver::on_cleanup(const rclcpp_lifecycle::State &) {
    RCLCPP_INFO(this->get_logger(), "Cleaning up CameraDriver...");
    
    DestroyGStreamer();

    // Release Publisher
    m_image_publisher.reset();

    RCLCPP_INFO(this->get_logger(), "CameraDriver cleaned up.");
    return CallbackReturn::SUCCESS;
}

CallbackReturn CameraDriver::on_shutdown(const rclcpp_lifecycle::State &) {
    RCLCPP_INFO(this->get_logger(), "Shutting down CameraDriver...");
    
    // Ensure everything is stopped
    if (m_monitor_timer) {
        m_monitor_timer->cancel();
        m_monitor_timer.reset();
    }
    DestroyGStreamer();
    m_image_publisher.reset();

    return CallbackReturn::SUCCESS;
}

CallbackReturn CameraDriver::on_error(const rclcpp_lifecycle::State &) {
    RCLCPP_ERROR(this->get_logger(), "Error detected in CameraDriver. Cleaning up...");
    
    // Ensure everything is stopped
    if (m_monitor_timer) {
        m_monitor_timer->cancel();
        m_monitor_timer.reset();
    }
    DestroyGStreamer();
    m_image_publisher.reset();

    return CallbackReturn::SUCCESS; // Transition to Unconfigured
}

// ------------------------ GStreamer Monitor & Cleanup ------------------------
void CameraDriver::DestroyGStreamer() {
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        m_appsink = nullptr;
    }
    if (m_bus) {
        gst_object_unref(m_bus);
        m_bus = nullptr;
    }

    // 자식 프로세스 정리
    if (m_rpicam_pid > 0) {
        kill(m_rpicam_pid, SIGTERM);
        waitpid(m_rpicam_pid, nullptr, 0);
        m_rpicam_pid = -1;
    }
    if (m_rpicam_fd >= 0) {
        close(m_rpicam_fd);
        m_rpicam_fd = -1;
    }
}

void CameraDriver::MonitorGStreamer() {
    // Only monitor if we are active (pipeline should exist)
    // 1. 파이프라인이 없으면 재연결 시도
    if (!m_pipeline) {
        // InitGStreamer가 성공하면 내부에서 로그 출력됨
        if (InitGStreamer()) {
            RCLCPP_INFO(this->get_logger(), "Restarting GStreamer pipeline...");
            gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
        }
        return;
    }

    // 2. 파이프라인이 있으면 버스 메시지 체크 (에러/EOS 감지)
    if (m_bus) {
        GstMessage* msg = nullptr;
        // Non-blocking pop
        while ((msg = gst_bus_pop_filtered(m_bus, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS)))) {
            bool fatal = false;
            GError *err = nullptr;
            gchar *debug_info = nullptr;

            switch (GST_MESSAGE_TYPE(msg)) {
                case GST_MESSAGE_ERROR:
                    gst_message_parse_error(msg, &err, &debug_info);
                    RCLCPP_ERROR(this->get_logger(), "[GStreamer] Error: %s", err->message);
                    RCLCPP_ERROR(this->get_logger(), "[GStreamer] Debug: %s", debug_info ? debug_info : "none");
                    g_clear_error(&err);
                    g_free(debug_info);
                    fatal = true;
                    break;

                case GST_MESSAGE_EOS:
                    RCLCPP_WARN(this->get_logger(), "[GStreamer] End-Of-Stream (EOS)");
                    fatal = true;
                    break;

                default:
                    break;
            }
            gst_message_unref(msg);

            if (fatal) {
                RCLCPP_ERROR(this->get_logger(), "Connection lost. Restarting pipeline...");
                DestroyGStreamer();
                break; // 루프 탈출 후 다음 주기에서 재연결
            }
        }
    }
}

// ------------------------ ROS 설정 ------------------------
void CameraDriver::InitRosParam() {
    // image capture param
    m_image_info.image_width  = declare_parameter<int>("image_width", 1920);
    m_image_info.image_height = declare_parameter<int>("image_height", 1080);
    m_image_info.framerate    = declare_parameter<int>("framerate", 30);
    m_image_info.format       = declare_parameter<std::string>("format", "NV12");
    m_image_info.device_path  = declare_parameter<std::string>("device_path", "/dev/video0");

    // image crop/resize param
    m_resize_info.image_crop    = declare_parameter<bool>("image_crop", false);
    m_resize_info.crop_width    = declare_parameter<int>("crop_width", 640);
    m_resize_info.crop_height   = declare_parameter<int>("crop_height", 480);

    m_resize_info.image_resize  = declare_parameter<bool>("image_resize", false);
    m_resize_info.resize_width  = declare_parameter<int>("resize_width", 640);
    m_resize_info.resize_height = declare_parameter<int>("resize_height", 360);

    // image publish param
    m_camera_frame_id  = declare_parameter<std::string>("camera_frame_id", "camera_frame");

    // print param
    RCLCPP_INFO(this->get_logger(), "---------------- ros param -------------- ");
    RCLCPP_INFO(this->get_logger(), "-- image capture param --");
    RCLCPP_INFO(this->get_logger(), "image_width  : %d", m_image_info.image_width);
    RCLCPP_INFO(this->get_logger(), "image_height : %d", m_image_info.image_height);
    RCLCPP_INFO(this->get_logger(), "framerate    : %d", m_image_info.framerate);
    RCLCPP_INFO(this->get_logger(), "format       : %s", m_image_info.format.c_str());
    RCLCPP_INFO(this->get_logger(), "device_path  : %s\n", m_image_info.device_path.c_str());

    RCLCPP_INFO(this->get_logger(), "-- image crop/resize param --");
    RCLCPP_INFO(this->get_logger(), "image_crop    : %d", m_resize_info.image_crop);
    RCLCPP_INFO(this->get_logger(), "crop_width    : %d", m_resize_info.crop_width);
    RCLCPP_INFO(this->get_logger(), "crop_height   : %d", m_resize_info.crop_height);
    RCLCPP_INFO(this->get_logger(), "image_resize  : %d", m_resize_info.image_resize);
    RCLCPP_INFO(this->get_logger(), "resize_width  : %d", m_resize_info.resize_width);
    RCLCPP_INFO(this->get_logger(), "resize_height : %d\n", m_resize_info.resize_height);

    RCLCPP_INFO(this->get_logger(), "-- image publish param --");
    RCLCPP_INFO(this->get_logger(), "camera_frame_id : %s", m_camera_frame_id.c_str());
    RCLCPP_INFO(this->get_logger(), "---------------- ros param -------------- ");
}

// InitRosCommon was merged into on_configure and on_activate

// InitCommonClass merged into on_activate / InitGStreamer

// ------------------------ GStreamer ------------------------
bool CameraDriver::InitGStreamer() {
    if (m_pipeline) {
        return true; // Already initialized
    }

    gst_init(nullptr, nullptr);

    std::stringstream pipeline_str;
    if (m_image_info.format == std::string("MJPG")) {
        pipeline_str << "v4l2src device=" << m_image_info.device_path << " ! "
                     << "image/jpeg," 
                     << "width=" << m_image_info.image_width << ", "
                     << "height=" << m_image_info.image_height << ", "
                     << "framerate=" << m_image_info.framerate << "/1 ! "
                     << "jpegparse ! jpegdec ! videoconvert ! video/x-raw,format=RGB ! appsink sync=false name=sink";
    }
    else if (m_image_info.device_path == std::string("RPICAM")) {
        pipeline_str << "libcamerasrc ! "
            << "video/x-raw, format=" << m_image_info.format << ", "
            << "width=" << m_image_info.image_width << ", "
            << "height=" << m_image_info.image_height << ", "
            << "framerate=" << m_image_info.framerate << "/1 ! "
            << "appsink name=sink";
    }
    else if (m_image_info.device_path == std::string("RPICAM-VID")) {
        if (!SpawnRpiCamVid()) return false;

        m_image_info.format = "RGB";

        const char* decoder = nullptr;
        if (gst_element_factory_find("v4l2h264dec")) {
            decoder = "v4l2h264dec";
        } else if (gst_element_factory_find("avdec_h264")) {
            decoder = "avdec_h264";
        } else {
            RCLCPP_ERROR(this->get_logger(),
                "No H.264 decoder found. Install 'gstreamer1.0-libav' (for avdec_h264) "
                "or enable V4L2 H.264 decoder (v4l2h264dec).");
            return false;
        }

        pipeline_str
          << "fdsrc fd=" << m_rpicam_fd << " do-timestamp=true ! "
          << "h264parse config-interval=-1 disable-passthrough=true ! "
          << decoder << " ! "                             
          << "videoconvert ! "
          << "video/x-raw,format="<<m_image_info.format<< " ! "
          << "queue max-size-buffers=1 leaky=downstream ! "
          << "appsink sync=false name=sink";
    }
    else {
        pipeline_str << "v4l2src device=" << m_image_info.device_path << " ! "
                     << "videoconvert ! "
                     << "video/x-raw, format=" << m_image_info.format << ", "
                     << "width=" << m_image_info.image_width << ", "
                     << "height=" << m_image_info.image_height << ", "
                     << "framerate=" << m_image_info.framerate << "/1 ! "
                     << "appsink sync=false name=sink";
    }

    RCLCPP_INFO(this->get_logger(), "%s", pipeline_str.str().c_str());

    m_pipeline = gst_parse_launch(pipeline_str.str().c_str(), nullptr);
    if (!m_pipeline) {
        RCLCPP_ERROR(this->get_logger(), "Failed to create pipeline.");
        return false;
    }

    m_bus = gst_element_get_bus(m_pipeline);

    m_appsink = gst_bin_get_by_name(GST_BIN(m_pipeline), "sink");
    if (!m_appsink) {
        RCLCPP_ERROR(this->get_logger(), "Failed to get appsink element.");
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        if (m_bus) { gst_object_unref(m_bus); m_bus = nullptr; }
        return false;
    }

    gst_app_sink_set_emit_signals((GstAppSink*)m_appsink, false);
    gst_app_sink_set_drop((GstAppSink*)m_appsink, true);
    gst_app_sink_set_max_buffers((GstAppSink*)m_appsink, 1);

    // gst_element_set_state(m_pipeline, GST_STATE_PLAYING); // Moved to on_activate

    GstAppSinkCallbacks callbacks = {};
    callbacks.new_sample = OnGStreamerCallback;
    gst_app_sink_set_callbacks(GST_APP_SINK(m_appsink), &callbacks, this, nullptr);

    return true;
}

GstFlowReturn CameraDriver::OnGStreamerCallback(GstAppSink* appsink, gpointer user_data) {
    CameraDriver* self = static_cast<CameraDriver*>(user_data);

    // Only publish if the publisher is active
    if (!self->m_image_publisher->is_activated()) {
        return GST_FLOW_OK;
    }

    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink));
    if (!sample) {
        RCLCPP_ERROR(self->get_logger(), "Failed to get appsink sample.");
        return GST_FLOW_ERROR;
    }

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        gst_sample_unref(sample);
        RCLCPP_ERROR(self->get_logger(), "Failed to map buffer.");
        return GST_FLOW_ERROR;
    }

    GstCaps *caps = gst_sample_get_caps(sample);
    GstStructure *structure = gst_caps_get_structure(caps, 0);

    int width = 0, height = 0;
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);

    cv::Mat image;

    cv::Mat raw_image;

    if (self->m_image_info.format == std::string("NV12")) {
        cv::Mat nv12 = cv::Mat(height + height / 2, width, CV_8UC1, (void*)map.data);
        cv::cvtColor(nv12, raw_image, cv::COLOR_YUV2RGB_NV12);
    } else if (self->m_image_info.format == std::string("YUY2")) {
        cv::Mat yuyv(height, width, CV_8UC2, map.data);
        cv::cvtColor(yuyv, raw_image, cv::COLOR_YUV2RGB_YUY2);
    } else if (self->m_image_info.format == std::string("RGBx")) {
        cv::Mat rgbx(height, width, CV_8UC4, map.data);
        cv::cvtColor(rgbx, raw_image, cv::COLOR_RGBA2RGB);
    } else if (self->m_image_info.format == std::string("MJPG")) {
        raw_image = cv::Mat(height, width, CV_8UC3, map.data);
    } else if (self->m_image_info.format == std::string("RGB")) {
        raw_image = cv::Mat(height, width, CV_8UC3, map.data);
    } else if (self->m_image_info.format == std::string("GRAY")) {
        cv::Mat gray(height, width, CV_8UC1, map.data);
        cv::cvtColor(gray, raw_image, cv::COLOR_GRAY2RGB);
    } else {
        RCLCPP_ERROR(self->get_logger(), "Unsupported image format: %s", self->m_image_info.format.c_str());
        return GST_FLOW_ERROR;
    }

    // Use a separate variable for processing to keep raw_image header intact if needed
    cv::Mat processed_image = raw_image;

    if (self->m_resize_info.image_crop) {
        int cx = processed_image.cols / 2;
        int cy = processed_image.rows / 2;
        int x = std::max(0, cx - self->m_resize_info.crop_width / 2);
        int y = std::max(0, cy - self->m_resize_info.crop_height / 2);
        int w = std::min(processed_image.cols - x, self->m_resize_info.crop_width);
        int h = std::min(processed_image.rows - y, self->m_resize_info.crop_height);
        
        // This is a view pointing to a sub-region of raw_image (or map.data)
        processed_image = processed_image(cv::Rect(x, y, w, h));
    }

    if (self->m_resize_info.image_resize) {
        cv::Size resize_size(self->m_resize_info.resize_width, self->m_resize_info.resize_height);
        cv::Mat resized_out;
        // cv::resize will allocate a new, continuous memory block for resized_out
        cv::resize(processed_image, resized_out, resize_size, cv::INTER_LINEAR);
        processed_image = resized_out;
    }

    self->ImagePublish(processed_image);

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);

    return GST_FLOW_OK;
}

void CameraDriver::ImagePublish(cv::Mat& image) {
    if (image.empty()) {
        RCLCPP_WARN(this->get_logger(), "Empty image received");
        return;
    }

    std_msgs::msg::Header header;
    header.stamp = this->get_clock()->now();
    header.frame_id = m_camera_frame_id;

    std::string encoding = sensor_msgs::image_encodings::RGB8;
    cv_bridge::CvImage cv_bridge_image(header, encoding, image);
    
    // LifecyclePublisher requires publish(msg)
    m_image_publisher->publish(*cv_bridge_image.toImageMsg());
}

} // namespace camera_interface

#include <rclcpp_components/register_node_macro.hpp>
RCLCPP_COMPONENTS_REGISTER_NODE(camera_interface::CameraDriver)
