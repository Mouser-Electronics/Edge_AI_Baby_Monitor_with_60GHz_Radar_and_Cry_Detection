/*******************************************************************************
* File Name: cry_detector.c
*
* Description: Baby cry detection using PDM microphone (channel 3) and the
*              DEEPCRAFT IMAI_AED babycry model. Feeds 16-bit PCM samples
*              normalised to [-1, 1] into the model one sample at a time and
*              exposes cry_detector_get_state() for the UI.
*******************************************************************************/

#include "cry_detector.h"
#include "cy_pdl.h"
#include "cybsp.h"
#include "ready_models/babycry_lib.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

/*******************************************************************************
* Constants
*******************************************************************************/
/* Sensitivity level → IMAI confidence threshold mapping (index 1-5) */
static const float k_conf_table[6] = { 0.0f, 0.80f, 0.65f, 0.50f, 0.35f, 0.20f };
static volatile float s_confidence  = 0.50f;  /* default = level 3 */
static volatile bool  s_sens_dirty  = false;   /* flag for task to re-apply */

#define PDM_CHANNEL             (3u)
#define FRAME_SIZE              (1024u)
#define SAMPLE_RATE_HZ          (16000u)
#define HW_FIFO_SIZE            (64u)
#define RX_FIFO_TRIG_LEVEL      (HW_FIFO_SIZE / 2u)
#define NUM_IRQS_PER_FRAME      (FRAME_SIZE / RX_FIFO_TRIG_LEVEL)
#define AUDIO_BITS              (16)
#define DIGITAL_BOOST           (1.0f)
/* Hold is_crying true for this many ms after the last detected event */
#define CRY_HOLD_MS             (2000u)
/* Print a heartbeat to the serial terminal every this many frames (~5 sec) */
#define DIAG_EVERY_FRAMES       (78u)

#define SAMPLE_NORMALIZE(s)  (((float)(s)) / (float)(1 << (AUDIO_BITS - 1)))

/*******************************************************************************
* PDM IRQ config
*******************************************************************************/
static const cy_stc_sysint_t s_pdm_irq_cfg = {
    .intrSrc      = (IRQn_Type)CYBSP_PDM_CHANNEL_3_IRQ,
    .intrPriority = 2u
};

/*******************************************************************************
* Ping-pong buffers + ISR state
*******************************************************************************/
static int16_t  s_buf0[FRAME_SIZE];
static int16_t  s_buf1[FRAME_SIZE];
static int16_t *s_active_buf;   /* ISR fills this */
static int16_t *s_full_buf;     /* task processes this */

static TaskHandle_t s_task_handle = NULL;

/*******************************************************************************
* Shared cry state (written by cry task, read by UI timer callback)
*******************************************************************************/
static volatile cry_state_t s_state = {
    .is_crying      = false,
    .cry_count      = 0u,
    .model_expired  = false,
    .hw_error       = false,
};

/*******************************************************************************
* PDM ISR — ping-pong fill + task notification
*******************************************************************************/
static void pdm_isr(void)
{
    static uint16_t frame_counter = 0u;
    BaseType_t higher_prio_woken  = pdFALSE;

    uint32_t status = Cy_PDM_PCM_Channel_GetInterruptStatusMasked(CYBSP_PDM_HW,
                                                                    PDM_CHANNEL);
    if (CY_PDM_PCM_INTR_RX_TRIGGER & status)
    {
        for (uint32_t i = 0u; i < RX_FIFO_TRIG_LEVEL; i++)
        {
            int32_t raw = (int32_t)Cy_PDM_PCM_Channel_ReadFifo(CYBSP_PDM_HW,
                                                                PDM_CHANNEL);
            s_active_buf[frame_counter * RX_FIFO_TRIG_LEVEL + i] = (int16_t)raw;
        }
        Cy_PDM_PCM_Channel_ClearInterrupt(CYBSP_PDM_HW, PDM_CHANNEL,
                                           CY_PDM_PCM_INTR_RX_TRIGGER);
        frame_counter++;
    }

    if (frame_counter >= NUM_IRQS_PER_FRAME)
    {
        int16_t *tmp  = s_active_buf;
        s_active_buf  = s_full_buf;
        s_full_buf    = tmp;
        frame_counter = 0u;

        vTaskNotifyGiveFromISR(s_task_handle, &higher_prio_woken);
        portYIELD_FROM_ISR(higher_prio_woken);
    }

    if ((CY_PDM_PCM_INTR_RX_FIR_OVERFLOW | CY_PDM_PCM_INTR_RX_OVERFLOW |
         CY_PDM_PCM_INTR_RX_IF_OVERFLOW  | CY_PDM_PCM_INTR_RX_UNDERFLOW) & status)
    {
        Cy_PDM_PCM_Channel_ClearInterrupt(CYBSP_PDM_HW, PDM_CHANNEL,
                                           CY_PDM_PCM_INTR_MASK);
    }
}

