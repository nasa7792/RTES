/*

 * 3b.cpp - Contains implementation for service and scheduler classes

 *

 * Original Authour-Steve Rizor 3/16/2025

 * Modified: Nalin Saxena/Bella Wolf

 *

 * Refrence and acknowledgement- For developing the code for this excerise we utilized chatgpt for getting the correct

 * syntax. Code related to timer_service is leveraged using chatgpt

 *

 * The suggestion for using unique_ptr for storing the services was provided by the insructor -Steve Rizor

 */

#include <cstdint>

#include <cstdio>

#include "3b.hpp"

#include <csignal>

volatile sig_atomic_t stop_flag = 0;

void signalHandler(int signum)

{

    if (signum == SIGINT)

    {

        syslog(LOG_INFO, "CAUGHT SIGNAL!");

        stop_flag = 1;
    }
}

void service2()

{

    std::puts("this is service 2 implemented as a function\n");
}

int main()

{

    std::signal(SIGINT, signalHandler);

    // Example use of the sequencer/service classes:

    Sequencer sequencer{};

    openlog("LOG_MSG", LOG_PID | LOG_PERROR, LOG_USER);

    syslog(LOG_INFO, "SYSLOG TEST MESSAGE!");

    sequencer.addService([]()

                         { std::puts("this is service 1 implemented in a lambda expression\n"); }, 1, 99, 5);

    sequencer.addService(service2, 1, 98, 10);

    sequencer.startServices();

    // todo: wait for ctrl-c or some other terminating condition

    while (!stop_flag)

    {

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    sequencer.stopServices();

    syslog(LOG_INFO, "Services stopped, exiting...");

    closelog();
}
