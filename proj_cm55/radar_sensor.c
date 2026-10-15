/*******************************************************************************
* File Name: radar_sensor.c
*
* Description: Radar processing for BGT60TR13C.
*
*  PIPELINE
*  ────────
*  1. DC removal + magnitude per chirp, averaged over 16 chirps
*  2. signal_pct  = normalised peak magnitude
*  3. breathing   = band-passed micro-variation of the peak magnitude
*                   (fast LPF − slow baseline ≈ 0.1–1.5 Hz life-sign band)
*  4. presence    = signal above threshold, with hysteresis + debounce,
*                   AND a life sign (breath band or motion) seen recently.
*                   A static reflector (book on the sensor, furniture) has a
*                   strong but lifeless return → presence drops after the
*                   life-hold window instead of sticking forever.
*  5. motion_pct  = mean |current_mag - prev_mag| across all bins
*  6. trend       = signal strength slope over last 8 frames
*  All outputs exponentially smoothed.
*******************************************************************************/

#include "radar_sensor.h"
#include "driver/radar/radar.h"
#include "buzzer.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Alert fires when motion_pct crosses this level (0-100) */
#define MOTION_ALERT_THRESHOLD  60U
/* Minimum frames between alerts (20 ms/frame → 150 = 3 s cooldown) */
#define MOTION_ALERT_COOLDOWN   150U
/* Alert tone: 1 kHz for 500 ms */
#define MOTION_ALERT_FREQ_HZ    1000U
#define MOTION_ALERT_DUR_MS     500U

#define NUM_SAMPLES_PER_CHIRP   (128U)
#define NUM_CHIRPS_PER_FRAME    (16U)
#define SAMPLES_PER_FRAME       (NUM_SAMPLES_PER_CHIRP * NUM_CHIRPS_PER_FRAME)

/* Range gate: only process bins within 1 metre
 * range_per_bin = c / (2 * BW / N) = 3e10 / (2 * 460e6 / 128) ≈ 4.17 cm/bin
 * 1 m = 100 cm → 100 / 4.17 ≈ 24 bins
 * Start after near-field coupling (skip 8 bins ≈ 33 cm)            */
#define SKIP_BINS               (8U)
#define MAX_RANGE_BINS          (32U)   /* SKIP_BINS + 24 ≈ 1 m limit        */

#define SIG_CEILING             (500U)  /* ADC counts = 100% signal           */
#define MOTION_CEILING          (80U)   /* ADC counts = 100% motion           */

#define ALPHA_SIGNAL            (4U)
#define ALPHA_MOTION            (3U)

/* ── Presence state machine (frames arrive at ~50 Hz) ─────────────────────
 * Hysteresis: enter above ENTER, leave below EXIT — the old single 55 %
 * threshold made "In crib" flicker whenever the signal hovered around it. */
#define PRESENCE_ENTER          (60U)   /* signal_pct to start detection      */
#define PRESENCE_EXIT           (45U)   /* signal_pct to end detection        */
#define PRESENCE_ON_FRAMES      (10U)   /* ~0.2 s above ENTER to switch on    */
#define PRESENCE_OFF_FRAMES     (100U)  /* ~2 s below EXIT to switch off      */

/* Life-sign gate: presence additionally requires breathing-band activity or
 * motion within this window.  A book/wall reflects strongly but shows no
 * micro-variation, so presence is released after the hold expires.         */
#define LIFE_HOLD_FRAMES        (750U)  /* ~15 s                              */
#define MOTION_LIFE_PCT         (8U)    /* raw motion % that counts as life   */

/* Breathing band levels (ADC counts of band-passed peak magnitude).
 * BREATH_MIN_LVL: below = noise floor (covered / empty).  Tune on hardware
 * using the [Radar] debug line: watch "br=" with a person vs a book.       */
#define BREATH_MIN_LVL          (3U)
#define BREATH_PCT_CEILING      (25U)   /* band counts shown as 100 %         */
#define BREATH_ACTIVE_MOTION    (15U)   /* gross motion also counts as alive  */

/* Direction: compare current smoothed signal to HIST_LEN frames ago.
 * Rising signal → target approaching (stronger reflection).
 * Falling signal → target receding.
 * HIST_LEN * ~100 ms/frame = ~800 ms lookback — immune to frame noise.  */
#define HIST_LEN                (8U)    /* frames of lookback                 */
#define APPROACH_THR            (5U)    /* % signal units to call motion      */

static SemaphoreHandle_t s_mutex  = NULL;
static radar_data_t      s_latest = {0};
static uint16_t         *s_frame  = NULL;

