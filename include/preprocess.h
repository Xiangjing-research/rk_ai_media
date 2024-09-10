#ifndef _RKNN_YOLOV5_DEMO_PREPROCESS_H_
#define _RKNN_YOLOV5_DEMO_PREPROCESS_H_

#include <stdio.h>
#include "im2d.h"
#include "rga.h"
#include "RgaUtils.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// void letterbox(const cv::Mat &image, cv::Mat &padded_image, BOX_RECT &pads, const float scale, const cv::Size &target_size, const cv::Scalar &pad_color = cv::Scalar(128, 128, 128));

int rgb24_resize(unsigned char *input_rgb, unsigned char *output_rgb, int width,
                 int height, int outwidth, int outheight);
#endif //_RKNN_YOLOV5_DEMO_PREPROCESS_H_
