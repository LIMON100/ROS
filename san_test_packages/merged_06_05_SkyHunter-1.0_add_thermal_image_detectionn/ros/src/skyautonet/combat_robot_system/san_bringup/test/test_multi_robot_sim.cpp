// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SkyHunter v1.5.3 — DCN-2026-012 v2 (Multi-Robot Sim Hardening).
//
// 6-case gtest for the sim-hardening structure wired into
// squadron.launch.xml + the 5 per-robot wrapper XMLs. Follows the
// same pattern as test_fastrtps_profile_selection.cpp: pure-XML
// structural verification via TinyXML-2 over the installed share
// dir, so CI runs in milliseconds and doesn't need a live ROS graph.
//
// Coverage
// --------
//   T1 (D-044) static_transform_publisher map → robot_namespace/odom,
//              gated by use_sim_time.
//   T2 (D-041) rosbridge_port let = 9090 + int(robot_id) and the
//              rosbridge_websocket node uses $(var rosbridge_port).
//   T3 (D-042) lifecycle_manager_navigation declared inside the robot
//              namespace, node_names contain bare names (no namespace
//              prefix), sim branch sets bond_timeout=0.0.
//   T4 (D-043) Sim lifecycle_manager wrapped in a <timer> that uses
//              sim_startup_delay (the per-robot stagger).
//   T5         All 5 wrapper XMLs declare + pass sim_startup_offset_sec
//              through to squadron.launch.xml.
//   T6         Real-hardware branch (use_sim_time=false) keeps the
//              lifecycle_manager but omits the <timer> wrapper and
//              the static_transform_publisher (no SLAM clash).

#include <gtest/gtest.h>

#include <ament_index_cpp/get_package_share_directory.hpp>
#include <tinyxml2.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{

std::string sanBringupShareDir()
{
  return ament_index_cpp::get_package_share_directory("san_bringup");
}

std::string readFile(const fs::path & p)
{
  std::ifstream in(p);
  if (!in) {return {};}
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::string squadronLaunchText()
{
  return readFile(
    fs::path(sanBringupShareDir()) / "launch" /
    "squadron.launch.xml");
}

// Recursively collect every element matching the predicate.
void collectAll(
  const tinyxml2::XMLElement * root,
  std::vector<const tinyxml2::XMLElement *> & out,
  const std::function<bool(const tinyxml2::XMLElement *)> & pred)
{
  if (!root) {return;}
  if (pred(root)) {out.push_back(root);}
  for (auto * c = root->FirstChildElement(); c; c = c->NextSiblingElement()) {
    collectAll(c, out, pred);
  }
}

// All <node> elements with a given pkg/exec pair.
std::vector<const tinyxml2::XMLElement *> findNodes(
  const tinyxml2::XMLElement * root,
  const std::string & pkg,
  const std::string & exec)
{
  std::vector<const tinyxml2::XMLElement *> out;
  collectAll(
    root, out, [&](const tinyxml2::XMLElement * e) {
      if (std::strcmp(e->Name(), "node") != 0) {return false;}
      const char * p = e->Attribute("pkg");
      const char * x = e->Attribute("exec");
      return p && x && pkg == p && exec == x;
    });
  return out;
}

// Find the nearest ancestor of `target` whose tag == `ancestor_name`.
// Returns nullptr if not found within `root`'s subtree.
const tinyxml2::XMLElement * findAncestor(
  const tinyxml2::XMLElement * root,
  const tinyxml2::XMLElement * target,
  const char * ancestor_name)
{
  if (!root || !target) {return nullptr;}
  // BFS-style: at each element, check if `target` is in subtree AND
  // this element matches. Track candidates as we recurse.
  if (std::strcmp(root->Name(), ancestor_name) == 0) {
    // Is target a descendant of root?
    std::vector<const tinyxml2::XMLElement *> all;
    collectAll(
      root, all, [&](const tinyxml2::XMLElement * e) {
        return e == target;
      });
    if (!all.empty() && root != target) {
      // Look deeper for a closer match first.
      for (auto * c = root->FirstChildElement(); c;
        c = c->NextSiblingElement())
      {
        if (auto * closer = findAncestor(c, target, ancestor_name)) {
          return closer;
        }
      }
      return root;
    }
  }
  for (auto * c = root->FirstChildElement(); c; c = c->NextSiblingElement()) {
    if (auto * hit = findAncestor(c, target, ancestor_name)) {return hit;}
  }
  return nullptr;
}

class MultiRobotSimTest : public ::testing::Test
{
protected:
  tinyxml2::XMLDocument doc_;
  const tinyxml2::XMLElement * root_ = nullptr;

  void SetUp() override
  {
    const auto text = squadronLaunchText();
    ASSERT_FALSE(text.empty()) << "squadron.launch.xml not installed";
    ASSERT_EQ(doc_.Parse(text.c_str()), tinyxml2::XML_SUCCESS);
    root_ = doc_.RootElement();
    ASSERT_NE(root_, nullptr);
    ASSERT_STREQ(root_->Name(), "launch");
  }
};

}  // namespace