static radar_data_t process_frame(const uint16_t *frame)
{
    static uint32_t prev_mag[NUM_SAMPLES_PER_CHIRP] = {0};
    static uint32_t smooth_sig    = 0;
    static uint32_t smooth_motion = 0;
    static uint8_t  sig_ring[HIST_LEN] = {0};  /* circular history of smooth_sig */
    static uint8_t  ring_head = 0;

    /* Breathing band-pass state (Q4 fixed point for LPF precision) */
    static uint32_t lpf_fast_q4   = 0;  /* ~1.5 Hz low-pass of peak magnitude */
    static uint32_t lpf_slow_q4   = 0;  /* ~0.1 Hz baseline                   */
    static uint32_t breath_lvl_q4 = 0;  /* smoothed |fast − slow|             */
    static bool     lpf_seeded    = false;

    /* Presence state machine */
    static bool     presence_state = false;
    static uint32_t on_count   = 0;
    static uint32_t off_count  = 0;
    static uint32_t life_timer = 0;
    static uint32_t dbg_count  = 0;

    /* ── 1. Build magnitude range profile (1 m gate only) ───────────── */
    uint32_t mag[NUM_SAMPLES_PER_CHIRP];
    memset(mag, 0, sizeof(mag));

    for (int chirp = 0; chirp < (int)NUM_CHIRPS_PER_FRAME; chirp++)
    {
        const uint16_t *c = &frame[chirp * NUM_SAMPLES_PER_CHIRP];
        uint32_t dc = 0;
        for (int s = 0; s < (int)NUM_SAMPLES_PER_CHIRP; s++) dc += c[s];
        dc /= NUM_SAMPLES_PER_CHIRP;

        /* Only compute magnitude inside the 1 m range gate */
        for (int s = (int)SKIP_BINS; s < (int)MAX_RANGE_BINS; s++)
        {
            int32_t diff = (int32_t)c[s] - (int32_t)dc;
            mag[s] += (uint32_t)(diff < 0 ? -diff : diff);
        }
    }
    for (int s = (int)SKIP_BINS; s < (int)MAX_RANGE_BINS; s++)
        mag[s] /= NUM_CHIRPS_PER_FRAME;

    /* ── 2. Peak magnitude within 1 m gate → signal strength ────────── */
    uint32_t peak_mag = 0;
    for (int s = (int)SKIP_BINS; s < (int)MAX_RANGE_BINS; s++)
        if (mag[s] > peak_mag) peak_mag = mag[s];

    uint32_t raw_sig = (peak_mag * 100U) / SIG_CEILING;
    if (raw_sig > 100U) raw_sig = 100U;
    smooth_sig = (smooth_sig * (8U - ALPHA_SIGNAL) + raw_sig * ALPHA_SIGNAL) / 8U;

    /* ── 3. Breathing band: micro-variation of the peak magnitude ───── *
     * Chest movement modulates the reflection slowly (0.2-1 Hz).  A fast
     * LPF follows it, a much slower LPF tracks the static baseline; the
     * rectified difference is the life-sign band.  Static objects (book
     * over the sensor) settle to band ≈ 0.                              */
    uint32_t peak_q4 = peak_mag << 4;
    if (!lpf_seeded)
    {
        lpf_fast_q4 = lpf_slow_q4 = peak_q4;
        lpf_seeded  = true;
    }
    lpf_fast_q4 = (uint32_t)((int32_t)lpf_fast_q4 +
                  ((int32_t)peak_q4 - (int32_t)lpf_fast_q4) / 8);
    lpf_slow_q4 = (uint32_t)((int32_t)lpf_slow_q4 +
                  ((int32_t)peak_q4 - (int32_t)lpf_slow_q4) / 128);
    int32_t band_q4 = (int32_t)lpf_fast_q4 - (int32_t)lpf_slow_q4;
    if (band_q4 < 0) band_q4 = -band_q4;
    breath_lvl_q4 = (uint32_t)((int32_t)breath_lvl_q4 +
                    (band_q4 - (int32_t)breath_lvl_q4) / 16);
    uint32_t breath_lvl = breath_lvl_q4 >> 4;       /* back to ADC counts */

    uint32_t breath_pct = (breath_lvl * 100U) / BREATH_PCT_CEILING;
    if (breath_pct > 100U) breath_pct = 100U;

    /* ── 4. Motion (within 1 m gate only) ──────────────────────────── */
    uint32_t motion_acc = 0;
    for (int s = (int)SKIP_BINS; s < (int)MAX_RANGE_BINS; s++)
    {
        uint32_t diff = (mag[s] > prev_mag[s])
                        ? (mag[s] - prev_mag[s]) : (prev_mag[s] - mag[s]);
        motion_acc += diff;
    }
    motion_acc /= (MAX_RANGE_BINS - SKIP_BINS);
    memcpy(prev_mag, mag, sizeof(mag));

    uint32_t raw_motion = (motion_acc * 100U) / MOTION_CEILING;
    if (raw_motion > 100U) raw_motion = 100U;

    /* ── 5. Presence: hysteresis + debounce + life-sign gate ────────── */
    bool life_sign = (breath_lvl >= BREATH_MIN_LVL) || (raw_motion >= MOTION_LIFE_PCT);
    if (life_sign)
        life_timer = LIFE_HOLD_FRAMES;
    else if (life_timer > 0U)
        life_timer--;

    if (!presence_state)
    {
        off_count = 0;
        if ((smooth_sig >= PRESENCE_ENTER) && (life_timer > 0U))
        {
            if (++on_count >= PRESENCE_ON_FRAMES)
                presence_state = true;
        }
        else
        {
            on_count = 0;
        }
    }
    else
    {
        on_count = 0;
        if (smooth_sig <= PRESENCE_EXIT)
        {
            if (++off_count >= PRESENCE_OFF_FRAMES)
                presence_state = false;
        }
        else
        {
            off_count = 0;
        }
        /* Strong but lifeless reflection (covered sensor, static clutter):
           release presence once no life sign was seen for the hold window. */
        if (life_timer == 0U)
            presence_state = false;
    }
    bool presence = presence_state;

    if (presence)
        smooth_motion = (smooth_motion * (8U - ALPHA_MOTION) + raw_motion * ALPHA_MOTION) / 8U;
    else
        smooth_motion = (smooth_motion * 6U) / 8U;

    bool breathing = presence &&
                     ((breath_lvl >= BREATH_MIN_LVL) ||
                      (smooth_motion >= BREATH_ACTIVE_MOTION));

    /* Periodic tuning aid (~every 5 s at 50 Hz) */
    if ((++dbg_count % 256U) == 0U)
    {
        printf("[Radar] sig=%lu%% mot=%lu%% br=%lu (min %u) life=%lu pres=%d\r\n",
               (unsigned long)smooth_sig, (unsigned long)smooth_motion,
               (unsigned long)breath_lvl, (unsigned)BREATH_MIN_LVL,
               (unsigned long)life_timer, (int)presence);
    }

    /* ── 6. Direction: compare current signal to HIST_LEN frames ago ── */
    uint8_t sig_old  = sig_ring[ring_head];
    sig_ring[ring_head] = (uint8_t)smooth_sig;
    ring_head = (uint8_t)((ring_head + 1U) % HIST_LEN);

    int8_t trend = 0;
    if (presence && sig_old > 0U)
    {
        int32_t delta = (int32_t)smooth_sig - (int32_t)sig_old;
        if      (delta >  (int32_t)APPROACH_THR) trend =  1;
        else if (delta < -(int32_t)APPROACH_THR) trend = -1;
    }
    else if (!presence)
    {
        /* Clear history so next detection starts fresh */
        memset(sig_ring, 0, sizeof(sig_ring));
        ring_head = 0;
    }

    radar_data_t d;
    d.presence   = presence;
    d.breathing  = breathing;
    d.signal_pct = (uint8_t)smooth_sig;
    d.motion_pct = (uint8_t)smooth_motion;
    d.breath_pct = (uint8_t)breath_pct;
    d.trend      = trend;

    for (int i = 0; i < (int)RADAR_RANGE_BINS; i++)
    {
        uint32_t v = (mag[i + (int)SKIP_BINS] * 100U) / SIG_CEILING;
        d.range_bins[i] = (uint8_t)(v > 100U ? 100U : v);
    }
    return d;
}

