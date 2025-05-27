/** @file atiForceSensor.c
 *  @brief This is an abstraction layer for the ATI NET F/T :
 *  It is based on the Net F/T C Sample found here:
 *  https://www.ati-ia.com/Products/ft/software/net_ft_software.aspx
 *  @author Ammar Qammaz (AmmarkoV)
 */

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


#include "atiForceSensor.h"

#define PORT 49152 /* Port the Net F/T always uses */
#define COMMAND 2 /* Command code 2 starts streaming */
#define NUM_SAMPLES 0 /* Will send 1 sample before stopping */

const unsigned long FORCE_RATIO=1000000l;
const unsigned long TORQUE_RATIO=1000000000l;


/* Typedefs used so integer sizes are more explicit */
typedef unsigned int uint32;
typedef int int32;
typedef unsigned short uint16;
typedef short int16;
typedef unsigned char byte;
typedef struct response_struct
{
	uint32 rdt_sequence;
	uint32 ft_sequence;
	uint32 status;
	int32 FTData[6];
} RESPONSE;

int ati_startStream(ATINetFTConfig * context)
{
	struct sockaddr_in addr;	/* Address of Net F/T. */
	struct hostent *he;			/* Host entry for Net F/T. */

	byte request[8];			/* The request data sent to the Net F/T. */
	//RESPONSE resp;				/* The structured response received from the Net F/T. */
	//byte response[36];			/* The raw response data received from the Net F/T. */
	//int i;						/* Generic loop/array index. */
	int err;					/* Error status of operations. */
	//char * AXES[] = { "Fx", "Fy", "Fz", "Tx", "Ty", "Tz" };	/* The names of the force and torque axes. */

	/* Calculate number of samples, command code, and open socket here. */
	context->socketHandle = socket(AF_INET, SOCK_DGRAM, 0);
	if (context->socketHandle == -1) { exit(1); }

	*(uint16*)&request[0] = htons(0x1234); /* standard header. */
	*(uint16*)&request[2] = htons(COMMAND); /* per table 9.1 in Net F/T user manual. */
	*(uint32*)&request[4] = htonl(NUM_SAMPLES); /* see section 9.1 in Net F/T user manual. */

	/* Sending the request. */
	he = gethostbyname(context->ip_address);
	memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(context->port);

	err = connect( context->socketHandle, (struct sockaddr *)&addr, sizeof(addr) );
	if (err == -1) {
		exit(2);
	}
	send( context->socketHandle, request, 8, 0 );
	return 0;
}


int ati_stopStream(ATINetFTConfig * context)
{
    context->running = 1;
    close(context->socketHandle);
    return 0;
}


int ati_call_callback(ATINetFTConfig *config, unsigned long timestamp, double fX,double fY,double fZ,double tX,double tY,double tZ)
{
    if (config->callback)
    {
        // Cast back to the correct function pointer type before calling
        int (*callback_func)(ATINetFTConfig *,unsigned long, double, double, double, double, double, double) =
                   (int (*)(ATINetFTConfig *,unsigned long, double, double, double, double, double, double)) config->callback;

        return callback_func(config, timestamp,  fX, fY, fZ, tX, tY, tZ);  // Call the function
    }
    return 0;  // Indicate failure if no callback is set
}



void *atinetft_simulated_thread(void *arg)
{
   ATINetFTConfig *config = (ATINetFTConfig *)arg;
   //GlobalConfig *cfg = config->global;

   double step = 0.0;
   unsigned long atiStartTime = GetTickCountMicroseconds();
   while (*config->keep_running)
   {
     config->running = 1;

     unsigned long receptionTime = GetTickCountMicroseconds();
	 double fX = (double) step;
	 double fY = (double) step;
	 double fZ = (double) step;
	 double tX = (double) step;
	 double tY = (double) step;
     double tZ = (double) step;

     step += 0.1;
     config->receivedDataFrames+=1;

     if (config->callback)
                 {
                    //Pass our CSV line to a callback function!
                    ati_call_callback(config,receptionTime,fX, fY, fZ, tX, tY, tZ);
                 }


     if (step>1000.0) { step = 0; }
     usleep(100);


     //Calculate framerates
     double timeElapsedInSeconds = (double) ((double) (receptionTime-atiStartTime)/(double) 1000000.0);
     double computeRate = 0.0;
     if (timeElapsedInSeconds!=0.0)
           { computeRate = (double) config->receivedDataFrames/timeElapsedInSeconds; }

     config->Hz = (float) computeRate;
   }

   return NULL;
}












