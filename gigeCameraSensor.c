/** @file gigeCameraSensor.c
 *  @brief This is an ARAVIS grabber ( https://github.com/AravisProject/aravis )
 *  wrapped to fit the rest of the modules and based on a standalone utility developed during early stages of the Magician Project
 *  https://github.com/AmmarkoV/aravis-c-examples/blob/main/06-grabber.c
 *  @author Ammar Qammaz (AmmarkoV)
 */

/* Aravis header */
#include <arv.h>

/* Standard headers */
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "gigeCameraSensor.h"
#include "imageStreamer.h"

/* Forward-declare WritePNG to avoid pulling in codecs/image.h which
   redefines struct Image already defined in imageStreamer.h */
int WritePNG(const char *filename, struct Image *pic);

volatile sig_atomic_t termination_requested = 0;

void sigterm_handler(int signum)
{
    termination_requested = 1;
}

struct Settings
{
    unsigned int delay,maxFramesToGrab;
    unsigned int exposure; // 0 means no setting
    double       gain;
    double       blackLevel;
    double       frameRate;
    char * tickCommand;
};

#include <sys/time.h>
#include <unistd.h>
#include <time.h>

#define EPOCH_YEAR_IN_TM_YEAR 1900
const unsigned int ARV_VIEWER_N_BUFFERS=100;
#define TRYHARD_MODE 1
#define EMMIT_ARAVIS_SIGNALS 0

#include <errno.h>
#include <ctype.h>

/* ---------- OS/UDP tuning helpers (Linux) ---------- */

static long long read_proc_ll(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    long long v = -1;
    if (fscanf(f, "%lld", &v) != 1) v = -1;
    fclose(f);
    return v;
}

/* Reads "min default max" triplets from /proc/sys/net/ipv4/udp_rmem_min etc.
   Some files contain a single value; this function handles both. */
static int read_proc_triplet_ll(const char *path, long long out[3])
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char buf[256] = {0};
    if (!fgets(buf, sizeof(buf), f)) { fclose(f); return -1; }
    fclose(f);

    // Try parse three numbers
    long long a=-1,b=-1,c=-1;
    int n = sscanf(buf, "%lld %lld %lld", &a, &b, &c);
    if (n == 3) { out[0]=a; out[1]=b; out[2]=c; return 0; }

    // Try parse single number, mirror to all three
    a = -1;
    n = sscanf(buf, "%lld", &a);
    if (n == 1) { out[0]=a; out[1]=a; out[2]=a; return 0; }

    return -1;
}

static const char* yesno(int cond) { return cond ? "YES" : "no"; }

