#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <termios.h>
#include <fcntl.h>
#include <ctype.h>  // For tolower()

#define NORMAL  "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */

#include "colors.h"
#include "performance.h"

#define MAXIMUM_ALLOWED_EXPOSURE_IN_MICROSECONDS 750  /**< Soft cap on exposure time in µs; requires --I_know_what_I_am_doing to exceed. */

#define TACTILE_STREAMING_WINDOW   4000  /**< Number of time-steps retained in the tactile ring buffer per flush cycle. */
#define TACTILE_STREAMING_ELEMENTS 8     /**< Number of sensor channels packed per time-step (force×3 + torque×3 + accel×2). */
//#define TACTILE_STREAMING_ELEMENTS 16 //Assuming everything enabled

static char arduinoUseRoundLight[]    = {"r\n"};
static char arduinoUseDistanceLight[] = {"a\n"};
static char arduinoUsePatternLight[]  = {"t\n"};

/** Margin added to the camera exposure when telling the controller its maximum
 *  light-on window. Covers exposure-signal rise/fall and ISR latency so the COB
 *  is fully on for the whole shutter window, without granting slack the overvolted
 *  COBs cannot afford. The controller hard-clamps the result regardless. */
#define LIGHT_ON_CEILING_MARGIN_IN_MICROSECONDS 150

/** Mirror of LIGHT_HARD_MAX_ON_US in the controller firmware. Kept here only so
 *  the host can warn before a capture that the COBs will be cut off mid-exposure;
 *  the controller enforces its own copy and never trusts this one. Keep in sync. */
#define LIGHT_ON_CEILING_CONTROLLER_HARD_MAX 1000

/** COBs wired to the controller board. They are 0-indexed internally and 1-indexed
 *  everywhere the user can see them, matching the "1".."6" wire protocol. Mirror of
 *  NUMBER_OF_LIGHTS in the firmware. */
#define LIGHTS_ON_CONTROLLER 6


typedef struct
{
    char outputDirectory[1024];         /**< Resolved output path (may be "tmpfs/…" or "/dev/null"). */
    char outputDirectoryOriginal[1024]; /**< Output path as supplied by the user before any resolution. */
    unsigned long maxTimeToGrabForInSeconds; /**< Hard stop time in seconds; ignored when run_forever is set. */

    // Global flag for termination
    char keep_running;       /**< Main-loop termination flag; set to 0 by SIGINT or when the capture time elapses. */
    char run_forever;        /**< When 1, maxTimeToGrabForInSeconds is ignored and capture runs until interrupted. */
    char viewer;             /**< When 1, launch the live viewer process alongside capture. */
    unsigned char countdown; /**< Seconds to count down before starting capture; 0 to skip. */

    char speak;              /**< When 1, use festival TTS to vocalise the countdown. */
    char silent;             /**< When 1, suppress per-frame progress output to stdout. */
    char unixtime;           /**< When 1, write Unix epoch timestamps instead of human-readable ones. */
    char manual_trigger_light; /**< When 1, send a light-change command to the Arduino after every captured frame. Legacy path; superseded by exposure_locked_light. */
    char exposure_locked_light;/**< When 1, the controller advances the light once per exposure in its own ISR and the host never steps lights per frame. */
    char polarizationLights;   /**< When 1, run the DoLP/AoLP light policy and upload schedules to the controller. */
    int  polarizationStride;   /**< Superpixel subsampling stride for the polarization reduction; 0 = default. */
    int  polarizationDwell;    /**< Consecutive frames each COB is held per cycle; 0 = default. */

    unsigned char lightSkipMask; /**< Bit i set = COB i+1 excluded by --skip. Such a COB is never scheduled, never selected and never scored. 0 = use them all. */
    char skipByAdvancing;        /**< When 1, skip by pushing the controller past an excluded COB with an extra '+' rather than by naming the COB we want. Keeps a firmware-owned pattern intact; see --skipadvance. */
    char firmwareOwnedLightOrder;/**< When 1, the ORDER of the COBs is decided on the controller and cannot be reproduced host-side (--dlight, --tlight), so --skip must veto rather than dictate. Round-robin is not marked: naming COBs in ascending order reproduces it exactly. */

    // Modules available to use
    char simulate;           /**< When 1, all device threads run in simulation mode (no hardware required). */
    char interceptKeyboard;  /**< When 1, capture raw keystrokes during acquisition (disables terminal echo). */
    char useRAM;             /**< When 1, write output to tmpfs/ (RAM-backed) to avoid disk bottleneck at high frame rates. */
    char useArduino;         /**< When 1, start the Arduino serial thread (distance sensor + lighting controller). */
    char isPico2;            /**< When 1, the serial board is the experimental RP2350/Pico 2 (native USB-CDC); keep DTR asserted so it transmits. Default 0 = legacy AVR Nano. */
    char useTeensy;          /**< When 1, start the Teensy serial thread (accelerometer). */
    char useCamera;          /**< When 1, start the GigE camera thread. */
    char useATIForce;        /**< When 1, start the ATI NetFT force/torque thread. */
    char streamData;         /**< When 1, publish frames to POSIX shared memory (disables file output by default). */
    char compress;           /**< When 1, save camera frames as PNG; otherwise raw PNM. */

    char * arduinoExtraCommand; /**< If non-NULL, sent to the Arduino at startup to select a lighting mode (see arduinoUse*Light strings). */

    char arduinoPath[128];   /**< Serial device path for the Arduino (default /dev/ttyUSB0). */
    char teensyPath[128];    /**< Serial device path for the Teensy (default /dev/ttyACM0). */
    char atiIP[128];         /**< ATI NetFT sensor IP address. */
    int  atiPort;            /**< ATI NetFT sensor UDP port. */

    char cameraStreamName[128];  /**< POSIX SHM stream name for camera frames (default "stream1"). */
    char tactileStreamName[128]; /**< POSIX SHM stream name for tactile data (default "stream_tactile"). */

    #if TACTILE
    char calculateTactileFeatures; /**< When 1, run the real-time tactile feature extractor (magician_grabber_tactile only). */
    #endif // TACTILE


    // Camera Default settings
    unsigned int width;      /**< Capture width in pixels. */
    unsigned int height;     /**< Capture height in pixels. */
    unsigned int exposure;   /**< Exposure time in microseconds; 0 means use the camera default. */
    double       gain;       /**< Analogue gain. */
    double       blackLevel; /**< Black-level offset. */
    double       frameRate;  /**< Target frame rate in Hz. Each frame is ~4.5 MB; use --ram above ~10 Hz to avoid disk saturation. */


    void * arduino_cfg; /**< Pointer to ArduinoSerialConfig; cast before use. Owned and freed by the main thread. */

} GlobalConfig;