/*******************************************************************************
* PDM init
*******************************************************************************/
static bool pdm_init(void)
{
    memset(s_buf0, 0, sizeof(s_buf0));
    memset(s_buf1, 0, sizeof(s_buf1));
    s_active_buf = s_buf0;
    s_full_buf   = s_buf1;

    cy_en_pdm_pcm_status_t st = Cy_PDM_PCM_Init(CYBSP_PDM_HW, &CYBSP_PDM_config);
    if (CY_PDM_PCM_SUCCESS != st)
    {
        printf("[CRY] PDM init failed: %d\r\n", (int)st);
        return false;
    }

    Cy_PDM_PCM_Channel_Enable(CYBSP_PDM_HW, PDM_CHANNEL);
    Cy_PDM_PCM_Channel_Init(CYBSP_PDM_HW, &channel_3_config, PDM_CHANNEL);
    Cy_PDM_PCM_SetGain(CYBSP_PDM_HW, PDM_CHANNEL, CY_PDM_PCM_SEL_GAIN_5DB);

    Cy_PDM_PCM_Channel_ClearInterrupt(CYBSP_PDM_HW, PDM_CHANNEL,
                                       CY_PDM_PCM_INTR_MASK);
    Cy_PDM_PCM_Channel_SetInterruptMask(CYBSP_PDM_HW, PDM_CHANNEL,
                                         CY_PDM_PCM_INTR_MASK);

    cy_en_sysint_status_t irq_st = Cy_SysInt_Init(&s_pdm_irq_cfg, pdm_isr);
    if (CY_SYSINT_SUCCESS != irq_st)
    {
        printf("[CRY] PDM IRQ init failed: %d\r\n", (int)irq_st);
        return false;
    }
    NVIC_ClearPendingIRQ(s_pdm_irq_cfg.intrSrc);
    NVIC_EnableIRQ(s_pdm_irq_cfg.intrSrc);

    Cy_PDM_PCM_Activate_Channel(CYBSP_PDM_HW, PDM_CHANNEL);
    return true;
}

