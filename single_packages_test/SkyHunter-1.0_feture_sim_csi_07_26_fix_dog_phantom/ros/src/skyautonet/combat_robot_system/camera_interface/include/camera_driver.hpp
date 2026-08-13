#ifndef CAMERA_DRIVER_HPP_
#define CAMERA_DRIVER_HPP_

/*** Include ***/
/* for general */
#include <iostream>

/* for Gstreamer */
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

/* for OpenCV */
#include <opencv2/opencv.hpp>

/* for ROS2 */
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>
#include <image_transport/image_transport.hpp>

namespace camera_interface {

using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

typedef struct {
    int image_width;
    int image_height;
    int framerate;

    std::string format;
    std::string device_path;
} image_param_t;

typedef struct {
    bool image_crop;
    int crop_width;
    int crop_height;

    bool image_resize;
    int resize_width;
    int resize_height;
} resize_param_t;

class CameraDriver : public rclcpp_lifecycle::LifecycleNode {
public:
    explicit CameraDriver(const rclcpp::NodeOptions& options);
    ~CameraDriver();

    // Lifecycle callbacks
    CallbackReturn on_configure(const rclcpp_lifecycle::State &);
    CallbackReturn on_activate(const rclcpp_lifecycle::State &);
    CallbackReturn on_deactivate(const rclcpp_lifecycle::State &);
    CallbackReturn on_cleanup(const rclcpp_lifecycle::State &);
    CallbackReturn on_shutdown(const rclcpp_lifecycle::State &);
    CallbackReturn on_error(const rclcpp_lifecycle::State &);

private:
    void InitRosParam();
    
    // GStreamer initialization is now part of configure/activate
    bool InitGStreamer();

    static GstFlowReturn OnGStreamerCallback(GstAppSink* appsink, gpointer user_data);
    void ImagePublish(cv::Mat& image);

    // GStreamer Management
    void MonitorGStreamer();
    void DestroyGStreamer();

    // Change to lifecycle publisher
    std::shared_ptr<rclcpp_lifecycle::LifecyclePublisher<sensor_msgs::msg::Image>> m_image_publisher;
    
    std::string m_camera_frame_id;
    image_param_t m_image_info;
    resize_param_t m_resize_info;

    rclcpp::TimerBase::SharedPtr m_monitor_timer;

    GstElement* m_pipeline = nullptr;
    GstElement* m_appsink = nullptr;
    GstBus*     m_bus = nullptr;

    int m_rpicam_fd = -1;
    pid_t m_rpicam_pid = -1;
    bool SpawnRpiCamVid();
};

} // namespace camera_interface

#endif // CAMERA_DRIVER_HPP_