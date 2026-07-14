#include "san_lte_redundancy/lte_link_quality_node.hpp"

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace san_lte_redundancy {

using Msg = combat_robot_msgs::msg::LteLinkQuality;
using namespace std::chrono_literals;

namespace {

uint64_t nowMs(rclcpp::Clock& clk) {
    return static_cast<uint64_t>(clk.now().nanoseconds() / 1'000'000ll);
}

bool parseInt(const std::string& s, long* out) {
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    long v = std::strtol(s.c_str(), &end, 10);
    if (errno != 0 || end == s.c_str()) return false;
    while (end && *end != '\0' && std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (end && *end != '\0') return false;
    *out = v;
    return true;
}

std::string trim(const std::string& s) {
    auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

}  // namespace

LteLinkQualityNode::LteLinkQualityNode()
    : LteLinkQualityNode(rclcpp::NodeOptions())
{}

LteLinkQualityNode::LteLinkQualityNode(const rclcpp::NodeOptions& options)
    : rclcpp::Node("lte_link_quality_node", options)
{
    declareParameters();
    readParameters();
    wireInterfaces();
}

void LteLinkQualityNode::declareParameters() {
    declare_parameter<std::string>(
        "status_file_path", "/var/run/san/lte_link_quality");
    declare_parameter<std::string>("source_iface", "lte0");
    declare_parameter<int>("publish_period_ms", 1000);
}

void LteLinkQualityNode::readParameters() {
    status_file_path_ = get_parameter("status_file_path").as_string();
    source_iface_ = get_parameter("source_iface").as_string();
    publish_period_ms_ = get_parameter("publish_period_ms").as_int();
    if (publish_period_ms_ < 100) publish_period_ms_ = 100;
}

void LteLinkQualityNode::wireInterfaces() {
    rclcpp::QoS qos(10);
    qos.reliable();
    pub_ = create_publisher<Msg>("/lte/link_quality", qos);

    timer_ = create_wall_timer(
        std::chrono::milliseconds(publish_period_ms_),
        std::bind(&LteLinkQualityNode::onTimer, this));
}

void LteLinkQualityNode::onTimer() {
    LteSignalRaw raw;
    {
        std::lock_guard<std::mutex> lock(inject_mu_);
        if (has_injected_) {
            raw = injected_;
        } else {
            raw = readStatusFile();
        }
    }
    auto msg = buildMessage(raw);
    pub_->publish(msg);
}

LteSignalRaw LteLinkQualityNode::readStatusFile() const {
    LteSignalRaw out;
    std::ifstream f(status_file_path_);
    if (!f.is_open()) return out;
    std::ostringstream buf;
    buf << f.rdbuf();
    return parseStatusBlob(buf.str());
}

LteSignalRaw LteLinkQualityNode::parseStatusBlob(const std::string& blob) {
    LteSignalRaw out;
    bool got_rsrp = false;
    bool got_rsrq = false;
    bool got_sinr = false;
    std::istringstream iss(blob);
    std::string line;
    while (std::getline(iss, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = trim(line.substr(0, eq));
        const std::string val = trim(line.substr(eq + 1));
        long v = 0;
        if (!parseInt(val, &v)) continue;
        if (key == "rsrp_dbm") {
            if (v < -140) v = -140;
            if (v >    0) v =    0;
            out.rsrp_dbm = static_cast<int16_t>(v);
            got_rsrp = true;
        } else if (key == "rsrq_db") {
            if (v < -20) v = -20;
            if (v >   0) v =   0;
            out.rsrq_db = static_cast<int8_t>(v);
            got_rsrq = true;
        } else if (key == "sinr_db") {
            if (v < -30) v = -30;
            if (v >  40) v =  40;
            out.sinr_db = static_cast<int8_t>(v);
            got_sinr = true;
        }
    }
    out.valid = got_rsrp;        // RSRP alone is enough to grade.
    (void)got_rsrq;
    (void)got_sinr;
    return out;
}

combat_robot_msgs::msg::LteLinkQuality
LteLinkQualityNode::buildMessage(const LteSignalRaw& raw) {
    Msg m;
    m.header.stamp = now();
    m.header.frame_id = source_iface_;
    m.source_iface = source_iface_;
    if (!raw.valid) {
        m.rsrp_dbm = 0;
        m.rsrq_db  = 0;
        m.sinr_db  = 0;
        m.grade    = Msg::LTE_GRADE_UNKNOWN;
    } else {
        m.rsrp_dbm = raw.rsrp_dbm;
        m.rsrq_db  = raw.rsrq_db;
        m.sinr_db  = raw.sinr_db;
        // [DCN-2026-006 EXT D-020] Hysteresis grader — see header
        // comment. Suppresses upward-chatter at -100/-110 dBm cliffs.
        m.grade    = stateful_grader_.grade(raw.rsrp_dbm);
    }
    m.timestamp_ms = nowMs(*get_clock());
    return m;
}

void LteLinkQualityNode::injectForTest(const LteSignalRaw& s) {
    std::lock_guard<std::mutex> lock(inject_mu_);
    injected_ = s;
    has_injected_ = true;
}

void LteLinkQualityNode::clearInjected() {
    std::lock_guard<std::mutex> lock(inject_mu_);
    has_injected_ = false;
}

combat_robot_msgs::msg::LteLinkQuality
LteLinkQualityNode::tickForTest() {
    LteSignalRaw raw;
    {
        std::lock_guard<std::mutex> lock(inject_mu_);
        if (has_injected_) raw = injected_;
        else               raw = readStatusFile();
    }
    auto msg = buildMessage(raw);
    if (pub_) pub_->publish(msg);
    return msg;
}

}  // namespace san_lte_redundancy
