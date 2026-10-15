#ifndef CAMERA_SENSOR_H
#define CAMERA_SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

#define CAMERA_FRAME_W         (320U)
#define CAMERA_FRAME_H         (240U)
#define CAMERA_FRAME_SIZE      (CAMERA_FRAME_W * CAMERA_FRAME_H * 2U)  /* RGB565 — 150 KB */

#define CAMERA_TASK_NAME       "Camera Task"
#define CAMERA_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 10) /* USB enum + YUYV conversion */
#define CAMERA_TASK_PRIORITY   (2U)

typedef enum {
    CAMERA_STATUS_IDLE      = 0,
    CAMERA_STATUS_STARTING  = 1,
    CAMERA_STATUS_NOT_FOUND = 2,
    CAMERA_STATUS_ERROR     = 3,
    CAMERA_STATUS_RUNNING   = 4,
} camera_status_t;

void            camera_hw_init(void);   /* call before display I2C ISR is registered */
void            camera_task(void *arg); /* frame-monitor loop; spawn after hw_init */
const uint8_t  *camera_get_frame(void);
uint32_t        camera_get_frame_count(void); /* increments each time a new frame is ready */
camera_status_t camera_get_status(void);

#ifdef __cplusplus
}
#endif

#endif /* CAMERA_SENSOR_H */
