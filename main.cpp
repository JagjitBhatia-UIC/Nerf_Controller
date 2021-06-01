#include "PTU_Controller.h"
#include "Firing_Controller.h"

int main() {
	PTU_Controller ptu = PTU_Controller();
	Firing_Controller gun = Firing_Controller();

	ptu.toOrigin();
	usleep(500000);
	gun.firingOn();
	usleep(500000);
	gun.firingOff();

}


/*

#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <iostream>

#define FIRING_HOST "127.0.0.1"
#define FIRING_PORT 5687

int main() {
	int sock, new_socket;

	struct sockaddr_in address;
	

	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		std::cout << "Failed to create socket" << std::endl;
		return -1;
	}

	else {
		std::cout << "Socket created!" << std::endl;
		std::cout << "Attempting to connect...." << std::endl;
	}	

	address.sin_family = AF_INET;
	address.sin_port = htons(FIRING_PORT);
	address.sin_addr.s_addr = inet_addr(FIRING_HOST);

	if(connect(sock, (struct sockaddr *) &address, sizeof(address)) < 0) {
		std::cout << "Failed to connect to server" << std::endl;
		return -1;
	}

	else {
		std::cout << "Socket connected!" << std::endl;
	}

    while(true) {
		usleep(100000);
	char* msg = (char*) "FIRING_ON";
	send(sock, msg, strlen(msg), 0);
	usleep(1000000);
	msg = (char*) "FIRING_OFF";
	send(sock, msg, strlen(msg), 0);
	}
	

	


}

*/