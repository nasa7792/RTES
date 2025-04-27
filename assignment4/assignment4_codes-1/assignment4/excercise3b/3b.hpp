/*
 * 3b.hpp - Contains implementation for service and scheduler classes
 * 
 * Original Authour-Steve Rizor 3/16/2025
 * Modified: Nalin Saxena/Bella Wolf
 * 
 * Refrence and acknowledgement- For developing the code for this excerise we utilized chatgpt for getting the correct 
 * syntax. Code related to timer_service is leveraged using chatgpt
 *
 * The suggestion for using unique_ptr for storing the services was provided by the insructor -Steve Rizor
 */

#pragma once


#include <cstdint>
#include <functional>
#include <thread>
#include<iostream>
#include <vector>
#include <syslog.h>


// The service class contains the service function and service parameters
// (priority, affinity, etc). It spawns a thread to run the service, configures
// the thread as required, and executes the service whenever it gets released.
class Service
{
public:
   template <typename T>
   Service(T &&doService, uint8_t affinity, uint8_t priority, uint32_t period) : _doService(doService),
                                                                                 _affinity(affinity),
                                                                                 _priority(priority),
                                                                                 _period(period),
                                                                                 _semaphore(0)
   {
       // todo: store service configuration values
       // todo: initialize release semaphore


       // Start the service thread, which will begin running the given function immediately
       syslog(LOG_INFO, "constructor called\n");
       syslog(LOG_INFO, "affinity %d \n", static_cast<int>(_affinity));
       syslog(LOG_INFO, "priority  %d \n", static_cast<int>(_priority));
       syslog(LOG_INFO, "period %d   \n", static_cast<int>(_period));
       _service = std::jthread(&Service::_provideService, this);
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


   int get_period()
   {
       return _period;
   }


private:
   std::function<void(void)> _doService;
   std::jthread _service;
   int _affinity;
   int _priority;
   int _period;
   std::binary_semaphore _semaphore;
   std::atomic<bool> _running{true}; // Atomic boolean initialized to true


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
       int policy = SCHED_FIFO; // FIFO policy (use SCHED_OTHER for normal priority)
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
       // todo: call _doService() on releases (sem acquire) while the atomic running variable is
       while (_running)
       {
           // try to acquire sem
           _semaphore.acquire();
           _doService();
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
       // todo: start timer(s), release services
       _seq_running = true;
       timer_thread = std::jthread(&Sequencer::timer_service, this);
   }


   void stopServices()
   {
       // todo: stop timer(s), stop services
       _seq_running = false;
       for (auto &service : _services)
       {
           service->stop();
       }
   }


private:
   std::vector<std::unique_ptr<Service>> _services; // Store services as unique_ptrs
   std::atomic<bool> _seq_running{false};
   std::jthread timer_thread;
//below Section of code written with chatgpt
   void timer_service()
   {
       using Clock = std::chrono::steady_clock;
       auto start_time = Clock::now();


       std::vector<uint32_t> service_periods;
       std::vector<std::chrono::milliseconds> next_release_times;


       // Initialize the next release times for each service
       for (const auto &service : _services)
       {
           service_periods.push_back(service->get_period());
           next_release_times.push_back(std::chrono::milliseconds(service->get_period()));
       }


       while (_seq_running)
       {
           auto now = Clock::now();
           auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);


           for (size_t i = 0; i < _services.size(); ++i)
           {
               if (elapsed_ms >= next_release_times[i])
               {
                   _services[i]->release();                                                // Release the service
                   next_release_times[i] += std::chrono::milliseconds(service_periods[i]); // Schedule next release
               }
           }


           std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Small sleep to avoid busy waiting
       }
   }
};
