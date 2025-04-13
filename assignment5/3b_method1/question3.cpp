/*
 * Authour- Nalin Saxena
 *
 * This file contains code written for ECEN 5623 ex-5 question 4
 *
 * Various methods for accessing gpios are explored and compared.
 */
#include <cstdint>
#include <cstdio>
#include "question3.hpp"
#include <csignal> // For signal handling
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <linux/gpio.h>
#include <sys/ioctl.h>
#include <cstring>
#include <sys/mman.h>
#include <sys/stat.h>
#define GPIO_BASE 512
#define GPIO_TEST 18      // output pin
#define GPIO_INPUT_PIN 23 // input pin
#define GPLEV0_INDEX 13   // offset of level register
#define DISABLE_AUTO_RELEASE -1
#define ON 1
#define OFF 0
#define GPIO_BASE_ADDRESS 0xFE200000
#define BLOCK_SIZE (4 * 1024)
#define GPIOSET0_INDEX 7
#define GPIOCLR0_INDEX 10

std::atomic<bool> main_running{true}; // Global flag to track termination
volatile bool current_status = false;
volatile bool last_input_state = false; // Track the last state of input pin
volatile unsigned int *gpio = NULL;

void signalHandler(int signum)
{
    syslog(LOG_INFO, "Received signal %d, stopping services...", signum);
    main_running = false;
}

// method-a
void toggle_led_pinctrl()
{
    std::string command;

    if (current_status)
    {
        syslog(LOG_INFO, "Turning led on! \n");
        command = "pinctrl " + std::to_string(GPIO_TEST) + " op dl"; // high output
    }
    else
    {
        syslog(LOG_INFO, "Turning led off! \n");
        command = "pinctrl " + std::to_string(GPIO_TEST) + " op dh"; // low output
    }
    current_status = !current_status;
    system(command.c_str());
}

// method-b some refrence for sysfs interface taken from chat gpt
void toggle_led_sysfs()
{

    int output_pin = GPIO_BASE + GPIO_TEST; // 512 + 17 = 529
    std::string output_pin_base = "/sys/class/gpio/gpio" + std::to_string(output_pin);
    std::string op_dir = output_pin_base + "/direction";
    std::string value_path = output_pin_base + "/value";
    struct stat status_buffer;

    if (stat(output_pin_base.c_str(), &status_buffer) != 0)
    {
        // export sysfs file if not already exported
        std::ofstream exportFile("/sys/class/gpio/export");
        exportFile << output_pin;
        exportFile.close();
        // Set pin direction to ouput
        std::ofstream output_direction(op_dir);
        output_direction << "out"; // output pin
        output_direction.close();
    }

    std::ofstream pin_value(value_path);
    pin_value << (current_status ? std::to_string(ON) : std::to_string(OFF));
    pin_value.close();
    current_status = !current_status;
}

// method-c ioctl
// code refrence https://blog.lxsang.me/post/id/33
void ioctl_toggle()
{
    struct gpiohandle_request rq{};
    struct gpiohandle_data data{};

    int fd = open("/dev/gpiochip0", O_RDWR);
    if (fd < 0)
    {
        syslog(LOG_ERR, "Unabel to open file");
        return;
    }
    rq.lineoffsets[0] = GPIO_TEST;
    rq.lines = 1;
    rq.flags = GPIOHANDLE_REQUEST_OUTPUT;
    if (ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, &rq) < 0)
    {
        perror("GPIO_GET_LINEHANDLE_IOCTL failed");
        close(fd);
        return;
    }
    data.values[0] = current_status ? ON : OFF;
    if (ioctl(rq.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) < 0)
    {
        perror("GPIOHANDLE_SET_LINE_VALUES_IOCTL failed");
    }
    current_status = !current_status;
    close(rq.fd);
    close(fd);
}

