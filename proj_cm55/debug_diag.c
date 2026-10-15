#include "debug_diag.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

/* ── FreeRTOS hooks ─────────────────────────────────────────────────────── */

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    printf("\r\n[FATAL] STACK OVERFLOW in task: %s\r\n", pcTaskName);
    fflush(stdout);
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}

void vApplicationMallocFailedHook(void)
{
    printf("\r\n[FATAL] MALLOC FAILED — heap exhausted\r\n");
    fflush(stdout);
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}

/* ── Watchdog task ──────────────────────────────────────────────────────── */
/* Runs at priority 1 (just above idle).  Prints a heartbeat every second.
   If the heartbeat STOPS printing, something higher-priority is spinning
   (ISR loop or high-priority task deadlock).
   If ONLY the watchdog keeps printing but the GFX-task traces stop, the
   GFX task is blocked/deadlocked. */
void watchdog_task(void *arg)
{
    (void)arg;
    uint32_t tick = 0;
    for (;;)
    {
        printf("[WDog] alive tick=%lu\r\n", (unsigned long)tick++);
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
