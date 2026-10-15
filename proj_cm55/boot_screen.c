/*******************************************************************************
* File Name: boot_screen.c
*
* Description: Full-screen Mouser boot overlay shown at cold boot.
*
*  Timeline (5 seconds total):
*    0 ms        — overlay covers main UI
*    200 ms      — "MOUSER" starts fading in; welcome sound starts
*    500–1500 ms — "ELECTRONICS" fades in
*    700–1700 ms — tagline fades in
*    4500 ms     — overlay begins 500 ms fade-out
*    5000 ms     — overlay deleted via second timer
*******************************************************************************/

#include "boot_screen.h"
#include "welcome_sound_data.h"
#include "buzzer.h"
#include "lvgl.h"
#include <stdio.h>

static lv_obj_t *s_overlay = NULL;

/*────────────────────────────────────────────────────────────────────────────*/
static void anim_opa_cb(void *var, int32_t val)
{
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)val, 0);
}

/* Fires at 200 ms — same moment MOUSER starts fading in */
static void sound_timer_cb(lv_timer_t *t)
{
    (void)t;
    buzzer_play_pcm(welcome_sound_pcm, welcome_sound_samples, false);
    printf("[Boot] welcome sound started\r\n");
}

/*────────────────────────────────────────────────────────────────────────────*/
static void delete_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (s_overlay != NULL)
    {
        lv_obj_del(s_overlay);
        s_overlay = NULL;
        printf("[Boot] overlay deleted — main UI visible\r\n");
    }
}

static void dismiss_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (s_overlay == NULL) return;

    /* Fade out the overlay over 500 ms */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_overlay);
    lv_anim_set_exec_cb(&a, anim_opa_cb);
    lv_anim_set_values(&a, 255, 0);
    lv_anim_set_duration(&a, 500);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_start(&a);

    /* Delete 120 ms after animation ends */
    lv_timer_t *del = lv_timer_create(delete_timer_cb, 620, NULL);
    lv_timer_set_repeat_count(del, 1);
}

/*────────────────────────────────────────────────────────────────────────────*/
void boot_screen_show(void)
{
    lv_obj_t *scr = lv_scr_act();

    /* Full-screen dark overlay on top of main UI */
    s_overlay = lv_obj_create(scr);
    lv_obj_set_pos(s_overlay, 0, 0);
    lv_obj_set_size(s_overlay, 800, 480);
    lv_obj_set_style_bg_color(s_overlay, lv_color_hex(0x0D1B2A), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_overlay, 0, 0);
    lv_obj_set_style_radius(s_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_overlay, 0, 0);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(s_overlay);

    /* "MOUSER" in Mouser orange */
    lv_obj_t *lbl_m = lv_label_create(s_overlay);
    lv_label_set_text(lbl_m, "MOUSER");
    lv_obj_set_style_text_font(lbl_m, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_m, lv_color_hex(0x0070BB), 0);
    lv_obj_set_style_opa(lbl_m, LV_OPA_TRANSP, 0);
    lv_obj_align(lbl_m, LV_ALIGN_CENTER, 0, -20);

    /* Tagline */
    lv_obj_t *lbl_t = lv_label_create(s_overlay);
    lv_label_set_text(lbl_t, "Powered by Infineon");
    lv_obj_set_style_text_font(lbl_t, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_t, lv_color_hex(0xAABBCC), 0);
    lv_obj_set_style_opa(lbl_t, LV_OPA_TRANSP, 0);
    lv_obj_align(lbl_t, LV_ALIGN_BOTTOM_MID, 0, -28);

    /* Animate "MOUSER": fade in (delay 200 ms, 1200 ms) */
    {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, lbl_m);
        lv_anim_set_exec_cb(&a, anim_opa_cb);
        lv_anim_set_values(&a, 0, 255);
        lv_anim_set_duration(&a, 1200);
        lv_anim_set_delay(&a, 200);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_start(&a);
    }

    /* Animate tagline: fade in (delay 700 ms, 1000 ms) */
    {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, lbl_t);
        lv_anim_set_exec_cb(&a, anim_opa_cb);
        lv_anim_set_values(&a, 0, 255);
        lv_anim_set_duration(&a, 1000);
        lv_anim_set_delay(&a, 700);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_start(&a);
    }

    /* Welcome sound — delayed 200 ms to align with MOUSER fade-in */
    lv_timer_t *st = lv_timer_create(sound_timer_cb, 200, NULL);
    lv_timer_set_repeat_count(st, 1);

    /* Dismiss at 4500 ms; a second timer deletes the overlay at 5120 ms */
    lv_timer_t *t = lv_timer_create(dismiss_timer_cb, 4500, NULL);
    lv_timer_set_repeat_count(t, 1);

    printf("[Boot] boot screen showing\r\n");
}
