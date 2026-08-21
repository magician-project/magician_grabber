/** @file arduinoSensor.c
 *  @brief This is an abstraction layer for both Arduino/Teensy Serial devices that:
 *  Start emitting output after receiving "i"
 *  Emit CSV lines
 *  Stop emitting output after receiving "f"
 *  @author Ammar Qammaz (AmmarkoV)
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include <sys/ioctl.h>

#include "arduinoSensor.h"

const double ADC_RESOLUTION = 1024.0;  // ADC resolution 12 bit
const double SUPPLY_VOLTAGE = 3.3;     // Supply voltage (V)
const double ZERO_G_VOLTAGE = 1.65;    // 0g voltage(V)
const double SENSITIVITY    = 0.330;   // Sensitivity (V/g)
const double GRAVITY        = 9.81;    // Gravity acceleration (m/s^2)


int serialport_init(const char* serialport, int baud, int pico2Mode)
{
    struct termios toptions={0};

    //fd = open(serialport, O_RDWR | O_NOCTTY | O_NDELAY);
    int fd = open(serialport, O_RDWR | O_NOCTTY ); //  | O_NDELAY

    if (fd == -1)
    {
        fprintf(stderr,RED "serialport_init: Unable to open port\n" NORMAL);

        fprintf(stderr,"Maybe consider :\n");
        fprintf(stderr,"sudo usermod -a -G tty [username]");
        fprintf(stderr,"sudo usermod -a -G dialout [username]");
        fprintf(stderr,"\n OR \n");
        fprintf(stderr,"sudo chmod 777 %s ",serialport);
        fprintf(stderr,"to make sure the problem is not permissions related\n\n");
        int i = system("ps -A | grep arduino && echo \"Arduino device maybe mounted by arduino-ide, Stop it before running me..!\"");
        if (i!=0)
        {
         fprintf(stderr," Could not check for arduino-ide..\n");
        }

        return -1;
    }

    //int iflags = TIOCM_DTR;
    //ioctl(fd, TIOCMBIS, &iflags);    // turn on DTR
    //ioctl(fd, TIOCMBIC, &iflags);    // turn off DTR

    if (tcgetattr(fd, &toptions) < 0)
    {
        fprintf(stderr,RED "serialport_init: Couldn't get term attributes\n" NORMAL);
        return -1;
    }

    //To Debug:
    //screen /dev/ttyACM0 115200


    speed_t brate = baud; // let you override switch below if needed
    switch(baud)
    {
     case 4800:   brate=B4800;   break;
     case 9600:   brate=B9600;   break;
#ifdef B14400
     case 14400:  brate=B14400;  break;
#endif
     case 19200:  brate=B19200;  break;
#ifdef B28800
     case 28800:  brate=B28800;  break;
#endif
     case 38400:  brate=B38400;  break;
     case 57600:  brate=B57600;  break;
     case 115200: brate=B115200; break;
     default:
        fprintf(stderr,"Unsupported speed %u baud\n",baud);
        exit(1);
    }

    fprintf(stderr,"Baudrate set to %u baud\n",baud);
    cfsetispeed(&toptions, brate);
    cfsetospeed(&toptions, brate);

    /*
    toptions.c_cflag = (CLOCAL | CREAD | CS8);
    toptions.c_iflag = IGNPAR;
    toptions.c_oflag = 0;
    toptions.c_lflag = 0;

    // see: http://unixwiz.net/techtips/termios-vmin-vtime.html
    toptions.c_cc[VMIN]  = 0;
    toptions.c_cc[VTIME] = 1;
    //toptions.c_cc[VTIME] = 20;*/


    toptions.c_cflag &= ~PARENB;
    toptions.c_cflag &= ~CSTOPB;
    toptions.c_cflag &= ~CSIZE;
    toptions.c_cflag |= CS8;
    // ----- SOME ADDITIONAL SETTINGS, That may be required (NO FLOW CONTROL) -----//
    toptions.c_cflag &= ~CRTSCTS;
    toptions.c_cflag |= CREAD | CLOCAL;  // turn on READ & ignore ctrl lines
    toptions.c_iflag &= ~(IXON | IXOFF | IXANY); // turn off s/w flow ctrl
    toptions.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // make raw
    toptions.c_oflag &= ~OPOST; // make raw
    // see: http://unixwiz.net/techtips/termios-vmin-vtime.html
    toptions.c_cc[VMIN]  = 0; // Don't expect bytes, we handle this in our read function.
    toptions.c_cc[VTIME] = 0; // 10 tenth of a second: VTIME * 100ms.

    tcflush(fd, TCIFLUSH);
    if( tcsetattr(fd, TCSANOW, &toptions) < 0)
    {
        fprintf(stderr,RED "init_serialport: Couldn't set term attributes TCSANOW" NORMAL);
        return -1;
    }

    if( tcsetattr(fd, TCSAFLUSH, &toptions) < 0)
    {
        fprintf(stderr,RED "init_serialport: Couldn't set term attributes TCSAFLUSH" NORMAL);
        return -1;
    }


    //This is to ensure connection with the arduino is up
    //---------------------------------------------------
    // Toggle DTR/RTS
    int flags;
    ioctl(fd, TIOCMGET, &flags);
    flags |= TIOCM_DTR;  // Set DTR
    flags |= TIOCM_RTS;  // Set RTS
    ioctl(fd, TIOCMSET, &flags);

    //usleep(100000); // Small delay //Removed 30/7/2025 Michele's fix to align timestamps properly

    if (!pico2Mode)
    {
      // Default (legacy AVR Arduino Nano deployed in production): clear DTR/RTS.
      // The Nano's USB-serial bridge treats DTR as an edge-triggered auto-reset
      // pulse, so clearing it here is the established, working behaviour.
      flags &= ~TIOCM_DTR;  // Clear DTR
      flags &= ~TIOCM_RTS;  // Clear RTS
      ioctl(fd, TIOCMSET, &flags);
    }
    else
    {
      // --pico2 (experimental RP2350/Pico 2 multizone board): KEEP DTR/RTS
      // asserted. The Pico 2 uses native USB-CDC which gates all transmission on
      // the host's DTR line (TinyUSB tud_cdc_connected() is false until DTR is
      // asserted); clearing it makes the firmware run but silently drop every
      // Serial.print(), so the host receives zero bytes. Leaving DTR asserted is
      // required here.
      fprintf(stderr,"Pico2 mode: keeping DTR/RTS asserted so the native USB-CDC will transmit\n");
    }

    //usleep(2000000);  // Allow 2 sec Arduino to reset //Removed 30/7/2025 Michele's fix to align timestamps properly
    //---------------------------------------------------

    return fd;
}


