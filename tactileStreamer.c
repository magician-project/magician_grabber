/** @file imageStreamer.c
 *  @brief This is an ARAVIS grabber ( https://github.com/AravisProject/aravis )
 *  wrapped to fit the rest of the modules and based on a standalone utility developed during early stages of the Magician Project
 *  https://github.com/AmmarkoV/aravis-c-examples/blob/main/07-streamer.c
 *  @author Ammar Qammaz (AmmarkoV)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tactileStreamer.h"

StreamingTactileContext * startTactileStream(const char * shm_name, const char * stream_name, unsigned int window, unsigned int elementsPerUnit)
{
   fprintf(stderr,"startTactileStream\n");
   StreamingTactileContext * shm_stream = (StreamingTactileContext*) malloc(sizeof(StreamingTactileContext));
   if (shm_stream!=0)
   {
    fprintf(stderr,"cleaning context of startTactileStream\n");
    memset(shm_stream,0,sizeof(StreamingTactileContext));
    //----------------------------------------------------------------------------------------
    //----------------------------------------------------------------------------------------
    //----------------------------------------------------------------------------------------
    //const char *shm_name    = "video_frames.shm";
    //const char *stream_name = "stream1";
    snprintf(shm_stream->shm_name,127,"%s",shm_name);
    snprintf(shm_stream->stream_name,127,"%s",stream_name);

    // Client process
    fprintf(stderr,"createSharedMemoryContextDescriptor(%s)\n",shm_stream->shm_name);
    if (createSharedMemoryContextDescriptor(shm_stream->shm_name) == -1)
    {
        free(shm_stream);
        return NULL;
    }

    fprintf(stderr,"connectToSharedMemoryContextDescriptor(%s)\n",shm_stream->shm_name);
    shm_stream->context = connectToSharedMemoryContextDescriptor(shm_stream->shm_name);
    if (!shm_stream->context)
    {
        free(shm_stream);
        return NULL;
    }


    shm_stream->data.data_size = window * elementsPerUnit * sizeof(float);

    fprintf(stderr,"Creating data stream %s, Window = %u Elements = %u\n", shm_stream->stream_name, window, elementsPerUnit);
    createGenericMetaData(shm_stream->context, shm_stream->stream_name,window*elementsPerUnit* sizeof(float));

    shm_stream->frame = getVideoBufferPointer(shm_stream->context,shm_stream->stream_name);
    if (!shm_stream->frame)
    {
       fprintf(stderr,RED "Failed getting a data buffer pointer for %s \n" NORMAL, shm_stream->stream_name);
       free(shm_stream);
       return NULL;
    }

    //shm_stream->localMap={0};
     memset(&shm_stream->localMap,0,sizeof(struct VideoFrameLocalMapping));

     if (map_frame_shared_memory(shm_stream->frame,1) == NULL)  //We want to overwrite the frame->data because we are the client and this makes the python API easier
     {
        fprintf(stderr,RED "Failed map_frame_shared_memory for %s \n" NORMAL, shm_stream->stream_name);
        free(shm_stream);
        return NULL;
     }
     //----------------------------------------------------------------------------------------
     //----------------------------------------------------------------------------------------
     //----------------------------------------------------------------------------------------

   }
   return shm_stream;
}


int stopTactileStream(StreamingTactileContext * shm_stream)
{
    return 0;
}



int stream_tactile(struct VideoFrame *frame, struct TactileBuffer* data)
{
    if (data==0)      { fprintf(stderr,"stream_tactile called with no tactile data\n");   return 0; }
    if (frame==0)     { fprintf(stderr,"stream_tactile called with no video frame\n");    return 0; }

    if (startWritingToVideoBufferPointer(frame))
    {
        copy_to_shared_memory(frame, data->data ,data->data_size);
        stopWritingToVideoBufferPointer(frame);
        return 1;
    } else
    {
        fprintf(stderr,"Failed to write to video buffer\n");
    }
  return 0;
}





