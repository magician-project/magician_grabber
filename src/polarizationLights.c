/** @file polarizationLights.c
 *  @brief Polarization-driven illumination control. See polarizationLights.h.
 *  @author Ammar Qammaz (AmmarkoV)
 */

#include "polarizationLights.h"
#include "imageStreamer.h"   //definition of struct Image used by the camera path
#include "arduinoSensor.h"   //also pulls in common.h for the colour macros

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define POL_DEFAULT_STRIDE 8
#define POL_DEFAULT_DWELL  3
#define POL_DEFAULT_TARGET_S0 110.0

/* Weight of a fresh cycle's measurement against the running score. Scenes change
 * slowly relative to a cycle (~0.8 s at 22 fps), so we smooth rather than jump. */
#define POL_SCORE_EMA 0.35

/* Score penalties. Saturation dominates: a blown-out surface is unrecoverable,
 * whereas a merely glossy one still carries usable polarization structure. */
#define POL_WEIGHT_SATURATION 4.0
#define POL_WEIGHT_POLINDEX   1.0
#define POL_WEIGHT_EXPOSURE   1.0

/* Extra dwell slots handed to the best-scoring COBs, on top of the guaranteed
 * one slot each. Bounded so a degenerate policy still spreads thermal load. */
#define POL_BONUS_SLOTS 2

// =============================================================================
//  Frame reduction
// =============================================================================

int polarizationAnalyseFrame(const struct Image *image, int stride, PolFrameStats *out)
{
    if (out == 0) { return 0; }
    memset(out, 0, sizeof(PolFrameStats));

    if (image == 0)             { return 0; }
    if (image->pixels == 0)     { return 0; }
    if (image->bitsperpixel!=8) { return 0; }  /* mosaic maths assumes mono8 */
    if (image->channels != 1)   { return 0; }
    if (image->width < 4)       { return 0; }
    if (image->height < 4)      { return 0; }

    if (stride < 1) { stride = 1; }

    const unsigned char *p = image->pixels;
    const unsigned int   w = image->width;
    const unsigned int   h = image->height;

    /* Step two rows/cols per superpixel, times the subsampling stride. */
    const unsigned int step = (unsigned int) (2 * stride);

    unsigned long long sumS0  = 0;
    unsigned long long sumAbs = 0;
    unsigned long      sat    = 0;
    unsigned long      n      = 0;

    /* Vector accumulation of the polarization direction: summing (S1,S2) and
     * taking the angle once at the end is both faster than a per-sample atan2
     * and the statistically correct way to average an angle that wraps. */
    double accX = 0.0;
    double accY = 0.0;

    for (unsigned int y = 0; y + 1 < h; y += step)
    {
        const unsigned char *rowEven = p + (size_t) y       * w;
        const unsigned char *rowOdd  = p + (size_t)(y + 1)  * w;

        for (unsigned int x = 0; x + 1 < w; x += step)
        {
            /* Mosaic: even row = 90,45 ; odd row = 135,0 */
            int i90  = rowEven[x];
            int i45  = rowEven[x + 1];
            int i135 = rowOdd[x];
            int i0   = rowOdd[x + 1];

            int s0 = (i0 + i45 + i90 + i135) >> 1;
            int s1 = i0  - i90;
            int s2 = i45 - i135;

            sumS0  += (unsigned long long) s0;
            sumAbs += (unsigned long long) (abs(s1) + abs(s2));

            if (i0   >= POL_SATURATION_LEVEL || i45 >= POL_SATURATION_LEVEL ||
                i90  >= POL_SATURATION_LEVEL || i135>= POL_SATURATION_LEVEL)
                { sat++; }

            accX += (double) s1;
            accY += (double) s2;
            n++;
        }
    }

    if (n == 0) { return 0; }

    out->samples     = (unsigned int) n;
    out->meanS0      = (double) sumS0 / (double) (n * 2);   /* S0 spans 0..510 */
    out->polIndex    = (sumS0 > 0) ? (double) sumAbs / (double) sumS0 : 0.0;
    out->satFraction = (double) sat / (double) n;

    /* 0.5*atan2 maps the doubled-angle Stokes representation back to [0,180). */
    double angle2 = atan2(accY, accX);            /* doubled angle, radians */
    double angle  = 0.5 * angle2 * (180.0 / M_PI);
    if (angle < 0.0) { angle += 180.0; }
    out->aolpDegrees = angle;

    /* Coherence: 1 when every sample shares one polarization angle, ~0 when the
     * field has no dominant direction.
     *
     * The natural numerator is the resultant length |sum(S1,S2)|, but the only
     * denominator we have is sum(|S1|+|S2|) — an L1 sum, deliberately, to keep the
     * inner loop free of per-sample sqrt. L1 and L2 differ by an angle-dependent
     * factor, so the raw ratio peaks at 1.0 only when the angle happens to land on
     * an analyser axis and sags to 1/sqrt(2) between them. Dividing that factor out
     * using the angle we just recovered makes the value mean what it claims at any
     * angle, and costs two trig calls once per frame rather than per sample. */
    double resultant = sqrt(accX * accX + accY * accY);
    double totalMag  = (double) sumAbs;
    if (totalMag > 0.0)
    {
        double l1overL2 = fabs(cos(angle2)) + fabs(sin(angle2));  /* in [1, sqrt(2)] */
        double coherence = (resultant * l1overL2) / totalMag;
        if (coherence > 1.0) { coherence = 1.0; }
        if (coherence < 0.0) { coherence = 0.0; }
        out->aolpCoherence = coherence;
    }
    else { out->aolpCoherence = 0.0; }

    return 1;
}

