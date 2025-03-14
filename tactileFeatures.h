#ifndef TACTILEFEATURES_H_INCLUDED
#define TACTILEFEATURES_H_INCLUDED

#ifdef __cplusplus
extern "C"
{
#endif

#include "common.h"

#include "arduinoSensor.h"
#include "atiForceSensor.h"


#define TACTILE_MAXIMUM_BUFFER_SIZE 1000


struct TactileDataState
{
  //-------------------------------------------------------
  GlobalConfig* global;
  char * keep_running;
  char running;
  //-------------------------------------------------------
  unsigned int currentSample;
  //-------------------------------------------------------
};

int addTactileForceReading(ATINetFTConfig *config, unsigned long timestamp, double fX,double fY,double fZ,double tX,double tY,double tZ);

int addTactileAccelerometerReading(ArduinoSerialConfig *arduino_config, unsigned long timestamp, unsigned long dev_timestamp, double accX, double accY, double accZ);


void *tactile_thread(void *arg);

#ifdef __cplusplus
}
#endif




#endif // TACTILEFEATURES_H_INCLUDED
