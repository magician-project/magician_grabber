/* SPDX-License-Identifier:Unlicense */

/* Aravis header */
#include <arv.h>

/* Standard headers */
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "gigeCameraSensor.h"
#include "imageStreamer.h"

volatile sig_atomic_t termination_requested = 0;

void sigterm_handler(int signum)
{
    termination_requested = 1;
}

struct Settings
{
    unsigned int delay,maxFramesToGrab;
    unsigned int exposure; // 0 means no setting
    double       gain;
    double       blackLevel;
    double       frameRate;
    char * tickCommand;
};

#include <sys/time.h>
#include <unistd.h>
#include <time.h>

#define EPOCH_YEAR_IN_TM_YEAR 1900

int writeSettings(const char * filename,struct Settings * settings)
{
    FILE * fp = fopen(filename,"w");
    if (fp!=0)
    {
        fprintf(fp,"{\n\"delay\": %u,\n",settings->delay);
        fprintf(fp,"\"maxFramesToGrab\": %u,\n",settings->maxFramesToGrab);
        fprintf(fp,"\"exposure\": %u,\n",settings->exposure);
        fprintf(fp,"\"blackLevel\": %f,\n",settings->blackLevel);
        fprintf(fp,"\"gain\": %f,\n",settings->gain);
        fprintf(fp,"\"frameRate\": %f,\n",settings->frameRate);
        fprintf(fp,"\"tickCommand\": \"%s\"\n}\n",settings->tickCommand);
        fclose(fp);
        return 1;
    }
    return 0;
}

unsigned int simplePowPPMG(unsigned int base,unsigned int exp)
{
    if (exp==0) return 1;
    unsigned int retres=base;
    unsigned int i=0;
    for (i=0; i<exp-1; i++)
    {
        retres*=base;
    }
    return retres;
}

int WritePPMG(const char * filename,struct Image * pic)
{
    //fprintf(stderr,"saveRawImageToFile(%s) called\n",filename);
    if (pic==0)
    {
        return 0;
    }
    if ( (pic->width==0) || (pic->height==0) || (pic->channels==0) || (pic->bitsperpixel==0) )
    {
        fprintf(stderr,"saveRawImageToFile(%s) called with zero dimensions ( %ux%u %u channels %u bpp\n",filename,pic->width, pic->height,pic->channels,pic->bitsperpixel);
        return 0;
    }
    if(pic->pixels==0)
    {
        fprintf(stderr,"saveRawImageToFile(%s) called for an unallocated (empty) frame , will not write any file output\n",filename);
        return 0;
    }
    if (pic->bitsperpixel>16)
    {
        fprintf(stderr,"PNM does not support more than 2 bytes per pixel..!\n");
        return 0;
    }

    FILE *fd=0;
    fd = fopen(filename,"wb");

    if (fd!=0)
    {
        unsigned int n;
        if (pic->channels==3) fprintf(fd, "P6\n");
        else if (pic->channels==1) fprintf(fd, "P5\n");
        else
        {
            fprintf(stderr,"Invalid channels arg (%u) for SaveRawImageToFile\n",pic->channels);
            fclose(fd);
            return 1;
        }

        fprintf(fd, "%d %d\n%u\n", pic->width, pic->height, simplePowPPMG(2,pic->bitsperpixel)-1);

        float tmp_n = (float) pic->bitsperpixel/ 8;
        tmp_n = tmp_n *  pic->width * pic->height * pic->channels ;
        n = (unsigned int) tmp_n;

        fwrite(pic->pixels, 1, n, fd);
        fflush(fd);
        fclose(fd);
        return 1;
    }
    else
    {
        fprintf(stderr,"SaveRawImageToFile could not open output file %s\n",filename);
        return 0;
    }
    return 0;
}