/* Call this once at startup after you know you're using GigE (UDP) streaming. */
static void gigecamera_print_udp_os_guidance(void)
{
    fprintf(stderr,
        "\n========== UDP / GigE Vision OS-side checklist (Linux) ==========\n"
        "If you see missing packets, incomplete frames, or Aravis 'n_failures' rising,\n"
        "the most common cause is the host/network dropping UDP due to buffer limits or bursts.\n\n");

    // Core socket buffer limits
    long long rmem_max = read_proc_ll("/proc/sys/net/core/rmem_max");
    long long rmem_def = read_proc_ll("/proc/sys/net/core/rmem_default");
    long long wmem_max = read_proc_ll("/proc/sys/net/core/wmem_max");
    long long wmem_def = read_proc_ll("/proc/sys/net/core/wmem_default");

    // UDP-specific min buffers (may not exist on all kernels)
    long long udp_rmem[3] = {-1,-1,-1};
    long long udp_wmem[3] = {-1,-1,-1};
    int have_udp_rmem = (read_proc_triplet_ll("/proc/sys/net/ipv4/udp_rmem_min", udp_rmem) == 0);
    int have_udp_wmem = (read_proc_triplet_ll("/proc/sys/net/ipv4/udp_wmem_min", udp_wmem) == 0);

    // Backlog / drops counters (best-effort)
    long long netdev_max_backlog = read_proc_ll("/proc/sys/net/core/netdev_max_backlog");

    if (rmem_max >= 0) {
        fprintf(stderr, "Current kernel socket buffers:\n");
        fprintf(stderr, "  net.core.rmem_max     = %lld bytes\n", rmem_max);
        fprintf(stderr, "  net.core.rmem_default = %lld bytes\n", rmem_def);
        fprintf(stderr, "  net.core.wmem_max     = %lld bytes\n", wmem_max);
        fprintf(stderr, "  net.core.wmem_default = %lld bytes\n", wmem_def);
        if (netdev_max_backlog >= 0)
            fprintf(stderr, "  net.core.netdev_max_backlog = %lld\n", netdev_max_backlog);
        fprintf(stderr, "\n");
    } else {
        fprintf(stderr,
            "Could not read /proc/sys/net/core/* (not Linux? restricted container?).\n\n");
    }

    if (have_udp_rmem || have_udp_wmem) {
        fprintf(stderr, "UDP kernel minima:\n");
        if (have_udp_rmem)
            fprintf(stderr, "  net.ipv4.udp_rmem_min = %lld (min)\n", udp_rmem[0]);
        if (have_udp_wmem)
            fprintf(stderr, "  net.ipv4.udp_wmem_min = %lld (min)\n", udp_wmem[0]);
        fprintf(stderr, "\n");
    }

    // Heuristics: warn if clearly too small for high-throughput GigE Vision
    // (These are conservative “starter” thresholds.)
    const long long RECOMM_RMEM_MAX = 32LL * 1024 * 1024;   // 32 MB
    const long long RECOMM_WMEM_MAX = 8LL  * 1024 * 1024;   // 8 MB
    const long long RECOMM_BACKLOG  = 5000;

    int warn_rmem = (rmem_max > 0 && rmem_max < RECOMM_RMEM_MAX);
    int warn_wmem = (wmem_max > 0 && wmem_max < RECOMM_WMEM_MAX);
    int warn_backlog = (netdev_max_backlog > 0 && netdev_max_backlog < RECOMM_BACKLOG);

    if (warn_rmem || warn_wmem || warn_backlog) {
        fprintf(stderr,
            "WARNING: Kernel defaults look low for high-rate UDP imaging:\n"
            "  rmem_max low?   %s\n"
            "  wmem_max low?   %s\n"
            "  backlog low?    %s\n\n",
            yesno(warn_rmem), yesno(warn_wmem), yesno(warn_backlog));
    }

    fprintf(stderr,
        "Suggested TEMPORARY tuning (until reboot) — copy/paste as root:\n"
        "  sudo sysctl -w net.core.rmem_max=33554432\n"
        "  sudo sysctl -w net.core.rmem_default=33554432\n"
        "  sudo sysctl -w net.core.wmem_max=8388608\n"
        "  sudo sysctl -w net.core.wmem_default=8388608\n"
        "  sudo sysctl -w net.core.netdev_max_backlog=5000\n");

    // Only print UDP min sysctls if we could read them (kernel dependent)
    if (have_udp_rmem) {
        fprintf(stderr,
            "  sudo sysctl -w net.ipv4.udp_rmem_min=8192\n");
    }
    if (have_udp_wmem) {
        fprintf(stderr,
            "  sudo sysctl -w net.ipv4.udp_wmem_min=8192\n");
    }

    fprintf(stderr,
        "\nTo make it PERSISTENT, create /etc/sysctl.d/99-gigevision.conf with those lines\n"
        "and run: sudo sysctl --system\n\n"
        "NIC / network checklist (very common causes of UDP loss):\n"
        "  • Confirm MTU is consistent end-to-end (camera, switch, NIC). If using jumbo (e.g. 9000),\n"
        "    EVERY hop must support it.\n"
        "  • Prefer a dedicated NIC or VLAN; avoid congested shared networks.\n"
        "  • Disable NIC power-saving features if they cause jitter.\n"
        "  • If packet loss persists at high bandwidth, reduce camera bandwidth:\n"
        "      - lower FPS or ROI/payload, or\n"
        "      - increase camera inter-packet delay (camera feature), or\n"
        "      - reduce GVSP packet size (if supported), especially if jumbo is flaky.\n"
        "===============================================================\n\n");
}

