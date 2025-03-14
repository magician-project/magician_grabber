#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdlib.h>
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

typedef struct
{
    char outputDirectory[1024];
    char outputDirectoryOriginal[1024];
    unsigned long maxTimeToGrabForInSeconds;

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
{
 clearScreen();
 printf(BLUE);
 printf("  __  __          _____ _____ _____ _____          _   _ \n");
 printf(" |  \\/  |   /\\   / ____|_   _/ ____|_   _|   /\\   | \\ | |\n");
 printf(" | \\  / |  /  \\ | |  __  | || |      | |    /  \\  |  \\| |\n");
 printf(" | |\\/| | / /\\ \\| | |_ | | || |      | |   / /\\ \\ | . ` |\n");
 printf(" | |  | |/ ____ \\ |__| |_| || |____ _| |_ / ____ \\| |\\  |\n");
 printf(" |_|  |_/_/    \\_\\_____|_____\\_____|_____/_/    \\_\\_| \\_|\n\n");
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

static void print_help()
{
    printf("Usage: magician_grabber [OPTIONS]\n\n");
    printf("Options:\n");
    printf("  -o, --output <path>       Set the output directory.\n");
    printf("  --nooutput                Disable file output (redirect to /dev/null).\n");
    printf("  --countdown <seconds>     Perform a countdown before starting.\n");
    printf("  --ram                     Use RAM to store data (recommended for high FPS).\n");
    printf("  --size <width> <height>   Set the camera resolution in pixels.\n");
    printf("  --exposure <microsec>     Set camera exposure time in microseconds.\n");
    printf("  --gain <value>            Set camera gain.\n");
    printf("  --fps <Hz>                Set the camera frame rate (use --ram for FPS >10).\n");
    printf("  --blacklevel <value>      Set camera black level.\n");
    printf("  --time <seconds>          Set the maximum time for frame grabbing.\n");
    printf("  --forever                 Run indefinitely.\n");
    printf("  --camera                  Enable the camera.\n");
    printf("  --force                   Enable force sensor.\n");
    printf("  --features                Enable force sensor features calculation.\n");
    printf("  --accelerometer           Enable accelerometer (Teensy device).\n");
    printf("  --distance                Enable distance sensor (Arduino device).\n");
    printf("  --dlight                  Use lighting based on distance sensor.\n");
    printf("  --rlight                  Use round-robin lighting.\n");
    printf("  --rt                      Set real-time priority (requires privileges).\n");
    printf("  --all                     Enable all available devices.\n");
    printf("  --stream                  Stream camera data to shared memory (disables file output).\n");
    printf("  --scan                    Scan using Arduino and exit.\n");
    printf("  --help                    Show this help message and exit.\n");
}

#ifdef __cplusplus
}
#endif



#endif // COMMON_H_INCLUDED
