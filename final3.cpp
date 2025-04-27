// finall.cpp

#include <cstdint>
#include <cstdio>
#include <csignal>   // For signal handling
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
#include <thread>
#include <chrono>
#include <syslog.h>
#include <pigpio.h>

#define DEVICE "/dev/video0"
#define WIDTH 640
#define HEIGHT 480
#define NBUF 4

struct Point2D { int x, y; };

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
    int speed_level;  // 1..3
};

// === Shared buffers & state ===
cv::Mat latest_frame;
std::mutex frame_mutex;

std::optional<Point2D> latest_laser_point;
std::mutex             point_mutex;
std::atomic<bool>      point_available{false};

std::optional<MovementCommand> latest_cmd;
std::mutex                     cmd_mutex;
std::atomic<bool>              cmd_available{false};

std::atomic<bool> stop_requested{false};

// === Camera buffer setup ===
struct Buffer { void* start; size_t length; };
struct CameraContext {
    int fd{-1};
    Buffer buffers[NBUF];
    v4l2_format fmt{};
} cam;

// Initialize camera
int init_camera() {
    cam.fd = open(DEVICE, O_RDWR | O_NONBLOCK);
    if (cam.fd < 0) { perror("Opening video device"); return 1; }
    cam.fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cam.fmt.fmt.pix.width  = WIDTH;
    cam.fmt.fmt.pix.height = HEIGHT;
    cam.fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    cam.fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(cam.fd, VIDIOC_S_FMT, &cam.fmt) < 0) { perror("Setting Pixel Format"); return 1; }

    v4l2_requestbuffers req{};
    req.count = NBUF;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(cam.fd, VIDIOC_REQBUFS, &req) < 0) { perror("Requesting Buffer"); return 1; }

    for (int i = 0; i < NBUF; ++i) {
        v4l2_buffer buf{};
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        if (ioctl(cam.fd, VIDIOC_QUERYBUF, &buf) < 0) { perror("Querying Buffer"); return 1; }
        cam.buffers[i].length = buf.length;
        cam.buffers[i].start  = mmap(nullptr, buf.length,
                                     PROT_READ|PROT_WRITE, MAP_SHARED,
                                     cam.fd, buf.m.offset);
        if (cam.buffers[i].start == MAP_FAILED) { perror("mmap"); return 1; }
        if (ioctl(cam.fd, VIDIOC_QBUF, &buf) < 0) { perror("Queue Buffer"); return 1; }
    }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(cam.fd, VIDIOC_STREAMON, &type) < 0) { perror("Start Capture"); return 1; }
    return 0;
}

// Service 1: camera capture
void camera_capture_service() {
    fd_set fds;
 
    FD_ZERO(&fds);
    FD_SET(cam.fd, &fds);


    v4l2_buffer buf{};
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(cam.fd, VIDIOC_DQBUF, &buf) < 0) {
        if (errno == EAGAIN) return;
        perror("Retrieving Frame"); return;
    }

    cv::Mat yuyv(HEIGHT, WIDTH, CV_8UC2, cam.buffers[buf.index].start);
    cv::Mat bgr;
    cv::cvtColor(yuyv, bgr, cv::COLOR_YUV2BGR_YUYV);
    {
        std::lock_guard lock(frame_mutex);
        latest_frame = bgr.clone();
    }

    if (ioctl(cam.fd, VIDIOC_QBUF, &buf) < 0) { perror("Requeue Buffer"); }
}

