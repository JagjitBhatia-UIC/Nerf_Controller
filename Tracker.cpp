#include "Tracker.h"

Tracker::Tracker() {
	Tracker::Tracker('R', 0);
}

Tracker::Tracker(char _color) {
	Tracker::Tracker(_color, 0);
}

Tracker::Tracker(int _cameraID) {
	Tracker::Tracker('R', _cameraID);
}

Tracker::Tracker(char _color, int _cameraID) {
	color = _color;
	cameraID = _cameraID;
	locked = false;
	trackerRunning = false;
	center_x = -1;
	center_y = -1;


	cap.open(cameraID, cv::CAP_ANY);

	if(!cap.isOpened()) {
		std::cout << "ERROR: Unable to open camera!" << std::endl;
		return;
	}
}

void Tracker::convertFacesToBodies(std::vector<cv::Rect> &faces) {
	for(int i = 0; i<faces.size(); i++) {
		faces[i].x *= 2;
		faces[i].y *= 4;
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

		Tracker::getCentroid(bodies[i], origin_x, origin_y);

		for(int m = 0; m < img.row; i++) {
			for(int n = 0; n < img.cols; j++) {
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
	
	
		rgb_tokens = Tracker::split(maxColor, ":");
	
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

void Tracker::track() {
	cv::CascadeClassifier faceDetector;
	cv::HOGDescriptor bodyDetector;
	std::vector<cv::Rect> faces;
	std::vector<cv::Rect> bodies;
	std::vector<cv::Rect> faces_cropped;
	std::vector<cv::Rect> bodies_cropped;
	std::vector<double> weights;

	int frameCount = 0;
	
	cv::Ptr<cv::Tracker> tracker = cv::TrackerMOSSE::create();

	cv::Mat frame;
	cv::Mat frame_gray;
	cv::Mat cropped;
	cv::Mat cropped_gray;
	
	if(!faceDetector.load(samples::findFile("data/haarcascades/haarcascade_frontalface_alt.xml"))) {
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
			cv::cvtColor(frame, frame_gray);
			cv::equalizeHist(frame_gray, frame_gray);

			faceDetector.detectMultiScale(frame_gray, faces);
			bodyDetector.detectMultiScale(frame_gray, bodies, weights);

			// Convert faces to (estimate) bodies
			Tracker::convertFacesToBodies(faces);

			// Append body estimates to bodies vector
			bodies.insert(bodies.end(), faces.begin(), faces.end());

			Tracker::filterByColor(bodies, frame);		// IMPORTANT: Use frame and not frame_gray for color detection

			target.bbox = bodies[rand() % bodies.size()];

			Tracker::getCentroid(target.bbox, target.center_x, target.center_y);

			tracker->init(frame, target.bbox);
			
		}

		else {
			// If tracker failure occurs, remove lock and reacquire target
			if(!tracker->update(frame, target.bbox)) {
				std::cout << "TARGET LOST!" << std::endl;
				locked = false;
				frameCount = 0;
				continue;
			}

			Tracker::getCentroid(target.bbox, target.center_x, target.center_y);

			frameCount++;


			// Verify locked target is actually a target
			// Do this every 150 frames = 5 seconds @ 30 FPS, 2.5 seconds @ 60 FPS
			if(frameCount > 150) {	
				cropped = cv::Mat(frame, target.bbox);

				cv::cvtColor(cropped, cropped_gray);
				cv::equalizeHist(cropped_gray, cropped_gray);

				faceDetector.detectMultiScale(cropped_gray, faces_cropped);
				bodyDetector.detectMultiScale(cropped_gray, bodies_cropped, weights);

				// Convert faces to (estimate) bodies
				Tracker::convertFacesToBodies(faces_cropped);

				// Append body estimates to bodies vector
				bodies_cropped.insert(bodies_cropped.end(), faces_cropped.begin(), faces_cropped.end());

				Tracker::filterByColor(bodies_cropped, frame);		// IMPORTANT: Use frame and not frame_gray for color detection

				if(bodies_cropped.size() < 1) {
					std::cout << "TARGET LOST!" << std::endl;
					locked = false;
				}

				frameCount = 0;
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
	x = center_x;
	y = center_y;
}

Tracker::~Tracker() {
	if(trackerRunning) Tracker::stopTracking();
}