// =============================================================================
//  Schedule construction
// =============================================================================

/* Build the base cycle in opposite-pair order: 0,3,1,4,2,5.
 *
 * Consecutive entries come from opposing sides of the ring, which is what makes
 * a per-pixel min() over the pair suppress specular lobes (a point cannot be
 * specular to two opposing sources at once) and what resolves the 180 degree
 * azimuth ambiguity inherent in AoLP. Plain round-robin gives neither. */
static void polBuildOppositeOrder(int lightCount, unsigned char *order)
{
    static const unsigned char alt[POL_MAX_LIGHTS] = {3, 4, 5, 1, 2, 0};
    unsigned char cur = 0;
    for (int i = 0; i < lightCount; i++)
    {
        order[i] = cur;
        cur = (lightCount == POL_MAX_LIGHTS) ? alt[cur]
                                             : (unsigned char)((cur + 1) % lightCount);
    }
}

/* Assemble the next schedule: every COB gets `dwell` consecutive slots so the
 * attribution window has a stable middle, and the best-scoring COBs get bonus
 * dwell blocks appended.
 *
 * Every COB always appears. That is deliberate and is the host-side half of the
 * thermal story: full photometric coverage is preserved for offline processing,
 * and no policy outcome — however degenerate — can concentrate the strobe load on
 * a single COB. The firmware's per-COB leaky bucket is the hard backstop; this is
 * the soft one that keeps the backstop from ever having to engage. */
static int polBuildSchedule(PolLightDriver *drv)
{
    unsigned char order[POL_MAX_LIGHTS];
    polBuildOppositeOrder(drv->lightCount, order);

    int len = 0;

    for (int i = 0; i < drv->lightCount && len + drv->dwell <= POL_MAX_SCHEDULE; i++)
    {
        for (int d = 0; d < drv->dwell; d++) { drv->schedule[len++] = order[i]; }
    }

    /* Rank COBs by score, best first (selection sort; six elements). */
    int ranked[POL_MAX_LIGHTS];
    for (int i = 0; i < drv->lightCount; i++) { ranked[i] = i; }
    for (int i = 0; i < drv->lightCount - 1; i++)
    {
        for (int j = i + 1; j < drv->lightCount; j++)
        {
            if (drv->score[ranked[j]] > drv->score[ranked[i]])
            { int t = ranked[i]; ranked[i] = ranked[j]; ranked[j] = t; }
        }
    }

    for (int b = 0; b < POL_BONUS_SLOTS && b < drv->lightCount; b++)
    {
        if (len + drv->dwell > POL_MAX_SCHEDULE) { break; }
        for (int d = 0; d < drv->dwell; d++)
        { drv->schedule[len++] = (unsigned char) ranked[b]; }
    }

    drv->scheduleLen = len;
    return len;
}

static int polUploadSchedule(PolLightDriver *drv)
{
    if (drv->arduino_cfg == 0) { return 0; }

    /* A legacy controller has nowhere to put a schedule; it is stepped one COB at a
     * time from polLegacyStep() instead. Recomputing the schedule still matters --
     * it is what polLegacyStep walks -- so this is a successful no-op, not a
     * failure. */
    if (drv->legacyStepping) { drv->schedulesSent++; return 1; }

    int sent = arduino_sendLightSchedule((ArduinoSerialConfig *) drv->arduino_cfg,
                                         drv->schedule, drv->scheduleLen);
    if (sent) { drv->schedulesSent++; }
    return sent;
}

