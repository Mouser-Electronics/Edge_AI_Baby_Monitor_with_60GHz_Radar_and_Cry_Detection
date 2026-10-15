/*******************************************************************************
* File Name: cry_detector.h
*
* Description: Baby cry detection task using PDM microphone and
*              DEEPCRAFT/Imagimob babycry_lib_eval model.
*******************************************************************************/

#ifndef CRY_DETECTOR_H_
#define CRY_DETECTOR_H_

#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

#define CRY_TASK_NAME        "cry_task"
#define CRY_TASK_STACK_SIZE  (configMINIMAL_STACK_SIZE * 12)
#define CRY_TASK_PRIORITY    (configMAX_PRIORITIES - 2)

typedef struct {
    bool     is_crying;       /* true while crying or within 2-second hold */
    uint32_t cry_count;       /* total number of cry events detected */
    bool     model_expired;   /* IMAI eval period ended */
    bool     hw_error;        /* PDM init failed */
} cry_state_t;

void        cry_detector_task(void *arg);
cry_state_t cry_detector_get_state(void);

/* Adjust model confidence threshold at runtime.
 * level 1 = low sensitivity (fires only when very sure)
 * level 3 = default (0.50 confidence)
 * level 5 = high sensitivity (fires more readily)            */
void        cry_set_sensitivity(uint8_t level);

#endif /* CRY_DETECTOR_H_ */
