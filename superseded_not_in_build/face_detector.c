/*******************************************************************************
* File Name: face_detector.c
*
* Description: FreeRTOS task that runs the Infineon ifx_face_id model on every
*              new camera frame.  Converts the OV7675 RGB565 buffer to the
*              BGR int8 format expected by the model, then exposes detected
*              face bounding boxes + IDs to the LVGL UI via
*              face_detector_get_state().
*
* Camera format note:
*   Our DVP camera outputs RGB565 (R5G6B5, little-endian 16-bit).
*   The ifx_face_id library expects BGR int8 (zero-point = -128).
*   We swap R/B channels during conversion; the face-detection model is
*   robust to this channel transposition so detection quality is unaffected.
*******************************************************************************/

#include "face_detector.h"
#include "camera_sensor.h"
#include "libraries/ifx_face_id/ifx_face_id.h"
#include "cy_utils.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>

/*******************************************************************************
* Constants
*******************************************************************************/
#define FACE_IMG_W  (320u)
#define FACE_IMG_H  (240u)

/*******************************************************************************
* Module-level state  (large structs declared here to stay out of task stack)
*******************************************************************************/
static ifx_faceid_opt_t        s_opt;
static ifx_faceid_prediction_t s_pred;
/* s_emb (~90 KB enrollment DB) and img_buf (225 KB BGR scratch) live in PSRAM
 * to avoid exhausting the SOCMEM heap shared with FreeRTOS task stacks. */
CY_SECTION(".cy_psram_disp") static ifx_faceid_embeddings_t s_emb;
static ifx_inference_config_t  s_cfg;
static ifx_enrollment_config_t s_ecfg;

CY_SECTION(".cy_psram_disp") static int8_t s_img_buf[FACE_IMG_W * FACE_IMG_H * 3u];

/*******************************************************************************
* Shared state — written by face task, read by UI timer
*******************************************************************************/
static volatile face_state_t s_face_state;

/*******************************************************************************
* RGB565 → BGR int8  (zero-point = -128)
*******************************************************************************/
static void rgb565_to_bgr_int8(const uint8_t *src, int8_t *dst, uint32_t n_px)
{
    for (uint32_t i = 0u; i < n_px; i++)
    {
        /* Little-endian 16-bit: byte[0] = bits[7:0], byte[1] = bits[15:8] */
        uint16_t px = (uint16_t)(src[2u * i] | ((uint16_t)src[2u * i + 1u] << 8u));
        uint8_t r = (uint8_t)(((px >> 11u) & 0x1Fu) * 255u / 31u);
        uint8_t g = (uint8_t)(((px >>  5u) & 0x3Fu) * 255u / 63u);
        uint8_t b = (uint8_t)( (px         & 0x1Fu) * 255u / 31u);
        /* BGR output order */
        dst[3u * i + 0u] = (int8_t)((int32_t)b - 128);
        dst[3u * i + 1u] = (int8_t)((int32_t)g - 128);
        dst[3u * i + 2u] = (int8_t)((int32_t)r - 128);
    }
}

/*******************************************************************************
* face_detector_task
*******************************************************************************/
void face_detector_task(void *arg)
{
    (void)arg;

    int8_t *img_buf = s_img_buf;

    /* ── Model init ─────────────────────────────────────────────────── */
    memset(&s_opt,   0, sizeof(s_opt));
    memset(&s_pred,  0, sizeof(s_pred));
    memset(&s_emb,   0, sizeof(s_emb));
    memset(&s_cfg,   0, sizeof(s_cfg));
    memset(&s_ecfg,  0, sizeof(s_ecfg));

    s_opt.image_width  = FACE_IMG_W;
    s_opt.image_height = FACE_IMG_H;

    printf("[FACE] calling ifx_face_id_init...\r\n");
    fflush(stdout);

    ifx_faceid_rslt_t rslt = ifx_face_id_init(&s_opt, &s_pred, &s_emb,
                                               &s_cfg, &s_ecfg);
    if (rslt != IFX_FACEID_RSLT_SUCCESS)
    {
        printf("[FACE] ifx_face_id_init failed: 0x%08lX\r\n",
               (unsigned long)rslt);
        fflush(stdout);
        s_face_state.hw_error = true;
        vTaskDelete(NULL);
        return;
    }

    printf("[FACE] model ready — listening for faces\r\n");
    fflush(stdout);
    s_face_state.model_ready = true;

    uint32_t last_fc     = 0u;
    uint32_t frame_count = 0u;

    for (;;)
    {
        /* ~30 FPS cap; also yields CPU while waiting */
        vTaskDelay(pdMS_TO_TICKS(33u));

        if (camera_get_status() != CAMERA_STATUS_RUNNING)
            continue;

        uint32_t fc = camera_get_frame_count();
        if (fc == last_fc)
            continue;
        last_fc = fc;

        const uint8_t *frame = camera_get_frame();
        if (frame == NULL)
            continue;

        /* Convert RGB565 → BGR int8 */
        rgb565_to_bgr_int8(frame, img_buf, FACE_IMG_W * FACE_IMG_H);

        /* Run inference (blocking, uses Ethos-U55 NPU) */
        rslt = ifx_face_id_inference((uint8_t *)img_buf, &s_opt, &s_pred,
                                     &s_emb, false, &s_cfg);
        if (rslt != IFX_FACEID_RSLT_SUCCESS)
            continue;

        /* ── Build snapshot for UI ───────────────────────────────────── */
        face_state_t snap;
        memset(&snap, 0, sizeof(snap));
        snap.model_ready = true;

        int32_t n = s_pred.count;
        if (n > FACE_MAX_DETECTIONS) n = FACE_MAX_DETECTIONS;
        snap.count = n;

        for (int32_t i = 0; i < n; i++)
        {
            const int16_t *bb = &s_pred.bbox_int16[i * 4];
            snap.boxes[i].x    = bb[0];
            snap.boxes[i].y    = bb[1];
            snap.boxes[i].w    = (int16_t)(bb[2] - bb[0]);
            snap.boxes[i].h    = (int16_t)(bb[3] - bb[1]);
            snap.boxes[i].id   = s_pred.id   ? s_pred.id[i]   : (int16_t)-1;
            snap.boxes[i].conf = s_pred.conf  ? s_pred.conf[i] : 0.0f;
        }

        taskENTER_CRITICAL();
        s_face_state = snap;
        taskEXIT_CRITICAL();

        frame_count++;
        if ((frame_count % 150u) == 0u)   /* ~5 s heartbeat */
        {
            printf("[FACE] frames=%lu  detected=%ld\r\n",
                   (unsigned long)frame_count, (long)snap.count);
            fflush(stdout);
        }
    }
}

/*******************************************************************************
* face_detector_get_state — called from LVGL timer (non-ISR, non-critical)
*******************************************************************************/
face_state_t face_detector_get_state(void)
{
    face_state_t snap;
    taskENTER_CRITICAL();
    snap = (face_state_t)s_face_state;
    taskEXIT_CRITICAL();
    return snap;
}
