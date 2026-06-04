// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SkyHunter v1.5.3 — DCN-2026-014 Item 8.
//
// C++ replacement for the v2 spec's static_ip_mesh0.sh — DCN-2026-014
// post-review elevated the SkyHunter "C++ for all code" policy and
// asked for this kind of pre-systemd setup to also live in C++.
//
// Job (systemd ExecStartPre, single-shot, runs as root):
//   1. Resolve own static IP from /etc/skyautonet/{robot_id,sbc_id}
//      via the deterministic swarm map (see lookupIp in the header).
//   2. ip addr flush + add + link up on mesh0.
//
// Not done here: FastDDS profile generation. interfaceWhiteList in
// san_bringup/config/fastrtps_profile.xml was dropped (post-review
// HIGH #5: FastDDS schema accepts IP literals only — "mesh0" silently
// falls back to all interfaces). Subnet routing (192.168.50.0/24 only
// exists on mesh0) does the isolation.
//
// External processes invoked: /sbin/ip via fork+execvp (no shell;
// argv is hardcoded + the only data input is a validated IP literal
// from the lookup table, so injection surface is zero).

#include "san_bringup/network_bringup.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

extern "C" {
#include <sys/wait.h>
#include <unistd.h>
}

namespace
{

constexpr const char * kRobotIdFile = "/etc/skyautonet/robot_id";
constexpr const char * kSbcIdFile = "/etc/skyautonet/sbc_id";
constexpr const char * kIface = "mesh0";
constexpr int kPrefixLen = 24;

std::string readFileTrimmed(const std::string & path)
{
  std::ifstream f(path);
  if (!f) {return "";}
  std::string line;
  std::getline(f, line);
  return san_bringup::network::trim(line);
}

// Spawn an external program. Returns the child's exit code, or -1 on
// fork/wait failure. Uses fork+execvp — no shell, no quoting risk.
int runExec(const std::vector<const char *> & argv)
{
  pid_t pid = fork();
  if (pid < 0) {
    std::perror("fork");
    return -1;
  }
  if (pid == 0) {
    execvp(argv[0], const_cast<char * const *>(argv.data()));
    std::perror("execvp");
    _exit(127);
  }
  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    std::perror("waitpid");
    return -1;
  }
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

}  // namespace

int main(int argc, char ** argv)
{
  using san_bringup::network::lookupIp;
  using san_bringup::network::parseInt;

  std::string robot_id_s;
  std::string sbc_id_s;

  // Dev / unit-test override: argv[1] = robot_id, argv[2] = sbc_id.
  // Useful for paired bench tests without touching /etc/.
  if (argc >= 3) {
    robot_id_s = argv[1];
    sbc_id_s = argv[2];
  } else {
    robot_id_s = readFileTrimmed(kRobotIdFile);
    sbc_id_s = readFileTrimmed(kSbcIdFile);
    if (sbc_id_s.empty()) {sbc_id_s = "0";}
    if (robot_id_s.empty()) {
      std::cerr << "ERROR: " << kRobotIdFile << " missing or empty\n";
      return 1;
    }
  }

  const auto robot_id = parseInt(robot_id_s);
  const auto sbc_id = parseInt(sbc_id_s);
  if (!robot_id || !sbc_id) {
    std::cerr << "ERROR: robot_id/sbc_id not parseable as int "
              << "(got '" << robot_id_s << "', '" << sbc_id_s << "')\n";
    return 2;
  }

  const char * ip = lookupIp(*robot_id, *sbc_id);
  if (!ip) {
    std::cerr << "ERROR: unknown robot_id=" << *robot_id
              << " sbc_id=" << *sbc_id << " combo — "
              << "check /etc/skyautonet/{robot_id,sbc_id} provisioning\n";
    return 3;
  }

  // Fail loud and early if the iface isn't there — clearer than
  // letting `ip addr add` produce its own kernel-text error.
  if (runExec({"ip", "link", "show", kIface, nullptr}) != 0) {
    std::cerr << "ERROR: interface " << kIface
              << " not present — EasyMesh driver loaded?\n";
    return 4;
  }

  const std::string cidr =
    std::string(ip) + "/" + std::to_string(kPrefixLen);

  if (runExec({"ip", "addr", "flush", "dev", kIface, nullptr}) != 0) {
    std::cerr << "ERROR: failed to flush " << kIface << "\n";
    return 5;
  }
  if (runExec(
      {"ip", "addr", "add", cidr.c_str(), "dev", kIface,
        nullptr}) != 0)
  {
    std::cerr << "ERROR: failed to add " << cidr << " to " << kIface << "\n";
    return 6;
  }
  if (runExec({"ip", "link", "set", kIface, "up", nullptr}) != 0) {
    std::cerr << "ERROR: failed to bring up " << kIface << "\n";
    return 7;
  }

  std::cout << kIface << " configured: " << cidr
            << " (robot_id=" << *robot_id
            << ", sbc_id=" << *sbc_id << ")\n";
  return 0;
}
