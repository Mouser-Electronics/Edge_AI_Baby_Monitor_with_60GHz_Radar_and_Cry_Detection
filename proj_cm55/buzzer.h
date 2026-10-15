/*******************************************************************************
* buzzer.h — TDM/I2S speaker driver: tone generation, PCM streaming,
*             ring-buffer stream mode, volume control, and mute.
*
*  Hardware path:
*    CM55 → TDM_STRUCT0 TX → TLV320DAC3100 codec (SCB0) → Class-D amp → speaker
*  Sample rate: 48 kHz stereo
*******************************************************************************/
#ifndef BUZZER_H_
#define BUZZER_H_

#include <stdint.h>
#include <stdbool.h>

/* Initialise TDM peripheral and TLV320DAC3100 codec.  Call once after RTOS. */
void    buzzer_init(void);

/* ── Tone output ──────────────────────────────────────────────────────────── */
void    buzzer_beep(uint32_t freq_hz, uint32_t duration_ms);
void    buzzer_stop(void);

/* ── PCM streaming (flat array, 48 kHz mono, no upsample) ───────────────── */
/* Loops if loop=true; stops when buzzer_stop() is called. */
void    buzzer_play_pcm(const int16_t *data, uint32_t len_samples, bool loop);

/* ── Ring-buffer stream mode (feed from a decoder task) ─────────────────── */
/* Call buzzer_stream_start() from the decoder task, then push samples with   */
/* buzzer_stream_write() in a loop.  ISR reads the ring buffer directly.      */
void     buzzer_stream_start(void);
uint32_t buzzer_stream_writable(void);   /* free slots in ring buffer         */
void     buzzer_stream_write(const int16_t *data, uint32_t n);

/* ── Volume / mute ────────────────────────────────────────────────────────── */
void    buzzer_set_volume(uint8_t vol_pct);   /* 0-100 */
uint8_t buzzer_get_volume(void);
void    buzzer_set_muted(bool mute);
bool    buzzer_is_muted(void);

/* ── Status ───────────────────────────────────────────────────────────────── */
bool    buzzer_is_playing(void);
uint8_t buzzer_get_fifo_level(void);

#endif /* BUZZER_H_ */