// ------------------------------------------------------------------- T1
// D-044: static_transform_publisher map → robot_namespace/odom (sim only)
TEST_F(MultiRobotSimTest, T1_StaticTransformPublisherMapToOdom) {
  auto stp_nodes = findNodes(root_, "tf2_ros", "static_transform_publisher");
  ASSERT_FALSE(stp_nodes.empty())
    << "no static_transform_publisher node found in squadron.launch.xml";

  bool found_map_to_odom = false;
  for (const auto * n : stp_nodes) {
    const char * args = n->Attribute("args");
    if (!args) {continue;}
    const std::string a(args);
    if (a.find("map") != std::string::npos &&
      a.find("$(var robot_namespace)/odom") != std::string::npos)
    {
      found_map_to_odom = true;
      // D-044: must be sim-only. Walk ancestors until we hit a <group>
      // with if="$(var use_sim_time)".
      const auto * g = findAncestor(root_, n, "group");
      ASSERT_NE(g, nullptr) << "static_transform_publisher must live "
        "under a <group> gate";
      const char * if_attr = g->Attribute("if");
      ASSERT_NE(if_attr, nullptr)
        << "enclosing <group> must carry an if=... gate";
      EXPECT_NE(std::string(if_attr).find("use_sim_time"), std::string::npos)
        << "static_transform_publisher must be gated by use_sim_time "
        "(else it would race SLAM on real hardware)";
    }
  }
  EXPECT_TRUE(found_map_to_odom)
    << "static_transform_publisher with map → robot_namespace/odom missing";
}

// ------------------------------------------------------------------- T2
// D-041: rosbridge_port = 9090 + robot_id, applied to rosbridge_websocket
TEST_F(MultiRobotSimTest, T2_RosbridgePortIsRobotIdOffset) {
  // 2a: <let name="rosbridge_port" value="$(eval ...9090 + int...)" />
  std::vector<const tinyxml2::XMLElement *> lets;
  collectAll(
    root_, lets, [](const tinyxml2::XMLElement * e) {
      return std::strcmp(e->Name(), "let") == 0;
    });
  bool found_let = false;
  for (const auto * l : lets) {
    const char * name = l->Attribute("name");
    const char * value = l->Attribute("value");
    if (!name || !value) {continue;}
    if (std::string(name) == "rosbridge_port") {
      const std::string v(value);
      EXPECT_NE(v.find("9090"), std::string::npos)
        << "rosbridge_port base must be 9090";
      EXPECT_NE(v.find("$(var robot_id)"), std::string::npos)
        << "rosbridge_port must offset by robot_id";
      EXPECT_NE(v.find("$(eval"), std::string::npos)
        << "rosbridge_port must use $(eval) arithmetic";
      found_let = true;
    }
  }
  ASSERT_TRUE(found_let) << "rosbridge_port <let> missing";

  // 2b: rosbridge_websocket node uses $(var rosbridge_port) for port
  auto rb_nodes = findNodes(root_, "rosbridge_server", "rosbridge_websocket");
  ASSERT_EQ(rb_nodes.size(), 1u)
    << "exactly one rosbridge_websocket node expected";
  bool port_uses_let = false;
  for (auto * p = rb_nodes[0]->FirstChildElement("param"); p;
    p = p->NextSiblingElement("param"))
  {
    const char * n = p->Attribute("name");
    const char * v = p->Attribute("value");
    if (n && v && std::string(n) == "port") {
      EXPECT_EQ(std::string(v), std::string("$(var rosbridge_port)"))
        << "rosbridge port must reference $(var rosbridge_port), not "
        "a hardcoded value";
      port_uses_let = true;
    }
  }
  EXPECT_TRUE(port_uses_let) << "rosbridge_websocket missing port param";

  // 2c: rosbridge node is gated by include_rosbridge
  const char * if_attr = rb_nodes[0]->Attribute("if");
  ASSERT_NE(if_attr, nullptr) << "rosbridge_websocket missing if=... gate";
  EXPECT_NE(std::string(if_attr).find("include_rosbridge"), std::string::npos);
}

