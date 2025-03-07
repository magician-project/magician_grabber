#include "tactileFeatures.h"

#include <stdio.h>
#include <stdlib.h>


int addTactileAccelerometerReading(struct TactileDataState * state,unsigned long system_timestamp,unsigned long device_timestamp,float accX,float accY,float accZ)
{


  return 1;
}


int addTactileForceReading(struct TactileDataState * state,unsigned long system_timestamp,float fX,float fY,float fZ,float tX,float tY,float tZ)
{


  return 1;
}


void *tactile_threading(void *arg)
{
    struct TactileDataState  *config = (struct TactileDataState  *)arg;
    GlobalConfig *cfg = config->global;


   while (*config->keep_running)
   {
     unsigned long loopTime = GetTickCountMicroseconds();


   }

   return 0;
}


/*
int main(int argc, char **argv)
{
   struct TactileDataState state = {0};
   tactile_threading((void*) &state);
}
*/

