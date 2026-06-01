// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 2 v2 - libuci wrapper unit test.
//
// Verifies that setOption/createSection/deleteSection/commit actually
// write to a UCI confdir on disk. The test redirects libuci to a
// temporary directory via UCI_CONFDIR before constructing the
// controller, so the host machine's /etc/config is never touched.

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "san_lte_redundancy/mwan3_uci_controller.hpp"

namespace fs = std::filesystem;

namespace
{

fs::path tempConfDir()
{
  fs::path d = fs::temp_directory_path() / "san_uci_test";
  return d;
}

std::string readFile(const fs::path & p)
{
  std::ifstream f(p);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

}  // namespace

class UciControllerTest : public ::testing::Test
{
protected:
  fs::path conf_dir_;
  fs::path save_dir_;

  void SetUp() override
  {
    conf_dir_ = tempConfDir();
    save_dir_ = conf_dir_.parent_path() / "san_uci_test_save";
    fs::remove_all(conf_dir_);
    fs::remove_all(save_dir_);
    fs::create_directories(conf_dir_);
    fs::create_directories(save_dir_);

    // Seed an empty mwan3 package so uci_lookup_ptr can autocreate
    // sections inside it.
    std::ofstream(conf_dir_ / "mwan3").close();

#ifdef _WIN32
    _putenv_s("UCI_CONFDIR", conf_dir_.string().c_str());
    _putenv_s("UCI_SAVEDIR", save_dir_.string().c_str());
#else
    setenv("UCI_CONFDIR", conf_dir_.string().c_str(), 1);
    setenv("UCI_SAVEDIR", save_dir_.string().c_str(), 1);
#endif
  }

  void TearDown() override
  {
    fs::remove_all(conf_dir_);
    fs::remove_all(save_dir_);
  }
};

TEST_F(UciControllerTest, CreateSectionAndSetWeight) {
  auto logger = rclcpp::get_logger("test_uci");
  san_lte_redundancy::Mwan3UciController uci(logger);

  EXPECT_TRUE(uci.createSection("mwan3.wan_lte", "interface"));
  EXPECT_TRUE(uci.setLteWeight(50));

  const std::string content = readFile(conf_dir_ / "mwan3");
  EXPECT_NE(content.find("wan_lte"), std::string::npos);
  EXPECT_NE(content.find("'50'"), std::string::npos);
}

TEST_F(UciControllerTest, SetOptionPersistsAfterCommit) {
  auto logger = rclcpp::get_logger("test_uci");
  {
    san_lte_redundancy::Mwan3UciController uci(logger);
    EXPECT_TRUE(uci.createSection("mwan3.wan_lte", "interface"));
    EXPECT_TRUE(uci.setOption("mwan3.wan_lte.enabled", "1"));
    EXPECT_TRUE(uci.setOption("mwan3.wan_lte.track_method", "ping"));
    EXPECT_TRUE(uci.commit("mwan3"));
  }
  const std::string content = readFile(conf_dir_ / "mwan3");
  EXPECT_NE(content.find("option enabled '1'"), std::string::npos);
  EXPECT_NE(content.find("option track_method 'ping'"), std::string::npos);
}

TEST_F(UciControllerTest, DeleteSectionIsIdempotent) {
  auto logger = rclcpp::get_logger("test_uci");
  san_lte_redundancy::Mwan3UciController uci(logger);

  // Deleting a non-existent section should still report success
  // (controller treats it as a no-op so init scripts can run
  // delete-then-create unconditionally).
  EXPECT_TRUE(uci.deleteSection("mwan3.wan_lte"));
  EXPECT_TRUE(uci.createSection("mwan3.wan_lte", "interface"));
  EXPECT_TRUE(uci.commit("mwan3"));
  EXPECT_TRUE(uci.deleteSection("mwan3.wan_lte"));
  EXPECT_TRUE(uci.commit("mwan3"));
  const std::string content = readFile(conf_dir_ / "mwan3");
  EXPECT_EQ(content.find("config interface 'wan_lte'"), std::string::npos);
}

TEST_F(UciControllerTest, SetLteWeightWritesNumericValue) {
  auto logger = rclcpp::get_logger("test_uci");
  san_lte_redundancy::Mwan3UciController uci(logger);
  EXPECT_TRUE(uci.createSection("mwan3.wan_lte", "interface"));
  EXPECT_TRUE(uci.setLteWeight(100));

  const std::string content = readFile(conf_dir_ / "mwan3");
  EXPECT_NE(content.find("option weight '100'"), std::string::npos);
  // No shell metacharacters smuggled through.
  EXPECT_EQ(content.find('&'), std::string::npos);
  EXPECT_EQ(content.find('|'), std::string::npos);
  EXPECT_EQ(content.find(';'), std::string::npos);
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  rclcpp::init(argc, argv);
  int rc = RUN_ALL_TESTS();
  rclcpp::shutdown();
  return rc;
}