int gigecamera_startStream(GiGECameraConfig * context)
{
    fprintf(stderr,"Starting camera stream");
    // Set up SIGTERM signal handler
    struct sigaction action;
    action.sa_handler = sigterm_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGTERM, &action, NULL);

    //guint64 n_completed_buffers=0, n_failures=0, n_underruns=0;

    GlobalConfig *cfg = context->global;


    //unsigned int i=0;
    unsigned int ARV_VIEWER_N_BUFFERS=10;
    struct Settings settings= {0};
    struct Image dataAsImage= {0};

    //Just use settings
    settings.blackLevel  =  context->blackLevel;
    settings.gain        =  context->gain;
    settings.frameRate   =  context->frameRate;
    settings.exposure    =  context->exposure;

    char forceDims = 0;
    char refreshDimsOnEachFrame = 1;

    /* Mandatory glib type system initialization */
    //arv_g_type_init ();

    ArvCamera *camera = NULL;
    //ArvStream *stream = NULL;
    GError *error = NULL;

    /* Connect to the first available camera */
    printf ("Trying to connect to camera \n");
    camera = arv_camera_new (NULL, &error);
    context->camera = (void*) camera;

    if ( (camera == NULL) && (error != NULL) )
    {
        fprintf (stderr,"No camera found, terminating streamer\n");
        exit(1);
    }
    printf ("Found a device ..\n");

    if (ARV_IS_CAMERA (camera))
    {
        ArvStream *stream = NULL;

        printf ("Found camera '%s'\n", arv_camera_get_model_name (camera, NULL));

        arv_camera_set_acquisition_mode (camera, ARV_ACQUISITION_MODE_CONTINUOUS, &error);

        if (error == NULL)
        {
            /* Create the stream object without callback */
            stream = arv_camera_create_stream (camera, NULL, NULL, NULL, &error);
            context->stream = (void*) stream;

            /*
            g_object_set (stream,
                      "packet-timeout", 3 * 1000,
                      "frame-retention", 25 * 1000,
                      "packet-request-ratio", 0.75,
                      NULL);*/

        }

        if (ARV_IS_STREAM (stream))
        {
            int i;
            size_t payload;

            /* Retrieve the payload size for buffer creation */
            payload = arv_camera_get_payload (camera, &error);
            if (error == NULL)
            {
                /* Insert some buffers in the stream buffer pool */
                for (i = 0; i < ARV_VIEWER_N_BUFFERS; i++)
                    arv_stream_push_buffer (stream, arv_buffer_new (payload, NULL));
            }


            arv_stream_set_emit_signals (stream, TRUE);
            arv_stream_create_buffers(stream, ARV_VIEWER_N_BUFFERS, NULL, NULL, NULL);


            if (error == NULL)
                /* Start the acquisition */
                arv_camera_set_acquisition_mode (camera, ARV_ACQUISITION_MODE_CONTINUOUS, NULL);
            arv_camera_start_acquisition (camera, &error);

            if (settings.exposure!=0)
            {
                arv_camera_set_exposure_time(camera, settings.exposure, NULL);
            }
            if (settings.gain!=0.0)
            {
                arv_camera_set_gain (camera, settings.gain, NULL);
            }
            if (settings.blackLevel!=0.0)
            {
                arv_camera_set_black_level(camera, settings.blackLevel, NULL);
            }
            if (settings.frameRate!=0.0)
            {
                arv_camera_set_frame_rate (camera, settings.frameRate, NULL);
            }


            if (error == NULL)
            {
                if ( (!refreshDimsOnEachFrame)&& (!forceDims) )
                {
                    //Poll dims so that we know them in advance if we dont want to get them from each buffer, and we dont want to force a specific dimension
                    int minvalue=0,maxvalue=0;
                    arv_camera_get_width_bounds(camera,&minvalue,&maxvalue,NULL);
                    dataAsImage.width  = (unsigned int) maxvalue;
                    arv_camera_get_height_bounds(camera,&minvalue,&maxvalue,NULL);
                    dataAsImage.height  = (unsigned int) maxvalue;
                }

                if ( (!refreshDimsOnEachFrame) || (forceDims) )
                {
                    //Attempt to setup region if there is no autorefresh of dimensions, or we want to force a specific dimension
                    arv_camera_set_region(camera,0,0,dataAsImage.width,dataAsImage.height,NULL); //Use full sensor area
                }

                //const void *data = NULL;
                char filename[1025]= {0};
                //unsigned int frameNumber = 0;
                //unsigned int brokenFrameNumber = 0;
                //ArvBuffer *buffer;

                snprintf(filename,1024,"%.512s/info.json",cfg->outputDirectory);
                writeSettings(filename,&settings);

                //unsigned long startTime = GetTickCountMicroseconds();

                //unsigned long startGrab, endGrab;
                //unsigned long microsecondsGrab;
                unsigned long timeToSleepToWaitFor1Frame = 0;

                if (settings.frameRate!=0.0)
                {
                    fprintf(stderr,"Waiting initial period to buffer at least one frame..\n");
                    timeToSleepToWaitFor1Frame = 1000000 / settings.frameRate;
                    usleep(timeToSleepToWaitFor1Frame);
                }

              return 0;
           }
       }
    }
   return 1;
}