#define USE_SIMPLE_FRAME_APPEND 0

#define EPOCH_YEAR_IN_TM_YEAR 1900

static unsigned long tickBaseTPMN = 0;
/**
 * @brief Return number of clock ticks for our system in microseconds
 * @return Returns number of ticks in microseconds.
 */
static unsigned long GetTickCountMicroseconds()
{
    struct timespec ts;
    if ( clock_gettime(CLOCK_MONOTONIC,&ts) != 0)
        {
            return 0;
        }

    if (tickBaseTPMN==0)
        {
            tickBaseTPMN  = ts.tv_sec*1000000 + ts.tv_nsec/1000;
            return 0;
        }

    return ( ts.tv_sec*1000000 + ts.tv_nsec/1000 ) - tickBaseTPMN ;
}


static struct termios orig_termios;
static int orig_flags;

// Save original terminal settings before modifying
static void set_nonblocking_mode()
{
    struct termios newt;

    // Save current terminal settings
    tcgetattr(STDIN_FILENO, &orig_termios);
    orig_flags = fcntl(STDIN_FILENO, F_GETFL, 0);

    // Modify settings
    newt = orig_termios;
    newt.c_lflag &= ~(ICANON | ECHO);  // Disable line buffering & echo
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    fcntl(STDIN_FILENO, F_SETFL, orig_flags | O_NONBLOCK);  // Set non-blocking mode
}

// Restore the terminal to its original state
static void restore_terminal_mode()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);  // Restore terminal settings
    fcntl(STDIN_FILENO, F_SETFL, orig_flags);         // Restore original flags
}


static int get_keystroke()
{
    int ch = getchar();
    if (ch != EOF)
    {
        return tolower(ch);  // Convert to lowercase
    }
    return 0;
}

static void clearScreen()
{
  const char *CLEAR_SCREEN_ANSI = "\033[1;1H\033[2J";
  int i = write(STDOUT_FILENO, CLEAR_SCREEN_ANSI, 11);
  if (i!=11) { fprintf(stderr,"Could not clear screen for some reason..\n"); }
}