void radar_task(void *arg)
{
    (void)arg;

    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) { vTaskDelete(NULL); return; }

    s_frame = (uint16_t *)pvPortMalloc(SAMPLES_PER_FRAME * sizeof(uint16_t));
    if (s_frame == NULL) { vTaskDelete(NULL); return; }

    if (radar_init() != 0)
    {
        printf("[Radar] init failed.\r\n");
        vPortFree(s_frame);
        vTaskDelete(NULL);
        return;
    }
    printf("[Radar] BGT60TR13C ready.\r\n");

    uint32_t alert_cooldown = 0U;

    for (;;)
    {
        if (radar_is_data_available())
        {
            if (radar_read_data(s_frame, SAMPLES_PER_FRAME) == 0)
            {
                radar_data_t d = process_frame(s_frame);
                xSemaphoreTake(s_mutex, portMAX_DELAY);
                s_latest = d;
                xSemaphoreGive(s_mutex);

                if (alert_cooldown > 0U)
                {
                    alert_cooldown--;
                }
                else if (d.motion_pct >= MOTION_ALERT_THRESHOLD &&
                         !buzzer_is_playing())
                {
                    /* Motion alert beep removed — buzzer reserved for lullaby/welcome sound */
                    alert_cooldown = MOTION_ALERT_COOLDOWN;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

radar_data_t radar_get_latest(void)
{
    radar_data_t copy = {0};
    if (s_mutex != NULL)
    {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        copy = s_latest;
        xSemaphoreGive(s_mutex);
    }
    return copy;
}
