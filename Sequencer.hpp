/*

 * This is a C++ version of the canonical pthread service example. It intends

 * to abstract the service management functionality and sequencing for ease

 * of use. Much of the code is left to be implemented by the student.

 *

 * Build with g++ --std=c++23 -Wall -Werror -pedantic

 * Steve Rizor 3/16/2025

 * Student implementation by Lokesh & Bhavya Saravanan

 */

 #pragma once



 #include <cstdint>
 
 #include <functional>
 
 #include <thread>
 
 #include <vector>
 
 #include <semaphore>
 
 #include <atomic>
 
 #include <mutex>
 
 #include <chrono>
 
 #include <iostream>
 
 #include <limits>
 
 
 
 class Service
 
 {
 
 public:
 
     template<typename T>
 
     Service(T&& doService, uint8_t affinity, uint8_t priority, uint32_t period)
 
       : _doService(doService),
 
         _affinity(affinity),
 
         _priority(priority),
 
         _period(period),
 
         _releaseSemaphore(0), // Initialize release semaphore
 
         _runningFlag(true)
 
     {
 
         // Start the service thread, which will run _provideService()
 
         _service = std::jthread(&Service::_provideService, this);
 
     }
 
 
 
     void stop(){
 
         // Signal the thread to stop
 
         _runningFlag = false;
 
         // Release the semaphore in case the thread is waiting
 
         _releaseSemaphore.release();
 
     }
 
 
 
     void release(){
 
         // Record the release time for jitter calculations
 
         {
 
             std::lock_guard<std::mutex> lock(_statsMutex);
 
             _releaseTime = std::chrono::steady_clock::now();
 
         }
 
         // Release the service using the semaphore
 
         _releaseSemaphore.release();
 
     }
 
 
 
     uint32_t getPeriod() const {
 
         return _period;
 
     }
 
 
 
     // Print timing statistics (called after the service has stopped)
 
     void printStats() {
 
         std::lock_guard<std::mutex> lock(_statsMutex);
 
 
 
         // If no samples were collected, just report no data
 
         if (_countStartJitter == 0 || _countExecTime == 0) {
 
             std::cout << "Service Stats: No samples collected.\n";
 
             return;
 
         }
 
 
 
         double avgStartJitterUs = static_cast<double>(_sumStartJitterUs) / _countStartJitter;
 
         double avgExecTimeUs    = static_cast<double>(_sumExecTimeUs)    / _countExecTime;
 
 
 
         std::cout << "Service Stats:\n";
 
         std::cout << "  Start Jitter (us):"
 
                   << " min=" << _minStartJitterUs
 
                   << " max=" << _maxStartJitterUs
 
                   << " avg=" << avgStartJitterUs
 
                   << " (based on " << _countStartJitter << " samples)\n";
 
 
 
         std::cout << "  Execution Time (us):"
 
                   << " min=" << _minExecTimeUs
 
                   << " max=" << _maxExecTimeUs
 
                   << " avg=" << avgExecTimeUs
 
                   << " (based on " << _countExecTime << " samples)\n";
 
     }
 
 
 
 private:
 
     // User-supplied service function
 
     std::function<void(void)> _doService;
 
 
 
     // Thread and scheduling parameters
 
     std::jthread              _service;
 
     uint32_t                  _affinity;
 
     uint32_t                  _priority;
 
     uint32_t                  _period;
 
 
 
     // Synchronization
 
     std::binary_semaphore     _releaseSemaphore;
 
     std::atomic<bool>         _runningFlag;
 
 
 
     // Timing stats
 
     std::mutex _statsMutex;
 
     // Last release time
 
     std::chrono::steady_clock::time_point _releaseTime;
 
 
 
     // Start jitter stats (release->start)
 
     long long _minStartJitterUs = std::numeric_limits<long long>::max();
 
     long long _maxStartJitterUs = 0;
 
     long long _sumStartJitterUs = 0;
 
     size_t    _countStartJitter = 0;
 
 
 
     // Execution time stats (start->end)
 
     long long _minExecTimeUs = std::numeric_limits<long long>::max();
 
     long long _maxExecTimeUs = 0;
 
     long long _sumExecTimeUs = 0;
 
     size_t    _countExecTime = 0;
 
 
 
     // Called once by the thread on startup (affinity, priority, etc. could be set here)
 
     void _initializeService()
 
     {
 
         // Optional:To set thread affinity, priority, etc. (platform-specific)
 
     }
 
 
 
     // Main loop for the service thread
 
     void _provideService()
 
     {
 
         _initializeService();
 
         while (_runningFlag)
 
         {
 
             // Wait until the service is released
 
             _releaseSemaphore.acquire();
 
             if (!_runningFlag) {
 
                 break; // check again in case we were stopped
 
             }
 
 
 
             // Capture start time
 
             auto startTime = std::chrono::steady_clock::now();
 
             std::chrono::steady_clock::time_point localReleaseTime;
 
 
 
             {
 
                 // Copy the last release time for jitter measurement
 
                 std::lock_guard<std::mutex> lock(_statsMutex);
 
                 localReleaseTime = _releaseTime;
 
             }
 
 
 
             // Calculate start-time jitter
 
             auto startJitterUs = std::chrono::duration_cast<std::chrono::microseconds>(
 
                 startTime - localReleaseTime
 
             ).count();
 
 
 
             // Run the user-provided service function
 
             _doService();
 
 
 
             // Capture end time
 
             auto endTime = std::chrono::steady_clock::now();
 
             auto execTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
 
                 endTime - startTime
 
             ).count();
 
 
 
             // Update stats
 
             {
 
                 std::lock_guard<std::mutex> lock(_statsMutex);
 
 
 
                 // Start jitter stats
 
                 if (startJitterUs < _minStartJitterUs) {
 
                     _minStartJitterUs = startJitterUs;
 
                 }
 
                 if (startJitterUs > _maxStartJitterUs) {
 
                     _maxStartJitterUs = startJitterUs;
 
                 }
 
                 _sumStartJitterUs += startJitterUs;
 
                 _countStartJitter++;
 
 
 
                 // Execution time stats
 
                 if (execTimeUs < _minExecTimeUs) {
 
                     _minExecTimeUs = execTimeUs;
 
                 }
 
                 if (execTimeUs > _maxExecTimeUs) {
 
                     _maxExecTimeUs = execTimeUs;
 
                 }
 
                 _sumExecTimeUs += execTimeUs;
 
                 _countExecTime++;
 
             }
 
         }
 
     }
 
 };
 
 
 
 // The sequencer class contains the services set and manages
 
 // starting/stopping the services. While the services are running,
 
 // the sequencer releases each service at the requisite timepoint.
 
 class Sequencer
 
 {
 
 public:
 
     template<typename... Args>
 
     void addService(Args&&... args)
 
     {
 
         // Construct a Service in-place and store it
 
         _services.emplace_back(
 
             std::make_unique<Service>(std::forward<Args>(args)...)
 
         );
 
     }
 
 
 
     void startServices()
 
     {
 
         _runningFlag = true;
 
         // Start a scheduler thread that periodically releases each service
 
         _schedulerThread = std::jthread([this]()
 
         {
 
             using std::chrono::steady_clock;
 
             using std::chrono::milliseconds;
 
             using std::chrono::duration_cast;
 
 
 
             std::vector<steady_clock::time_point> lastReleaseVector;
 
             lastReleaseVector.reserve(_services.size());
 
 
 
             // Initialize all last release times to "now"
 
             auto currentTime = steady_clock::now();
 
             for (size_t i = 0; i < _services.size(); i++)
 
             {
 
                 lastReleaseVector.push_back(currentTime);
 
             }
 
 
 
             while (_runningFlag)
 
             {
 
                 currentTime = steady_clock::now();
 
                 // Check each service's period
 
                 for (size_t i = 0; i < _services.size(); i++)
 
                 {
 
                     auto& servicePtr = _services[i];
 
                     auto& currentService = *servicePtr;
 
                     auto servicePeriod = currentService.getPeriod();
 
 
 
                     auto elapsedTime = duration_cast<milliseconds>(
 
                         currentTime - lastReleaseVector[i]
 
                     ).count();
 
 
 
                     // If enough time has passed, release the service
 
                     if (elapsedTime >= servicePeriod)
 
                     {
 
                         currentService.release();
 
                         lastReleaseVector[i] = currentTime;
 
                     }
 
                 }
 
                 // Sleep briefly to avoid busy-waiting
 
                 std::this_thread::sleep_for(std::chrono::milliseconds(1));
 
             }
 
         });
 
     }
 
 
 
     void stopServices()
 
     {
 
         // Signal the scheduler to stop
 
         _runningFlag = false;
 
 
 
         // Stop each service
 
         for (auto& service : _services)
 
         {
 
             if (service)
 
                 service->stop();
 
         }
 
 
 
         // Now print out each service's collected stats
 
         // (the jthreads will join automatically as their Service objects go out of scope)
 
         for (auto& service : _services)
 
         {
 
             if (service)
 
                 service->printStats();
 
         }
 
     }
 
 
 
 private:
 
     std::vector<std::unique_ptr<Service>> _services;
 
     std::jthread                         _schedulerThread;
 
     std::atomic<bool>                    _runningFlag{false};
 
 };
