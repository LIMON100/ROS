#include "san_lte_redundancy/mwan3_init_node.hpp"

namespace san_lte_redundancy {

Mwan3InitNode::Mwan3InitNode()
    : Mwan3InitNode(rclcpp::NodeOptions())
{}

Mwan3InitNode::Mwan3InitNode(const rclcpp::NodeOptions& options)
    : rclcpp::Node("mwan3_init_node", options)
{
    declare_parameter<int>("robot_id", 3);
    declare_parameter<std::string>("role", "backup");
    declare_parameter<std::string>("track_ip", "8.8.8.8");

    readParameters();
    uci_ = std::make_unique<Mwan3UciController>(get_logger());
    configureMwan3();
}

Mwan3InitNode::Mwan3InitNode(const rclcpp::NodeOptions& options,
                             std::unique_ptr<Mwan3UciController> uci)
    : rclcpp::Node("mwan3_init_node", options),
      uci_(std::move(uci))
{
    declare_parameter<int>("robot_id", 3);
    declare_parameter<std::string>("role", "backup");
    declare_parameter<std::string>("track_ip", "8.8.8.8");

    readParameters();
    configureMwan3();
}

void Mwan3InitNode::readParameters() {
    robot_id_ = get_parameter("robot_id").as_int();
    role_     = get_parameter("role").as_string();
    track_ip_ = get_parameter("track_ip").as_string();
    initial_weight_ = (role_ == "primary") ? 100 : 0;
}

void Mwan3InitNode::configureMwan3() {
    if (!uci_) {
        RCLCPP_ERROR(get_logger(), "no UCI controller bound");
        return;
    }

    // Tear down any stale wan_lte section so the layout matches our
    // expectation precisely (option ordering, no leftover options).
    uci_->deleteSection("mwan3.wan_lte");
    uci_->commit("mwan3");

    const bool ok =
        uci_->createSection("mwan3.wan_lte", "interface")
        && uci_->setOption("mwan3.wan_lte.enabled",     "1")
        && uci_->setOption("mwan3.wan_lte.weight",
                            std::to_string(initial_weight_))
        && uci_->setOption("mwan3.wan_lte.family",       "ipv4")
        && uci_->setOption("mwan3.wan_lte.track_method", "ping")
        && uci_->setOption("mwan3.wan_lte.track_ip",     track_ip_)
        && uci_->setOption("mwan3.wan_lte.reliability",  "1")
        && uci_->setOption("mwan3.wan_lte.count",        "1")
        && uci_->setOption("mwan3.wan_lte.timeout",      "2")
        && uci_->setOption("mwan3.wan_lte.interval",     "5")
        && uci_->setOption("mwan3.wan_lte.down",         "3")
        && uci_->setOption("mwan3.wan_lte.up",           "3")
        && uci_->commit("mwan3");

    if (!ok) {
        RCLCPP_ERROR(get_logger(),
            "[SAN-LTE] mwan3 init FAILED for robot %d role=%s",
            robot_id_, role_.c_str());
        return;
    }

    configured_ = true;
    RCLCPP_INFO(get_logger(),
        "[SAN-LTE] Robot %d initialized as %s (initial_weight=%d, track_ip=%s)",
        robot_id_, role_.c_str(), initial_weight_, track_ip_.c_str());
}

}  // namespace san_lte_redundancy
