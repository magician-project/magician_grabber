#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

#include <unistd.h>
#include <time.h>

typedef struct
{
    char outputDirectory[1024];
    unsigned long maxTimeToGrabForInSeconds;

} GlobalConfig;


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

#define NORMAL   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */


#endif // COMMON_H_INCLUDED
