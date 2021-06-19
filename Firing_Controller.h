#ifndef FIRE_CTRL_H
#define FIRE_CTRL_H

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

#define US_TO_SEC 1000000

class Firing_Controller {
	private:
		int server;
		bool connected;

	public:
		Firing_Controller();
		// Firing_Controller(int port);		// TODO: Must first implement args in firingScript.py
		~Firing_Controller();
		void firingOn();
		void firingOff();
		void fire(int time_sec);
		void disconnect();
};

#endif