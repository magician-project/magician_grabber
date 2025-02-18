//Written by Ammar Qammaz a.k.a. AmmarkoV

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

static const char MagicianGrabberVersion[]="0.00";

#include "common.h"
#include "arduinoSensor.h"
#include "atiForceSensor.h"
#include "gigeCameraSensor.h"


int setOutputDirectory(GlobalConfig *cfg, const char * outputDirectory)
{
    snprintf(cfg->outputDirectory,512,"%s",outputDirectory);

    if (strcmp(outputDirectory,"./")!=0)
    {
     char makedircmd[2048]= {0};
     snprintf(makedircmd,1024,"mkdir -p %s",cfg->outputDirectory);

     int z = system(makedircmd);
                if (z==0)
                {
                    fprintf(stderr,"Output Path set to \"%s\" \n",cfg->outputDirectory);
                }
                else
                {
                    fprintf(stderr,RED "Failed setting output Path to \"%s\" \n" NORMAL,cfg->outputDirectory);
                }
    }
    return 1;
}


int main (int argc, char **argv)
{
    // Global flag for termination
    char keep_running = 1;


    char useArduino  = 0;
    char useTeensy   = 0;
    char useCamera   = 0;
    char useATIForce = 0;

    // Grabber Configurations
    GlobalConfig cfg={0};

    setOutputDirectory(&cfg, "./");
    cfg.maxTimeToGrabForInSeconds = 30; //Grab for 60 seconds


    unsigned int width = 2448;
    unsigned int height = 2048;
    unsigned int exposure = 6500; // 0 means no setting
    double       gain = 0.0;
    double       blackLevel = 0.0;
    double       frameRate = 15.0;


    //Parse command line arguments
    for (int i=0; i<argc; i++)
    {
        if ( (strcmp(argv[i],"-o")==0) || (strcmp(argv[i],"--output")==0) )
        {
            if (argc>i+1)
            { setOutputDirectory(&cfg,argv[i+1]); }
            else
            { fprintf(stderr,"Failed setting output Path, not enough arguments! \n"); }
        }
        else if (strcmp(argv[i],"--size")==0)
        {
            width  = atoi(argv[i+1]);
            height = atoi(argv[i+2]);
            fprintf(stderr,"Camera size set to %u x %u pixels \n",width,height);
        }
        else if (strcmp(argv[i],"--exposure")==0)
        {
            exposure=atoi(argv[i+1]);
            fprintf(stderr,"Exposure will be set to %u μsec \n",exposure);
        }
        else if (strcmp(argv[i],"--gain")==0)
        {
            gain=atof(argv[i+1]);
            fprintf(stderr,"Gain will be set to %f \n",gain);
        }
        else if (strcmp(argv[i],"--fps")==0)
        {
            frameRate=atof(argv[i+1]);
            fprintf(stderr,"Framerate will be set to %f Hz \n",frameRate);
        }
        else if (strcmp(argv[i],"--blacklevel")==0)
        {
            blackLevel=atof(argv[i+1]);
            fprintf(stderr,"Black Level will be set to %f μsec \n",blackLevel);
        }
        else if (strcmp(argv[i],"--time")==0)
        {
            cfg.maxTimeToGrabForInSeconds=atoi(argv[i+1]);
            fprintf(stderr,"Setting frame grab to %u \n",cfg.maxTimeToGrabForInSeconds);
        }
        else if (strcmp(argv[i],"--camera")==0)
        {
            useCamera = 1;
            fprintf(stderr,"Activating Camera\n");
        }
        else if (strcmp(argv[i],"--force")==0)
        {
            useATIForce = 1;
            fprintf(stderr,"Activating Force\n");
        }
        else if (strcmp(argv[i],"--accelerometer")==0)
        {
            useTeensy = 1;
            fprintf(stderr,"Activating Force\n");
        }
        else if (strcmp(argv[i],"--distance")==0)
        {
            useArduino = 1;
            fprintf(stderr,"Activating Arduino\n");
        }
    }


   if ( (!useArduino) && (!useTeensy) && (!useCamera) && (!useATIForce) )
   {
     fprintf(stderr,"No Input Sources selected! \n");
     fprintf(stderr,"Please use --camera --force --accelerometer --distance\n");
     exit(0);
   }




    pthread_t gigecamera_tid, arduino_tid, teensy_tid, atinetft_tid;

    // Initialize Configurations
    GiGECameraConfig camera_config     = {&cfg, "3205040", "camera.csv", width, height, exposure, gain, blackLevel, frameRate, 0, NULL, &keep_running, 0, 0, 0, 0, NULL, NULL, NULL };
    ATINetFTConfig atinetft_config     = {&cfg, "192.168.200.11",  49152, "force.csv",  NULL, &keep_running, 0, 0};
    ArduinoSerialConfig teensy_config  = {&cfg, "/dev/ttyACM1",    "accelerometer.csv", 115200, NULL, &keep_running, 0, 0};
    ArduinoSerialConfig arduino_config = {&cfg, "/dev/ttyACM2",    "controller.csv", 115200, NULL, &keep_running, 0, 0};

    // Start Threads
    if (useCamera)   {
                       pthread_create(&gigecamera_tid, NULL, gigecamera_thread, &camera_config);
                       usleep(10000); //This is the slowest to start todo handshake with worker thread
                     }

    if (useTeensy)   { pthread_create(&teensy_tid,     NULL, arduino_thread,    &teensy_config);   }
    if (useArduino)  { pthread_create(&arduino_tid,    NULL, arduino_thread,    &arduino_config);  }
    if (useATIForce) { pthread_create(&atinetft_tid,   NULL, atinetft_thread,   &atinetft_config); }

    unsigned long startTime = GetTickCountMicroseconds();
    unsigned long currentTime = startTime;
    printf("Data collection started.\n");
    // Run until flag is cleared (placeholder for user signal handling)
    while (keep_running)
    {
        // Simulate main loop work
        usleep(1000);
        currentTime = GetTickCountMicroseconds();
        unsigned long runningTimeInSeconds = (currentTime - startTime) / 1000000;


        printf("\r");
        printf(GREEN " %lu sec " NORMAL,cfg.maxTimeToGrabForInSeconds - runningTimeInSeconds );

        if (useCamera)
            {
             printf("|Cam %u %0.2fHz ",camera_config.framesCaptured, camera_config.actualFrameRate, frameRate );
             printf(" Ok %lu/Fail %lu/Under %lu",camera_config.n_completed_buffers, camera_config.n_failures,camera_config.n_underruns);
            }

        if (useTeensy)   { printf("|Teensy %lu samples",teensy_config.receivedDataFrames ); }
        if (useATIForce) { printf("|ATI %lu samples",atinetft_config.receivedDataFrames ); }
        printf("|\r");



        if (currentTime-startTime > cfg.maxTimeToGrabForInSeconds * 1000000)
        {
          fprintf(stderr,GREEN "\n\n\n\nClosing down due to timeout..\n" NORMAL);
          keep_running = 0;
          usleep(10000);
        }
    }

    // Wait for threads to finish
    if (useCamera)   { pthread_join(gigecamera_tid, NULL); }
    if (useTeensy)   { pthread_join(teensy_tid, NULL);    }
    if (useArduino)  { pthread_join(arduino_tid, NULL);    }
    if (useATIForce) { pthread_join(atinetft_tid, NULL);   }

    printf("Data collection terminated.\n");
    return 0;
}

