#include "skyhunter_perception/yolo_detector_node.hpp"
#include <cv_bridge/cv_bridge.h>
#include <pcl_conversions/pcl_conversions.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <ament_index_cpp/get_package_share_directory.hpp>
#include <opencv2/highgui.hpp>
#include <omp.h>
#include <pcl/filters/voxel_grid.h>
#include <std_msgs/msg/bool.hpp>

YoloDetectorNode::YoloDetectorNode() : Node("yolo_detector_node") {
    std::string pkg_dir = ament_index_cpp::get_package_share_directory("skyhunter_perception");
    std::string model_path = pkg_dir + "/models/yolov8m.onnx";
    engine_ = std::make_unique<YoloEngine>(model_path, true);
    tracker_ = std::make_unique<byte_track::BYTETracker>(30, 30);

    img_cb_group_ = this->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
    scan_cb_group_ = this->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);

    tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
    tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

    // --- 2. ASSIGN GROUPS TO OPTIONS ---
    rclcpp::SubscriptionOptions img_opts;
    img_opts.callback_group = img_cb_group_;

    rclcpp::SubscriptionOptions scan_opts;
    scan_opts.callback_group = scan_cb_group_;

    last_sweep_time_ = this->get_clock()->now();

    auto qos = rclcpp::SensorDataQoS();
    
    this->declare_parameter<std::string>("robot_ns", "");
    my_ns_ = this->get_parameter("robot_ns").as_string();
    if (my_ns_.length() > 0 && my_ns_[0] == '/') my_ns_ = my_ns_.substr(1);
    is_leader_ = (my_ns_ == ""); 
    current_local_role_ = is_leader_ ? 2 : 0;

    std::string img_topic = is_leader_ ? "/rgb_camera/image_raw" : "/" + my_ns_ + "/rgb_camera/image_raw";
    std::string scan_topic = is_leader_ ? "/scan/points" : "/" + my_ns_ + "/scan/points";
    std::string odom_topic = is_leader_ ? "/odom" : "/" + my_ns_ + "/odom_filtered"; 
    std::string joint_topic = is_leader_ ? "/joint_states" : "/" + my_ns_ + "/joint_states";

    
    // sub_image_ = this->create_subscription<sensor_msgs::msg::Image>(img_topic, qos, std::bind(&YoloDetectorNode::image_callback, this, std::placeholders::_1));
    
    sub_image_ = this->create_subscription<sensor_msgs::msg::Image>(
            img_topic, qos, std::bind(&YoloDetectorNode::image_callback, this, std::placeholders::_1), img_opts);

    sub_halt_ = this->create_subscription<std_msgs::msg::Bool>(
        "/swarm/halt_active", 10,
        [this](const std_msgs::msg::Bool::SharedPtr msg) {
            in_halt_mode_ = msg->data;
            if (in_halt_mode_) {
                RCLCPP_INFO(this->get_logger(), "=== PERIMETER SWEEP HALT ACTIVATED ===");
            } else {
                RCLCPP_INFO(this->get_logger(), "Halt finished - back to driving mode");
            }
        });

    sub_odom_ = this->create_subscription<nav_msgs::msg::Odometry>("/odom", qos, 
        [this](const nav_msgs::msg::Odometry::SharedPtr msg) { 
            std::lock_guard<std::mutex> lock(odom_pan_mutex_); // LOCK 
            latest_odom_ = *msg; 
        });

    sub_joints_ = this->create_subscription<sensor_msgs::msg::JointState>("/joint_states", qos, 
        [this](const sensor_msgs::msg::JointState::SharedPtr msg) {
            std::lock_guard<std::mutex> lock(odom_pan_mutex_); // LOCK
            for(size_t i=0; i<msg->name.size(); i++) { 
                if(msg->name[i] == "gimbal_pan_joint") current_pan_ = msg->position[i]; 
            }
        });

    sub_swarm_state_ = this->create_subscription<skyhunter_msgs::msg::LeaderState>("/leader_state", 10, [this](const skyhunter_msgs::msg::LeaderState::SharedPtr msg) { last_swarm_msg_ = *msg; });

    sub_role_ = this->create_subscription<std_msgs::msg::Int8>(
            "local_role", 10, [this](const std_msgs::msg::Int8::SharedPtr msg) {
                current_local_role_ = msg->data;
                if (current_local_role_ == 2 && !was_leader_last_tick_) {
                    RCLCPP_WARN(this->get_logger(), "PROMOTED TO LEADER! Waking up YOLO & Radar...");
                    was_leader_last_tick_ = true;
                }
            });

    // --- VOTING SYSTEM PUBS/SUBS ---
    pub_confirm_ = this->create_publisher<std_msgs::msg::String>("/swarm/target_confirmations", 10);

    if (is_leader_) {
        // sub_scan_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(scan_topic, qos, std::bind(&YoloDetectorNode::scan_callback, this, std::placeholders::_1));
        sub_scan_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
                scan_topic, qos, std::bind(&YoloDetectorNode::scan_callback, this, std::placeholders::_1), scan_opts);

        // pub_leader_state_ = this->create_publisher<skyhunter_msgs::msg::LeaderState>("leader_state", 10);
        pub_combat_state_ = this->create_publisher<skyhunter_msgs::msg::LeaderState>("perception/combat_state", 10);
        
        sub_confirm_ = this->create_subscription<std_msgs::msg::String>(
            "/swarm/target_confirmations", 10, [this](const std_msgs::msg::String::SharedPtr msg) {
                RCLCPP_INFO(this->get_logger(), ">>> LEADER RECEIVED VOTE FROM: %s", msg->data.c_str());
                follower_confirmations_[msg->data] = this->get_clock()->now();
            });

        cv::namedWindow("LEADER_TACTICAL_VIEW", cv::WINDOW_AUTOSIZE);
        RCLCPP_INFO(this->get_logger(), "[SH_01] LEADER PREDATOR MODE ONLINE.");
    } else {
        RCLCPP_INFO(this->get_logger(), "[%s] FOLLOWER SNIPER MODE ONLINE.", my_ns_.c_str());
    }

    pub_detections_ = this->create_publisher<skyhunter_msgs::msg::DetectionArray>("swarm/detections", 10);
    pub_pan_ = this->create_publisher<std_msgs::msg::Float64>("gimbal/pan_cmd", 10);
    pub_tilt_ = this->create_publisher<std_msgs::msg::Float64>("gimbal/tilt_cmd", 10);
    pub_overlay_ = this->create_publisher<sensor_msgs::msg::Image>("perception/overlay", 10);
}

