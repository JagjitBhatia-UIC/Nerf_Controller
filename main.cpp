#include "PTU_Controller.h"
#include "Firing_Controller.h"
#include <linux/joystick.h>
#include <cmath>
#include <pthread.h>

int x_speed = 0;
int y_speed = 0;

bool firing = false;

void *firing_thread(void* args) {
	Firing_Controller gun = Firing_Controller();
	
	while(true) {
		if(firing) gun.firingOn();
		else gun.firingOff();
		
		usleep(5000);
	}
}

void *ptu_thread(void* args) {
	PTU_Controller ptu = PTU_Controller();
  	ptu.toOrigin();
  	ptu.SetDelayMS(20);
  	
  	while(true) {
  		ptu.pan(x_speed);
  		ptu.tilt(y_speed);
  		
  		usleep(5000);	
  	}

}

int main() {
	int js_fd = open("/dev/input/js0", O_RDONLY);
	
	pthread_t tid_ptu;
	pthread_t tid_firing;
	
	pthread_create(&tid_ptu, NULL, &ptu_thread, NULL);
	pthread_create(&tid_firing, NULL, &firing_thread, NULL);
	
	struct js_event e;
	
	
	while(true) {
		e = {};
		read(js_fd, &e, sizeof(e));
		
		if(e.type >= 0x80) continue; // Ignore synthetic events
		
		if(e.type == 1 && e.value == 1 && e.number == 2) break;  // "X" Quit
		
		if(e.type == 2) {
			if(e.number == 3) x_speed = round((e.value/32767.0) * 30);
			
			if(e.number == 1) y_speed = round((e.value/32767.0) * 5);
			
			if(e.number == 5) firing = (e.value > 0);
		
		}
		
		
	
	}
	
	
	

}