int serialport_close( int fd )
{
    return close( fd );
}




int arduino_startStream(ArduinoSerialConfig * context)
{
  int pico2Mode = (context->global!=0) ? context->global->isPico2 : 0;
  context->serial_fd = serialport_init(context->port_name,context->baud_rate,pico2Mode);

  //Ask the lighting controller for its version banner first. The reply lands
  //asynchronously in the read loop below and gates every multi-character command
  //we might send afterwards: a pre-1.33 controller would execute the individual
  //characters of "S031425" as separate commands and end up with its lights in an
  //arbitrary state. The Teensy shares this start path but has no such protocol.
  if (strstr(context->csv_name,"controller")!=0)
     { arduino_probeControllerVersion(context); }

  //Send Start Command!
  char buffer[]={"i\n"};
  int n = write(context->serial_fd, buffer, sizeof(buffer)-1);

  if (n<=0)
  {
    fprintf(stderr,"Sending start command to %s failed (%d)\n",context->port_name,n);
    exit(1);
  }
  fprintf(stderr,"Send start command (%u / %lu bytes) to %s , wrote %u bytes \n",n,sizeof(buffer)-1,context->port_name,n);

  usleep(1000);

  if (context->extraCommands!=0)
  {
   int n = write(context->serial_fd, context->extraCommands, strlen(context->extraCommands));
   if (n<0)
   {
    fprintf(stderr,"Sending extra command to %s failed (%d)\n",context->port_name,n);
    exit(1);
   }
   fprintf(stderr,"Send extra command (%u / %lu bytes) to %s , wrote %u bytes \n",n,strlen(context->extraCommands),context->port_name,n);
  }

  //tcdrain(context->serial_fd);

  return 0;
}

