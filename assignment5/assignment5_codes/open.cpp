#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <vector>

// Struct to store 2D points
struct Point2D {
    int x;
    int y;
};

int main() {
    cv::VideoCapture cap(0, cv::CAP_V4L2); // Use V4L2 backend for Raspberry Pi

    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open the camera." << std::endl;
        return -1;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    cap.set(cv::CAP_PROP_FPS, 30);

    cv::Mat frame, hsv, mask;
    std::vector<Point2D> laserTrail; // Store all detected points

    while (true) {
        // Start timer for frame capture
        auto capture_start = std::chrono::high_resolution_clock::now();

        cap >> frame;

        // End timer for frame capture
        auto capture_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> capture_duration = capture_end - capture_start;

        if (frame.empty()) break;

        // Start timer for red point detection
        auto process_start = std::chrono::high_resolution_clock::now();

        cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

        // Draw 4 reference points
        cv::circle(frame, cv::Point(0, 0), 5, cv::Scalar(255, 0, 0), -1);
        cv::circle(frame, cv::Point(639, 0), 5, cv::Scalar(255, 0, 0), -1);
        cv::circle(frame, cv::Point(0, 479), 5, cv::Scalar(255, 0, 0), -1);
        cv::circle(frame, cv::Point(639, 479), 5, cv::Scalar(255, 0, 0), -1);

        // Red detection masks
        cv::Mat mask1, mask2;
        cv::inRange(hsv, cv::Scalar(0, 80, 80), cv::Scalar(10, 255, 255), mask1);
        cv::inRange(hsv, cv::Scalar(160, 80, 80), cv::Scalar(180, 255, 255), mask2);
        mask = mask1 | mask2;

        // Morphological operations
        cv::erode(mask, mask, cv::Mat(), cv::Point(-1, -1), 2);
        cv::dilate(mask, mask, cv::Mat(), cv::Point(-1, -1), 2);

        // Find contours
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        if (!contours.empty()) {
            double maxArea = 0;
            int maxIdx = -1;

            for (size_t i = 0; i < contours.size(); ++i) {
                double area = cv::contourArea(contours[i]);
                if (area > maxArea) {
                    maxArea = area;
                    maxIdx = static_cast<int>(i);
                }
            }

            if (maxIdx != -1 && maxArea > 5.0) {
                cv::Moments M = cv::moments(contours[maxIdx]);
                int cx = static_cast<int>(M.m10 / M.m00);
                int cy = static_cast<int>(M.m01 / M.m00);

                // Store in structure
                Point2D laserPoint{cx, cy};
                laserTrail.push_back(laserPoint); // Optionally store over time

                std::cout << "Laser coordinates: (" << laserPoint.x << ", " << laserPoint.y << ")" << std::endl;

                // Draw on frame
                cv::circle(frame, cv::Point(laserPoint.x, laserPoint.y), 10, cv::Scalar(0, 255, 0), 2);
                cv::putText(frame, "Laser", cv::Point(laserPoint.x + 10, laserPoint.y), cv::FONT_HERSHEY_SIMPLEX, 0.5, {0, 255, 0}, 1);
            }
        }

        auto process_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> process_duration = process_end - process_start;

        // Show timings
        std::cout << std::fixed << std::setprecision(2)
                  << "Capture Time: " << capture_duration.count() << " ms | "
                  << "Processing Time: " << process_duration.count() << " ms" << std::endl;

        // Display output
        cv::imshow("Laser Detection", frame);
        cv::imshow("Red Mask", mask);

        if (cv::waitKey(1) == 27) break; // ESC to quit
    }

    return 0;
}