// ------------------------------------------------------------------- T3
// D-042: lifecycle_manager declared, node_names bare, sim bond_timeout=0
TEST_F(MultiRobotSimTest, T3_LifecycleManagerNodeNamesAndBondTimeout) {
  auto lm_nodes = findNodes(
    root_, "nav2_lifecycle_manager",
    "lifecycle_manager");
  ASSERT_EQ(lm_nodes.size(), 2u)
    << "expected 2 lifecycle_manager nodes (sim + hw branches)";

  bool found_sim = false, found_hw = false;
  for (const auto * n : lm_nodes) {
    std::string node_names, bond_timeout;
    bool use_sim_time_param = false;
    for (auto * p = n->FirstChildElement("param"); p;
      p = p->NextSiblingElement("param"))
    {
      const char * nm = p->Attribute("name");
      const char * vl = p->Attribute("value");
      if (!nm || !vl) {continue;}
      if (std::string(nm) == "node_names") {node_names = vl;}
      if (std::string(nm) == "bond_timeout") {bond_timeout = vl;}
      if (std::string(nm) == "use_sim_time" &&
        std::string(vl) == "true") {use_sim_time_param = true;}
    }

    ASSERT_FALSE(node_names.empty())
      << "lifecycle_manager missing node_names param";
    // node_names must be bare — no robot_ prefix (which would happen
    // if someone accidentally namespaced them).
    EXPECT_EQ(node_names.find("robot_"), std::string::npos)
      << "node_names contains 'robot_' prefix — lifecycle_manager is "
      "already inside the namespace push, names must be bare";
    // Required Nav2 nodes
    for (const auto * required : {"controller_server", "planner_server",
        "behavior_server", "bt_navigator",
        "waypoint_follower", "velocity_smoother"})
    {
      EXPECT_NE(node_names.find(required), std::string::npos)
        << "lifecycle_manager node_names must include " << required;
    }

    if (use_sim_time_param) {
      found_sim = true;
      EXPECT_EQ(bond_timeout, "0.0")
        << "sim branch must set bond_timeout=0.0 to prevent localhost "
        "lifecycle hangs (Limon 2026-05-14 report)";
    } else {
      found_hw = true;
      // Real-hw branch: bond_timeout may be unset (default) OR > 0
      if (!bond_timeout.empty()) {
        EXPECT_NE(bond_timeout, "0.0")
          << "real-hw branch must NOT zero out bond_timeout";
      }
    }
  }
  EXPECT_TRUE(found_sim) << "no sim-branch lifecycle_manager found";
  EXPECT_TRUE(found_hw) << "no hw-branch lifecycle_manager found";
}

// ------------------------------------------------------------------- T4
// D-043: TimerAction wraps sim lifecycle_manager with sim_startup_delay
TEST_F(MultiRobotSimTest, T4_TimerWrapsSimLifecycleManager) {
  // sim_startup_delay <let> must exist with $(eval) referencing robot_id
  std::vector<const tinyxml2::XMLElement *> lets;
  collectAll(
    root_, lets, [](const tinyxml2::XMLElement * e) {
      return std::strcmp(e->Name(), "let") == 0;
    });
  bool found_delay = false;
  for (const auto * l : lets) {
    const char * name = l->Attribute("name");
    const char * value = l->Attribute("value");
    if (!name || !value) {continue;}
    if (std::string(name) == "sim_startup_delay") {
      const std::string v(value);
      EXPECT_NE(v.find("sim_startup_offset_sec"), std::string::npos);
      EXPECT_NE(v.find("$(var robot_id)"), std::string::npos);
      EXPECT_NE(v.find("$(eval"), std::string::npos);
      found_delay = true;
    }
  }
  ASSERT_TRUE(found_delay) << "sim_startup_delay <let> missing";

  // The sim lifecycle_manager (the one with use_sim_time=true) must
  // have a <timer period="$(var sim_startup_delay)"> ancestor.
  auto lm_nodes = findNodes(
    root_, "nav2_lifecycle_manager",
    "lifecycle_manager");
  const tinyxml2::XMLElement * sim_lm = nullptr;
  for (const auto * n : lm_nodes) {
    for (auto * p = n->FirstChildElement("param"); p;
      p = p->NextSiblingElement("param"))
    {
      const char * nm = p->Attribute("name");
      const char * vl = p->Attribute("value");
      if (nm && vl && std::string(nm) == "use_sim_time" &&
        std::string(vl) == "true")
      {
        sim_lm = n;
      }
    }
  }
  ASSERT_NE(sim_lm, nullptr) << "no sim-branch lifecycle_manager";

  const auto * timer = findAncestor(root_, sim_lm, "timer");
  ASSERT_NE(timer, nullptr)
    << "sim lifecycle_manager must be wrapped in a <timer> (D-043)";
  const char * period = timer->Attribute("period");
  ASSERT_NE(period, nullptr) << "<timer> missing period=...";
  EXPECT_EQ(std::string(period), std::string("$(var sim_startup_delay)"))
    << "<timer> must use sim_startup_delay";
}

