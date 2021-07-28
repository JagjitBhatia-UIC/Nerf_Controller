#include "PTU_Controller.h"
#include "Firing_Controller.h"
#include "Tracker.h"

#include <linux/joystick.h>
#include <cmath>
#include <pthread.h>
#include <time.h>
#include <cstring>

#define MANUAL 0
#define AUTONOMOUS 1

#define SCAN 0
#define ENGAGE 1

#define FOV_X 66.64
#define FOV_Y 40.62

// Shared globals
int x_speed = 0;
int y_speed = 0;
int x_position;
int y_position;
bool firing = false;
bool colorToggle = false;
char color = 'R';
int camera = 0;
int mode;
int state;

// Snagged from https://www.fluentcpp.com/2017/04/21/how-to-split-a-string-in-c/
std::vector<std::string> split(const std::string& s, char delimiter)
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

// Convert tracker x,y coordinates to pan,tilt movements
void convertTrackerPositionToPanTilt(int tracker_x, int tracker_y, int &pan, int &tilt) {
 

  /*
  pan =  round(((double) (tracker_x * 1.0)/RESOLUTION_X) * ((double) (MAX_POSITION * 1.0)/FOV_X));
  tilt =  round(((double) (tracker_y * 1.0)/RESOLUTION_Y) * ((double) (MAX_POSITION_Y * 1.0)/FOV_Y)); */

  //pan = round(((double) (tracker_x * 1.0)/RESOLUTION_X) * ((double) (FOV_X/360.0) * (MAX_POSITION * 1.0)/(360.0)));

   pan = -1 * round(((((double) (tracker_x - (RESOLUTION_X * 1.0)/2)) * ((double) FOV_X))/(360.0))/(270.0/MAX_POSITION));

  //pan *= 10;
}


void *tcp_thread(void* args) {
	std::cout << "Attempting to connect..." << std::endl;
	int tcp_socket;
	struct sockaddr_in address;
	char recvBuf[1024];

	if((tcp_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		std::cout << "Failed to initialize socket" << std::endl;
		return 0;
	}

	else {
		std::cout << "Socket initialized." << std::endl;
	}

	address.sin_family = AF_INET;
	address.sin_port = htons(2123);
	address.sin_addr.s_addr = inet_addr("127.0.0.1");

	if(connect(tcp_socket, (struct sockaddr*) &address, sizeof(address)) < 0) {
		std::cout << "Failed to connect to TCP Server" << std::endl;
		return 0;
	}

	else {
		std::cout << "Connection successful!" << std::endl;
	}

	while(true) {
		memset(recvBuf, 0, sizeof(recvBuf));
		if(read(tcp_socket, recvBuf, sizeof(recvBuf) - 1) > 0) {
			if(std::strcmp(recvBuf, "lock") == 0) state = ENGAGE;
			if(std::strcmp(recvBuf, "unlock") == 0) state = SCAN;
		}
	}
}

void *udp_thread(void* args) {
	int udp_socket, tracker_x, tracker_y;
	struct sockaddr_in address, client;
	char recvBuf[1024];
	std::vector<std::string> position_vector;

	if((udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		std::cout << "Failed to initialize socket" << std::endl;
		return 0;
	}

	address.sin_family = AF_INET;
	address.sin_port = htons(3284);
	address.sin_addr.s_addr = INADDR_ANY;

	if(bind(udp_socket, (const struct sockaddr*) &address, sizeof(address)) < 0) {
		std::cout << "Failed to bind to address & port" << std::endl;
		return 0;
	}

	socklen_t len = sizeof(client);
	
	while(true) {
		memset(recvBuf, 0, sizeof(recvBuf));
		if(recvfrom(udp_socket, (char*) recvBuf, sizeof(recvBuf), MSG_WAITALL, (struct sockaddr*) &client, &len) > 0) {
			position_vector = split(std::string(recvBuf), ',');
			if(position_vector.size() > 1) {
				tracker_x = atoi(position_vector[0]);
				tracker_y = atoi(position_vector[1]);

				convertTrackerPositionToPanTilt(tracker_x, tracker_y, x_position, y_position);

			}
		}
	}
	
	return 0;

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
				std::cout << "\n\n\n\n\nPerforming color toggle...\n\n\n\n\n" << std::endl;
				tracker.toggleColor();
				colorChanged = !colorChanged;
			}

			if(tracker.targetLocked()) {
				timerOn = false;
				tracker.getTargetPosition(tracker_x, tracker_y);
				std::cout << "TARGET LOCKED AT POSITION: " << tracker_x << " " << tracker_y << std::endl;
				convertTrackerPositionToPanTilt(tracker_x, tracker_y, x_position, y_position);
				state = ENGAGE;
			}

			else {
				if(!timerOn) {
					begin_time = clock();
					timerOn = true;
				}

				// Target timeout at 1.5 seconds - this is designed to continue locking even during fluctuations
				if((float( clock() - begin_time ) /  CLOCKS_PER_SEC) > 1 && state == ENGAGE) {
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
	int curr_x, curr_y;
  	ptu.toOrigin();
  	ptu.SetDelayMS(0);

	ptu.getCurrentPosition(curr_x, curr_y);


	
  	
	while(true) {
		while(mode == MANUAL) {
			if(x_speed != 0) ptu.pan(x_speed);

			if(y_speed != 0) ptu.tilt(y_speed);
			
			usleep(5000);	
		}

		while(mode == AUTONOMOUS) {
			ptu.toOrigin();

			/*
			while(state == SCAN && mode == AUTONOMOUS) {
				ptu.getCurrentPosition(curr_x, curr_y);

				for(int i = curr_x; i < MAX_POSITION && state == SCAN && mode == AUTONOMOUS; i += 2) {
					ptu.pan(2);
					usleep(2000);
				}

				if(state != SCAN || mode != AUTONOMOUS) break;

				for(int j = curr_x; j > MIN_POSITION && state == SCAN && mode == AUTONOMOUS; j -= 2) {
					ptu.pan(-2);
					usleep(1000);
				}
			}*/

			while(state == ENGAGE && mode == AUTONOMOUS) {
				std::cout << "MOVING BY X: " << x_position << std::endl;
				ptu.pan_abs(ORIGIN_X + x_position); 
				//ptu.move_abs(x_position, y_position);
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
	//pthread_t tid_tracker;

	pthread_create(&tid_ptu, NULL, &ptu_thread, NULL);
	pthread_create(&tid_firing, NULL, &firing_thread, NULL);
	//pthread_create(&tid_tracker, NULL, &tracker_thread, NULL);

	pthread_t tid_tcp_server;
	pthread_t tid_udp_server;

	pthread_create(&tid_tcp_server, NULL, &tcp_thread, NULL);
	pthread_create(&tid_udp_server, NULL, &udp_thread, NULL);

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
