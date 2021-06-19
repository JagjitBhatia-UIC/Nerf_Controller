make: main.cpp PTU_Controller.cpp Firing_Controller.cpp
	g++ -o nerf main.cpp PTU_Controller.cpp Firing_Controller.cpp -pthread