static void banner(const char * ver)
{clearScreen();
 printf(BLUE);
 //printf(CYNB);
 //ANSI SHADOW
 printf("███╗   ███╗ █████╗  ██████╗ ██╗ ██████╗██╗ █████╗ ███╗   ██╗\n");
 printf("████╗ ████║██╔══██╗██╔════╝ ██║██╔════╝██║██╔══██╗████╗  ██║\n");
 printf("██╔████╔██║███████║██║  ███╗██║██║     ██║███████║██╔██╗ ██║\n");
 printf("██║╚██╔╝██║██╔══██║██║   ██║██║██║     ██║██╔══██║██║╚██╗██║\n");
 printf("██║ ╚═╝ ██║██║  ██║╚██████╔╝██║╚██████╗██║██║  ██║██║ ╚████║\n");
 printf("╚═╝     ╚═╝╚═╝  ╚═╝ ╚═════╝ ╚═╝ ╚═════╝╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝\n");
 //printf(BLKB);
 printf("                                Grabber v%s\n\n" NORMAL,ver);
 printf("\n");
}

static void broadcasting(unsigned long frameNumber)
{
  const int update = 10;
  if (frameNumber%update > update /2)
     { printf("((( )))"); } else
     { printf("(((" RED "•" NORMAL ")))"); }
}


static void progress_bar(unsigned long runningTimeInSeconds,unsigned long maxTimeToGrabForInSeconds)
{
  #define PROGRESS_BAR_LENGTH 10  // Length of the progress bar

  double progress = (double)runningTimeInSeconds / maxTimeToGrabForInSeconds;
  int filledLength = (int)(progress * PROGRESS_BAR_LENGTH);
  if (filledLength > PROGRESS_BAR_LENGTH) filledLength = PROGRESS_BAR_LENGTH;

        // Draw the progress bar
  printf(" [");
  for (int i = 0; i < PROGRESS_BAR_LENGTH; i++)
         {
            if (i < filledLength)  { printf(GREEN "█" NORMAL); } else
                                   { printf("-"); }
        }
  printf("] ");
}

static void countdownBeforeStart(unsigned int countdown,char speak)
{
   fprintf(stderr,"Performing initial countdown (%u seconds) : ",countdown);

   char whatToSay[101]={0};

   unsigned int i,c;
   for (c=0; c<countdown; c++)
       {
         unsigned int remaining = (unsigned int) countdown-c;

         if ( (remaining<=3) && (speak) )
         {
          snprintf(whatToSay,100,"echo \"%u\" | festival --tts&",remaining);
          i=system(whatToSay);
          if (i!=0) {  fprintf(stderr,"Failed executing : %s\n",whatToSay); }
         }

         usleep(1000000); //1 sec
         fprintf(stderr,".");
       }
   fprintf(stderr,"\n");

   if (speak)
   {
     snprintf(whatToSay,100,"echo \"Start\" | festival --tts&");
     i=system(whatToSay);
     if (i!=0) {  fprintf(stderr,"Failed executing : %s\n",whatToSay);}
   }
}


static int setOutputDirectory(GlobalConfig *cfg, const char * outputDirectory)
{
    if (cfg==0) { return 0; }
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
     if (z==0) { fprintf(stderr,"Output Path set to \"%s\" \n",cfg->outputDirectory); } else
                { fprintf(stderr,RED "Failed setting output Path to \"%s\" \n" NORMAL,cfg->outputDirectory); }

     snprintf(makedircmd,1024,"mkdir -p %.512s/tactile",cfg->outputDirectory);
     z = system(makedircmd);
     if (z==0) { fprintf(stderr,"Also created a tactile subdirectory \n"); } else
                { fprintf(stderr,RED "Failed creating tactile subdir \n" NORMAL); }

    }
    return 1;
}




static int setOutputDirectoryFromTimestamp(GlobalConfig *cfg)
{
    time_t rawtime;
    struct tm * timeinfo;
    char buffer[32]={0}; // Enough space for "YYYY_MM_DD_HH_MM_SS\0"

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, 20, "%Y-%m-%d-%H-%M-%S", timeinfo);

    char outDir[512]={0};

    if (cfg->useRAM)
       { snprintf(outDir,512,"tmpfs/%s-dur%lu",buffer,cfg->maxTimeToGrabForInSeconds); } else
       { snprintf(outDir,512,"./%s-dur%lu",buffer,cfg->maxTimeToGrabForInSeconds);     }

    return setOutputDirectory(cfg,outDir);
}

