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
  GlobalConfig* global;           /**< Shared runtime configuration. */
  char * keep_running;            /**< Points to the global termination flag; thread exits when *keep_running == 0. */
  char running;                   /**< 1 while the thread is active, 0 after it exits. */
  //-------------------------------------------------------
  unsigned int currentSample;     /**< Ring-buffer write index; incremented on every incoming sensor sample. */
  //-------------------------------------------------------
  void * tactile_shm_stream;      /**< Shared-memory stream context for tactile data; non-NULL when --stream is active. */
};

int addTactileForceReading(ATINetFTConfig *config, unsigned long timestamp, double fX,double fY,double fZ,double tX,double tY,double tZ);

int addTactileAccelerometerReading(ArduinoSerialConfig *arduino_config, unsigned long timestamp, unsigned long dev_timestamp, double accX, double accY, double accZ);


void *tactile_thread(void *arg);

#ifdef __cplusplus
}
#endif




#endif // TACTILEFEATURES_H_INCLUDED
