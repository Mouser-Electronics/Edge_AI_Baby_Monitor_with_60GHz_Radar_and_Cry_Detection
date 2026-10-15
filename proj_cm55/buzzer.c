/*******************************************************************************
* File Name: buzzer.c
*
* Description: Interrupt-driven I2S driver using TDM_STRUCT0.
*
*  Modes
*  ─────
*  TONE   — square-wave ring buffer at a fixed frequency (buzzer_beep)
*  PCM    — 16 kHz mono s16 source upsampled 3× to 48 kHz stereo (buzzer_play_pcm)
*
*  Volume is applied in the ISR via integer scaling (no I2C needed at runtime).
*  Mute outputs zeros without stopping the TDM clock, avoiding codec glitches.
*******************************************************************************/

#include "buzzer.h"
#include "cycfg_peripherals.h"
#include "cy_tdm.h"
#include "cy_sysint.h"
#include "cy_scb_i2c.h"
#include "cy_sysclk.h"
#include "mtb_hal_i2c.h"
#include "mtb_tlv320dac3100.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>

/*────────────────────────────────────────────────────────────────────────────*/
#define BUZZER_SAMPLE_RATE   48000u
#define BUZZER_AMPLITUDE     16000      /* ±50% full scale for tones */
#define TONE_BUF_PAIRS       256u       /* 256 stereo pairs = 1 KB BSS */
#define FIFO_REFILL_ENTRIES  64u        /* half FIFO — matches trigger level */

#define CODEC_MCLK_HZ        12288000u
#define CODEC_VOLUME_INIT    100u       /* hardware volume at init (unchanged) */

/* Audio stored at 48 kHz — same rate as TDM, no upsampling needed */
#define LULLABY_UPSAMPLE     1u

/*────────────────────────────────────────────────────────────────────────────*/
typedef enum { BUZZER_IDLE = 0, BUZZER_TONE, BUZZER_PCM, BUZZER_STREAM } buzzer_mode_t;

/* Ring buffer for stream mode — fed by decoder task, read by ISR */
#define STREAM_BUF_SAMPLES  8192u                       /* ~170 ms at 48 kHz */
#define STREAM_BUF_MASK     (STREAM_BUF_SAMPLES - 1u)  /* power-of-2 mask   */
static int16_t           s_stream_buf[STREAM_BUF_SAMPLES];
static volatile uint32_t s_stream_rd = 0u;             /* ISR reader index  */
static volatile uint32_t s_stream_wr = 0u;             /* task writer index */

/*────────────────────────────────────────────────────────────────────────────*/
/* Codec */
static cy_stc_scb_i2c_context_t  s_i2c_ctx;
static mtb_hal_i2c_t             s_hal_i2c;
static bool                      s_codec_ok = false;

/* Tone buffer */
static int16_t   s_tone_buf[TONE_BUF_PAIRS * 2u];
static uint32_t  s_buf_pos;

/* PCM stream state */
static const int16_t *volatile s_pcm_data    = NULL;
static volatile uint32_t       s_pcm_len     = 0u;
static volatile uint32_t       s_pcm_pos     = 0u;
static volatile bool           s_pcm_loop    = false;
static volatile uint8_t        s_upsample_ph = 0u;
static volatile int16_t        s_pcm_cur     = 0;

/* Common playback state */
static volatile buzzer_mode_t  s_mode      = BUZZER_IDLE;
static volatile bool           s_active    = false;
static volatile TickType_t     s_stop_tick = 0u;

/* Volume / mute */
static volatile int32_t s_vol_scale = 128;  /* 128 = 100%, applied as (s * scale) >> 7 */
static volatile bool    s_muted     = false;

static const cy_stc_sysint_t k_tdm_irq_cfg =
{
    .intrSrc      = CYBSP_TDM_CONTROLLER_0_TX_IRQ,
    .intrPriority = 5u,
};

