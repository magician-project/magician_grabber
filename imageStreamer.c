/** @file imageStreamer.c
 *  @brief This is an ARAVIS grabber ( https://github.com/AravisProject/aravis )
 *  wrapped to fit the rest of the modules and based on a standalone utility developed during early stages of the Magician Project
 *  https://github.com/AmmarkoV/aravis-c-examples/blob/main/07-streamer.c
 *  @author Ammar Qammaz (AmmarkoV)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "imageStreamer.h"

StreamingContext * startStream(const char * shm_name, const char * stream_name, unsigned int width, unsigned int height, unsigned int channels)
{
   StreamingContext * shm_stream = (StreamingContext*) malloc(sizeof(StreamingContext));
   if (shm_stream!=0)
   {
    //memset(shm_stream,0,sizeof(StreamingContext));
    //----------------------------------------------------------------------------------------
    //----------------------------------------------------------------------------------------
    //----------------------------------------------------------------------------------------
    //const char *shm_name    = "video_frames.shm";
    //const char *stream_name = "stream1";
    snprintf(shm_stream->shm_name,127,"%s",shm_name);
    snprintf(shm_stream->stream_name,127,"%s",stream_name);

    // Client process
    if (createSharedMemoryContextDescriptor(shm_stream->shm_name) == -1)
    {
        free(shm_stream);
        return NULL;
    }

    shm_stream->context = connectToSharedMemoryContextDescriptor(shm_stream->shm_name);
    if (!shm_stream->context)
    {
        free(shm_stream);
        return NULL;
    }

    shm_stream->dataAsImage.pixels   = 0;
    shm_stream->dataAsImage.bitsperpixel = 0;
    shm_stream->dataAsImage.width    = width;
    shm_stream->dataAsImage.height   = height;
    shm_stream->dataAsImage.channels = channels;


    createVideoFrameMetaData(shm_stream->context, shm_stream->stream_name, width, height, channels);
    fprintf(stderr,"Creating video stream %s, %ux%u:%u\n", shm_stream->stream_name, width, height, channels);

    shm_stream->frame = getVideoBufferPointer(shm_stream->context,shm_stream->stream_name);
    if (!shm_stream->frame)
    {
       fprintf(stderr,"Failed getting a video buffer pointer for %s \n", shm_stream->stream_name);
       free(shm_stream);
       return NULL;
    }

    //shm_stream->localMap={0};
    memset(&shm_stream->localMap,0,sizeof(struct VideoFrameLocalMapping));

    if (map_frame_shared_memory(shm_stream->frame,1) == NULL)  //We want to overwrite the frame->data because we are the client and this makes the python API easier
    {
        fprintf(stderr,"Failed map_frame_shared_memory for %s \n", shm_stream->stream_name);
        free(shm_stream);
        return NULL;
    }
   //----------------------------------------------------------------------------------------
   //----------------------------------------------------------------------------------------
   //----------------------------------------------------------------------------------------

   }
   return shm_stream;
}


int stopStream(StreamingContext * shm_stream)
{
    return 0;
}



int stream_image(struct VideoFrame *frame, struct Image* dataAsImage)
{
    if (dataAsImage==0) { fprintf(stderr,"stream_image called with no image data\n");   return 0; }
    if (frame==0)       { fprintf(stderr,"stream_image called with no video frame\n");  return 0; }

    if (startWritingToVideoBufferPointer(frame))
    {
        copy_to_shared_memory(frame, dataAsImage->pixels ,dataAsImage->image_size);
        stopWritingToVideoBufferPointer(frame);
        return 1;
    } else
    {
        fprintf(stderr,"Failed to write to video buffer\n");
    }
  return 0;
}





