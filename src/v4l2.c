#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include "v4l2.h"

static int open_device(char *device, v4l2_context_t *ctx);
static int v4l2_close(v4l2_context_t *ctx);
static int start_capturing(v4l2_context_t *ctx);
static int init_device(v4l2_context_t *ctx);
static void main_loop(v4l2_context_t *ctx);

static int xioctl(int fh, int request, void *arg);
static int init_mmap(v4l2_context_t *ctx);
static int init_read(unsigned int buffer_size, v4l2_context_t *ctx);

static int read_frame(v4l2_context_t *ctx);

static int v4l2_close(v4l2_context_t *ctx)
{
        enum v4l2_buf_type type;
        unsigned int i;

        switch (ctx->io_method)
        {
        case IO_METHOD_READ:
                /* Nothing to do. */
                break;
        case IO_METHOD_MMAP:
                type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                xioctl(ctx->fd, VIDIOC_STREAMOFF, &type);
                break;
        }
        switch (ctx->io_method)
        {
        case IO_METHOD_READ:
                free(ctx->buffers[0].start);
                break;
        case IO_METHOD_MMAP:
                for (i = 0; i < ctx->n_buffers; ++i)
                        munmap(ctx->buffers[i].start, ctx->buffers[i].length);
                break;
        }
        free(ctx->buffers);
        close(ctx->fd);
        free(ctx);
        return 0;
}

v4l2_context_t *alloc_v4l2_context()
{
        v4l2_context_t *ctx = (v4l2_context_t *)malloc(sizeof(v4l2_context_t));
        ctx->open_device = open_device;
        ctx->init_device = init_device;
        ctx->start_capturing = start_capturing;
        ctx->main_loop = main_loop;
        ctx->close = v4l2_close;
        return ctx;
}

static int xioctl(int fh, int request, void *arg)
{
        int r;
        do
        {
                r = ioctl(fh, request, arg);
        } while (-1 == r && EINTR == errno);

        return r;
}

static int open_device(char *device, v4l2_context_t *ctx)
{
        ctx->dev_name = device;
        int ret;
        struct stat st;
        if (stat(device, &st) == -1)
        {
                fprintf(stderr, "Cannot identify '%s': %d, %s\n", device, errno, strerror(errno));
                return -1;
        }

        if (!S_ISCHR(st.st_mode))
        {
                fprintf(stderr, "%s is not device", ctx->dev_name);
                return -1;
        }

        ctx->fd = open(device, O_RDWR /* required */ | O_NONBLOCK, 0);
        if (ctx->fd == -1)
        {
                fprintf(stderr, "Cannot open '%s': %d, %s\\n", device, errno, strerror(errno));
                return -1;
        }
        return 0;
}