int arduino_signalNewFrame(ArduinoSerialConfig * context)
{
  if (context!=0)
  {
    if (context->global->useArduino)
    {
     //fprintf(stderr,"Trigger light change to %s \n",context->port_name);
     char buffer[]={"+\n"};
     int n = write(context->serial_fd, buffer, sizeof(buffer)-1);

     if (n<=0)
     {
       fprintf(stderr,"Sending new frame command to %s failed (%d)\n",context->port_name,n);
       return 0;
     }

     return 1;
    }
  }
  return 0;
}


int arduino_controllerSupportsExposureLock(const ArduinoSerialConfig * context)
{
  if (context==0) { return 0; }
  if (context->controllerVersionMajor > 1)  { return 1; }
  if (context->controllerVersionMajor == 1) { return (context->controllerVersionMinor >= 33); }
  return 0; //no banner seen -> assume legacy and stay on the '+' path
}

int arduino_probeControllerVersion(ArduinoSerialConfig * context)
{
  if (context==0)                   { return 0; }
  if (!context->global->useArduino) { return 0; }

  char buffer[]={"v\n"};
  int n = write(context->serial_fd, buffer, sizeof(buffer)-1);
  return (n>0);
}

int arduino_enterExposureLockedMode(ArduinoSerialConfig * context)
{
  if (context==0)                 { return 0; }
  if (!context->global->useArduino) { return 0; }
  if (!arduino_controllerSupportsExposureLock(context)) { return 0; }

  char buffer[]={"e\n"};
  int n = write(context->serial_fd, buffer, sizeof(buffer)-1);
  if (n<=0)
  {
    fprintf(stderr,RED "Failed handing light sequencing to the controller (%d)\n" NORMAL,n);
    return 0;
  }
  fprintf(stderr,GREEN "Controller now owns light sequencing (one step per exposure)\n" NORMAL);
  return 1;
}

int arduino_setLightOnCeiling(ArduinoSerialConfig * context, unsigned int microseconds)
{
  if (context==0)                 { return 0; }
  if (!context->global->useArduino) { return 0; }
  if (!arduino_controllerSupportsExposureLock(context)) { return 0; }

  char buffer[32]={0};
  int len = snprintf(buffer,sizeof(buffer),"E%u\n",microseconds);
  int n = write(context->serial_fd, buffer, len);
  if (n<=0)
  {
    fprintf(stderr,RED "Failed setting light-on ceiling (%d)\n" NORMAL,n);
    return 0;
  }
  fprintf(stderr,"Requested controller light-on ceiling of %u usec (controller hard-clamps this)\n",microseconds);
  return 1;
}

int arduino_sendLightSelect(ArduinoSerialConfig * context, int lightIndex)
{
  if (context==0)                   { return 0; }
  if (!context->global->useArduino) { return 0; }
  if (lightIndex<0)                 { return 0; }
  if (lightIndex>8)                 { return 0; } //'1'..'9' is all the wire protocol has

  //Deliberately no version gate: single-digit light selection predates every other
  //command here and behaves identically on the Nano and on the Pico.
  char buffer[3] = { (char)('1' + lightIndex), '\n', 0 };
  int n = write(context->serial_fd, buffer, 2);
  if (n<=0)
  {
    fprintf(stderr,RED "Failed selecting light %d on %s (%d)\n" NORMAL,lightIndex+1,context->port_name,n);
    return 0;
  }
  return 1;
}

int arduino_sendLightSchedule(ArduinoSerialConfig * context, const unsigned char * schedule, int length)
{
  if (context==0)                   { return 0; }
  if (schedule==0)                  { return 0; }
  if (length<=0)                    { return 0; }
  if (!context->global->useArduino) { return 0; }
  if (!arduino_controllerSupportsExposureLock(context)) { return 0; }

  //"S" + one digit per entry + "\n" + NUL
  char buffer[64]={0};
  if (length > (int) sizeof(buffer)-3) { length = (int) sizeof(buffer)-3; }

  int at = 0;
  buffer[at++] = 'S';
  for (int i=0; i<length; i++)
  {
    if (schedule[i]>9) { continue; } //single-digit protocol, 6 COBs so this cannot legitimately trip
    buffer[at++] = (char) ('0' + schedule[i]);
  }
  buffer[at++] = '\n';

  int n = write(context->serial_fd, buffer, at);
  if (n<=0)
  {
    fprintf(stderr,RED "Failed sending light schedule to %s (%d)\n" NORMAL,context->port_name,n);
    return 0;
  }
  return 1;
}


