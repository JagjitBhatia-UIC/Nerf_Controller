#include "Tracker.h"
#include "nms.hpp"

Tracker::Tracker() {
	Tracker('R', 0);
}

Tracker::Tracker(char _color) {
	Tracker(_color, 0);
}

Tracker::Tracker(int _cameraID) {
	Tracker('R', _cameraID);
}

Tracker::Tracker(char _color, int _cameraID) {
	if(_color != 'B' && _color != 'R') color = 'R';
	else color = _color;
	cameraID = _cameraID;
	locked = false;
	trackerRunning = false;
	target.center_x = -1;
	target.center_y = -1;


	cap.open(cameraID, cv::CAP_ANY);
	cap.set(3, RESOLUTION_X);
	cap.set(4, RESOLUTION_Y);

	if(!cap.isOpened()) {
		std::cout << "ERROR: Unable to open camera!" << std::endl;
		return;
	}
}

void Tracker::toggleColor() {
	if(color == 'R') color = 'B';
	else color = 'R';
}

void Tracker::convertFacesToBodies(std::vector<cv::Rect> &faces) {
	for(int i = 0; i<faces.size(); i++) {
		faces[i].width *= 2;
		faces[i].height *= 4;
	}
}

// Snagged from https://www.fluentcpp.com/2017/04/21/how-to-split-a-string-in-c/
std::vector<std::string> Tracker::split(const std::string& s, char delimiter)
{
   std::vector<std::string> tokens;
   std::string token;
   std::istringstream tokenStream(s);
   while (std::getline(tokenStream, token, delimiter))
   {
      tokens.push_back(token);
   }
   return tokens;
}



void Tracker::getCentroid(const cv::Rect bbox, int &center_x, int &center_y) {
	center_x = bbox.x + round((bbox.width * 1.0)/2.0);
	center_y = bbox.y + round((bbox.height * 1.0)/2.0);
}

void Tracker::avgCentroid(std::vector<cv::Rect> &bodies, int &center_x, int &center_y) {
	int sum_center_x = 0;
	int sum_center_y = 0;
	int x, y;

	for(int i = 0; i<bodies.size(); i++) {
		getCentroid(bodies[i], x, y);
		sum_center_x += x;
		sum_center_y += y;
	}

	int average_center_x = round((sum_center_x * 1.0)/(bodies.size() * 1.0));
	int average_center_y = round((sum_center_y * 1.0)/(bodies.size() * 1.0));

	center_x = average_center_x;
	center_y = average_center_y;
}

void Tracker::filterByColor(std::vector<cv::Rect> &bodies, cv::Mat img) {
	std::vector<cv::Rect> filtered;
	std::vector<std::string> rgb_tokens;
	std::unordered_map<std::string, int> colors;

	std::string color_key;
	std::string maxColor;

	int origin_x, origin_y;
	int R, G, B;
	double dist;
	double strength;


	for(int i = 0; i < bodies.size(); i++) {
		colors.clear();

		getCentroid(bodies[i], origin_x, origin_y);

		for(int m = 0; m < img.rows; m++) {
			for(int n = 0; n < img.cols; n++) {
				dist = sqrt(pow(abs(origin_x - m), 2) + pow(abs(origin_y - n), 2));
				strength = 1.0/(1.0 + dist);

				B = static_cast<int>(img.at<cv::Vec3b>(m, n)[0]);
				G = static_cast<int>(img.at<cv::Vec3b>(m, n)[1]);
				R = static_cast<int>(img.at<cv::Vec3b>(m, n)[2]);

				color_key = std::to_string(R) + ":" + std::to_string(G) + ":" + std::to_string(B);

				if(colors.find(color_key) == colors.end()) {
					colors[color_key] = strength;
				}

				else {
					colors[color_key] += strength;
				}
			}
		}

		maxColor = std::max_element(colors.begin(), colors.end(), 
				[](std::pair<std::string, int> color1, std::pair<std::string, int> color2) -> bool {
					return color1.second < color2.second;
				})->first;
	
	
		rgb_tokens = split(maxColor, ':');
	
		R = std::stoi(rgb_tokens[0]);
		G = std::stoi(rgb_tokens[1]);
		B = std::stoi(rgb_tokens[2]);
	
		if(color == 'B') {
			if(B > 100 && B > G && (B + G) > R * 2) filtered.push_back(bodies[i]);
		}

		if(color == 'R') {
			if(R > 100 && R > G * 2 && R > B * 2) filtered.push_back(bodies[i]);
		}
	}

	bodies = filtered;
}

std::vector<std::vector<float>> rectanglesToFloatVector(std::vector<cv::Rect> rectangles) {
	std::vector<std::vector<float>> to_ret;

	for(int i = 0; i<rectangles.size(); i++) {
		std::vector<float> rectangle;
		rectangle.push_back(rectangles[i].x);
		rectangle.push_back(rectangles[i].y);
		rectangle.push_back(rectangles[i].x + rectangles[i].width);
		rectangle.push_back(rectangles[i].y + rectangles[i].height);

		to_ret.push_back(rectangle);
	}

	return to_ret;
}

