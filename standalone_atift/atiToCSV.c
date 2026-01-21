/*
  netft / atiToCSV
  Logs ATI Net F/T UDP samples to CSV for a given duration.

  Usage:
    ./atiToCSV IP PORT OUTPUT.csv DURATION_SECONDS

  CSV columns:
    unixtimestamp_us,atitimestamp,Fx,Fy,Fz,Tx,Ty,Tz

  Notes:
  - unixtimestamp_us is Unix epoch time in microseconds (integer).
  - timestamp is the device ft_sequence (matches the “1522, 1701, …” style often seen).
  - Force/Torque raw ints are divided by 1e6 (ATI RDT scaling).
*/

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define COMMAND      2      /* RDT command 2: start/stream (common in sample code) */
#define NUM_SAMPLES  1      /* Request 1 sample per request (simple polling loop)  */

const unsigned long FORCE_RATIO=1000000l;
const unsigned long TORQUE_RATIO=1000000000l;

typedef unsigned int   uint32;
typedef int            int32;
typedef unsigned short uint16;
typedef unsigned char  byte;

typedef struct response_struct
{
    uint32 rdt_sequence;
    uint32 ft_sequence;
    uint32 status;
    int32  FTData[6];
} RESPONSE;

static uint64_t unix_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

static int file_is_empty_or_missing(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return 1;
    return (st.st_size == 0);
}

int main(int argc, char **argv)
{
    if (argc < 5)
    {
        fprintf(stderr, "Usage: %s IP PORT OUTPUT.csv DURATION_SECONDS\n", argv[0]);
        return 1;
    }

    const char *ip_str   = argv[1];
    int port             = (int)strtol(argv[2], NULL, 10);
    const char *out_csv  = argv[3];
    double duration_sec  = strtod(argv[4], NULL);

    if (port <= 0 || port > 65535)
    {
        fprintf(stderr, "Invalid PORT: %s\n", argv[2]);
        return 1;
    }
    if (!(duration_sec > 0.0))
    {
        fprintf(stderr, "Invalid DURATION_SECONDS: %s\n", argv[4]);
        return 1;
    }

    /* Open CSV early */
    FILE *fp = fopen(out_csv, "a");
    if (!fp)
    {
        fprintf(stderr, "Failed to open output CSV '%s': %s\n", out_csv, strerror(errno));
        return 1;
    }
    if (file_is_empty_or_missing(out_csv))
    {
        fprintf(fp, "unixtimestamp_us,atitimestamp,Fx,Fy,Fz,Tx,Ty,Tz\n");
        fflush(fp);
    }

    /* Create UDP socket */
    int socketHandle = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketHandle == -1)
    {
        fprintf(stderr, "socket() failed: %s\n", strerror(errno));
        fclose(fp);
        return 1;
    }

    /* Optional: receive timeout so we can exit close to duration even if device stops responding */
    {
        struct timeval to;
        to.tv_sec = 0;
        to.tv_usec = 250000; /* 250 ms */
        setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));
    }

    /* Resolve host */
    struct hostent *he = gethostbyname(ip_str);
    if (!he || !he->h_addr_list || !he->h_addr_list[0])
    {
        fprintf(stderr, "Could not resolve host: %s\n", ip_str);
        close(socketHandle);
        fclose(fp);
        return 1;
    }

    /* Connect to target */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16)port);

    if (connect(socketHandle, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        fprintf(stderr, "connect() failed: %s\n", strerror(errno));
        close(socketHandle);
        fclose(fp);
        return 1;
    }

    /* Prepare request packet */
    byte request[8];
    *(uint16*)&request[0] = htons(0x1234);
    *(uint16*)&request[2] = htons(COMMAND);
    *(uint32*)&request[4] = htonl(NUM_SAMPLES);

    byte response[36];
    RESPONSE resp;

    uint64_t start_us = unix_time_us();
    uint64_t end_us = start_us + (uint64_t)(duration_sec * 1000000.0);

    while (unix_time_us() < end_us)
    {

        ssize_t s = send(socketHandle, request, sizeof(request), 0);
        if (s < 0)
        {
            fprintf(stderr, "send() failed: %s\n", strerror(errno));
            break;
        }

        ssize_t r = recv(socketHandle, response, sizeof(response), 0);
        if (r < 0)
        {
            /* timeout or transient error: keep trying until time is up */
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                continue;
            }
            fprintf(stderr, "recv() failed: %s\n", strerror(errno));
            break;
        }
        if (r != (ssize_t)sizeof(response))
        {
            /* Unexpected packet size; ignore and continue */
            continue;
        }

        /* Parse response */
        resp.rdt_sequence = ntohl(*(uint32*)&response[0]);
        resp.ft_sequence  = ntohl(*(uint32*)&response[4]);
        resp.status       = ntohl(*(uint32*)&response[8]);
        for (int i = 0; i < 6; i++)
        {
            resp.FTData[i] = ntohl(*(int32*)&response[12 + i * 4]);
        }

        uint64_t uts = unix_time_us();

        double fx = (double)resp.FTData[0] / FORCE_RATIO;
        double fy = (double)resp.FTData[1] / FORCE_RATIO;
        double fz = (double)resp.FTData[2] / FORCE_RATIO;
        double tx = (double)resp.FTData[3] / TORQUE_RATIO;
        double ty = (double)resp.FTData[4] / TORQUE_RATIO;
        double tz = (double)resp.FTData[5] / TORQUE_RATIO;

        fprintf(fp, "%" PRIu64 ",%u,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                uts,
                (unsigned int)resp.ft_sequence,
                fx, fy, fz, tx, ty, tz);

        /* Ensure data hits disk even if interrupted */
        fflush(fp);
    }

    close(socketHandle);
    fclose(fp);
    return 0;
}