int arduino_stopStream(ArduinoSerialConfig * context)
{
    context->running = 1;

    fprintf(stderr,"Arduino stopping stream\n");

    char stoplights[]={"0\n"};
    int n = write(context->serial_fd, stoplights, sizeof(stoplights)-1);
    fprintf(stderr,"Send stop lights command (%u / %lu bytes) to %s \n",n,sizeof(stoplights)-1,context->port_name);


    char buffer[]={"f\n"};
    n = write(context->serial_fd, buffer, sizeof(buffer)-1);
    //tcdrain(context->serial_fd);
    fprintf(stderr,"Send stop command (%u / %lu bytes) to %s \n",n,sizeof(buffer)-1,context->port_name);

    serialport_close(context->serial_fd);
    return 0;
}



int arduino_call_accelerometer_callback(ArduinoSerialConfig *arduino_config, unsigned long timestamp, unsigned long dev_timestamp, double accX, double accY, double accZ)
{
    if (arduino_config->callback)
    {
        int (*callback_func)(ArduinoSerialConfig *,unsigned long, unsigned long, double, double, double);
        memcpy(&callback_func, &arduino_config->callback, sizeof(callback_func));
        return callback_func(arduino_config, timestamp, dev_timestamp, accX, accY, accZ);
    }
    return 0;  // Indicate failure if no callback is set
}




int process_accelerometer_data(ArduinoSerialConfig *arduino_config, char enabledFileOutput, unsigned long timestamp,const char *line, unsigned int lineLength)
{
 (void)lineLength;
 unsigned long dev_timestamp=0;
 int rawX=0, rawY=0, rawZ=0;

 // Parse the comma-separated values
 if (sscanf(line, "%lu,%d,%d,%d", &dev_timestamp, &rawX, &rawY, &rawZ) != 4)
    {
        fprintf(stderr, "Error: process_accelerometer_data Invalid format in line: %s\n", line);
        return 0; // Error in parsing
    }

 //These could be

 // voltage conversion
 double voltageX = (double) ((double) rawX / ADC_RESOLUTION) * SUPPLY_VOLTAGE;
 double voltageY = (double) ((double) rawY / ADC_RESOLUTION) * SUPPLY_VOLTAGE;
 double voltageZ = (double) ((double) rawZ / ADC_RESOLUTION) * SUPPLY_VOLTAGE;

 // acceleration conversion (g)
 double accelX_g = (double) ((double)voltageX - ZERO_G_VOLTAGE) / SENSITIVITY;
 double accelY_g = (double) ((double)voltageY - ZERO_G_VOLTAGE) / SENSITIVITY;
 double accelZ_g = (double) ((double)voltageZ - ZERO_G_VOLTAGE) / SENSITIVITY;

 //  acceleration conversion (m/s²)
 double accelX_ms2 = (double) accelX_g * GRAVITY;
 double accelY_ms2 = (double) accelY_g * GRAVITY;
 double accelZ_ms2 = (double) accelZ_g * GRAVITY;

 //  print processed value to file
 if (enabledFileOutput)
   { fprintf(arduino_config->csv_file, "%lu,%lu,%f,%f,%f\n",timestamp,dev_timestamp,accelX_ms2,accelY_ms2,accelZ_ms2); }

  if (arduino_config->callback)
                   {
                    //Pass our data to a callback function!
                    return arduino_call_accelerometer_callback(arduino_config,timestamp,dev_timestamp,accelX_ms2,accelY_ms2,accelZ_ms2);
                   }


 return 1;
}


int arduino_call_string_callback_generic(ArduinoSerialConfig *arduino_config, unsigned long timestamp, const char *line, unsigned int lineLength)
{
    if (arduino_config->callback)
    {
        int (*callback_func)(ArduinoSerialConfig *,unsigned long, const char *, unsigned int);
        memcpy(&callback_func, &arduino_config->callback, sizeof(callback_func));
        return callback_func(arduino_config, timestamp, line, lineLength);
    }
    return 0;  // Indicate failure if no callback is set
}



