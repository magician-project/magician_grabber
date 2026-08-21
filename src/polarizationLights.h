#ifndef POLARIZATIONLIGHTS_H_INCLUDED
#define POLARIZATIONLIGHTS_H_INCLUDED

/** @file polarizationLights.h
 *  @brief Polarization-driven illumination control for the MagicianCam4 rig.
 *
 *  The XCG-CP510 carries a Sony IMX250MZR sensor: a 2x2 micro-polarizer mosaic
 *  over the pixel array, so a SINGLE frame already contains all four analyser
 *  angles. There is no need to capture multiple frames to recover polarization
 *  state — Stokes parameters, DoLP and AoLP all come out of one exposure.
 *
 *  Mosaic layout (matches PolarShadowVisionSensorCalibrationFromDatasets.py):
 *      (even row, even col) =  90 deg     (even row, odd col) =  45 deg
 *      (odd  row, even col) = 135 deg     (odd  row, odd col) =   0 deg
 *
 *  Stokes, per 2x2 superpixel:
 *      S0   = (I0 + I45 + I90 + I135) / 2      total intensity
 *      S1   =  I0  - I90                       0/90 linear component
 *      S2   =  I45 - I135                      45/135 linear component
 *      DoLP =  sqrt(S1^2 + S2^2) / S0          degree of linear polarization
 *      AoLP =  0.5 * atan2(S2, S1)             angle of linear polarization
 *
 *  ## Why this drives the lights
 *
 *  A directional source producing a specular lobe shows up unmistakably as high
 *  DoLP together with high S0 / near-saturation. Diffuse-dominant illumination
 *  gives low DoLP at comparable S0. So the polarization state is a direct,
 *  single-frame readout of "is this COB currently blowing out the surface", which
 *  is exactly the signal needed to choose between COBs.
 *
 *  ## Cost
 *
 *  Full resolution is 2448x2048 = 1.25M superpixels, too slow to reduce inside
 *  the frame callback at capture rates. We subsample on a stride grid (default
 *  every 8th superpixel, ~19k samples) and avoid per-sample sqrt/atan2 entirely:
 *  integer sums are accumulated and reduced once at the end. This runs in well
 *  under a millisecond and leaves the callback's timing budget intact.
 *
 *  ## Relationship to the controller's thermal limits
 *
 *  This module only ever chooses WHICH COB fires, never for how long. Pulse width
 *  and per-COB duty are owned exclusively by the firmware (see the safety note at
 *  the top of MagicianCam4_Rev_1.33.ino) and cannot be influenced from here. The
 *  schedule builder additionally guarantees every COB appears at least once per
 *  cycle, so even a degenerate policy spreads thermal load rather than pinning
 *  one COB — belt and braces with the firmware's leaky-bucket enforcement.
 *
 *  @author Ammar Qammaz (AmmarkoV)
 */

