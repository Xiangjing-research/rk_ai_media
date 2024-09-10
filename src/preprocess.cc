// Copyright (c) 2023 by Rockchip Electronics Co., Ltd. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "preprocess.h"

// void letterbox(const cv::Mat &image, cv::Mat &padded_image, BOX_RECT &pads, const float scale, const cv::Size &target_size, const cv::Scalar &pad_color)
// {
//     // 调整图像大小
//     cv::Mat resized_image;
//     cv::resize(image, resized_image, cv::Size(), scale, scale);

//     // 计算填充大小
//     int pad_width = target_size.width - resized_image.cols;
//     int pad_height = target_size.height - resized_image.rows;

//     pads.left = pad_width / 2;
//     pads.right = pad_width - pads.left;
//     pads.top = pad_height / 2;
//     pads.bottom = pad_height - pads.top;

//     // 在图像周围添加填充
//     cv::copyMakeBorder(resized_image, padded_image, pads.top, pads.bottom, pads.left, pads.right, cv::BORDER_CONSTANT, pad_color);
// }

int rgb24_resize(unsigned char *input_rgb, unsigned char *output_rgb, int width,
                 int height, int outwidth, int outheight)
{
    rga_buffer_t src =
        wrapbuffer_virtualaddr(input_rgb, width, height, RK_FORMAT_RGB_888);
    rga_buffer_t dst = wrapbuffer_virtualaddr(output_rgb, outwidth, outheight,
                                              RK_FORMAT_RGB_888);
    rga_buffer_t pat = {0};
    im_rect src_rect = {0, 0, width, height};
    im_rect dst_rect = {0, 0, outwidth, outheight};
    im_rect pat_rect = {0};
    IM_STATUS STATUS = improcess(src, dst, pat, src_rect, dst_rect, pat_rect, 0);
    if (STATUS != IM_STATUS_SUCCESS)
    {
        printf("imcrop failed: %s\n", imStrError(STATUS));
        return -1;
    }
    return 0;
}