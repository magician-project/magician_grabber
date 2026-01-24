/*
  netft / atiToCSV
  Logs ATI Net F/T UDP samples to CSV for a given duration.

  Usage:
    ./atiToCSV IP PORT OUTPUT.csv DURATION_SECONDS

  Set DURATION_SECONDS to 0 to run forever (until Ctrl+C).

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

#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMMAND      2      /* RDT command 2: start/stream (common in sample code) */
#define NUM_SAMPLES  1      /* Request 1 sample per request (simple polling loop)  */

/*
   IMPORTANT FOR HIGH-RATE UDP (4kHz+):
   - Keep recv() blocking (no SO_RCVTIMEO / select timeouts) to minimize overhead.
   - Avoid per-sample fflush()/printf().
*/
#define SET_TIMEOUT 0          /* Set a timeout (0 = disabled) */
#define TIMEOUT_VALUE_USEC 0   /* Timeout value (unused when SET_TIMEOUT=0) */
#define INCREASE_RECV_BUFFER 0 /*Do not rely on a sane system recv buffer, set our own*/
#define RECV_BUFFER_SIZE_MB  4 /*The buffer size in Megabytes*/

const unsigned long FORCE_RATIO  = 1000000l;
const unsigned long TORQUE_RATIO = 1000000000l;

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

/* --- Terminal colors / progress bar --- */
#define NORMAL  "\033[0m"
#define BLACK   "\033[30m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"

#define PROGRESS_BAR_LENGTH 10

static void progress_bar(double elapsed_s, double total_s)
{
    double progress = (total_s > 0.0) ? (elapsed_s / total_s) : 1.0;
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;

    int filledLength = (int)(progress * PROGRESS_BAR_LENGTH);
    if (filledLength > PROGRESS_BAR_LENGTH) filledLength = PROGRESS_BAR_LENGTH;

    printf("[");
    for (int i = 0; i < PROGRESS_BAR_LENGTH; i++)
    {
        if (i < filledLength)
        {
            printf(GREEN "█" NORMAL);
        }
        else
        {
            printf("-");
        }
    }
    printf("]");
}

/* Epoch time for CSV timestamps */
static uint64_t unix_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

/* Monotonic-ish time for progress/rates (gettimeofday is OK for this use) */
static uint64_t now_us(void)
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

/* --- Graceful shutdown (Ctrl+C / SIGTERM) ---
   Use classic signal() style (no sigaction/sigemptyset), per request.
   Keep the handler minimal: set a flag that the main loop checks.
*/
static volatile sig_atomic_t g_stop_requested = 0;

static void (*TerminationCallback)(void) = 0;

static void Ati_GlobalTerminationHandler(int signum)
{
    (void)signum;
    g_stop_requested = 1;
    //if (TerminationCallback) { TerminationCallback(); }
}

static int RegisterTerminationSignals(void (*callback)(void))
{
    TerminationCallback = callback;
    unsigned int failures = 0;

    if (signal(SIGINT,  Ati_GlobalTerminationHandler)  == SIG_ERR)
    {
        ++failures;
    }
    if (signal(SIGHUP,  Ati_GlobalTerminationHandler)  == SIG_ERR)
    {
        ++failures;
    }
    if (signal(SIGTERM, Ati_GlobalTerminationHandler)  == SIG_ERR)
    {
        ++failures;
    }
    /* NOTE: SIGKILL cannot be caught/handled; do not register it. */

    return (failures == 0);
}

int main(int argc, char **argv)
{
    if (argc < 5)
    {
        fprintf(stderr, "Usage: %s IP PORT OUTPUT.csv DURATION_SECONDS\n"
                "       (set DURATION_SECONDS to 0 to run forever)\n",
                argv[0]);
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
    if (duration_sec < 0.0)
    {
        fprintf(stderr, "Invalid DURATION_SECONDS (must be >= 0): %s\n", argv[4]);
        return 1;
    }

    (void)RegisterTerminationSignals(NULL);

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
    }

    /* Buffer CSV output to avoid per-sample disk I/O stalls at high rates */
    setvbuf(fp, NULL, _IOFBF, 1 << 20); /* 1MB buffer */

    /* Create UDP socket */
    int socketHandle = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketHandle == -1)
    {
        fprintf(stderr, "socket() failed: %s\n", strerror(errno));
        fclose(fp);
        return 1;
    }

    /* Optional receive timeout */
#if SET_TIMEOUT
    {
        struct timeval to;
        to.tv_sec = 0;
        to.tv_usec = TIMEOUT_VALUE_USEC; /* 0 ms */
        setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));
    }
