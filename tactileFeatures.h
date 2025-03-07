#ifndef TACTILEFEATURES_H_INCLUDED
#define TACTILEFEATURES_H_INCLUDED

#ifdef __cplusplus
extern "C"
{
#endif

#include "common.h"

#define TACTILE_MAXIMUM_BUFFER_SIZE 1000


struct TactileDataState
{
  //-------------------------------------------------------
  GlobalConfig* global;
  char * keep_running;
  char running;
  //-------------------------------------------------------
  unsigned int start;
  unsigned int finish;
  float forcevalues[6*TACTILE_MAXIMUM_BUFFER_SIZE];
  float accvalues[3*TACTILE_MAXIMUM_BUFFER_SIZE];
  unsigned long timestamps[TACTILE_MAXIMUM_BUFFER_SIZE];
  unsigned int currentSample;
  //-------------------------------------------------------
};


#ifdef __cplusplus
}
#endif




#endif // TACTILEFEATURES_H_INCLUDED
