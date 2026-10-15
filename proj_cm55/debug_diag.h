#ifndef DEBUG_DIAG_H
#define DEBUG_DIAG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"
#include "task.h"

#define WATCHDOG_TASK_NAME       "WDog"
#define WATCHDOG_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 4)
#define WATCHDOG_TASK_PRIORITY   (1U)   /* just above idle */

void watchdog_task(void *arg);

#ifdef __cplusplus
}
#endif
#endif /* DEBUG_DIAG_H */
