/*
 * This is the main file for the sequencer project.
 * It demonstrates non-blocking service management by scheduling
 * two different services that toggle an external LED connected to
 * GPIO BCM 17 every 100ms.
 *
 * Build with: g++ --std=c++23 -Wall -Werror -pedantic Sequencer.cpp -o sequencer_app
 * Authors: bhavya and Lokesh
 * Date: 4/2/2025 (Modified)
 */

#include "Sequencer.hpp"
#include <csignal>
#include <chrono>
#include <thread>
#include <atomic>
#include <cstdio>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>
#include <sys/mman.h>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <cstdlib>    // for system()

#include <fstream>    // Method 2 file I/O


#include <unistd.h>   // close, read, write


// Global atomic flag for terminating the program.
std::atomic<bool> quitFlag(false);

// Signal handler: sets quitFlag to true when SIGINT is caught.
void signalHandler(int signum) {
    quitFlag = true;
}

// 2) Method 1: Toggle GPIO17 using shell commands with sudo tee

// --------------------------------------------------------------------------

void gpio17ShellToggleService()

{

    static bool initialized = false;

    static bool state = false;  // false => LOW, true => HIGH



    if (!initialized)

    {

        std::cout << "[gpio17ShellToggleService] Initializing GPIO17 via shell commands...\n";

        // Use sudo tee so that output redirection works even when writing to restricted files.

        system("echo 529 | sudo tee /sys/class/gpio/export > /dev/null");

        system("echo out | sudo tee /sys/class/gpio/gpio529/direction > /dev/null");

        initialized = true;

    }



    // Toggle the value: write "1" for HIGH, "0" for LOW.

    if (state)

    {

        system("echo 1 | sudo tee /sys/class/gpio/gpio529/value > /dev/null");

    }

    else

    {

        system("echo 0 | sudo tee /sys/class/gpio/gpio529/value > /dev/null");

    }



    std::cout << "[gpio17ShellToggleService] Toggled GPIO17 to " << (state ? "HIGH" : "LOW") << "\n";

    state = !state;

}

// 3) Method 2: Toggle GPIO17 using direct file I/O (std::ofstream)

// --------------------------------------------------------------------------

void gpio17SysfsToggleService()

{

    static bool initialized = false;

    static bool state = false;  // false => LOW, true => HIGH



    if (!initialized)

    {

        // Export GPIO17.

        std::ofstream exportFile("/sys/class/gpio/export");

        if (exportFile) {

            exportFile << "529";

            exportFile.close();

        } else {

            std::cerr << "[gpio17SysfsToggleService] Error: Cannot open /sys/class/gpio/export\n";

        }



        // Set GPIO17 as output.

        std::ofstream directionFile("/sys/class/gpio/gpio529/direction");

        if (directionFile) {

            directionFile << "out";

            directionFile.close();

        } else {

            std::cerr << "[gpio17SysfsToggleService] Error: Cannot open /sys/class/gpio/gpio17/direction\n";

        }



        std::cout << "[gpio17SysfsToggleService] Initialized GPIO17 for output.\n";

        initialized = true;

    }



    // Toggle the value by writing to the value file.

    std::ofstream valueFile("/sys/class/gpio/gpio529/value");

    if (valueFile) {

        valueFile << (state ? "1" : "0");

        valueFile.close();

        std::cout << "[gpio17SysfsToggleService] Toggled GPIO17 to " << (state ? "HIGH" : "LOW") << "\n";

    } else {

        std::cerr << "[gpio17SysfsToggleService] Error: Cannot open /sys/class/gpio/gpio17/value\n";

    }

    state=!state;

}

/* 
 * Method 3: Service using ioctl interface.
 * This service toggles GPIO BCM 17 using the character device API.
 */