YoloDetectorNode::~YoloDetectorNode() {
    if(is_leader_) cv::destroyWindow("LEADER_TACTICAL_VIEW");
}

void YoloDetectorNode::scan_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg) {

    // if (current_local_role_ != 2 && last_swarm_msg_.swarm_state < 3) return;

    if (current_local_role_ != 2 && !in_halt_mode_ && last_swarm_msg_.swarm_state < 3) {
        return;
    }
    
    // --- 1. MEMORY OPTIMIZATION ---
    // Use static allocations to prevent memory fragmentation at 10+ Hz
    static pcl::PointCloud<pcl::PointXYZ> raw_cloud;
    static pcl::PointCloud<pcl::PointXYZ> filtered_cloud;
    
    raw_cloud.clear();
    filtered_cloud.clear();
    
    pcl::fromROSMsg(*msg, raw_cloud);
    if (raw_cloud.empty()) return;

    // --- 2. FAST DOWNSAMPLING ---
    // Voxel grid to drastically reduce points before the math loop
    pcl::VoxelGrid<pcl::PointXYZ> vg;
    vg.setInputCloud(raw_cloud.makeShared()); // Pass a temporary shared_ptr for API compliance
    vg.setLeafSize(0.2f, 0.2f, 0.2f); 
    vg.filter(filtered_cloud);

    // --- 3. HARDWARE-ACCELERATED ANGULAR FILTERING ---
    const int num_bins = 360;
    std::vector<int> bin_counts(num_bins, 0);
    std::vector<double> bin_min_dist_sq(num_bins, 10000.0); // Use Squared distance

    // Pre-calculate squared thresholds to avoid sqrt() in the heavy loop
    const double min_d_sq = 0.8 * 0.8; // 0.64
    const double max_d_sq = 5.0 * 5.0; // 25.0

    // Using OpenMP for loop vectorization
    #pragma omp parallel
    {
        // Thread-local variables to prevent race conditions
        std::vector<int> local_bin_counts(num_bins, 0);
        std::vector<double> local_bin_min_dist_sq(num_bins, 10000.0);

        #pragma omp for nowait
        for (size_t i = 0; i < filtered_cloud.points.size(); ++i) {
            const auto& p = filtered_cloud.points[i];
            
            // Fast Z & X filter (Ignore floor/sky, ignore rear)
            if (p.z < -0.2 || p.z > 0.6) continue; 
            if (p.x < -0.5) continue;              

            // FAST MATH: No sqrt()!
            double d_sq = (p.x * p.x) + (p.y * p.y);
            if (d_sq < min_d_sq || d_sq > max_d_sq) continue;      

            // Fast Angle Calculation
            double angle_deg = (std::atan2(p.y, p.x) * 180.0 / M_PI) + 180.0;
            int idx = (int)angle_deg % num_bins;

            local_bin_counts[idx]++;
            if (d_sq < local_bin_min_dist_sq[idx]) {
                local_bin_min_dist_sq[idx] = d_sq;
            }
        }

        // Merge thread-local results back into the global arrays
        #pragma omp critical
        {
            for (int i = 0; i < num_bins; ++i) {
                bin_counts[i] += local_bin_counts[i];
                if (local_bin_min_dist_sq[i] < bin_min_dist_sq[i]) {
                    bin_min_dist_sq[i] = local_bin_min_dist_sq[i];
                }
            }
        }
    }

    // --- 4. FIND THE PRIMARY THREAT ---
    double best_dist = 100.0;
    double best_angle = 0.0;
    bool found = false;

    for (int i = 0; i < num_bins; ++i) {
        // Less than 6 points = Person/Pole. More than 6 = Solid Wall.
        if (bin_counts[i] > 0 && bin_counts[i] < 6) {
            // ONLY do the square root here, on the very final candidates!
            double actual_dist = std::sqrt(bin_min_dist_sq[i]); 
            if (actual_dist < best_dist) {
                best_dist = actual_dist;
                best_angle = (i - 180) * M_PI / 180.0; // Convert back to Radians for Gimbal
                found = true;
            }
        }
    }

    std::lock_guard<std::mutex> lock(radar_mutex_); // Lock memory

    // --- 5. UPDATE CLASS STATE ---
    closest_blob_dist_ = best_dist;
    closest_blob_angle_ = best_angle;
    blob_detected_ = found;
}

