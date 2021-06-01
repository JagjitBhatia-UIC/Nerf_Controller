#include "PTU_Controller.h"
#include "Firing_Controller.h"

int main() {

	//Example code
	
	PTU_Controller ptu = PTU_Controller();
	Firing_Controller gun = Firing_Controller();

	ptu.toOrigin();
	usleep(500000);
	gun.firingOn();
	usleep(500000);
	gun.firingOff();

}