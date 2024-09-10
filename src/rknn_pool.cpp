/**
 *
 */
#include "rknn_pool.h"
#include "postprocess.h"
#include <string.h>

RknnPool::RknnPool(const std::string model_path, const int thread_num,
                   const std::string label_path)
{
  this->thread_num_ = thread_num;
  this->model_path_ = model_path;
  this->label_path_ = label_path;
  this->Init();
}

RknnPool::~RknnPool() { this->DeInit(); }

void RknnPool::Init()
{
  // init_post_process(this->label_path_);
  init_post_process();
  try
  {
    // 配置线程池
    this->pool_ = std::make_unique<ThreadPool>(this->thread_num_);
    // 这里每一个线程需要加载一个模型
    for (int i = 0; i < this->thread_num_; ++i)
    {

      rknn_app_context_t* ctx = (rknn_app_context_t* )malloc(sizeof(rknn_app_context_t));
      memset(ctx, 0, sizeof(rknn_app_context_t));
      models_.push_back(ctx);
    }
  }
  catch (const std::bad_alloc &e)
  {
    // KAYLORDUT_LOG_ERROR("Out of memory: {}", e.what());
    exit(EXIT_FAILURE);
  }
  for (int i = 0; i < this->thread_num_; ++i)
  {
    auto ret = init_yolov5_model(models_[i]);
    if (ret != 0)
    {
      // KAYLORDUT_LOG_ERROR("Init rknn model failed!");
      exit(EXIT_FAILURE);
    }
  }
}

void RknnPool::DeInit() { deinit_post_process(); }

void RknnPool::AddInferenceTask(img_info_t* src_img,
                                void* (*Callback)(object_detect_result_list))
{
  pool_->enqueue(
      [&](std::shared_ptr<img_info_t> src_img)
      {
        auto convert_img = image_process.Convert(*original_img);
        auto mode_id = get_model_id();
        cv::Mat rgb_img = cv::Mat::zeros(
            this->models_[mode_id]->get_model_width(),
            this->models_[mode_id]->get_model_height(), convert_img->type());
        cv::cvtColor(*convert_img, rgb_img, cv::COLOR_RGB2BGR);
        object_detect_result_list od_results;
        this->models_[mode_id]->Inference(rgb_img.ptr(), &od_results,
                                          image_process.get_letter_box());
        image_process.ImagePostProcess(*original_img, od_results);
        std::lock_guard<std::mutex> lock_guard(this->image_results_mutex_);
        this->image_results_.push(std::move(original_img));
      },
      std::move(src));
}

int RknnPool::get_model_id()
{
  std::lock_guard<std::mutex> lock(id_mutex_);
  int mode_id = id % thread_num_;
  id++;
  //  KAYLORDUT_LOG_INFO("id = {}, num = {}, mode id = {}", id, thread_num_,
  //  mode_id);
  return mode_id;
}

std::shared_ptr<cv::Mat> RknnPool::GetImageResultFromQueue()
{
  std::lock_guard<std::mutex> lock_guard(this->image_results_mutex_);
  if (this->image_results_.empty())
  {
    return nullptr;
  }
  else
  {
    auto res = this->image_results_.front();
    this->image_results_.pop();
    return std::move(res);
  }
}

int RknnPool::GetTasksSize() { return pool_->TasksSize(); }