#include "PTU_Controller.h"
#include "Firing_Controller.h"
#include "Tracker.h"

#include <linux/joystick.h>
#include <cmath>
#include <pthread.h>
#include <time.h>

#define MANUAL 0
#define AUTONOMOUS 1

#define SCAN 0
#define ENGAGE 1

#define FOV_X 66.64
#define FOV_Y 40.62

// Shared globals
int x_speed = 0;
int y_speed = 0;
int x_position = 0;
int y_position = 0;
bool firing = false;
bool colorToggle = false;
char color = 'R';
int camera = 0;
int mode;
int state;

// Convert tracker x,y coordinates to pan,tilt movements
void convertTrackerPositionToPanTilt(int tracker_x, int tracker_y, int &pan, int &tilt) {
  tracker_x -= RESOLUTION_X/2;
  tracker_y -= RESOLUTION_Y/2;

  pan =  round(((double) (tracker_x * 1.0)/RESOLUTION_X) * ((double) (MAX_POSITION * 1.0)/FOV_X));
  tilt =  round(((double) (tracker_y * 1.0)/RESOLUTION_Y) * ((double) (MAX_POSITION_Y * 1.0)/FOV_Y));
}

// Tracker Controller Thread
void *tracker_thread(void* args) {
	bool timerOn = false;
	bool trackerRunning = false;
	bool colorChanged = false;
	int tracker_x, tracker_y;
	clock_t begin_time;

	Tracker tracker(color, camera);

	while(true) {
		while(mode == AUTONOMOUS) {
			if(!trackerRunning) tracker.startTracking();

			if(colorChanged != colorToggle) {
				std::cout << "Performing color toggle..." << std::endl;
				tracker.toggleColoTOGGLEr();
				colorChanged = !colorChanged;
			}

			if(tracker.targetLocked()) {
				timerOn = false;
				tracker.getTargetPosition(tracker_x, tracker_y);
				convertTrackerPositionToPanTilt(tracker_x, tracker_y, x_position, y_position);
				state = ENGAGE;
			}

			else {
				if(!timerOn) {
					begin_time = clock();
					timerOn = true;
				}

				// Target timeout at 1.5 seconds - this is designed to continue locking even during fluctuations
				if((float( clock() - begin_time ) /  CLOCKS_PER_SEC) > 1) {
					std::cout << "Target timed out....Switching to SCAN" << std::endl;
					state = SCAN;
				}
			}
		}

		if(trackerRunning) tracker.stopTracking();
		trackerRunning = false;

	}
}

// Firing Controller Thread
void *firing_thread(void* args) {
	Firing_Controller gun = Firing_Controller();

	while(true) {
		while(mode == MANUAL) {
			if(firing) gun.firingOn();
			else gun.firingOff();
			usleep(5000);
		}

		while(mode == AUTONOMOUS) {
			if(state == SCAN) gun.firingOff();
			if(state == ENGAGE) gun.firingOn();
		}
	}
}

// PTU Controller Thread
void *ptu_thread(void* args) {
	PTU_Controller ptu = PTU_Controller();
  	ptu.toOrigin();
  	ptu.SetDelayMS(20);
  	
	while(true) {
		while(mode == MANUAL) {
			if(x_speed != 0) ptu.pan(x_speed);

			if(y_speed != 0) ptu.tilt(y_speed);
			
			usleep(5000);	
		}

		while(mode == AUTONOMOUS) {
			while(state == SCAN && mode == AUTONOMOUS) {
				for(int i = MIN_POSITION; i < MAX_POSITION && state == SCAN && mode == AUTONOMOUS; i += 5) {
					ptu.pan_abs(i);
				}

				if(state != SCAN || mode != AUTONOMOUS) break;

				for(int j = MAX_POSITION; j > MIN_POSITION && state == SCAN && mode == AUTONOMOUS; j -= 5) {
					ptu.pan_abs(j);
				}
			}

			while(state == ENGAGE && mode == AUTONOMOUS) {
				ptu.move_abs(x_position, y_position);
			}


		}
	}

}


int main(int argc, char **argv) {
	int js_fd = open("/dev/input/js0", O_RDONLY);
	
	struct js_event e;

	mode = MANUAL;
	state = SCAN;

	for(int i = 1; i<argc; i++) {
		if(std::string(argv[i]) == (std::string) "--color" && (i + 1 < argc)) {
			color = std::toupper((char) argv[i+1][0]);

			if(color != 'R' && color != 'B') {
				std::cout << "Invalid color. Color must either be B (blue) or R (red)." << std::endl;
				std::cout << "Using default color R (red)..." << std::endl;
				color = 'R';
			}
		}

		if(std::string(argv[i]) == (std::string) "--camera" && (i + 1 < argc)) {
			if(!isdigit((char) argv[i+1][0])) {
				std::cout << "Invalid camera ID. Camera ID must be a digit." << std::endl;
				std::cout << "Using default camera 0..." << std::endl;
				camera = 0;
			}
			else camera = atoi(argv[i+1]);
		}
	}

	pthread_t tid_ptu;
	pthread_t tid_firing;
	pthread_t tid_tracker;

	pthread_create(&tid_ptu, NULL, &ptu_thread, NULL);
	pthread_create(&tid_firing, NULL, &firing_thread, NULL);
	pthread_create(&tid_tracker, NULL, &tracker_thread, NULL);

	bool quit = false;

	// Main loop
	while(!quit) {
		while(mode == MANUAL) {
			e = {};
			read(js_fd, &e, sizeof(e));
			
			if(e.type >= 0x80) continue;	// Ignore synthetic events
			
			if(e.type == 1 && e.value == 1 && e.number == 2) {
				quit = true;
				break; 	// "X" Quit
			}

			if(e.type == 1 && e.value == 1 && e.number == 3) {
				std::cout << "SWITCHING MODE TO AUTONOMOUS" << std::endl;
				mode = AUTONOMOUS;
				break;
			}
			
			if(e.type == 2) {
				if(e.number == 3) x_speed = round((e.value/32767.0) * 30);	// Handle left/right joystick movement
				
				if(e.number == 1) y_speed = round((e.value/32767.0) * 5);	// Handle up/down joystick movement
				
				if(e.number == 5) firing = (e.value > 0);	// Handle trigger pull
			
			}
		}

		while(mode == AUTONOMOUS) {
			e = {};
			read(js_fd, &e, sizeof(e));
			
			if(e.type >= 0x80) continue;	// Ignore synthetic events
			
			if(e.type == 1 && e.value == 1 && e.number == 2) {
				quit = true;
				break; 	// "X" Quit
			}

			if(e.type == 1 && e.value == 1 && e.number == 3) {
				std::cout << "SWITCHING MODE TO MANUAL" << std::endl;
				mode = MANUAL;
				break;
			}

			if(e.type == 1 && e.value == 1 && e.number == 1) {
				std::cout << "SWITCHING TARGET COLOR" << std::endl;
				colorToggle = !colorToggle;
			}
		}

		 
	}
	
	
}
