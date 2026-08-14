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
#include "polarizationLights.h"

//Shared memory for streaming
#include "tactileStreamer.h"
#include "imageStreamer.h"
#include "sharedMemoryVideoBuffers.h"

#include "performance.h"

//Callback defaults
#include "callbacks.h"


#if TACTILE
#include "tactile_processor/TactileFeaturesProcessor.hpp"
#endif // TACTILE


static const char MagicianGrabberVersion[]="1.0";

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
//
//LEGACY PATH. Kept because pre-1.33 controllers have no other way to be stepped:
//they own no notion of the exposure schedule, so the host must nudge them once per
//frame. It is inherently latency-bound (the '+' has to cross the GigE readout, the
//USB-CDC link and the controller's loop tick before the next exposure begins), which
//is why --exposure-locked exists for firmware that can sequence itself. Selected
//automatically at startup based on the controller's reported version.
static int camera_callback_next_light(GiGECameraConfig *config, unsigned long timestamp, struct Image *dataAsImage)
{
    (void) timestamp; (void) dataAsImage;
    if (config==NULL)         { return 0; }
    if (config->global==NULL) { return 0; }

    ArduinoSerialConfig * arduino = (ArduinoSerialConfig *) config->global->arduino_cfg;
    if (arduino==0) { return 0; }

    //With --skip in force, name the COB we want instead of nudging the board's cursor
    //with '+'. '+' lands on whatever the board decides comes next, and nothing tells
    //us what that was in time to veto it before the shutter opens. Naming it costs the
    //same single byte, cannot land on an excluded COB, and needs no model of where the
    //board's cursor is. --skipadvance buys the other trade — the firmware keeps
    //choosing the order and excluded COBs are stepped over reactively from the serial
    //thread (see arduino_process_light_fields) — at the price of one wasted step.
    if ( (config->global->lightSkipMask!=0) && (!config->global->skipByAdvancing) )
    {
      static int cursor = -1;
      cursor = lightNextAllowed(config->global,cursor+1);
      if (cursor<0) { return 0; } //parse_arguments rejects an empty set, so unreachable
      return arduino_sendLightSelect(arduino,cursor);
    }

    return arduino_signalNewFrame(arduino);
}

//Global driver state, reachable from both the camera callback and the serial thread.
static PolLightDriver polarizationDriver = {0};

//Bridges the controller's per-strobe report into the polarization driver so frames
//are attributed to the COB that genuinely fired rather than to a host-side guess.
static void controller_strobe_callback(void *userData, unsigned long counter, int light)
{
    (void) counter;
    polarizationDriverNoteStrobe((PolLightDriver *) userData, light);
}