void YoloDetectorNode::image_callback(const sensor_msgs::msg::Image::SharedPtr msg) {
    
    // --- 1. TACTICAL SLEEP MODE ---
    // if (current_local_role_ != 2 && last_swarm_msg_.swarm_state < 3) return;

    double leader_vel = std::abs(last_swarm_msg_.velocity.linear.x);
    if (current_local_role_ != 2 && !in_halt_mode_ && last_swarm_msg_.swarm_state < 3) {
        return;
    }

    cv_bridge::CvImagePtr cv_ptr;
    try { cv_ptr = cv_bridge::toCvCopy(msg, "bgr8"); } catch (...) { return; }

    // --- 2. ACTIVE INFERENCE ---
    auto results = engine_->run_inference(cv_ptr->image);
    std::vector<byte_track::Object> objects;
    
    for (const auto& res : results) {
        bool is_person = (engine_->class_names[res.class_id] == "person");
        bool is_frisbee = (res.class_id == 74);

        if (is_person || is_frisbee) {
            byte_track::Rect<float> rect(res.box.x, res.box.y, res.box.width, res.box.height);
            objects.emplace_back(rect, res.class_id, res.confidence);
        }
    }
    
    auto tracked_targets = tracker_->update(objects, cv::Mat::eye(2, 3, CV_64F));

    int best_idx = -1; 
    double min_dist_m = 1e9;
    std::vector<double> current_dists;
    
    for (size_t i = 0; i < tracked_targets.size(); ++i) {
        double height_factor = (tracked_targets[i]->getScore() > 0.5) ? 1.7 : 0.3; 
        double d = (550.0 * height_factor) / tracked_targets[i]->getRect().height();
        current_dists.push_back(d);
        if (d < min_dist_m) { min_dist_m = d; best_idx = i; }
    }
    
    skyhunter_msgs::msg::DetectionArray det_msg;
    det_msg.header = msg->header;
    std_msgs::msg::Float64 pan_cmd, tilt_cmd;
    pan_cmd.data = 0.0; tilt_cmd.data = 0.0; 
    
    TrackingState current_state = PATROL; 
    bool ai_sees_target = !tracked_targets.empty();

    // --- LEADER VOTE COUNTING ---
    int active_votes = 0;
    if (is_leader_) {
        auto now = this->get_clock()->now();
        for (const auto& [f_ns, time] : follower_confirmations_) {
            if ((now - time).seconds() < 1.5) active_votes++;
        }
    }

    // STATIC HYSTERESIS VARIABLE ---
    static int blind_frames = 0;

    if (ai_sees_target) {
        blind_frames = 0; // Reset counter because AI sees the target

        if (!is_leader_ && (last_swarm_msg_.swarm_state == 3 || last_swarm_msg_.swarm_state == 4)) {
            std_msgs::msg::String vote; vote.data = my_ns_;
            pub_confirm_->publish(vote);
            cv::putText(cv_ptr->image, "VOTING: TARGET CONFIRMED", cv::Point(20, 90), 0, 0.7, cv::Scalar(0, 255, 0), 2);
        }

        for (size_t i = 0; i < tracked_targets.size(); ++i) {
            auto r = tracked_targets[i]->getRect();
            cv::Rect box(r.x(), r.y(), r.width(), r.height());
            std::string label = "ID:" + std::to_string(tracked_targets[i]->getTrackId()) + " " + std::to_string(current_dists[i]).substr(0,4) + "m";
            
            skyhunter_msgs::msg::Detection det;
            det.class_id = tracked_targets[i]->getTrackId(); 
            det.label = "person";
            det.confidence = current_dists[i];
            det.x = box.x; det.y = box.y; det.w = box.width; det.h = box.height;
            det_msg.detections.push_back(det);

            if ((int)i == best_idx) {
                // ** LEAD POINT LOGIC (RCWS FIRE SOLUTION) **
                std::vector<float> vel = tracked_targets[i]->getVelocity();
                float vx = vel[0]; float vy = vel[1];
                float lead_time = 0.5f; 
                float pred_cx = (r.x() + r.width()/2.0f) + (vx * lead_time);
                float pred_cy = (r.y() + r.height()/2.0f) + (vy * lead_time);

                // --- TACTICAL COLOR & TEXT LOGIC ---
                cv::Scalar target_color;
                std::string target_text;

                if (is_leader_) {
                    if (active_votes > 1) {
                        target_color = cv::Scalar(0, 0, 255); // Red
                        target_text = "TARGET CONFIRMED " + label;
                        current_state = ENGAGED;
                    } else {
                        target_color = cv::Scalar(0, 255, 255); // Yellow
                        target_text = "WAITING FOR VOTES " + label;
                        current_state = COMBAT;
                    }
                } else {
                    target_color = cv::Scalar(0, 165, 255); // Orange
                    target_text = "SLAVE LOCK " + label;
                }

                // DRAW VISUALS DYNAMICALLY
                cv::rectangle(cv_ptr->image, box, target_color, 3); 
                cv::circle(cv_ptr->image, cv::Point(pred_cx, pred_cy), 8, target_color, -1); 
                cv::putText(cv_ptr->image, "LEAD", cv::Point(pred_cx+10, pred_cy), 0, 0.5, target_color, 1);
                cv::putText(cv_ptr->image, target_text, cv::Point(r.x(), r.y() - 10), 0, 0.6, target_color, 2);
                
                // --- THE FULL PID CONTROLLER ---
                double err_x = (pred_cx - 320.0) / 320.0;
                double err_y = (pred_cy - 240.0) / 240.0;

                static double integral_x = 0.0;
                static double integral_y = 0.0;
                double p_gain_pan = -1.2; double i_gain_pan = -0.15; 
                double p_gain_tilt = 0.8; double i_gain_tilt = 0.1;  

                if (std::abs(pred_cx - 320.0) < 5.0 && std::abs(pred_cy - 240.0) < 5.0) {
                    pan_cmd.data = 0.0; tilt_cmd.data = 0.0;
                    integral_x = 0.0; integral_y = 0.0;
                } else {
                    integral_x += err_x; integral_y += err_y;
                    integral_x = std::max(-2.0, std::min(2.0, integral_x));
                    integral_y = std::max(-2.0, std::min(2.0, integral_y));

                    double p_out_x = std::copysign(std::pow(std::abs(err_x), 0.7), err_x) * p_gain_pan;
                    double p_out_y = std::copysign(std::pow(std::abs(err_y), 0.7), err_y) * p_gain_tilt;

                    pan_cmd.data = p_out_x + (integral_x * i_gain_pan);
                    tilt_cmd.data = p_out_y + (integral_y * i_gain_tilt);
                }

                // --- LEADER STATE BROADCAST ---
                if (is_leader_) {
                    skyhunter_msgs::msg::LeaderState state_msg;
                    state_msg.header.stamp = this->get_clock()->now();

                    // double r_yaw = tf2::getYaw(latest_odom_.pose.pose.orientation);
                    // double lead_angle_offset = std::atan2(320.0 - pred_cx, 550.0); 
                    // double total_angle = r_yaw + current_pan_ - lead_angle_offset;

                    // state_msg.target_pos.x = latest_odom_.pose.pose.position.x + (current_dists[i] * std::cos(total_angle));
                    // state_msg.target_pos.y = latest_odom_.pose.pose.position.y + (current_dists[i] * std::sin(total_angle));
                    // state_msg.target_locked = true; 

                    // --- FIX 4: Use Global Position for Targets ---
                    // double global_x = latest_odom_.pose.pose.position.x;
                    // double global_y = latest_odom_.pose.pose.position.y;
                    // double r_yaw = tf2::getYaw(latest_odom_.pose.pose.orientation);

                    // try {
                    //     auto tf_now = tf_buffer_->lookupTransform("map", "base_footprint", tf2::TimePointZero);
                    //     global_x = tf_now.transform.translation.x;
                    //     global_y = tf_now.transform.translation.y;
                    //     r_yaw = tf2::getYaw(tf_now.transform.rotation);
                    // } catch (...) {
                    //     RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "TF map to base_footprint not ready, using odom");
                    // }



                    double global_x = latest_odom_.pose.pose.position.x;
                    double global_y = latest_odom_.pose.pose.position.y;
                    double r_yaw = tf2::getYaw(latest_odom_.pose.pose.orientation);

                    try {
                        // --- CRITICAL FIX 2: DYNAMIC FRAME FOR FOLLOWERS ---
                        std::string my_frame = is_leader_ ? "base_footprint" : my_ns_ + "/base_footprint";
                        auto tf_now = tf_buffer_->lookupTransform("map", my_frame, tf2::TimePointZero);
                        
                        global_x = tf_now.transform.translation.x;
                        global_y = tf_now.transform.translation.y;
                        r_yaw = tf2::getYaw(tf_now.transform.rotation);
                    } catch (...) {
                        RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "TF map to base_footprint not ready, using odom");
                    }


                    double lead_angle_offset = std::atan2(320.0 - pred_cx, 550.0); 
                    double total_angle = r_yaw + current_pan_ - lead_angle_offset;

                    // Publish based on global_x and global_y
                    state_msg.target_pos.x = global_x + (current_dists[i] * std::cos(total_angle));
                    state_msg.target_pos.y = global_y + (current_dists[i] * std::sin(total_angle));
                    state_msg.target_locked = true;

                    if (current_dists[i] < 10.0) {
                        if (active_votes >= 1) {
                            state_msg.swarm_state = 4; // ENGAGED / FIRE NET
                            state_msg.target_locked = true;
                            RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
                                "FIRE NET ACTIVATED - Target locked at (%.1f, %.1f) with %d votes",
                                state_msg.target_pos.x, state_msg.target_pos.y, active_votes);
                        } else {
                            state_msg.swarm_state = 3; // CUEING
                            state_msg.target_locked = true;
                        }
                    } 
                    else {
                        state_msg.swarm_state = 0;
                        state_msg.target_locked = false;
                    }

                    
                    // pub_leader_state_->publish(state_msg);
                    pub_combat_state_->publish(state_msg);
                }
            } else {
                // Secondary Targets (GREEN)
                cv::rectangle(cv_ptr->image, box, cv::Scalar(0, 255, 0), 1);
                cv::putText(cv_ptr->image, label, cv::Point(r.x(), r.y() - 5), 0, 0.4, cv::Scalar(0, 255, 0), 1);
            }
        }
    }
    else {
        // ==========================================
        // NO AI TARGETS SEEN - BLIND SPOT LOGIC
        // ==========================================
        blind_frames++;

        double safe_blob_dist, safe_blob_angle;
        bool safe_blob_detected;
        {
            std::lock_guard<std::mutex> lock(radar_mutex_);
            safe_blob_dist = closest_blob_dist_;
            safe_blob_angle = closest_blob_angle_;
            safe_blob_detected = blob_detected_;
        }

        nav_msgs::msg::Odometry safe_odom;
        double safe_pan;
        {
            std::lock_guard<std::mutex> lock(odom_pan_mutex_);
            safe_odom = latest_odom_;
            safe_pan = current_pan_;
        }

        if (in_halt_mode_) {
            auto now = this->get_clock()->now();
            double dt = (now - last_sweep_time_).seconds();

            if (dt > 0.0) {
                const double PAN_SPEED_RAD_S = 60.0 * M_PI / 180.0;  // 60°/sec
                const double MAX_PAN_RAD = 30.0 * M_PI / 180.0;      // ±30°

                current_sweep_pan_ += sweep_direction_ * PAN_SPEED_RAD_S * dt;

                if (current_sweep_pan_ > MAX_PAN_RAD) {
                    current_sweep_pan_ = MAX_PAN_RAD;
                    sweep_direction_ = -1;
                } else if (current_sweep_pan_ < -MAX_PAN_RAD) {
                    current_sweep_pan_ = -MAX_PAN_RAD;
                    sweep_direction_ = 1;
                }
            }
            last_sweep_time_ = now;

            pan_cmd.data = current_sweep_pan_;
            tilt_cmd.data = 0.0;

            cv::putText(cv_ptr->image, "PERIMETER SWEEP (HALT 20s)", cv::Point(20, 60), 0, 0.7,
                        cv::Scalar(0, 255, 255), 2);
        }

        else if (is_leader_ && safe_blob_detected && safe_blob_dist < 5.0 && blind_frames > 15) {
            current_state = SCANNING;
            
            // Use absolute angle from LiDAR, do not add current_pan
            pan_cmd.data = safe_blob_angle;
            tilt_cmd.data = 0.0; // Look straight ahead while checking
            
            cv::putText(cv_ptr->image, "RADAR CUEING...", cv::Point(20, 60), 0, 0.7, cv::Scalar(255, 0, 255), 2);
            cv::putText(cv_ptr->image, "ANG: " + std::to_string(safe_blob_angle*57.3).substr(0,4) + " deg", cv::Point(20, 90), 0, 0.7, cv::Scalar(255, 0, 255), 2);
        }

        else if (is_leader_) {
            // The AI sees nothing, so we MUST tell the swarm to stand down (PATROL).
            skyhunter_msgs::msg::LeaderState empty_state_msg;
            empty_state_msg.header.stamp = this->get_clock()->now();
            empty_state_msg.target_locked = false; 
            empty_state_msg.swarm_state = 0; 

            {
                std::lock_guard<std::mutex> lock(odom_pan_mutex_);
                empty_state_msg.pose = latest_odom_.pose.pose;
                empty_state_msg.velocity = latest_odom_.twist.twist;
            }

            // pub_leader_state_->publish(empty_state_msg);
            pub_combat_state_->publish(empty_state_msg);
        }

        // 5. FOLLOWER SLAVE MODE
        else if (!is_leader_ && (last_swarm_msg_.swarm_state == 3 || last_swarm_msg_.swarm_state == 4)) {
            current_state = COMBAT;
            double tx = last_swarm_msg_.target_pos.x;
            double ty = last_swarm_msg_.target_pos.y;
            double my_x = latest_odom_.pose.pose.position.x;
            double my_y = latest_odom_.pose.pose.position.y;
            double my_yaw = tf2::getYaw(latest_odom_.pose.pose.orientation);
            
            double global_angle = std::atan2(ty - my_y, tx - my_x);
            double p_angle = global_angle - my_yaw;
            while (p_angle > M_PI) p_angle -= 2 * M_PI;
            while (p_angle < -M_PI) p_angle += 2 * M_PI;
            pan_cmd.data = p_angle;
            cv::putText(cv_ptr->image, "SEARCHING LEADER COORDS", cv::Point(20, 60), 0, 0.7, cv::Scalar(0, 165, 255), 2);
        }
        else {
            // PATROL: Look Straight
            pan_cmd.data = 0.0;
            tilt_cmd.data = 0.0;

            current_sweep_pan_ = 0.0;
            sweep_direction_ = 1;
        }
    }

    // Draw Status
    std::string status_txt = "PATROL";
    if (current_state == SCANNING) status_txt = "SCANNING";
    if (current_state == COMBAT) status_txt = "COMBAT (CUE)";
    if (current_state == ENGAGED) status_txt = "ENGAGED (FIRE NET READY)";
    cv::putText(cv_ptr->image, "MODE: " + status_txt, cv::Point(20, 30), 0, 0.6, cv::Scalar(255, 255, 0), 2);

    pub_pan_->publish(pan_cmd); 
    pub_tilt_->publish(tilt_cmd);
    pub_detections_->publish(det_msg);

    cv::drawMarker(cv_ptr->image, cv::Point(320, 240), cv::Scalar(180, 180, 0), cv::MARKER_CROSS, 25, 2);
    pub_overlay_->publish(*(cv_ptr->toImageMsg()));
}