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

void Tracker::filterByColor(std::vector<cv::Rect> &bodies) {
	// TODO: Implement color filtering
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
	
	cv::Ptr<Tracker> tracker = cv::TrackerMOSSE::create();

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
	while(true) {
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