#ifndef ATIFORCESENSOR_H_INCLUDED
#define ATIFORCESENSOR_H_INCLUDED


#ifdef __cplusplus
extern "C"
{
#endif

#include "common.h"

typedef struct
{
    GlobalConfig* global;
    char ip_address[128];
    int port;
    char csv_name[128];
    FILE * csv_file;
    char * keep_running;
    int socketHandle;
    unsigned long receivedDataFrames;
} ATINetFTConfig;



void *atinetft_thread(void *arg);

#ifdef __cplusplus
}
#endif



#endif // ATIFORCESENSOR_H_INCLUDED