static int noOutputDirectory(GlobalConfig *cfg)
{
  if (cfg==0) { return 0; }

  setOutputDirectory(cfg,"/dev/null");
  return 1;
}

/** Has --skip excluded this COB? @param light0 0-indexed COB. */
static int lightIsSkipped(const GlobalConfig *cfg, int light0)
{
    if (cfg==0)                       { return 0; }
    if (light0<0)                     { return 0; }
    if (light0>=LIGHTS_ON_CONTROLLER) { return 0; }
    return (int) ((cfg->lightSkipMask >> light0) & 1);
}

/** Fill `out` with the 0-indexed COBs --skip has left in play, in ascending order.
 *  @return how many were written. */
static int lightBuildAllowedList(const GlobalConfig *cfg, unsigned char *out, int maxOut)
{
    int n = 0;
    for (int i=0; (i<LIGHTS_ON_CONTROLLER) && (n<maxOut); i++)
    {
        if (!lightIsSkipped(cfg,i)) { out[n++] = (unsigned char) i; }
    }
    return n;
}

/** Next COB at or after `from` that --skip has left in play, wrapping at the end.
 *  @return 0-indexed COB, or -1 if every COB is excluded. */
static int lightNextAllowed(const GlobalConfig *cfg, int from)
{
    if (from<0) { from = 0; }
    for (int step=0; step<LIGHTS_ON_CONTROLLER; step++)
    {
        int candidate = (from + step) % LIGHTS_ON_CONTROLLER;
        if (!lightIsSkipped(cfg,candidate)) { return candidate; }
    }
    return -1;
}

/** Render the COBs still in play as a human-readable 1-indexed list ("1,2,3,4,6"). */
static void lightDescribeAllowed(const GlobalConfig *cfg, char *buffer, unsigned int bufferSize)
{
    if ( (buffer==0) || (bufferSize==0) ) { return; }
    buffer[0] = 0;

    unsigned int at = 0;
    for (int i=0; i<LIGHTS_ON_CONTROLLER; i++)
    {
        if (lightIsSkipped(cfg,i)) { continue; }
        int written = snprintf(buffer+at,bufferSize-at,"%s%d",(at!=0)?",":"",i+1);
        if (written<=0) { break; }
        at += (unsigned int) written;
        if (at>=bufferSize) { break; }
    }
}

/** Parse a --skip argument. Every digit in the string is taken as a 1-indexed light
 *  number, so "--skip 5", "--skip 5,6" and "--skip 56" all say the same thing — the
 *  lights are single digits on the wire, so there is no two-digit reading to lose.
 *  The flag is repeatable and the exclusions accumulate. */
static void parseLightSkipList(GlobalConfig *cfg, const char *list)
{
    int found = 0;
    for (const char *p=list; *p!=0; p++)
    {
        if ( (*p<'0') || (*p>'9') ) { continue; } //whatever separator the user typed
        found = 1;

        int light = (int) (*p - '0');
        if ( (light<1) || (light>LIGHTS_ON_CONTROLLER) )
        {
            fprintf(stderr,YELLOW "--skip: there is no light %d (lights are 1..%d), ignoring it\n" NORMAL,light,LIGHTS_ON_CONTROLLER);
            continue;
        }

        cfg->lightSkipMask |= (unsigned char) (1u << (light-1));
        fprintf(stderr,"Light %d will be skipped\n",light);
    }

    if (!found)
    { fprintf(stderr,YELLOW "--skip: \"%s\" holds no light numbers, ignoring it\n" NORMAL,list); }
}


