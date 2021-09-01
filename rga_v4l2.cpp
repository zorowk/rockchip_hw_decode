#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h> /* getopt_long() */
#include <fcntl.h> /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <dlfcn.h>

#include <linux/videodev2.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>


#ifdef USE_RGA
#include "rga_const.h"
#endif

extern "C" {
#define virtual vir
#include <drm.h>
#include <drm_mode.h>
#undef virtual
}

#include <xf86drm.h>
#include <xf86drmMode.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define FMT_NUM_PLANES 1

#define BUFFER_COUNT 4

enum io_method {
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR,
    IO_METHOD_DMABUF,
};

struct buffer {
    void *start;
    size_t length;
    struct v4l2_buffer v4l2_buf;
};

struct display_buffer {
	void *start;
	size_t length;
	size_t width;
	size_t height;
	int buf_fd;
	int bpp;
};

static char dev_name[255] = "/dev/video0";
static int width = 640;
static int height = 480;
static int format = V4L2_PIX_FMT_NV12;
static int fd = -1;
static int io = IO_METHOD_MMAP;
static enum v4l2_buf_type buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
struct buffer *buffers;
static unsigned int n_buffers;
static int frame_count = 6000;
FILE *fp=NULL;
static int debug = 1;

int dev_fd;

#ifdef USE_RGA
RockchipRga *rga;
bo_t bo_dst;
rga_info_t rga_info_src;
rga_info_t rga_info_dst;
#endif

struct display_buffer disp_buf;
cv::Mat* mat;

#define DBG(...) do { if(!debug) printf(__VA_ARGS__); } while(0)
#define ERR(...) do { fprintf(stderr, __VA_ARGS__); } while (0)

static void errno_exit(const char *s)
{
        ERR("%s error %d, %s\n", s, errno, strerror(errno));
        exit(EXIT_FAILURE);
}

static int xioctl(int fh, int request, void *arg)
{
    int r;
    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);
    return r;
}

static void process_buffer(struct buffer* buff, int size)
{

	if (fp) {
		fwrite(buff->start, size, 1, fp);
		fflush(fp);
	}

#ifdef USE_RGA
	rga_info_src.virAddr = buff->start;
	rga->RkRgaBlit(&rga_info_src, &rga_info_dst, NULL);

	cv::imshow("video", *mat);
#else
    cv::Mat yuvmat(cv::Size(width, height*3/2), CV_8UC1, buff->start);
    cv::Mat rgbmat(cv::Size(width, height), CV_8UC3);
    cv::cvtColor(yuvmat, rgbmat, CV_YUV2BGR_NV12);
    cv::imshow("video", rgbmat);
#endif

	cv::waitKey(1);
}

static int read_frame()
{
    struct v4l2_buffer buf;
    int i, bytesused;

    CLEAR(buf);

    buf.type = buf_type;
    buf.memory = V4L2_MEMORY_MMAP;

    if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == buf_type) {
        struct v4l2_plane planes[FMT_NUM_PLANES];
        buf.m.planes = planes;
        buf.length = FMT_NUM_PLANES;
    }

    if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf))
            errno_exit("VIDIOC_DQBUF");

    i = buf.index;

    if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == buf_type)
        bytesused = buf.m.planes[0].bytesused;
    else
        bytesused = buf.bytesused;
    process_buffer(&(buffers[i]), bytesused);
    DBG("bytesused %d\n", bytesused);

    if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
        errno_exit("VIDIOC_QBUF");

    return 1;
}

static unsigned long get_time(void)
{
	struct timeval ts;
	gettimeofday(&ts, NULL);
	return (ts.tv_sec * 1000 + ts.tv_usec / 1000);
}

static void mainloop(void)
{
        unsigned int count = 1;
        //pthread_create(&display_thread_id, NULL, RgaProcessThread, dec);
        unsigned long read_start_time, read_end_time;
        while (1) {

            DBG("No.%d\n", count);        //显示当前帧数目
            if(count++ == frame_count)
                break;

			read_start_time = get_time();

            read_frame();
			read_end_time = get_time();
			DBG("take time %lu ms\n",read_end_time - read_start_time);
        }
        DBG("\nREAD AND SAVE DONE!\n");
}

static void stop_capturing(void)
{
    enum v4l2_buf_type type;

    type = buf_type;
    if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
        errno_exit("VIDIOC_STREAMOFF");
}

static void start_capturing(void)
{
    unsigned int i;
    enum v4l2_buf_type type;

    for (i = 0; i < n_buffers; ++i) {
        struct v4l2_buffer buf;

        CLEAR(buf);
        buf.type = buf_type;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == buf_type) {
            struct v4l2_plane planes[FMT_NUM_PLANES];

            buf.m.planes = planes;
            buf.length = FMT_NUM_PLANES;
        }
        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
            errno_exit("VIDIOC_QBUF");
    }
    type = buf_type;
    if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
        errno_exit("VIDIOC_STREAMON");
}

static void uninit_device(void)
{
    unsigned int i;
    struct drm_mode_destroy_dumb destory_arg;

    for (i = 0; i < n_buffers; ++i) {
            if (-1 == munmap(buffers[i].start, buffers[i].length))
                    errno_exit("munmap");
    }

    free(buffers);
}