/*******************************************************************************
* cry_detector_task
*******************************************************************************/
void cry_detector_task(void *arg)
{
    (void)arg;
    s_task_handle = xTaskGetCurrentTaskHandle();

    IMAI_AED_init();

    /* Lower detection threshold from the conservative default so the model
     * triggers on typical microphone capture of baby cry sounds.
     * confidence=0.5 : fire when model is ≥50% sure (was ~0.9 default)
     * subsequent=1   : one above-threshold window is enough
     * average/pool   : minimal smoothing */
    {
        PP_config_t pp = { .confidence = s_confidence,
                           .average = 1, .subsequent = 1,
                           .pool = 1,   .pool_selection = 1 };
        IMAI_AED_sensitivity(pp);
    }

    if (!pdm_init())
    {
        s_state.hw_error = true;
        vTaskDelete(NULL);
        return;
    }

    printf("[CRY] task started — babycry model listening\r\n");
    fflush(stdout);

    TickType_t cry_hold_until   = 0u;
    uint32_t   frame_count      = 0u;
    uint32_t   prediction_count = 0u;  /* total IMAI_RET_SUCCESS returns */
    int16_t    peak_sample      = 0;   /* max |sample| across last diag window */

    for (;;)
    {
        /* Re-apply sensitivity if changed by UI */
        if (s_sens_dirty)
        {
            PP_config_t pp = { .confidence = s_confidence,
                               .average = 1, .subsequent = 1,
                               .pool = 1,   .pool_selection = 1 };
            IMAI_AED_sensitivity(pp);
            s_sens_dirty = false;
        }

        /* Wait for ISR to signal a full 1024-sample frame is ready */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        frame_count++;

        int label_scores[IMAI_DATA_OUT_COUNT] = {0};
        bool cry_this_frame = false;

        for (uint32_t i = 0u; i < FRAME_SIZE; i++)
        {
            int16_t raw = s_full_buf[i];

            /* Track peak amplitude for diagnostics */
            int16_t abs_raw = (raw < 0) ? (int16_t)(-raw) : raw;
            if (abs_raw > peak_sample) { peak_sample = abs_raw; }

            float sample = SAMPLE_NORMALIZE(raw) * DIGITAL_BOOST;
            if      (sample >  1.0f) { sample =  1.0f; }
            else if (sample < -1.0f) { sample = -1.0f; }

            IMAI_AED_enqueue(&sample);

            int ret = IMAI_AED_dequeue(label_scores);
            if (ret == IMAI_RET_SUCCESS)
            {
                prediction_count++;
                if (label_scores[1] == 1)
                {
                    cry_this_frame = true;
                    s_state.cry_count++;
                    cry_hold_until = xTaskGetTickCount() + pdMS_TO_TICKS(CRY_HOLD_MS);
                }
            }
            else if (ret == IMAI_RET_TIMEDOUT)
            {
                if (!s_state.model_expired)
                {
                    printf("[CRY] eval period ended\r\n");
                    fflush(stdout);
                }
                s_state.model_expired = true;
            }
        }

        /* Update crying state once per frame (after all samples are processed) */
        s_state.is_crying = (xTaskGetTickCount() < cry_hold_until);

        if (cry_this_frame)
        {
            printf("[CRY] *** BABY CRY *** count=%lu\r\n",
                   (unsigned long)s_state.cry_count);
            fflush(stdout);
        }

        /*
         * Heartbeat every ~5 s.  peak=N shows mic amplitude (0-32767):
         *   peak < 200  → mic not picking up sound (too quiet / wrong channel)
         *   peak ~1000+ → normal speech/cry level
         *   peak = 32767 → saturated (gain too high)
         */
        if ((frame_count % DIAG_EVERY_FRAMES) == 0u)
        {
            printf("[CRY] frames=%lu  predictions=%lu  cries=%lu  peak=%d\r\n",
                   (unsigned long)frame_count,
                   (unsigned long)prediction_count,
                   (unsigned long)s_state.cry_count,
                   (int)peak_sample);
            fflush(stdout);
            peak_sample = 0;   /* reset for next window */
        }
    }
}

/*******************************************************************************
* cry_detector_get_state — called from LVGL timer (non-ISR context)
*******************************************************************************/
cry_state_t cry_detector_get_state(void)
{
    cry_state_t snap;
    snap.is_crying     = s_state.is_crying;
    snap.cry_count     = s_state.cry_count;
    snap.model_expired = s_state.model_expired;
    snap.hw_error      = s_state.hw_error;
    return snap;
}

/*******************************************************************************
* cry_set_sensitivity — 1 = low (high confidence needed), 5 = high
*******************************************************************************/
void cry_set_sensitivity(uint8_t level)
{
    if (level < 1u) level = 1u;
    if (level > 5u) level = 5u;
    s_confidence = k_conf_table[level];
    s_sens_dirty = true;
}