// ------------------------------------------------------------------- T5
// All 5 wrapper XMLs declare + pass sim_startup_offset_sec
TEST_F(MultiRobotSimTest, T5_AllWrappersPassSimStartupOffset) {
  const std::vector<std::string> wrappers = {
    "leader_go2.launch.xml",
    "hub_sbc1.launch.xml",
    "hub_sbc2.launch.xml",
    "deputy.launch.xml",
    "follower.launch.xml",
  };

  for (const auto & w : wrappers) {
    const auto text = readFile(fs::path(sanBringupShareDir()) / "launch" / w);
    ASSERT_FALSE(text.empty()) << w << " not installed";

    tinyxml2::XMLDocument d;
    ASSERT_EQ(d.Parse(text.c_str()), tinyxml2::XML_SUCCESS)
      << w << " is not valid XML";
    auto * r = d.RootElement();
    ASSERT_NE(r, nullptr);

    // Wrapper must declare an <arg name="sim_startup_offset_sec"/>
    bool declared = false;
    for (auto * a = r->FirstChildElement("arg"); a;
      a = a->NextSiblingElement("arg"))
    {
      const char * n = a->Attribute("name");
      if (n && std::string(n) == "sim_startup_offset_sec") {declared = true;}
    }
    EXPECT_TRUE(declared) << w << " missing arg sim_startup_offset_sec";

    // And the <include> for squadron.launch.xml must pass it through.
    std::vector<const tinyxml2::XMLElement *> includes;
    collectAll(
      r, includes, [](const tinyxml2::XMLElement * e) {
        return std::strcmp(e->Name(), "include") == 0;
      });
    bool passed = false;
    for (const auto * inc : includes) {
      for (auto * a = inc->FirstChildElement("arg"); a;
        a = a->NextSiblingElement("arg"))
      {
        const char * n = a->Attribute("name");
        const char * v = a->Attribute("value");
        if (n && v && std::string(n) == "sim_startup_offset_sec" &&
          std::string(v) == "$(var sim_startup_offset_sec)")
        {
          passed = true;
        }
      }
    }
    EXPECT_TRUE(passed) << w << " does not pass sim_startup_offset_sec to "
      "squadron.launch.xml include";
  }
}

// ------------------------------------------------------------------- T6
// Real-hw branch must NOT wrap lifecycle_manager in <timer> and must NOT
// activate the static_transform_publisher (those are sim-only).
TEST_F(MultiRobotSimTest, T6_RealHardwareModeUnchanged) {
  auto lm_nodes = findNodes(
    root_, "nav2_lifecycle_manager",
    "lifecycle_manager");
  const tinyxml2::XMLElement * hw_lm = nullptr;
  for (const auto * n : lm_nodes) {
    bool has_sim_param = false;
    for (auto * p = n->FirstChildElement("param"); p;
      p = p->NextSiblingElement("param"))
    {
      const char * nm = p->Attribute("name");
      const char * vl = p->Attribute("value");
      if (nm && vl && std::string(nm) == "use_sim_time" &&
        std::string(vl) == "true")
      {
        has_sim_param = true;
      }
    }
    if (!has_sim_param) {hw_lm = n;}
  }
  ASSERT_NE(hw_lm, nullptr) << "no real-hw lifecycle_manager branch";

  // Real-hw lifecycle_manager must NOT be inside a <timer>.
  const auto * timer = findAncestor(root_, hw_lm, "timer");
  EXPECT_EQ(timer, nullptr)
    << "real-hw lifecycle_manager must NOT be timer-wrapped";

  // Its enclosing <group> must carry `unless="$(var use_sim_time)"`.
  const auto * g = findAncestor(root_, hw_lm, "group");
  ASSERT_NE(g, nullptr);
  const char * unless_attr = g->Attribute("unless");
  EXPECT_NE(unless_attr, nullptr)
    << "real-hw lifecycle_manager group must use unless=use_sim_time";
  if (unless_attr) {
    EXPECT_NE(
      std::string(unless_attr).find("use_sim_time"),
      std::string::npos);
  }

  // The static_transform_publisher must NOT have a hw counterpart —
  // verify there is exactly one stp node and it is sim-gated.
  auto stp_nodes = findNodes(root_, "tf2_ros", "static_transform_publisher");
  EXPECT_EQ(stp_nodes.size(), 1u)
    << "real-hw branch must not duplicate static_transform_publisher";
  if (stp_nodes.size() == 1) {
    const auto * sg = findAncestor(root_, stp_nodes[0], "group");
    ASSERT_NE(sg, nullptr);
    const char * if_attr = sg->Attribute("if");
    ASSERT_NE(if_attr, nullptr);
    EXPECT_NE(std::string(if_attr).find("use_sim_time"), std::string::npos);
  }
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
