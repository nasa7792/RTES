#include <cstdint>
#include <cstdio>
#include <csignal> // For signal handling
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <linux/gpio.h>
#include <sys/ioctl.h>
#include <cstring>
#include <linux/videodev2.h>
#include <opencv2/opencv.hpp>
#include <sys/mman.h>
#include "Sequencer.hpp"
#include <optional>
#include <mutex>
#include <sys/stat.h>
#include <thread>
#include <chrono>
#define DEVICE "/dev/video0"
#define WIDTH 640
#define HEIGHT 480
#define NBUF 4

cv::Mat latest_frame;
std::mutex frame_mutex;  
std::mutex point_mutex;
std::atomic<bool> stop_requested(false);

struct Point2D {
    int x;
    int y;
};


std::optional<Point2D> latest_laser_point;

enum Direction {
    STOP,
    FORWARD,
    LEFT,
    RIGHT,
    TURN_LEFT_THEN_FORWARD,
    TURN_RIGHT_THEN_FORWARD
};

struct MovementCommand {
    Direction dir;
    int speed_level;
};
struct Buffer {
    void* start;
    size_t length;
};

typedef struct CameraContext{
	int fd=-1;
	Buffer buffers[NBUF];
    v4l2_format fmt{};

}CameraContext;

//try to not use globals later
CameraContext cam;

int init_camera()
{
//open the device in a non blocking mode
  const char* dev_name = "/dev/video0";
     cam.fd = open(dev_name, O_RDWR | O_NONBLOCK);
    if (cam.fd == -1) {
        perror("Opening video device");
        return 1;
    }
    
    //set format
    cam.fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cam.fmt.fmt.pix.width = WIDTH;
    cam.fmt.fmt.pix.height = HEIGHT;
    cam.fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    cam.fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(cam.fd, VIDIOC_S_FMT, &cam.fmt) == -1) {
        perror("Setting Pixel Format");
        return 1;
    }
    
    //request for buffer to store our frames
    v4l2_requestbuffers req = {};
    req.count = NBUF;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(cam.fd, VIDIOC_REQBUFS, &req) == -1) {
        perror("Requesting Buffer");
        return 1;
    }
    
    //query and queue buffers
    for (int i = 0; i < NBUF; ++i) {
        v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(cam.fd, VIDIOC_QUERYBUF, &buf) == -1) {
            perror("Querying Buffer");
            return 1;
        }

        cam.buffers[i].length = buf.length;
        cam.buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, cam.fd, buf.m.offset);
        
        if (cam.buffers[i].start == MAP_FAILED) {
            perror("mmap");
            return 1;
        }

        if (ioctl(cam.fd, VIDIOC_QBUF, &buf) == -1) {
            perror("Queue Buffer");
            return 1;
        }
    }
    // Start streaming
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(cam.fd, VIDIOC_STREAMON, &type) == -1) {
        perror("Start Capture");
        return 1;
    }
    return 0;
}	

void camera_capture_service() {
    fd_set fds;
    struct timeval tv;
    int r;
 
        FD_ZERO(&fds);
        FD_SET(cam.fd, &fds);
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        r = select(cam.fd + 1, &fds, NULL, NULL, &tv);
        if (r == -1) {
            perror("Waiting for Frame");
            return;
        }

        v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl(cam.fd, VIDIOC_DQBUF, &buf) == -1) {
            if (errno == EAGAIN) return;
            perror("Retrieving Frame");
            return;
        }

        cv::Mat yuyv(HEIGHT, WIDTH, CV_8UC2, cam.buffers[buf.index].start);
        cv::Mat bgr;
        cv::cvtColor(yuyv, bgr, cv::COLOR_YUV2BGR_YUYV);

        {
            std::lock_guard<std::mutex> lock(frame_mutex);
            latest_frame = bgr.clone();
        }

        if (ioctl(cam.fd, VIDIOC_QBUF, &buf) == -1) {
            perror("Requeue Buffer");
            return;
        }
    
}

