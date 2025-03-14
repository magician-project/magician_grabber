/** @file multiModalGrabber.c
 *  @brief This is a high-performance standalone grabber for the Magician EU Project
 *  It's only dependency is the Aravis 0.10 GiGE SDK ( https://github.com/AravisProject/aravis )
 *  that should be installed on the system.
 *
 *  Running the grabber to record data:
 *  ./magician_grabber --output targetDatasetDirectoryHere --all --time 30 --rt --fps 22
 *
 *  The grabber can also stream data using shared memory ( https://github.com/AmmarkoV/SharedMemoryVideoBuffers )
 *  You can do so by issuing:
 *  ./magician_grabber --camera --stream --forever
 *
 *  @author Ammar Qammaz (AmmarkoV)
 */

//Regular imports
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

//Our modules
#include "common.h"
#include "arduinoSensor.h"
#include "atiForceSensor.h"
#include "gigeCameraSensor.h"
#include "resolveUSBDevice.h"
#include "callbacks.h"

//Shared memory for streaming
#include "imageStreamer.h"
#include "sharedMemoryVideoBuffers.h"

#include "performance.h"

static const char MagicianGrabberVersion[]="0.92";

volatile sig_atomic_t stop = 0;

void handle_sigint(int sig)
{
    printf("\nCaught signal %d (Ctrl + C). Exiting gracefully (%d/3)...\n", sig, stop);
    stop += 1;

    if (stop>3)
    {
      printf("Killing process...\n");
      exit(0);
    }
}