static int init_device(v4l2_context_t *ctx)
{
        struct v4l2_capability cap;
        struct v4l2_cropcap cropcap;
        struct v4l2_crop crop;
        struct v4l2_format fmt;
        unsigned int min;

        if (xioctl(ctx->fd, VIDIOC_QUERYCAP, &cap) == -1)
        {
                fprintf(stderr, "get VIDIOC_QUERYCAP error: %d, %s\n", errno, strerror(errno));
                return -1;
        }

        if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
        {
                fprintf(stderr, "%s is not video capture device\n", ctx->dev_name);
                return -1;
        }

        if (!(cap.capabilities & V4L2_CAP_STREAMING))
        {
                fprintf(stderr, "%s does not support streaming i/o\n", ctx->dev_name);

                if (!(cap.capabilities & V4L2_CAP_READWRITE))
                {
                        fprintf(stderr, "%s does not support read i/o\n", ctx->dev_name);
                        return -1;
                }
                ctx->io_method = IO_METHOD_READ;
        }
        else
        {
                ctx->io_method = IO_METHOD_MMAP;
        }
        /* Select video input, video standard and tune here. */
        memset(&cropcap, 0, sizeof(cropcap));
        cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (xioctl(ctx->fd, VIDIOC_CROPCAP, &cropcap) == 0)
        {
                memset(&crop, 0, sizeof(crop));
                crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                crop.c = cropcap.defrect; /* reset to default */

                if (xioctl(ctx->fd, VIDIOC_S_CROP, &crop) == -1)
                {
                        fprintf(stderr, "set VIDIOC_S_CROP failed: %d, %s\n", errno, strerror(errno));
                }
        }
        else
        {
                fprintf(stderr, "get VIDIOC_CROPCAP failed: %d, %s\n", errno, strerror(errno));
        }

        /* Enum pixel format */
        for (int i = 0; i < 20; i++)
        {
                struct v4l2_fmtdesc fmtdesc;
                fmtdesc.index = i;
                fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                if (xioctl(ctx->fd, VIDIOC_ENUM_FMT, &fmtdesc) == -1)
                        break;
                printf("%d: %s\n", i, fmtdesc.description);
        }

        memset(&fmt, 0, sizeof(fmt));
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ctx->force_format)
        {
                fmt.fmt.pix.width = ctx->width;
                fmt.fmt.pix.height = ctx->height;
                fmt.fmt.pix.pixelformat = ctx->pixelformat;
                fmt.fmt.pix.field = ctx->field;

                if (xioctl(ctx->fd, VIDIOC_S_FMT, &fmt) == -1)
                {
                        fprintf(stderr, "get VIDIOC_S_FMT failed: %d, %s\n", errno, strerror(errno));
                        return -1;
                }

                /* Note VIDIOC_S_FMT may change width and height. */
        }
        else
        {
                /* Preserve original settings as set by v4l2-ctl for example */
                if (xioctl(ctx->fd, VIDIOC_G_FMT, &fmt) == -1)
                {
                        fprintf(stderr, "get VIDIOC_G_FMT failed: %d, %s\n", errno, strerror(errno));
                        return -1;
                }
        }
        printf("fmt.w=%d,fmt.h=%d\n", fmt.fmt.pix.width, fmt.fmt.pix.height);
        printf("fmt.pixfmt=0x%x\n", fmt.fmt.pix.pixelformat);

        /* Buggy driver paranoia. */
        min = fmt.fmt.pix.width * 2;
        if (fmt.fmt.pix.bytesperline < min)
                fmt.fmt.pix.bytesperline = min;
        min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
        if (fmt.fmt.pix.sizeimage < min)
                fmt.fmt.pix.sizeimage = min;

        if (ctx->io_method == IO_METHOD_MMAP)
                return init_mmap(ctx);
        else
                return init_read(fmt.fmt.pix.sizeimage, ctx);
}

static void main_loop(v4l2_context_t *ctx)
{
        int fd = ctx->fd;
        int r;
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        for (;;)
        {
                struct timeval tv;
                tv.tv_sec = 2;
                tv.tv_usec = 0;

                fd_set rdset = fds;
                r = select(fd + 1, &rdset, NULL, NULL, &tv);

                if (r > 0)
                {
                        if (read_frame(ctx) == -2)
                                break;
                }
                else if (r == 0)
                {
                        fprintf(stderr, "select timeout\\n");
                }
                else
                {
                        if (EINTR == errno || EAGAIN == errno)
                                continue;
                        fprintf(stderr, "select failed: %d, %s\n", errno, strerror(errno));
                        break;
                }
                /* EAGAIN - continue select loop. */
        }
}

static int init_mmap(v4l2_context_t *ctx)
{
        struct v4l2_requestbuffers req;
        memset(&req, 0, sizeof(req));

        req.count = 4;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;

        if (xioctl(ctx->fd, VIDIOC_REQBUFS, &req) == -1)
        {
                fprintf(stderr, "set VIDIOC_REQBUFS failed: %d, %s\n", errno, strerror(errno));
                return -1;
        }

        if (req.count < 2)
        {
                fprintf(stderr, "Insufficient buffer memory on %s\n", ctx->dev_name);
                return -1;
        }

        ctx->buffers = (struct buffer *)calloc(req.count, sizeof(struct buffer));

        if (!ctx->buffers)
        {
                fprintf(stderr, "Out of memory\n");
                return -1;
        }

        for (ctx->n_buffers = 0; ctx->n_buffers < req.count; ++ctx->n_buffers)
        {
                struct v4l2_buffer buf;
                memset(&buf, 0, sizeof(buf));
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = ctx->n_buffers;

                if (xioctl(ctx->fd, VIDIOC_QUERYBUF, &buf) == -1)
                {
                        fprintf(stderr, "set VIDIOC_QUERYBUF %u failed: %d, %s\n", ctx->n_buffers, errno, strerror(errno));
                        return -1;
                }
                /* 建立映射关系
                 * 注意这里的索引号，v4l2_buf.index 与 usr_buf 的索引是一一对应的,
                 * 当我们将内核缓冲区出队时，可以通过查询内核缓冲区的索引来获取用户缓冲区的索引号，
                 * 进而能够知道应该在第几个用户缓冲区中取数据
                 */
                ctx->buffers[ctx->n_buffers].length = buf.length;
                ctx->buffers[ctx->n_buffers].start =
                    mmap(NULL /* start anywhere */,
                         buf.length,
                         PROT_READ | PROT_WRITE /* required */,
                         MAP_SHARED /* recommended */,
                         ctx->fd, buf.m.offset);

                if (MAP_FAILED == ctx->buffers[ctx->n_buffers].start)
                {
                        fprintf(stderr, "mmap %u failed: %d, %s\n", ctx->n_buffers, errno, strerror(errno));
                        return -1;
                }
        }

        return 0;
}

