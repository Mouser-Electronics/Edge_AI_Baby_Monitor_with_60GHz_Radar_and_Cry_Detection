/*******************************************************************************
* lullaby_player.h — Lullaby playback and multi-sensor auto-trigger logic.
*
*  Trigger conditions (auto-start):
*    1. Baby has cried N times within a 5-minute sliding window  (default N=3)
*    2. Radar motion exceeds the configured threshold for 8+ seconds
*
*  Auto-stop:
*    Lullaby has been playing for at least 60 s AND baby has been calm for
*    2 minutes (no crying + motion < 20 %).
*******************************************************************************/
#ifndef LULLABY_PLAYER_H_
#define LULLABY_PLAYER_H_

#include <stdbool.h>
#include <stdint.h>
#include "cry_detector.h"
#include "radar_sensor.h"

/* ── Lifecycle ────────────────────────────────────────────────────────────── */
void lullaby_player_init(void);

/* ── Manual control ───────────────────────────────────────────────────────── */
void lullaby_player_start(void);
void lullaby_player_stop(void);
void lullaby_player_toggle(void);
bool lullaby_player_is_playing(void);

/* ── Algorithm update — call every 200 ms from the LVGL timer ────────────── */
void lullaby_player_update(const cry_state_t *cry, const radar_data_t *radar);

/* ── Threshold configuration ─────────────────────────────────────────────── */
/* Radar motion % required to trigger (20–90, default 70) */
void    lullaby_set_radar_threshold(uint8_t pct);
uint8_t lullaby_get_radar_threshold(void);

/* ── UI helpers ───────────────────────────────────────────────────────────── */
/* Short status string: "Playing (auto)", "Stopped", "2/3 cries", etc. */
const char *lullaby_player_get_status(void);
/* Recent cry count within the 5-min window (for UI display) */
uint8_t lullaby_get_window_cry_count(void);

#endif /* LULLABY_PLAYER_H_ */
