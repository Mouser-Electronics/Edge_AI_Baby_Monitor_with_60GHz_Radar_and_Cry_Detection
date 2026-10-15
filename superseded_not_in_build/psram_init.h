/*******************************************************************************
* File Name: psram_init.h
*
* Description: PSRAM (S70KS1283, SMIF1) initialisation for XIP mode.
*              Call psram_init() once from main() before vTaskStartScheduler().
*******************************************************************************/

#ifndef PSRAM_INIT_H
#define PSRAM_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

void psram_init(void);

#ifdef __cplusplus
}
#endif

#endif /* PSRAM_INIT_H */
