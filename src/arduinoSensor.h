#ifndef ARDUINOSENSOR_H_INCLUDED
#define ARDUINOSENSOR_H_INCLUDED


#ifdef __cplusplus
extern "C"
{
#endif

#include "common.h"

typedef struct
{
    GlobalConfig* global;           /**< Shared runtime configuration. */
    char port_name[128];            /**< Serial device path (e.g. "/dev/ttyUSB0"). */
    char csv_name[128];             /**< Path of the output CSV file. */
    int baud_rate;                  /**< Serial baud rate (default 115200). */

    FILE *csv_file;                 /**< Open handle to csv_name; NULL when file output is disabled. */
    char * keep_running;            /**< Points to the global termination flag; thread exits when *keep_running == 0. */
    char running;                   /**< 1 while the thread is active, 0 after it exits. */

    char * extraCommands;           /**< Optional ASCII command sent to the Arduino at startup to select a lighting mode (e.g. "r\n"). */

    int serial_fd;                  /**< File descriptor for the open serial port. */
    unsigned long receivedDataFrames; /**< Total data frames received since thread start. */
    float Hz;                       /**< Measured sensor acquisition rate in Hz. */

    void * callback;                /**< Optional user callback; cast to accelerometer_callback_t or controller_callback_t depending on firmware. */

    FILE *distances_file;           /**< Open handle to distances.csv (per-zone ToF frames from multizone boards); NULL when disabled or device is not the controller. */

    /** Invoked once per NEW strobe reported by a Rev 1.33+ controller, with the COB
     *  that actually fired. Signature: void (*)(void *userData, unsigned long counter, int light).
     *  Left NULL for pre-1.33 firmware, which does not emit the strobe fields. */
    void * strobeCallback;
    void * strobeUserData;          /**< Opaque pointer handed back to strobeCallback. */
    unsigned long lastStrobeCounter;/**< Most recent strobe index seen from the controller. */
    int lastStrobeLight;            /**< COB that fired on that strobe; -1 if unknown/suppressed. */
    char haveStrobeFields;          /**< 1 once the controller has been observed emitting strobe fields. */
    char csvHeaderPending;          /**< 1 while the controller CSV header still has to be written; it is deferred until the first data line reveals how many columns this firmware emits. */

    int controllerVersionMajor;     /**< Parsed from the "V:major.minor" banner; 0 until seen. */
    int controllerVersionMinor;     /**< Parsed from the "V:major.minor" banner; 0 until seen. */

    /** COB the controller last reported as lit (0-indexed), -1 when none is. Read out
     *  of the Light1..Light6 columns, which every firmware generation emits — unlike
     *  the strobe fields, this feedback is available on a Nano too. */
    int currentLight;
    /** COB the last corrective '+' was issued for, so --skipadvance pushes once per
     *  step onto an excluded COB rather than once per reported line. -1 = idle. */
    int lastSkipCorrection;
} ArduinoSerialConfig;

/** Does this controller understand the Rev 1.33 multi-character commands (e/E/S)?
 *
 *  This gate is not optional. A pre-1.33 controller consumes one byte per loop
 *  tick and dispatches every one of them through its single-character switch, so
 *  a schedule upload like "S031425" is executed as: 'S' (ignored), '0' (ALL LIGHTS
 *  OFF), '3' (light 3), '1' (light 1)... — it would scramble the board rather than
 *  be ignored. Never send e/E/S unless this returns 1. */
int arduino_controllerSupportsExposureLock(const ArduinoSerialConfig * context);

/** Ask the controller for its version banner ("v"). The reply is parsed
 *  asynchronously by the serial thread into controllerVersion{Major,Minor}. */
int arduino_probeControllerVersion(ArduinoSerialConfig * context);



int arduino_signalNewFrame(ArduinoSerialConfig * context);

/** Hand light sequencing to the controller ("e"). From this point the controller
 *  advances one schedule entry per camera exposure, in its own ISR, and the host
 *  stops stepping lights per frame entirely. Removes the host->controller round
 *  trip from the per-frame critical path. */
int arduino_enterExposureLockedMode(ArduinoSerialConfig * context);

/** Set the controller's maximum light-on window in microseconds ("E<us>").
 *
 *  SAFETY: the COBs are overvolted and this value bounds how long any one of them
 *  can be driven. The controller hard-clamps whatever it receives, so this can
 *  only ever tighten the limit, never loosen it past the firmware ceiling.
 *  It must be sent: the controller charges its per-COB thermal budget in units of
 *  this ceiling, so leaving it at the 1000us default while running a 450us
 *  exposure over-charges every strobe by >2x and provokes needless COB
 *  substitutions. Pass the real exposure plus a small margin. */
int arduino_setLightOnCeiling(ArduinoSerialConfig * context, unsigned int microseconds);

/** Activate one specific light by index ("1".."6"), 0-indexed on the wire.
 *
 *  This is the stepping primitive for LEGACY controllers. They cannot hold a
 *  schedule, but they have understood single-digit light selection since 0.x, so a
 *  host-side policy can drive them by naming the COB it wants rather than sending
 *  '+' and hoping the board's internal cursor agrees. One byte per frame, exactly
 *  like '+', but with no shared cursor to drift out of step.
 *  Works on every firmware generation. */
int arduino_sendLightSelect(ArduinoSerialConfig * context, int lightIndex);

/** Upload a strobe schedule ("S<digits>"). Entries are 0-indexed COBs. Applied by
 *  the controller at its next cycle boundary, so arrival timing does not matter —
 *  unlike the per-frame '+' it replaces, where a late byte corrupted the
 *  frame-to-light mapping.
 *  @return 1 if the schedule was written to the port, 0 otherwise. */
int arduino_sendLightSchedule(ArduinoSerialConfig * context, const unsigned char * schedule, int length);

void *arduino_thread(void *arg);


#ifdef __cplusplus
}
#endif



#endif // ARDUINOSENSOR_H_INCLUDED
