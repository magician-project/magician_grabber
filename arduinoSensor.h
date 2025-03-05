#ifndef ARDUINOSENSOR_H_INCLUDED
#define ARDUINOSENSOR_H_INCLUDED


#ifdef __cplusplus
extern "C"
{
#endif

#include "common.h"

typedef struct
{
    GlobalConfig* global;
    char port_name[128];
    char csv_name[128];
    int baud_rate;

    FILE *csv_file;
    char * keep_running;
    char running;

    int serial_fd;
    unsigned long receivedDataFrames;
    float Hz;
} ArduinoSerialConfig;



void *arduino_thread(void *arg);


#ifdef __cplusplus
}
#endif



#endif // ARDUINOSENSOR_H_INCLUDED
