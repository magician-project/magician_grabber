#ifndef ARDUINOSENSOR_H_INCLUDED
#define ARDUINOSENSOR_H_INCLUDED


#ifdef __cplusplus
extern "C"
{
#endif

#include "common.h"

typedef struct
{
    GlobalConfig* global;           /**< Shared runtime configuration. */
    char port_name[128];            /**< Serial device path (e.g. "/dev/ttyUSB0"). */
    char csv_name[128];             /**< Path of the output CSV file. */
    int baud_rate;                  /**< Serial baud rate (default 115200). */

    FILE *csv_file;                 /**< Open handle to csv_name; NULL when file output is disabled. */
    char * keep_running;            /**< Points to the global termination flag; thread exits when *keep_running == 0. */
    char running;                   /**< 1 while the thread is active, 0 after it exits. */

    char * extraCommands;           /**< Optional ASCII command sent to the Arduino at startup to select a lighting mode (e.g. "r\n"). */

    int serial_fd;                  /**< File descriptor for the open serial port. */
    unsigned long receivedDataFrames; /**< Total data frames received since thread start. */
    float Hz;                       /**< Measured sensor acquisition rate in Hz. */

    void * callback;                /**< Optional user callback; cast to accelerometer_callback_t or controller_callback_t depending on firmware. */
} ArduinoSerialConfig;



int arduino_signalNewFrame(ArduinoSerialConfig * context);

void *arduino_thread(void *arg);


#ifdef __cplusplus
}
#endif



#endif // ARDUINOSENSOR_H_INCLUDED
