/*******************************************************************************
* File Name: sys_monitor.h
*
* Description: Lightweight system resource snapshot using FreeRTOS APIs.
*              No runtime-stats config changes required.
*******************************************************************************/

#ifndef SYS_MONITOR_H
#define SYS_MONITOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
    uint32_t heap_free_kb;   /* Current free heap in KB  (from mallinfo)      */
    uint32_t heap_used_kb;   /* Currently allocated heap in KB                */
    uint32_t uptime_sec;     /* Seconds since boot                            */
    uint16_t task_count;     /* Number of active FreeRTOS tasks               */
} sys_monitor_t;

sys_monitor_t sys_monitor_get(void);

#ifdef __cplusplus
}
#endif

#endif /* SYS_MONITOR_H */
