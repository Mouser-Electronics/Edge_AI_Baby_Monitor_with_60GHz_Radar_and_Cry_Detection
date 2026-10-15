/*******************************************************************************
* File Name: radar_ui.c
*
* Description: LVGL dashboard for the BGT60TR13C radar sensor.
*
*  SCREEN LAYOUT  (800 x 480)
*  ─────────────────────────────────────────────────────────
*
*   ┌──────────────────────────────────── [PRESENCE BADGE] ┐  46px
*   │  PSoC Edge E84 — 60 GHz Radar Sensor                 │
*   ├──────────────────────┬────────────────────────────────┤
*   │   PRESENCE           │   MOTION INTENSITY             │ 196px
*   │   (circle indicator) │   (arc gauge)                  │
*   ├──────────────────────┼────────────────────────────────┤
*   │   DIRECTION          │   SIGNAL STRENGTH OVER TIME    │ 156px
*   │   (arrow + label)    │   (live chart)                 │
*   ├──────────────────────┴────────────────────────────────┤
*   │  [Mouser Electronics]                                 │  44px
*   └───────────────────────────────────────────────────────┘
*******************************************************************************/

#include "radar_ui.h"
#include "radar_sensor.h"
#include "camera_sensor.h"
#include "cry_detector.h"
#include "buzzer.h"
#include "lullaby_player.h"
#include "sys_monitor.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

/*******************************************************************************
* Colours
*******************************************************************************/
#define COLOR_BG        lv_color_hex(0x0D1B2A)
#define COLOR_PANEL     lv_color_hex(0x1B2838)
#define COLOR_ACCENT    lv_color_hex(0x00D4FF)
#define COLOR_GREEN     lv_color_hex(0x00FF88)
#define COLOR_ORANGE    lv_color_hex(0xFF9900)
#define COLOR_GRAY      lv_color_hex(0x3A4A5A)
#define COLOR_TEXT_PRI  lv_color_hex(0xFFFFFF)
#define COLOR_TEXT_SEC  lv_color_hex(0xAABBCC)
#define COLOR_BAR_BG    lv_color_hex(0x263545)
#define COLOR_MOUSER    lv_color_hex(0x0057A8)

/*******************************************************************************
* Layout
*******************************************************************************/
/* Screen tabs: 32px bar at bottom → content = 800×448 */
#define TAB_BAR_H   32
#define GAP          6
#define LEFT_W       370
#define RIGHT_W      (800 - LEFT_W - GAP * 3)
#define TITLE_H      42
#define TOP_H        184
#define BOT_H        148
#define TOP_Y        (TITLE_H + GAP)
#define BOT_Y        (TOP_Y + TOP_H + GAP)
#define RIGHT_X      (GAP + LEFT_W + GAP)
#define CHART_POINTS 40
/* Camera tab: 597×448 keeps 4:3, centred in 800×448 content */
#define CAM_SCALE       478   /* 1.867× in LVGL 1/256 units */
/* Baby tab: camera on the left, 400×300 (1.25×), 320/256 LVGL units */
#define BABY_CAM_SCALE  320
/* Safety tab: camera 480×360 (1.5×), 384/256 LVGL units — kept large */
#define SF_CAM_SCALE    384
/* Baby tab right-panel geometry */
#define BABY_RIGHT_X    (GAP + 400 + GAP)             /* 412 */
#define BABY_RIGHT_W    (800 - BABY_RIGHT_X - GAP)    /* 382 */
#define BABY_PANEL_H    ((448 - GAP * 4) / 3)         /* ~141, 3 panels */
/* Safety Monitor tab geometry */
#define SF_HDR_H        36
#define SF_CAM_Y        (SF_HDR_H + GAP)               /* 42  */
#define SF_CAM_H        (CAMERA_FRAME_H * SF_CAM_SCALE / 256)   /* 360 */
#define SF_CAM_W        (CAMERA_FRAME_W * SF_CAM_SCALE / 256)   /* 480 */
#define SF_RIGHT_X      (GAP + SF_CAM_W + GAP)                  /* 492 */
#define SF_RIGHT_W      (800 - SF_RIGHT_X - GAP)                /* 302 */
#define SF_CHIP_H       50
#define SF_YELLOW_MS    60000u
/* Safety cam stays visible this long after the last cry / breathing alert */
#define SF_CAM_HOLD_MS  10000u

typedef enum { SF_GREEN = 0, SF_YELLOW, SF_RED } sf_state_t;

/*******************************************************************************
* Widget handles
*******************************************************************************/
static lv_obj_t *s_presence_circle = NULL;
static lv_obj_t *s_presence_lbl    = NULL;
static lv_obj_t *s_presence_sub    = NULL;

static lv_obj_t *s_arc_motion      = NULL;
static lv_obj_t *s_lbl_motion      = NULL;

static lv_obj_t *s_lbl_direction   = NULL;
static lv_obj_t *s_lbl_dir_arrow   = NULL;

static lv_obj_t *s_lbl_heap_free   = NULL;
static lv_obj_t *s_lbl_heap_min    = NULL;
static lv_obj_t *s_lbl_uptime      = NULL;
static lv_obj_t *s_lbl_tasks       = NULL;

static lv_obj_t *s_chart              = NULL;
static lv_chart_series_t *s_series    = NULL;

static lv_obj_t *s_bar_chart          = NULL;
static lv_chart_series_t *s_bar_series = NULL;

static lv_obj_t *s_badge           = NULL;
static lv_obj_t *s_badge_lbl       = NULL;

/* Camera tab */
static lv_obj_t       *s_cam_img      = NULL;   /* created lazily on first frame */
static lv_obj_t       *s_cam_wait_lbl = NULL;
static lv_obj_t       *s_tab_camera   = NULL;
static bool            s_cam_started  = false;

/* Baby monitor tab */
static lv_obj_t       *s_tab_baby         = NULL;
static lv_obj_t       *s_baby_cam_img     = NULL;   /* lazy, same s_cam_dsc source */
static lv_obj_t       *s_baby_cam_wait    = NULL;
static lv_obj_t       *s_cry_circle       = NULL;
static lv_obj_t       *s_cry_status_lbl   = NULL;
static lv_obj_t       *s_cry_count_lbl    = NULL;
static lv_obj_t       *s_baby_arc_motion  = NULL;
static lv_obj_t       *s_baby_lbl_motion  = NULL;

/* Safety monitor tab */
static lv_obj_t          *s_tab_safety    = NULL;
static lv_obj_t          *s_sf_header_bg  = NULL;
static lv_obj_t          *s_sf_state_lbl  = NULL;
static lv_obj_t          *s_sf_uptime_lbl = NULL;
static lv_obj_t          *s_sf_cam_img    = NULL;
static lv_obj_t          *s_sf_cam_wait   = NULL;
static lv_obj_t          *s_sf_breath_dot = NULL;
static lv_obj_t          *s_sf_breath_val = NULL;
static lv_obj_t          *s_sf_crib_dot   = NULL;
static lv_obj_t          *s_sf_crib_val   = NULL;
static lv_obj_t          *s_sf_cry_dot    = NULL;
static lv_obj_t          *s_sf_cry_val    = NULL;
static lv_obj_t          *s_sf_chart      = NULL;
static lv_chart_series_t *s_sf_ser        = NULL;
/* Safety tab playback controls */
static lv_obj_t          *s_sf_play_lbl   = NULL;
static lv_obj_t          *s_sf_mute_lbl   = NULL;
/* Safety cam privacy gate: hidden unless alarm (cry / no breathing) or forced */
static lv_obj_t          *s_sf_cam_btn_lbl  = NULL;
static lv_obj_t          *s_sf_cam_off_lbl  = NULL;
static bool               s_sf_cam_forced   = false;
static TickType_t         s_sf_cam_show_until = 0;
static sf_state_t         s_sf_state        = SF_GREEN;
static TickType_t         s_sf_yellow_until = 0;
static TickType_t         s_sf_crib_entry   = 0;
static bool               s_sf_had_presence = false;

/* Baby Mon settings widgets */
static lv_obj_t          *s_cry_sens_slider  = NULL;
static lv_obj_t          *s_cry_sens_val     = NULL;
static lv_obj_t          *s_radar_thr_slider = NULL;
static lv_obj_t          *s_radar_thr_val    = NULL;
static lv_obj_t          *s_vol_slider       = NULL;
static lv_obj_t          *s_vol_val          = NULL;
static lv_obj_t          *s_lullaby_status   = NULL;
static lv_obj_t          *s_lullaby_cry_info = NULL;

static lv_image_dsc_t  s_cam_dsc      = {
    .header.magic  = LV_IMAGE_HEADER_MAGIC,
    .header.cf     = LV_COLOR_FORMAT_RGB565,
    .header.w      = CAMERA_FRAME_W,
    .header.h      = CAMERA_FRAME_H,
    .header.stride = CAMERA_FRAME_W * 2U,  /* bytes per row — required by VGLite; 0 causes GPU hang */
    .data_size     = CAMERA_FRAME_SIZE,
    .data          = NULL,
};

