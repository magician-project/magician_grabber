#ifndef IMAGE_STREAMER_H_INCLUDED
#define IMAGE_STREAMER_H_INCLUDED

#ifdef __cplusplus
extern "C"
{
#endif


#include "sharedMemoryVideoBuffers.h"

struct Image
{
    const unsigned char * pixels;
    unsigned int width;
    unsigned int height;
    unsigned int channels;
    unsigned int bitsperpixel;
    unsigned int image_size;
    unsigned int timestamp;
};



typedef struct
{
    char shm_name[128];    //"video_frames.shm"
    char stream_name[128]; //"stream1"
    struct SharedMemoryContext *context;
    struct VideoFrame *frame;
    struct Image dataAsImage;
    struct VideoFrameLocalMapping localMap;
} StreamingContext;


StreamingContext * startStream(const char * shm_name, const char * stream_name, unsigned int width, unsigned int height, unsigned int channels);
int stopStream(StreamingContext * shm_stream);

int stream_image(struct VideoFrame *frame, struct Image* dataAsImage);


#ifdef __cplusplus
}
#endif


#endif // IMAGE_STREAMER_H_INCLUDED