void gpio_toggle_ioctl() {
    static bool state = false;
    static int devFd = -1;
    static int lineFd = -1;
    static bool initialized = false;
    const char* devName = "/dev/gpiochip0";
    const unsigned int offset = 17; // Using GPIO BCM 17

    if (!initialized) {
        devFd = open(devName, O_RDONLY);
        if (devFd < 0) {
            std::cerr << "Failed to open " << devName << ": " << strerror(errno) << "\n";
            return;
        }
        struct gpiohandle_request req;
        memset(&req, 0, sizeof(req));
        req.lineoffsets[0] = offset;
        req.flags = GPIOHANDLE_REQUEST_OUTPUT;
        req.lines = 1;
        if (ioctl(devFd, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0) {
            std::cerr << "Failed to get line handle: " << strerror(errno) << "\n";
            close(devFd);
            return;
        }
        lineFd = req.fd;
        initialized = true;
    }

    struct gpiohandle_data data;
    data.values[0] = state ? 1 : 0;
    if (ioctl(lineFd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) < 0) {
        std::cerr << "Failed to set line value: " << strerror(errno) << "\n";
        return;
    }
    state = !state;
    std::cout << "Method 3 (ioctl): Toggled GPIO BCM 17 to " << (state ? "HIGH" : "LOW") << std::endl;
}

/* 
 * Method 4: Service using mmap.
 * This service toggles GPIO BCM 17 by mapping the GPIO registers directly.
 * For RPi 4B, the GPIO base is assumed at 0xFE200000.
 * Since GPIO 17 is in the first bank (0-31), we use GPSET0/GPCLR0.
 */
void gpio_toggle_mmap() {
    static bool state = false;
    static volatile uint32_t* gpio = nullptr;
    static bool initialized = false;
    // Raspberry Pi 4B GPIO base address:
    const off_t gpioBase = 0xFE200000;
    const size_t gpioLength = 4096; // Map one page (4096 bytes)
    const int gpioPin = 17; // External LED at BCM 17
    // For pins 0-31, offset is gpioPin itself.
    const int offsetInSet = gpioPin;
    // Register indexes for first bank (BCM2835 layout):
    const int GPSET0_INDEX = 7;
    const int GPCLR0_INDEX = 10;

    if (!initialized) {
        int memFd = open("/dev/mem", O_RDWR | O_SYNC);
        if (memFd < 0) {
            std::cerr << "Failed to open /dev/mem: " << strerror(errno) << "\n";
            return;
        }
        gpio = (volatile uint32_t*)mmap(nullptr, gpioLength, PROT_READ | PROT_WRITE, MAP_SHARED, memFd, gpioBase);
        if (gpio == MAP_FAILED) {
            std::cerr << "Failed to mmap: " << strerror(errno) << "\n";
            close(memFd);
            return;
        }
        close(memFd);
        initialized = true;
    }

    if (state) {
        gpio[GPCLR0_INDEX] = (1 << offsetInSet); // Clear the bit to turn off the LED.
    } else {
        gpio[GPSET0_INDEX] = (1 << offsetInSet);  // Set the bit to turn on the LED.
    }
    state = !state;
    std::cout << "Method 4 (mmap): Toggled GPIO BCM 17 to " << (state ? "HIGH" : "LOW") << std::endl;
}

int main(){
    // Set up the signal handler for SIGINT (Ctrl+C).
    std::signal(SIGINT, signalHandler);

    Sequencer sequencer;
    

    // Uncomment the method you want to use:
    //Method 1:
     //sequencer.addService(gpio17ShellToggleService, 1, 90, 100);

   // Method 2: Toggle using direct file I/O via std::ofstream

   //sequencer.addService(gpio17SysfsToggleService,1,91,100);
    
    // Method 3: Toggle GPIO via ioctl interface.
    sequencer.addService(gpio_toggle_ioctl, 1, 95, 100);
    
    // Method 4: Toggle GPIO via memory mapping.
    //sequencer.addService(gpio_toggle_mmap, 1, 95, 100);
    
    sequencer.startServices();

    std::puts("GPIO toggle service started (toggling every 100 ms on BCM 17). Press Ctrl+C to stop.");

    // Main thread waits until SIGINT is caught.
    while (!quitFlag) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::puts("Stopping services...");
    sequencer.stopServices();
    //system("echo 0 > /sys/class/gpio/gpio529/value");
    
    return 0;
}