/*******************************************************************************
* Helpers
*******************************************************************************/
static lv_obj_t *make_panel(lv_obj_t *parent,
                             lv_coord_t x, lv_coord_t y,
                             lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_set_pos(p, x, y);
    lv_obj_set_size(p, w, h);
    lv_obj_set_style_bg_color(p, COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(p, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(p, 1, 0);
    lv_obj_set_style_border_opa(p, LV_OPA_30, 0);
    lv_obj_set_style_radius(p, 10, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

static lv_obj_t *make_arc(lv_obj_t *parent, int32_t min, int32_t max,
                           lv_color_t color)
{
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_set_size(arc, 120, 120);
    lv_arc_set_range(arc, (int16_t)min, (int16_t)max);
    lv_arc_set_value(arc, (int16_t)min);
    lv_arc_set_bg_angles(arc, 135, 45);
    lv_obj_set_style_arc_color(arc, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    return arc;
}

static void section_label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 8);
}

/*******************************************************************************
* Update callback — 200 ms
*******************************************************************************/
static void radar_ui_update_cb(lv_timer_t *t)
{
    (void)t;
    radar_data_t d = radar_get_latest();

    /* ── Presence ─────────────────────────────────────────────────────── */
    if (d.presence)
    {
        lv_obj_set_style_bg_color(s_presence_circle, COLOR_GREEN, 0);
        lv_obj_set_style_shadow_color(s_presence_circle, COLOR_GREEN, 0);
        lv_obj_set_style_shadow_width(s_presence_circle, 20, 0);
        lv_label_set_text(s_presence_lbl, "DETECTED");
        lv_obj_set_style_text_color(s_presence_lbl, lv_color_hex(0x000000), 0);
        lv_label_set_text(s_presence_sub, "Target in range");
    }
    else
    {
        lv_obj_set_style_bg_color(s_presence_circle, COLOR_GRAY, 0);
        lv_obj_set_style_shadow_width(s_presence_circle, 0, 0);
        lv_label_set_text(s_presence_lbl, "NO TARGET");
        lv_obj_set_style_text_color(s_presence_lbl, COLOR_TEXT_SEC, 0);
        lv_label_set_text(s_presence_sub, "Monitoring...");
    }

    /* ── Motion ───────────────────────────────────────────────────────── */
    lv_arc_set_value(s_arc_motion, (int16_t)d.motion_pct);
    lv_label_set_text_fmt(s_lbl_motion, "%u%%", d.motion_pct);

    /* ── Direction ────────────────────────────────────────────────────── */
    if (!d.presence)
    {
        lv_label_set_text(s_lbl_dir_arrow, "");
        lv_label_set_text(s_lbl_direction, "No target");
        lv_obj_set_style_text_color(s_lbl_direction, COLOR_TEXT_SEC, 0);
    }
    else if (d.trend > 0)
    {
        lv_label_set_text(s_lbl_dir_arrow, "\xe2\x86\x97");   /* ↗ */
        lv_obj_set_style_text_color(s_lbl_dir_arrow, COLOR_ORANGE, 0);
        lv_label_set_text(s_lbl_direction, "APPROACHING");
        lv_obj_set_style_text_color(s_lbl_direction, COLOR_ORANGE, 0);
    }
    else if (d.trend < 0)
    {
        lv_label_set_text(s_lbl_dir_arrow, "\xe2\x86\x98");   /* ↘ */
        lv_obj_set_style_text_color(s_lbl_dir_arrow, COLOR_ACCENT, 0);
        lv_label_set_text(s_lbl_direction, "RECEDING");
        lv_obj_set_style_text_color(s_lbl_direction, COLOR_ACCENT, 0);
    }
    else
    {
        lv_label_set_text(s_lbl_dir_arrow, "\xe2\x86\x92");   /* → */
        lv_obj_set_style_text_color(s_lbl_dir_arrow, COLOR_GREEN, 0);
        lv_label_set_text(s_lbl_direction, "STATIONARY");
        lv_obj_set_style_text_color(s_lbl_direction, COLOR_GREEN, 0);
    }

    /* ── Signal over time chart ───────────────────────────────────────── */
    lv_chart_set_next_value(s_chart, s_series, (lv_value_precise_t)d.signal_pct);

    /* ── Range profile chart ──────────────────────────────────────────── */
    for (uint32_t i = 0U; i < RADAR_RANGE_BINS; i++)
        lv_chart_set_value_by_id(s_bar_chart, s_bar_series, i,
                                 (lv_value_precise_t)d.range_bins[i]);
    lv_chart_refresh(s_bar_chart);

    /* ── Camera frame (only update if camera is running) ─────────────── */
    if (s_cam_started)
    {
        static uint32_t s_cam_poll_count  = 0;
        static uint32_t s_cam_last_frame  = 0;
        camera_status_t cam_st = camera_get_status();

        if (cam_st == CAMERA_STATUS_NOT_FOUND && s_cam_wait_lbl != NULL)
        {
            lv_label_set_text(s_cam_wait_lbl, "Camera module not connected");
            lv_obj_set_style_text_color(s_cam_wait_lbl, lv_color_hex(0xFF4444), 0);
        }
        else if (cam_st == CAMERA_STATUS_ERROR && s_cam_wait_lbl != NULL)
        {
            lv_label_set_text(s_cam_wait_lbl, "Camera init failed");
            lv_obj_set_style_text_color(s_cam_wait_lbl, lv_color_hex(0xFF4444), 0);
        }
        else if (cam_st == CAMERA_STATUS_RUNNING)
        {
            /* Print alive every 5 polls (~1 s) so we can see GFX task is live */
            if ((s_cam_poll_count % 5U) == 0U)
            {
                printf("[UI] cam poll=%lu  frame=%p  img=%p\r\n",
                       (unsigned long)s_cam_poll_count,
                       (const void *)camera_get_frame(), (void *)s_cam_img);
                fflush(stdout);
            }
            s_cam_poll_count++;

            uint32_t       fc    = camera_get_frame_count();
            const uint8_t *frame = camera_get_frame();
            if (frame != NULL && fc != s_cam_last_frame)
            {
                s_cam_last_frame  = fc;
                s_cam_dsc.data    = frame;

                /* ── CAMERA tab image ─────────────────────────────── */
                if (s_cam_img == NULL && s_tab_camera != NULL)
                {
                    s_cam_img = lv_image_create(s_tab_camera);
                    lv_image_set_scale(s_cam_img, CAM_SCALE);
                    lv_obj_align(s_cam_img, LV_ALIGN_CENTER, 0, 0);
                    lv_image_set_src(s_cam_img, &s_cam_dsc);
                    if (s_cam_wait_lbl != NULL)
                    {
                        lv_obj_del(s_cam_wait_lbl);
                        s_cam_wait_lbl = NULL;
                    }
                }
                else if (s_cam_img != NULL)
                {
                    lv_obj_invalidate(s_cam_img);
                }

                /* ── BABY MON tab image (same source buffer) ──────── */
                if (s_baby_cam_img == NULL && s_tab_baby != NULL)
                {
                    s_baby_cam_img = lv_image_create(s_tab_baby);
                    lv_image_set_scale(s_baby_cam_img, BABY_CAM_SCALE);
                    lv_obj_set_pos(s_baby_cam_img, GAP,
                                   (448 - (CAMERA_FRAME_H * BABY_CAM_SCALE / 256)) / 2);
                    lv_image_set_src(s_baby_cam_img, &s_cam_dsc);
                    if (s_baby_cam_wait != NULL)
                    {
                        lv_obj_del(s_baby_cam_wait);
                        s_baby_cam_wait = NULL;
                    }
                }
                else if (s_baby_cam_img != NULL)
                {
                    lv_obj_invalidate(s_baby_cam_img);
                }

                /* ── SAFETY MONITOR tab image (same source buffer) ─── */
                if (s_sf_cam_img == NULL && s_tab_safety != NULL)
                {
                    s_sf_cam_img = lv_image_create(s_tab_safety);
                    lv_image_set_scale(s_sf_cam_img, SF_CAM_SCALE);
                    lv_obj_set_pos(s_sf_cam_img, GAP, SF_CAM_Y);
                    lv_image_set_src(s_sf_cam_img, &s_cam_dsc);
                    /* Move camera image behind overlay rects so bbox draws on top */
                    lv_obj_move_background(s_sf_cam_img);
                    if (s_sf_cam_wait != NULL)
                    {
                        lv_obj_del(s_sf_cam_wait);
                        s_sf_cam_wait = NULL;
                    }
                }
                else if (s_sf_cam_img != NULL)
                {
                    lv_obj_invalidate(s_sf_cam_img);
                }
            }
        }
    }

    /* ── System stats ────────────────────────────────────────────────── */
    sys_monitor_t sys = sys_monitor_get();
    lv_label_set_text_fmt(s_lbl_heap_free, "%lu KB", sys.heap_free_kb);
    lv_label_set_text_fmt(s_lbl_heap_min,  "%lu KB", sys.heap_used_kb);
    lv_label_set_text_fmt(s_lbl_uptime,    "%02lu:%02lu:%02lu",
                          sys.uptime_sec / 3600U,
                          (sys.uptime_sec % 3600U) / 60U,
                          sys.uptime_sec % 60U);
    lv_label_set_text_fmt(s_lbl_tasks,     "%u", sys.task_count);

    /* ── Badge ────────────────────────────────────────────────────────── */
    if (d.presence)
    {
        lv_obj_set_style_bg_color(s_badge, COLOR_GREEN, 0);
        lv_label_set_text(s_badge_lbl, "PRESENCE DETECTED");
        lv_obj_set_style_text_color(s_badge_lbl, lv_color_hex(0x000000), 0);
    }
    else
    {
        lv_obj_set_style_bg_color(s_badge, COLOR_GRAY, 0);
        lv_label_set_text(s_badge_lbl, "NO TARGET");
        lv_obj_set_style_text_color(s_badge_lbl, COLOR_TEXT_PRI, 0);
    }

    /* ── Baby monitor widgets ─────────────────────────────────────────── */
    if (s_cry_circle != NULL)
    {
        cry_state_t cry = cry_detector_get_state();

        if (cry.hw_error)
        {
            lv_obj_set_style_bg_color(s_cry_circle, COLOR_GRAY, 0);
            lv_obj_set_style_shadow_width(s_cry_circle, 0, 0);
            lv_label_set_text(s_cry_status_lbl, "Mic error");
            lv_obj_set_style_text_color(s_cry_status_lbl, lv_color_hex(0xFF4444), 0);
        }
        else if (cry.model_expired)
        {
            lv_obj_set_style_bg_color(s_cry_circle, COLOR_GRAY, 0);
            lv_obj_set_style_shadow_width(s_cry_circle, 0, 0);
            lv_label_set_text(s_cry_status_lbl, "Eval expired");
            lv_obj_set_style_text_color(s_cry_status_lbl, COLOR_ORANGE, 0);
        }
        else if (cry.is_crying)
        {
            lv_obj_set_style_bg_color(s_cry_circle, lv_color_hex(0xFF3333), 0);
            lv_obj_set_style_shadow_color(s_cry_circle, lv_color_hex(0xFF3333), 0);
            lv_obj_set_style_shadow_width(s_cry_circle, 24, 0);
            lv_label_set_text(s_cry_status_lbl, "BABY CRYING!");
            lv_obj_set_style_text_color(s_cry_status_lbl, lv_color_hex(0xFF3333), 0);
        }
        else
        {
            lv_obj_set_style_bg_color(s_cry_circle, COLOR_GRAY, 0);
            lv_obj_set_style_shadow_width(s_cry_circle, 0, 0);
            lv_label_set_text(s_cry_status_lbl, "Listening...");
            lv_obj_set_style_text_color(s_cry_status_lbl, COLOR_TEXT_SEC, 0);
        }

        lv_label_set_text_fmt(s_cry_count_lbl, "Events: %lu",
                              (unsigned long)cry.cry_count);
    }

    if (s_baby_arc_motion != NULL)
    {
        lv_arc_set_value(s_baby_arc_motion, (int16_t)d.motion_pct);
        lv_label_set_text_fmt(s_baby_lbl_motion, "%u%%", d.motion_pct);
    }

    /* ── Safety Monitor FSM ──────────────────────────────────────────── */
    if (s_sf_header_bg != NULL)
    {
        cry_state_t sf_cry = cry_detector_get_state();
        TickType_t  now    = xTaskGetTickCount();

        if (d.presence && !s_sf_had_presence)
        {
            s_sf_crib_entry   = now;
            s_sf_had_presence = true;
        }
        else if (!d.presence)
        {
            s_sf_had_presence = false;
        }

        if (sf_cry.is_crying)
            s_sf_yellow_until = now + pdMS_TO_TICKS(SF_YELLOW_MS);

        sf_state_t next;
        if (!d.presence)
            next = SF_RED;
        else if (now < s_sf_yellow_until)
            next = SF_YELLOW;
        else
            next = SF_GREEN;
        s_sf_state = next;

        lv_color_t  hdr_color;
        const char *state_str;
        if (s_sf_state == SF_GREEN)
        {
            hdr_color = lv_color_hex(0x1A7A3C);
            state_str = LV_SYMBOL_OK "  SLEEPING WELL";
        }
        else if (s_sf_state == SF_YELLOW)
        {
            hdr_color = lv_color_hex(0xB87800);
            state_str = LV_SYMBOL_WARNING "  CRY DETECTED";
        }
        else
        {
            hdr_color = lv_color_hex(0xAA1515);
            state_str = LV_SYMBOL_CLOSE "  NO PRESENCE";
        }
        lv_obj_set_style_bg_color(s_sf_header_bg, hdr_color, 0);
        lv_label_set_text(s_sf_state_lbl, state_str);

        /* ── Camera privacy gate ──────────────────────────────────────
         * Feed is hidden by default; it appears while the baby cries or
         * the radar reports presence without breathing, and stays up for
         * SF_CAM_HOLD_MS after the alert clears.  The Cam button forces
         * it permanently on when the caregiver wants to look anyway.    */
        /* Alarm whenever breathing is not confirmed — covers both "present
           but not breathing" and "no presence at all" (breathing is only
           true while a live target is detected), plus active crying.      */
        bool sf_alarm = sf_cry.is_crying || !d.breathing;
        if (sf_alarm)
            s_sf_cam_show_until = now + pdMS_TO_TICKS(SF_CAM_HOLD_MS);
        bool sf_cam_show = s_sf_cam_forced || (now < s_sf_cam_show_until);

        if (s_sf_cam_img != NULL)
        {
            if (sf_cam_show) lv_obj_clear_flag(s_sf_cam_img, LV_OBJ_FLAG_HIDDEN);
            else             lv_obj_add_flag(s_sf_cam_img, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_sf_cam_wait != NULL)
        {
            if (sf_cam_show) lv_obj_clear_flag(s_sf_cam_wait, LV_OBJ_FLAG_HIDDEN);
            else             lv_obj_add_flag(s_sf_cam_wait, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_sf_cam_off_lbl != NULL)
        {
            if (sf_cam_show) lv_obj_add_flag(s_sf_cam_off_lbl, LV_OBJ_FLAG_HIDDEN);
            else             lv_obj_clear_flag(s_sf_cam_off_lbl, LV_OBJ_FLAG_HIDDEN);
        }

        if (s_sf_had_presence && s_sf_crib_entry > 0u)
        {
            uint32_t secs = (uint32_t)((now - s_sf_crib_entry) / pdMS_TO_TICKS(1000U));
            lv_label_set_text_fmt(s_sf_uptime_lbl, "In crib: %uh %02um",
                                  secs / 3600U, (secs % 3600U) / 60U);
        }
        else
        {
            lv_label_set_text(s_sf_uptime_lbl, "In crib: --");
        }

        if (s_sf_breath_dot != NULL)
        {
            bool breathing = d.breathing;
            lv_obj_set_style_bg_color(s_sf_breath_dot,
                breathing ? COLOR_GREEN : COLOR_GRAY, 0);
            lv_label_set_text(s_sf_breath_val, breathing ? "Active" : "Low signal");
            lv_obj_set_style_text_color(s_sf_breath_val,
                breathing ? COLOR_GREEN : COLOR_TEXT_SEC, 0);
        }

        if (s_sf_crib_dot != NULL)
        {
            lv_obj_set_style_bg_color(s_sf_crib_dot,
                d.presence ? COLOR_GREEN : COLOR_GRAY, 0);
            lv_label_set_text(s_sf_crib_val, d.presence ? "In crib" : "Not detected");
            lv_obj_set_style_text_color(s_sf_crib_val,
                d.presence ? COLOR_GREEN : COLOR_TEXT_SEC, 0);
        }

        if (s_sf_cry_dot != NULL)
        {
            bool crying = sf_cry.is_crying;
            lv_obj_set_style_bg_color(s_sf_cry_dot,
                crying ? lv_color_hex(0xFF3333) : COLOR_GRAY, 0);
            lv_label_set_text(s_sf_cry_val, crying ? "Crying!" : "Quiet");
            lv_obj_set_style_text_color(s_sf_cry_val,
                crying ? lv_color_hex(0xFF3333) : COLOR_TEXT_SEC, 0);
        }

        if (s_sf_chart != NULL)
            lv_chart_set_next_value(s_sf_chart, s_sf_ser,
                                    (lv_value_precise_t)d.signal_pct);

        /* ── Playback control button labels ───────────────────────────── */
        if (s_sf_play_lbl != NULL)
            lv_label_set_text(s_sf_play_lbl,
                lullaby_player_is_playing()
                ? LV_SYMBOL_PAUSE "  Stop Lullaby"
                : LV_SYMBOL_PLAY  "  Play Lullaby");

        if (s_sf_mute_lbl != NULL)
            lv_label_set_text(s_sf_mute_lbl,
                buzzer_is_muted()
                ? LV_SYMBOL_AUDIO "  Unmute"
                : LV_SYMBOL_MUTE  "  Mute");

        /* ── Baby Mon AUDIO panel status ──────────────────────────────── */
        if (s_lullaby_status != NULL)
            lv_label_set_text(s_lullaby_status, lullaby_player_get_status());

        if (s_lullaby_cry_info != NULL)
            lv_label_set_text_fmt(s_lullaby_cry_info,
                "Cries (5 min): %u / 3", (unsigned)lullaby_get_window_cry_count());

        /* ── Multi-sensor algorithm update ────────────────────────────── */
        lullaby_player_update(&sf_cry, &d);
    }
}

/*******************************************************************************
* Safety tab — playback control buttons
*******************************************************************************/
static void on_play_click(lv_event_t *e)
{
    (void)e;
    printf("[UI] Play/Stop pressed  is_playing=%d\r\n",
           (int)lullaby_player_is_playing()); fflush(stdout);
    lullaby_player_toggle();
}

static void on_mute_click(lv_event_t *e)
{
    (void)e;
    buzzer_set_muted(!buzzer_is_muted());
}

static void on_sf_cam_click(lv_event_t *e)
{
    (void)e;
    s_sf_cam_forced = !s_sf_cam_forced;
    if (s_sf_cam_btn_lbl != NULL)
    {
        lv_label_set_text(s_sf_cam_btn_lbl, s_sf_cam_forced
                          ? LV_SYMBOL_EYE_OPEN  "  Cam: On"
                          : LV_SYMBOL_EYE_CLOSE "  Cam: Auto");
    }
}

/*******************************************************************************
* Baby Mon tab — sensitivity / threshold / volume sliders
*******************************************************************************/
static void on_cry_sens_changed(lv_event_t *e)
{
    lv_obj_t *sl  = (lv_obj_t *)lv_event_get_target(e);
    uint8_t   val = (uint8_t)lv_slider_get_value(sl);
    cry_set_sensitivity(val);
    if (s_cry_sens_val != NULL)
        lv_label_set_text_fmt(s_cry_sens_val, "%u/5", (unsigned)val);
}

static void on_radar_thr_changed(lv_event_t *e)
{
    lv_obj_t *sl  = (lv_obj_t *)lv_event_get_target(e);
    uint8_t   val = (uint8_t)lv_slider_get_value(sl);
    lullaby_set_radar_threshold(val);
    if (s_radar_thr_val != NULL)
        lv_label_set_text_fmt(s_radar_thr_val, "%u%%", (unsigned)val);
}

static void on_vol_changed(lv_event_t *e)
{
    lv_obj_t *sl  = (lv_obj_t *)lv_event_get_target(e);
    uint8_t   val = (uint8_t)lv_slider_get_value(sl);
    buzzer_set_volume(val);
    if (s_vol_val != NULL)
        lv_label_set_text_fmt(s_vol_val, "%u%%", (unsigned)val);
}

/*******************************************************************************
* Tab-change handler — lazy camera init
*******************************************************************************/
static void on_tab_changed(lv_event_t *e)
{
    lv_obj_t *tv  = (lv_obj_t *)lv_event_get_target(e);
    uint32_t  tab = lv_tabview_get_tab_active(tv);
    printf("[UI] tab changed -> tab %u  s_cam_started=%d\r\n",
           (unsigned)tab, (int)s_cam_started);
    fflush(stdout);

    /* Camera tab (1) or Baby monitor tab (2) — start camera if not running */
    if ((tab == 1U || tab == 2U || tab == 3U) && !s_cam_started)
    {
        s_cam_started = true;
        camera_status_t st = camera_get_status();
        if (st == CAMERA_STATUS_NOT_FOUND)
        {
            if (s_cam_wait_lbl != NULL)
            {
                lv_label_set_text(s_cam_wait_lbl, "Camera module not connected");
                lv_obj_set_style_text_color(s_cam_wait_lbl, lv_color_hex(0xFF4444), 0);
            }
            if (s_baby_cam_wait != NULL)
            {
                lv_label_set_text(s_baby_cam_wait, "Camera module not connected");
                lv_obj_set_style_text_color(s_baby_cam_wait, lv_color_hex(0xFF4444), 0);
            }
            if (s_sf_cam_wait != NULL)
            {
                lv_label_set_text(s_sf_cam_wait, "Camera module not connected");
                lv_obj_set_style_text_color(s_sf_cam_wait, lv_color_hex(0xFF4444), 0);
            }
        }
        else if (st != CAMERA_STATUS_RUNNING)
        {
            if (s_cam_wait_lbl != NULL)
            {
                lv_label_set_text(s_cam_wait_lbl, "Camera init failed");
                lv_obj_set_style_text_color(s_cam_wait_lbl, lv_color_hex(0xFF4444), 0);
            }
            if (s_baby_cam_wait != NULL)
            {
                lv_label_set_text(s_baby_cam_wait, "Camera init failed");
                lv_obj_set_style_text_color(s_baby_cam_wait, lv_color_hex(0xFF4444), 0);
            }
            if (s_sf_cam_wait != NULL)
            {
                lv_label_set_text(s_sf_cam_wait, "Camera init failed");
                lv_obj_set_style_text_color(s_sf_cam_wait, lv_color_hex(0xFF4444), 0);
            }
        }
    }
}

/*******************************************************************************
* radar_ui_create
*******************************************************************************/
void radar_ui_create(void)
{
    printf("[UI] radar_ui_create start\r\n"); fflush(stdout);
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* ══════════════════════════════════════════════════════════════════════
     * TOP-LEVEL TABVIEW  —  tab bar at bottom, 32 px
     * Tabs: RADAR (dashboard)  |  CAMERA (live feed)
     * ══════════════════════════════════════════════════════════════════════ */
    lv_obj_t *root_tv = lv_tabview_create(scr);
    lv_obj_set_size(root_tv, 800, 480);
    lv_obj_set_pos(root_tv, 0, 0);
    lv_tabview_set_tab_bar_position(root_tv, LV_DIR_BOTTOM);
    lv_tabview_set_tab_bar_size(root_tv, TAB_BAR_H);
    lv_obj_set_style_bg_color(root_tv, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(root_tv, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(root_tv, 0, 0);
    lv_obj_set_style_border_width(root_tv, 0, 0);

    lv_obj_t *root_bar = lv_tabview_get_tab_bar(root_tv);
    lv_obj_set_style_bg_color(root_bar, lv_color_hex(0x111820), 0);
    lv_obj_set_style_bg_opa(root_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(root_bar, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_color(root_bar, COLOR_ACCENT, LV_STATE_CHECKED);

    lv_obj_t *tab_radar  = lv_tabview_add_tab(root_tv, LV_SYMBOL_LIST "  RADAR");
    s_tab_camera         = lv_tabview_add_tab(root_tv, LV_SYMBOL_IMAGE "  CAMERA");
    s_tab_baby           = lv_tabview_add_tab(root_tv, LV_SYMBOL_AUDIO "  BABY MON");
    s_tab_safety         = lv_tabview_add_tab(root_tv, LV_SYMBOL_BELL "  SAFETY");
    lv_obj_t *tab_camera = s_tab_camera;
    lv_obj_t *tab_baby   = s_tab_baby;
    lv_obj_t *tab_safety = s_tab_safety;
    lv_obj_clear_flag(tab_radar,  LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tab_camera, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tab_baby,   LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tab_safety, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(tab_radar,  0, 0);
    lv_obj_set_style_pad_all(tab_camera, 0, 0);
    lv_obj_set_style_pad_all(tab_baby,   0, 0);
    lv_obj_set_style_pad_all(tab_safety, 0, 0);
    lv_obj_set_style_bg_color(tab_radar,  COLOR_BG, 0);
    lv_obj_set_style_bg_opa(tab_radar,   LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(tab_camera, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(tab_camera,  LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(tab_baby,   COLOR_BG, 0);
    lv_obj_set_style_bg_opa(tab_baby,    LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(tab_safety, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(tab_safety,  LV_OPA_COVER, 0);

    /* Use tab_radar as parent for all dashboard widgets (replaces scr) */
    lv_obj_t *p = tab_radar;   /* alias — used by all widget creation below */
    (void)scr;

    /* ── Title ────────────────────────────────────────────────────────── */
    lv_obj_t *title = lv_label_create(p);
    lv_label_set_text(title, "PSoC Edge E84  \xe2\x80\x94  60 GHz Radar Sensor");
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, GAP, 10);

    /* ── Badge (top right) ────────────────────────────────────────────── */
    s_badge = lv_obj_create(p);
    lv_obj_set_size(s_badge, 210, 30);
    lv_obj_align(s_badge, LV_ALIGN_TOP_RIGHT, -GAP, 6);
    lv_obj_set_style_radius(s_badge, 15, 0);
    lv_obj_set_style_bg_color(s_badge, COLOR_GRAY, 0);
    lv_obj_set_style_border_width(s_badge, 0, 0);
    lv_obj_clear_flag(s_badge, LV_OBJ_FLAG_SCROLLABLE);

    s_badge_lbl = lv_label_create(s_badge);
    lv_label_set_text(s_badge_lbl, "NO TARGET");
    lv_obj_set_style_text_color(s_badge_lbl, COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(s_badge_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(s_badge_lbl, LV_ALIGN_CENTER, 0, 0);

    /* ════════════════════════════════════════════════════════════════════
     * TOP LEFT — PRESENCE
     * ════════════════════════════════════════════════════════════════════ */
    lv_obj_t *p_pres = make_panel(p, GAP, TOP_Y, LEFT_W, TOP_H);
    section_label(p_pres, "PRESENCE");

    s_presence_circle = lv_obj_create(p_pres);
    lv_obj_set_size(s_presence_circle, 110, 110);
    lv_obj_align(s_presence_circle, LV_ALIGN_LEFT_MID, 20, 8);
    lv_obj_set_style_radius(s_presence_circle, 55, 0);
    lv_obj_set_style_bg_color(s_presence_circle, COLOR_GRAY, 0);
    lv_obj_set_style_border_width(s_presence_circle, 0, 0);
    lv_obj_set_style_shadow_width(s_presence_circle, 0, 0);
    lv_obj_clear_flag(s_presence_circle, LV_OBJ_FLAG_SCROLLABLE);

    s_presence_lbl = lv_label_create(p_pres);
    lv_label_set_text(s_presence_lbl, "NO TARGET");
    lv_obj_set_style_text_color(s_presence_lbl, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s_presence_lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(s_presence_lbl, LV_ALIGN_LEFT_MID, 148, -14);

    s_presence_sub = lv_label_create(p_pres);
    lv_label_set_text(s_presence_sub, "Monitoring...");
    lv_obj_set_style_text_color(s_presence_sub, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s_presence_sub, &lv_font_montserrat_14, 0);
    lv_obj_align(s_presence_sub, LV_ALIGN_LEFT_MID, 148, 12);

    /* ════════════════════════════════════════════════════════════════════
     * TOP RIGHT — MOTION INTENSITY
     * ════════════════════════════════════════════════════════════════════ */
    lv_obj_t *p_mot = make_panel(p, RIGHT_X, TOP_Y, RIGHT_W, TOP_H);
    section_label(p_mot, "MOTION INTENSITY");

    s_arc_motion = make_arc(p_mot, 0, 100, COLOR_ORANGE);
    /* 92 px (down from 120) — the full-size arc overlapped the section title */
    lv_obj_set_size(s_arc_motion, 92, 92);
    lv_obj_align(s_arc_motion, LV_ALIGN_CENTER, -40, 14);

    s_lbl_motion = lv_label_create(p_mot);
    lv_label_set_text(s_lbl_motion, "0%");
    lv_obj_set_style_text_color(s_lbl_motion, COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(s_lbl_motion, &lv_font_montserrat_28, 0);
    lv_obj_align(s_lbl_motion, LV_ALIGN_CENTER, -40, 14);

    lv_obj_t *mot_desc = lv_label_create(p_mot);
    lv_label_set_text(mot_desc, "How much the\nscene is changing");
    lv_obj_set_style_text_color(mot_desc, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(mot_desc, &lv_font_montserrat_12, 0);
    lv_obj_align(mot_desc, LV_ALIGN_RIGHT_MID, -16, 8);

    /* ════════════════════════════════════════════════════════════════════
     * BOTTOM LEFT — TABVIEW: Direction  /  System
     * ════════════════════════════════════════════════════════════════════ */
    lv_obj_t *dir_tv = lv_tabview_create(p);
    lv_obj_set_pos(dir_tv, GAP, BOT_Y);
    lv_obj_set_size(dir_tv, LEFT_W, BOT_H);
    lv_tabview_set_tab_bar_size(dir_tv, 30);
    lv_obj_set_style_bg_color(dir_tv, COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(dir_tv, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(dir_tv, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(dir_tv, 1, 0);
    lv_obj_set_style_border_opa(dir_tv, LV_OPA_30, 0);
    lv_obj_set_style_radius(dir_tv, 10, 0);
    lv_obj_set_style_pad_all(dir_tv, 0, 0);

    lv_obj_t *dir_bar = lv_tabview_get_tab_bar(dir_tv);
    lv_obj_set_style_bg_color(dir_bar, COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(dir_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(dir_bar, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_color(dir_bar, COLOR_ACCENT, LV_STATE_CHECKED);

    lv_obj_t *tab_dir = lv_tabview_add_tab(dir_tv, "DIRECTION");
    lv_obj_t *tab_sys = lv_tabview_add_tab(dir_tv, "SYSTEM");
    lv_obj_clear_flag(tab_dir, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tab_sys, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(tab_dir, 4, 0);
    lv_obj_set_style_pad_all(tab_sys, 8, 0);

    /* ── Direction tab ──────────────────────────────────────────────── */
    s_lbl_dir_arrow = lv_label_create(tab_dir);
    lv_label_set_text(s_lbl_dir_arrow, "");
    lv_obj_set_style_text_font(s_lbl_dir_arrow, &lv_font_montserrat_48, 0);
    lv_obj_align(s_lbl_dir_arrow, LV_ALIGN_LEFT_MID, 20, 0);

    s_lbl_direction = lv_label_create(tab_dir);
    lv_label_set_text(s_lbl_direction, "No target");
    lv_obj_set_style_text_color(s_lbl_direction, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s_lbl_direction, &lv_font_montserrat_20, 0);
    lv_obj_align(s_lbl_direction, LV_ALIGN_LEFT_MID, 115, 0);

    lv_obj_t *dir_note = lv_label_create(tab_dir);
    lv_label_set_text(dir_note, "Signal strength\ntrend over 800ms");
    lv_obj_set_style_text_color(dir_note, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(dir_note, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(dir_note, LV_TEXT_ALIGN_RIGHT, 0);
    /* Bottom-right corner — at RIGHT_MID it collided with "APPROACHING" */
    lv_obj_align(dir_note, LV_ALIGN_BOTTOM_RIGHT, -6, -2);

    /* ── System tab — 2×2 stat grid ────────────────────────────────── */
    lv_coord_t col2 = (LEFT_W - 16) / 2;   /* x offset of second column */

    /* FREE HEAP */
    lv_obj_t *lbl_t1 = lv_label_create(tab_sys);
    lv_label_set_text(lbl_t1, "FREE HEAP");
    lv_obj_set_style_text_color(lbl_t1, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_t1, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_t1, 0, 0);

    s_lbl_heap_free = lv_label_create(tab_sys);
    lv_label_set_text(s_lbl_heap_free, "--- KB");
    lv_obj_set_style_text_color(s_lbl_heap_free, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(s_lbl_heap_free, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(s_lbl_heap_free, 0, 16);

    /* MIN HEAP (watermark) */
    lv_obj_t *lbl_t2 = lv_label_create(tab_sys);
    lv_label_set_text(lbl_t2, "HEAP USED");
    lv_obj_set_style_text_color(lbl_t2, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_t2, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_t2, col2, 0);

    s_lbl_heap_min = lv_label_create(tab_sys);
    lv_label_set_text(s_lbl_heap_min, "--- KB");
    lv_obj_set_style_text_color(s_lbl_heap_min, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(s_lbl_heap_min, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(s_lbl_heap_min, col2, 16);

    /* UPTIME */
    lv_obj_t *lbl_t3 = lv_label_create(tab_sys);
    lv_label_set_text(lbl_t3, "UPTIME");
    lv_obj_set_style_text_color(lbl_t3, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_t3, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_t3, 0, 58);

    s_lbl_uptime = lv_label_create(tab_sys);
    lv_label_set_text(s_lbl_uptime, "00:00:00");
    lv_obj_set_style_text_color(s_lbl_uptime, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(s_lbl_uptime, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(s_lbl_uptime, 0, 74);

    /* TASK COUNT */
    lv_obj_t *lbl_t4 = lv_label_create(tab_sys);
    lv_label_set_text(lbl_t4, "RTOS TASKS");
    lv_obj_set_style_text_color(lbl_t4, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_t4, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_t4, col2, 58);

    s_lbl_tasks = lv_label_create(tab_sys);
    lv_label_set_text(s_lbl_tasks, "--");
    lv_obj_set_style_text_color(s_lbl_tasks, COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(s_lbl_tasks, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(s_lbl_tasks, col2, 74);


    /* ════════════════════════════════════════════════════════════════════
     * BOTTOM RIGHT — TABVIEW: Signal Over Time  /  Range Profile
     * ════════════════════════════════════════════════════════════════════ */
    lv_obj_t *tabview = lv_tabview_create(p);
    lv_obj_set_pos(tabview, RIGHT_X, BOT_Y);
    lv_obj_set_size(tabview, RIGHT_W, BOT_H);
    lv_tabview_set_tab_bar_size(tabview, 30);

    lv_obj_set_style_bg_color(tabview, COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(tabview, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(tabview, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(tabview, 1, 0);
    lv_obj_set_style_border_opa(tabview, LV_OPA_30, 0);
    lv_obj_set_style_radius(tabview, 10, 0);
    lv_obj_set_style_pad_all(tabview, 0, 0);

    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(tabview);
    lv_obj_set_style_bg_color(tab_bar, COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(tab_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(tab_bar, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_color(tab_bar, COLOR_ACCENT, LV_STATE_CHECKED);

    lv_obj_t *tab1 = lv_tabview_add_tab(tabview, "SIGNAL OVER TIME");
    lv_obj_t *tab2 = lv_tabview_add_tab(tabview, "RANGE PROFILE");
    lv_obj_clear_flag(tab1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tab2, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(tab1, 4, 0);
    lv_obj_set_style_pad_all(tab2, 4, 0);

    lv_coord_t chart_w = RIGHT_W - 16;
    lv_coord_t chart_h = BOT_H - 30 - 20;

    /* ── Tab 1: Signal strength over time ───────────────────────────── */
    s_chart = lv_chart_create(tab1);
    lv_obj_set_size(s_chart, chart_w, chart_h);
    lv_obj_align(s_chart, LV_ALIGN_CENTER, 0, 0);
    lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_chart, CHART_POINTS);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_obj_set_style_bg_color(s_chart, COLOR_BAR_BG, 0);
    lv_obj_set_style_border_width(s_chart, 0, 0);
    lv_obj_set_style_line_color(s_chart, lv_color_hex(0x2A3A4A), LV_PART_MAIN);
    s_series = lv_chart_add_series(s_chart, COLOR_ACCENT, LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(s_chart, s_series, 0);
    lv_obj_set_style_size(s_chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(s_chart, 2, LV_PART_ITEMS);

    /* ── Tab 2: Range profile (magnitude vs distance bin) ───────────── */
    s_bar_chart = lv_chart_create(tab2);
    lv_obj_set_size(s_bar_chart, chart_w, chart_h);
    lv_obj_align(s_bar_chart, LV_ALIGN_CENTER, 0, 0);
    lv_chart_set_type(s_bar_chart, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(s_bar_chart, RADAR_RANGE_BINS);
    lv_chart_set_range(s_bar_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_obj_set_style_bg_color(s_bar_chart, COLOR_BAR_BG, 0);
    lv_obj_set_style_border_width(s_bar_chart, 0, 0);
    lv_obj_set_style_line_color(s_bar_chart, lv_color_hex(0x2A3A4A), LV_PART_MAIN);
    s_bar_series = lv_chart_add_series(s_bar_chart, COLOR_ACCENT, LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(s_bar_chart, s_bar_series, 0);

    /* ════════════════════════════════════════════════════════════════════
     * BOTTOM BAR — Infineon + Mouser logo badges (lower-left, no overlap)
     * SVG rendering not available (ThorVG disabled in this build).
     * Brand colours extracted directly from the supplied SVG files:
     *   Infineon.svg  → cls-1 #0066B3 (blue), cls-2 #EC1840 (red)
     *   Mouser-Electronics.svg → gradient stop orange #F05A28
     * ════════════════════════════════════════════════════════════════════ */
    /* ── Infineon badge ─────────────────────────────────────────── */
    {
        lv_obj_t *ifx = lv_obj_create(p);
        lv_obj_set_size(ifx, 100, 26);
        lv_obj_align(ifx, LV_ALIGN_BOTTOM_LEFT, GAP, -GAP);
        lv_obj_set_style_radius(ifx, 5, 0);
        lv_obj_set_style_bg_color(ifx, lv_color_hex(0x0066B3), 0);
        lv_obj_set_style_bg_opa(ifx, LV_OPA_90, 0);
        lv_obj_set_style_border_width(ifx, 0, 0);
        lv_obj_clear_flag(ifx, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *ifx_dot = lv_obj_create(ifx);
        lv_obj_set_size(ifx_dot, 6, 6);
        lv_obj_align(ifx_dot, LV_ALIGN_LEFT_MID, 7, 0);
        lv_obj_set_style_radius(ifx_dot, 3, 0);
        lv_obj_set_style_bg_color(ifx_dot, lv_color_hex(0xEC1840), 0);
        lv_obj_set_style_border_width(ifx_dot, 0, 0);

        lv_obj_t *ifx_lbl = lv_label_create(ifx);
        lv_label_set_text(ifx_lbl, "Infineon");
        lv_obj_set_style_text_color(ifx_lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(ifx_lbl, &lv_font_montserrat_12, 0);
        lv_obj_align(ifx_lbl, LV_ALIGN_LEFT_MID, 18, 0);
    }

    /* ── Mouser badge ───────────────────────────────────────────── */
    {
        lv_obj_t *mouser = lv_obj_create(p);
        lv_obj_set_size(mouser, 152, 26);
        lv_obj_align(mouser, LV_ALIGN_BOTTOM_LEFT, GAP + 100 + 4, -GAP);
        lv_obj_set_style_radius(mouser, 5, 0);
        lv_obj_set_style_bg_color(mouser, lv_color_hex(0x1A2E44), 0);
        lv_obj_set_style_bg_opa(mouser, LV_OPA_90, 0);
        lv_obj_set_style_border_color(mouser, lv_color_hex(0xF05A28), 0);
        lv_obj_set_style_border_width(mouser, 1, 0);
        lv_obj_clear_flag(mouser, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *mouser_dot = lv_obj_create(mouser);
        lv_obj_set_size(mouser_dot, 6, 6);
        lv_obj_align(mouser_dot, LV_ALIGN_LEFT_MID, 7, 0);
        lv_obj_set_style_radius(mouser_dot, 3, 0);
        lv_obj_set_style_bg_color(mouser_dot, lv_color_hex(0xF05A28), 0);
        lv_obj_set_style_border_width(mouser_dot, 0, 0);

        lv_obj_t *mouser_lbl = lv_label_create(mouser);
        lv_label_set_text(mouser_lbl, "Mouser");
        lv_obj_set_style_text_color(mouser_lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(mouser_lbl, &lv_font_montserrat_12, 0);
        lv_obj_align(mouser_lbl, LV_ALIGN_LEFT_MID, 18, 0);
    }

    /* ════════════════════════════════════════════════════════════════════
     * CAMERA TAB — live 320×240 RGB565 feed, scaled to fill 597×448.
     * s_cam_img is created lazily in the update timer once the first frame
     * arrives — no lv_image widget exists in tab_camera until then, so
     * LVGL's tab-switch layout/render pass sees only a plain label, which
     * avoids a hard hang when the VGLite GPU tries to process an image
     * with a NULL data pointer or an in-flight DMA buffer.
     * ════════════════════════════════════════════════════════════════════ */
    s_cam_wait_lbl = lv_label_create(tab_camera);
    lv_obj_set_style_text_font(s_cam_wait_lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(s_cam_wait_lbl, LV_ALIGN_CENTER, 0, 0);

    {
        camera_status_t st = camera_get_status();
        if (st == CAMERA_STATUS_RUNNING)
        {
            lv_label_set_text(s_cam_wait_lbl, "Starting camera...");
            lv_obj_set_style_text_color(s_cam_wait_lbl, COLOR_TEXT_SEC, 0);
            /* Start the frame-monitor task immediately so frames accumulate
               before the user even taps the CAMERA tab. */
            s_cam_started = true;
            xTaskCreate(camera_task, CAMERA_TASK_NAME,
                        CAMERA_TASK_STACK_SIZE, NULL,
                        CAMERA_TASK_PRIORITY, NULL);
        }
        else if (st == CAMERA_STATUS_NOT_FOUND)
        {
            lv_label_set_text(s_cam_wait_lbl, "Camera module not connected");
            lv_obj_set_style_text_color(s_cam_wait_lbl, lv_color_hex(0xFF4444), 0);
        }
        else
        {
            lv_label_set_text(s_cam_wait_lbl, "Camera init failed");
            lv_obj_set_style_text_color(s_cam_wait_lbl, lv_color_hex(0xFF4444), 0);
        }
    }

    /* ════════════════════════════════════════════════════════════════════
     * BABY MONITOR TAB
     * Left: live 320×240 camera feed scaled to 480×360.
     * Right: two stacked panels — CRY DETECTION (top) + MOTION (bottom).
     * Camera image is created lazily (same s_cam_dsc source as CAMERA tab).
     * ════════════════════════════════════════════════════════════════════ */

    /* Camera placeholder label — replaced by image on first frame */
    s_baby_cam_wait = lv_label_create(tab_baby);
    lv_obj_set_style_text_font(s_baby_cam_wait, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(s_baby_cam_wait, GAP + 120, 224 - 8);

    {
        camera_status_t st = camera_get_status();
        if (st == CAMERA_STATUS_RUNNING)
        {
            lv_label_set_text(s_baby_cam_wait, "Starting camera...");
            lv_obj_set_style_text_color(s_baby_cam_wait, COLOR_TEXT_SEC, 0);
        }
        else if (st == CAMERA_STATUS_NOT_FOUND)
        {
            lv_label_set_text(s_baby_cam_wait, "Camera not connected");
            lv_obj_set_style_text_color(s_baby_cam_wait, lv_color_hex(0xFF4444), 0);
        }
        else
        {
            lv_label_set_text(s_baby_cam_wait, "Camera init failed");
            lv_obj_set_style_text_color(s_baby_cam_wait, lv_color_hex(0xFF4444), 0);
        }
    }

    /* ── Right-side panels (3 × BABY_PANEL_H each) ──────────────────── */

    /* Helper: create a compact slider with title + value labels */
    /* Slider fits within BABY_PANEL_H — placed at bottom of each panel. */
#define MAKE_SLIDER(parent, sl_ptr, val_ptr, sl_min, sl_max, sl_def, cb, title_str, val_fmt) \
    do { \
        lv_coord_t _sw = BABY_RIGHT_W - 16; \
        lv_coord_t _sy = BABY_PANEL_H - 30; \
        lv_obj_t *_t = lv_label_create(parent); \
        lv_label_set_text(_t, (title_str)); \
        lv_obj_set_style_text_color(_t, COLOR_TEXT_SEC, 0); \
        lv_obj_set_style_text_font(_t, &lv_font_montserrat_12, 0); \
        lv_obj_set_pos(_t, 8, _sy - 18); \
        *(val_ptr) = lv_label_create(parent); \
        lv_label_set_text_fmt(*(val_ptr), val_fmt, (unsigned)(sl_def)); \
        lv_obj_set_style_text_color(*(val_ptr), COLOR_ACCENT, 0); \
        lv_obj_set_style_text_font(*(val_ptr), &lv_font_montserrat_12, 0); \
        lv_obj_align(*(val_ptr), LV_ALIGN_TOP_RIGHT, -8, _sy - 18); \
        *(sl_ptr) = lv_slider_create(parent); \
        lv_obj_set_size(*(sl_ptr), _sw, 20); \
        lv_obj_set_pos(*(sl_ptr), 8, _sy); \
        lv_slider_set_range(*(sl_ptr), (sl_min), (sl_max)); \
        lv_slider_set_value(*(sl_ptr), (sl_def), LV_ANIM_OFF); \
        lv_obj_set_style_bg_color(*(sl_ptr), COLOR_BAR_BG, LV_PART_MAIN); \
        lv_obj_set_style_bg_color(*(sl_ptr), COLOR_ACCENT, LV_PART_INDICATOR); \
        lv_obj_set_style_bg_color(*(sl_ptr), COLOR_TEXT_PRI, LV_PART_KNOB); \
        lv_obj_add_event_cb(*(sl_ptr), (cb), LV_EVENT_VALUE_CHANGED, NULL); \
    } while (0)

    /* ─ CRY DETECTION panel ─ */
    lv_obj_t *p_cry = make_panel(tab_baby,
                                  BABY_RIGHT_X, GAP,
                                  BABY_RIGHT_W, BABY_PANEL_H);
    section_label(p_cry, "CRY DETECTION");

    s_cry_circle = lv_obj_create(p_cry);
    lv_obj_set_size(s_cry_circle, 50, 50);
    lv_obj_set_pos(s_cry_circle, 8, 26);
    lv_obj_set_style_radius(s_cry_circle, 25, 0);
    lv_obj_set_style_bg_color(s_cry_circle, COLOR_GRAY, 0);
    lv_obj_set_style_border_width(s_cry_circle, 0, 0);
    lv_obj_set_style_shadow_width(s_cry_circle, 0, 0);
    lv_obj_clear_flag(s_cry_circle, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *mic_icon = lv_label_create(s_cry_circle);
    lv_label_set_text(mic_icon, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(mic_icon, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(mic_icon, COLOR_TEXT_PRI, 0);
    lv_obj_align(mic_icon, LV_ALIGN_CENTER, 0, 0);

    s_cry_status_lbl = lv_label_create(p_cry);
    lv_label_set_text(s_cry_status_lbl, "Listening...");
    lv_obj_set_style_text_color(s_cry_status_lbl, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s_cry_status_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(s_cry_status_lbl, 66, 26);

    s_cry_count_lbl = lv_label_create(p_cry);
    lv_label_set_text(s_cry_count_lbl, "Events: 0");
    lv_obj_set_style_text_color(s_cry_count_lbl, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s_cry_count_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(s_cry_count_lbl, 66, 46);

    MAKE_SLIDER(p_cry, &s_cry_sens_slider, &s_cry_sens_val,
                1, 5, 3, on_cry_sens_changed, "Sensitivity", "%u/5");

    /* ─ MOTION INTENSITY panel ─ */
    lv_obj_t *p_bmot = make_panel(tab_baby,
                                   BABY_RIGHT_X, GAP + BABY_PANEL_H + GAP,
                                   BABY_RIGHT_W, BABY_PANEL_H);
    section_label(p_bmot, "MOTION INTENSITY");

    s_baby_arc_motion = make_arc(p_bmot, 0, 100, COLOR_ORANGE);
    lv_obj_set_size(s_baby_arc_motion, 70, 70);
    lv_obj_set_pos(s_baby_arc_motion, 8, 22);

    s_baby_lbl_motion = lv_label_create(p_bmot);
    lv_label_set_text(s_baby_lbl_motion, "0%");
    lv_obj_set_style_text_color(s_baby_lbl_motion, COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(s_baby_lbl_motion, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(s_baby_lbl_motion, 85, 38);

    lv_obj_t *bmot_desc = lv_label_create(p_bmot);
    lv_label_set_text(bmot_desc, "Radar motion");
    lv_obj_set_style_text_color(bmot_desc, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(bmot_desc, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(bmot_desc, 85, 58);

    MAKE_SLIDER(p_bmot, &s_radar_thr_slider, &s_radar_thr_val,
                20, 90, 70, on_radar_thr_changed, "Lullaby trigger", "%u%%");

    /* ─ AUDIO / LULLABY panel ─ */
    lv_obj_t *p_audio = make_panel(tab_baby,
                                    BABY_RIGHT_X,
                                    GAP + (BABY_PANEL_H + GAP) * 2,
                                    BABY_RIGHT_W, BABY_PANEL_H);
    section_label(p_audio, "AUDIO");

    s_lullaby_status = lv_label_create(p_audio);
    lv_label_set_text(s_lullaby_status, "Idle");
    lv_obj_set_style_text_color(s_lullaby_status, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(s_lullaby_status, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(s_lullaby_status, 8, 26);

    s_lullaby_cry_info = lv_label_create(p_audio);
    lv_label_set_text(s_lullaby_cry_info, "Cries (5 min): 0 / 3");
    lv_obj_set_style_text_color(s_lullaby_cry_info, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s_lullaby_cry_info, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(s_lullaby_cry_info, 8, 46);

    MAKE_SLIDER(p_audio, &s_vol_slider, &s_vol_val,
                0, 100, 80, on_vol_changed, "Volume", "%u%%");

    /* Apply default volume */
    buzzer_set_volume(80u);

#undef MAKE_SLIDER

    /* ════════════════════════════════════════════════════════════════════
     * SAFETY MONITOR TAB
     * Green/Yellow/Red FSM — radar presence + cry detector.
     * Left: 480×360 camera (same s_cam_dsc, lazy).
     * Right: 4 status chips + mini radar signal chart.
     * Bottom: action button row (visual placeholders).
     * ════════════════════════════════════════════════════════════════════ */

    /* Header bar — colour changes with FSM state */
    s_sf_header_bg = lv_obj_create(tab_safety);
    lv_obj_set_pos(s_sf_header_bg, 0, 0);
    lv_obj_set_size(s_sf_header_bg, 800, SF_HDR_H);
    lv_obj_set_style_bg_color(s_sf_header_bg, lv_color_hex(0x1A7A3C), 0);
    lv_obj_set_style_bg_opa(s_sf_header_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_sf_header_bg, 0, 0);
    lv_obj_set_style_radius(s_sf_header_bg, 0, 0);
    lv_obj_clear_flag(s_sf_header_bg, LV_OBJ_FLAG_SCROLLABLE);

    s_sf_state_lbl = lv_label_create(s_sf_header_bg);
    lv_label_set_text(s_sf_state_lbl, LV_SYMBOL_OK "  SLEEPING WELL");
    lv_obj_set_style_text_color(s_sf_state_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(s_sf_state_lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(s_sf_state_lbl, LV_ALIGN_LEFT_MID, 10, 0);

    s_sf_uptime_lbl = lv_label_create(s_sf_header_bg);
    lv_label_set_text(s_sf_uptime_lbl, "In crib: --");
    lv_obj_set_style_text_color(s_sf_uptime_lbl, lv_color_hex(0xCCFFCC), 0);
    lv_obj_set_style_text_font(s_sf_uptime_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(s_sf_uptime_lbl, LV_ALIGN_RIGHT_MID, -10, 0);

    /* Camera wait-label (replaced lazily when first frame arrives) */
    s_sf_cam_wait = lv_label_create(tab_safety);
    lv_obj_set_style_text_font(s_sf_cam_wait, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(s_sf_cam_wait, GAP + 100, SF_CAM_Y + 170);
    {
        camera_status_t cst = camera_get_status();
        if (cst == CAMERA_STATUS_RUNNING)
        {
            lv_label_set_text(s_sf_cam_wait, "Starting camera...");
            lv_obj_set_style_text_color(s_sf_cam_wait, COLOR_TEXT_SEC, 0);
        }
        else if (cst == CAMERA_STATUS_NOT_FOUND)
        {
            lv_label_set_text(s_sf_cam_wait, "Camera not connected");
            lv_obj_set_style_text_color(s_sf_cam_wait, lv_color_hex(0xFF4444), 0);
        }
        else
        {
            lv_label_set_text(s_sf_cam_wait, "Camera init failed");
            lv_obj_set_style_text_color(s_sf_cam_wait, lv_color_hex(0xFF4444), 0);
        }
    }

    /* Right-side status chips */
    {
        lv_coord_t chip_x = SF_RIGHT_X;
        lv_coord_t chip_w = SF_RIGHT_W - GAP;
        lv_coord_t cy     = SF_CAM_Y + GAP;

        /* Chip helper macro — creates panel + icon label + dot + value label */
#define MAKE_CHIP(parent, cx, cyy, cw, ch, icon_sym, dot_ptr, val_ptr)          \
        do {                                                                      \
            lv_obj_t *chip = make_panel((parent), (cx), (cyy), (cw), (ch));     \
            lv_obj_t *icon = lv_label_create(chip);                             \
            lv_label_set_text(icon, (icon_sym));                                 \
            lv_obj_set_style_text_color(icon, COLOR_TEXT_SEC, 0);               \
            lv_obj_set_style_text_font(icon, &lv_font_montserrat_12, 0);        \
            lv_obj_align(icon, LV_ALIGN_LEFT_MID, 8, 0);                       \
            *(dot_ptr) = lv_obj_create(chip);                                   \
            lv_obj_set_size(*(dot_ptr), 12, 12);                                \
            lv_obj_set_style_radius(*(dot_ptr), 6, 0);                          \
            lv_obj_set_style_bg_color(*(dot_ptr), COLOR_GRAY, 0);               \
            lv_obj_set_style_border_width(*(dot_ptr), 0, 0);                    \
            lv_obj_clear_flag(*(dot_ptr), LV_OBJ_FLAG_SCROLLABLE);              \
            lv_obj_align(*(dot_ptr), LV_ALIGN_RIGHT_MID, -55, 0);              \
            *(val_ptr) = lv_label_create(chip);                                 \
            lv_label_set_text(*(val_ptr), "...");                                \
            lv_obj_set_style_text_color(*(val_ptr), COLOR_TEXT_SEC, 0);         \
            lv_obj_set_style_text_font(*(val_ptr), &lv_font_montserrat_12, 0);  \
            lv_obj_align(*(val_ptr), LV_ALIGN_RIGHT_MID, -8, 0);               \
        } while (0)

        MAKE_CHIP(tab_safety, chip_x, cy, chip_w, SF_CHIP_H,
                  LV_SYMBOL_REFRESH "  BREATHING", &s_sf_breath_dot, &s_sf_breath_val);
        cy += SF_CHIP_H + GAP;

        MAKE_CHIP(tab_safety, chip_x, cy, chip_w, SF_CHIP_H,
                  LV_SYMBOL_HOME "  IN CRIB", &s_sf_crib_dot, &s_sf_crib_val);
        cy += SF_CHIP_H + GAP;

        MAKE_CHIP(tab_safety, chip_x, cy, chip_w, SF_CHIP_H,
                  LV_SYMBOL_AUDIO "  CRY", &s_sf_cry_dot, &s_sf_cry_val);
        cy += SF_CHIP_H + GAP;

#undef MAKE_CHIP

        /* Mini radar signal chart — fills remaining right-panel height */
        {
            lv_coord_t chart_panel_h = (SF_CAM_Y + SF_CAM_H) - cy - GAP;
            if (chart_panel_h < 40) chart_panel_h = 40;
            lv_obj_t *p_mini = make_panel(tab_safety, chip_x, cy, chip_w, chart_panel_h);
            lv_obj_t *clbl   = lv_label_create(p_mini);
            lv_label_set_text(clbl, "RADAR SIGNAL");
            lv_obj_set_style_text_color(clbl, COLOR_TEXT_SEC, 0);
            lv_obj_set_style_text_font(clbl, &lv_font_montserrat_12, 0);
            lv_obj_align(clbl, LV_ALIGN_TOP_LEFT, 6, 4);

            s_sf_chart = lv_chart_create(p_mini);
            lv_obj_set_size(s_sf_chart, chip_w - 12, chart_panel_h - 24);
            lv_obj_align(s_sf_chart, LV_ALIGN_BOTTOM_MID, 0, -4);
            lv_chart_set_type(s_sf_chart, LV_CHART_TYPE_LINE);
            lv_chart_set_point_count(s_sf_chart, 20);
            lv_chart_set_range(s_sf_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
            lv_obj_set_style_bg_color(s_sf_chart, COLOR_BAR_BG, 0);
            lv_obj_set_style_border_width(s_sf_chart, 0, 0);
            lv_obj_set_style_line_color(s_sf_chart, lv_color_hex(0x2A3A4A), LV_PART_MAIN);
            s_sf_ser = lv_chart_add_series(s_sf_chart, COLOR_GREEN, LV_CHART_AXIS_PRIMARY_Y);
            lv_chart_set_all_value(s_sf_chart, s_sf_ser, 0);
            lv_obj_set_style_size(s_sf_chart, 0, 0, LV_PART_INDICATOR);
            lv_obj_set_style_line_width(s_sf_chart, 2, LV_PART_ITEMS);
        }
    }

    /* Placeholder shown while the camera feed is privacy-hidden */
    s_sf_cam_off_lbl = lv_label_create(tab_safety);
    lv_label_set_text(s_sf_cam_off_lbl,
                      LV_SYMBOL_EYE_CLOSE "  Camera off\nShows on cry or breathing alert");
    lv_obj_set_style_text_color(s_sf_cam_off_lbl, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s_sf_cam_off_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(s_sf_cam_off_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_sf_cam_off_lbl, GAP + 100, SF_CAM_Y + SF_CAM_H / 2 - 20);

    /* Play/Stop + Mute + Cam buttons — placed just below the camera feed */
    {
        lv_coord_t ctrl_h = 36;
        lv_coord_t ctrl_y = SF_CAM_Y + SF_CAM_H + 2;  /* 404 — 2 px below camera */
        lv_coord_t bbtn_w = (SF_CAM_W - GAP * 4) / 3;  /* ~152, three across */

        /* Shared button style helper */
#define MAKE_BTN(parent, bx, by, bw, bh, cb, lbl_ptr, lbl_init) \
        do { \
            lv_obj_t *_btn = lv_obj_create(parent); \
            lv_obj_set_pos(_btn, (bx), (by)); \
            lv_obj_set_size(_btn, (bw), (bh)); \
            lv_obj_set_style_bg_color(_btn, COLOR_PANEL, 0); \
            lv_obj_set_style_bg_opa(_btn, LV_OPA_COVER, 0); \
            lv_obj_set_style_border_color(_btn, COLOR_ACCENT, 0); \
            lv_obj_set_style_border_width(_btn, 1, 0); \
            lv_obj_set_style_border_opa(_btn, LV_OPA_50, 0); \
            lv_obj_set_style_radius(_btn, 8, 0); \
            lv_obj_clear_flag(_btn, LV_OBJ_FLAG_SCROLLABLE); \
            lv_obj_add_flag(_btn, LV_OBJ_FLAG_CLICKABLE); \
            lv_obj_add_event_cb(_btn, (cb), LV_EVENT_CLICKED, NULL); \
            *(lbl_ptr) = lv_label_create(_btn); \
            lv_label_set_text(*(lbl_ptr), (lbl_init)); \
            lv_obj_set_style_text_color(*(lbl_ptr), COLOR_TEXT_PRI, 0); \
            lv_obj_set_style_text_font(*(lbl_ptr), &lv_font_montserrat_14, 0); \
            lv_obj_align(*(lbl_ptr), LV_ALIGN_CENTER, 0, 0); \
        } while (0)

        MAKE_BTN(tab_safety,
                 GAP, ctrl_y, bbtn_w, ctrl_h,
                 on_play_click, &s_sf_play_lbl,
                 LV_SYMBOL_PLAY "  Play Lullaby");

        MAKE_BTN(tab_safety,
                 GAP * 2 + bbtn_w, ctrl_y, bbtn_w, ctrl_h,
                 on_mute_click, &s_sf_mute_lbl,
                 LV_SYMBOL_MUTE "  Mute");

        MAKE_BTN(tab_safety,
                 GAP * 3 + bbtn_w * 2, ctrl_y, bbtn_w, ctrl_h,
                 on_sf_cam_click, &s_sf_cam_btn_lbl,
                 LV_SYMBOL_EYE_CLOSE "  Cam: Auto");

#undef MAKE_BTN
    }

    /* on_tab_changed handles error-label updates for non-RUNNING cases */
    lv_obj_add_event_cb(root_tv, on_tab_changed, LV_EVENT_VALUE_CHANGED, NULL);

    /* ── Update timer ─────────────────────────────────────────────────── */
    lv_timer_create(radar_ui_update_cb, 200, NULL);

    printf("[UI] radar_ui_create done  cam_status=%d\r\n",
           (int)camera_get_status());
    fflush(stdout);
}