int gigecamera_stopStream(GiGECameraConfig * context)
{
  context->running = 0;

  GError *error = NULL;

  ArvCamera *camera = context->camera;
  ArvStream *stream = context->stream;

  /* Stop the acquisition */
  arv_stream_set_emit_signals (stream, FALSE);
  arv_camera_stop_acquisition (camera, &error);

  /* Destroy the stream object */
  g_clear_object (&stream);

  /* Destroy the camera instance */
  g_clear_object (&camera);

  if (error != NULL)
    {
        /* En error happened, display the correspdonding message */
        printf ("Error: %s\n", error->message);
        return EXIT_FAILURE;
    }

    printf("\n\nDone\n");
    printf("Camera Summary : Ok %lu/Fail %lu/Under %lu    \n", context->n_completed_buffers, context->n_failures, context->n_underruns);

    if (context->exposure!=0)
    {
        printf("Exposure time was %u\n",context->exposure);
        printf("This is equivalent to %0.2f FPS\n",(float) 1000000.0/context->exposure);
    }

    context->camera=NULL;
    context->stream=NULL;
    return EXIT_SUCCESS;
}



void *gigecamera_thread(void *arg)
{
    GiGECameraConfig *config = (GiGECameraConfig *)arg;
    GlobalConfig *cfg = config->global;

    gigecamera_startStream(config);

    char enabledFileOutput = (strcmp(cfg->outputDirectory,"/dev/null")!=0);

    char fullCSVOutputPath[2048]={0};
    snprintf(fullCSVOutputPath,2048,"%s/%s",cfg->outputDirectory,config->csv_name);


    if (enabledFileOutput)
    {
     config->csv_file = fopen(fullCSVOutputPath, "w");
     if (!config->csv_file)
      {
        perror("Failed to open GiGE camera log file");
        return NULL;
      }
     fprintf(config->csv_file,"timestamp,frameID\n");
    } else
    {
     config->csv_file = NULL;
    }

    //ArvCamera *camera = config->camera;
    ArvStream *stream = config->stream;

    unsigned long startGrab, endGrab;
    unsigned long microsecondsGrab;
    unsigned long timeToSleepToWaitFor1Frame = 0;
    ArvBuffer *buffer = NULL;

    char refreshDimsOnEachFrame = 1;

    const void *data = NULL;
    char filename[2048]= {0};
    unsigned int frameNumber = 0;
    unsigned int brokenFrameNumber = 0;

    unsigned int i=0;
    //unsigned int ARV_VIEWER_N_BUFFERS=10;
    struct Settings settings= {0};
    struct Image dataAsImage= {0};
    dataAsImage.width  = config->width;
    dataAsImage.height = config->height;


    unsigned long startTime = GetTickCountMicroseconds();

    StreamingContext * shm_stream = NULL;
    if (config->camera_shm_stream!=NULL)
          {
            shm_stream = (StreamingContext *) config->camera_shm_stream;
            if (shm_stream->frame == NULL)
            {
              fprintf(stderr,"\nNo video buffer for streaming shm=%p\n",shm_stream);
              exit(1);
            }
          }

    while (*config->keep_running)
                  {
                    startGrab = GetTickCountMicroseconds();
                    buffer = arv_stream_pop_buffer (stream);
                    if (ARV_IS_BUFFER(buffer))
                    {
                        config->running = 1;
                        if (refreshDimsOnEachFrame)
                        {
                            dataAsImage.width        = arv_buffer_get_image_width (buffer);
                            dataAsImage.height       = arv_buffer_get_image_height(buffer);
                        }

                        if ((dataAsImage.width!=0) && (dataAsImage.height!=0))
                        {
                            size_t size;
                            data = arv_buffer_get_image_data(buffer,&size);
                            //printf ("Size =  %lu\n",size);
                            dataAsImage.pixels       = data;

                            dataAsImage.channels     = 1;
                            dataAsImage.bitsperpixel = 8;
                            dataAsImage.image_size   = dataAsImage.width  * dataAsImage.height * dataAsImage.channels;
                            dataAsImage.timestamp    = i;

                            /* Display some informations about the retrieved buffer */
                            //printf ("Acquired %d×%d buffer\n",dataAsImage.width,dataAsImage.height);
                            unsigned long endTime = GetTickCountMicroseconds();

                            arv_stream_get_statistics (stream, &config->n_completed_buffers, &config->n_failures, &config->n_underruns);
                            //float frameRate = arv_camera_get_frame_rate (camera, NULL);
                            config->actualFrameRate = (double) frameNumber / ((endTime-startTime)/1000000);
                            //printf("\r %u Frames Grabbed (%u dropped) - @ %0.2f FPS (set %0.2f) ",frameNumber,brokenFrameNumber, config->actualFrameRate, frameRate );
                            //printf("Ok %lu/Fail %lu/Under %lu    \r",config->n_completed_buffers,config->n_failures,config->n_underruns);


                            if (shm_stream!=NULL)
                                       { stream_image(shm_stream->frame,&dataAsImage); }

                            if (enabledFileOutput)
                            {
                            //Dump to accompanying file
                             fprintf(config->csv_file, "%lu,",GetTickCountMicroseconds());
                             fprintf(config->csv_file, "%u\n",frameNumber);
                             snprintf(filename,1024,"%.512s/colorFrame_0_%05u.pnm", cfg->outputDirectory, frameNumber);
                             WritePPMG(filename,&dataAsImage);
                            }

                            frameNumber = frameNumber+1;
                            config->framesCaptured = frameNumber; // Update as soon as it is done
                        }
                        else
                        {
                            brokenFrameNumber = brokenFrameNumber + 1;
                        }

                        /* Don't destroy the buffer, but put it back into the buffer pool */
                        arv_stream_push_buffer (stream, buffer);
                    }
                    else
                    {
                        usleep(timeToSleepToWaitFor1Frame);
                    }

                    endGrab = GetTickCountMicroseconds();

                    if (settings.frameRate!=0.0)
                    {
                        //Enforce framrates to prevent buffer underrun
                        microsecondsGrab = endGrab - startGrab;
                        // Calculate the time to usleep to achieve target framerate
                        unsigned long targetMicroseconds = 1000000 / settings.frameRate;
                        if (microsecondsGrab < targetMicroseconds)
                        {
                            usleep(targetMicroseconds - microsecondsGrab);
                        }
                    }//We have a framerate set
                   //usleep(10);
                } //While loop

    if (enabledFileOutput)
           { fclose(config->csv_file); }

    if (shm_stream!=NULL)
           { stopStream(shm_stream); }

    gigecamera_stopStream(config);
    return NULL;
}