static int arduino_call_string_callback(ArduinoSerialConfig *arduino_config,unsigned long timestamp,const char * line,unsigned int lineLength)
{
    (void)lineLength;
    if (arduino_config->callback)
    {
    //fprintf(stderr,"Arduino callback for %s received %s\n",arduino_config->csv_name,line);

    unsigned long dev_timestamp=0;

    int Button1;
    int Button2;
    int Distance1;
    int Distance2;
    int Distance3;

    int Light1;
    int Light2;
    int Light3;
    int Light4;
    int Light5;
    int Light6;

    char distanceStrA[128]={0};
    char distanceStrB[128]={0};
    char distanceStrC[128]={0};
    // Parse the comma-separated values
    int res = sscanf(line, 
    "%lu,%d,%d,%127[^,],%127[^,],%127[^,],%d,%d,%d,%d,%d,%d", &dev_timestamp, &Button1,  &Button2, distanceStrA, distanceStrB, distanceStrC, &Light1, &Light2, &Light3, &Light4, &Light5, &Light6); 
    if ( res == 12)
    {

        Distance1 = atoi(distanceStrA);  
        Distance2 = atoi(distanceStrB);  
        Distance3 = atoi(distanceStrC);  

        int (*callback_func)(ArduinoSerialConfig *,unsigned long, int, int, int, int, int, int, int, int, int, int, int);
        memcpy(&callback_func, &arduino_config->callback, sizeof(callback_func));
        return callback_func(arduino_config, timestamp, Button1, Button2, Distance1, Distance2, Distance3, Light1, Light2, Light3, Light4, Light5, Light6);

        //printf("Parsed values - Timestamp: %lu, Button1: %d, Distance1: %d, Distance2: %d, Distance3: %d\n", dev_timestamp, Button1, Distance1, Distance2, Distance3);
        return 1;  // Success
    } else
    {
        fprintf(stderr, "Error (%u): arduino_call_string_callback Invalid format in line: %s\n",res, line);
        return 0; // Error in parsing
    }

    }
    return 0;
}


/* Return a pointer just past the Nth comma of `line`, or NULL if it has fewer.
 * Used to reach the Rev 1.33 strobe fields without disturbing the 12-field parse
 * that the legacy controller callback still relies on. */
static const char * arduino_field(const char *line, int fieldIndex)
{
    const char *p = line;
    for (int i=0; i<fieldIndex; i++)
    {
        p = strchr(p, ',');
        if (p==0) { return 0; }
        p++;
    }
    return p;
}

/* Extract the Rev 1.33 strobe tail and notify the registered hook on each NEW
 * strobe. The controller line is:
 *   dev_ts,b1,b2,d0,d1,d2,L0..L5,strobeCounter,strobeLight,subs,starved,wdt
 * so strobeCounter is field 12 and strobeLight is field 13 (0-indexed).
 * Pre-1.33 firmware simply lacks these fields and this becomes a no-op. */
static void arduino_process_strobe_fields(ArduinoSerialConfig *config, const char *line)
{
    const char *p = arduino_field(line, 12);
    if (p==0) { return; }

    unsigned long counter = 0;
    int light = -1;
    if (sscanf(p, "%lu,%d", &counter, &light) != 2) { return; }

    config->haveStrobeFields = 1;

    if (counter == config->lastStrobeCounter) { return; } //same strobe, nothing new
    config->lastStrobeCounter = counter;
    config->lastStrobeLight   = light;

    if (config->strobeCallback)
    {
        void (*strobe_func)(void *, unsigned long, int);
        memcpy(&strobe_func, &config->strobeCallback, sizeof(strobe_func));
        strobe_func(config->strobeUserData, counter, light);
    }
}

/* Track which COB the controller says is lit, and — under --skipadvance — push the
 * board past one that --skip excluded.
 *
 * The Light1..Light6 columns sit at fields 6..11 and are emitted by every firmware
 * generation, so this is the only light feedback that works on a Nano, a 1.32 Pico
 * and a 1.33+ Pico alike. Reacting to what the controller REPORTS, rather than to a
 * host-side model of where its cursor ought to be, is what makes this safe on the
 * pattern modes: the firmware keeps choosing the order and the host only vetoes
 * entries. A dropped '+' cannot desynchronise anything, because there is no shared
 * cursor being counted — the next report re-states the truth.
 *
 * The extra '+' goes out once per transition ONTO an excluded COB, so a controller
 * reporting at hundreds of Hz still produces exactly one nudge per unwanted step. If
 * the COB it lands on is also excluded, the next report pushes again; parse_arguments
 * guarantees at least one COB is in play, so this always terminates. */