void *atinetft_thread(void *arg)
{
    ATINetFTConfig *config = (ATINetFTConfig *)arg;
    GlobalConfig *cfg = config->global;


    if (cfg->simulate)
    {
        return atinetft_simulated_thread(arg);
    }

    char enabledFileOutput = (strcmp(cfg->outputDirectory,"/dev/null")!=0);

    //byte request[8];			/* The request data sent to the Net F/T. */
	RESPONSE resp;				/* The structured response received from the Net F/T. */
	byte response[36];			/* The raw response data received from the Net F/T. */
	int i;						/* Generic loop/array index. */
	//int err;					/* Error status of operations. */
	//char * AXES[] = { "Fx", "Fy", "Fz", "Tx", "Ty", "Tz" };	/* The names of the force and torque axes. */

    if (ati_startStream(config)==0)
    {

     if (enabledFileOutput)
     {
      char fullCSVOutputPath[2048]={0};
      snprintf(fullCSVOutputPath,2048,"%s/%s",cfg->outputDirectory,config->csv_name);
      config->csv_file = fopen(fullCSVOutputPath, "w");
      if (!config->csv_file)
      {
        perror("Failed to open ATI NetFT log file");
        return NULL;
      }
      fprintf(config->csv_file,"timestamp,fX,fY,fZ,tX,tY,tZ\n");
     }


   unsigned long atiStartTime = GetTickCountMicroseconds();

   while (*config->keep_running)
   {
    config->running = 1;

	/* Receiving the response. */
	recv(config->socketHandle, response, 36, 0 );
	unsigned long receptionTime = GetTickCountMicroseconds();
	resp.rdt_sequence = ntohl(*(uint32*)&response[0]);
	resp.ft_sequence  = ntohl(*(uint32*)&response[4]);
	resp.status = ntohl(*(uint32*)&response[8]);
	for( i = 0; i < 6; i++ ) { resp.FTData[i] = ntohl(*(int32*)&response[12 + i * 4]); }

	/* Output the response data. */
	//printf( "Status: 0x%08x\n", resp.status );
	//for (i =0;i < 6;i++) { printf("%s: %d\n", AXES[i], resp.FTData[i]); }

        // Calculate Force/Torque using double precision
	    double fX = (double) resp.FTData[0]/FORCE_RATIO;
	    double fY = (double) resp.FTData[1]/FORCE_RATIO;
	    double fZ = (double) resp.FTData[2]/FORCE_RATIO;
	    double tX = (double) resp.FTData[3]/TORQUE_RATIO;
	    double tY = (double) resp.FTData[4]/TORQUE_RATIO;
	    double tZ = (double) resp.FTData[5]/TORQUE_RATIO;

        // Placeholder for actual force sensor data acquisition
        if (enabledFileOutput)
          { fprintf(config->csv_file, "%lu,%f,%f,%f,%f,%f,%f\n",receptionTime,fX,fY,fZ,tX,tY,tZ); }

        // Propagate values to any callbacks that need them
        if (config->callback)
                 {
                    //Pass our CSV line to a callback function!
                    ati_call_callback(config,receptionTime,fX, fY, fZ, tX, tY, tZ);
                 }

        //Make sure values are flushed (sudden termination protection?)
        fflush(config->csv_file);

        config->receivedDataFrames+=1;

        //Calculate framerates
        double timeElapsedInSeconds = (double) ((double) (receptionTime-atiStartTime)/(double) 1000000.0);
        double computeRate = 0.0;
        if (timeElapsedInSeconds!=0.0)
           { computeRate = (double) config->receivedDataFrames/timeElapsedInSeconds; }

        config->Hz = (float) computeRate;
        //usleep(100);
     }
     fprintf(stderr,"ATI Thread terminating\n");

     if (enabledFileOutput)
        { fclose(config->csv_file); }

     ati_stopStream(config);
    }
    return NULL;
}

