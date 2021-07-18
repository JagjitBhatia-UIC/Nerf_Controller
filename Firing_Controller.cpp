#include "Firing_Controller.h"

Firing_Controller::Firing_Controller() {
	system("python3 ../firingScript.py > firingLog.txt &");	// Start firingScript.py
  	usleep(1 * US_TO_SEC);		// Sleep for 1 second to wait for script to initialize

	int sock;

	struct sockaddr_in address;
	
	connected = false;
	
	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		std::cout << "Failed to create socket" << std::endl;
		return;
	}

	else {
		// std::cout << "Socket created!" << std::endl;
		// std::cout << "Attempting to connect...." << std::endl;
	}	

	address.sin_family = AF_INET;
	address.sin_port = htons(FIRING_PORT);
	address.sin_addr.s_addr = inet_addr(FIRING_HOST);

	if(connect(sock, (struct sockaddr *) &address, sizeof(address)) < 0) {
		std::cout << "Failed to connect to server" << std::endl;
		return;
	}

	else {
		//std::cout << "Socket connected!" << std::endl;
		connected = true;
	}
	
	std::cout << "Firing Controller Ready!" << std::endl;
	server = sock;
}

void Firing_Controller::disconnect() {
	if(connected) {
		char* msg = (char*) "QUIT";
		send(server, msg, strlen(msg), 0);
		close(server);
		connected = false;
	}
}

Firing_Controller::~Firing_Controller() {
	disconnect();
}

void Firing_Controller::firingOn() {
	if(connected) {
		char* msg = (char*) "FIRING_ON";
		send(server, msg, strlen(msg), 0);
	}

	else {
		std::cout << "Firing service not connected!" << std::endl;
	}
}

void Firing_Controller::firingOff() {
	if(connected) {
		char* msg = (char*) "FIRING_OFF";
		send(server, msg, strlen(msg), 0);
	}

	else {
		std::cout << "Firing service not connected!" << std::endl;
	}
}

void Firing_Controller::fire(int time_sec) {
	firingOn();
	usleep(time_sec * US_TO_SEC);
	firingOff();
}