static void arduino_process_light_fields(ArduinoSerialConfig *config, const char *line)
{
    const char *p = arduino_field(line, 6);
    if (p==0) { return; }

    int lit = -1;
    for (int i=0; i<LIGHTS_ON_CONTROLLER; i++)
    {
        if (atoi(p)!=0) { lit = i; break; }
        //Nothing has to follow the last light column: on a pre-1.33 controller with
        //every COB off, Light6 is the end of the row. Looking for a comma there would
        //throw away a perfectly good "no light is on" report.
        if (i+1 >= LIGHTS_ON_CONTROLLER) { break; }
        p = strchr(p,',');
        if (p==0) { return; } //row is shorter than the light block, so it is not one
        p++;
    }

    config->currentLight = lit;

    if (config->global==0)                { return; }
    if (config->global->lightSkipMask==0) { return; }
    if (!config->global->skipByAdvancing) { return; }

    if ( (lit<0) || (!lightIsSkipped(config->global,lit)) )
    {
      config->lastSkipCorrection = -1; //back on a COB we want; re-arm
      return;
    }

    if (config->lastSkipCorrection == lit) { return; } //already pushed past this one
    config->lastSkipCorrection = lit;

    arduino_signalNewFrame(config); //a bare '+': let the board pick its own next entry
}

/* Emit the controller.csv header, sized to the row format this controller actually
 * produces.
 *
 * The header cannot be written at file-open time: how many columns a controller
 * emits depends on its firmware, and that is not known until the first data line
 * arrives. Rev 1.33 appends five strobe/thermal fields; the Nano (0.28) and Pico
 * 1.32 emit the original twelve. Writing the wide header unconditionally produced
 * a header/row column mismatch for every pre-1.33 board. */
static void arduino_write_controller_csv_header(ArduinoSerialConfig *config, const char *firstLine)
{
    unsigned int fields = 1;
    for (const char *p=firstLine; *p!=0; p++) { if (*p==',') { fields++; } }

    fprintf(config->csv_file,
            "timestamp,dev_timestamp,Button1,Button2,Distance1,Distance2,Distance3,"
            "Light1,Light2,Light3,Light4,Light5,Light6");

    unsigned int named = 12; //fields covered by the columns above
    if (fields>=17)
    {
        //Rev 1.33+ ground truth. StrobeLight is the COB that ACTUALLY fired, which
        //may differ from Light1..6 when the controller substituted a cooler COB.
        fprintf(config->csv_file,",StrobeCounter,StrobeLight,Substitutions,Starved,WatchdogTrips");
        named = 17;
    }

    //Any columns beyond what we recognise still get a name, so the CSV stays
    //rectangular against a firmware newer than this build.
    for (unsigned int extra=named; extra<fields; extra++)
       { fprintf(config->csv_file,",Extra%u",extra); }

    fprintf(config->csv_file,"\n");
    config->csvHeaderPending = 0;
}

int process_generic_string_data(ArduinoSerialConfig *config, char enabledFileOutput,  unsigned long timestamp,const char *lineBuffer, unsigned int lineIndex)
{
    //We write raw data to disk witout even processing it
    if (enabledFileOutput)
     {
      if (config->csvHeaderPending)
         { arduino_write_controller_csv_header(config,lineBuffer); }
      fprintf(config->csv_file, "%lu,%s\n", timestamp, lineBuffer);
     }

    //Rev 1.33+ controllers append the strobe ground truth; feed it to any listener.
    arduino_process_strobe_fields(config,lineBuffer);

    //Every generation reports which COB is lit; that drives the --skipadvance veto.
    arduino_process_light_fields(config,lineBuffer);

    if (config->callback)
                   {
                    //Pass our CSV line to a callback function!
                     return arduino_call_string_callback(config,timestamp,lineBuffer,lineIndex);
                   }
    return 1;
}


/* Handle a per-zone ToF frame line emitted by a multizone board ("x1/x2/x3,dev_ts,z0,...,z63").
 * These lines are written verbatim to distances.csv (never to controller.csv) and are stamped
 * with the host timestamp of the controller frame they belong to, so distances.csv and
 * controller.csv share identical timestamps and can be joined row-for-row. */
int process_distance_zone_data(ArduinoSerialConfig *config, char enabledFileOutput, unsigned long timestamp,const char *lineBuffer, unsigned int lineIndex)
{
    (void)lineIndex;
    if (enabledFileOutput && (config->distances_file!=0))
       { fprintf(config->distances_file, "%lu,%s\n", timestamp, lineBuffer); }
    return 1;
}



