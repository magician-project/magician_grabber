/** @file tactileFeatures.c
 *  @brief  A real-time feature calculation utility for the ATI sensor

 *  @author Michele Pompilio
 */

#include "tactileFeatures.h"

#include <stdio.h>
#include <stdlib.h>

#if TACTILE
#include "tactile_processor/TactileFeaturesProcessor.hpp"
#endif // TACTILE



int addTactileAccelerometerReading(ArduinoSerialConfig *arduino_config, unsigned long system_timestamp, unsigned long dev_timestamp, double accX, double accY, double accZ)
{
  #if TACTILE
  //fprintf(stderr,"addTactileAccelerometerReading called\n\n");
  return tactile_add_acc(system_timestamp,accX,accY,accZ);
  #endif // TACTILE
  return 0;
}


int addTactileForceReading(ATINetFTConfig *config, unsigned long system_timestamp, double fX,double fY,double fZ,double tX,double tY,double tZ)
{
   #if TACTILE
    //fprintf(stderr,"addTactileForceReading called\n\n");
    return tactile_add_force(system_timestamp,fX,fY,fZ);
   #endif // TACTILE
  return 0;
}


void *tactile_thread(void *arg)
{
   #if TACTILE
   struct TactileDataState  *config = (struct TactileDataState  *) arg;
   GlobalConfig *cfg = config->global;

   char enabledFileOutput = (strcmp(cfg->outputDirectory,"/dev/null")!=0);

   unsigned long lastUpdateTime = GetTickCountMicroseconds();




   FILE * FrFD   = 0;
   FILE * AsFD   = 0;
   FILE * ApsdFD = 0;
   FILE * FpsdFD = 0;


   if (enabledFileOutput)
       {
        char fullCSVOutputPath[2048]={0};

        //-----------------------------------------------------------------------------------
        snprintf(fullCSVOutputPath,2048,"%s/tactile/friction.csv",cfg->outputDirectory);
        FrFD = fopen(fullCSVOutputPath, "w");
        if (!FrFD)
        {
           fprintf(stderr,"Failed to open %s CSV file",fullCSVOutputPath);
           exit(1); //Terminate
        } else
        {
           fprintf(stderr,"Opened %s for output\n",fullCSVOutputPath);
        }
        //-----------------------------------------------------------------------------------


        //-----------------------------------------------------------------------------------
        snprintf(fullCSVOutputPath,2048,"%s/tactile/acceleration_spikeness.csv",cfg->outputDirectory);
        AsFD = fopen(fullCSVOutputPath, "w");
        if (!AsFD)
        {
           fprintf(stderr,"Failed to open %s CSV file",fullCSVOutputPath);
           exit(1); //Terminate
        } else
        {
           fprintf(stderr,"Opened %s for output\n",fullCSVOutputPath);
        }
        //-----------------------------------------------------------------------------------


        //-----------------------------------------------------------------------------------
        snprintf(fullCSVOutputPath,2048,"%s/tactile/acceleration_psd.csv",cfg->outputDirectory);
        ApsdFD = fopen(fullCSVOutputPath, "w");
        if (!ApsdFD)
        {
           fprintf(stderr,"Failed to open %s CSV file",fullCSVOutputPath);
           exit(1); //Terminate
        } else
        {
           fprintf(stderr,"Opened %s for output\n",fullCSVOutputPath);
        }
        //-----------------------------------------------------------------------------------


        //-----------------------------------------------------------------------------------
        snprintf(fullCSVOutputPath,2048,"%s/tactile/force_psd.csv",cfg->outputDirectory);
        FpsdFD = fopen(fullCSVOutputPath, "w");
        if (!FpsdFD)
        {
           fprintf(stderr,"Failed to open %s CSV file",fullCSVOutputPath);
           exit(1); //Terminate
        } else
        {
           fprintf(stderr,"Opened %s for output\n",fullCSVOutputPath);
        }
        //-----------------------------------------------------------------------------------

       }



   if ( (FrFD != 0) && (AsFD != 0) && (ApsdFD != 0) && (FpsdFD != 0) )
   {
    while (*config->keep_running)
    {
     unsigned long now = GetTickCountMicroseconds();


     if (now - lastUpdateTime > 10000)
     {
       tactile_write_disk(FrFD, AsFD, ApsdFD , FpsdFD);
       fflush(FrFD);
       fflush(AsFD);
       fflush(ApsdFD);
       fflush(FpsdFD);
     }

     usleep(1000);
   }


   fprintf(stderr,"Closing tactile files \n");

   fclose(FrFD);
   fclose(AsFD);
   fclose(ApsdFD);
   fclose(FpsdFD);

   }
   #endif // TACTILE

   return 0;
}