/* Call this periodically when you suspect issues, e.g. every ~1s, or when failures rise.
   It prints stream stats and gives next-step guidance when problems appear. */
static void gigecamera_print_aravis_udp_health(ArvStream *stream, unsigned int frames, unsigned int framesInPeriod)
{
    if (!stream) return;

    guint64 n_completed=0, n_failures=0, n_underruns=0;
    arv_stream_get_statistics(stream, &n_completed, &n_failures, &n_underruns);

    // Print a compact status line
    fprintf(stderr,
        "[Aravis] frames=%u (+%u recent) completed=%" G_GUINT64_FORMAT
        " failures=%" G_GUINT64_FORMAT " underruns=%" G_GUINT64_FORMAT "\n",
        frames, framesInPeriod, n_completed, n_failures, n_underruns);

    // Guidance when things look bad
    if (n_failures > 0 || n_underruns > 0) {
        fprintf(stderr,
            "  [Hint] Detected stream failures/underruns. This often indicates UDP packet loss or\n"
            "         receiver overload. Try, in this order:\n"
            "    1) Increase OS UDP/socket buffers (see the UDP checklist printed at startup).\n"
            "    2) In Aravis, set a larger 'socket-buffer-size' on the GvStream.\n"
            "    3) Enable/raise packet resend + tune 'packet-timeout' and 'frame-retention'.\n"
            "    4) Reduce bandwidth: smaller ROI, lower FPS, add camera inter-packet delay.\n"
            "    5) Verify MTU/jumbo consistency across camera/switch/NIC.\n");
    }
}





int writeSettings(const char * filename,struct Settings * settings)
{
    FILE * fp = fopen(filename,"w");
    if (fp!=0)
    {
        fprintf(fp,"{\n\"delay\": %u,\n",settings->delay);
        fprintf(fp,"\"maxFramesToGrab\": %u,\n",settings->maxFramesToGrab);
        fprintf(fp,"\"exposure\": %u,\n",settings->exposure);
        fprintf(fp,"\"blackLevel\": %f,\n",settings->blackLevel);
        fprintf(fp,"\"gain\": %f,\n",settings->gain);
        if (settings->tickCommand!=0)
          {
             fprintf(fp,"\"frameRate\": %f,\n",settings->frameRate);
             fprintf(fp,"\"tickCommand\": \"%s\"\n}\n",settings->tickCommand);
          } else
          {
             fprintf(fp,"\"frameRate\": %f }\n",settings->frameRate);
          }
        fclose(fp);
        return 1;
    }
    return 0;
}

unsigned int simplePowPPMG(unsigned int base,unsigned int exp)
{
    if (exp==0) return 1;
    unsigned int retres=base;
    unsigned int i=0;
    for (i=0; i<exp-1; i++)
    {
        retres*=base;
    }
    return retres;
}

int WritePPMG(const char * filename,struct Image * pic)
{
    //fprintf(stderr,"saveRawImageToFile(%s) called\n",filename);
    if (pic==0)
    {
        return 0;
    }
    if ( (pic->width==0) || (pic->height==0) || (pic->channels==0) || (pic->bitsperpixel==0) )
    {
        fprintf(stderr,"saveRawImageToFile(%s) called with zero dimensions ( %ux%u %u channels %u bpp\n",filename,pic->width, pic->height,pic->channels,pic->bitsperpixel);
        return 0;
    }
    if(pic->pixels==0)
    {
        fprintf(stderr,"saveRawImageToFile(%s) called for an unallocated (empty) frame , will not write any file output\n",filename);
        return 0;
    }
    if (pic->bitsperpixel>16)
    {
        fprintf(stderr,"PNM does not support more than 2 bytes per pixel..!\n");
        return 0;
    }

    FILE *fd=0;
    fd = fopen(filename,"wb");

    if (fd!=0)
    {
        unsigned int n;
        if (pic->channels==3) fprintf(fd, "P6\n");
        else if (pic->channels==1) fprintf(fd, "P5\n");
        else
        {
            fprintf(stderr,"Invalid channels arg (%u) for SaveRawImageToFile\n",pic->channels);
            fclose(fd);
            return 0;
        }

        fprintf(fd, "%d %d\n%u\n", pic->width, pic->height, simplePowPPMG(2,pic->bitsperpixel)-1);

        float tmp_n = (float) pic->bitsperpixel/ 8;
        tmp_n = tmp_n *  pic->width * pic->height * pic->channels ;
        n = (unsigned int) tmp_n;

        fwrite(pic->pixels, 1, n, fd);
        fflush(fd);
        fclose(fd);
        return 1;
    }
    else
    {
        fprintf(stderr,"SaveRawImageToFile could not open output file %s\n",filename);
        return 0;
    }
    return 0;
}

