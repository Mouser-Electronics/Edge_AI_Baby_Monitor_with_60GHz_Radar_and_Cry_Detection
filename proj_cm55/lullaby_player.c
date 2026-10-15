/*******************************************************************************
* lullaby_player.c — Lullaby playback + multi-sensor trigger algorithm.
*
*  Playback engine
*  ───────────────
*  The song is stored in flash as pre-converted 48 kHz mono PCM
*  (lullaby_data.c, generated on a PC by regen_audio_48k.py).  Playback hands
*  that array to buzzer_play_pcm(); the TDM ISR reads the samples directly
*  from flash at 48 kHz.  No MP3 decoding runs on the device.  The lullaby
*  loops indefinitely until stopped.
*
*  Algorithm overview
*  ──────────────────
*  State machine: IDLE -> PLAYING -> (auto-stop) -> IDLE
*
*  Trigger (IDLE -> PLAYING):
*    A. Cry count: a circular timestamp buffer tracks the last MAX_CRY_EVENTS
*       cry events.  When >= CRY_TRIGGER_COUNT cries fall inside the 5-minute
*       window the lullaby auto-starts.
*    B. Radar agitation: when motion_pct >= threshold for >= AGITATION_MS ms
*       the lullaby auto-starts (sustained movement = baby is restless).
*
*  Auto-stop (PLAYING -> IDLE, only for auto-triggered sessions):
*    Baby is calm for CALM_MS ms:
*      - no active cry detected
*      - last cry event > CALM_MS ago
*      - radar motion < 20 %
*    AND the lullaby has played for at least MIN_PLAY_MS ms.
*******************************************************************************/

#include "lullaby_player.h"
#include "lullaby_data.h"
#include "buzzer.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>

/*────────────────────────────────────────────────────────────────────────────*/
#define CRY_TRIGGER_COUNT   3u
#define MAX_CRY_EVENTS      20u
#define CRY_WINDOW_MS       (5u * 60u * 1000u)
#define AGITATION_MS        8000u
#define CALM_MS             (2u * 60u * 1000u)
#define MIN_PLAY_MS         60000u
#define RADAR_CALM_THRESH   20u
#define DEFAULT_RADAR_THR   70u

/*────────────────────────────────────────────────────────────────────────────*/
static bool           s_playing        = false;
static bool           s_auto_triggered = false;
static TickType_t     s_play_start     = 0u;
static TickType_t     s_last_cry_tick  = 0u;
static uint32_t       s_last_cry_count = 0u;

static TickType_t s_cry_ts[MAX_CRY_EVENTS];
static uint8_t    s_cry_head  = 0u;
static uint8_t    s_win_count = 0u;

static TickType_t s_high_motion_start = 0u;
static bool       s_in_high_motion    = false;

static uint8_t s_radar_threshold = DEFAULT_RADAR_THR;

static char s_status_buf[48];


/*────────────────────────────────────────────────────────────────────────────*/
void lullaby_player_init(void)
{
    memset(s_cry_ts, 0, sizeof(s_cry_ts));
    printf("[Lullaby] init — %lu samples at 48 kHz (ext flash @ 0x60C00000)\r\n",
           (unsigned long)lullaby_pcm_samples);
    fflush(stdout);
}

/*────────────────────────────────────────────────────────────────────────────*/
void lullaby_player_start(void)
{
    if (s_playing) return;
    s_playing    = true;
    s_play_start = xTaskGetTickCount();
    buzzer_play_pcm(lullaby_pcm_data, lullaby_pcm_samples, true);
    printf("[Lullaby] started\r\n"); fflush(stdout);
}

void lullaby_player_stop(void)
{
    if (!s_playing) return;
    s_playing        = false;
    s_auto_triggered = false;
    buzzer_stop();
    printf("[Lullaby] stopped\r\n"); fflush(stdout);
}

void lullaby_player_toggle(void)
{
    if (s_playing)
        lullaby_player_stop();
    else
        lullaby_player_start();
}

bool lullaby_player_is_playing(void) { return s_playing; }

/*────────────────────────────────────────────────────────────────────────────*/
void lullaby_set_radar_threshold(uint8_t pct)
{
    if (pct < 20u) pct = 20u;
    if (pct > 90u) pct = 90u;
    s_radar_threshold = pct;
}

uint8_t lullaby_get_radar_threshold(void) { return s_radar_threshold; }
uint8_t lullaby_get_window_cry_count(void) { return s_win_count; }

/*────────────────────────────────────────────────────────────────────────────*/
void lullaby_player_update(const cry_state_t *cry, const radar_data_t *radar)
{
    TickType_t now = xTaskGetTickCount();

    uint32_t cur_count = cry->cry_count;
    if (cur_count > s_last_cry_count)
    {
        for (uint32_t k = 0u; k < (cur_count - s_last_cry_count); k++)
        {
            s_cry_ts[s_cry_head] = now;
            s_cry_head = (uint8_t)((s_cry_head + 1u) % MAX_CRY_EVENTS);
        }
        s_last_cry_count = cur_count;
        s_last_cry_tick  = now;
    }

    uint8_t wcount = 0u;
    for (uint8_t i = 0u; i < MAX_CRY_EVENTS; i++)
    {
        if (s_cry_ts[i] != 0u &&
            (now - s_cry_ts[i]) <= pdMS_TO_TICKS(CRY_WINDOW_MS))
            wcount++;
    }
    s_win_count = wcount;

    if (radar->motion_pct >= s_radar_threshold)
    {
        if (!s_in_high_motion)
        {
            s_high_motion_start = now;
            s_in_high_motion    = true;
        }
    }
    else
    {
        s_in_high_motion = false;
    }

    if (!s_playing && !cry->model_expired && !cry->hw_error)
    {
        bool trigger = false;
        if (wcount >= CRY_TRIGGER_COUNT) trigger = true;
        if (s_in_high_motion &&
            (now - s_high_motion_start) >= pdMS_TO_TICKS(AGITATION_MS))
            trigger = true;
        if (trigger)
        {
            lullaby_player_start();
            s_auto_triggered = true;
        }
    }

    if (s_playing && s_auto_triggered)
    {
        bool min_played = ((now - s_play_start) >= pdMS_TO_TICKS(MIN_PLAY_MS));
        bool calm = (!cry->is_crying &&
                     (s_last_cry_tick == 0u ||
                      (now - s_last_cry_tick) >= pdMS_TO_TICKS(CALM_MS)) &&
                     radar->motion_pct < RADAR_CALM_THRESH);
        if (min_played && calm)
        {
            printf("[Lullaby] auto-stop — baby calm\r\n"); fflush(stdout);
            lullaby_player_stop();
        }
    }
}

/*────────────────────────────────────────────────────────────────────────────*/
const char *lullaby_player_get_status(void)
{
    if (s_playing)
    {
        if (s_auto_triggered)
            snprintf(s_status_buf, sizeof(s_status_buf), "Playing (auto)");
        else
            snprintf(s_status_buf, sizeof(s_status_buf), "Playing");
    }
    else
    {
        if (s_win_count > 0u)
            snprintf(s_status_buf, sizeof(s_status_buf),
                     "Idle  (%u/%u cries)", (unsigned)s_win_count,
                     (unsigned)CRY_TRIGGER_COUNT);
        else
            snprintf(s_status_buf, sizeof(s_status_buf), "Idle");
    }
    return s_status_buf;
}
