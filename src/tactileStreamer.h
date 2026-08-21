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
    float data[TACTILE_STREAMING_ELEMENTS * TACTILE_STREAMING_WINDOW]; /**< Interleaved sensor samples: [elem0_t0, elem1_t0, …, elem0_t1, …]. */
    unsigned int data_size;   /**< Number of valid floats currently stored in data[]. */
    unsigned long timestamp;  /**< Microsecond host timestamp of the most recent sample in the buffer. */
};



typedef struct
{
    char shm_name[128];    /**< POSIX shared-memory object name (e.g. "tactile_frames.shm"). */
    char stream_name[128]; /**< Logical stream name used by the SHM server (e.g. "stream_tactile"). */
    struct SharedMemoryContext *context;     /**< SHM server context returned by startTactileStream(). */
    struct VideoFrame *frame;                /**< Current VideoFrame slot in the SHM ring buffer. */
    struct TactileBuffer data;               /**< Local copy of the buffer serialised into the SHM frame. */
    struct VideoFrameLocalMapping localMap;  /**< mmap mapping that backs frame->data. */
} StreamingTactileContext;


StreamingTactileContext * startTactileStream(const char * shm_name, const char * stream_name, unsigned int window, unsigned int elementsPerUnit);
int stopTactileStream(StreamingTactileContext * shm_stream);

int stream_tactile(struct VideoFrame *frame, struct TactileBuffer* data);


#ifdef __cplusplus
}
#endif


#endif // IMAGE_STREAMER_H_INCLUDED
