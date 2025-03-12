#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>

#include "arduinoSensor.h"


int serialport_init(const char* serialport, int baud)
{
    struct termios toptions;
    int fd;

    //fd = open(serialport, O_RDWR | O_NOCTTY | O_NDELAY);
    fd = open(serialport, O_RDWR | O_NOCTTY | O_NDELAY);//| O_NONBLOCK

    if (fd == -1)
    {
        fprintf(stderr,RED "serialport_init: Unable to open port " NORMAL);
        return -1;
    }

    //int iflags = TIOCM_DTR;
    //ioctl(fd, TIOCMBIS, &iflags);    // turn on DTR
    //ioctl(fd, TIOCMBIC, &iflags);    // turn off DTR

    if (tcgetattr(fd, &toptions) < 0)
    {
        fprintf(stderr,RED "serialport_init: Couldn't get term attributes" NORMAL);
        return -1;
    }


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

    cfsetispeed(&toptions, brate);
    cfsetospeed(&toptions, brate);

    toptions.c_cflag = (CLOCAL | CREAD | CS8);
    toptions.c_iflag = IGNPAR;
    toptions.c_oflag = 0;
    toptions.c_lflag = 0;

    // see: http://unixwiz.net/techtips/termios-vmin-vtime.html
    toptions.c_cc[VMIN]  = 0;
    toptions.c_cc[VTIME] = 0;
    //toptions.c_cc[VTIME] = 20;

    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &toptions);
    if( tcsetattr(fd, TCSAFLUSH, &toptions) < 0)
    {
        fprintf(stderr,RED "init_serialport: Couldn't set term attributes" NORMAL);
        return -1;
    }

    return fd;
}


int serialport_close( int fd )
{
    return close( fd );
}




int arduino_startStream(ArduinoSerialConfig * context)
{
    context->serial_fd = serialport_init(context->port_name,context->baud_rate);

    /*
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
        fprintf(stderr,RED "Failed to get serial attributes" NORMAL);
        close(context->serial_fd);
        exit(1);
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
        fprintf(stderr,RED "Failed to set serial attributes" NORMAL);
        close(context->serial_fd);
        exit(1);
      /  return 1;
    }*/

  //Send Start Command!
  char buffer[]={"i\n"};
  int n = write(context->serial_fd, buffer, sizeof(buffer)-1);

  if (n<0)
  {
    fprintf(stderr,"Sending start command to %s failed (%d)\n",context->port_name,n);
    exit(1);
  }
  fprintf(stderr,"Send start command (%u / %lu bytes) to %s , wrote %u bytes \n",n,sizeof(buffer)-1,context->port_name,n);

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

  tcdrain(context->serial_fd);

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

    serialport_close(context->serial_fd);
    //close(context->serial_fd);
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
        double computeRate = 0.0;
        if (timeElapsedInSeconds!=0.0)
           { computeRate = (double) config->receivedDataFrames/timeElapsedInSeconds; }
        config->Hz = (float) computeRate;
        usleep(1000);
      }

      fprintf(stderr,"Arduino Thread %s terminating\n",config->csv_name);


      fclose(config->csv_file);

      arduino_stopStream(config);
    }


     free(buffer);
    }
    return NULL;
}