static void print_help()
{
    printf("Usage: magician_grabber [OPTIONS]\n\n");
    printf("Options:\n");
    printf("  --simulate                Simulate Devices (development).\n");
    printf("  -o, --output <path>       Set the output directory.\n");
    printf("  --arduino <path>          Set the path to arduino (def. /dev/ttyACM0).\n");
    printf("  --pico2                   Serial board is the experimental RP2350/Pico 2 (keep DTR asserted for its USB-CDC).\n");
    printf("  --teensy <path>           Set the path to teensy (def. /dev/ttyACM1).\n");
    printf("  --nooutput                Disable file output (redirect to /dev/null).\n");
    printf("  --countdown <seconds>     Perform a countdown before starting.\n");
    printf("  --view                    Use Viewer.\n");
    printf("  --ram                     Use RAM to store data (recommended for high FPS).\n");
    printf("  --trigger                 Manually trigger light change after each captured frame.\n");
    printf("  --notrigger               Do not manually trigger light change after each captured frame.\n");
    printf("  --size <width> <height>   Set the camera resolution in pixels.\n");
    printf("  --exposure <microsec>     Set camera exposure time in microseconds (def. 650, which is\n");
    printf("                            what the downstream neural networks are tuned for).\n");
    printf("  --gain <value>            Set camera gain.\n");
    printf("  --fps <Hz>                Set the camera frame rate (use --ram for FPS >10).\n");
    printf("  --blacklevel <value>      Set camera black level.\n");
    printf("  --duration <seconds>      Set the maximum time for frame grabbing.\n");
    printf("  --time <seconds>          Set the maximum time for frame grabbing.\n");
    printf("  --forever                 Run indefinitely.\n");
    printf("  --camera                  Enable the camera.\n");
    printf("  --force                   Enable force sensor.\n");
    printf("  --atiip <ip>              Set ATI NetFT sensor IP address.\n");
    printf("  --atiport <port>          Set ATI NetFT sensor port.\n");
    printf("  --features                Enable force sensor features calculation.\n");
    printf("  --accelerometer           Enable accelerometer (Teensy device).\n");
    printf("  --distance                Enable distance sensor (Arduino device).\n");
    printf("  --exposure-locked         Let the controller advance lights itself, one step per\n");
    printf("                            camera exposure, instead of the host sending '+' per frame.\n");
    printf("                            Requires controller firmware >= 1.33; ignored otherwise.\n");
    printf("  --polarization            Choose lights from per-frame DoLP/AoLP measurements.\n");
    printf("                            Implies --exposure-locked.\n");
    printf("  --skip <n[,n...]>         Do not use light n (1..%d). Repeatable, and it accumulates,\n",LIGHTS_ON_CONTROLLER);
    printf("                            so \"--skip 5 --skip 6\" and \"--skip 5,6\" are the same thing.\n");
    printf("                            Honoured on every lighting path and on every firmware.\n");
    printf("  --skipadvance             Skip by sending an extra '+' when the controller reports an\n");
    printf("                            excluded light, instead of naming the light to use. Keeps a\n");
    printf("                            firmware-owned pattern (--rlight/--tlight/--dlight) intact.\n");
    printf("  --polstride <n>           Polarization subsampling stride (default 8; 1 = every superpixel).\n");
    printf("  --poldwell <n>            Frames each light is held per cycle (default 3).\n");
    printf("  --dlight                  Use lighting based on distance sensor.\n");
    printf("  --rlight                  Use round-robin lighting.\n");
    printf("  --tlight                  Use patterned lighting.\n");
    printf("  --speak                   Use TTS.\n");
    printf("  --rt                      Set real-time priority (requires privileges).\n");
    printf("  --all                     Enable all available devices.\n");
    printf("  --stream                  Stream camera data to shared memory (disables file output).\n");
    printf("  --camerastream <name>     Set shared memory stream name for camera (def. stream1).\n");
    printf("  --tactilestream <name>    Set shared memory stream name for tactile (def. stream_tactile).\n");
    printf("  --scan                    Scan using Arduino and exit.\n");
    printf("  --help                    Show this help message and exit.\n");
    printf("  --compress                Save camera frames as .png instead of .pnm.\n");
    printf("  --silent                  Don't produce progress messages.\n");
    printf("  --unixtime                Use unix-time for timestamps.\n");
}


static void printHz(float Hz)
{
    if (Hz>1000)
    { printf("%0.2f Khz", (float) Hz /1000); } else
    { printf("%0.2f Hz", Hz );  }
}


