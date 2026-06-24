// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "san_lte_redundancy/mwan3_uci_controller.hpp"

#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace san_lte_redundancy
{

namespace
{

// libuci mutates the buffer passed to uci_lookup_ptr (it inserts '\0'
// separators between package/section/option). We copy into a local
// buffer per call so callers can pass `std::string` arguments without
// surprise.
constexpr std::size_t kUciBufferSize = 256;

bool copyToBuffer(
  const std::string & src,
  char (& dst)[kUciBufferSize],
  rclcpp::Logger & logger,
  const char * context)
{
  if (src.size() >= kUciBufferSize) {
    RCLCPP_ERROR(
      logger, "UCI %s exceeds %zu bytes: %s",
      context, kUciBufferSize, src.c_str());
    return false;
  }
  std::memset(dst, 0, kUciBufferSize);
  std::memcpy(dst, src.data(), src.size());
  return true;
}

}  // namespace

Mwan3UciController::Mwan3UciController(rclcpp::Logger logger)
: logger_(logger), uci_ctx_(uci_alloc_context())
{
  if (uci_ctx_ == nullptr) {
    throw std::runtime_error("uci_alloc_context returned NULL");
  }
  // Persistent dirs (overridable via env vars during tests).
  if (const char * confdir = std::getenv("UCI_CONFDIR")) {
    uci_set_confdir(uci_ctx_, confdir);
  }
  if (const char * savedir = std::getenv("UCI_SAVEDIR")) {
    uci_set_savedir(uci_ctx_, savedir);
  }
}

Mwan3UciController::~Mwan3UciController()
{
  if (uci_ctx_ != nullptr) {
    uci_free_context(uci_ctx_);
    uci_ctx_ = nullptr;
  }
}

void Mwan3UciController::setConfDir(const std::string & confdir)
{
  std::lock_guard<std::mutex> lock(uci_mutex_);
  uci_set_confdir(uci_ctx_, confdir.c_str());
}

void Mwan3UciController::setSaveDir(const std::string & savedir)
{
  std::lock_guard<std::mutex> lock(uci_mutex_);
  uci_set_savedir(uci_ctx_, savedir.c_str());
}

bool Mwan3UciController::setLteWeight(int weight)
{
  if (!setOption("mwan3.wan_lte.weight", std::to_string(weight))) {
    return false;
  }
  return commit("mwan3");
}

bool Mwan3UciController::setOption(
  const std::string & key,
  const std::string & value)
{
  std::lock_guard<std::mutex> lock(uci_mutex_);

  char buf[kUciBufferSize];
  const std::string assignment = key + "=" + value;
  if (!copyToBuffer(assignment, buf, logger_, "set")) {
    return false;
  }

  struct uci_ptr ptr;
  if (uci_lookup_ptr(uci_ctx_, &ptr, buf, true) != UCI_OK) {
    char * err = nullptr;
    uci_get_errorstr(uci_ctx_, &err, "uci_lookup_ptr");
    RCLCPP_ERROR(
      logger_, "uci_lookup_ptr(%s) failed: %s",
      assignment.c_str(), err ? err : "(no detail)");
    std::free(err);
    return false;
  }

  if (uci_set(uci_ctx_, &ptr) != UCI_OK) {
    char * err = nullptr;
    uci_get_errorstr(uci_ctx_, &err, "uci_set");
    RCLCPP_ERROR(
      logger_, "uci_set(%s) failed: %s",
      assignment.c_str(), err ? err : "(no detail)");
    std::free(err);
    return false;
  }
  return true;
}

bool Mwan3UciController::createSection(
  const std::string & section_path,
  const std::string & type)
{
  // libuci treats `pkg.section=type` as a section-create assignment
  // when looked up with autocreate=true. Route through setOption.
  return setOption(section_path, type);
}

bool Mwan3UciController::deleteSection(const std::string & section_path)
{
  std::lock_guard<std::mutex> lock(uci_mutex_);

  char buf[kUciBufferSize];
  if (!copyToBuffer(section_path, buf, logger_, "delete")) {
    return false;
  }

  struct uci_ptr ptr;
  if (uci_lookup_ptr(uci_ctx_, &ptr, buf, true) != UCI_OK) {
    // Missing section is not an error for delete — caller can
    // unconditionally delete-then-recreate during init.
    return true;
  }
  if ((ptr.flags & UCI_LOOKUP_COMPLETE) == 0) {
    return true;
  }

  if (uci_delete(uci_ctx_, &ptr) != UCI_OK) {
    char * err = nullptr;
    uci_get_errorstr(uci_ctx_, &err, "uci_delete");
    RCLCPP_ERROR(
      logger_, "uci_delete(%s) failed: %s",
      section_path.c_str(), err ? err : "(no detail)");
    std::free(err);
    return false;
  }
  return true;
}

bool Mwan3UciController::commit(const std::string & package)
{
  std::lock_guard<std::mutex> lock(uci_mutex_);

  char buf[kUciBufferSize];
  if (!copyToBuffer(package, buf, logger_, "commit")) {
    return false;
  }

  struct uci_ptr ptr;
  if (uci_lookup_ptr(uci_ctx_, &ptr, buf, true) != UCI_OK) {
    char * err = nullptr;
    uci_get_errorstr(uci_ctx_, &err, "uci_lookup_ptr(commit)");
    RCLCPP_ERROR(
      logger_, "uci_lookup_ptr commit(%s) failed: %s",
      package.c_str(), err ? err : "(no detail)");
    std::free(err);
    return false;
  }
  if (uci_commit(uci_ctx_, &ptr.p, false) != UCI_OK) {
    char * err = nullptr;
    uci_get_errorstr(uci_ctx_, &err, "uci_commit");
    RCLCPP_ERROR(
      logger_, "uci_commit(%s) failed: %s",
      package.c_str(), err ? err : "(no detail)");
    std::free(err);
    return false;
  }
  return true;
}

}  // namespace san_lte_redundancy
