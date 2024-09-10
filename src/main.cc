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

/*-------------------------------------------
                Includes
-------------------------------------------*/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <stack>

#include "preprocess.h"
#include "yolov5.h"

extern "C"
{
#include "image_utils.h"
#include "file_utils.h"
#include "image_drawing.h"
#include "v4l2.h"
#include <fcntl.h>
}

static long getCurrentTimeMsec()
{
    long msec = 0;
    char str[20] = {0};
    struct timeval stuCurrentTime;

    gettimeofday(&stuCurrentTime, NULL);
    sprintf(str, "%ld%03ld", stuCurrentTime.tv_sec,
            (stuCurrentTime.tv_usec) / 1000);
    for (size_t i = 0; i < strlen(str); i++)
    {
        msec = msec * 10 + (str[i] - '0');
    }

    return msec;
}

rknn_app_context_t rknn_app_ctx;
v4l2_context_t *v4l2_ctx;
std::stack<image_buffer_t *> frame_stack;
static int g_flag_run = 1;

static void save_image(uint8_t *p, int size, char *path)
{
    char filename[64];
    printf("save_image\n");
    sprintf(filename, "./output/video_raw_data_%s_%ld.yuv", path, (getCurrentTimeMsec() >> 4));
    printf("capture to %s\n", filename);
    int fd_file = open(filename, O_RDWR | O_CREAT, 0666);
    if (fd_file < 0)
    {
        printf("can not create file : %s\n", filename);
    }

    write(fd_file, p, size);
    close(fd_file);
}

static void *StartStream(void *arg)
{

    v4l2_ctx = alloc_v4l2_context();
    /*Configure v4l2_context_t*/
    v4l2_ctx->process_image = [](uint8_t *p, int size, struct timeval) -> _Bool
    {
        // 这里是lambda函数体，根据需要编写判断逻辑
        // 返回true或false

        // sleep(1);
        // save_image(p, size, "v4l2buffer");

        // printf("size=%d\n",size);size=614400
        image_buffer_t *image = (image_buffer_t *)malloc(sizeof(rga_buffer_t));
        memset(image, 0, sizeof(rga_buffer_t));
        image->width = v4l2_ctx->width;
        image->height = v4l2_ctx->height;
        image->format = IMAGE_FORMAT_YUYV_422;
        memcpy(image->virt_addr, p, size);

        cvtcolor_rga(image, IMAGE_FORMAT_RGB888);

        frame_stack.push(image);
        printf("frame_stack_size = %d\n", frame_stack.size());
        if (frame_stack.size() > 10)
        {
            printf("clear_frame_stack\n");
            // 清空
            while (!frame_stack.empty())
                frame_stack.pop();
        }

        return 1;
    };
    v4l2_ctx->force_format = 1;
    v4l2_ctx->width = 640;
    v4l2_ctx->height = 480;
    v4l2_ctx->pixelformat = V4L2_PIX_FMT_YUYV;
    v4l2_ctx->field = V4L2_FIELD_INTERLACED;
    v4l2_ctx->open_device((char *)arg, v4l2_ctx);
    v4l2_ctx->init_device(v4l2_ctx); // 调用init_mmap
    v4l2_ctx->start_capturing(v4l2_ctx);
    v4l2_ctx->main_loop(v4l2_ctx);
    v4l2_ctx->close(v4l2_ctx);
}

/*-------------------------------------------
                  Main Function
-------------------------------------------*/
int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("%s <model_path> <dev_path>\n", argv[0]);
        return -1;
    }

    const char *model_path = argv[1];
    const char *dev_path = argv[2];

    int ret = 0;

    memset(&rknn_app_ctx, 0, sizeof(rknn_app_context_t));

    init_post_process();
    rknn_app_ctx.model_path = model_path;
    ret = init_yolov5_model(&rknn_app_ctx);
    if (ret != 0)
    {
        printf("init_yolov5_model fail! ret=%d model_path=%s\n", ret, model_path);
        goto out;
    }

    // image_buffer_t src_image;
    // memset(&src_image, 0, sizeof(image_buffer_t));
    // ret = read_image(image_path, &src_image);
    pthread_t read_thread;
    pthread_create(&read_thread, NULL, StartStream, (void *)dev_path);
    object_detect_result_list od_results;
    while (g_flag_run)
    {
        if (frame_stack.empty())
        {
            continue;
        }
        else
        {
            long start_time = getCurrentTimeMsec();
            image_buffer_t *src_img = frame_stack.top();
            frame_stack.pop();

            // uint8_t *rgb_buffer = (uint8_t *)malloc(v4l2_ctx->width * v4l2_ctx->height * 3);

            // yuyv_to_rgb(src_img.buf, rgb_buffer, src_img.img_width, src_img.img_height);
            // save_image(rgb_buffer,src_img.img_width * src_img.img_height * 3, "rgb_buffer");
            // int rga_buffer_model_input_size = rknn_app_ctx.model_width * rknn_app_ctx.model_height * get_bpp_from_format(RK_FORMAT_RGB_888);

            // uint8_t *rga_buffer_model_input = (uint8_t *)malloc(rga_buffer_model_input_size);

            // rgb24_resize(rgb_buffer, rga_buffer_model_input, src_img.img_width, src_img.img_height, rknn_app_ctx.model_width, rknn_app_ctx.model_height);

            ret = inference_yolov5_model(&rknn_app_ctx, src_img, &od_results);
            long end_time = getCurrentTimeMsec();
            printf("infernece_once=%ldms\n", end_time - start_time);
            if (ret != 0)
            {
                printf("init_yolov5_model fail! ret=%d\n", ret);
                goto out;
            }

            // 画框和概率
            char text[256];
            for (int i = 0; i < od_results.count; i++)
            {
                object_detect_result *det_result = &(od_results.results[i]);
                printf("%s @ (%d %d %d %d) %.3f\n", coco_cls_to_name(det_result->cls_id),
                       det_result->box.left, det_result->box.top,
                       det_result->box.right, det_result->box.bottom,
                       det_result->prop);
                // int x1 = det_result->box.left;
                // int y1 = det_result->box.top;
                // int x2 = det_result->box.right;
                // int y2 = det_result->box.bottom;

                // draw_rectangle(&src_image, x1, y1, x2 - x1, y2 - y1, COLOR_BLUE, 3);

                // sprintf(text, "%s %.1f%%", coco_cls_to_name(det_result->cls_id), det_result->prop * 100);
                // draw_text(&src_image, text, x1, y1 - 20, COLOR_RED, 10);
            }
        }
    }
out:
    deinit_post_process();
    pthread_join(read_thread, NULL);
    ret = release_yolov5_model(&rknn_app_ctx);
    if (ret != 0)
    {
        printf("release_yolov5_model fail! ret=%d\n", ret);
    }
    return 0;
}