//Runs the polarization policy on each captured frame. Sends nothing per frame in the
//common case: a new schedule only goes out once a full measurement cycle completes.
static int camera_callback_polarization(GiGECameraConfig *config, unsigned long timestamp, struct Image *dataAsImage)
{
    (void) config; (void) timestamp;
    polarizationDriverProcessFrame(&polarizationDriver, dataAsImage);
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
    snprintf(cfg.cameraStreamName,128,"%s","stream1");
    snprintf(cfg.tactileStreamName,128,"%s","stream_tactile");

    // Global flag for termination
    cfg.keep_running = 1;
    cfg.run_forever  = 0;
    cfg.countdown    = 0;
    cfg.speak        = 0;
    cfg.viewer       = 0;


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
    cfg.streamData = 0;
    snprintf(cfg.atiIP,128,"127.0.0.1");
    cfg.atiPort = 49152;

    #if TACTILE
    cfg.calculateTactileFeatures    = 0;
    #endif // TACTILE

    // Camera Default settings
    cfg.width      = 2448;
    cfg.height     = 2048;
    // 650 µs is the exposure the downstream neural networks are tuned for, so it is
    // the default rather than a starting point. Override it for experiments or to
    // compensate for ambient light; the host forwards whatever is set to the
    // controller as its light-on ceiling, and the controller clamps that in turn.
    cfg.exposure   = 650; // 0 means no setting
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

   if (strcmp("./",cfg.outputDirectory)==0)
   {
     fprintf(stderr,"No Output Directory given will auto generate one! \n");
     setOutputDirectoryFromTimestamp(&cfg);
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
      countdownBeforeStart(cfg.countdown,cfg.speak);
   }

    //Record time that acquisition started (this will be considered as timestamp 0 from now on)
    unsigned long acquisitionStartTime = GetTickCountMicroseconds();

    pthread_t gigecamera_tid, arduino_tid, teensy_tid, atinetft_tid;

    // Initialize Configurations
    //To debug aravis connection use : arv-camera-test-0.10  -d stream  "192.168.137.201"
    ATINetFTConfig atinetft_config     = {&cfg, "127.0.0.1",  49152, "tactile/force.csv",  NULL, &cfg.keep_running,0 , 0, 0, 0.0, NULL};
    ArduinoSerialConfig teensy_config  = {&cfg, "copy from cfg later",    "tactile/accelerometer.csv", 115200, NULL, &cfg.keep_running, 0, NULL , 0, 0, 0.0, NULL};
    ArduinoSerialConfig arduino_config = {&cfg, "copy from cfg later",    "controller.csv",    115200, NULL, &cfg.keep_running, 0, cfg.arduinoExtraCommand, 0, 0, 0.0, NULL};
    GiGECameraConfig camera_config     = {&cfg, "3205040", "camera.csv", cfg.width, cfg.height, cfg.exposure, cfg.gain, cfg.blackLevel, cfg.frameRate, 0, NULL, &cfg.keep_running,0 , 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL };

    //Make arduino_cfg visible!
    cfg.arduino_cfg = (void*) &arduino_config;

    //The camera callback is chosen after the controller has identified itself; see
    //the light-control negotiation further down. Leaving it unset until then keeps
    //a legacy controller from being stepped by a mode it does not implement.

    //Copy teensy/arduino port
    snprintf(teensy_config.port_name,128,"%s",cfg.teensyPath);
    snprintf(arduino_config.port_name,128,"%s",cfg.arduinoPath);

    //Copy ATI IP/port from cfg
    snprintf(atinetft_config.ip_address,128,"%s",cfg.atiIP);
    atinetft_config.port = cfg.atiPort;
    if (cfg.useATIForce)
    { fprintf(stderr,"ATI NetFT address : %s:%d\n",atinetft_config.ip_address,atinetft_config.port); }

    //Try to make arduino wake up correctly
    //system("stty -F /dev/ttyACM0 115200 raw -echo");
    //system("stty -F /dev/ttyACM1 115200 raw -echo");




    #if TACTILE
    printf("Tactile Feature Processor version : %s\n",TactileFeatureProcessorVersion);
    cfg.interceptKeyboard = 0; //Do not intercept keyboard until crashes are resolved
    cfg.calculateTactileFeatures = (cfg.useTeensy) && (cfg.useATIForce);
    pthread_t tactile_tid = 0;
    struct TactileDataState  tactile_config = {&cfg, &cfg.keep_running, 0, 0};

    StreamingTactileContext * streaming_tactile_context=0;
    if ( (cfg.streamData) && (cfg.calculateTactileFeatures) )
         {
           fprintf(stderr,"Starting Tactile Stream..\n");
           streaming_tactile_context = startTactileStream("tactile_frames.shm", cfg.tactileStreamName, TACTILE_STREAMING_WINDOW, TACTILE_STREAMING_ELEMENTS);
           tactile_config.tactile_shm_stream = (void*) streaming_tactile_context;
           if (tactile_config.tactile_shm_stream==NULL)
                                  {  fprintf(stderr,RED "Failed to start streaming tactile data to shared memory!\n" NORMAL); exit(1); }
           if (streaming_tactile_context->frame==NULL)
                                  {  fprintf(stderr,RED "Failed to establish data frame!\n" NORMAL); exit(1); }
         }
    #endif // TACTILE



    #if TACTILE
    //After (potentially) acquiring the streaming tactile context let's start the thread

    if (cfg.calculateTactileFeatures)
    {
        teensy_config.callback   = (void*) addTactileAccelerometerReading;//accelerometer_callback;
        atinetft_config.callback = (void*) addTactileForceReading;//force_callback;
        pthread_create(&tactile_tid, NULL, tactile_thread, &tactile_config);
    }
    #endif // TACTILE

    if (cfg.useCamera)
    {
      fprintf(stderr,"Configuring camera exposure pins..\n");
      int i=system("arv-tool-0.10 control LineSelector=Line3 LineMode=Output LineSource=ExposureActive LineInverter=0");
      if (i!=0)
                           {
                               fprintf(stderr,"Failed setting Aravis Camera Exposure pins, halting to protect LED COBs\n");
                               abort();
                           }
    }

    StreamingContext * streaming_context=0;
    if ( (cfg.streamData) && (cfg.useCamera) )
                         {
                           fprintf(stderr,"Starting Camera Stream..\n");

                           //We transport the raw sensor as 1 channel! (hence the 1 in next line)
                           streaming_context = startStream("video_frames.shm", cfg.cameraStreamName, cfg.width, cfg.height, 1);
                           camera_config.camera_shm_stream = (void*) streaming_context;
                           //fprintf(stderr,"Main Thread shm=%p\n",streaming_context);
                           //fprintf(stderr,"Main Thread #2 shm=%p\n",camera_config.camera_shm_stream);
                           if (camera_config.camera_shm_stream==NULL)
                                  { fprintf(stderr,RED "Failed to start streaming images to shared memory!\n" NORMAL); exit(1); }
                           if (streaming_context->frame==NULL)
                                  {  fprintf(stderr,RED "Failed to establish image video frame!\n" NORMAL); exit(1); }
                         }




    //Arduino takes some time to powerup
    if (cfg.useArduino)  { fprintf(stderr,"Creating Arduino Thread\n"); pthread_create(&arduino_tid,    NULL, arduino_thread,    &arduino_config);  }
    if (cfg.useTeensy)   {
                            fprintf(stderr,"Creating Teensy Thread\n");
                           /*
                              char *teensy_port = find_teensy_port();
                              if (teensy_port) { fprintf(stderr,GREEN "Teensy found on: %s\n" NORMAL, teensy_port); } else
                                               { fprintf(stderr,RED "Teensy port not found\n" NORMAL); exit(1); }*/
                           pthread_create(&teensy_tid,    NULL, arduino_thread,    &teensy_config);
                         }

    //=============================================================
    //  Light control negotiation
    //
    //  Which lighting path we can use depends on what the controller is. Rev 1.33+
    //  can sequence itself off the exposure signal; anything older has to be
    //  stepped by the host once per frame. We must know which BEFORE the camera
    //  starts, because sending the newer multi-character commands to an older
    //  controller does not fail harmlessly — it executes their individual
    //  characters as separate legacy commands.
    //=============================================================
    if (cfg.useArduino)
    {
        //Wait briefly for the version banner requested by arduino_startStream().
        unsigned int waited = 0;
        while ( (arduino_config.controllerVersionMajor==0) && (waited<200) && (cfg.keep_running) )
             { usleep(10000); waited++; }

        int modernController = arduino_controllerSupportsExposureLock(&arduino_config);

        if (!modernController)
        {
            fprintf(stderr,YELLOW "Controller did not report firmware >= 1.33"
                                  " - using the legacy per-frame stepping path\n" NORMAL);

            //Exposure-locked sequencing is a hardware capability, not a firmware one:
            //the legacy boards have no camera exposure input to lock to. It cannot be
            //emulated, so it is dropped.
            if (cfg.exposure_locked_light && !cfg.polarizationLights)
            {
              fprintf(stderr,YELLOW "  (--exposure-locked needs a controller with an exposure input; ignoring it)\n" NORMAL);
            }
            cfg.exposure_locked_light = 0;

            //Polarization selection, by contrast, works fine: the policy is host-side,
            //and a legacy board is driven by naming the COB once per frame instead of
            //uploading a schedule. Same decisions, later delivery.
            if (cfg.polarizationLights)
            {
              fprintf(stderr,YELLOW "  (polarization will step the lights per frame rather than by exposure)\n" NORMAL);
              cfg.manual_trigger_light = 0;   //the driver does the stepping itself
            }
            else
            {
              cfg.manual_trigger_light = 1;
            }
        }

        //Exposure vs light window. Both firmware generations cap the light-on time at
        //1000us, so this check applies regardless of which controller answered - only
        //the ability to *narrow* the window with 'E' is version-dependent.
        if (cfg.exposure!=0)
        {
          unsigned int requestedCeiling = cfg.exposure + LIGHT_ON_CEILING_MARGIN_IN_MICROSECONDS;

          //If the controller's clamp bites, the COBs go dark before the shutter closes
          //and the tail of every exposure is unlit - which reads as an underexposed
          //dataset rather than a lighting fault, so say so up front rather than letting
          //it pass silently. --exposure is expected to be tuned per experiment, so this
          //is a live path, not a theoretical one.
          if (requestedCeiling > LIGHT_ON_CEILING_CONTROLLER_HARD_MAX)
          {
            fprintf(stderr,YELLOW "Exposure %u us needs a %u us light window, but the controller"
                                  " hard-caps at %u us.\n" NORMAL,
                    cfg.exposure, requestedCeiling, LIGHT_ON_CEILING_CONTROLLER_HARD_MAX);
            fprintf(stderr,YELLOW "  The COBs will switch off ~%u us before the shutter closes."
                                  " Lower --exposure, or raise the\n"
                                  "  firmware ceiling only if the COBs can take it.\n" NORMAL,
                    requestedCeiling - LIGHT_ON_CEILING_CONTROLLER_HARD_MAX);
          }

          //SAFETY: tell the controller how long the COBs may be driven. It also meters
          //its per-COB thermal budget in units of this ceiling, so sending the real
          //exposure keeps the accounting honest. The controller hard-clamps whatever we
          //ask for, and ignores this entirely on pre-1.33 firmware.
          if (modernController)
             { arduino_setLightOnCeiling(&arduino_config,requestedCeiling); }
        }

        if (modernController && cfg.exposure_locked_light)
        {
          arduino_enterExposureLockedMode(&arduino_config);
          cfg.manual_trigger_light = 0;

          //On this path the controller walks a schedule it holds, so excluding a COB
          //is simply leaving it out of the upload — no per-frame work and nothing that
          //can drift. The polarization driver builds and uploads its own schedule (and
          //is handed the same mask), so it is left to do that itself.
          if ( (cfg.lightSkipMask!=0) && (!cfg.polarizationLights) )
          {
            unsigned char schedule[LIGHTS_ON_CONTROLLER]={0};
            int scheduleLen = lightBuildAllowedList(&cfg,schedule,LIGHTS_ON_CONTROLLER);
            if (scheduleLen>0)
               { arduino_sendLightSchedule(&arduino_config,schedule,scheduleLen); }
          }
        }

        if (cfg.lightSkipMask!=0)
        {
          char inUse[64]={0};
          lightDescribeAllowed(&cfg,inUse,sizeof(inUse));
          fprintf(stderr,GREEN "Lights in use: %s" NORMAL " (%s)\n",inUse,
                  cfg.exposure_locked_light ? "left out of the controller's schedule" :
                  cfg.skipByAdvancing       ? "stepped over when the controller reports one" :
                                              "selected by name once per frame");
        }
    }
    else
    {
        //No controller in the loop at all.
        cfg.exposure_locked_light = 0;
        cfg.polarizationLights    = 0;
        cfg.manual_trigger_light  = 0;

        if (cfg.lightSkipMask!=0)
        { fprintf(stderr,YELLOW "--skip needs the lighting controller (--arduino); ignoring it\n" NORMAL); }
        cfg.lightSkipMask = 0;
    }

    if (cfg.polarizationLights)
    {
        polarizationDriverStart(&polarizationDriver, (void*) &arduino_config,
                                cfg.polarizationStride, cfg.polarizationDwell,
                                !arduino_controllerSupportsExposureLock(&arduino_config),
                                cfg.lightSkipMask);
        //Ground truth for frame->COB attribution comes from the controller, not from
        //a host-side model of the schedule, so dropped frames cannot desynchronise it.
        arduino_config.strobeCallback = (void*) controller_strobe_callback;
        arduino_config.strobeUserData = (void*) &polarizationDriver;
        camera_config.callback        = (void*) camera_callback_polarization;
    }
    else if (cfg.manual_trigger_light)
    {
        camera_config.callback = (void*) camera_callback_next_light;
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

                             if (timeCheck>3000)
                             {
                               fprintf(stderr,RED "\nCamera timed-out (%u ticks)..\n" NORMAL,timeCheck);
                               cfg.keep_running = 0; //<- this will make the program exit
                               break;
                             }
                           }

                           if (camera_config.running)
                              { fprintf(stderr,"\nCamera online (%u ticks)..\n",timeCheck); }
                          }

    if (cfg.useATIForce) { pthread_create(&atinetft_tid,   NULL, atinetft_thread,   &atinetft_config); }


    if (cfg.viewer)
    {
        int i=system("viewer/viewer.sh&");
        if (i!=0) { fprintf(stderr,RED "Failed executing viewer!\n" NORMAL); }
    }

    //Enable keystrokes to be received without blocking execution
    int key = 0;
    if (cfg.interceptKeyboard)
             { set_nonblocking_mode(); }

    unsigned long startTime = GetTickCountMicroseconds();
    unsigned long currentTime = startTime;

    if (cfg.unixtime)
          { startTime=0; }

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

        if (!cfg.silent)
        { //Terminal progress output
         printf("\r");
         //-----------------------------------------------------------------------------------------------------------------
         if (cfg.streamData)   { broadcasting(camera_config.framesCaptured); }
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
         if (cfg.polarizationLights)
                              { char polSummary[256]={0};
                                polarizationDriverSummary(&polarizationDriver,polSummary,sizeof(polSummary));
                                printf("%s",polSummary); }
         if (cfg.useTeensy)   { if (teensy_config.receivedDataFrames==0) {printf(RED);}
                               printf("|Teensy ");  printHz(teensy_config.Hz); printf(NORMAL); }
         if (cfg.useATIForce) { if (atinetft_config.receivedDataFrames==0) {printf(RED);}
                               printf("|ATI ");     printHz(atinetft_config.Hz); printf(NORMAL); }
         //-----------------------------------------------------------------------------------------------------------------
         printf("|   \r");
         fflush(stdout);
        }

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

    usleep(100000);
    if (cfg.useCamera)   { fprintf(stderr,"Releasing Camera\n");  pthread_join(gigecamera_tid, NULL); }
    exit(0); //Camera spawns another thread so there is a problem making it join again..

    return 0;
}
