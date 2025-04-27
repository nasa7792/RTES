#include "red_laser_service.hpp"

//default config


HSVConfig config; 
std::mutex config_mutex;

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

void red_laser_detect (){
	
	    
    struct timespec start = {0, 0};
    struct timespec end = {0, 0};
    struct timespec exec = {0, 0};
  
    cv::Mat frame;

    {
        std::lock_guard<std::mutex> lock(frame_mutex);
        if (latest_frame.empty()) return;
        frame = latest_frame.clone();
    }
    
    HSVConfig current_config;
//get latest config in case if its updated
{
    std::lock_guard<std::mutex> lock(config_mutex);
    current_config = config;

}
    //most execution overhead due to this 
	clock_gettime(CLOCK_REALTIME, &start);
    cv::Mat hsv, mask, lower_red, upper_red;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

cv::inRange(hsv, current_config.lower1, current_config.upper1, lower_red);
cv::inRange(hsv, current_config.lower2, current_config.upper2, upper_red);
    mask = lower_red | upper_red;

    cv::erode(mask, mask, cv::Mat(), cv::Point(-1, -1), 2);
    cv::dilate(mask, mask, cv::Mat(), cv::Point(-1, -1), 2);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    for (const auto& contour : contours) {
        if (cv::contourArea(contour) < 50) continue;
        cv::Moments m = cv::moments(contour);
        if (m.m00 != 0) {
            int cx = int(m.m10 / m.m00);
            int cy = int(m.m01 / m.m00);
           syslog(LOG_INFO,"laser detected x,y %d %d",cx,cy);
           //cv::circle(frame, cv::Point(cx, cy), 5, cv::Scalar(0, 255, 0), -1);
        }
    }
clock_gettime(CLOCK_REALTIME, &end);
delta_t(&end, &start, &exec);
double run_time = (exec.tv_sec * 1000.0) + (exec.tv_nsec / 1000000.0);

//show to prof
//syslog(LOG_INFO, "  run Execution Time    : %.3f ms (%.0f ns)", run_time, run_time * 1e6);
//do not enable these | only for debugging enable
	//cv::imshow("Red Laser Detection", frame);
    //cv::waitKey(1);
}
