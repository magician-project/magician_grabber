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
#include "tactileFeatures.h"

//Shared memory for streaming
#include "imageStreamer.h"
#include "sharedMemoryVideoBuffers.h"

#include "performance.h"

//Callback defaults
#include "callbacks.h"

static const char MagicianGrabberVersion[]="0.94";

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


//These is a callback that triggers the next light
static int camera_callback_next_light(GiGECameraConfig *config, unsigned long timestamp, struct Image *dataAsImage)
{
    if (config!=NULL)
    {
      if (config->global!=NULL)
      {
        ArduinoSerialConfig * arduino = (ArduinoSerialConfig *) config->global->arduino_cfg;
        if (arduino!=0)
         {
          return arduino_signalNewFrame(arduino);
         }
      }
    }
    return 0;
}



int main (int argc, char **argv)
{
    // Show Welcome Message
    banner(MagicianGrabberVersion);

    // Handle Ctrl+C to stop recording gracefully
    signal(SIGINT, handle_sigint);

    // Grabber Configurations
    GlobalConfig cfg={0};
    //Initially everything is set to zero

    //-------------------------------
    setOutputDirectory(&cfg, "./");
    cfg.maxTimeToGrabForInSeconds = 30; //Grab for 30 seconds by default

    //Set defaults for arduino/teensy
    snprintf(cfg.arduinoPath,128,"%s","/dev/ttyUSB0");
    snprintf(cfg.teensyPath,128,"%s","/dev/ttyACM0");

    // Global flag for termination
    cfg.keep_running = 1;
    cfg.run_forever  = 0;
    cfg.countdown    = 0;


    //fprintf(stderr,"Will manually trigger light changes!\n");
    //By default try to manually trigger light
    cfg.manual_trigger_light = 1;

    // Modules available to use
    cfg.interceptKeyboard = 1;
    cfg.useRAM       = 0;
    cfg.useArduino   = 0;
    cfg.useTeensy    = 0;
    cfg.useCamera    = 0;
    cfg.useATIForce  = 0;
    cfg.streamCamera = 0;

    #if TACTILE
    cfg.calculateTactileFeatures    = 0;
    #endif // TACTILE

    // Camera Default settings
    cfg.width      = 2448;
    cfg.height     = 2048;
    cfg.exposure   = 5500; // 0 means no setting
    cfg.gain       = 0.0;
    cfg.blackLevel = 0.0;
    cfg.frameRate  = 10.0; //Each image is 4.5MB,
    //this framerate writes 45MB/sec to disk which is a sane value
    //use --ram to store data on a tmpfs/ for higher speeds
    //use --rt to elevate priority for higher speeds

    // Arduino commands
    cfg.arduinoExtraCommand = arduinoUseRoundLight; //0 Or Always set round lights on

    //=============================================================
    //  See common.h -> parse_arguments() or run with --help
    //                 for all available options
    //=============================================================
    parse_arguments(&cfg,argc,argv);
    //=============================================================
    //=============================================================

   if ( (!cfg.useArduino) && (!cfg.useTeensy) && (!cfg.useCamera) && (!cfg.useATIForce) )
   {
     fprintf(stderr,"No Input Sources selected! \n");
     fprintf(stderr,"Please use --camera --force --accelerometer --distance\n");
     exit(0);
   }

   if (cfg.useRAM)
   {
       snprintf(cfg.outputDirectoryOriginal,1024,"%s",cfg.outputDirectory);
       int i = system("sudo mkdir tmpfs");
       if (i!=0)  { fprintf(stderr,RED "Failed creating a tmpfs directory to mount tmpfs \n" NORMAL); }

       i = system("sudo mount -t tmpfs -o size=4G tmpfs tmpfs/");
       if (i!=0)  { fprintf(stderr,RED "Failed creating a tmpfs mount.. :(\n" NORMAL); return 1; }
       snprintf(cfg.outputDirectory,1024,"%s","tmpfs/");
   }


   if (cfg.countdown!=0)
   {
      fprintf(stderr,"Performing initial countdown : ");
      for (int i=0; i<cfg.countdown; i++)
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
    ATINetFTConfig atinetft_config     = {&cfg, "192.168.200.11",  49152, "tactile/force.csv",  NULL, &cfg.keep_running,0 , 0, 0, 0.0, NULL};
    ArduinoSerialConfig teensy_config  = {&cfg, "copy from cfg later",    "tactile/accelerometer.csv", 115200, NULL, &cfg.keep_running, 0, NULL , 0, 0, 0.0, NULL};
    ArduinoSerialConfig arduino_config = {&cfg, "copy from cfg later",    "controller.csv",    115200, NULL, &cfg.keep_running, 0, cfg.arduinoExtraCommand, 0, 0, 0.0, NULL};
    GiGECameraConfig camera_config     = {&cfg, "3205040", "camera.csv", cfg.width, cfg.height, cfg.exposure, cfg.gain, cfg.blackLevel, cfg.frameRate, 0, NULL, &cfg.keep_running,0 , 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL };

    //Make arduino_cfg visible!
    cfg.arduino_cfg = (void*) &arduino_config;

    //Make each camera frame trigger next light!
    if (cfg.manual_trigger_light)
    {
     camera_config.callback = (void*) camera_callback_next_light;
    }

    //Copy teensy/arduino port
    snprintf(teensy_config.port_name,128,"%s",cfg.teensyPath);
    snprintf(arduino_config.port_name,128,"%s",cfg.arduinoPath);

    //Try to make arduino wake up correctly
    //system("stty -F /dev/ttyACM0 115200 raw -echo");
    //system("stty -F /dev/ttyACM1 115200 raw -echo");

    #if TACTILE
    //printf("Tactile Feature Procsessor version : %s\n",TactileFeatureProcessorVersion);
    cfg.interceptKeyboard = 0; //Do not intercept keyboard until crashes are resolved
    cfg.calculateTactileFeatures = (cfg.useTeensy) && (cfg.useATIForce);
    pthread_t tactile_tid;
    struct TactileDataState  tactile_config = {&cfg, &cfg.keep_running, 0, 0};
    if (cfg.calculateTactileFeatures)
    {
        teensy_config.callback   = (void*) addTactileAccelerometerReading;//accelerometer_callback;
        atinetft_config.callback = (void*) addTactileForceReading;//force_callback;
        pthread_create(&tactile_tid,    NULL, tactile_thread,    &tactile_config);
    }
    #endif // TACTILE


    StreamingContext * streaming_context=0;

    if ( (cfg.streamCamera) && (cfg.useCamera) )
                         {
                           fprintf(stderr,"Starting stream..\n");
                           //We transport the raw sensor as 1 channel! (hence the 1 in next line)
                           streaming_context = startStream("video_frames.shm", "stream1", cfg.width, cfg.height, 1);
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
    if (cfg.useArduino)  { pthread_create(&arduino_tid,    NULL, arduino_thread,    &arduino_config);  }
    if (cfg.useTeensy)   {
                           /*
                              char *teensy_port = find_teensy_port();
                              if (teensy_port) { fprintf(stderr,GREEN "Teensy found on: %s\n" NORMAL, teensy_port); } else
                                               { fprintf(stderr,RED "Teensy port not found\n" NORMAL); exit(1); }*/
                           pthread_create(&teensy_tid,    NULL, arduino_thread,    &teensy_config);
                         }

    // Start Threads
    if (cfg.useCamera)   {
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
                               cfg.keep_running = 0; //<- this will make the program exit
                               break;
                             }
                           }

                           if (camera_config.running)
                              { fprintf(stderr,"\nCamera online (%u ticks)..\n",timeCheck); }
                          }

    if (cfg.useATIForce) { pthread_create(&atinetft_tid,   NULL, atinetft_thread,   &atinetft_config); }

    //Enable keystrokes to be received without blocking execution
    int key = 0;
    if (cfg.interceptKeyboard)
          { set_nonblocking_mode(); }

    unsigned long startTime = GetTickCountMicroseconds();
    unsigned long currentTime = startTime;
    printf("Data collection started.\n");
    // Run until flag is cleared (placeholder for user signal handling)
    while (cfg.keep_running)
    {
        // Simulate main loop work
        usleep(1000);

        if (cfg.interceptKeyboard)
             { key = get_keystroke(); }

        if (key == 'q')
        {  // Stop when 'q' is pressed
          fprintf(stderr, "\nUser requested exit (pressed 'q')\n");
          cfg.keep_running = 0;
          break;
        } else
        {
          process_keyboard_input(&arduino_config,key);
        }

        if (stop)
        {  // Stop when Ctrl+C is received
           fprintf(stderr,"\nTerminating because of signal\n");
           cfg.keep_running = 0;
           break;
        }

        currentTime = GetTickCountMicroseconds();
        unsigned long runningTimeInSeconds = (currentTime - startTime) / 1000000;

        printf("\r");
        //-----------------------------------------------------------------------------------------------------------------
        if (cfg.streamCamera) { broadcasting(camera_config.framesCaptured); }
        if (cfg.run_forever)  { printf(GREEN " %lu sec " NORMAL, runningTimeInSeconds ); } else
                          {
                           printf(GREEN " %lu sec " NORMAL,cfg.maxTimeToGrabForInSeconds - runningTimeInSeconds );
                           progress_bar(runningTimeInSeconds,cfg.maxTimeToGrabForInSeconds);
                          }

        if (cfg.useCamera)
            {
             printf("|Cam %lu ",camera_config.framesCaptured);
             printHz(camera_config.actualFrameRate);
             printf(" Ok %lu/Fail %lu/Under %lu",camera_config.n_completed_buffers, camera_config.n_failures,camera_config.n_underruns);
            }

        if (cfg.useArduino)  { if (arduino_config.receivedDataFrames==0) {printf(RED);}
                               printf("|Arduino "); printHz(arduino_config.Hz); printf(NORMAL); }
        if (cfg.useTeensy)   { if (teensy_config.receivedDataFrames==0) {printf(RED);}
                               printf("|Teensy ");  printHz(teensy_config.Hz); printf(NORMAL); }
        if (cfg.useATIForce) { if (atinetft_config.receivedDataFrames==0) {printf(RED);}
                               printf("|ATI ");     printHz(atinetft_config.Hz); printf(NORMAL); }
        //-----------------------------------------------------------------------------------------------------------------
        printf("|   \r");

        if ( (!cfg.run_forever) && (currentTime-startTime > cfg.maxTimeToGrabForInSeconds * 1000000) )
        {
          fprintf(stderr,GREEN "\n\n\n\nSuccesfully Completed recording time..\n" NORMAL);
          cfg.keep_running = 0;
          usleep(10000);
        }
    }

    //Restore terminal to its former state
    if (cfg.interceptKeyboard)
         { restore_terminal_mode(); }

    // Wait for threads to finish
    if (cfg.useArduino)  { fprintf(stderr,"Releasing Arduino\n"); pthread_join(arduino_tid, NULL);    }
    if (cfg.useTeensy)   { fprintf(stderr,"Releasing Teensy\n");  pthread_join(teensy_tid, NULL);     }
    if (cfg.useATIForce) { fprintf(stderr,"Releasing ATI\n");     pthread_join(atinetft_tid, NULL);   }

    printf("\n\n");

   if (cfg.useRAM)
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
    if (cfg.useCamera)   { fprintf(stderr,"Releasing Camera\n");  pthread_join(gigecamera_tid, NULL); }

    return 0;
}
