#ifndef TRACKER_H
#define TRACKER_H

// Include OpenCV libraries
#include <opencv2/objdetect.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <opencv2/core/ocl.hpp>
#include <opencv2/tracking/tracking_legacy.hpp>

// Include std libraries
#include <iostream>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <iterator>
#include <thread>

// Include C libraries
#include <stdlib.h>
#include <time.h>
#include <math.h>

typedef struct Target {
	cv::Rect_<double> bbox;
	int center_x;
	int center_y;
} Target;

class Tracker {
	private:
		bool locked;
		cv::VideoCapture cap;
		char color;
		int cameraID;
		Target target;
		bool trackerRunning;
		std::thread trackerThread;

		void getCentroid(const cv::Rect bbox, int &center_x, int &center_y);
		void convertFacesToBodies(std::vector<cv::Rect> &faces);
		void filterByColor(std::vector<cv::Rect> &bodies, cv::Mat img);
		void track(); 
		std::vector<std::string> split(const std::string& s, char delimiter);


	public:
		Tracker();
		Tracker(char _color);
		Tracker(int _cameraID);
		Tracker(char _color, int _cameraID);
		~Tracker();
		bool targetLocked();
		void getTargetPosition(int &x, int &y);
		void startTracking();
		void stopTracking();
		void avgCentroid(std::vector<cv::Rect> &bodies, int &center_x, int &center_y);

};

#endif
