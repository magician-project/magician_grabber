#ifndef ATIFORCESENSOR_H_INCLUDED
#define ATIFORCESENSOR_H_INCLUDED


#ifdef __cplusplus
extern "C"
{
#endif

#include "common.h"

typedef struct
{
    GlobalConfig* global;           /**< Shared runtime configuration. */
    char ip_address[128];           /**< ATI NetFT sensor IP address (e.g. "192.168.1.1"). */
    int port;                       /**< UDP port the sensor listens on (default 49152). */
    char csv_name[128];             /**< Path of the output CSV file for force/torque data. */

    FILE * csv_file;                /**< Open handle to csv_name; NULL when file output is disabled. */
    char * keep_running;            /**< Points to the global termination flag; thread exits when *keep_running == 0. */
    char running;                   /**< 1 while the thread is active, 0 after it exits. */

    int socketHandle;               /**< UDP socket file descriptor used to receive NetFT packets. */
    unsigned long receivedDataFrames; /**< Total UDP frames received since thread start. */
    float Hz;                       /**< Measured sensor acquisition rate in Hz. */

    void * callback;                /**< Optional user callback invoked on every new sample; cast to force_callback_t. */

} ATINetFTConfig;



void *atinetft_thread(void *arg);

#ifdef __cplusplus
}
#endif



#endif // ATIFORCESENSOR_H_INCLUDED