unsigned long getSleepTimeBasedOnFramerate(double frameRate)
{
  unsigned long timeToSleepToWaitFor1Frame = (unsigned long) 1000000 / frameRate;
  return timeToSleepToWaitFor1Frame;
}

int gigecamera_startStream(GiGECameraConfig * context)
{
    fprintf(stderr,"Starting camera stream");

    gigecamera_print_udp_os_guidance();

    // Set up SIGTERM signal handler
    struct sigaction action;
    action.sa_handler = sigterm_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGTERM, &action, NULL);

    //guint64 n_completed_buffers=0, n_failures=0, n_underruns=0;

    GlobalConfig *cfg = context->global;


    //unsigned int i=0;
    struct Settings settings= {0};
    struct Image dataAsImage= {0};

    //Just use settings
    settings.blackLevel  =  context->blackLevel;
    settings.gain        =  context->gain;
    settings.frameRate   =  context->frameRate;
    settings.exposure    =  context->exposure;

    char forceDims = 0;
    char refreshDimsOnEachFrame = 1;

    /* Mandatory glib type system initialization */
    //arv_g_type_init ();

    ArvCamera *camera = NULL;
    //ArvStream *stream = NULL;
    GError *error = NULL;

    /* Connect to the first available camera */
    printf ("Trying to connect to camera \n");
    camera = arv_camera_new (NULL, &error);
    context->camera = (void*) camera;

    if (camera == NULL)
    {
        fprintf (stderr,"\nNo camera found, terminating grabber\n");
        exit(1);
    }
    if (error != NULL)
    {
        fprintf (stderr,"\nError while searching for camera\n");
        exit(1);
    }

    printf ("Found a device ..\n");

    if (ARV_IS_CAMERA (camera))
    {
        ArvStream *stream = NULL;

        printf ("Found camera '%s'\n", arv_camera_get_model_name (camera, NULL));

        arv_camera_set_acquisition_mode (camera, ARV_ACQUISITION_MODE_CONTINUOUS, &error);

        if (error == NULL)
        {
            /* Create the stream object without callback */
            stream = arv_camera_create_stream (camera, NULL, NULL, NULL, &error);
            context->stream = (void*) stream;

            /*
            g_object_set (stream,
                      "packet-timeout", 3 * 1000,
                      "frame-retention", 25 * 1000,
                      "packet-request-ratio", 0.75,
                      NULL);*/

            #if TRYHARD_MODE
            g_object_set(stream,
                         "packet-resend", ARV_GV_STREAM_PACKET_RESEND_ALWAYS,
                         "packet-timeout", 40000,          // 40 ms (µs)
                         "frame-retention", 200000,        // 200 ms (µs)
                         "packet-request-ratio", 0.5,      // up to 50% of packets can be requested
                         "socket-buffer-size", 32*1024*1024,
                        NULL);
            #endif


        }

        if (ARV_IS_STREAM (stream))
        {
            size_t payload;

            /* Retrieve the payload size for buffer creation */
            payload = arv_camera_get_payload (camera, &error);
            if (error == NULL)
            {
                /* Insert some buffers in the stream buffer pool */
                for (unsigned int i = 0; i < ARV_VIEWER_N_BUFFERS; i++)
                    arv_stream_push_buffer (stream, arv_buffer_new (payload, NULL));

                //arv_stream_create_buffers(stream, ARV_VIEWER_N_BUFFERS, NULL, NULL, NULL);

            } else
            {
              fprintf (stderr,"\nFailed allocating %u buffers\n",ARV_VIEWER_N_BUFFERS);
              exit(1);
            }



            #if EMMIT_ARAVIS_SIGNALS
            arv_stream_set_emit_signals (stream, TRUE);
            #endif

            if (error == NULL)
                /* Start the acquisition */
                arv_camera_set_acquisition_mode (camera, ARV_ACQUISITION_MODE_CONTINUOUS, NULL);
            arv_camera_start_acquisition (camera, &error);

            if (settings.exposure!=0)
            {
                arv_camera_set_exposure_time(camera, settings.exposure, NULL);
            }
            if (settings.gain!=0.0)
            {
                arv_camera_set_gain (camera, settings.gain, NULL);
            }
            if (settings.blackLevel!=0.0)
            {
                arv_camera_set_black_level(camera, settings.blackLevel, NULL);
            }
            if (settings.frameRate!=0.0)
            {
                arv_camera_set_frame_rate (camera, settings.frameRate, NULL);
            }


            if (error == NULL)
            {
                if ( (!refreshDimsOnEachFrame)&& (!forceDims) )
                {
                    //Poll dims so that we know them in advance if we dont want to get them from each buffer, and we dont want to force a specific dimension
                    int minvalue=0,maxvalue=0;
                    arv_camera_get_width_bounds(camera,&minvalue,&maxvalue,NULL);
                    dataAsImage.width  = (unsigned int) maxvalue;
                    arv_camera_get_height_bounds(camera,&minvalue,&maxvalue,NULL);
                    dataAsImage.height  = (unsigned int) maxvalue;
                }

                if ( (!refreshDimsOnEachFrame) || (forceDims) )
                {
                    //Attempt to setup region if there is no autorefresh of dimensions, or we want to force a specific dimension
                    arv_camera_set_region(camera,0,0,dataAsImage.width,dataAsImage.height,NULL); //Use full sensor area
                }

                //const void *data = NULL;
                char filename[1025]= {0};
                //unsigned int frameNumber = 0;
                //unsigned int brokenFrameNumber = 0;
                //ArvBuffer *buffer;

                snprintf(filename,1024,"%.512s/info.json",cfg->outputDirectory);
                writeSettings(filename,&settings);

                //unsigned long startTime = GetTickCountMicroseconds();

                //unsigned long startGrab, endGrab;
                //unsigned long microsecondsGrab;

                if (settings.frameRate!=0.0)
                {
                    fprintf(stderr,"Waiting initial period to buffer at least one frame..\n");
                    unsigned long timeToSleepToWaitFor1Frame = getSleepTimeBasedOnFramerate(settings.frameRate);
                    usleep(timeToSleepToWaitFor1Frame);
                }

              return 0;
           }
       }
    }
   return 1;
}







