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

#define MAXIMUM_ALLOWED_EXPOSURE_IN_MICROSECONDS 750

#define TACTILE_STREAMING_WINDOW 4000
#define TACTILE_STREAMING_ELEMENTS 8
//#define TACTILE_STREAMING_ELEMENTS 16 //Assuming everything enabled

static char arduinoUseRoundLight[]    = {"r\n"};
static char arduinoUseDistanceLight[] = {"a\n"};
static char arduinoUsePatternLight[]  = {"t\n"};


typedef struct
{
    char outputDirectory[1024];
    char outputDirectoryOriginal[1024];
    unsigned long maxTimeToGrabForInSeconds;

    // Global flag for termination
    char keep_running       ;
    char run_forever        ;
    char viewer             ;
    unsigned char countdown ;

    char speak;
    char silent;
    char unixtime;
    char manual_trigger_light;

    // Modules available to use
    char simulate;
    char interceptKeyboard;
    char useRAM       ;
    char useArduino   ;
    char useTeensy    ;
    char useCamera    ;
    char useATIForce  ;
    char streamData   ;
    char compress     ;

    char * arduinoExtraCommand;

    char arduinoPath[128];
    char teensyPath[128];
    char atiIP[128];
    int  atiPort;

    #if TACTILE
    char calculateTactileFeatures;
    #endif // TACTILE


    // Camera Default settings
    unsigned int width      ;
    unsigned int height     ;
    unsigned int exposure   ; // 0 means no setting
    double       gain       ;
    double       blackLevel;
    double       frameRate ; //Each image is 4.5MB,
    //this framerate writes 45MB/sec to disk which is a sane value
    //use --ram to store data on a tmpfs/ for higher speeds
    //use --rt to elevate priority for higher speeds


    void * arduino_cfg;

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
  const char *CLEAR_SCREEN_ANSI = "\e[1;1H\e[2J";
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

static void print_help()
{
    printf("Usage: magician_grabber [OPTIONS]\n\n");
    printf("Options:\n");
    printf("  --simulate                Simulate Devices (development).\n");
    printf("  -o, --output <path>       Set the output directory.\n");
    printf("  --arduino <path>          Set the path to arduino (def. /dev/ttyACM0).\n");
    printf("  --teensy <path>           Set the path to teensy (def. /dev/ttyACM1).\n");
    printf("  --nooutput                Disable file output (redirect to /dev/null).\n");
    printf("  --countdown <seconds>     Perform a countdown before starting.\n");
    printf("  --view                    Use Viewer.\n");
    printf("  --ram                     Use RAM to store data (recommended for high FPS).\n");
    printf("  --trigger                 Manually trigger light change after each captured frame.\n");
    printf("  --notrigger               Do not manually trigger light change after each captured frame.\n");
    printf("  --size <width> <height>   Set the camera resolution in pixels.\n");
    printf("  --exposure <microsec>     Set camera exposure time in microseconds.\n");
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
    printf("  --dlight                  Use lighting based on distance sensor.\n");
    printf("  --rlight                  Use round-robin lighting.\n");
    printf("  --tlight                  Use patterned lighting.\n");
    printf("  --speak                   Use TTS.\n");
    printf("  --rt                      Set real-time priority (requires privileges).\n");
    printf("  --all                     Enable all available devices.\n");
    printf("  --stream                  Stream camera data to shared memory (disables file output).\n");
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
            fprintf(stderr,"Using Lighting based on patterned light\n");
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
}
return 1;
}

#ifdef __cplusplus
}
#endif



#endif // COMMON_H_INCLUDED
