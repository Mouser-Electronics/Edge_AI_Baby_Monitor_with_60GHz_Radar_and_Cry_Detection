/*******************************************************************************
* File Name: sys_monitor.c
*
* Description: System resource monitor using FreeRTOS heap and tick APIs.
*******************************************************************************/

#include "sys_monitor.h"
#include "FreeRTOS.h"
#include "task.h"
#include <malloc.h>

sys_monitor_t sys_monitor_get(void)
{
    struct mallinfo mi = mallinfo();

    sys_monitor_t s;
    s.heap_free_kb = (uint32_t)((unsigned int)mi.fordblks / 1024U);
    s.heap_used_kb = (uint32_t)((unsigned int)mi.uordblks / 1024U);
    s.uptime_sec   = (uint32_t)(xTaskGetTickCount() / (TickType_t)configTICK_RATE_HZ);
    s.task_count   = (uint16_t)uxTaskGetNumberOfTasks();
    return s;
}
