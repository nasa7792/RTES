// RobotVisionControl.cpp
// Grabs frames from the Pi camera, finds the laser dot, decides a direction,
// and drives the L298N‑controlled wheels in 10 ms bursts via the GPIO chardev ioctl API.
//
// Build with:
//   g++ --std=c++23 -Wall -Werror -pedantic `pkg-config --cflags --libs opencv4` \
//       RobotVisionControl.cpp -o robot_vision_exec
//
// Run with sudo:
//   sudo ./robot_vision_exec

#include <opencv2/opencv.hpp>
#include <linux/gpio.h>
#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>
#include <vector>

// === MotorDriver via ioctl ===
struct MotorDriver {
    int chipFd{-1}, lineFd{-1};
    gpiohandle_data data{};
    // BCM offsets for L298N IN1..IN4
    unsigned offsets[4] = {17, 27, 22, 23};

    bool init() {
        chipFd = open("/dev/gpiochip0", O_RDONLY);
        if (chipFd < 0) {
            std::cerr << "open gpiochip0: " << strerror(errno) << "\n";
            return false;
        }
        gpiohandle_request req{};
        req.lines = 4;
        for (int i = 0; i < 4; ++i)
            req.lineoffsets[i] = offsets[i];
        req.flags = GPIOHANDLE_REQUEST_OUTPUT;
        if (ioctl(chipFd, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0) {
            std::cerr << "GPIO_GET_LINEHANDLE_IOCTL: " << strerror(errno) << "\n";
            close(chipFd);
            return false;
        }
        lineFd = req.fd;
        return true;
    }

    void setAll(bool l1, bool l2, bool r1, bool r2) {
        data.values[0] = l1;
        data.values[1] = l2;
        data.values[2] = r1;
        data.values[3] = r2;
        if (ioctl(lineFd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) < 0) {
            std::cerr << "GPIO_SET_LINE_VALUES: " << strerror(errno) << "\n";
        }
    }

    void brake() { setAll(false,false,false,false); }

    ~MotorDriver() {
        if (lineFd >= 0) close(lineFd);
        if (chipFd >= 0) close(chipFd);
    }
};

// Fire a 10 ms burst on the motors, then brake
void burstMove(MotorDriver &drv, bool l1, bool l2, bool r1, bool r2) {
    drv.setAll(l1, l2, r1, r2);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    drv.brake();
}

// 2D point + movement enums + decision function (Service 3)
struct Point2D { int x, y; };
enum Direction {
    STOP,
    FORWARD,
    LEFT,
    RIGHT,
    TURN_LEFT_THEN_FORWARD,
    TURN_RIGHT_THEN_FORWARD
};
struct MovementCommand { Direction dir; int speed_level; };

MovementCommand service3_decide_direction(Point2D pos) {
    MovementCommand cmd;
    bool isLeft   = pos.x < 213;
    bool isCenter = pos.x >= 213 && pos.x <= 426;
    bool isRight  = pos.x > 426;
    bool isTop    = pos.y < 160;
    bool isBottom = pos.y > 320;

    if (isLeft  && isTop)    cmd.dir = TURN_LEFT_THEN_FORWARD;
    else if (isRight && isTop)    cmd.dir = TURN_RIGHT_THEN_FORWARD;
    else if (isLeft && isBottom)  cmd.dir = LEFT;
    else if (isRight && isBottom) cmd.dir = RIGHT;
    else if (isCenter && isTop)   cmd.dir = FORWARD;
    else                           cmd.dir = STOP;

    if (pos.y < 160)          cmd.speed_level = 3;
    else if (pos.y <= 320)    cmd.speed_level = 2;
    else                      cmd.speed_level = 1;

    return cmd;
}

std::atomic<bool> quitFlag{false};
void onSig(int){ quitFlag = true; }

int main(){
    std::signal(SIGINT, onSig);

    MotorDriver drv;
    if (!drv.init()) return 1;
    drv.brake();

    cv::VideoCapture cap(0, cv::CAP_V4L2);
    if (!cap.isOpened()) {
        std::cerr << "Cannot open camera\n"; 
        return -1;
    }
    cap.set(cv::CAP_PROP_FRAME_WIDTH,  640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    cap.set(cv::CAP_PROP_FPS,          30);

    cv::Mat frame, hsv, mask;
    while (!quitFlag) {
        cap >> frame;
        if (frame.empty()) break;

        // detect red dot
        cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
        cv::Mat m1, m2;
cv::inRange(hsv, cv::Scalar(0, 50, 50), cv::Scalar(10, 255, 255), m1);
cv::inRange(hsv, cv::Scalar(160, 50, 50), cv::Scalar(180, 255, 255), m2);
        mask = m1 | m2;
        cv::erode(mask, mask, {}, {}, 2);
        cv::dilate(mask, mask, {}, {}, 2);

        // find largest contour
        std::vector<std::vector<cv::Point>> ctrs;
        cv::findContours(mask, ctrs, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        if (!ctrs.empty()) {
            double bestA=0; int bestI=-1;
            for (int i=0;i<(int)ctrs.size();++i){
                double A=cv::contourArea(ctrs[i]);
                if (A>bestA){bestA=A; bestI=i;}
            }
            if (bestI>=0 && bestA>5.0) {
                auto M = cv::moments(ctrs[bestI]);
                Point2D P{ int(M.m10/M.m00), int(M.m01/M.m00) };

                auto cmd = service3_decide_direction(P);

                // **10 ms burst motor action here**
                switch(cmd.dir){
                  case FORWARD:
                  std::cout << "Forward\n";
                    burstMove(drv, true,false, true,false);
                    break;
                  case LEFT:
                  std::cout << "Left\n";
                    burstMove(drv, false,false, true,false);
                    break;
                  case RIGHT:
                  std::cout << "right\n";
                    burstMove(drv, true,false, false,false);
                    break;
                  case TURN_LEFT_THEN_FORWARD:
                  std::cout << "Left and Forward\n";
                    burstMove(drv, false,false, true,false);
                    burstMove(drv, true,false, true,false);
                    break;
                  case TURN_RIGHT_THEN_FORWARD:
                  std::cout << "Right and Forward\n";
                    burstMove(drv, true,false, false,false);
                    burstMove(drv, true,false, true,false);
                    break;
                  case STOP:
                  default:
                    drv.brake();
                    break;
                }

                cv::circle(frame, {P.x,P.y}, 10, {0,255,0}, 2);
                std::cout << "Dir=" << cmd.dir
                          << "  Pos=("<<P.x<<","<<P.y<<")\n";
            }
        } else {
            drv.brake();
        }

        cv::imshow("Frame", frame);
        cv::imshow("Mask", mask);
        if (cv::waitKey(1)==27) break;
    }

    drv.brake();
    return 0;
}
