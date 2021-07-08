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

// Include std libraries
#include <iostream>
#include <string>

// Include C libraries
#include <stdlib.h>
#include <time.h>

typedef struct Target {
	cv::Rect bbox;
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

		void getCentroid(const cv::Rect bbox, int &center_x, int &center_y);

	
	public:
		Tracker();
		Tracker(char _color);
		Tracker(int _cameraID);
		Tracker(char _color, int _cameraID);
		~Tracker();

};

#endif