/* Advance the legacy cursor and tell the controller which COB to light next.
 * Called once per captured frame, replacing the '+' the grabber used to send. */
static void polLegacyStep(PolLightDriver *drv)
{
    if (drv->arduino_cfg == 0)  { return; }
    if (drv->scheduleLen <= 0)  { return; }

    drv->legacyCursor++;
    if (drv->legacyCursor >= drv->scheduleLen) { drv->legacyCursor = 0; }

    unsigned char want = drv->schedule[drv->legacyCursor];
    ArduinoSerialConfig *arduino = (ArduinoSerialConfig *) drv->arduino_cfg;
    arduino_sendLightSelect(arduino, (int) want);

    /* Firmware old enough to lack the strobe fields reports no ground truth, so
     * there is nothing to attribute against. Feed the ring what we just commanded
     * instead. That is sound *only* on this path: the host names an absolute COB
     * index every frame, so there is no controller-side cursor that could silently
     * drift out of step -- the failure the ring exists to defend against. The
     * pipeline delay remains, and the unanimity window absorbs it exactly as it
     * does for a reporting controller. */
    if (!arduino->haveStrobeFields)
       { polarizationDriverNoteStrobe(drv, (int) want); }
}

// =============================================================================
//  Scoring
// =============================================================================

/* Fold one cycle's accumulated measurements into the running per-COB scores.
 *
 *   score = -w_sat  * saturated fraction        blown highlights are unusable
 *           -w_pol  * polarization index        specular-dominated is undesirable
 *           -w_exp  * |meanS0 - target| / target   too dark or too hot
 *
 * Higher is better; all terms are penalties, so a perfect COB scores 0. A COB
 * with no attributed samples this cycle keeps its previous score untouched. */
static void polScoreCycle(PolLightDriver *drv)
{
    for (int i = 0; i < drv->lightCount; i++)
    {
        if (drv->sampleCount[i] == 0) { continue; }

        double inv  = 1.0 / (double) drv->sampleCount[i];
        double pol  = drv->sumPolIndex[i]    * inv;
        double sat  = drv->sumSatFraction[i] * inv;
        double s0   = drv->sumMeanS0[i]      * inv;

        double exposureError = fabs(s0 - drv->targetS0) / drv->targetS0;

        double fresh = - POL_WEIGHT_SATURATION * sat
                       - POL_WEIGHT_POLINDEX   * pol
                       - POL_WEIGHT_EXPOSURE   * exposureError;

        drv->score[i] = (1.0 - POL_SCORE_EMA) * drv->score[i] + POL_SCORE_EMA * fresh;
    }

    memset(drv->sumPolIndex,    0, sizeof(drv->sumPolIndex));
    memset(drv->sumSatFraction, 0, sizeof(drv->sumSatFraction));
    memset(drv->sumMeanS0,      0, sizeof(drv->sumMeanS0));
    memset(drv->sampleCount,    0, sizeof(drv->sampleCount));
}

// =============================================================================
//  Public entry points
// =============================================================================

int polarizationDriverStart(PolLightDriver *drv, void *arduino_cfg, int stride, int dwell, int legacyStepping)
{
    if (drv == 0) { return 0; }
    memset(drv, 0, sizeof(PolLightDriver));

    drv->enabled        = 1;
    drv->lightCount     = POL_MAX_LIGHTS;
    drv->stride         = (stride > 0) ? stride : POL_DEFAULT_STRIDE;
    drv->dwell          = (dwell  > 0) ? dwell  : POL_DEFAULT_DWELL;
    drv->targetS0       = POL_DEFAULT_TARGET_S0;
    drv->arduino_cfg    = arduino_cfg;
    drv->legacyStepping = legacyStepping;
    drv->legacyCursor   = 0;

    /* A frame is attributed only if this many consecutive recent strobe reports
     * name the same COB. It must exceed the GigE pipeline offset (1-2 frames) yet
     * stay below the dwell, or no frame in a dwell would ever qualify. */
    drv->unanimityWindow = drv->dwell - 1;
    if (drv->unanimityWindow < 2)                 { drv->unanimityWindow = 2; }
    if (drv->unanimityWindow > POL_STROBE_RING)   { drv->unanimityWindow = POL_STROBE_RING; }

    for (int i = 0; i < POL_STROBE_RING; i++) { drv->strobeRing[i] = 0xFF; }

    polBuildSchedule(drv);
    polUploadSchedule(drv);
    if (drv->legacyStepping)
    {
        //Put the first COB up now; from here each captured frame advances one step.
        arduino_sendLightSelect((ArduinoSerialConfig *) drv->arduino_cfg,
                                (int) drv->schedule[0]);
    }

    fprintf(stderr, GREEN "Polarization light driver active" NORMAL
                    " (stride %d, dwell %d, %d-slot schedule, unanimity %d, %s stepping)\n",
            drv->stride, drv->dwell, drv->scheduleLen, drv->unanimityWindow,
            drv->legacyStepping ? "legacy per-frame" : "controller exposure-locked");
    return 1;
}

