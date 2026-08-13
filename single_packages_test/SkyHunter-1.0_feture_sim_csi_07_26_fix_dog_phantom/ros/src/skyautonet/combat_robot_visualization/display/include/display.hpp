#ifndef DISPLAY_NODE_HPP_
#define DISPLAY_NODE_HPP_

#include <mutex>
#include <thread>
#include <chrono>

// ROS includes
#include <rclcpp/rclcpp.hpp>
#include <ament_index_cpp/get_package_share_directory.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <std_msgs/msg/int32.hpp>
#include <std_msgs/msg/float64.hpp>

// OpenGL and EGL includes
#include <X11/Xlib.h>
#include <X11/Xatom.h> // For XInternAtom
#include <X11/Xutil.h> // For XSetWindowAttributes
//#include <GL/gl.h>
//#include <GLES3/gl3.h>
#include <GLES2/gl2.h> 
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_glut.h>

// OpenCV includes
#if __has_include(<cv_bridge/cv_bridge.hpp>)
#include <cv_bridge/cv_bridge.hpp>
#else
#include <cv_bridge/cv_bridge.h>
#endif
#include <opencv2/imgproc/imgproc.hpp>

#include "gui/GUI.hpp"
#include "gui/screen2D.hpp"

namespace combat_robot_visualization {

class DisplayNode : public rclcpp::Node {
public:
    explicit DisplayNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions());
    virtual ~DisplayNode();

private:
    // ROS subscribers
    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr camera_sub_;
    rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr distance_sub_;
    rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr battery_sub_;
    rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr wind_speed_sub_;
    rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr bullet_sub_;

    // Callback functions
    void imageCallback(const sensor_msgs::msg::Image::SharedPtr msg);
    void distanceCallback(const std_msgs::msg::Float64::SharedPtr msg);
    void batteryCallback(const std_msgs::msg::Int32::SharedPtr msg);
    void windSpeedCallback(const std_msgs::msg::Int32::SharedPtr msg);
    void bulletCallback(const std_msgs::msg::Int32::SharedPtr msg);

    // OpenGL/EGL related members
    Display* display_;
    EGLDisplay egl_display_;
    EGLContext egl_context_;
    EGLSurface egl_surface_;
    Window x11_window_;
    
    // GUI related members
    GUI* gui_;
    Screen2D* main_screen_;
    ImGuiContext* imgui_context_;
    cv::Mat camera_frame_;
    
    // Camera texture for OpenGL
    unsigned int camera_texture_;
    int last_frame_width_ = 0;
    int last_frame_height_ = 0;

    // Mutex for thread-safe access to OpenGL resources
    std::mutex gl_mutex_;

    // Initialize functions
    bool initX11andEGL();
    void initGUI();
    
    // Timer for rendering
    rclcpp::TimerBase::SharedPtr render_timer_;
    void renderCallback();
};

}  // namespace combat_robot_visualization

#endif  // DISPLAY_NODE_HPP_
