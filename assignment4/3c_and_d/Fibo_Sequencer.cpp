 /*
 * Fibo_Sequencer.cpp - This is a C++ version of the canonical pthread service example. It intends
 * to abstract the service management functionality and sequencing for ease
 * of use. Much of the code is left to be implemented by the student
 * 
 * 
 * Original Authour-Steve Rizor 3/16/2025
 * Modified: Nalin Saxena/Bella Wolf
 */
#include <cstdint>
#include <cstdio>
#include "Fibo_Sequencer.hpp"
#include <csignal> // For signal handling
#define FIB_LIMIT_FOR_32_BIT 47
#define FIB_TEST_CYCLES 1800
#define NSEC_PER_SEC (1000000000)
#define PRE_TEST 200
#define TEST_DURATION_MS 1000
uint32_t seqIterations = FIB_LIMIT_FOR_32_BIT;
uint32_t idx = 0, jdx = 1;
uint32_t fib = 0, fib0 = 0, fib1 = 1;
uint32_t fib10Cnt = 0, fib20Cnt = 0;

volatile int ten_ms_count = 0;
volatile int twenty_ms_count = 0;

/*following macro taken as is from vx works example
 * */
#define FIB_TEST(seqCnt, iterCnt)       \
    for (idx = 0; idx < iterCnt; idx++) \
    {                                   \
        fib = fib0 + fib1;              \
        while (jdx < seqCnt)            \
        {                               \
            fib0 = fib1;                \
            fib1 = fib;                 \
            fib = fib0 + fib1;          \
            jdx++;                      \
        }                               \
    }

/*below code taken as is from : Sam siewert posix clcock example
 * */
int delta_t(struct timespec *stop, struct timespec *start, struct timespec *delta_t)
{
    int dt_sec = stop->tv_sec - start->tv_sec;
    int dt_nsec = stop->tv_nsec - start->tv_nsec;

    if (dt_sec >= 0)
    {
        if (dt_nsec >= 0)
        {
            delta_t->tv_sec = dt_sec;
            delta_t->tv_nsec = dt_nsec;
        }
        else
        {
            delta_t->tv_sec = dt_sec - 1;
            delta_t->tv_nsec = NSEC_PER_SEC + dt_nsec;
        }
    }
    else
    {
        if (dt_nsec >= 0)
        {
            delta_t->tv_sec = dt_sec;
            delta_t->tv_nsec = dt_nsec;
        }
        else
        {
            delta_t->tv_sec = dt_sec - 1;
            delta_t->tv_nsec = NSEC_PER_SEC + dt_nsec;
        }
    }

    return (1);
}

/*Fib 10 service to generate 10 ms of workload
 *
 * Parameters:
 *            None
 *
 * Returns:
 *          None
 *
 */
void fib10()
{

    // Warm-up computation
    FIB_TEST(seqIterations, FIB_TEST_CYCLES);

    // Simulate workload
    for (int i = 0; i < ten_ms_count; i++)
    {
        FIB_TEST(seqIterations, FIB_TEST_CYCLES);
    }
}

/*Fib 20 service to generate 20 ms of workload
 *
 * Parameters:
 *            None
 *
 * Returns:
 *          None
 *
 */
void fib20()
{

    // Warm-up computation
    FIB_TEST(seqIterations, FIB_TEST_CYCLES);

    // Simulate workload
    for (int i = 0; i < twenty_ms_count; i++)
    {
        FIB_TEST(seqIterations, FIB_TEST_CYCLES);
    }
}

/*A function to get iterations required to run
 *for delay values
 * Parameters:
 *            None
 *
 * Returns:
 *          None
 *
 */
void run_iteration_test()
{
    int accum_iter_fib10 = 0;
    int accum_iter_fib20 = 0;
    // Warm-up computation
    FIB_TEST(seqIterations, FIB_TEST_CYCLES);

    for (int i = 0; i < PRE_TEST; i++)
    {
        struct timespec start_time, finish_time, thread_dt;

        clock_gettime(CLOCK_REALTIME, &start_time); // Get start time
        FIB_TEST(seqIterations, FIB_TEST_CYCLES);
        clock_gettime(CLOCK_REALTIME, &finish_time); // Get end time

        delta_t(&finish_time, &start_time, &thread_dt);
        double run_time_calc = (thread_dt.tv_sec * 1000.0) + (thread_dt.tv_nsec / 1000000.0);
        int req_iterations_fib10 = (int)(10 / run_time_calc);
        int req_iterations_fib20 = (int)(18.5 / run_time_calc);//keeping this as 20 overshoots the service2 deadline
        accum_iter_fib10 += req_iterations_fib10;
        accum_iter_fib20 += req_iterations_fib20;
    }
    ten_ms_count = (int)accum_iter_fib10 / PRE_TEST;
    twenty_ms_count = (int)accum_iter_fib20 / PRE_TEST;
    syslog(LOG_INFO, "RUNNING fib iteration test! %d %d", ten_ms_count, twenty_ms_count);
}

int main()
{
    openlog("LOG_MSG", LOG_PID | LOG_PERROR, LOG_USER);
    run_iteration_test();
    // Example use of the sequencer/service classes:
    Sequencer sequencer{};
    sequencer.addService(fib10, 0, 98, 20, 1);
    sequencer.addService(fib20, 0, 97, 50, 2);
    usleep(10000); // a sleep for the initliazation to complete 
    sequencer.startServices();
    std::this_thread::sleep_for(std::chrono::milliseconds(TEST_DURATION_MS));
    sequencer.stopServices();
    syslog(LOG_INFO, "Services stopped, exiting...");
    closelog();
}