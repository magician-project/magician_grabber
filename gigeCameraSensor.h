#ifndef GIGECAMERASENSOR_H_INCLUDED
#define GIGECAMERASENSOR_H_INCLUDED


#ifdef __cplusplus
extern "C"
{
#endif

#include "common.h"

// Structure definitions for each sensor
typedef struct
{
    GlobalConfig* global;
    char device_name[128];
    char csv_name[128];
    unsigned int width;
    unsigned int height;

    unsigned int exposure; // 0 means no setting
    double       gain;
    double       blackLevel;
    double       frameRate;
    double       actualFrameRate;

    FILE *csv_file;
    char * keep_running;
    char running;

    unsigned long framesCaptured;
    unsigned long n_completed_buffers;
    unsigned long n_failures;
    unsigned long n_underruns;

    void * camera;
    void * stream;
    void * error;

    void * camera_shm_stream;
    void * callback;
} GiGECameraConfig;



void *gigecamera_thread(void *arg);


#ifdef __cplusplus
}
#endif


#endif // GIGECAMERASENSOR_H_INCLUDED