#ifdef __cplusplus
extern "C"
{
#endif

/* Forward declaration only. The tree carries two different definitions of
 * struct Image (imageStreamer.h and codecs/image.h, which disagree on the
 * constness of `pixels`), so pulling either one in here would collide with
 * whichever the including translation unit already had. The implementation
 * includes imageStreamer.h, matching the grabber's camera path. */
struct Image;

#define POL_MAX_LIGHTS      6   /**< COBs on the Cam4 board (GP6-GP11). */
#define POL_MAX_SCHEDULE   32   /**< Must not exceed MAX_SCHEDULE_LEN in the firmware. */
#define POL_STROBE_RING    16   /**< Recent strobe reports retained for attribution. */

/** Per-frame polarization summary produced by polarizationAnalyseFrame(). */
typedef struct
{
    double meanS0;      /**< Mean total intensity, 0-255. Exposure adequacy. */
    double polIndex;    /**< sum(|S1|+|S2|) / sum(S0). Division-free DoLP proxy; gloss/specular strength. */
    double satFraction; /**< Fraction of superpixels with any channel >= POL_SATURATION_LEVEL. */
    double aolpDegrees; /**< Dominant angle of linear polarization, degrees in [0,180). */
    double aolpCoherence;/**< 0-1. How aligned the polarization is; low means no dominant angle. */
    unsigned int samples;/**< Superpixels actually examined. */
} PolFrameStats;

/** Running state of the closed-loop light policy. Zero-initialise before use. */
typedef struct
{
    int    enabled;          /**< 0 = module inactive. */
    int    lightCount;       /**< COBs available; normally POL_MAX_LIGHTS. */
    int    stride;           /**< Superpixel subsampling stride. Larger = faster, noisier. */
    int    dwell;            /**< Consecutive frames each COB is held during a probe cycle. */
    double targetS0;         /**< Desired mean intensity; scores penalise deviation from this. */

    /** 1 when driving a legacy controller that cannot hold a schedule.
     *
     *  The policy is identical either way; only the delivery differs. A Rev 1.33+
     *  controller receives the whole schedule once per cycle and walks it against
     *  its own exposure signal. A legacy controller is told which COB to use, one
     *  digit per frame, from polarizationDriverProcessFrame(). The legacy path
     *  reintroduces the host round trip the exposure-locked mode was built to
     *  remove, so the light lands a frame or two late at high framerates — but it
     *  is no worse than the '+' stepping it replaces, and it is strictly better
     *  than '+' because the host names the COB instead of nudging a shared cursor
     *  that silently drifts when a command is dropped. */
    int    legacyStepping;
    int    legacyCursor;     /**< Position in the schedule; only used when legacyStepping. */

    /* Per-COB score accumulators, reset each cycle. */
    double sumPolIndex[POL_MAX_LIGHTS];
    double sumSatFraction[POL_MAX_LIGHTS];
    double sumMeanS0[POL_MAX_LIGHTS];
    unsigned int sampleCount[POL_MAX_LIGHTS];

    /* Last computed score per COB, kept for reporting. */
    double score[POL_MAX_LIGHTS];

    /* ── Strobe attribution ───────────────────────────────────────────────────
     * Which COB lit the frame currently in hand is NOT modelled host-side. A
     * host-side model drifts the moment the camera drops a frame (the controller
     * still strobed, the host never saw it), and it silently mis-attributes every
     * frame thereafter. Instead the controller reports the COB that actually
     * fired for every strobe, the serial thread pushes those into this ring, and
     * the camera thread attributes a frame only when the recent history is
     * unanimous — which is true in the middle of a dwell and false at its edges.
     *
     * This makes attribution independent of the ~1-2 frame GigE pipeline offset,
     * of dropped frames, and of firmware thermal substitutions (a substituted COB
     * is reported as what it really was, so the score lands on the right COB).
     * Written by the serial thread, read by the camera thread; a torn read can
     * only cause a frame to be discarded, never mis-attributed. */
    volatile unsigned char strobeRing[POL_STROBE_RING];
    volatile unsigned long strobeWrites;  /**< Monotonic count of strobes noted. */
    int    unanimityWindow;               /**< Recent strobes that must agree to attribute. */
    unsigned long framesAttributed;       /**< Frames that passed the unanimity test. */
    unsigned long framesDiscarded;        /**< Frames dropped at a dwell edge. */

    /* Frame bookkeeping. */
    unsigned long framesSeen;      /**< Frames fed to the driver since start. */
    unsigned long schedulesSent;   /**< Number of schedules uploaded to the controller. */

    unsigned char schedule[POL_MAX_SCHEDULE];
    int           scheduleLen;

    void         *arduino_cfg;     /**< ArduinoSerialConfig*, for uploading schedules. */
} PolLightDriver;

/** Threshold above which a channel counts as blown out. */
#define POL_SATURATION_LEVEL 250

/** Reduce one raw mosaic frame to a polarization summary.
 *  @param image  Mono8 frame straight off the sensor, undebayered.
 *  @param stride Superpixel subsampling stride (1 = every superpixel). Clamped to >=1.
 *  @param out    Receives the summary.
 *  @return 1 on success, 0 if the frame was unusable (wrong depth, too small, NULL). */
int polarizationAnalyseFrame(const struct Image *image, int stride, PolFrameStats *out);

/** Initialise the driver and push the opening schedule to the controller.
 *  @param drv          Driver state (need not be pre-zeroed).
 *  @param arduino_cfg  ArduinoSerialConfig* used to reach the controller.
 *  @param stride       Subsampling stride; 0 selects the default (8).
 *  @param dwell        Frames held per COB per cycle; 0 selects the default (3).
 *  @param legacyStepping 1 for a controller that cannot hold a schedule, so the
 *                      driver names the COB once per frame instead. See the field
 *                      of the same name in PolLightDriver.
 *  @return 1 on success. */
int polarizationDriverStart(PolLightDriver *drv, void *arduino_cfg, int stride, int dwell, int legacyStepping);

/** Record the COB that the controller reports as having actually fired.
 *  Call from the controller's serial callback, once per reported strobe.
 *  @param strobeLight 0-indexed COB, or >= lightCount if the pulse was suppressed. */
void polarizationDriverNoteStrobe(PolLightDriver *drv, int strobeLight);

/** Feed one captured frame to the policy. Call from the camera callback.
 *  Attributes the frame to a COB using the strobe ring (see PolLightDriver), and
 *  at the end of a probe cycle scores the COBs and uploads a new schedule.
 *  @return 1 if a new schedule was uploaded on this call, 0 otherwise. */
int polarizationDriverProcessFrame(PolLightDriver *drv, const struct Image *image);

/** Human-readable one-line summary of the last scoring round, for the progress display. */
void polarizationDriverSummary(const PolLightDriver *drv, char *buffer, unsigned int bufferSize);

#ifdef __cplusplus
}
#endif

#endif // POLARIZATIONLIGHTS_H_INCLUDED