static int init_read(unsigned int buffer_size, v4l2_context_t *ctx)
{
        ctx->buffers = (struct buffer *)calloc(1, sizeof(struct buffer));

        if (!ctx->buffers)
        {
                fprintf(stderr, "Out of memory\n");
                return -1;
        }

        ctx->buffers[0].length = buffer_size;
        ctx->buffers[0].start = malloc(buffer_size);

        if (!ctx->buffers[0].start)
        {
                fprintf(stderr, "Out of memory\n");
                return -1;
        }

        return 0;
}

static int start_capturing(v4l2_context_t *ctx)
{
        printf("start_capturing\n");
        unsigned int i;
        enum v4l2_buf_type type;

        switch (ctx->io_method)
        {
        case IO_METHOD_READ:
                /* Nothing to do. */
                break;
        case IO_METHOD_MMAP:
                // 把所有的buffer 放到空闲链表
                for (i = 0; i < ctx->n_buffers; ++i)
                {
                        struct v4l2_buffer buf;
                        memset(&buf, 0, sizeof(buf));

                        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                        buf.memory = V4L2_MEMORY_MMAP;
                        buf.index = i;

                        if (xioctl(ctx->fd, VIDIOC_QBUF, &buf) == -1)
                        {
                                fprintf(stderr, "set VIDIOC_QBUF failed: %d, %s\n", errno, strerror(errno));
                                return -1;
                        }
                }
                // 启动摄像头
                type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                if (xioctl(ctx->fd, VIDIOC_STREAMON, &type) == -1)
                {
                        fprintf(stderr, "set VIDIOC_STREAMON failed: %d, %s\n", errno, strerror(errno));
                        return -1;
                }
                break;
        }
        return 0;
}

static int read_frame(v4l2_context_t *ctx)
{
        struct v4l2_buffer buf;
        switch (ctx->io_method)
        {
        case IO_METHOD_READ:
                if (read(ctx->fd, ctx->buffers[0].start, ctx->buffers[0].length) == -1)
                {
                        if (errno == EAGAIN || errno == EINTR)
                        {
                                return 0;
                        }
                        else
                        {
                                fprintf(stderr, "read failed: %d, %s\n", errno, strerror(errno));
                                return -1;
                        }
                }
                if (!(ctx->process_image)((uint8_t *)ctx->buffers[0].start, ctx->buffers[0].length,buf.timestamp))
                {
                        return -2;
                }
                break;

        case IO_METHOD_MMAP:
                memset(&buf, 0, sizeof(buf));

                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;

                if (xioctl(ctx->fd, VIDIOC_DQBUF, &buf) == -1)
                {
                        if (errno == EAGAIN || errno == EINTR)
                        {
                                return 0;
                        }
                        else
                        {
                                fprintf(stderr, "set VIDIOC_DQBUF failed: %d, %s\n", errno, strerror(errno));
                                return -1;
                        }
                }
                if (buf.index < ctx->n_buffers)
                {
                        // 因为内核缓冲区与用户缓冲区建立的映射，所以可以通过用户空间缓冲区直接访问这个缓冲区的数据,通过index访问
                        if (!(ctx->process_image)((uint8_t *)ctx->buffers[buf.index].start, buf.bytesused,buf.timestamp))
                        {
                                return -2;
                        }
                        if (-1 == xioctl(ctx->fd, VIDIOC_QBUF, &buf))
                        {
                                fprintf(stderr, "set VIDIOC_QBUF failed: %d, %s\n", errno, strerror(errno));
                                return -1;
                        }
                }
                break;
        }
        return 0;
}