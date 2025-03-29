/*
 * This is a C++ version of the canonical pthread service example. It intends
 * to abstract the service management functionality and sequencing for ease
 * of use. Much of the code is left to be implemented by the student.
 *
 * Build with g++ --std=c++23 -Wall -Werror -pedantic
 * Steve Rizor 3/16/2025
 */

 #pragma once

 #include <cstdint>
 #include <functional>
 #include <thread>
 #include <vector>
 #include <iostream>
 #include <semaphore>
 using namespace std;
 
 // The service class contains the service function and service parameters
 // (priority, affinity, etc). It spawns a thread to run the service, configures
 // the thread as required, and executes the service whenever it gets released.
 class Service
 {
 public:
     template <typename T> // explain About templates in case of template classes and constructor
     // constructor for class Service
     Service(T &&doService, uint8_t affinity, uint8_t priority, uint32_t period) : _doService(doService), // initilziation list a must in certain Cases
                                                                                   _affinity(affinity),
                                                                                   _priority(priority),
                                                                                   _period(period),
                                                                                   _semaphore(0)
     //      _prviate_sema(0)
 
     {
         // todo: store service configuration values
         // todo: initialize release semaphore
         cout << "hello constructor Service\n";
         // Start the service thread, which will begin running the given function immediately
         _service = jthread(&Service::_provideService, this);
     }
 
     void stop()
     {
         // todo: change state to "not running" using an atomic variable
         // (heads up: what if the service is waiting on the semaphore when this happens?)
     }
 
     void release()
     {
         // todo: release the service using the semaphore
     }
 
 private:
     function<void(void)> _doService;
     jthread _service;
     uint8_t _affinity;
     uint8_t _priority;
     uint32_t _period;
     binary_semaphore _semaphore;
     void _initializeService()
     {
         // todo: set affinity, priority, sched policy
         // (heads up: the thread is already running and we're in its context right now)
     }
 
     void _provideService()
     {
         _initializeService();
         // todo: call _doService() on releases (sem acquire) while the atomic running variable is true
     }
 };
 
 // The sequencer class contains the services set and manages
 // starting/stopping the services. While the services are running,
 // the sequencer releases each service at the requisite timepoint.
 class Sequencer
 {
 public:
     template <typename... Args>
     void addService(Args &&...args) // a varidaic funcitons
     {
         // Add the new service to the services list,
         // constructing it in-place with the given args
         // Perfectly forwards each argument to the Service constructor. std::forward<Args>(args)...
         _services.emplace_back(forward<Args>(args)...); // Directly constructs a Service object in-place inside the vector might be like push_back  ?
     }
 
     void startServices()
     {
         // todo: start timer(s), release services
     }
 
     void stopServices()
     {
         // todo: stop timer(s), stop services
     }
 
 private:
     vector<Service> _services;
 };
 