/*────────────────────────────────────────────────────────────────────────────*/
static void codec_init(void)
{
    cy_en_scb_i2c_status_t i2c_st = Cy_SCB_I2C_Init(CYBSP_I2C_CONTROLLER_HW,
                                                      &CYBSP_I2C_CONTROLLER_config,
                                                      &s_i2c_ctx);
    if (i2c_st != CY_SCB_I2C_SUCCESS)
    {
        printf("[Buzzer] SCB0 I2C init failed (%d)\r\n", (int)i2c_st);
        return;
    }
    Cy_SCB_I2C_Enable(CYBSP_I2C_CONTROLLER_HW);

    cy_rslt_t r = mtb_hal_i2c_setup(&s_hal_i2c,
                                     &CYBSP_I2C_CONTROLLER_hal_config,
                                     &s_i2c_ctx, NULL);
    if (r != CY_RSLT_SUCCESS) { printf("[Buzzer] HAL I2C setup failed\r\n"); return; }

    r = mtb_tlv320dac3100_init(&s_hal_i2c);
    if (r != CY_RSLT_SUCCESS) { printf("[Buzzer] TLV320 init failed\r\n"); return; }

    r = mtb_tlv320dac3100_configure_clocking(CODEC_MCLK_HZ,
                                              TLV320DAC3100_DAC_SAMPLE_RATE_48_KHZ,
                                              TLV320DAC3100_I2S_WORD_SIZE_16,
                                              TLV320DAC3100_SPK_AUDIO_OUTPUT);
    if (r != CY_RSLT_SUCCESS) { printf("[Buzzer] TLV320 clock config failed\r\n"); return; }

    r = mtb_tlv320dac3100_adjust_speaker_output_volume(CODEC_VOLUME_INIT);
    if (r != CY_RSLT_SUCCESS) { printf("[Buzzer] TLV320 volume set failed\r\n"); return; }

    r = mtb_tlv320dac3100_activate();
    if (r != CY_RSLT_SUCCESS) { printf("[Buzzer] TLV320 activate failed\r\n"); return; }

    s_codec_ok = true;
    printf("[Buzzer] TLV320DAC3100 ready\r\n");
    fflush(stdout);
}

/*────────────────────────────────────────────────────────────────────────────*/
static void build_tone(uint32_t freq_hz)
{
    uint32_t inc   = (freq_hz << 16u) / BUZZER_SAMPLE_RATE;
    uint32_t phase = 0u;
    for (uint32_t i = 0u; i < TONE_BUF_PAIRS; i++)
    {
        int16_t s = (phase < 0x8000u) ? (int16_t)BUZZER_AMPLITUDE
                                      : (int16_t)(-BUZZER_AMPLITUDE);
        s_tone_buf[i * 2u]      = s;
        s_tone_buf[i * 2u + 1u] = s;
        phase = (phase + inc) & 0xFFFFu;
    }
}

/*────────────────────────────────────────────────────────────────────────────*/
/* Apply volume scale and mute — used in ISR only */
static inline int16_t apply_vol(int16_t raw)
{
    if (s_muted) return 0;
    return (int16_t)(((int32_t)raw * s_vol_scale) >> 7);
}

