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


int serialport_init(const char* serialport, int baud)
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

    usleep(100000); // Small delay

    flags &= ~TIOCM_DTR;  // Clear DTR
    flags &= ~TIOCM_RTS;  // Clear RTS
    ioctl(fd, TIOCMSET, &flags);

    usleep(2000000);  // Allow 2 sec Arduino to reset
    //---------------------------------------------------

    return fd;
}


int serialport_close( int fd )
{
    return close( fd );
}




int arduino_startStream(ArduinoSerialConfig * context)
{
  context->serial_fd = serialport_init(context->port_name,context->baud_rate);

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
        // Cast back to the correct function pointer type before calling
        int (*callback_func)(ArduinoSerialConfig *,unsigned long, unsigned long, double, double, double) =
                (int (*)(ArduinoSerialConfig *,unsigned long, unsigned long, double, double, double)) arduino_config->callback;

        return callback_func(arduino_config, timestamp, dev_timestamp, accX, accY, accZ);  // Call the function
    }
    return 0;  // Indicate failure if no callback is set
}




int process_accelerometer_data(ArduinoSerialConfig *arduino_config, char enabledFileOutput, unsigned long timestamp,const char *line, unsigned int lineLength)
{
 unsigned long dev_timestamp=0;
 int rawX=0, rawY=0, rawZ=0;

 // Parse the comma-separated values
 if (sscanf(line, "%lu,%d,%d,%d", &dev_timestamp, &rawX, &rawY, &rawZ) != 4)
    {
        fprintf(stderr, "Error: Invalid format in line: %s\n", line);
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
        // Cast back to the correct function pointer type before calling
        int (*callback_func)(ArduinoSerialConfig *,unsigned long, const char *, unsigned int) =
                (int (*)(ArduinoSerialConfig *,unsigned long, const char *, unsigned int)) arduino_config->callback;

        return callback_func(arduino_config, timestamp, line, lineLength);  // Call the function
    }
    return 0;  // Indicate failure if no callback is set
}



static int arduino_call_string_callback(ArduinoSerialConfig *arduino_config,unsigned long timestamp,const char * line,unsigned int lineLength)
{
    if (arduino_config->callback)
    {
    //fprintf(stderr,"Arduino callback for %s received %s\n",arduino_config->csv_name,line);

    unsigned long dev_timestamp=0;

    int Button1;
    int Distance1;
    int Distance2;
    int Distance3;

    int Light1;
    int Light2;
    int Light3;
    int Light4;
    int Light5;
    int Light6;

    // Parse the comma-separated values
    if (sscanf(line, "%lu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &dev_timestamp, &Button1, &Distance1, &Distance2, &Distance3, &Light1, &Light2, &Light3, &Light4, &Light5, &Light6) == 11)
    {
         // Cast back to the correct function pointer type before calling
        int (*callback_func)(ArduinoSerialConfig *,unsigned long, int, int, int, int, int, int, int, int, int, int) =
                (int (*)(ArduinoSerialConfig *,unsigned long, int, int, int, int, int, int, int, int, int, int)) arduino_config->callback;

        return callback_func(arduino_config, timestamp, Button1, Distance1, Distance2, Distance3, Light1, Light2, Light3, Light4, Light5, Light6);  // Call the function

        //printf("Parsed values - Timestamp: %lu, Button1: %d, Distance1: %d, Distance2: %d, Distance3: %d\n", dev_timestamp, Button1, Distance1, Distance2, Distance3);
        return 1;  // Success
    } else
    {
        fprintf(stderr, "Error: Invalid format in line: %s\n", line);
        return 0; // Error in parsing
    }

    }
    return 0;
}


int process_generic_string_data(ArduinoSerialConfig *config, char enabledFileOutput,  unsigned long timestamp,const char *lineBuffer, unsigned int lineIndex)
{
    //We write raw data to disk witout even processing it
    if (enabledFileOutput)
     { fprintf(config->csv_file, "%lu,%s\n", timestamp, lineBuffer); }

    if (config->callback)
                   {
                    //Pass our CSV line to a callback function!
                     return arduino_call_string_callback(config,timestamp,lineBuffer,lineIndex);
                   }
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
           {  fprintf(config->csv_file,"timestamp,dev_timestamp,Button1,Distance1,Distance2,Distance3,Light1,Light2,Light3,Light4,Light5,Light6\n"); }
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
                 {
                   //This is general data that is dumped without being processed
                   process_generic_string_data(config,enabledFileOutput,receptionTime,lineBuffer,lineIndex);
                   //any callbacks have been called at this point
                 }
                 //----------------------------------------------------------------------

                 lineIndex = 0;
                 config->receivedDataFrames+=1;
                 fflush(config->csv_file);
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

      arduino_stopStream(config);
    }


     free(buffer);
     free(lineBuffer);
    }
    return NULL;
}