void red_laser_detect_and_show() {
    cv::Mat frame_copy;

    {
        std::lock_guard<std::mutex> lock(frame_mutex);
        if (latest_frame.empty()) return;
        frame_copy = latest_frame.clone();
    }

    cv::Mat hsv, mask;
    cv::cvtColor(frame_copy, hsv, cv::COLOR_BGR2HSV);

    // Red color range
    cv::Mat lower_red, upper_red;
cv::inRange(hsv, cv::Scalar(0, 70, 50), cv::Scalar(10, 255, 255), lower_red);
cv::inRange(hsv, cv::Scalar(170, 70, 50), cv::Scalar(180, 255, 255), upper_red);
    mask = lower_red | upper_red;

    cv::erode(mask, mask, cv::Mat(), cv::Point(-1, -1), 2);
    cv::dilate(mask, mask, cv::Mat(), cv::Point(-1, -1), 2);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    for (const auto& contour : contours) {
        if (cv::contourArea(contour) < 50) continue;
        cv::Moments m = cv::moments(contour);
        if (m.m00 != 0) {
            int cx = int(m.m10 / m.m00);
            int cy = int(m.m01 / m.m00);
            syslog(LOG_INFO,"laser detected x,y %d %d",cx,cy);
            cv::circle(frame_copy, cv::Point(cx, cy), 5, cv::Scalar(0, 255, 0), -1);
            {
                std::lock_guard<std::mutex> lock(point_mutex);
                latest_laser_point = Point2D{cx, cy};
            }
            
        }
    }
//do not enable these | only for debugging enable
cv::imshow("Red Laser Detection", frame_copy);
    
cv::waitKey(1);
}
MovementCommand service3_decide_direction(Point2D pos) {
    MovementCommand cmd;
    bool isLeft = pos.x < 213;
    bool isCenter = pos.x >= 213 && pos.x <= 426;
    bool isRight = pos.x > 426;
    bool isTop = pos.y < 160;
    bool isBottom = pos.y > 320;

    if (isLeft && isTop) {
        cmd.dir = TURN_LEFT_THEN_FORWARD;
    } else if (isRight && isTop) {
        cmd.dir = TURN_RIGHT_THEN_FORWARD;
    } else if (isLeft && isBottom) {
        cmd.dir = LEFT;
    } else if (isRight && isBottom) {
        cmd.dir = RIGHT;
    } else if (isCenter && isTop) {
        cmd.dir = FORWARD;
    } else {
        cmd.dir = STOP;
    }

    if (pos.y < 160) {
        cmd.speed_level = 3;
    } else if (pos.y <= 320) {
        cmd.speed_level = 2;
    } else {
        cmd.speed_level = 1;
    }
    return cmd;
}

void service3_thread() {
    std::optional<Point2D> point_copy;
    {
        std::lock_guard<std::mutex> lock(point_mutex);
        point_copy = latest_laser_point;
    }
    if (point_copy) {
        MovementCommand cmd = service3_decide_direction(*point_copy);
        std::string dirStr;
        switch (cmd.dir) {
            case FORWARD: dirStr = "FORWARD"; break;
            case LEFT: dirStr = "LEFT"; break;
            case RIGHT: dirStr = "RIGHT"; break;
            case TURN_LEFT_THEN_FORWARD: dirStr = "TURN LEFT THEN FORWARD"; break;
            case TURN_RIGHT_THEN_FORWARD: dirStr = "TURN RIGHT THEN FORWARD"; break;
            case STOP: default: dirStr = "STOP"; break;
        }
        syslog(LOG_INFO, "Service 3 Decision: %s | Speed: %d | Pos=(%d,%d)",
               dirStr.c_str(), cmd.speed_level, point_copy->x, point_copy->y);
    }
}


void signal_handler(int signum) {
    syslog(LOG_INFO, "Interrupt signal (%d) received. Stopping services...", signum);
    stop_requested = true;
}

int main() {
    openlog("LOG_MSG", LOG_PID | LOG_PERROR, LOG_USER);

    signal(SIGINT, signal_handler); // Register handler for Ctrl+C

    if (init_camera() != 0) return 1;

    Sequencer sequencer{};
    
    //service 1 is camera service running at 1000/40 25fps
    sequencer.addService(camera_capture_service, 1, 98, 40, 1, 1);
    sequencer.addService(red_laser_detect_and_show, 1, 97, 30, 2, 2);
    sequencer.addService(service3_thread, 1, 96, 30, 3, 3);
    sequencer.startServices();

    // Wait until Ctrl+C is pressed
    while (!stop_requested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    sequencer.stopServices();
    syslog(LOG_INFO, "Services stopped. Exiting.");
    return 0;
}

