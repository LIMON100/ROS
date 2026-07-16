#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#if __has_include(<cv_bridge/cv_bridge.hpp>)
#include <cv_bridge/cv_bridge.hpp>
#else
#include <cv_bridge/cv_bridge.h>
#endif
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

class ImageViewer : public rclcpp::Node {
public:
  ImageViewer() : Node("image_best_effort_viewer"), frame_count_(0),
                  clock_(RCL_SYSTEM_TIME),
                  last_display_time_(0, 0, RCL_SYSTEM_TIME),
                  last_fps_time_(0, 0, RCL_SYSTEM_TIME) {

    rclcpp::QoS qos(rclcpp::KeepLast(10));
    qos.best_effort();  // 퍼블리셔와 QoS 일치 필수

    sub_ = this->create_subscription<sensor_msgs::msg::Image>(
      "/human_detector/human/image_raw", qos,
      std::bind(&ImageViewer::image_callback, this, std::placeholders::_1));

    cv::startWindowThread();
    cv::namedWindow("Camera View", cv::WINDOW_AUTOSIZE);
  }

private:
  void image_callback(const sensor_msgs::msg::Image::SharedPtr msg) {
    auto now = clock_.now();
    const double min_interval_sec = 0.06;  // 약 16 FPS 제한

    if ((now - last_display_time_).seconds() < min_interval_sec) return;
    last_display_time_ = now;

    try {
      auto cv_image = cv_bridge::toCvShare(msg, "bgr8");

      // 해상도 축소: 1920x1080 → 640x480
      cv::Mat resized_image;
      cv::resize(cv_image->image, resized_image, cv::Size(640, 480), 0, 0, cv::INTER_NEAREST);

      // 화면 표시
      cv::imshow("Camera View", resized_image);
      cv::waitKey(1);

      // FPS 로그 출력
      fps_counter_++;
      if ((now - last_fps_time_).seconds() >= 1.0) {
        RCLCPP_INFO(this->get_logger(), "Viewer FPS: %d", fps_counter_);
        fps_counter_ = 0;
        last_fps_time_ = now;
      }

    } catch (cv_bridge::Exception &e) {
      RCLCPP_ERROR(this->get_logger(), "cv_bridge exception: %s", e.what());
    }
  }

  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr sub_;
  rclcpp::Clock clock_;
  rclcpp::Time last_display_time_;
  rclcpp::Time last_fps_time_;
  int frame_count_;
  int fps_counter_ = 0;
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<ImageViewer>();
  rclcpp::executors::MultiThreadedExecutor exec;
  exec.add_node(node);
  exec.spin();
  rclcpp::shutdown();
  return 0;
}