void *arduino_simulated_thread(void *arg)
{
   ArduinoSerialConfig *config = (ArduinoSerialConfig *)arg;
   //GlobalConfig *cfg = config->global;



   double step = 0.0;
   unsigned long arduinoStartTime = GetTickCountMicroseconds();
   while (*config->keep_running)
   {
     config->running = 1;

     unsigned long receptionTime = GetTickCountMicroseconds();
	 double accelX_ms2 = (double) step;
	 double accelY_ms2 = (double) step;
	 double accelZ_ms2 = (double) step;

     step += 0.1;
     config->receivedDataFrames+=1;

    if (config->callback)
                   {
                    //Pass our data to a callback function!
                     if (arduino_call_accelerometer_callback(config,receptionTime,receptionTime,accelX_ms2,accelY_ms2,accelZ_ms2) == 0)
                     {
                         fprintf(stderr,"Arduino Callback failed to complete \n");
                     }
                   }

     if (step>1000.0) { step = 0; }
     usleep(250);


     double timeElapsedInSeconds = (double) ((double) (receptionTime-arduinoStartTime)/(double) 1000000.0);
     double computeRate = 0.0;
     if (timeElapsedInSeconds!=0.0)
           { computeRate = (double) config->receivedDataFrames/timeElapsedInSeconds; }
     config->Hz = (float) computeRate;
   }

   return NULL;
}



