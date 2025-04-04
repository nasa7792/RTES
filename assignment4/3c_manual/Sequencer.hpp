/*
 * Sequencer.hpp - Contains implementation for service and scheduler classes
 * 
 * Original Authour-Steve Rizor 3/16/2025
 * Modified: Nalin Saxena/Bella Wolf
 * 
 */


#pragma once

#include <cstdint>
#include <functional>
#include <thread>
#include <iostream>
#include <vector>
#include <syslog.h>
#include <mutex>
#include <csignal>

double global_start_time;
double getCurrentTimeInMs(void)
{
    struct timespec currentTime = {0, 0};

    // Get the current time from the CLOCK_MONOTONIC clock
    clock_gettime(CLOCK_MONOTONIC, &currentTime);

    // Convert the time to milliseconds (seconds * 1000 + nanoseconds / 1,000,000)
    return ((currentTime.tv_sec) * 1000.0) + ((currentTime.tv_nsec) / 1000000.0);
}

// The service class contains the service function and service parameters
// (priority, affinity, etc). It spawns a thread to run the service, configures
// the thread as required, and executes the service whenever it gets released.
class Service
{
public:
    template <typename T>
    //Constructor for Service class uses an initlization list for setting up the Service object
    Service(T &&doService, uint8_t affinity, uint8_t priority, uint32_t period, int service_identifier) : _doService(doService),
                                                                                                          _affinity(affinity),
                                                                                                          _priority(priority),
                                                                                                          _period(period),
                                                                                                          _service_identifier(service_identifier),
                                                                                                          _semaphore(0)//start in blocked state

    {
        // todo: store service configuration values
        // todo: initialize release semaphore

        // Start the service thread, which will begin running the given function immediately
        syslog(LOG_INFO, "constructor called for Service %d\n", _service_identifier);
        syslog(LOG_INFO, "affinity %d \n", static_cast<int>(_affinity));
        syslog(LOG_INFO, "priority  %d \n", static_cast<int>(_priority));
        syslog(LOG_INFO, "period %d   \n", static_cast<int>(_period));
        _service = std::jthread(&Service::_provdeService, this); 
    }

    void stop()
    {
        // todo: change state to "not running" using an atomic variable
        // (heads up: what if the service is waiting on the semaphore when this happens?)
        _running = false;
        _semaphore.release();
    }

    void release()
    {
        // todo: release the service using the semaphore
        _semaphore.release();
    }
    //a getter to return private data
    int get_period()
    {
        return _period;
    }
    //a getter to return private data
    int get_id()
    {
        return _service_identifier;
    }
    //prints logs for each service after stop is called from sequencer
    void logStats()
    {
        auto avg_time = _total_time / _exec_count;
        auto avg_jitter = _total_exec_jitter / _exec_count; // Calculate the average jitter
        syslog(LOG_INFO, "Service Execution Stats: For %d", _service_identifier);
        syslog(LOG_INFO, "_exec_count : %d", _exec_count);
        syslog(LOG_INFO, "  Min Execution Time: %ld ms", _min_time.count());
        syslog(LOG_INFO, "  Max Execution Time: %ld ms", _max_time.count());
        syslog(LOG_INFO, "  Avg Execution Time: %ld ms", avg_time.count());
        syslog(LOG_INFO, "  Avg Jitter: %ld ms", avg_jitter.count()); // Log the average jitter
    }

private:
    std::function<void(void)> _doService;
    std::jthread _service;
    int _affinity;//core id the thread should be bound to
    int _priority;
    int _period;
    int _service_identifier;//a unique identifier for each service
    std::binary_semaphore _semaphore;
    std::atomic<bool> _running{true};
    //logging data members
    std::chrono::milliseconds _min_time{std::numeric_limits<long>::max()};
    std::chrono::milliseconds _max_time{0};
    std::chrono::milliseconds _total_time{0};
    unsigned int _exec_count = 0;
    std::chrono::milliseconds _total_exec_jitter{0};

