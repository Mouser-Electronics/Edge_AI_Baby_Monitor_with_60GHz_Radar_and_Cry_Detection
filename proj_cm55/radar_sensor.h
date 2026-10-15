/*******************************************************************************
* File Name: radar_sensor.h
*
* Description: Radar sensor driver interface for the BGT60TR13C.
*
*  METRICS
*  ───────
*  presence     — target detected in field of view
*  motion_pct   — frame-to-frame scene change 0-100 %
*  signal_pct   — peak reflection strength 0-100 %
*  trend        — +1 approaching / 0 stationary / -1 receding
*                 derived from signal strength change (reliable)
*******************************************************************************/

#ifndef RADAR_SENSOR_H
#define RADAR_SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define RADAR_TASK_NAME        "Radar Task"
#define RADAR_TASK_STACK_SIZE  (configMINIMAL_STACK_SIZE * 4)
#define RADAR_TASK_PRIORITY    (3U)

#define RADAR_RANGE_BINS       (24U)   /* usable bins in 1 m gate (skip 8, up to 32) */

typedef struct {
    bool     presence;                      /* Target in field of view (debounced, life-gated) */
    bool     breathing;                     /* Life-sign band active (breath or motion) */
    uint8_t  motion_pct;                    /* Motion intensity  0-100 %        */
    uint8_t  signal_pct;                    /* Reflection strength 0-100 %      */
    uint8_t  breath_pct;                    /* Breathing-band level 0-100 %     */
    int8_t   trend;                         /* +1 approaching / 0 / -1 receding */
    uint8_t  range_bins[RADAR_RANGE_BINS];  /* per-bin magnitude 0-100 %        */
} radar_data_t;

void          radar_task(void *arg);
radar_data_t  radar_get_latest(void);

#ifdef __cplusplus
}
#endif

#endif /* RADAR_SENSOR_H */