#endif


#if INCREASE_RECV_BUFFER
    int rcvbuf = RECV_BUFFER_SIZE_MB * 1024 * 1024; // MB of RECV buffer
    setsockopt(socketHandle, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
#endif


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

    uint64_t start_us = now_us();
    uint64_t end_us   = (duration_sec > 0.0)
                        ? (start_us + (uint64_t)(duration_sec * 1000000.0))
                        : 0;

    /* Stats for bitrate + sample rate */
    uint64_t total_bytes = 0;
    uint64_t total_samples = 0;

    uint64_t interval_bytes = 0;
    uint64_t interval_samples = 0;

    uint64_t last_report_us = start_us;
    const uint64_t report_period_us = 100000; /* 100 ms => ~10 Hz UI updates */

    uint64_t loop_iters = 0;

    while (!g_stop_requested)
    {
        ssize_t s = send(socketHandle, request, sizeof(request), 0);
        if (s < 0)
        {
            if (errno == EINTR && g_stop_requested)
            {
                /* Interrupted by signal: exit cleanly */
                break;
            }
            fprintf(stderr, "send() failed: %s\n", strerror(errno));
            break;
        }

        ssize_t r = recv(socketHandle, response, sizeof(response), 0);
        if (r < 0)
        {
            if (errno == EINTR && g_stop_requested)
            {
                /* Interrupted by signal: exit cleanly */
                break;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                /* no data this time, keep trying until time is up */
            }
            else
            {
                fprintf(stderr, "recv() failed: %s\n", strerror(errno));
                break;
            }
        }
        else
        if (r == (ssize_t)sizeof(response))
        {
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
                    uts, (unsigned int)resp.ft_sequence, fx, fy, fz, tx, ty, tz);

            /* Update stats */
            total_bytes += (uint64_t)r;
            total_samples += 1;

            interval_bytes += (uint64_t)r;
            interval_samples += 1;
        }
        else
        {
            /* Unexpected packet size; ignore */
        }

        /*
           Periodic UI + time checks.
           IMPORTANT: do NOT call now_us() on every iteration (it can cost packets at 4kHz).
           Instead, check time occasionally.
        */
        ++loop_iters;
        if ((loop_iters & 0x3FULL) == 0) /* every 64 iterations */
        {
            uint64_t t_us = now_us();

            if (duration_sec > 0.0 && t_us >= end_us)
            {
                break;
            }

            if (t_us - last_report_us >= report_period_us)
            {
                double elapsed_s = (double)(t_us - start_us) / 1e6;
                double remain_s  = (duration_sec > 0.0) ? (duration_sec - elapsed_s) : 0.0;
                if (remain_s < 0.0) remain_s = 0.0;

                double interval_s = (double)(t_us - last_report_us) / 1e6;
                if (interval_s <= 0.0) interval_s = 1e-9;

                /* Current (interval) rates */
                double bitrate_mbps = ((double)interval_bytes * 8.0) / interval_s / 1e6;
                double sps_current  = (double)interval_samples / interval_s;

                /* Average rates since start */
                double total_s = (double)(t_us - start_us) / 1e6;
                if (total_s <= 0.0) total_s = 1e-9;
                double bitrate_avg_mbps = ((double)total_bytes * 8.0) / total_s / 1e6;
                double sps_avg          = (double)total_samples / total_s;

                /* Draw one updating line */
                printf("\r");
                if (duration_sec > 0.0)
                {
                    progress_bar(elapsed_s, duration_sec);
                    printf("  %5.1f%%  t=%6.2fs/%6.2fs  ", 100.0 * elapsed_s / duration_sec, elapsed_s, duration_sec);
                }
                else
                {
                    /* Run forever: show elapsed only */
                    printf("[" GREEN "RUN" NORMAL "]  t=%6.2fs  ", elapsed_s);
                }

                printf("bitrate=%7.2f Mb/s (avg %7.2f)  "
                       "samples/s=%7.0f (avg %7.0f)  "
                       "samples=%" PRIu64 "  bytes=%" PRIu64 "   ",
                       bitrate_mbps, bitrate_avg_mbps,
                       sps_current, sps_avg,
                       total_samples, total_bytes);
                fflush(stdout);

                /* Reset interval counters */
                interval_bytes = 0;
                interval_samples = 0;
                last_report_us = t_us;
            }
        }
    }

    /* Finish line cleanly */
    printf("\n");

    if (g_stop_requested)
    {
        fprintf(stderr, "Stopped (signal received). Closing socket and file...\n");
    }

    close(socketHandle);
    fclose(fp);
    return 0;
}