    //setting up the thread priority,affinity and scheduling policy to SCHED_FIFO
    void _initializeService()
    {
        // todo: set affinity, priority, sched policy
        // (heads up: the thread is already running and we're in its context right now)
        syslog(LOG_INFO, "Initializing service...");
        pthread_t threadID = pthread_self(); // Get current thread ID
        cpu_set_t cpuset;
        struct sched_param param;
        CPU_ZERO(&cpuset);
        CPU_SET(_affinity, &cpuset);
        if (pthread_setaffinity_np(threadID, sizeof(cpu_set_t), &cpuset) != 0)
        {
            syslog(LOG_ERR, "Failed to set CPU affinity for thread.");
        }
        int policy = SCHED_FIFO; // sudo access
        param.sched_priority = _priority;

        if (pthread_setschedparam(threadID, policy, &param) != 0)
        {
            syslog(LOG_ERR, "Failed to set scheduling parameters.");
        }

        syslog(LOG_INFO, "Service initialized with affinity %d and priority %d", _affinity, _priority);
    }
    void _provideService()
    {
        _initializeService();
        while (_running)
        {
            _semaphore.acquire();
            if (!_running)
                break;

            auto start_time = std::chrono::steady_clock::now();

            auto start_time_ms = getCurrentTimeInMs() - global_start_time;
            int cpu = sched_getcpu();
            syslog(LOG_INFO, "Service %d started execution at %.2f ms core %d", _service_identifier, start_time_ms, cpu);
            //call the service function
            _doService();

            auto end_time = std::chrono::steady_clock::now();
            auto exec_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

            auto end_time_ms = getCurrentTimeInMs() - global_start_time;
            syslog(LOG_INFO, "Service %d finished execution at %.2f ms core %d", _service_identifier, end_time_ms, cpu);

            std::chrono::milliseconds jitter(0);
            if (_service_identifier == 1)
            {
                jitter = exec_time - std::chrono::milliseconds(10);
            }
            else if (_service_identifier == 2)
            {
                jitter = exec_time - std::chrono::milliseconds(30);
            }
            // execution stats
            _exec_count++; 
            _total_time += exec_time;
            _total_exec_jitter += abs(jitter);
            _min_time = std::min(_min_time, exec_time);
            _max_time = std::max(_max_time, exec_time);
        }
    }
};

// The sequencer class contains the services set and manages
// starting/stopping the services. While the services are running,
// the sequencer releases each service at the requisite timepoint.
class Sequencer
{
public:
    template <typename... Args>
    void addService(Args &&...args)
    {
        // Add the new service to the services list,
        // constructing it in-place with the given args
        _services.emplace_back(std::make_unique<Service>(std::forward<Args>(args)...));
    }

    void startServices()
    {
        _seq_running = true;
        //manually trigger threads
        _trigger_thread = std::jthread(&Sequencer::manualServiceTriggering, this);

        // Set thread scheduling policy and priority (SCHED_FIFO with priority 99 scheduler thread should run at max prio)
        setThreadRealtimePriority(_trigger_thread);
    }

    void stopServices()
    {
        _seq_running = false;
        for (auto &service : _services)
        {
            service->stop();
            service->logStats(); //display stats
        }
    }

private:
    void manualServiceTriggering()
    {
        if (!_seq_running)
            return;

        // manually release services
        global_start_time = getCurrentTimeInMs(); //at this instant be record our inital time
        while (_seq_running)
        {
            syslog(LOG_INFO, "CI INSTANT! %f \n", getCurrentTimeInMs() - global_start_time);
            auto &service = _services;
            service[0]->release();
            service[1]->release();

            usleep(slep20Ms);
            service[0]->release();
            syslog(LOG_INFO, "sequencer! %f \n", getCurrentTimeInMs() - global_start_time);

            usleep(slep20Ms);
            service[0]->release();
            syslog(LOG_INFO, "sequencer! %f \n", getCurrentTimeInMs() - global_start_time);

            usleep(slep10MS);
            service[1]->release();
            syslog(LOG_INFO, "sequencer! %f \n", getCurrentTimeInMs() - global_start_time);

            usleep(slep10MS);
            service[0]->release();
            syslog(LOG_INFO, "sequencer! %f \n", getCurrentTimeInMs() - global_start_time);

            usleep(slep20Ms);
            service[0]->release();
            syslog(LOG_INFO, "sequencer! %f \n", getCurrentTimeInMs() - global_start_time);
            usleep(slep20Ms);
        }
    }
    //setup up sequencer as RT thread
    void setThreadRealtimePriority(std::jthread &thread)
    {
        std::thread::native_handle_type handle = thread.native_handle();

        // Set scheduling policy to FIFO and priority to 99
        struct sched_param sched_param;
        sched_param.sched_priority = 99;
        if (pthread_setschedparam(handle, SCHED_FIFO, &sched_param) != 0)
        {
            std::cerr << "Failed to set real-time scheduling policy" << std::endl;
        }
        // Set the CPU affinity to core 0
        cpu_set_t cpu_set;
        CPU_ZERO(&cpu_set);
        CPU_SET(0, &cpu_set); // Pin the thread to core 0
        if (pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpu_set) != 0)
        {
            std::cerr << "Failed to set CPU affinity" << std::endl;
        }
    }
    std::vector<std::unique_ptr<Service>> _services;
    std::atomic<bool> _seq_running{false};

    std::jthread _trigger_thread; // manual triggering
    const int slep10MS = 10000;   // 10 milliseconds sleep
    const int slep20Ms = 20000;   // 20 milliseconds sleep
};