void Tracker::track() {
	trackerRunning = true;
	cv::CascadeClassifier faceDetector;
	cv::HOGDescriptor bodyDetector;
	std::vector<cv::Rect> faces;
	std::vector<cv::Rect> bodies;
	std::vector<cv::Rect> faces_cropped;
	std::vector<cv::Rect> bodies_cropped;
	std::vector<double> weights;

	cv::Ptr<cv::legacy::TrackerMOSSE> tracker;

	int frameCount = 0;
	
	cv::Mat frame;
	cv::Mat frame_gray;
	
	if(!faceDetector.load("/Users/jagjitbhatia/Desktop/cvtest/opencv/data/haarcascades/haarcascade_frontalface_default.xml")) {
		std::cout << "ERROR: Unable to load face detector!" << std::endl;
		return;
	}

	bodyDetector.setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector());

	srand(time(NULL));


	// TODO: Improve code re-use for target acquisition/verification 
	while(trackerRunning) {
		cap.read(frame);

		if(frame.empty()) {
			std::cout << "ERROR: Empty Frame!" << std::endl;
			continue;
		}
		
		// If target is not yet acquired, attempt to acquire target
		if(!locked) {
			cv::cvtColor(frame, frame_gray, cv::COLOR_RGB2GRAY);
			cv::equalizeHist(frame_gray, frame_gray);


			faceDetector.detectMultiScale(frame_gray, faces, 1.3, 5); // 1.3 = scale factor, 5 = minimum neighbor
			bodyDetector.detectMultiScale(frame_gray, bodies, weights,0, cv::Size(8,8));

			// Convert faces to (estimate) bodies
			convertFacesToBodies(faces);

			// Append body estimates to bodies vector
			bodies.insert(bodies.end(), faces.begin(), faces.end());
			std::vector<std::vector<float>> bodies_vect = rectanglesToFloatVector(bodies);
			bodies = nms(bodies_vect, 0.001);

			filterByColor(bodies, frame);		// IMPORTANT: Use frame and not frame_gray for color detection
			
			if(bodies.size() > 0) {
				target.bbox = bodies[rand() % bodies.size()];
				getCentroid(target.bbox, target.center_x, target.center_y);
				tracker = cv::legacy::TrackerMOSSE::create();
				tracker->init(frame, target.bbox);
				locked = true;
			}
		}

		else {
			// If tracker failure occurs, remove lock and reacquire target
			if((!tracker->update(frame, target.bbox))) {
				locked = false;
				frameCount = 0;
				continue;
			}

			getCentroid(target.bbox, target.center_x, target.center_y);


			if(target.center_x < 0 || target.center_y < 0) {
				locked = false;
				frameCount = 0;
				continue;
			}

			frameCount++;


			// Verify locked target is actually a target
			// Do this every 150 frames = 5 seconds @ 30 FPS, 2.5 seconds @ 60 FPS
			if(frameCount > 150) {	
				frameCount = 0;

				if(target.center_x < 0 || target.center_y < 0) {
					locked = false;
					continue;
				}
				
				cv::Rect target_bbox = target.bbox;

				if(target.bbox.x < 0) target_bbox.x = 0;
				if(target.bbox.y < 0) target_bbox.y = 0;

				if(target.bbox.x + target.bbox.width > frame.cols) {
					target_bbox.width = frame.cols - target.bbox.x;
				}

				if(target.bbox.y + target.bbox.height > frame.rows) {
					target_bbox.height = frame.rows - target.bbox.y;
				}

				faceDetector.detectMultiScale(frame_gray, faces, 1.3, 5); // 1.3 = scale factor, 5 = minimum neighbor
				bodyDetector.detectMultiScale(frame_gray, bodies, weights,0, cv::Size(8,8));

				// Convert faces to (estimate) bodies
				convertFacesToBodies(faces_cropped);

				// Append body estimates to bodies vector
				bodies_cropped.insert(bodies_cropped.end(), faces_cropped.begin(), faces_cropped.end());

				filterByColor(bodies_cropped, frame);		// IMPORTANT: Use frame and not frame_gray for color detection

				if(bodies_cropped.size() < 1) {
					locked = false;
				}

			}


		}

	}
}

void Tracker::startTracking() {
	if(!trackerRunning) {
		trackerRunning = true;
		trackerThread = std::thread(&Tracker::track, this);
	}
}

void Tracker::stopTracking() {
	trackerRunning = false;
	trackerThread.join();
}

bool Tracker::targetLocked() {
	return locked;
}

void Tracker::getTargetPosition(int &x, int &y) {
	x = target.center_x;
	y = target.center_y;
}

Tracker::~Tracker() {
	if(trackerRunning) stopTracking();
}

