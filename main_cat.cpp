#include <cstdint>	
#include <cstdio>
#include <csignal> 
#include <sys/ioctl.h>
#include <cstring>
#include <zmq.hpp>
#include "Sequencer.hpp"
#include "cameraService.hpp"
#include "red_laser_service.hpp"
#include "config_update_service.hpp"

bool stop_requested=false;


void signal_handler(int signum) {
    syslog(LOG_INFO, "Interrupt signal (%d) received. Stopping services...", signum);
    stop_requested = true;
}

int main() {
    openlog("LOG_MSG", LOG_PID | LOG_PERROR, LOG_USER);

    signal(SIGINT, signal_handler); // Register handler for Ctrl+C
	
	//attempt to initalize the camera
    if (init_camera() != EXIT_SUCCESS) 
    {
		syslog(LOG_ERR,"camera failed to setup exiting !");
		return EXIT_FAILURE;
	}

  
    Sequencer sequencer{};
    
    //service 1 is camera service running at 1000/30 approx 30fps
    sequencer.addService(camera_capture_service, 1, 98, 30, 1);
    sequencer.addService(red_laser_detect, 1, 97, 35, 2);
    sequencer.addService(config_update_service,1,96,2000,3);
//warm up cache?
    for(int i=0;i<10;i++){
camera_capture_service();
}  
    sequencer.startServices();

    // Wait until Ctrl+C is pressed
    while (!stop_requested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    sequencer.stopServices();
    syslog(LOG_INFO, "Services stopped. Exiting.");
    return 0;
}