static int parse_arguments(GlobalConfig *cfg,int argc, char **argv)
{
    int I_Know_What_I_Am_Doing = 0;

    //Parse command line arguments
    for (int i=0; i<argc; i++)
    {
        if (strcmp(argv[i], "--help") == 0)
        {
         print_help();
         exit(0);
        } else
        if (strcmp(argv[i], "--compress") == 0)
        {
           fprintf(stderr,"Will compress camera frames to PNG\n");
           cfg->compress = 1;
        } else
        if (strcmp(argv[i], "--silent") == 0)
        {
           fprintf(stderr,"Silencing terminal output\n");
           cfg->silent = 1;
        } else
        if (strcmp(argv[i], "--unixtime") == 0)
        {
           fprintf(stderr,"Using unix time for timestamps\n");
           cfg->unixtime = 1;
        } else
        if (strcmp(argv[i], "--simulate") == 0)
        {
           cfg->simulate = 1;
           cfg->interceptKeyboard = 0;
           #if TACTILE
           cfg->calculateTactileFeatures = 1;
           cfg->useTeensy =1;
           cfg->useATIForce =1;
           #endif // TACTILE
           fprintf(stderr,"Simulating input\n");
        } else
        if (strcmp(argv[i],"--noarduino")==0)
        {
              cfg->useArduino = 0;
        } else
        if (strcmp(argv[i],"--pico2")==0)
        {
              cfg->isPico2 = 1;
              fprintf(stderr,"Experimental RP2350/Pico 2 board selected (native USB-CDC DTR handling)\n");
        } else
        if (strcmp(argv[i],"--arduino")==0)
        {
            if (argc>i+1)
            {
              cfg->useArduino = 1;
              snprintf(cfg->arduinoPath,128,"%s",argv[i+1]);
            }
            else
            { fprintf(stderr,"Failed setting arduino path, not enough arguments! \n"); }
        } else
        if (strcmp(argv[i],"--teensy")==0)
        {
            if (argc>i+1)
            {
              cfg->useTeensy = 1;
              snprintf(cfg->teensyPath,128,"%s",argv[i+1]);
            }
            else
            { fprintf(stderr,"Failed setting arduino path, not enough arguments! \n"); }
        } else
        if ( (strcmp(argv[i],"-o")==0) || (strcmp(argv[i],"--output")==0) )
        {
            if (argc>i+1)
            { setOutputDirectory(cfg,argv[i+1]); }
            else
            { fprintf(stderr,"Failed setting output Path, not enough arguments! \n"); }
        }
        else if (strcmp(argv[i],"--nooutput")==0)
        {
            noOutputDirectory(cfg);
            fprintf(stderr,"File output disabled\n");
        }
        else if (strcmp(argv[i],"--countdown")==0)
        {
            cfg->countdown = (unsigned char) atoi(argv[i+1]);
            fprintf(stderr,"Will perform countdown before starting\n");
        }
        else if (strcmp(argv[i],"--nokb")==0)
        {
            cfg->interceptKeyboard = 0;
            fprintf(stderr,"Will not intercept keyboard presses\n");
        }
        else if (strcmp(argv[i],"--trigger")==0)
        {
            cfg->manual_trigger_light = 1;
            fprintf(stderr,"Will manually trigger light changes!\n");
        }
        else if (strcmp(argv[i],"--notrigger")==0)
        {
            cfg->manual_trigger_light = 0;
            fprintf(stderr,"Will NOT manually trigger light changes!\n");
        }
        else if (strcmp(argv[i],"--kb")==0)
        {
            cfg->interceptKeyboard = 1;
            fprintf(stderr,"Will intercept keyboard presses\n");
        }
        else if (strcmp(argv[i],"--ram")==0)
        {
            cfg->useRAM = 1;
            fprintf(stderr,"Will use RAM to store data\n");
        }
        else if (strcmp(argv[i],"--size")==0)
        {
            cfg->width  = atoi(argv[i+1]);
            cfg->height = atoi(argv[i+2]);
            fprintf(stderr,"Camera size set to %u x %u pixels \n",cfg->width,cfg->height);
        }
        else if (strcmp(argv[i],"--I_know_what_I_am_doing")==0)
        {
            I_Know_What_I_Am_Doing = 1;
            fprintf(stderr," USER EXPLICITLY STATED HE KNOWS WHAT HE IS DOING\n");
        }
        else if (strcmp(argv[i],"--exposure")==0)
        {
            cfg->exposure=atoi(argv[i+1]);
            if ( (cfg->exposure>MAXIMUM_ALLOWED_EXPOSURE_IN_MICROSECONDS) && (!I_Know_What_I_Am_Doing) )
            {  
              fprintf(stderr,"Not allowing exposure to be set to more than %u μsec \n",MAXIMUM_ALLOWED_EXPOSURE_IN_MICROSECONDS);
              cfg->exposure = MAXIMUM_ALLOWED_EXPOSURE_IN_MICROSECONDS;
            }
            fprintf(stderr,"Exposure will be set to %u μsec \n",cfg->exposure);
        }
        else if (strcmp(argv[i],"--gain")==0)
        {
            cfg->gain=atof(argv[i+1]);
            fprintf(stderr,"Gain will be set to %f \n",cfg->gain);
        }
        else if (strcmp(argv[i],"--fps")==0)
        {
            cfg->frameRate=atof(argv[i+1]);
            fprintf(stderr,"Framerate will be set to %f Hz \n",cfg->frameRate);
            if (cfg->frameRate>10)
            {
              fprintf(stderr,"Consider using --ram to write to a tmpfs to support this framerate without frame drops!\n");
            }
        }
        else if (strcmp(argv[i],"--blacklevel")==0)
        {
            cfg->blackLevel=atof(argv[i+1]);
            fprintf(stderr,"Black Level will be set to %f μsec \n",cfg->blackLevel);
        }
        else if  ( (strcmp(argv[i],"--time")==0) || (strcmp(argv[i],"--duration")==0) )
        {
            cfg->run_forever=0;
            cfg->maxTimeToGrabForInSeconds=atoi(argv[i+1]);
            fprintf(stderr,"Setting frame grab to %lu \n",cfg->maxTimeToGrabForInSeconds);
        }
        else if (strcmp(argv[i],"--forever")==0)
        {
            cfg->run_forever=1;
            fprintf(stderr,"Running forever..\n");
        }
        else if ( (strcmp(argv[i],"--view")==0) || (strcmp(argv[i],"--viewer")==0) )
        {
            cfg->viewer=1;
            cfg->streamData = 1;
            cfg->useCamera    = 1;
            fprintf(stderr,"Also running viewer..\n");
        }
        else if (strcmp(argv[i],"--camera")==0)
        {
            cfg->useCamera = 1;
            fprintf(stderr,"Activating Camera\n");
        }
        else if (strcmp(argv[i],"--nocamera")==0)
        {
            cfg->useCamera = 0;
            fprintf(stderr,"Deactivating Camera\n");
        }
        else if (strcmp(argv[i],"--speak")==0)
        {
            cfg->speak = 1;
            fprintf(stderr,"Activating Speaking\n");
        }
        else if (strcmp(argv[i],"--force")==0)
        {
            cfg->useATIForce = 1;
            fprintf(stderr,"Activating Force\n");
        }
        else if (strcmp(argv[i],"--atiip")==0)
        {
            if (argc>i+1)
            {
              snprintf(cfg->atiIP,128,"%s",argv[i+1]);
              fprintf(stderr,"ATI IP set to %s\n",cfg->atiIP);
            }
            else
            { fprintf(stderr,"Failed setting ATI IP, not enough arguments!\n"); }
        }
        else if (strcmp(argv[i],"--atiport")==0)
        {
            if (argc>i+1)
            {
              cfg->atiPort = atoi(argv[i+1]);
              fprintf(stderr,"ATI port set to %d\n",cfg->atiPort);
            }
            else
            { fprintf(stderr,"Failed setting ATI port, not enough arguments!\n"); }
        }
        else if (strcmp(argv[i],"--features")==0)
        {
            #if TACTILE
              cfg->calculateTactileFeatures = 1;
              fprintf(stderr,"Activating Force\n");
            #else
              fprintf(stderr,"This build of magician grabber has no tactile features, try magician_grabber_tactile\n");
              exit(1);
            #endif // TACTILE
        }
        else if (strcmp(argv[i],"--accelerometer")==0)
        {
            cfg->useTeensy = 1;
            fprintf(stderr,"Activating Force\n");
        }
        else if (strcmp(argv[i],"--distance")==0)
        {
            cfg->useArduino = 1;
            fprintf(stderr,"Activating Arduino\n");
        }
        else if (strcmp(argv[i],"--dlight")==0)
        {
            cfg->arduinoExtraCommand = arduinoUseDistanceLight;
            cfg->firmwareOwnedLightOrder = 1; //the distance reading picks the COB, not us
            fprintf(stderr,"Using Lighting based on distance\n");
        }
        else if (strcmp(argv[i],"--rlight")==0)
        {
            cfg->arduinoExtraCommand = arduinoUseRoundLight;
            fprintf(stderr,"Using Lighting based on round robin\n");
        }
        else if (strcmp(argv[i],"--tlight")==0)
        {
            cfg->arduinoExtraCommand = arduinoUsePatternLight;
            cfg->firmwareOwnedLightOrder = 1; //the pattern lives in the firmware
            fprintf(stderr,"Using Lighting based on patterned light\n");
        }
        else if (strcmp(argv[i],"--exposure-locked")==0)
        {
            cfg->exposure_locked_light = 1;
            cfg->manual_trigger_light  = 0;
            fprintf(stderr,"Handing light sequencing to the controller (needs firmware >= 1.33)\n");
        }
        else if (strcmp(argv[i],"--polarization")==0)
        {
            //The polarization policy works by uploading schedules, which only exists
            //in the exposure-locked path, so selecting it implies that path.
            cfg->polarizationLights    = 1;
            cfg->exposure_locked_light = 1;
            cfg->manual_trigger_light  = 0;
            fprintf(stderr,"Using polarization (DoLP/AoLP) driven lighting\n");
        }
        else if (strcmp(argv[i],"--skip")==0)
        {
            if (argc>i+1)
            { parseLightSkipList(cfg,argv[i+1]); }
            else
            { fprintf(stderr,"Failed setting skipped light, not enough arguments! \n"); }
        }
        else if (strcmp(argv[i],"--skipadvance")==0)
        {
            cfg->skipByAdvancing = 1;
            fprintf(stderr,"Skipped lights will be stepped over rather than bypassed by name\n");
        }
        else if (strcmp(argv[i],"--polstride")==0)
        {
            cfg->polarizationStride = atoi(argv[i+1]);
            fprintf(stderr,"Polarization subsampling stride set to %d\n",cfg->polarizationStride);
        }
        else if (strcmp(argv[i],"--poldwell")==0)
        {
            cfg->polarizationDwell = atoi(argv[i+1]);
            fprintf(stderr,"Polarization dwell set to %d frames per COB\n",cfg->polarizationDwell);
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
            cfg->useArduino  = 1;
            cfg->useTeensy   = 1;
            cfg->useATIForce = 1;
            cfg->useCamera   = 1;
            //cfg->calculateTactileFeatures = 1;
            fprintf(stderr,"Activating All Devices\n");
        }
        else if (strcmp(argv[i],"--stream")==0)
        {
            cfg->streamData = 1;
            cfg->useCamera    = 1;
            cfg->useArduino   = 1;
            cfg->run_forever  = 1;
            fprintf(stderr,"Streaming camera data to shared memory\n");
            noOutputDirectory(cfg);
            fprintf(stderr,"File output disabled, use --output with a later command to re-enable\n");
        }
        else if (strcmp(argv[i],"--camerastream")==0)
        {
            if (argc>i+1)
            {
              snprintf(cfg->cameraStreamName,128,"%s",argv[i+1]);
              fprintf(stderr,"Camera stream name set to \"%s\"\n",cfg->cameraStreamName);
            }
            else
            { fprintf(stderr,"Failed setting camera stream name, not enough arguments!\n"); }
        }
        else if (strcmp(argv[i],"--tactilestream")==0)
        {
            if (argc>i+1)
            {
              snprintf(cfg->tactileStreamName,128,"%s",argv[i+1]);
              fprintf(stderr,"Tactile stream name set to \"%s\"\n",cfg->tactileStreamName);
            }
            else
            { fprintf(stderr,"Failed setting tactile stream name, not enough arguments!\n"); }
        }
}

if (cfg->lightSkipMask!=0)
{
  //Nothing downstream has a sane answer for "light the scene with no lights", and
  //every stepping path would have no COB left to select. Refuse it here rather than
  //let it surface as a dark dataset.
  unsigned char allowed[LIGHTS_ON_CONTROLLER]={0};
  if (lightBuildAllowedList(cfg,allowed,LIGHTS_ON_CONTROLLER)==0)
  {
    fprintf(stderr,RED "--skip excluded all %d lights, leaving nothing to illuminate with.\n" NORMAL,LIGHTS_ON_CONTROLLER);
    exit(1);
  }

  //--dlight and --tlight let the CONTROLLER decide which COB comes next, from inputs
  //the host does not have. Naming a COB per frame would quietly replace that with a
  //plain ascending sweep, so veto the excluded ones instead and leave the choice
  //where the user put it. Round-robin needs no such care: an ascending sweep over the
  //COBs still in play is exactly what it would have produced.
  if ( (cfg->firmwareOwnedLightOrder) && (!cfg->skipByAdvancing) )
  {
    cfg->skipByAdvancing = 1;
    fprintf(stderr,"The controller owns the light order here, so skipped lights will be stepped over rather than bypassed by name\n");
  }
}

return 1;
}

#ifdef __cplusplus
}
#endif



#endif // COMMON_H_INCLUDED
