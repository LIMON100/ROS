#ifndef _HAILO_ASYNC_INFERENCE_HPP_
#define _HAILO_ASYNC_INFERENCE_HPP_

#include "hailo/hailort.hpp"
#include <map>
#include <vector>
#include <utility>
#include <functional>
#include <string>


#include <iostream>
#include <opencv2/opencv.hpp>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <memory>

using namespace hailort;

// Restore the complete and correct BoundedTSQueue ---
template<typename T>
class BoundedTSQueue {
private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cond_not_empty;
    std::condition_variable m_cond_not_full;
    const size_t m_max_size;
    std::atomic<bool> m_stopped;

public:
    explicit BoundedTSQueue(size_t max_size) : m_max_size(max_size), m_stopped(false) {}
    ~BoundedTSQueue() { stop(); }

    BoundedTSQueue(const BoundedTSQueue&) = delete;
    BoundedTSQueue& operator=(const BoundedTSQueue&) = delete;

    void push(T item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond_not_full.wait(lock, [this] { return m_queue.size() < m_max_size || m_stopped; });
        if (m_stopped) return;
        m_queue.push(std::move(item));
        m_cond_not_empty.notify_one();
    }

    bool pop(T &out_item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond_not_empty.wait(lock, [this] { return !m_queue.empty() || m_stopped; });
        if (m_stopped && m_queue.empty()) {
            return false;
        }
        out_item = std::move(m_queue.front());
        m_queue.pop();
        m_cond_not_full.notify_one();
        return true;
    }

    bool try_peek(T &out_item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            return false;
        }
        out_item = m_queue.front(); // This makes a copy for inspection
        return true;
    }
    
    void stop() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopped = true;
        }
        m_cond_not_empty.notify_all();
        m_cond_not_full.notify_all();
    }
    
    bool is_stopped() {
        return m_stopped.load();
    }
};

class AsyncModelInfer {
private:
    std::shared_ptr<hailort::VDevice> vdevice;
    std::shared_ptr<hailort::InferModel> infer_model;
    hailort::ConfiguredInferModel configured_infer_model;
    hailort::ConfiguredInferModel::Bindings bindings;
    hailort::AsyncInferJob last_infer_job;
    bool m_job_is_running;
    std::map<std::string, hailo_vstream_info_t> output_vstream_info_by_name;

    void set_input_buffers(const std::shared_ptr<cv::Mat> &input_data,
                           std::vector<std::shared_ptr<cv::Mat>> &input_guards);
    std::vector<std::pair<uint8_t*, hailo_vstream_info_t>> prepare_output_buffers(
        std::vector<std::shared_ptr<uint8_t>> &output_guards);
    void wait_and_run_async(
        const std::vector<std::pair<uint8_t*, hailo_vstream_info_t>> &output_data_and_infos,
        const std::vector<std::shared_ptr<uint8_t>> &output_guards,
        const std::vector<std::shared_ptr<cv::Mat>> &input_guards,
        std::function<void(const hailort::AsyncInferCompletionInfo&,
            const std::vector<std::pair<uint8_t*, hailo_vstream_info_t>> &,
            const std::vector<std::shared_ptr<uint8_t>> &)> callback);

public:
    AsyncModelInfer(const std::string &hef_path);
    AsyncModelInfer(std::shared_ptr<hailort::VDevice> vdevice, const std::string &hef_path);
    ~AsyncModelInfer();

    std::shared_ptr<hailort::VDevice> get_vdevice();
    const std::vector<hailort::InferModel::InferStream>& get_inputs();
    const std::vector<hailort::InferModel::InferStream>& get_outputs();
    const std::shared_ptr<hailort::InferModel> get_infer_model();
    
    void infer(
        std::shared_ptr<cv::Mat> input_data,
        std::function<void(
            const hailort::AsyncInferCompletionInfo &,
            const std::vector<std::pair<uint8_t*, hailo_vstream_info_t>> &,
            const std::vector<std::shared_ptr<uint8_t>> &)> callback);
};

#endif /* _HAILO_ASYNC_INFERENCE_HPP_ */