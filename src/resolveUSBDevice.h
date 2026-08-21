#ifndef RESOLVEUSBDEVICE_H_INCLUDED
#define RESOLVEUSBDEVICE_H_INCLUDED

#ifdef __cplusplus
extern "C"
{
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BUFFER 256
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BUFFER 256


static int executeCommandLineAndRetreiveAllResults(const char *  command , char * what2GetBack , unsigned int what2GetBackMaxSize, unsigned long * what2GetBackSize)
{
  what2GetBack[0]=0;
  what2GetBack[what2GetBackMaxSize-1]=0;

 /* Open the command for reading. */
 FILE * f = popen(command, "r");
 if (f == 0)
       {
         fprintf(stderr,"Failed to run command (%s) \n",command);
         return 0;
       }


  size_t contentSize = fread(what2GetBack, 1 , what2GetBackMaxSize, f);
  *what2GetBackSize = contentSize;

  /* close */
  pclose(f);
  return 1;
}


static char* find_teensy_port()
{
    static char port[MAX_BUFFER] = {0};
    char buffer[MAX_BUFFER]={0};
    unsigned long resultSize=0;

    // Step 1: Get Bus and Device number of Teensy
    if (!executeCommandLineAndRetreiveAllResults("lsusb | grep Teensy | cut -d: -f 1", buffer, sizeof(buffer), &resultSize) || resultSize == 0)
    {
        fprintf(stderr, "Teensy not found (%lu bytes result)\n",resultSize);
        return NULL;
    }

    // Step 2: Find corresponding /dev/tty device
    if (!executeCommandLineAndRetreiveAllResults(
            "udevadm info -q path -n /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | xargs -I{} udevadm info -q all -p {} | grep -B 5 'ID_MODEL=Teensy' | grep DEVNAME",
            port, sizeof(port), &resultSize) || resultSize == 0)
    {
        fprintf(stderr, "No matching serial port found for Teensy\n");
        return NULL;
    }

    // Extract /dev/ttyACM* or /dev/ttyUSB*
    sscanf(port, "E: DEVNAME=%s", port);
    return port;
}



static int mainT()
{
    char *teensy_port = find_teensy_port();
    if (teensy_port)
    {
        printf("Teensy found on: %s\n", teensy_port);
    }
    else
    {
        printf("Teensy port not found\n");
    }
    return 0;
}


#ifdef __cplusplus
}
#endif


#endif // RESOLVEUSBDEVICE_H_INCLUDED