int setOutputDirectory(GlobalConfig *cfg, const char * outputDirectory)
{
    snprintf(cfg->outputDirectory,512,"%s",outputDirectory);

    char enabledFileOutput = (strcmp(cfg->outputDirectory,"/dev/null")!=0);
    if (!enabledFileOutput)
    {
        //If there is no file output we are done now..
        return 1;
    }


    if (strcmp(outputDirectory,"./")!=0)
    {
     char makedircmd[2048]={0};
     snprintf(makedircmd,1024,"mkdir -p %.512s",cfg->outputDirectory);

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


int noOutputDirectory(GlobalConfig *cfg)
{
  setOutputDirectory(cfg,"/dev/null");
}

int process_keyboard_input(ArduinoSerialConfig * arduino_config,int key)
{
  int processed = 0;
  switch (key)
  {
      case '0': break;
      case '1': break;
      case '2': break;
      case '3': break;
      case '4': break;
      case '5': break;
      case '6': break;
      case '+': break;
      case '-': break;
  };

 //Return if keystroke processed
 return processed;
}


int main (int argc, char **argv)
{
    // Show Welcome Message
    banner(MagicianGrabberVersion);

    // Handle Ctrl+C to stop recording gracefully
    signal(SIGINT, handle_sigint);

    // Global flag for termination
    char keep_running = 1;
    char run_forever  = 0;
    unsigned char countdown    = 0;

    // Modules available to use
    char useRAM       = 0;
    char useArduino   = 0;
    char useTeensy    = 0;
    char useCamera    = 0;
    char useATIForce  = 0;
    char streamCamera = 0;
    char calculateTactileFeatures    = 0;

    // Grabber Configurations
    GlobalConfig cfg={0};
    setOutputDirectory(&cfg, "./");
    cfg.maxTimeToGrabForInSeconds = 30; //Grab for 30 seconds by default

    // Camera Default settings
    unsigned int width      = 2448;
    unsigned int height     = 2048;
    unsigned int exposure   = 5500; // 0 means no setting
    double       gain       = 0.0;
    double       blackLevel = 0.0;
    double       frameRate  = 10.0; //Each image is 4.5MB,
    //this framerate writes 45MB/sec to disk which is a sane value
    //use --ram to store data on a tmpfs/ for higher speeds
    //use --rt to elevate priority for higher speeds

    // Arduino commands
    char arduinoUseRoundLight[]    = {"r\n"};
    char arduinoUseDistanceLight[] = {"a\n"};
    char * arduinoExtraCommand = arduinoUseRoundLight; //0 Or Always set round lights on

    //Parse command line arguments
    for (int i=0; i<argc; i++)
    {
        if (strcmp(argv[i], "--help") == 0)
        {
         print_help();
         exit(0);
        } else
        if ( (strcmp(argv[i],"-o")==0) || (strcmp(argv[i],"--output")==0) )
        {
            if (argc>i+1)
            { setOutputDirectory(&cfg,argv[i+1]); }
            else
            { fprintf(stderr,"Failed setting output Path, not enough arguments! \n"); }
        }
        else if (strcmp(argv[i],"--nooutput")==0)
        {
            noOutputDirectory(&cfg);
            fprintf(stderr,"File output disabled\n");
        }
        else if (strcmp(argv[i],"--countdown")==0)
        {
            countdown = (unsigned char) atoi(argv[i+1]);
            fprintf(stderr,"Will perform countdown before starting\n");
        }
        else if (strcmp(argv[i],"--ram")==0)
        {
            useRAM = 1;
            fprintf(stderr,"Will use RAM to store data\n");
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
            if (frameRate>10)
            {
              fprintf(stderr,"Consider using --ram to write to a tmpfs to support this framerate without frame drops!\n");
            }
        }
        else if (strcmp(argv[i],"--blacklevel")==0)
        {
            blackLevel=atof(argv[i+1]);
            fprintf(stderr,"Black Level will be set to %f μsec \n",blackLevel);
        }
        else if (strcmp(argv[i],"--time")==0)
        {
            run_forever=0;
            cfg.maxTimeToGrabForInSeconds=atoi(argv[i+1]);
            fprintf(stderr,"Setting frame grab to %lu \n",cfg.maxTimeToGrabForInSeconds);
        }
        else if (strcmp(argv[i],"--forever")==0)
        {
            run_forever=1;
            fprintf(stderr,"Running forever..\n");
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
        else if (strcmp(argv[i],"--features")==0)
        {
            calculateTactileFeatures = 1;
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
        else if (strcmp(argv[i],"--dlight")==0)
        {
            arduinoExtraCommand = arduinoUseDistanceLight;
            fprintf(stderr,"Using Lighting based on distance\n");
        }
        else if (strcmp(argv[i],"--rlight")==0)
        {
            arduinoExtraCommand = arduinoUseRoundLight;
            fprintf(stderr,"Using Lighting based on round robin\n");
        }
        else if (strcmp(argv[i],"--rt")==0)
        {
            fprintf(stderr,"Trying to set real-time priority\n");

            if (elevate_nice_priority(-20))
            //if (set_process_nice(-10))
            //if ( set_realtime_thread_priority() )
                           { drop_privileges(0); } //drop_privileges("nobody");
        }
        else if (strcmp(argv[i],"--all")==0)
        {
            useArduino = 1;
            useTeensy = 1;
            useATIForce = 1;
            useCamera = 1;
            //calculateTactileFeatures = 1;
            fprintf(stderr,"Activating All Devices\n");
        }
        else if (strcmp(argv[i],"--stream")==0)
        {
            streamCamera = 1;
            useCamera    = 1;
            useArduino   = 1;
            run_forever  = 1;
            fprintf(stderr,"Streaming camera data to shared memory\n");
            noOutputDirectory(&cfg);
            fprintf(stderr,"File output disabled, use --output with a later command to re-enable\n");
        }
        else if (strcmp(argv[i],"--scan")==0)
        {
          fprintf(stderr,"Activating Arduino\n");
          mainT();
          exit(0);
        }



    } // Process commandline input


   if ( (!useArduino) && (!useTeensy) && (!useCamera) && (!useATIForce) )
   {
     fprintf(stderr,"No Input Sources selected! \n");
     fprintf(stderr,"Please use --camera --force --accelerometer --distance\n");
     exit(0);
   }


   if (useRAM)
   {
       snprintf(cfg.outputDirectoryOriginal,1024,"%s",cfg.outputDirectory);
       int i = system("sudo mkdir tmpfs");
       if (i!=0)  { fprintf(stderr,RED "Failed creating a tmpfs directory to mount tmpfs \n" NORMAL); }

       i = system("sudo mount -t tmpfs -o size=4G tmpfs tmpfs/");
       if (i!=0)  { fprintf(stderr,RED "Failed creating a tmpfs mount.. :(\n" NORMAL); return 1; }
       snprintf(cfg.outputDirectory,1024,"%s","tmpfs/");
   }


   if (countdown!=0)
   {
      fprintf(stderr,"Performing initial countdown : ");
      for (int i=0; i<countdown; i++)
       {
         usleep(1000000); //1 sec
         fprintf(stderr,".");
       }
      fprintf(stderr,"\n");
   }


    //Record time that acquisition started (this will be considered as timestamp 0 from now on)
    unsigned long acquisitionStartTime = GetTickCountMicroseconds();


    pthread_t gigecamera_tid, arduino_tid, teensy_tid, atinetft_tid;


    // Initialize Configurations
    //To debug aravis connection use : arv-camera-test-0.10  -d stream
    GiGECameraConfig camera_config     = {&cfg, "3205040", "camera.csv", width, height, exposure, gain, blackLevel, frameRate, 0, NULL, &keep_running,0 , 0, 0, 0, 0, NULL, NULL, NULL, NULL };
    ATINetFTConfig atinetft_config     = {&cfg, "192.168.200.11",  49152, "force.csv",  NULL, &keep_running,0 , 0, 0, 0.0, NULL};
    ArduinoSerialConfig teensy_config  = {&cfg, "/dev/ttyACM1",    "accelerometer.csv", 115200, NULL, &keep_running, 0, NULL , 0, 0, 0.0, NULL};
    ArduinoSerialConfig arduino_config = {&cfg, "/dev/ttyACM0",    "controller.csv",    115200, NULL, &keep_running, 0, arduinoExtraCommand, 0, 0, 0.0, NULL};

    //Try to make arduino wake up correctly
    //system("stty -F /dev/ttyACM0 115200 raw -echo");
    //system("stty -F /dev/ttyACM1 115200 raw -echo");

    if (calculateTactileFeatures)
    {
        teensy_config.callback   = (void*) accelerometer_callback;
        //atinetft_config.callback = (void*) force_callback;
    }

    StreamingContext * streaming_context=0;

    if ( (streamCamera) && (useCamera) )
                         {
                           fprintf(stderr,"Starting stream..\n");
                           //We transport the raw sensor as 1 channel! (hence the 1 in next line)
                           streaming_context = startStream("video_frames.shm", "stream1", width, height, 1);
                           camera_config.camera_shm_stream = (void*) streaming_context;
                           //fprintf(stderr,"Main Thread shm=%p\n",streaming_context);
                           //fprintf(stderr,"Main Thread #2 shm=%p\n",camera_config.camera_shm_stream);
                           if (camera_config.camera_shm_stream==NULL)
                           {
                               fprintf(stderr,"Failed to start streaming to shared memory!\n");
                               exit(1);
                           }

                           if (streaming_context->frame==NULL)
                           {
                               fprintf(stderr,"Failed to establish video frame!\n");
                               exit(1);
                           }
                         }

    //Arduino takes some time to powerup
    if (useArduino)  { pthread_create(&arduino_tid,    NULL, arduino_thread,    &arduino_config);  }
    if (useTeensy)   {
                      /*
                      char *teensy_port = find_teensy_port();
                      if (teensy_port) { fprintf(stderr,GREEN "Teensy found on: %s\n" NORMAL, teensy_port); } else
                                       { fprintf(stderr,RED "Teensy port not found\n" NORMAL); exit(1); }*/
                        pthread_create(&teensy_tid,    NULL, arduino_thread,    &teensy_config);
                     }

    // Start Threads
    if (useCamera)   {
                       pthread_create(&gigecamera_tid, NULL, gigecamera_thread, &camera_config);
                       fprintf(stderr,"Waiting for camera to wake up ..\n");

                       //This is the most complex loop to start
                       //This is a busy wait but since it is only for a
                       //few seconds only on the start and makes code easier
                       //it is justified :)
                       unsigned int timeCheck = 0;
                       while (!camera_config.running)
                       {
                         fprintf(stderr,".");
                         usleep(10000);
                         timeCheck+=1;

                         if (timeCheck>300)
                         {
                           fprintf(stderr,"\nCamera timed-out (%u ticks)..\n",timeCheck);
                           keep_running = 0; //<- this will make the program exit
                           break;
                         }
                       }

                       if (camera_config.running)
                          { fprintf(stderr,"\nCamera online (%u ticks)..\n",timeCheck); }

                      }

    if (useATIForce) { pthread_create(&atinetft_tid,   NULL, atinetft_thread,   &atinetft_config); }

    //Enable keystrokes to be received without blocking execution
    //set_nonblocking_mode();

    unsigned long startTime = GetTickCountMicroseconds();
    unsigned long currentTime = startTime;
    printf("Data collection started.\n");
    // Run until flag is cleared (placeholder for user signal handling)
    while (keep_running)
    {
        // Simulate main loop work
        usleep(1000);

        int key = 0;// get_keystroke();

        if (key == 'q')
        {  // Stop when 'q' is pressed
          fprintf(stderr, "\nUser requested exit (pressed 'q')\n");
          keep_running = 0;
          break;
        } else
        {
          process_keyboard_input(&arduino_config,key);
        }

        if (stop)
        {  // Stop when Ctrl+C is received
           fprintf(stderr,"\nTerminating because of signal\n");
           keep_running = 0;
           break;
        }

        currentTime = GetTickCountMicroseconds();
        unsigned long runningTimeInSeconds = (currentTime - startTime) / 1000000;


        printf("\r");
        //-----------------------------------------------------------------------------------------------------------------

        if (streamCamera) { broadcasting(camera_config.framesCaptured); }
        if (run_forever)  { printf(GREEN " %lu sec " NORMAL, runningTimeInSeconds ); } else
                          {
                           printf(GREEN " %lu sec " NORMAL,cfg.maxTimeToGrabForInSeconds - runningTimeInSeconds );
                           progress_bar(runningTimeInSeconds,cfg.maxTimeToGrabForInSeconds);
                          }

        if (useCamera)
            {
             printf("|Cam %lu %0.2fHz ",camera_config.framesCaptured, camera_config.actualFrameRate);
             printf(" Ok %lu/Fail %lu/Under %lu",camera_config.n_completed_buffers, camera_config.n_failures,camera_config.n_underruns);
            }

        if (useArduino)  { printf("|Arduino %0.2fHz/%lu samples",arduino_config.Hz, arduino_config.receivedDataFrames ); }
        if (useTeensy)   { printf("|Teensy %0.2fHz/%lu samples",teensy_config.Hz, teensy_config.receivedDataFrames ); }
        if (useATIForce) { printf("|ATI %0.2fHz/%lu samples",atinetft_config.Hz, atinetft_config.receivedDataFrames); }
        //-----------------------------------------------------------------------------------------------------------------
        printf("|   \r");



        if ( (!run_forever) && (currentTime-startTime > cfg.maxTimeToGrabForInSeconds * 1000000) )
        {
          fprintf(stderr,GREEN "\n\n\n\nSuccesfully Completed recording time..\n" NORMAL);
          keep_running = 0;
          usleep(10000);
        }
    }

    // Wait for threads to finish
    if (useTeensy)   { fprintf(stderr,"Releasing Teensy\n");  pthread_join(teensy_tid, NULL);     }
    if (useArduino)  { fprintf(stderr,"Releasing Arduino\n"); pthread_join(arduino_tid, NULL);    }
    if (useATIForce) { fprintf(stderr,"Releasing ATI\n");     pthread_join(atinetft_tid, NULL);   }

    printf("\n\n");

   if (useRAM)
   {
       printf("\n\nWriting Data from RAM to Disk..\n");
       printf("This will take some time..\n");
       char command[4096]={0};

       //  mv tmpfs/* %s/  OR  rsync --info=progress2  -av tmpfs/ %s/
       snprintf(command,4096,"du -sh tmpfs/ && rsync --info=progress2 -a --remove-source-files tmpfs/ %s/ && find tmpfs/ -type d -empty -delete && sync && sleep 35 && sudo umount tmpfs/ && rmdir tmpfs/",cfg.outputDirectoryOriginal);
       int i = system(command);
       if (i!=0)  { fprintf(stderr,RED "Failed copying data from RAM to Disk, check folder tmpfs/ for surviving data.. :(\n" NORMAL); }
   }


    //Record time that acquisition started (this will be considered as timestamp 0 from now on)
    unsigned long elapsedAcquisitionTime = GetTickCountMicroseconds() - acquisitionStartTime;
    printf("Data collection terminated after %0.2f seconds\n", (double) elapsedAcquisitionTime / 1000000.0);


    usleep(1000);
    exit(0); //Camera spawns another thread so there is a problem making it join again..
    if (useCamera)   { fprintf(stderr,"Releasing Camera\n");  pthread_join(gigecamera_tid, NULL); }


    return 0;
}

