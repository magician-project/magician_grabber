/* Simple demo showing how to communicate with Net F/T using C language. */

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
    close(context->socketHandle);
    return 0;
}


void *atinetft_thread(void *arg)
{
    ATINetFTConfig *config = (ATINetFTConfig *)arg;
    GlobalConfig *cfg = config->global;

    //byte request[8];			/* The request data sent to the Net F/T. */
	RESPONSE resp;				/* The structured response received from the Net F/T. */
	byte response[36];			/* The raw response data received from the Net F/T. */
	int i;						/* Generic loop/array index. */
	//int err;					/* Error status of operations. */
	//char * AXES[] = { "Fx", "Fy", "Fz", "Tx", "Ty", "Tz" };	/* The names of the force and torque axes. */

    if (ati_startStream(config)==0)
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

     while (*config->keep_running)
   {
	/* Receiving the response. */
	recv(config->socketHandle, response, 36, 0 );
	unsigned long receptionTime = GetTickCountMicroseconds();
	resp.rdt_sequence = ntohl(*(uint32*)&response[0]);
	resp.ft_sequence = ntohl(*(uint32*)&response[4]);
	resp.status = ntohl(*(uint32*)&response[8]);
	for( i = 0; i < 6; i++ ) { resp.FTData[i] = ntohl(*(int32*)&response[12 + i * 4]); }

	/* Output the response data. */
	//printf( "Status: 0x%08x\n", resp.status );
	//for (i =0;i < 6;i++) { printf("%s: %d\n", AXES[i], resp.FTData[i]); }

         // Placeholder for actual force sensor data acquisition
        fprintf(config->csv_file, "%lu,",receptionTime);
        fprintf(config->csv_file, "%f,",(double) resp.FTData[0]/FORCE_RATIO);
        fprintf(config->csv_file, "%f,",(double) resp.FTData[1]/FORCE_RATIO);
        fprintf(config->csv_file, "%f,",(double) resp.FTData[2]/FORCE_RATIO);
        fprintf(config->csv_file, "%f,",(double) resp.FTData[3]/TORQUE_RATIO);
        fprintf(config->csv_file, "%f,",(double) resp.FTData[4]/TORQUE_RATIO);
        fprintf(config->csv_file, "%f\n",(double) resp.FTData[5]/TORQUE_RATIO);
        fflush(config->csv_file);

        config->receivedDataFrames+=1;
     }

     fclose(config->csv_file);

     ati_stopStream(config);
    }
    return NULL;
}