void polarizationDriverNoteStrobe(PolLightDriver *drv, int strobeLight)
{
    if (drv == 0)        { return; }
    if (!drv->enabled)   { return; }

    unsigned long w = drv->strobeWrites;
    drv->strobeRing[w % POL_STROBE_RING] =
        (strobeLight >= 0 && strobeLight < drv->lightCount) ? (unsigned char) strobeLight
                                                            : (unsigned char) 0xFF;
    drv->strobeWrites = w + 1;   /* published after the payload */
}

/* Return the COB that the last `unanimityWindow` strobes all agree on, or -1. */
static int polAttributeFrame(const PolLightDriver *drv)
{
    unsigned long w = drv->strobeWrites;
    if (w < (unsigned long) drv->unanimityWindow) { return -1; }

    unsigned char first = drv->strobeRing[(w - 1) % POL_STROBE_RING];
    if (first >= drv->lightCount) { return -1; }   /* suppressed pulse */

    for (int k = 1; k < drv->unanimityWindow; k++)
    {
        if (drv->strobeRing[(w - 1 - k) % POL_STROBE_RING] != first) { return -1; }
    }
    return (int) first;
}

int polarizationDriverProcessFrame(PolLightDriver *drv, const struct Image *image)
{
    if (drv == 0)      { return 0; }
    if (!drv->enabled) { return 0; }

    drv->framesSeen++;

    int light = polAttributeFrame(drv);
    if (light < 0)
    {
        drv->framesDiscarded++;
        //Still advance a legacy controller, or the sequence would stall on any frame
        //that lands on a dwell boundary.
        if (drv->legacyStepping) { polLegacyStep(drv); }
        return 0;
    }

    PolFrameStats stats;
    if (!polarizationAnalyseFrame(image, drv->stride, &stats))
    {
        if (drv->legacyStepping) { polLegacyStep(drv); }
        return 0;
    }

    drv->sumPolIndex[light]    += stats.polIndex;
    drv->sumSatFraction[light] += stats.satFraction;
    drv->sumMeanS0[light]      += stats.meanS0;
    drv->sampleCount[light]    += 1;
    drv->framesAttributed++;

    /* A cycle is complete once every COB has contributed at least one sample.
     * Keying off coverage rather than a frame count means dropped frames delay
     * the update instead of corrupting it. */
    int cycleComplete = 1;
    for (int i = 0; i < drv->lightCount; i++)
    {
        if (drv->sampleCount[i] == 0) { cycleComplete = 0; break; }
    }

    int uploaded = 0;
    if (cycleComplete)
    {
        polScoreCycle(drv);
        polBuildSchedule(drv);
        uploaded = polUploadSchedule(drv);
    }

    /* Step last, so the command that goes out reflects any schedule just rebuilt. */
    if (drv->legacyStepping) { polLegacyStep(drv); }
    return uploaded;
}

void polarizationDriverSummary(const PolLightDriver *drv, char *buffer, unsigned int bufferSize)
{
    if (buffer == 0 || bufferSize == 0) { return; }
    if (drv == 0 || !drv->enabled) { buffer[0] = 0; return; }

    int best = 0;
    for (int i = 1; i < drv->lightCount; i++)
    { if (drv->score[i] > drv->score[best]) { best = i; } }

    snprintf(buffer, bufferSize, "|Pol best L%d (%.2f) sched %lu attr %lu/%lu",
             best + 1, drv->score[best], drv->schedulesSent,
             drv->framesAttributed, drv->framesSeen);
}
