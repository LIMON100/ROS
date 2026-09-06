// Stub of the HailoRT C++ API surface this repo uses — SYNTAX CHECK ONLY.
//
// The vendor SDKs are not redistributable and are absent from CI and from
// most dev machines, so the RIPOSTE_WITH_* translation units are normally
// never compiled at all. That let syntax and type errors sit in them until
// hardware bring-up. ci/vendor_syntax.sh compiles those TUs against these
// stubs with -fsyntax-only, which catches that class of error immediately.
//
// This declares just enough to parse. It has NO behaviour: nothing here may
// ever be linked or executed, and a passing check says the code compiles,
// not that it works against the real device.
#pragma once
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using hailo_status = int;
constexpr hailo_status HAILO_SUCCESS = 0;
enum hailo_format_order_t {
    HAILO_FORMAT_ORDER_HAILO_NMS = 1,
    HAILO_FORMAT_ORDER_HAILO_NMS_BY_CLASS = 22,
    HAILO_FORMAT_ORDER_OTHER = 2
};
enum hailo_format_type_t { HAILO_FORMAT_TYPE_FLOAT32 = 10 };
struct hailo_format_t {
    hailo_format_order_t order;
};
struct hailo_3d_image_shape_t {
    uint32_t height, width, features;
};
struct hailo_nms_shape_t {
    uint32_t number_of_classes;
};

namespace hailort {

template <typename T>
class Expected {
public:
    Expected(T v) : v_(std::move(v)), ok_(true) {}
    Expected() : ok_(false) {}
    explicit operator bool() const { return ok_; }
    hailo_status status() const { return ok_ ? 0 : -1; }
    T release() { return std::move(v_); }
    T* operator->() { return &v_; }

private:
    T v_{};
    bool ok_ = false;
};

class MemoryView {
public:
    MemoryView(void* /*p*/, std::size_t /*n*/) {}
};

struct AsyncInferCompletionInfo {
    hailo_status status = HAILO_SUCCESS;
};

class AsyncInferJob {
public:
    void detach() {}
};

class InferStreamBinding {
public:
    hailo_status set_buffer(const MemoryView&) { return HAILO_SUCCESS; }
};

class InferStream {
public:
    hailo_3d_image_shape_t shape() const { return {}; }
    std::size_t get_frame_size() const { return 0; }
    hailo_format_t format() const { return {}; }
    Expected<hailo_nms_shape_t> get_nms_shape() const { return {hailo_nms_shape_t{}}; }
    void set_nms_score_threshold(float) {}
    void set_format_type(hailo_format_type_t) {}
};

class ConfiguredInferModel {
public:
    class Bindings {
    public:
        Expected<InferStreamBinding> input(const std::string&) {
            return {InferStreamBinding{}};
        }
        Expected<InferStreamBinding> output(const std::string&) {
            return {InferStreamBinding{}};
        }
    };
    Expected<Bindings> create_bindings() { return {Bindings{}}; }
    hailo_status wait_for_async_ready(std::chrono::milliseconds) { return HAILO_SUCCESS; }
    Expected<AsyncInferJob> run_async(
        Bindings&, std::function<void(const AsyncInferCompletionInfo&)>) {
        return {AsyncInferJob{}};
    }
};

class InferModel {
public:
    std::vector<std::string> get_input_names() const { return {}; }
    std::vector<std::string> get_output_names() const { return {}; }
    Expected<InferStream> input(const std::string&) { return {InferStream{}}; }
    Expected<InferStream> output(const std::string&) { return {InferStream{}}; }
    Expected<ConfiguredInferModel> configure() { return {ConfiguredInferModel{}}; }
};

class VDevice {
public:
    static Expected<std::unique_ptr<VDevice>> create() {
        return Expected<std::unique_ptr<VDevice>>(nullptr);
    }
    Expected<std::shared_ptr<InferModel>> create_infer_model(const std::string&) {
        return Expected<std::shared_ptr<InferModel>>(nullptr);
    }
};

} // namespace hailort
