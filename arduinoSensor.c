#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>

#include "arduinoSensor.h"


int arduino_startStream(ArduinoSerialConfig * context)
{
    context->serial_fd = open(context->port_name, O_RDWR | O_NOCTTY);
    if (context->serial_fd == -1)
    {
        fprintf(stderr,RED "Failed to open serial port %s\n" NORMAL,context->port_name);
        return 1;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(context->serial_fd, &tty) != 0)
    {
        perror("Failed to get serial attributes");
        close(context->serial_fd);
        return 1;
    }

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    tty.c_cflag = (CLOCAL | CREAD | CS8);
    tty.c_iflag = IGNPAR;
    tty.c_oflag = 0;
    tty.c_lflag = 0;

    tcflush(context->serial_fd, TCIFLUSH);
    if (tcsetattr(context->serial_fd, TCSANOW, &tty) != 0)
    {
        perror("Failed to set serial attributes");
        close(context->serial_fd);
        return 1;
    }

  //Send Start Command!
  char buffer[]={"i\n"};
  int n = write(context->serial_fd, buffer, sizeof(buffer)-1);
  tcdrain(context->serial_fd);
  fprintf(stderr,"Send start command (%u / %lu bytes)\n",n,sizeof(buffer)-1);

  return 0;
}


int arduino_stopStream(ArduinoSerialConfig * context)
{
    context->running = 1;

    fprintf(stderr,"Arduino stopping stream\n");
    char buffer[]={"f\n"};
    int n = write(context->serial_fd, buffer, sizeof(buffer)-1);
    tcdrain(context->serial_fd);
    fprintf(stderr,"Send stop command (%u / %lu bytes)\n",n,sizeof(buffer)-1);

    close(context->serial_fd);
    return 0;
}


int appendDataAfterInjectingTimestamp(FILE * fd,char * buffer,int bufferSize,int * halfway,unsigned long timestamp, unsigned long *receivedFrames)
{
  #if USE_SIMPLE_FRAME_APPEND
  fprintf(fd, "%s", buffer);
  return 1;
  #else
  int c = 0;
  char * lineStart = buffer;

  char alreadyAtNewline = 0;

  while (c<bufferSize)
  {
    switch (buffer[c])
    {
        //------------------------------------------
        case 0:
          //Reach end of buffer, flush the rest
          fprintf(fd, "%s", lineStart);
          alreadyAtNewline = 0; //<- not needed but for sake of having a correct state
          return 1;
        break;
        //------------------------------------------
        case 10:
        case 13:
          //Reached new line on buffer, flush it
          *halfway=0;
          buffer[c] = 0;
          if (!alreadyAtNewline) //Only print one new line in case of CR LF ( 10 13 )
            { fprintf(fd, "%s\n", lineStart);
              *receivedFrames+=1;
            }
          lineStart = buffer + c + 1;
          alreadyAtNewline = 1;
        break;
        //------------------------------------------
        default:
          //Reached normal character, inser timestamp if its the first
          if (*halfway==0)
          {
            fprintf(fd, "%lu,", timestamp);
            *halfway=1;
          }
          alreadyAtNewline = 0;
        break;
        //------------------------------------------
    }
    ++c;
  }
  return 1;
  #endif // USE_SIMPLE_FRAME_APPEND
}


void *arduino_thread(void *arg)
{
    ArduinoSerialConfig *config = (ArduinoSerialConfig *)arg;
    GlobalConfig *cfg = config->global;

    char enabledFileOutput = (strcmp(cfg->outputDirectory,"/dev/null")!=0);

    #define BUFFER_SIZE 256
    char buffer[BUFFER_SIZE + 1]={0};

    if (arduino_startStream(config)==0)
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
     fprintf(config->csv_file,"timestamp,dev_timestamp,accX,accY,accY\n");

     unsigned long arduinoStartTime = GetTickCountMicroseconds();

      int halfway = 0;
      while (*config->keep_running)
      {
        config->running = 1;

        int n = read(config->serial_fd, buffer, BUFFER_SIZE - 1);
	    unsigned long receptionTime = GetTickCountMicroseconds();
        if (n > 0)
        {
            buffer[n] = 0;//We can do this because buffer is 1 byte bigger

            appendDataAfterInjectingTimestamp(config->csv_file,buffer,n,&halfway,receptionTime,&config->receivedDataFrames);

            buffer[0] = 0;//Always have a printable buffer
            //fflush(config->csv_file);
        }


        double timeElapsedInSeconds = (double) ((double) (receptionTime-arduinoStartTime)/(double) 1000000.0);
        double computeRate = (double) config->receivedDataFrames/timeElapsedInSeconds;
        config->Hz = (float) computeRate;
        //usleep(1000);
      }

      fclose(config->csv_file);

      arduino_stopStream(config);
    }
    return NULL;
}