// Service 2: red‑laser detect
void red_laser_detect_and_show() {
    cv::Mat frame_copy;
    {
        std::lock_guard lock(frame_mutex);
        if (latest_frame.empty()) return;
        frame_copy = latest_frame.clone();
    }

    cv::Mat hsv, mask, l, u;
    cv::cvtColor(frame_copy, hsv, cv::COLOR_BGR2HSV);
    cv::inRange(hsv, cv::Scalar(  0,  70,  50), cv::Scalar( 10, 255, 255), l);
    cv::inRange(hsv, cv::Scalar(170,  70,  50), cv::Scalar(180, 255, 255), u);
    mask = l | u;
    cv::erode(mask, mask, {}, {-1,-1}, 2);
    cv::dilate(mask, mask, {}, {-1,-1}, 2);

    std::vector<std::vector<cv::Point>> C;
    cv::findContours(mask, C, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    for (auto &cnt : C) {
        if (cv::contourArea(cnt) < 50) continue;
        auto M = cv::moments(cnt);
        if (M.m00 == 0) continue;
        int cx = int(M.m10 / M.m00), cy = int(M.m01 / M.m00);
      //  syslog(LOG_INFO, "laser x,y %d %d", cx, cy);
        cv::circle(frame_copy, {cx, cy}, 5, {0,255,0}, -1);
        {
            std::lock_guard lock(point_mutex);
            latest_laser_point = Point2D{cx, cy};
            point_available.store(true, std::memory_order_release);
        }
    }
   //cv::imshow("Laser", frame_copy);
   //cv::waitKey(1);
}

// Service 3: decision (only on new laser point)
MovementCommand service3_decide_direction(Point2D pos) {
    MovementCommand cmd;
    bool L = pos.x < 200, C1 = pos.x >= 200,C2 = pos.x <= 450,C3 = pos.y <= 480, R = pos.x > 450;
    bool T = pos.y < 400, B = pos.y > 400;
    //if (L && T || (L && T)) cmd.dir = TURN_LEFT_THEN_FORWARD;
    //else if (R && T) cmd.dir = TURN_RIGHT_THEN_FORWARD;
    if (L && (B || T)) cmd.dir = LEFT;
    else if (R && (B || T)) cmd.dir = RIGHT;
    else if (C1 && C2 && C3  ) cmd.dir = FORWARD;
    else cmd.dir = STOP;
    cmd.speed_level = (pos.y < 160 ? 3 : (pos.y <= 320 ? 2 : 1));
    return cmd;
}

void service3_thread() {
    if (!point_available.load(std::memory_order_acquire)) return;

    std::optional<Point2D> p;
    {
        std::lock_guard lock(point_mutex);
        p = latest_laser_point;
        point_available.store(false, std::memory_order_release);
    }
    if (!p) return;

    auto cmd = service3_decide_direction(*p);
    {
        std::lock_guard lock(cmd_mutex);
        latest_cmd = cmd;
        cmd_available.store(true, std::memory_order_release);
    }

    const char* dirStr = "STOP";
    switch (cmd.dir) {
      case FORWARD:                 dirStr = "FORWARD";       break;
      case LEFT:                    dirStr = "LEFT";          break;
      case RIGHT:                   dirStr = "RIGHT";         break;
      case TURN_LEFT_THEN_FORWARD:  dirStr = "TL+THEN+FWD";   break;
      case TURN_RIGHT_THEN_FORWARD: dirStr = "TR+THEN+FWD";   break;
      case STOP: default:           dirStr = "STOP";          break;
    }
    syslog(LOG_INFO,
           "Service 3 → Dir: %s | Speed: %d | Pos=(%d,%d)",
           dirStr, cmd.speed_level, p->x, p->y
    );
}

// Service 4: Motor control + software‑PWM burst on ENA/ENB
struct MotorDriver {
    int chipFd{-1}, lineFd{-1};
    gpiohandle_data data{};
    // offsets: IN1,IN2,IN3,IN4, ENA, ENB
    unsigned offsets[6]{17,27,22,23, 18, 19};

    bool init() {
        chipFd = open("/dev/gpiochip0", O_RDONLY);
        if (chipFd < 0) {
            syslog(LOG_ERR,"MD open: %s", strerror(errno));
            return false;
        }
        gpiohandle_request r{};
        r.lines = 6;
        for (int i = 0; i < 6; ++i) r.lineoffsets[i] = offsets[i];
        r.flags = GPIOHANDLE_REQUEST_OUTPUT;
        if (ioctl(chipFd, GPIO_GET_LINEHANDLE_IOCTL, &r) < 0) {
            syslog(LOG_ERR,"MD GET_LINEHANDLE: %s", strerror(errno));
            close(chipFd);
            return false;
        }
        lineFd = r.fd;
        return true;
    }

    /// drive: A,B,C,D = direction bits
    /// speed = 1..3
    /// burst_ms = how many ms total to run
    /// dirStr = text to log
    void drive(bool A,bool B,bool C,bool D, int speed,
               int burst_ms, const char* dirStr)
    {
        // PWM slice = 10ms (high_ms+low_ms)
        int high_ms = speed * 3 + (speed == 3 ? 1 : 0); // 3,6,10
        if (speed == 1){
			high_ms = 6;
			}
		else if (speed == 2){high_ms=8;}
        int low_ms  = 10 - high_ms;
		
		
		int pwmPin = 18;            // GPIO18 = PWM0
		int pwmPin1 = 19;			// GPIO19 = PWM1

		// burst
		//int speed = 2; // 1,2 or 3
		int duty=0;
		if (speed==3) duty = 100;
		else if (speed==2) duty = 80;
		else if (speed==1) duty = 70;
			  
		gpioPWM(pwmPin, duty);
		gpioPWM(pwmPin1, duty);
		
		
        // direction pins
        data.values[0]=A; data.values[1]=B;
        data.values[2]=C; data.values[3]=D;

		ioctl(lineFd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);

		if ( (speed==3) || (speed==2) || (speed==1) ){
        syslog(LOG_INFO, "Service 4 → Dir: %s | PWM duty=%d ",
               dirStr, duty);
		   }
    }

    ~MotorDriver() {
        if (lineFd >= 0)  close(lineFd);
        if (chipFd >= 0)  close(chipFd);
    }
};
static MotorDriver drv;
void service4_motor_control() {
    
    static bool ok = false;
    /*if (!ok) {
        ok = drv.init();
        if (!ok) {
            syslog(LOG_ERR,"S4 MD init failed");
            return;
        }
*/
	const char* dirStr = "STOP";
    bool A=false,B=false,C=false,D=false;
    int speed = 0;
    if (!cmd_available.load(std::memory_order_acquire)){
    int burst_ms = 15;  
    drv.drive(A,B,C,D, speed, burst_ms, dirStr);
		return;
		} 

    std::optional<MovementCommand> c;
    {
        std::lock_guard lock(cmd_mutex);
        c = latest_cmd;
        cmd_available.store(false, std::memory_order_release);
    }
	
    
    if (c) {
        speed = c->speed_level; // 1..3
        switch (c->dir) {
          case FORWARD:
            A=true; C=true; dirStr="FORWARD";       break;
          case LEFT:
            B=false; C=true; dirStr="LEFT";          break;
          case RIGHT:
            A=true; D=false; dirStr="RIGHT";         break;
          case TURN_LEFT_THEN_FORWARD:
            B=false; C=true; dirStr="TL+THEN+FWD";  break;
          case TURN_RIGHT_THEN_FORWARD:
            A=true; D=false; dirStr="TR+THEN+FWD";  break;
          case STOP: default:
            speed = 0; dirStr="STOP";               break;
        }
    }

    // burst for e.g. 100ms (or scale by speed)
    int burst_ms = 15;  
    drv.drive(A,B,C,D, speed, burst_ms, dirStr);
}

// Signal handler
void signal_handler(int s) {
    syslog(LOG_INFO,"SIGINT, stopping...");
    stop_requested = true;
}

int main(){
	
    openlog("robot_rt", LOG_PID|LOG_PERROR, LOG_USER);
 
    if (init_camera()) return 1;
	
	if (gpioInitialise()<0) {
		syslog(LOG_ERR,"ERROR");
		return 1;};
		
	signal(SIGINT, signal_handler);
	
	int pwmPin = 18;            
	int pwmPin1 = 19;
	int range  = 100;           
	int freq   = 800;           

	gpioSetPWMfrequency(pwmPin, freq);
	gpioSetPWMrange(pwmPin, range);
	gpioSetPWMfrequency(pwmPin1, freq);
	pioSetPWMrange(pwmPin1, range);
			  
	  // Initialize kernel-GPIO driver (direction lines only)
  if (!drv.init()) {
    syslog(LOG_ERR, "MotorDriver init failed");
    return 1;
  }
  
    Sequencer seq;
    seq.addService(camera_capture_service,     1, 98,  30, 1, 1);
    seq.addService(red_laser_detect_and_show,  1, 97,  35, 2, 2);
    seq.addService(service3_thread,            1, 96,  40, 3, 3);
    seq.addService(service4_motor_control,     1, 95,  50, 4, 4);

    seq.startServices();
    syslog(LOG_INFO,"All services started.");

    while (!stop_requested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    seq.stopServices();
    syslog(LOG_INFO,"Services stopped, exiting.");
    return 0;
}
