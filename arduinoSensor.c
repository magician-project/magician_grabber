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
        perror("Failed to open serial port");
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
  fprintf(stderr,"Send start command (%lu bytes)\n",sizeof(buffer)-1);

  return 0;
}


int arduino_stopStream(ArduinoSerialConfig * context)
{
    fprintf(stderr,"Arduino stopping stream\n");
    char buffer[]={"f\n"};
    int n = write(context->serial_fd, buffer, sizeof(buffer)-1);
    tcdrain(context->serial_fd);
    fprintf(stderr,"Send stop command (%lu bytes)\n",sizeof(buffer)-1);

    close(context->serial_fd);
    return 0;
}


void *arduino_thread(void *arg)
{
    ArduinoSerialConfig *config = (ArduinoSerialConfig *)arg;
    GlobalConfig *cfg = config->global;

    #define BUFFER_SIZE 256
    volatile char buffer[BUFFER_SIZE + 1]={0};

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
     fprintf(config->csv_file,"timestamp,accX,accY,accY\n");


      while (*config->keep_running)
      {
        int n = read(config->serial_fd, buffer, BUFFER_SIZE - 1);
	    unsigned long receptionTime = GetTickCountMicroseconds();
        if (n > 0)
        {
            buffer[n] = 0;//We can do this because buffer is 1 byte bigger
            fprintf(config->csv_file, "%s", buffer);
            buffer[0] = 0;//Always have a printable buffer
            //fflush(config->csv_file);
            config->receivedDataFrames+=1;
        }
        usleep(10);
      }

      fclose(config->csv_file);

      arduino_stopStream(config);
    }
    return NULL;
}

