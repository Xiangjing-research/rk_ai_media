
#pragma once
#include "queue"
#include "threadpool.h"
#include "yolov5.h"
#include "preprocess.h"
class RknnPool {
 public:
  RknnPool(const std::string model_path, const int thread_num,
           const std::string label_path);
  ~RknnPool();
  void Init();
  void DeInit();
  void AddInferenceTask(img_info_t* src_img,
                        void* (*Callback)(object_detect_result_list));
  int get_model_id();
  // std::shared_ptr<cv::Mat> GetImageResultFromQueue();
  // int GetTasksSize();

 private:
  int thread_num_{1};
  std::string model_path_{"null"};
  std::string label_path_{"null"};
  uint32_t id{0};
  std::unique_ptr<ThreadPool> pool_;
  std::queue<uint8_t> image_results_;
  std::vector<rknn_app_context_t* > models_;
  std::mutex id_mutex_;
  std::mutex image_results_mutex_;
};