void setup_memory_mapped_gpio()
{
    // source and refrence - https://www.codeembedded.com/blog/raspberry_pi_gpio/
    int mem_fd;
    void *gpio_map;
    if ((mem_fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0)
    {
        syslog(LOG_ERR, "unable to open /dev/mem try running as sudo");
        exit(-1);
    }
    gpio_map = mmap(
        NULL,
        BLOCK_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        mem_fd,
        GPIO_BASE_ADDRESS);

    close(mem_fd);

    if (gpio_map == MAP_FAILED)
    {
        perror("mmap error");
        exit(EXIT_FAILURE);
    }

    gpio = (volatile unsigned int *)gpio_map;

    // assign pin as output

    int reg_index = GPIO_TEST / 10;
    int bit_index_start = (GPIO_TEST % 10) * 3;
    // clear test pin's bits
    gpio[reg_index] = gpio[reg_index] & ~(7 << bit_index_start);
    // make 001
    gpio[reg_index] = gpio[reg_index] | (1 << bit_index_start);

    // make input read (extra credit)
    reg_index = GPIO_INPUT_PIN % 10;
    bit_index_start = (GPIO_INPUT_PIN % 10) * 3;
    gpio[reg_index] = gpio[reg_index] & ~(7 << bit_index_start); // Clear bits
    gpio[reg_index] = gpio[reg_index] | (0 << bit_index_start);  // Set as input
}

void toggle_mmap()
{
    if (current_status)
    {
        gpio[GPIOSET0_INDEX] = (1 << GPIO_TEST);
    }
    else
    {
        gpio[GPIOCLR0_INDEX] = (1 << GPIO_TEST);
    }
    current_status = !current_status;
}

// extra credit question 6
bool read_input_pin()
{
    int level_reg_index = GPLEV0_INDEX + (GPIO_INPUT_PIN / 32);
    int bit_index = GPIO_INPUT_PIN % 32;
    // chec level of the pin
    return (gpio[level_reg_index] & (1 << bit_index)) != 0;
}

// extra credit question 6
void poll_input_and_toggle_output()
{
    bool current_input_state = read_input_pin();
    syslog(LOG_INFO, "checking state CHANGED !%d", current_input_state);
    // check for state change
    if (current_input_state != last_input_state)
    {
        syslog(LOG_INFO, "STATE CHANGED !");
        last_input_state = current_input_state;
        toggle_mmap();
    }
}

int main(int argc, char *argv[])
{
    struct sigaction action{};
    action.sa_handler = signalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    sigaction(SIGINT, &action, nullptr);  // Handle Ctrl+C
    sigaction(SIGTSTP, &action, nullptr); // Handle Ctrl

    Sequencer sequencer{};
    openlog("LOG_MSG", LOG_PID | LOG_PERROR, LOG_USER);
    void (*toggle_method)() = toggle_led_pinctrl;
    ;               // a function pointer to hold method
    int method = 1; // use pinctrl as default method
    if (argc == 2)
    {
        method = std::atoi(argv[1]);
        syslog(LOG_INFO, "Using method: %d", method);
        switch (method)
        {
        case 1:
            toggle_method = toggle_led_pinctrl;
            break;
        case 2:
            toggle_method = toggle_led_sysfs;
            break;
        case 3:
            toggle_method = ioctl_toggle;
            break;
        case 4:
            setup_memory_mapped_gpio();
            toggle_method = toggle_mmap;
            break;
        case 5:
            setup_memory_mapped_gpio();
            toggle_method = poll_input_and_toggle_output;
            break;
        case 6:
            setup_memory_mapped_gpio();
            toggle_method = poll_input_and_toggle_output;
            break;
        default:
            syslog(LOG_WARNING, "Unknown method: %d. Using default pinctrl method.", method);
            break;
        }
    }
    else
    {
        syslog(LOG_WARNING, "Usage enter method 1-5 \n");
    }
    if (method != 6)
        sequencer.addService(toggle_method, 1, 98, 1000, 1, method);
    else
        sequencer.addService(toggle_method, 2, 98, DISABLE_AUTO_RELEASE, 1, method);
    sequencer.startServices();
    // todo: wait for ctrl-c or some other terminating condition
    while (main_running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    sequencer.stopServices();
    syslog(LOG_INFO, "Services stopped, exiting...");
    closelog();
}