static void init_mmap(void)
{
    struct v4l2_requestbuffers req;

    CLEAR(req);

    req.count = BUFFER_COUNT;
    req.type = buf_type;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            ERR("%s does not support memory mapping\n", dev_name);
            exit(EXIT_FAILURE);
        } else {
            errno_exit("VIDIOC_REQBUFS");
        }
    }

    if (req.count < 2) {
        ERR("Insufficient buffer memory on %s\n", dev_name);
        exit(EXIT_FAILURE);
    }

    buffers = (struct buffer*)calloc(req.count, sizeof(*buffers));

    if (!buffers) {
        ERR("Out of memory\n");
        exit(EXIT_FAILURE);
    }

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;
        struct v4l2_plane planes[FMT_NUM_PLANES];
        CLEAR(buf);
        CLEAR(planes);

        buf.type = buf_type;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;

        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == buf_type) {
            buf.m.planes = planes;
            buf.length = FMT_NUM_PLANES;
        }

        if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
                errno_exit("VIDIOC_QUERYBUF");

        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == buf_type) {
            buffers[n_buffers].length = buf.m.planes[0].length;
            buffers[n_buffers].start =
                mmap(NULL /* start anywhere */,
                        buf.m.planes[0].length,
                        PROT_READ | PROT_WRITE /* required */,
                        MAP_SHARED /* recommended */,
                        fd, buf.m.planes[0].m.mem_offset);
        } else {
            buffers[n_buffers].length = buf.length;
            buffers[n_buffers].start =
                mmap(NULL /* start anywhere */,
                        buf.length,
                        PROT_READ | PROT_WRITE /* required */,
                        MAP_SHARED /* recommended */,
                        fd, buf.m.offset);
        }

        if (MAP_FAILED == buffers[n_buffers].start)
                errno_exit("mmap");
    }
}

static void init_display_buf(int buffer_size, int width, int height)
{
    int bpp;
    int export_dmafd;

    //disp_buf.start = get_drm_fd(drm_fd, width, height, 24, &export_dmafd);
	bpp = buffer_size * 8 / width / height;
	disp_buf.length= buffer_size;
	disp_buf.width = width;
	disp_buf.height= height;
	disp_buf.bpp   = bpp;
	//disp_buf.buf_fd= export_dmafd;

	//mat = new cv::Mat(cv::Size(width, height), CV_8UC3, disp_buf.start);
#ifdef USE_RGA
	//alloc a buffer for rga input
//dst
	rga->RkRgaGetAllocBuffer(&bo_dst, disp_buf.width, disp_buf.height, 32);
	rga->RkRgaGetBufferFd(&bo_dst, &(rga_info_dst.fd));

	set_rect_size(&(rga_info_dst.rect), disp_buf.width, disp_buf.height);
	set_rect_crop(&(rga_info_dst.rect), 0, 0, disp_buf.width, disp_buf.height);
	set_rect_format(&(rga_info_dst.rect), V4l2ToRgaFormat(V4L2_PIX_FMT_RGB24));

	rga->RkRgaGetMmap(&bo_dst);
	mat = new cv::Mat(cv::Size(width, height), CV_8UC3, bo_dst.ptr);

//src
	set_rect_size(&(rga_info_src.rect), width, height);
	set_rect_crop(&(rga_info_src.rect), 0, 0, width, height);
	set_rect_format(&(rga_info_src.rect), V4l2ToRgaFormat(format));
#endif
	cv::namedWindow("video");
}

#ifdef USE_RGA
static void init_rga(struct display_buffer* disp_buf)
{
	rga = new RockchipRga();
	rga->RkRgaInit();

	memset(&rga_info_src, 0, sizeof(rga_info_t));
	rga_info_src.mmuFlag = 1;
	rga_info_src.fd = -1;

	memset(&rga_info_dst, 0, sizeof(rga_info_t));
	rga_info_dst.mmuFlag = 1;
	rga_info_dst.fd = -1;
}
#endif

static void init_device(void)
{
    struct v4l2_capability cap;
    struct v4l2_format fmt;

    if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
            if (EINVAL == errno) {
                ERR("%s is no V4L2 device\n",dev_name);
                exit(EXIT_FAILURE);
            } else {
                errno_exit("VIDIOC_QUERYCAP");
            }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
            !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
        ERR("%s is not a video capture device, capabilities: %x\n", dev_name, cap.capabilities);
        exit(EXIT_FAILURE);
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        ERR("%s does not support streaming i/o\n",dev_name);
        exit(EXIT_FAILURE);
    }

    if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)
        buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    else if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
        buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    CLEAR(fmt);
    fmt.type = buf_type;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = format;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
            errno_exit("VIDIOC_S_FMT");

    init_mmap();

#ifdef USE_RGA
    init_rga(&disp_buf);
#endif
    init_display_buf(fmt.fmt.pix.sizeimage, width, height);
}

static void close_device(void)
{
    if (-1 == close(fd))
            errno_exit("close");

    fd = -1;
}

static void open_device(void)
{
    fd = open("/dev/video0", O_RDWR /* required */ /*| O_NONBLOCK*/, 0);

    if (-1 == fd) {
        ERR("Cannot open 'video0': %d, %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void sysfail(char *msg)
{
	perror(msg);
	exit(1);
}

int main(int argc, char **argv)
{
    open_device();
    init_device();
    start_capturing();
    mainloop();

    if (fp) fclose(fp);

    stop_capturing();
    uninit_device();
    close_device();
    return 0;
}
