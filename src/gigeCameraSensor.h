#ifndef GIGECAMERASENSOR_H_INCLUDED
#define GIGECAMERASENSOR_H_INCLUDED


#ifdef __cplusplus
extern "C"
{
#endif

#include "common.h"

typedef struct
{
    GlobalConfig* global;           /**< Shared runtime configuration. */
    char device_name[128];          /**< ARAVIS camera identifier string. */
    char csv_name[128];             /**< Path of the per-frame timestamp CSV file. */
    unsigned int width;             /**< Capture width in pixels. */
    unsigned int height;            /**< Capture height in pixels. */

    unsigned int exposure;          /**< Exposure time in microseconds; 0 means use the camera default. */
    double       gain;              /**< Analogue gain applied by the camera. */
    double       blackLevel;        /**< Black-level offset applied by the camera. */
    double       frameRate;         /**< Requested frame rate in Hz. */
    double       actualFrameRate;   /**< Measured frame rate computed from captured frame timestamps. */

    FILE *csv_file;                 /**< Open handle to csv_name; NULL when file output is disabled. */
    char * keep_running;            /**< Points to the global termination flag; thread exits when *keep_running == 0. */
    char running;                   /**< 1 while the thread is active, 0 after it exits. */

    unsigned long framesCaptured;        /**< Total frames successfully written to disk or shared memory. */
    unsigned long n_completed_buffers;   /**< ARAVIS completed-buffer counter. */
    unsigned long n_failures;            /**< ARAVIS buffer failure counter. */
    unsigned long n_underruns;           /**< ARAVIS input-queue underrun counter (camera ran out of pre-allocated buffers). */

    void * camera;                  /**< Opaque ArvCamera handle. */
    void * stream;                  /**< Opaque ArvStream handle. */
    void * error;                   /**< Last GError from ARAVIS; NULL on success. */

    void * camera_shm_stream;       /**< Shared-memory stream context; non-NULL when --stream is active. */
    void * callback;                /**< Optional user callback invoked on every captured frame; cast to camera_callback_t. */
} GiGECameraConfig;



void *gigecamera_thread(void *arg);


#ifdef __cplusplus
}
#endif


#endif // GIGECAMERASENSOR_H_INCLUDED
