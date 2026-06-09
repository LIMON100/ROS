#include "human_detector/async_inference.hpp"

#if defined(__unix__)
#include <sys/mman.h>
#endif

static std::shared_ptr<uint8_t> page_aligned_alloc(size_t size, void* buff = nullptr) {
    #if defined(__unix__)
        auto addr = mmap(buff, size, PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (MAP_FAILED == addr) throw std::bad_alloc();
        return std::shared_ptr<uint8_t>(reinterpret_cast<uint8_t*>(addr), [size](void *addr) { munmap(addr, size); });
    #else
        throw std::runtime_error("Aligned alloc not supported on this platform.");
    #endif
}

// Constructor 1: For the first model instance. Creates and owns the VDevice.
AsyncModelInfer::AsyncModelInfer(const std::string &hef_path)
    : m_job_is_running(false) // <-- INITIALIZE FLAG
{
    auto vdevice_exp = hailort::VDevice::create();
    if (!vdevice_exp) {
        throw std::runtime_error("Failed to create VDevice: " + std::to_string(vdevice_exp.status()));
    }
    this->vdevice = std::move(vdevice_exp.release());

    // ... (rest of constructor is the same)
    auto infer_model_exp = this->vdevice->create_infer_model(hef_path);
    if (!infer_model_exp) {
        std::cerr << "Failed to create infer model, status = " << infer_model_exp.status() << std::endl;
        throw std::runtime_error("Failed to create infer model");
    }
    this->infer_model = infer_model_exp.release();
    for (auto& output_vstream_info : this->infer_model->hef().get_output_vstream_infos().release()) {
        std::string name(output_vstream_info.name);
        this->output_vstream_info_by_name[name] = output_vstream_info;
    }
    this->configured_infer_model = this->infer_model->configure().expect("Failed to create configured infer model");
    this->bindings = configured_infer_model.create_bindings().expect("Failed to create infer bindings");
}

// Constructor 2: For subsequent instances. Shares an existing VDevice.
AsyncModelInfer::AsyncModelInfer(std::shared_ptr<hailort::VDevice> vdevice, const std::string &hef_path)
    : vdevice(vdevice), m_job_is_running(false) // <-- INITIALIZE FLAG
{
    // ... (rest of constructor is the same)
    auto infer_model_exp = this->vdevice->create_infer_model(hef_path);
    if (!infer_model_exp) {
        std::cerr << "Failed to create infer model from shared VDevice, status = " << infer_model_exp.status() << std::endl;
        throw std::runtime_error("Failed to create infer model");
    }
    this->infer_model = infer_model_exp.release();
    for (auto& output_vstream_info : this->infer_model->hef().get_output_vstream_infos().release()) {
        std::string name(output_vstream_info.name);
        this->output_vstream_info_by_name[name] = output_vstream_info;
    }
    this->configured_infer_model = this->infer_model->configure().expect("Failed to create configured infer model");
    this->bindings = configured_infer_model.create_bindings().expect("Failed to create infer bindings");
}

AsyncModelInfer::~AsyncModelInfer()
{
    // Now we check our own reliable flag.
    if (m_job_is_running) {
        last_infer_job.wait(std::chrono::milliseconds(1000));
    }
}

std::shared_ptr<hailort::VDevice> AsyncModelInfer::get_vdevice() {
    return this->vdevice;
}

const std::vector<hailort::InferModel::InferStream>& AsyncModelInfer::get_inputs(){
    return this->infer_model->inputs();
}

const std::vector<hailort::InferModel::InferStream>& AsyncModelInfer::get_outputs(){
    return this->infer_model->outputs();
}

const std::shared_ptr<hailort::InferModel> AsyncModelInfer::get_infer_model(){
    return this->infer_model;
}

void AsyncModelInfer::infer(
    std::shared_ptr<cv::Mat> input_data,
    std::function<void(const hailort::AsyncInferCompletionInfo&,
                       const std::vector<std::pair<uint8_t*, hailo_vstream_info_t>> &,
                       const std::vector<std::shared_ptr<uint8_t>> &)> callback) {

    std::vector<std::shared_ptr<cv::Mat>> input_guards;
    set_input_buffers(input_data, input_guards);

    std::vector<std::shared_ptr<uint8_t>> output_guards;
    auto output_data_and_infos = prepare_output_buffers(output_guards);

    wait_and_run_async(output_data_and_infos, output_guards, input_guards, callback);
}

void AsyncModelInfer::set_input_buffers(const std::shared_ptr<cv::Mat> &input_data,
                                        std::vector<std::shared_ptr<cv::Mat>> &input_guards) {
    for (const auto &input_name : infer_model->get_input_names()) {
        size_t frame_size = infer_model->input(input_name)->get_frame_size();
        auto status = bindings.input(input_name)->set_buffer(MemoryView(input_data->data, frame_size));
        if (HAILO_SUCCESS != status) {
            std::cerr << "Failed to set infer input buffer, status = " << status << std::endl;
        }
        input_guards.push_back(input_data);
    }
}

std::vector<std::pair<uint8_t*, hailo_vstream_info_t>> AsyncModelInfer::prepare_output_buffers(
    std::vector<std::shared_ptr<uint8_t>> &output_guards) {

    std::vector<std::pair<uint8_t*, hailo_vstream_info_t>> result;
    for (const auto &output_name : infer_model->get_output_names()) {
        size_t frame_size = infer_model->output(output_name)->get_frame_size();
        auto buffer = page_aligned_alloc(frame_size);
        auto status = bindings.output(output_name)->set_buffer(MemoryView(buffer.get(), frame_size));
        if (HAILO_SUCCESS != status) {
            std::cerr << "Failed to set output buffer, status = " << status << std::endl;
        }
        result.emplace_back(bindings.output(output_name)->get_buffer()->data(), output_vstream_info_by_name[output_name]);
        output_guards.push_back(buffer);
    }
    return result;
}

void AsyncModelInfer::wait_and_run_async(
    const std::vector<std::pair<uint8_t*, hailo_vstream_info_t>> &output_data_and_infos,
    const std::vector<std::shared_ptr<uint8_t>> &output_guards,
    const std::vector<std::shared_ptr<cv::Mat>> &input_guards,
    std::function<void(const hailort::AsyncInferCompletionInfo&,
                       const std::vector<std::pair<uint8_t*, hailo_vstream_info_t>> &,
                       const std::vector<std::shared_ptr<uint8_t>> &)> callback)
{
    auto status = configured_infer_model.wait_for_async_ready(std::chrono::milliseconds(1000));
    if (HAILO_SUCCESS != status) {
        std::cerr << "Failed wait_for_async_ready, status = " << status << std::endl;
    }
    auto job = configured_infer_model.run_async(
        bindings,
        [callback, output_data_and_infos, input_guards, output_guards](const hailort::AsyncInferCompletionInfo& info)
        {
            callback(info, output_data_and_infos, output_guards);
        }
    );
    if (!job) {
        std::cerr << "Failed to start async infer job, status = " << job.status() << std::endl;
        m_job_is_running = false; // Job failed, so it's not running
    } else {
        // If the job was created successfully, set our flag.
        m_job_is_running = true;
        last_infer_job = std::move(job.release());
        last_infer_job.detach();
    }
}