/*────────────────────────────────────────────────────────────────────────────*/
static void refill_fifo(uint32_t pairs)
{
    for (uint32_t i = 0u; i < pairs; i++)
    {
        int16_t samp;

        if (s_mode == BUZZER_STREAM)
        {
            if ((s_stream_wr - s_stream_rd) > 0u)
            {
                samp = apply_vol(s_stream_buf[s_stream_rd & STREAM_BUF_MASK]);
                s_stream_rd++;
            }
            else
            {
                samp = 0;  /* underrun — output silence */
            }
        }
        else if (s_mode == BUZZER_PCM)
        {
            /* Fetch new source sample every LULLABY_UPSAMPLE output frames */
            if (s_upsample_ph == 0u)
            {
                if (s_pcm_pos < s_pcm_len)
                {
                    s_pcm_cur = s_pcm_data[s_pcm_pos++];
                }
                else if (s_pcm_loop)
                {
                    s_pcm_pos = 0u;
                    s_pcm_cur = (s_pcm_len > 0u) ? s_pcm_data[s_pcm_pos++] : 0;
                }
                else
                {
                    s_active = false;
                    /* Output silence for the remainder of this refill */
                    for (; i < pairs; i++)
                    {
                        Cy_AudioTDM_WriteTxData(CYBSP_TDM_CONTROLLER_0_TX_HW, 0u);
                        Cy_AudioTDM_WriteTxData(CYBSP_TDM_CONTROLLER_0_TX_HW, 0u);
                    }
                    return;
                }
            }
            s_upsample_ph = (uint8_t)((s_upsample_ph + 1u) % LULLABY_UPSAMPLE);
            samp = apply_vol(s_pcm_cur);
        }
        else  /* BUZZER_TONE */
        {
            uint32_t pos = s_buf_pos;
            s_buf_pos = (pos + 1u >= TONE_BUF_PAIRS) ? 0u : pos + 1u;
            samp = apply_vol(s_tone_buf[pos * 2u]);
        }

        Cy_AudioTDM_WriteTxData(CYBSP_TDM_CONTROLLER_0_TX_HW, (uint32_t)(uint16_t)samp);
        Cy_AudioTDM_WriteTxData(CYBSP_TDM_CONTROLLER_0_TX_HW, (uint32_t)(uint16_t)samp);
    }
}

/*────────────────────────────────────────────────────────────────────────────*/
static void buzzer_isr(void)
{
    uint32_t masked = Cy_AudioTDM_GetTxInterruptStatusMasked(
                          CYBSP_TDM_CONTROLLER_0_TX_HW);
    Cy_AudioTDM_ClearTxInterrupt(CYBSP_TDM_CONTROLLER_0_TX_HW, masked);

    /* Idle — keep TDM/codec BCLK alive by streaming silence.
     * Never call DeActivateTx: the codec loses BCLK sync and subsequent
     * activations produce no audio. */
    if (!s_active || s_mode == BUZZER_IDLE)
    {
        for (uint32_t i = 0u; i < FIFO_REFILL_ENTRIES / 2u; i++)
        {
            Cy_AudioTDM_WriteTxData(CYBSP_TDM_CONTROLLER_0_TX_HW, 0u);
            Cy_AudioTDM_WriteTxData(CYBSP_TDM_CONTROLLER_0_TX_HW, 0u);
        }
        return;
    }

    /* Tone mode: stop when duration expires */
    if (s_mode == BUZZER_TONE &&
        xTaskGetTickCountFromISR() >= s_stop_tick)
    {
        s_active = false;
        s_mode   = BUZZER_IDLE;
        for (uint32_t i = 0u; i < FIFO_REFILL_ENTRIES / 2u; i++)
        {
            Cy_AudioTDM_WriteTxData(CYBSP_TDM_CONTROLLER_0_TX_HW, 0u);
            Cy_AudioTDM_WriteTxData(CYBSP_TDM_CONTROLLER_0_TX_HW, 0u);
        }
        return;
    }

    refill_fifo(FIFO_REFILL_ENTRIES / 2u);

    /* PCM ended (non-looping) — switch to IDLE so next ISR outputs silence */
    if (!s_active)
    {
        s_mode = BUZZER_IDLE;
    }
}

/*────────────────────────────────────────────────────────────────────────────*/
void buzzer_init(void)
{
    Cy_SysClk_PeriGroupSlaveInit(CY_MMIO_TDM0_PERI_NR,
                                 CY_MMIO_TDM0_GROUP_NR,
                                 CY_MMIO_TDM0_SLAVE_NR,
                                 CY_MMIO_TDM0_CLK_HF_NR);

    (void)Cy_AudioTDM_Init(CYBSP_TDM_CONTROLLER_0_HW,
                           &CYBSP_TDM_CONTROLLER_0_config);
    Cy_AudioTDM_EnableTx(CYBSP_TDM_CONTROLLER_0_TX_HW);
    (void)Cy_SysInt_Init(&k_tdm_irq_cfg, buzzer_isr);
    NVIC_EnableIRQ(CYBSP_TDM_CONTROLLER_0_TX_IRQ);

    codec_init();

    /* Pre-fill FIFO with silence, then activate TDM once and leave it running.
     * Keeping BCLK alive prevents the codec from losing I2S synchronisation
     * between audio sessions — the root cause of PCM playback being silent
     * after the first (tone) playback session. */
    for (uint32_t i = 0u; i < 64u; i++)
    {
        Cy_AudioTDM_WriteTxData(CYBSP_TDM_CONTROLLER_0_TX_HW, 0u);
        Cy_AudioTDM_WriteTxData(CYBSP_TDM_CONTROLLER_0_TX_HW, 0u);
    }
    Cy_AudioTDM_SetTxTriggerInterruptMask(CYBSP_TDM_CONTROLLER_0_TX_HW);
    Cy_AudioTDM_ActivateTx(CYBSP_TDM_CONTROLLER_0_TX_HW);
}

