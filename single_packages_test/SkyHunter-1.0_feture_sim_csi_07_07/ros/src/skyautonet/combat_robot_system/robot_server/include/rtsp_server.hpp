#ifndef rtsp_server_HPP
#define rtsp_server_HPP

/* for OpenCV */
#include <opencv2/opencv.hpp>
#if __has_include(<cv_bridge/cv_bridge.hpp>)
#include <cv_bridge/cv_bridge.hpp>
#else
#include <cv_bridge/cv_bridge.h>
#endif

/* for ROS2 */
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>
#include <lifecycle_msgs/msg/state.hpp>
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/rtsp-server/rtsp-media-factory.h>
#include <gst/app/gstappsrc.h>
#include <sensor_msgs/msg/image.hpp>
#include <image_transport/image_transport.hpp>
#include <combat_robot_msgs/msg/stream_control_command.hpp>
#include <std_msgs/msg/u_int8.hpp>
#include <mutex>
#include <thread>
#include <atomic>

namespace rtsp_server{

struct StreamConfig {
    std::string rtsp_port;
    std::string encoder;
};

// Pipeline Encoder Options
static constexpr const char* PIPELINE_RPI5 = "x264enc tune=zerolatency speed-preset=superfast";
static constexpr const char* PIPELINE_RK3588 = "mpph264enc";
static constexpr const char* PIPELINE_PC = "x264enc tune=zerolatency speed-preset=ultrafast";

using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

class RTSPServerNode : public rclcpp_lifecycle::LifecycleNode {
public:
    RTSPServerNode(const rclcpp::NodeOptions& options);
    ~RTSPServerNode();

    // Lifecycle callbacks
    CallbackReturn on_configure(const rclcpp_lifecycle::State &);
    CallbackReturn on_activate(const rclcpp_lifecycle::State &);
    CallbackReturn on_deactivate(const rclcpp_lifecycle::State &);
    CallbackReturn on_cleanup(const rclcpp_lifecycle::State &);
    CallbackReturn on_shutdown(const rclcpp_lifecycle::State &);
    CallbackReturn on_error(const rclcpp_lifecycle::State &);

    // Public getter for stream configuration (Thread-safe)
    StreamConfig get_stream_config() {
        std::lock_guard<std::mutex> lock(params_mutex_);
        return current_config_;
    }

private:
    void InitRosCommon();
    void init_rtsp_server();
    
    // GStreamer Main Loop
    GMainLoop *main_loop_ = nullptr;
    std::thread gst_thread_;
    void gst_loop_thread();

    void rtsp_server_add_url(const char *url, const char *sPipeline);

    // factory 콜백 (RTSP 미디어 준비/해제 대응)
    static void media_configure(GstRTSPMediaFactory *factory, GstRTSPMedia *media, gpointer user_data);
    static void on_media_unprepared(GstRTSPMedia *media, gpointer user_data); 

    // cam0 콜백/푸시 (기존)
    void CallbackImage(const sensor_msgs::msg::Image::ConstSharedPtr& msg);
    void push_image_to_appsrc(const cv::Mat& img, const rclcpp::Time& stamp);

    // cam1 콜백/푸시 (추가)
    void CallbackImage2(const sensor_msgs::msg::Image::ConstSharedPtr& msg);
    void push_image_to_appsrc2(const cv::Mat& img, const rclcpp::Time& stamp);

    static gboolean session_cleanup(gpointer user_data);

    // 구독자
    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr m_sub_image;   // cam0 (/camera/image_raw2)
    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr m_sub_image2;  // cam1 (/camera/image_raw)
    rclcpp::Subscription<combat_robot_msgs::msg::StreamControlCommand>::SharedPtr stream_control_sub_;

    // RTSP Status Publisher & Timer
    rclcpp::Publisher<std_msgs::msg::UInt8>::SharedPtr rtsp_status_pub_;
    rclcpp::TimerBase::SharedPtr status_timer_;
    void publish_rtsp_status();

    // RTSP/GStreamer
    GstRTSPServer *server_ = nullptr;
    guint m_server_source_id = 0;

    // cam0 appsrc 상태 (기존)
    GstElement *appsrc_ = nullptr;
    bool caps_set_ = false;
    int framerate_ = 30;
    bool is_first_frame_ = true;
    rclcpp::Time first_frame_stamp_;
    std::mutex appsrc_mutex_;
    std::atomic<int> last_image_width_{640};   // Match camera_driver config
    std::atomic<int> last_image_height_{640};

    // cam1 appsrc 상태 (추가)
    GstElement *appsrc2_ = nullptr;
    bool caps_set2_ = false;
    int framerate2_ = 30;
    bool is_first_frame2_ = true; // Added missing member
    rclcpp::Time first_frame_stamp2_;
    std::mutex appsrc_mutex2_;
    std::atomic<int> last_image_width2_{1920};  // Last received image dimensions for cam1
    std::atomic<int> last_image_height2_{1080};

    // Dynamic Stream Settings
    std::mutex params_mutex_;
    StreamConfig current_config_; // Replaces individual atomics for better consistency

    // Stream control callback
    void onStreamControlCommand(const combat_robot_msgs::msg::StreamControlCommand::SharedPtr msg);

    // RTSP/GStreamer related objects to be managed
    GstRTSPMountPoints *mounts_ = nullptr; // Store mounts here to manipulate factories

    // Media factories for each stream, to be managed dynamically
    GstRTSPMediaFactory *factory_cam0_ = nullptr;
    GstRTSPMediaFactory *factory_cam1_ = nullptr;

    // Phase-1 app integration keeps the fixed pipeline but allows logical stream on/off.
    std::atomic<bool> stream_requested_{true};

    // Flag to indicate if pipeline needs re-creation and its mutex
    std::atomic<bool> pipeline_recreate_needed_{false};
    std::mutex pipeline_recreate_mutex_;

    // Helper to recreate pipelines immediately
    void checkAndRecreatePipelines();
};

}  //namespace rtsp_server

#endif // rtsp_server_HPP
