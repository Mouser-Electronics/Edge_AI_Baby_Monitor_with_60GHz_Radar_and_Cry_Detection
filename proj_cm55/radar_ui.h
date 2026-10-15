/*******************************************************************************
* File Name: radar_ui.h
*
* Description: LVGL-based radar sensor dashboard UI for PSoC Edge E84 AI Kit.
*******************************************************************************/

#ifndef RADAR_UI_H
#define RADAR_UI_H

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
* Function Prototypes
*******************************************************************************/

/* Call once after lv_init() and lv_port_disp_init() to build the screen. */
void radar_ui_create(void);

#ifdef __cplusplus
}
#endif

#endif /* RADAR_UI_H */
