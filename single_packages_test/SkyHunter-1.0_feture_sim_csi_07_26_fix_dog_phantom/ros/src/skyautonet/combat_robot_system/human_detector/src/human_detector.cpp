#include <filesystem>
#include <ament_index_cpp/get_package_share_directory.hpp>

#include "human_detector.hpp" // Adjusted include path

namespace human_detector {

HumanDetectorComponent::HumanDetectorComponent(const rclcpp::NodeOptions & options)
 : Node("human_detector", options) // Changed node name
{
    InitRosParam();
    InitRosCommon();
    InitCommonClass();
    RCLCPP_INFO(this->get_logger(), "human_detector initialization");
}

void HumanDetectorComponent::InitRosParam() {
    std::string model_path = declare_parameter("model_path", "");

    if (model_path.empty()) {
        try {
            std::string share_dir = ament_index_cpp::get_package_share_directory("human_detector");
            model_path = share_dir + "/models/";
            RCLCPP_INFO(this->get_logger(), "Model path set to default: %s", model_path.c_str());
        } catch (const std::exception& e) {
            RCLCPP_ERROR(this->get_logger(), "Failed to get package share directory: %s", e.what());
        }
    }
    
    // common param
    m_is_compressed = declare_parameter("is_compressed", false);
    
    // hailo model param
    m_label_text_path        = model_path + declare_parameter("label_text_path", "");
    m_hailo_param.model_path = model_path + declare_parameter("hailo_model", "");
    m_hailo_param.box_thres  = declare_parameter("box_thresh", 0.25);
    m_human_thresh = declare_parameter("human_thresh", 0.6);
    m_drone_thresh = declare_parameter("drone_thresh", 0.3);
    m_class_num = declare_parameter("class_num", 2); 

    // debug param
    m_debug_mode = declare_parameter("debug_mode", false);
    m_save_image = declare_parameter("save_image", false);
    m_save_dir   = declare_parameter("save_image_path", "");

    // print param
    RCLCPP_INFO(this->get_logger(), "---------------- ros param -------------- ");
    RCLCPP_INFO(this->get_logger(), "-- common param --");
    RCLCPP_INFO(this->get_logger(), "is_compressed : %d\n", m_is_compressed);

    RCLCPP_INFO(this->get_logger(), "-- hailo model param --");
    RCLCPP_INFO(this->get_logger(), "label_text_path : %s", m_label_text_path.c_str());
    RCLCPP_INFO(this->get_logger(), "model_path      : %s", m_hailo_param.model_path.c_str());
    RCLCPP_INFO(this->get_logger(), "box_thres       : %f\n", m_hailo_param.box_thres);
    RCLCPP_INFO(this->get_logger(), "human_thresh    : %f", m_human_thresh);
    RCLCPP_INFO(this->get_logger(), "drone_thresh    : %f", m_drone_thresh);
    
    RCLCPP_INFO(this->get_logger(), "-- debug param --");
    RCLCPP_INFO(this->get_logger(), "debug_mode : %d", m_debug_mode);
    RCLCPP_INFO(this->get_logger(), "save_image : %d", m_save_image);
    RCLCPP_INFO(this->get_logger(), "save_dir   : %s", m_save_dir.c_str());
    RCLCPP_INFO(this->get_logger(), "---------------- ros param -------------- ");
}

void HumanDetectorComponent::InitRosCommon() {    
    rclcpp::QoS qos_profile(rclcpp::KeepLast(10));
    //qos_profile.reliability(rclcpp::ReliabilityPolicy::Reliable);
    qos_profile.reliability(rclcpp::ReliabilityPolicy::BestEffort);
    qos_profile.durability(rclcpp::DurabilityPolicy::Volatile);

    m_pub_human_objects = this->create_publisher<DetectedObjectsMsg>("out/human/info", qos_profile);
    m_pub_target_point = this->create_publisher<TargetPointMsg>("out/human/target_point", qos_profile); 

    if (m_debug_mode) {
        m_pub_human_image = image_transport::create_publisher(this, "out/human/image",
         													qos_profile.get_rmw_qos_profile());
    }

    std::string transport = m_is_compressed ? "compressed" : "raw";

    std::string node_namespace(this->get_namespace());
    if (!node_namespace.empty() && node_namespace.back() != '/') {
        node_namespace += "/";
    }

    m_sub_camera_image = image_transport::create_subscription(this, node_namespace + "in/image",
                                                              std::bind(&HumanDetectorComponent::CallbackImage, this, std::placeholders::_1),
                                                              transport, qos_profile.get_rmw_qos_profile());
}

void HumanDetectorComponent::InitCommonClass() {
    m_detector = std::make_shared<DetectionProcessor>(m_hailo_param, m_label_text_path);

    m_save_count = 0;
    if (m_debug_mode && m_save_image) {
        MakeDir(m_save_dir);
    }
}

void HumanDetectorComponent::MakeDir(const std::string& dir) {
    try {
        if (!std::filesystem::exists(dir)) {
            if (!std::filesystem::create_directories(dir)) {
                std::cerr << "Error: Failed to create directory: " << dir << std::endl;
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error in MakeDir(" << dir << "): " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception in MakeDir(" << dir << "): " << e.what() << std::endl;
    }
}

int HumanDetectorComponent::selectTargetByPriority(const std::vector<hailo::util::object_t>& human_objects, const cv::Size& image_size, const std::vector<int>& track_ids) {
    // Priority:
    // 1. Inside a central frame (simulating a narrower FOV).
    // 2. Class: Head (0) & Torso (2) > Arm (1) > Others.
    // 3. Proximity to the image center.
    // 4. Track ID consistency (prefer objects with valid track IDs)
    if (human_objects.empty()) {
        return -1;
    }

    // Define the central frame (e.g., inner 50% of the image).
    // This could be made a configurable ROS parameter.
    const float central_frame_scale = 0.5f;
    cv::Rect2f central_frame(
        image_size.width * (1.0f - central_frame_scale) / 2.0f,
        image_size.height * (1.0f - central_frame_scale) / 2.0f,
        image_size.width * central_frame_scale,
        image_size.height * central_frame_scale
    );
    cv::Point2f image_center(image_size.width / 2.0f, image_size.height / 2.0f);

    struct TargetCandidate {
        int index;
        int location_priority; // 0: inside, 1: outside
        int class_priority;    // 0: head/torso, 1: arm, 2: others
        int track_priority;    // 0: has track ID, 1: no track ID
        double distance_to_center;

        // For sorting: lower value means higher priority
        // Set Priority here
        bool operator<(const TargetCandidate& other) const {
            if (track_priority != other.track_priority) {
                return track_priority < other.track_priority;
            }
            if (location_priority != other.location_priority) {
                return location_priority < other.location_priority;
            }
            if (class_priority != other.class_priority) {
                return class_priority < other.class_priority;
            }
            return distance_to_center < other.distance_to_center;
        }
    };

    std::vector<TargetCandidate> candidates;
    for (size_t i = 0; i < human_objects.size(); ++i) {
        const auto& obj = human_objects[i];
        cv::Point2f obj_center(obj.box.x + obj.box.width / 2.0f, obj.box.y + obj.box.height / 2.0f);

        int location_prio = central_frame.contains(obj_center) ? 0 : 1;
        
        // Initialize with a default low priority to avoid uninitialized variable warning.
        int class_prio = 3;
        if (m_class_num == 1) {
            class_prio = 0; // Only one class, so treat it as the highest priority
        } else if (m_class_num == 2) {
            // person_drone model
            // person: 0, drone: 1
            if (obj.class_id == 0) {
                class_prio = 0; // person high priority
            } else {
                class_prio = 1; // drone lower priority
            }
        } else if (m_class_num == 4) {
            // Multiple classes, assign priorities based on class_id
            switch (obj.class_id) {
                case HEAD: // head
                    class_prio = 0;
                    break;
                case TORSO: // torso
                    class_prio = 1;
                    break;
                case ARMS: // arm
                    class_prio = 2;
                    break;
                default: // legs, etc.
                    class_prio = 3;
                    break;
            }
        }

        int track_prio = (i < track_ids.size() && track_ids[i] >= 0) ? 0 : 1;

        double dist = cv::norm(obj_center - image_center);
        candidates.push_back({static_cast<int>(i), location_prio, class_prio, track_prio, dist});
    }

    if (candidates.empty()) {
        return -1;
    }

    // Sort candidates to find the one with the highest priority
    std::sort(candidates.begin(), candidates.end());

    return candidates.front().index;
}

TargetInfo HumanDetectorComponent::GetTargetPoint(const std::vector<hailo::util::object_t>& human_objects, const cv::Size& image_size, const std::vector<int>& track_ids) {
    TargetInfo target_info;
    int target_idx = selectTargetByPriority(human_objects, image_size, track_ids);

    if (target_idx == -1) {
        return target_info; // Return default (not found)
    }

    const hailo::util::object_t& target = human_objects[target_idx];

    target_info.is_found = true;
    target_info.norm_x = (target.box.x + target.box.width / 2.0) / image_size.width;
    target_info.norm_y = (target.box.y + target.box.height / 2.0) / image_size.height;
    target_info.norm_h = static_cast<float>(target.box.height) / image_size.height;
    target_info.track_id = track_ids[target_idx];
    target_info.class_id = target.class_id;
    target_info.box = target.box;

    return target_info;
}

void HumanDetectorComponent::getTrackID(std::vector<Track>& track_list, std::vector<hailo::util::object_t>& detected_objects, std::vector<int>& track_ids) {
    int T = (int)track_list.size();
    int D = (int)detected_objects.size();
    int N = std::max(T, D);

    std::vector<std::vector<float>> cost_matrix(N, std::vector<float>(N, 1.0f));

    for(int i = 0; i < T; i++){
        const auto& tb = track_list[i].GetLatestData().bbox;
        for(int j = 0; j < D; j++){
            BoundingBox det;
            det.x = detected_objects[j].box.x;
            det.y = detected_objects[j].box.y;   
            det.w = detected_objects[j].box.width;
            det.h = detected_objects[j].box.height;
            det.class_id = detected_objects[j].class_id;
            det.label = "human";
            det.score = detected_objects[j].prob;
            float iou = BoundingBoxUtils::CalculateIoU(tb, det);       
            
            if(iou >= 0.3){
            }
            else if(iou < 0.1){
                iou = 0.0f;
            }
            else{
                if(tb.class_id != det.class_id){
                    iou = 0.0f;
                }
            }

            cost_matrix[i][j] = 1.0f - iou;     
        }
    }

    std::vector<int> det_for_track(N, -1);
    std::vector<int> track_for_det(N, -1);

    if(track_list.size() > 0 && detected_objects.size() > 0) {
        HungarianAlgorithm<float> solver(cost_matrix);
        solver.Solve(det_for_track, track_for_det);
    }

    track_ids.clear();
    track_ids.resize(detected_objects.size(), -1);
    
    for(int j = 0; j < D; j++){
        int ti = track_for_det[j];
        if(ti >= 0 && ti < T && cost_matrix[ti][j] < 1.0f){
            track_ids[j] = track_list[ti].GetId();
        }
        else{
            track_ids[j] = -1;
        }
    }
}

void HumanDetectorComponent::CallbackImage(const sensor_msgs::msg::Image::ConstSharedPtr& msg) {
    // FPS calculation
    m_frame_count++;
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - m_last_fps_time).count();
 
    if (duration >= 5.0) { // Print FPS every 5 seconds
        double fps = static_cast<double>(m_frame_count) / duration;
        RCLCPP_INFO(this->get_logger(), "FPS: %.2f", fps);
        m_frame_count = 0;
        m_last_fps_time = now;
    }

    cv_bridge::CvImagePtr image_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::RGB8);
    cv::Mat& image = image_ptr->image;

    //RCLCPP_INFO(this->get_logger(), "Received image: %dx%d", image.cols, image.rows);

    // Detection
    std::vector<hailo::util::object_t> human_objects = m_detector->hailo_process(image);
    
    // DEBUG: Log raw detections before filtering
    {
        int person_count = 0, drone_count = 0;
        for (const auto& obj : human_objects) {
            if (obj.class_id == 0) person_count++;
            else if (obj.class_id == 1) drone_count++;
        }
        RCLCPP_INFO_THROTTLE(this->get_logger(), *get_clock(), 2000, 
            "[RAW] Total: %zu | Person(0): %d | Drone(1): %d", 
            human_objects.size(), person_count, drone_count);
    }
    
    // Filter by class specific threshold
    auto it = human_objects.begin();
    while (it != human_objects.end()) {
        bool keep = true;
        if (it->class_id == 0) { // Person
            if (it->prob < m_human_thresh) keep = false;
        } else if (it->class_id == 1) { // Drone
            if (it->prob < m_drone_thresh) keep = false;
        }
        
        if (!keep) {
            it = human_objects.erase(it);
        } else {
            ++it;
        }
    }

    //RCLCPP_INFO(this->get_logger(), "Detected %zu human objects", human_objects.size());

    // Convert hailo objects to BoundingBox format for tracking
    std::vector<BoundingBox> bbox_list;
    bbox_list.reserve(human_objects.size());
    
    for (const auto& human : human_objects) {
        if (human.box.area() > 100000) {
            continue; // Skip invalid boxes
        }
        bbox_list.push_back({human.class_id, "human", human.prob,
                             human.box.x, human.box.y,
                             human.box.width, human.box.height});
    }
    
    // Update tracker
    m_tracker.Update(bbox_list);
    auto& track_list = m_tracker.GetTrackList();
    
    // Assign track IDs to detected objects using Hungarian algorithm
    std::vector<int> track_ids;
    getTrackID(track_list, human_objects, track_ids);
    
    // publish target point
    TargetInfo target_info = GetTargetPoint(human_objects, image.size(), track_ids);
    TargetPointMsg msg_target_point;
    msg_target_point.header = msg->header;
    if (!target_info.is_found) {
        // If no valid target point, set to -1
        msg_target_point.is_locked = false;
        msg_target_point.x = -1;
        msg_target_point.y = -1;
        msg_target_point.height = 0;
        msg_target_point.class_id = -1;
        msg_target_point.track_id = -1;
    } else {
        // Set the target point to the center of the largest detected object
        // x,y is normalized to [0,1]
        msg_target_point.is_locked = true;
        msg_target_point.x = target_info.norm_x;
        msg_target_point.y = target_info.norm_y;
        msg_target_point.height = target_info.norm_h;
        msg_target_point.track_id = target_info.track_id;
        msg_target_point.class_id = target_info.class_id;
        msg_target_point.box.x = target_info.box.x;
        msg_target_point.box.y = target_info.box.y;
        msg_target_point.box.width = target_info.box.width;
        msg_target_point.box.height = target_info.box.height;
    }
    m_pub_target_point->publish(msg_target_point);
    
    if (m_debug_mode && m_pub_human_image) {
        // publish detected objects
        DetectedObjectsMsg msg_objects;
        msg_objects.header = msg->header;
        msg_objects.image_width = image.cols;
        msg_objects.image_height = image.rows;

        for (const auto& human : human_objects) {
            DetectedObjectMsg msg_object;

            msg_object.id = human.class_id;
            msg_object.prob = human.prob;
            msg_object.box.x = human.box.x;
            msg_object.box.y = human.box.y;
            msg_object.box.width = human.box.width;
            msg_object.box.height = human.box.height;

            msg_objects.objects.emplace_back(msg_object);
        }
        
        m_pub_human_objects->publish(msg_objects);

        drawDetectionsAndPredictions(image, human_objects, track_ids, track_list);

        if (!human_objects.empty() && m_save_image) {
            std::string dir(m_save_dir + "/image_" + std::to_string(m_save_count) + ".png");
            cv::imwrite(dir, image);
            m_save_count++;
        }

        m_pub_human_image.publish(*image_ptr->toImageMsg());
    }
}

bool HumanDetectorComponent::isPredictedOnlyThisFrame(Track& tr) 
{
    auto& b = tr.GetLatestData().bbox;
    return b.score == 0.0f;
} 

/* (Optional) dash line for prediction bbox */
void HumanDetectorComponent::drawDashRect(cv::Mat& image, const cv::Rect& r, const cv::Scalar& col, int thickness, int dash) 
{
    auto seg = [&](cv::Point p0, cv::Point p1){
        const float line_length = cv::norm(p1 - p0);
        const int n = std::max(1, int(line_length/dash));
        for(int i = 0; i < n; i += 2){
            cv::Point start = p0 + (p1 - p0) * (float(i) / n);
            cv::Point end = p0 + (p1 - p0) * (float(std::min(i + 1, n)) / n);
            cv::line(image, start, end, col, thickness);
        }
    };

    cv::Point x1y1(r.x, r.y);
    cv::Point x2y1(r.x + r.width, r.y);
    cv::Point x1y2(r.x, r.y + r.height);
    cv::Point y2y2(r.x + r.width, r.y + r.height);

    seg(x1y1, x2y1);
    seg(x2y1, y2y2);
    seg(y2y2, x1y2);    
    seg(x1y2, x1y1);
}

void HumanDetectorComponent::drawDetectionsAndPredictions(cv::Mat& image, 
                                                        const std::vector<hailo::util::object_t>& human_objects, 
                                                        const std::vector<int>& track_ids, 
                                                        std::vector<Track>& track_list)
{   
    // lambda for IOU calculation
    auto IOU = [](const cv::Rect& r1, const cv::Rect& r2){ 
        const int x1 = std::max(r1.x, r2.x);
        const int y1 = std::max(r1.y, r2.y);
        const int x2 = std::min(r1.x + r1.width, r2.x + r2.width);
        const int y2 = std::min(r1.y + r1.height, r2.y + r2.height);

        const int w = std::max(0, x2 - x1);
        const int h = std::max(0, y2 - y1);

        const int intersection = float(w * h);
        const float union_area = float(r1.area() + r2.area()) - intersection;

        return (union_area > 0.f) ? (intersection / union_area) : 0.f; // return float type
    };
    
    // track IDs that have been matched with detections
    std::unordered_set<int> matched_ids;
    matched_ids.reserve(track_ids.size());
    // store detected bounding boxes for IOU checking
    std::vector<cv::Rect> det_bboxes;
    det_bboxes.reserve(human_objects.size());

    // [1] currently, draw detected boxes
    for(size_t i = 0; i < human_objects.size(); i++) {
        const auto& human = human_objects[i];
        cv::Rect bbox(human.box.x, human.box.y, human.box.width, human.box.height);
        det_bboxes.emplace_back(bbox);

        int id = (i < track_ids.size() ? track_ids[i] : -1); // get track ID if assigned

        cv::rectangle(image, bbox, cv::Scalar(0, 255, 0), 2); // green for detected
        std::string comment = (id >= 0 ? ("ID " + std::to_string(id) + " DET") : "DET (-)");
        cv::putText(image, comment, {bbox.x, bbox.y - 6}, cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);

        if(id >= 0) matched_ids.insert(id);
    }

    // [2] currently, only draw predicted boxes without detection
    // reduce redundant drawing by checking IOU with detected boxes
    const float iou_merge_thresh = 0.4f; // threshold to consider as "already drawn"
    size_t track_cnt = 0;
    for(auto& tr : track_list){
        int id = tr.GetId();

        if(matched_ids.count(id)) continue; // already drawn as detected -> skip

        if(!isPredictedOnlyThisFrame(tr)) continue; // not only predicted -> skip

        const auto& pred_b = tr.GetLatestData().bbox;
        if(pred_b.w <= 0 || pred_b.h <= 0) continue; // invalid bbox -> skip
        cv::Rect pred_bbox(pred_b.x, pred_b.y, pred_b.w, pred_b.h);

        // check IOU with detected boxes
        bool is_merged = false;
        for(const auto& det_box : det_bboxes){
            if(IOU(pred_bbox, det_box) >= iou_merge_thresh) {is_merged = true; break;}
        }
        if(is_merged) continue; // consider as already drawn -> skip


        // draw dash rectangle for predicted only
        drawDashRect(image, pred_bbox, cv::Scalar(255, 255, 255), 2, 10); // white for predicted only
        cv::putText(image, "ID " + std::to_string(id) + " PRED", {pred_bbox.x, pred_bbox.y - 6}, cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 2);
        track_cnt++;
    }

    // [3] (optional) count info
    cv::putText(image, "DET CNT " + std::to_string(human_objects.size()), cv::Point(10, 30),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 0, 0), 2);
    cv::putText(image, "PRED CNT " + std::to_string(track_cnt), cv::Point(10, 60),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 0, 0), 2);

}

} // namespace human_detector

#include <rclcpp_components/register_node_macro.hpp>
RCLCPP_COMPONENTS_REGISTER_NODE(human_detector::HumanDetectorComponent)