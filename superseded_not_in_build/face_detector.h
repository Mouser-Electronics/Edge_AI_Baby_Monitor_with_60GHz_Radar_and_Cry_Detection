/*******************************************************************************
* File Name: face_detector.h
*
* Description: Real-time face detection using the Infineon ifx_face_id library.
*              Runs on the CM55 core, reads camera frames from camera_sensor,
*              and exposes bounding-box results to the LVGL UI.
*******************************************************************************/

#ifndef FACE_DETECTOR_H_
#define FACE_DETECTOR_H_

#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

#define FACE_TASK_NAME        "face_task"
#define FACE_TASK_STACK_SIZE  (configMINIMAL_STACK_SIZE * 48)
#define FACE_TASK_PRIORITY    (configMAX_PRIORITIES - 3)

/* Maximum bounding boxes reported to the UI (caps IFX_FACEID_MAX_DETECTIONS=20) */
#define FACE_MAX_DETECTIONS   5

/* Bounding box in 320×240 camera-pixel space */
typedef struct {
    int16_t x;      /* x_min */
    int16_t y;      /* y_min */
    int16_t w;      /* width  (x_max - x_min) */
    int16_t h;      /* height (y_max - y_min) */
    int16_t id;     /* -1 = unknown, >=0 = enrolled user index */
    float   conf;   /* detection confidence [0,1] */
} face_box_t;

typedef struct {
    int32_t    count;
    face_box_t boxes[FACE_MAX_DETECTIONS];
    bool       hw_error;     /* init failed */
    bool       model_ready;  /* init succeeded */
} face_state_t;

void         face_detector_task(void *arg);
face_state_t face_detector_get_state(void);

#endif /* FACE_DETECTOR_H_ */
