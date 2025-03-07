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
        exit(1);
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
  fprintf(stderr,"Send start command (%u / %lu bytes) to %s \n",n,sizeof(buffer)-1,context->port_name);

  return 0;
}


int arduino_stopStream(ArduinoSerialConfig * context)
{
    context->running = 1;

    fprintf(stderr,"Arduino stopping stream\n");
    char buffer[]={"f\n"};
    int n = write(context->serial_fd, buffer, sizeof(buffer)-1);
    tcdrain(context->serial_fd);
    fprintf(stderr,"Send stop command (%u / %lu bytes) to %s \n",n,sizeof(buffer)-1,context->port_name);

    close(context->serial_fd);
    return 0;
}

void *arduino_thread(void *arg)
{
    ArduinoSerialConfig *config = (ArduinoSerialConfig *)arg;
    GlobalConfig *cfg = config->global;

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

     //Inject CSV headers where needed..
     if (strstr(config->csv_name,"accelerometer")!=0)
        { fprintf(config->csv_file,"timestamp,dev_timestamp,accX,accY,accY\n"); }
     if (strstr(config->csv_name,"controller")!=0)
        { fprintf(config->csv_file,"timestamp,dev_timestamp,Button1,Distance1,Distance2,Distance3,Light1,Light2,Light3,Light4,Light5,Light6\n"); }


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
                 fprintf(config->csv_file, "%lu,", receptionTime);
                 fprintf(config->csv_file, "%s\n", lineBuffer);
                 lineIndex = 0;
                 config->receivedDataFrames+=1;
                 //fprintf(stderr,"DUMP TO FILE %s\n",lineBuffer);
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
        double computeRate = (double) config->receivedDataFrames/timeElapsedInSeconds;
        config->Hz = (float) computeRate;
        usleep(1000);
      }

      fclose(config->csv_file);

      arduino_stopStream(config);
    }


     free(buffer);
    }
    return NULL;
}