int gigecamera_stopStream(GiGECameraConfig * context)
{
  context->running = 0;

  GError *error = NULL;

  ArvCamera *camera = (ArvCamera *) context->camera;
  ArvStream *stream = (ArvStream *) context->stream;

  /* Stop the acquisition */
  #if EMMIT_ARAVIS_SIGNALS
  arv_stream_set_emit_signals (stream, FALSE);
  #endif
  arv_camera_stop_acquisition (camera, &error);

  fprintf(stderr,"Sleeping after stopping acquisition!\n");
  sleep(2);

  /* Destroy the stream object */
  g_clear_object (&stream);

  /* Destroy the camera instance */
  g_clear_object (&camera);

  if (error != NULL)
    {
        /* En error happened, display the correspdonding message */
        printf ("Error: %s\n", error->message);
        return EXIT_FAILURE;
    }

    printf("\n\nDone\n");
    printf("Camera Summary : Ok %lu/Fail %lu/Under %lu    \n", context->n_completed_buffers, context->n_failures, context->n_underruns);

    if (context->exposure!=0)
    {
        printf("Exposure time was %u\n",context->exposure);
        printf("This is equivalent to %0.2f FPS\n",(float) 1000000.0/context->exposure);
    }

    context->camera=NULL;
    context->stream=NULL;
    return EXIT_SUCCESS;
}


