#ifndef PTU_CTRL_H
#define PTU_CTRL_H

// C library headers
#include <iostream>
#include <string.h>

// Linux headers
#include <fcntl.h> 
#include <errno.h> 
#include <termios.h> 
#include <unistd.h> 
#include <sys/file.h>
#include <string>

#define ORIGIN_X 511
#define ORIGIN_Y 530
#define MAX_POSITION 1023

#define PTU_PACKET_SIZE_BYTES 8
#define PTU_SERIAL_MAGIC_NUMBER 7   // Magic number to sleep before starting PTU serial writes 

#define DEFAULT_MSG_RATE_MS 500
#define MS_TO_NS 1000

typedef struct QueryPacket {
    int Tilt_High;
    int Tilt_Low;
    int Pan_High;
    int Pan_Low;
    int OpCode;
} QueryPacket;


class PTU_Controller {
    private:
        int pos_x;
        int pos_y;
        int ptu_conn;
        bool connected;
        int rate_ms;

        void serializeQueryPacket(QueryPacket query, char (&cmd_packet)[PTU_PACKET_SIZE_BYTES]);
        void ToTwoBytes(unsigned long number, int &low, int &high);
        int PTU_SendCommand(QueryPacket cmd);

    public:
        PTU_Controller();
        PTU_Controller(int _pos_x, int _pos_y);
        ~PTU_Controller();
        int PTU_Connect();
        void PTU_Disconnect();
        int toOrigin();
        int tilt_abs(int pos);
        int pan_abs(int pos);
        int move_abs(int _pos_x, int _pos_y);
        int tilt(int inc);
        int pan(int inc);
        int move(int _pan, int _tilt);
        void PrintState();
        void SetDelayMS(int delay);
        
};

#endif