/*────────────────────────────────────────────────────────────────────────────*/
void buzzer_beep(uint32_t freq_hz, uint32_t duration_ms)
{
    if (freq_hz == 0u) { buzzer_stop(); return; }

    /* Pause ISR output by clearing active first, then update state.
     * TDM stays running — ISR outputs silence until s_active is set. */
    s_active = false;
    s_mode   = BUZZER_IDLE;

    build_tone(freq_hz);
    s_buf_pos   = 0u;
    s_stop_tick = xTaskGetTickCount() + pdMS_TO_TICKS(duration_ms);
    s_mode      = BUZZER_TONE;
    s_active    = true;
}

/*────────────────────────────────────────────────────────────────────────────*/
void buzzer_play_pcm(const int16_t *data, uint32_t len_samples, bool loop)
{
    if (data == NULL || len_samples == 0u) return;
    printf("[Buzzer] play_pcm data=%p len=%lu loop=%d\r\n",
           (const void *)data, (unsigned long)len_samples, (int)loop);
    fflush(stdout);

    /* Put ISR on silence path before touching shared state */
    s_active = false;
    s_mode   = BUZZER_IDLE;

    s_pcm_data    = data;
    s_pcm_len     = len_samples;
    s_pcm_pos     = 0u;
    s_pcm_loop    = loop;
    s_upsample_ph = 0u;
    s_pcm_cur     = 0;

    /* Switch ISR to PCM output */
    s_mode   = BUZZER_PCM;
    s_active = true;
}

/*────────────────────────────────────────────────────────────────────────────*/
void buzzer_stop(void)
{
    s_active = false;
    s_mode   = BUZZER_IDLE;   /* ISR outputs silence; TDM clock stays alive */
}

/*────────────────────────────────────────────────────────────────────────────*/
void buzzer_set_volume(uint8_t vol_pct)
{
    if (vol_pct > 100u) vol_pct = 100u;
    /* Map 0-100 → 0-128 for ISR scale (applied as sample * scale >> 7) */
    s_vol_scale = (int32_t)((uint32_t)vol_pct * 128u / 100u);
}

uint8_t buzzer_get_volume(void)
{
    return (uint8_t)((uint32_t)s_vol_scale * 100u / 128u);
}

void buzzer_set_muted(bool mute)
{
    s_muted = mute;
}

bool buzzer_is_muted(void)
{
    return s_muted;
}

bool buzzer_is_playing(void)
{
    return s_active;
}

/*────────────────────────────────────────────────────────────────────────────*/
void buzzer_stream_start(void)
{
    s_active = false;
    s_mode   = BUZZER_IDLE;
    s_stream_rd = 0u;
    s_stream_wr = 0u;
    s_mode   = BUZZER_STREAM;
    s_active = true;
}

uint32_t buzzer_stream_writable(void)
{
    return STREAM_BUF_SAMPLES - (s_stream_wr - s_stream_rd);
}

void buzzer_stream_write(const int16_t *data, uint32_t n)
{
    for (uint32_t i = 0u; i < n; i++)
    {
        s_stream_buf[s_stream_wr & STREAM_BUF_MASK] = data[i];
        s_stream_wr++;
    }
}

uint8_t buzzer_get_fifo_level(void)
{
    return Cy_AudioTDM_GetNumInTxFifo(CYBSP_TDM_CONTROLLER_0_TX_HW);
}