int camera_callback_relay(GiGECameraConfig *config, unsigned long timestamp, struct Image *dataAsImage)
{
    if (config->callback)
    {
        // Cast back to the correct function pointer type before calling
        int (*callback_func)(GiGECameraConfig *,unsigned long, struct Image *) =
                (int (*)(GiGECameraConfig *,unsigned long, struct Image *)) config->callback;

        return callback_func(config, timestamp, dataAsImage);  // Call the function
    }
    return 0;  // Indicate failure if no callback is set
}


void *gigecamera_thread(void *arg)
{
    GiGECameraConfig *config = (GiGECameraConfig *)arg;
    GlobalConfig *cfg = config->global;

    gigecamera_startStream(config);

    char enabledFileOutput = (strcmp(cfg->outputDirectory,"/dev/null")!=0);

    char fullCSVOutputPath[2048]={0};
    snprintf(fullCSVOutputPath,2048,"%s/%s",cfg->outputDirectory,config->csv_name);


    if (enabledFileOutput)
    {
     config->csv_file = fopen(fullCSVOutputPath, "w");
     if (!config->csv_file)
      {
        perror("Failed to open GiGE camera log file");
        return NULL;
      }
     fprintf(config->csv_file,"timestamp,frameID\n");
    } else
    {
     config->csv_file = NULL;
    }

    //ArvCamera *camera = config->camera;
    ArvStream *stream = (ArvStream *) config->stream;

    unsigned long startGrab, endGrab;
    unsigned long microsecondsGrab;
    unsigned long timeToSleepToWaitFor1Frame = getSleepTimeBasedOnFramerate(config->frameRate);
    ArvBuffer *buffer = NULL;

    char refreshDimsOnEachFrame = 1;

    const void *data = NULL;
    char filename[2048]= {0};
    unsigned int frameNumber = 0;
    unsigned int brokenFrameNumber = 0;

    struct Image dataAsImage= {0};
    dataAsImage.width  = config->width;
    dataAsImage.height = config->height;


    unsigned long startTime = GetTickCountMicroseconds();

    StreamingContext * shm_stream = NULL;
    if (config->camera_shm_stream!=NULL)
          {
            shm_stream = (StreamingContext *) config->camera_shm_stream;
            if (shm_stream->frame == NULL)
            {
              fprintf(stderr,"\nNo video buffer for streaming shm=%p\n",(void*) shm_stream);
              exit(1);
            }
          }



while (*config->keep_running && !termination_requested)
{
    char lastFrameIsBroken = 0;
    startGrab = GetTickCountMicroseconds();

    // Prefer timeout_pop to avoid busy waiting.
    // Timeout is in microseconds in Aravis; pick something near frame period.
    buffer = arv_stream_timeout_pop_buffer(stream, 20000); // 20ms

    if (!ARV_IS_BUFFER(buffer))
    {
        brokenFrameNumber++;
        lastFrameIsBroken=1;
        // No buffer ready yet (or stream starving). Sleep a bit to reduce CPU.
        usleep(1000);
        continue;
    }

    config->running = 1;

    // 1) CRITICAL: Always check status first. UDP loss/incomplete frames show up here.
    ArvBufferStatus st = arv_buffer_get_status(buffer);
    if (st != ARV_BUFFER_STATUS_SUCCESS)
    {
        brokenFrameNumber++;
        lastFrameIsBroken=1;

        // Optional: print occasional guidance when problems happen
        if ((brokenFrameNumber % 100) == 1) {
            arv_stream_get_statistics(stream,
                                      &config->n_completed_buffers,
                                      &config->n_failures,
                                      &config->n_underruns);
            fprintf(stderr,
                "[Aravis] Non-success buffer (status=%d). completed=%" G_GUINT64_FORMAT
                " failures=%" G_GUINT64_FORMAT " underruns=%" G_GUINT64_FORMAT "\n"
                "  Hint: This often indicates UDP packet loss or receiver overload.\n"
                "        - Increase OS UDP/socket buffers (rmem_max / netdev_max_backlog)\n"
                "        - Increase ArvGvStream socket-buffer-size\n"
                "        - Enable/tune packet resend + packet-timeout/frame-retention\n"
                "        - Reduce bandwidth: ROI/FPS or add camera inter-packet delay\n",
                (int)st,
                config->n_completed_buffers, config->n_failures, config->n_underruns);
        }

        // Requeue buffer and move on
        arv_stream_push_buffer(stream, buffer);
        continue;
    }

    // 2) Get dimensions/data only for SUCCESS buffers
    if (refreshDimsOnEachFrame || dataAsImage.width == 0 || dataAsImage.height == 0)
    {
        dataAsImage.width  = (unsigned int) arv_buffer_get_image_width(buffer);
        dataAsImage.height = (unsigned int) arv_buffer_get_image_height(buffer);
    }

    if (dataAsImage.width == 0 || dataAsImage.height == 0)
    {
        brokenFrameNumber++;
        lastFrameIsBroken=1;
        arv_stream_push_buffer(stream, buffer);
        continue;
    }

    size_t size = 0;
    data = arv_buffer_get_image_data(buffer, &size);

    if (data == NULL || size == 0)
    {
        brokenFrameNumber++;
        lastFrameIsBroken=1;
        arv_stream_push_buffer(stream, buffer);
        continue;
    }

    dataAsImage.pixels = (unsigned char*)data;

    // NOTE: Hardcoded mono8 here. OK if camera is configured for Mono8.
    // If not, you should map pixel format -> channels/bpp properly.
    dataAsImage.channels     = 1;
    dataAsImage.bitsperpixel = 8;

    // Use size from Aravis rather than width*height when possible (safer for strides/packing).
    // If your downstream expects exact width*height, keep that, but ensure it matches `size`.
    dataAsImage.image_size = (unsigned int)size;

    { struct timespec _ts; clock_gettime(CLOCK_REALTIME, &_ts); dataAsImage.timestamp = (unsigned long)_ts.tv_sec * 1000000000UL + (unsigned long)_ts.tv_nsec; }

    // Stats + computed fps
    unsigned long endTime = GetTickCountMicroseconds();
    arv_stream_get_statistics(stream,
                              &config->n_completed_buffers,
                              &config->n_failures,
                              &config->n_underruns);

    config->actualFrameRate = (double)frameNumber / ((endTime - startTime) / 1000000.0);

    // --- Data consumers ---
    if (shm_stream != NULL)    { stream_image(shm_stream->frame, &dataAsImage);          }
    if (config->callback != 0) { camera_callback_relay(config, startGrab, &dataAsImage); }
    if (enabledFileOutput)     {
                                fprintf(config->csv_file, "%lu,", GetTickCountMicroseconds());
                                fprintf(config->csv_file, "%u\n", frameNumber);
                                if (cfg->compress) {
                                    snprintf(filename, 1024, "%.512s/colorFrame_0_%05u.png", cfg->outputDirectory, frameNumber);
                                    WritePNG(filename, &dataAsImage);
                                } else {
                                    snprintf(filename, 1024, "%.512s/colorFrame_0_%05u.pnm", cfg->outputDirectory, frameNumber);
                                    WritePPMG(filename, &dataAsImage);
                                }
                               }

    // Update counters
    frameNumber++;
    config->framesCaptured = frameNumber;

    // 3) Requeue buffer ASAP once done consuming its data
    arv_stream_push_buffer(stream, buffer);

    endGrab = GetTickCountMicroseconds();

    // 4) Optional pacing (if you truly need to limit CPU/bandwidth).
    if (config->frameRate > 0.0)
    {
        unsigned long microsecondsGrab   = endGrab - startGrab;
        unsigned long targetMicroseconds = (unsigned long)(1000000.0 / config->frameRate);
        if (microsecondsGrab < targetMicroseconds)
        {
            usleep(targetMicroseconds - microsecondsGrab);
        }
    }

    if (refreshDimsOnEachFrame)
     {
        dataAsImage.width  = 0;
        dataAsImage.height = 0;
    }
}


/*
    while (*config->keep_running && !termination_requested)
                  {
                    startGrab = GetTickCountMicroseconds();
                    buffer = arv_stream_pop_buffer (stream);


                    if (!ARV_IS_BUFFER(buffer)) {
                                  // No buffer ready yet (or stream starving). Sleep a bit to reduce CPU.
                                  usleep(1000);
                                  //continue;
                                } else

                    if (ARV_IS_BUFFER(buffer))
                    {
                        config->running = 1;
                        if (refreshDimsOnEachFrame)
                        {
                            dataAsImage.width        = arv_buffer_get_image_width (buffer);
                            dataAsImage.height       = arv_buffer_get_image_height(buffer);
                        }

                        if ((dataAsImage.width!=0) && (dataAsImage.height!=0))
                        {
                            size_t size;
                            data = arv_buffer_get_image_data(buffer,&size);
                            //printf ("Size =  %lu\n",size);
                            dataAsImage.pixels       = (unsigned char*) data;

                            dataAsImage.channels     = 1;
                            dataAsImage.bitsperpixel = 8;
                            dataAsImage.image_size   = dataAsImage.width  * dataAsImage.height * dataAsImage.channels;
                            dataAsImage.timestamp    = (unsigned int) GetTickCountMicroseconds() / 1000;

                            // Display some informations about the retrieved buffer
                            //printf ("Acquired %d×%d buffer\n",dataAsImage.width,dataAsImage.height);
                            unsigned long endTime = GetTickCountMicroseconds();

                            arv_stream_get_statistics (stream, &config->n_completed_buffers, &config->n_failures, &config->n_underruns);
                            //float frameRate = arv_camera_get_frame_rate (camera, NULL);
                            config->actualFrameRate = (double) frameNumber / ((endTime-startTime)/1000000);
                            //printf("\r %u Frames Grabbed (%u dropped) - @ %0.2f FPS (set %0.2f) ",frameNumber,brokenFrameNumber, config->actualFrameRate, frameRate );
                            //printf("Ok %lu/Fail %lu/Under %lu    \r",config->n_completed_buffers,config->n_failures,config->n_underruns);

                            if (shm_stream!=NULL)
                                       { stream_image(shm_stream->frame,&dataAsImage); }

                            if (config->callback!=0)
                                       { camera_callback_relay(config, startGrab, &dataAsImage); }

                            if (enabledFileOutput)
                            {
                            //Dump to accompanying file
                             fprintf(config->csv_file, "%lu,",GetTickCountMicroseconds());
                             fprintf(config->csv_file, "%u\n",frameNumber);
                             snprintf(filename,1024,"%.512s/colorFrame_0_%05u.pnm", cfg->outputDirectory, frameNumber);
                             WritePPMG(filename,&dataAsImage);
                            }

                            //At this time we consider that the image has been consumed!

                            frameNumber = frameNumber+1;
                            config->framesCaptured = frameNumber; // Update as soon as it is done
                        }
                        else
                        {
                            brokenFrameNumber = brokenFrameNumber + 1;
                        }

                        // Don't destroy the buffer, but put it back into the buffer pool
                        arv_stream_push_buffer (stream, buffer);
                    }
                    else
                    {
                        usleep(timeToSleepToWaitFor1Frame);
                    }

                    endGrab = GetTickCountMicroseconds();

                    if (config->frameRate!=0.0)
                    {
                        //Enforce framrates to prevent buffer underrun
                        microsecondsGrab = endGrab - startGrab;
                        // Calculate the time to usleep to achieve target framerate
                        unsigned long targetMicroseconds = 1000000 / config->frameRate;
                        if (microsecondsGrab < targetMicroseconds)
                        {
                            usleep(targetMicroseconds - microsecondsGrab);
                        }
                    }//We have a framerate set
                   //usleep(10);

                   if (refreshDimsOnEachFrame)
                        {   //Since we are resetting dims
                            //lets put them to zero
                            dataAsImage.width  = 0;
                            dataAsImage.height = 0;
                        }
                } //While loop
  */



    fprintf(stderr,"Camera Thread terminating\n");

    if (enabledFileOutput)
           { fclose(config->csv_file); }

    if (shm_stream!=NULL)
           { stopStream(shm_stream); }

    gigecamera_stopStream(config);
    return NULL;
}

