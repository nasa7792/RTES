// RobotMain_ioctl.cpp

// Cyclic executive driving a twoâ€‘wheel robot in a rectangle using ioctl GPIO.

// Build with: g++ --std=c++23 -Wall -Werror -pedantic RobotMain_ioctl.cpp -o robot_exec_ioctl

// Run with sudo: sudo ./robot_exec_ioctl



#include <chrono>

#include <csignal>

#include <iostream>

#include <thread>

#include <atomic>

#include <fcntl.h>

#include <unistd.h>

#include <sys/ioctl.h>

#include <linux/gpio.h>

#include <cstring>



// BCM pin numbers for the four L298N inputs

static constexpr unsigned L_IN1 = 17;  // left forward

static constexpr unsigned L_IN2 = 27;  // left backward

static constexpr unsigned R_IN1 = 22;  // right forward

static constexpr unsigned R_IN2 = 23;  // right backward



std::atomic<bool> quitFlag{false};

void onSig(int){ quitFlag = true; }



// Helper struct to keep our handle and data

struct MotorDriver {

    int chipFd = -1;

    int lineFd = -1;

    gpiohandle_data data{};   // holds the 4 output bits

    // offsets order: L_IN1, L_IN2, R_IN1, R_IN2

    unsigned offsets[4] = { L_IN1, L_IN2, R_IN1, R_IN2 };



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



    void closeAll() {

        if (lineFd >= 0)  close(lineFd);

        if (chipFd >= 0)  close(chipFd);

    }

};



int main(){

    std::signal(SIGINT, onSig);

    MotorDriver drv;

    if (!drv.init()) return 1;



    std::cout << "Starting rectangle (ioctl) drive. Ctrl+C to stop.\n";



    enum Phase { F1, T1, F2, T2, DONE } phase = F1;

    const int forward_ms = 1000, turn_ms = 500;

    auto phaseStart = std::chrono::steady_clock::now();

    auto nextTick   = phaseStart;

    const auto tick = std::chrono::milliseconds(10);



    while (!quitFlag && phase != DONE) {

        nextTick += tick;

        std::this_thread::sleep_until(nextTick);



        auto now = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - phaseStart).count();



        switch (phase) {

            case F1:

                drv.setAll(true, false, true, false);  // forward

                if (elapsed >= forward_ms) {

                    phase = T1; phaseStart = now;

                    std::cout << "Phase â†’ Turn 1\n";

                }

                break;

            case T1:

                drv.setAll(true, false, false, false);  // pivot right

                if (elapsed >= turn_ms) {

                    phase = F2; phaseStart = now;

                    std::cout << "Phase â†’ Fwd 2\n";

                }

                break;

            case F2:

                drv.setAll(true, false, true, false);

                if (elapsed >= forward_ms) {

                    phase = T2; phaseStart = now;

                    std::cout << "Phase â†’ Turn 2\n";

                }

                break;

            case T2:

                drv.setAll(true, false, false, false);

                if (elapsed >= turn_ms) {

                    phase = F1;

                    std::cout << "Rectangle done\n";

                }

                break;

            default: break;

        }

    }



    // brake

    drv.setAll(false,false,false,false);

    drv.closeAll();

    std::cout << "Exiting cleanly.\n";

    return 0;

}

