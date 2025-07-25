#ifndef TACTILE_STREAMER_H_INCLUDED
#define TACTILE_STREAMER_H_INCLUDED

#ifdef __cplusplus
extern "C"
{
#endif

#include "common.h"
#include "sharedMemoryVideoBuffers.h"

struct TactileBuffer
{
    float data[TACTILE_STREAMING_ELEMENTS * TACTILE_STREAMING_WINDOW];
    unsigned int data_size;
    unsigned long timestamp;
};



typedef struct
{
    char shm_name[128];    //"tactile_frames.shm"
    char stream_name[128]; //"stream1"
    struct SharedMemoryContext *context;
    struct VideoFrame *frame;
    struct TactileBuffer data;
    struct VideoFrameLocalMapping localMap;
} StreamingTactileContext;


StreamingTactileContext * startTactileStream(const char * shm_name, const char * stream_name, unsigned int window, unsigned int elementsPerUnit);
int stopTactileStream(StreamingTactileContext * shm_stream);

int stream_tactile(struct VideoFrame *frame, struct TactileBuffer* data);


#ifdef __cplusplus
}
#endif


#endif // IMAGE_STREAMER_H_INCLUDED
