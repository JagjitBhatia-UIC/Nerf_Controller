#include "PTU_Controller.h"
#include "Firing_Controller.h"

int main() {

	//Example code
	
	PTU_Controller ptu = PTU_Controller();
	Firing_Controller gun = Firing_Controller();

	ptu.toOrigin();
	ptu.SetDelayMS(30);
	for(int i = 0; i<100; i++) ptu.pan(12);
	gun.firingOn();
	usleep(5000000);
	for(int j = 0; j<100; j++) ptu.pan(-12);
	gun.firingOff();

}