void *arduino_thread(void *arg)
{
    ArduinoSerialConfig *config = (ArduinoSerialConfig *)arg;
    GlobalConfig *cfg = config->global;

    //"no light reported yet" and "no correction outstanding" are both -1, and the
    //positional struct initialisers in main() cannot say that.
    config->currentLight       = -1;
    config->lastSkipCorrection = -1;

    if (cfg->simulate)
    {
        return arduino_simulated_thread(arg);
    }


    char enabledFileOutput = (strcmp(cfg->outputDirectory,"/dev/null")!=0);

    #define BUFFER_SIZE 10000//10K buffer is big
    char * buffer = (char*) malloc(sizeof(char) * (BUFFER_SIZE + 1) );

    #define LINE_BUFFER_SIZE 1024//1K buffer is big
    char * lineBuffer = (char*) malloc(sizeof(char) * (LINE_BUFFER_SIZE + 1) );
    int lineIndex = 0;      // Current write position
    unsigned long lastFrameTimestamp = 0;  // Host timestamp of the most recent controller frame; reused for its x1/x2/x3 zone lines


    if ( (buffer!=0) && (lineBuffer!=0) )
    {
     memset(buffer,0,sizeof(char) * (BUFFER_SIZE + 1));
     memset(lineBuffer,0,sizeof(char) * (LINE_BUFFER_SIZE + 1));

     if (arduino_startStream(config)==0)
     {

       if (enabledFileOutput)
       {
        char fullCSVOutputPath[2048]={0};
        snprintf(fullCSVOutputPath,2048,"%s/%s",cfg->outputDirectory,config->csv_name);
        config->csv_file = fopen(fullCSVOutputPath, "w");
        if (!config->csv_file)
        {
           perror("Failed to open CSV file");
           close(config->serial_fd);
           return NULL;
        } else
        {
           fprintf(stderr,"Opened %s for output\n",fullCSVOutputPath);
        }
       }

     //Inject CSV headers where needed..
     int isAccelerometerData = 0;
     if (strstr(config->csv_name,"accelerometer")!=0)
        {
         if (enabledFileOutput)
            { fprintf(config->csv_file,"timestamp,dev_timestamp,accX,accY,accZ\n"); }
         isAccelerometerData = 1;
        }
     if (strstr(config->csv_name,"controller")!=0)
        { if (enabledFileOutput)
           {
            //The controller header is deferred until the first data line, because its
            //column count depends on the firmware generation that answered. See
            //arduino_write_controller_csv_header().
            config->csvHeaderPending = 1;

            //Multizone boards stream full 8x8 (64-zone) ToF frames as extra "x1/x2/x3" lines.
            //Split those out into a separate distances.csv sitting next to controller.csv.
            char fullDistancesOutputPath[2048]={0};
            snprintf(fullDistancesOutputPath,2048,"%s/distances.csv",cfg->outputDirectory);
            config->distances_file = fopen(fullDistancesOutputPath, "w");
            if (!config->distances_file)
               { perror("Failed to open distances CSV file"); }
            else
               {
                fprintf(stderr,"Opened %s for output\n",fullDistancesOutputPath);
                fprintf(config->distances_file,"timestamp,sensor,dev_timestamp");
                for (int z=0; z<64; z++) { fprintf(config->distances_file,",z%d",z); }
                fprintf(config->distances_file,"\n");
               }
           }
        }


     unsigned long arduinoStartTime = GetTickCountMicroseconds();


     while (*config->keep_running)
      {
        config->running = 1;

        int n = read(config->serial_fd, buffer, BUFFER_SIZE - 1);
	    unsigned long receptionTime = GetTickCountMicroseconds();

        if (n > 0)
        {
        //fprintf(stderr,"%s: Read %d / Buffer at %u / %s\n ",config->csv_name,n,lineIndex,lineBuffer);
        //buffer[n] = 0; // Ensure null-terminated string
        for (int i = 0; i < n; i++)
        {
            if (buffer[i] == 10 || buffer[i] == 13)
            {
                //This check makes sure we are not passing a double new line (CRLF) to the CSV..
                if (lineIndex!=0)
                {
                 // End of a valid line, flush it
                 lineBuffer[lineIndex] = 0;

                 //----------------------------------------------------------------------
                 if (isAccelerometerData)
                 {
                   //Accelerometer data needs extra processing to convert values
                   process_accelerometer_data(config,enabledFileOutput,receptionTime,lineBuffer,lineIndex);
                   //any callbacks have been called at this point
                 } else
                 if (lineBuffer[0]=='x')
                 {
                   //Per-zone ToF frame ("x1/x2/x3,...") from a multizone board.
                   //Route to distances.csv, reusing the timestamp of the most recent controller
                   //frame so both files stay timestamp-aligned. controller.csv is left untouched.
                   process_distance_zone_data(config,enabledFileOutput,lastFrameTimestamp,lineBuffer,lineIndex);
                 } else
                 if (lineBuffer[0]=='V')
                 {
                   //Firmware version banner (e.g. "V:1.33") - not CSV data, but it
                   //decides whether this controller can be given the Rev 1.33
                   //multi-character commands. See arduino_controllerSupportsExposureLock().
                   int maj=0, min=0;
                   if (sscanf(lineBuffer,"V:%d.%d",&maj,&min)==2)
                   {
                     config->controllerVersionMajor = maj;
                     config->controllerVersionMinor = min;
                     fprintf(stderr,"Controller on %s reports firmware v%d.%d\n",config->port_name,maj,min);
                   }
                 } else
                 {
                   //This is general data that is dumped without being processed
                   lastFrameTimestamp = receptionTime;
                   process_generic_string_data(config,enabledFileOutput,receptionTime,lineBuffer,lineIndex);
                   //any callbacks have been called at this point
                 }
                 //----------------------------------------------------------------------

                 lineIndex = 0;
                 config->receivedDataFrames+=1;
                 fflush(config->csv_file);
                 if (config->distances_file!=0) { fflush(config->distances_file); }
                }
            }
            else
            {
                if (lineIndex<LINE_BUFFER_SIZE-1)
                {
                 lineBuffer[lineIndex] = buffer[i];
                 lineIndex+=1;
                } else
                {
                 fprintf(stderr,"ARDUINO/TEENSY RECEIVE THREAD RUN OUT OF BUFFER SPACE\n");
                 lineIndex = 0;
                }
            }
        }
        }

        double timeElapsedInSeconds = (double) ((double) (receptionTime-arduinoStartTime)/(double) 1000000.0);
        double computeRate = 0.0;
        if (timeElapsedInSeconds!=0.0)
           { computeRate = (double) config->receivedDataFrames/timeElapsedInSeconds; }
        config->Hz = (float) computeRate;
        //usleep(1000);
      }

      fprintf(stderr,"Arduino Thread %s terminating\n",config->csv_name);

      if (enabledFileOutput)
         { fclose(config->csv_file); }

      if (config->distances_file!=0)
         { fclose(config->distances_file); config->distances_file=0; }

      arduino_stopStream(config);
    }


     free(buffer);
     free(lineBuffer);
    }
    return NULL;
}

