/* USB (UVC) camera backend — kit rev *A ships a 0.3 MP USB camera module on
 * the USB-C J2 connector instead of the OV7675 DVP module (J14 is depopulated).
 * Frames arrive as uncompressed YUYV 320x240 via emUSB-Host's video class
 * driver; camera_task converts them to RGB565 so the existing display /
 * frame-diff consumers are unchanged. */

#include "camera_sensor.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include "USBH.h"
#include "USBH_VIDEO.h"

#include <string.h>
#include <stdio.h>
#include "cy_utils.h"

/* Known cameras — VID/PID gates only the preferred frame interval; unknown
 * UVC cameras still work if they offer uncompressed 320x240. */
#define HBV_CAM_0P3_VID     (0x058FU)   /* 0.3 MP module shipped with kit rev *A */
#define HBV_CAM_0P3_PID     (0x5608U)
#define HBV_CAM_2P0_VID     (0x05A3U)
#define HBV_CAM_2P0_PID     (0x2B01U)
#define LOGI_C920_VID       (0x046DU)
#define LOGI_C920_PID       (0x08E5U)
#define LOGI_C920E_PID      (0x08B6U)

#define FRAME_INTERVAL_30FPS_0P3  (333332U)  /* 100 ns units — 0.3 MP cam */
#define FRAME_INTERVAL_30FPS      (333333U)  /* 2 MP cam */
#define FRAME_INTERVAL_10FPS      (1000000U) /* Logitech C920/C920e */

#define NUM_IMAGE_BUFFERS   (2U)

#define TASK_PRIO_USBH_MAIN (configMAX_PRIORITIES - 2)
#define TASK_PRIO_USBH_ISR  (configMAX_PRIORITIES - 1)
#define USBH_TASK_STACK_WORDS (2048U)

/* YUYV 422 accumulation buffers — filled by memcpy in USBH task context.
 * Same 2 bytes/pixel as RGB565, so sizes match the old DVP layout. */
CY_SECTION(".cy_socmem_camera_dma") static uint8_t __attribute__((aligned(64)))
    s_cam_buf0[CAMERA_FRAME_SIZE];
CY_SECTION(".cy_socmem_camera_dma") static uint8_t __attribute__((aligned(64)))
    s_cam_buf1[CAMERA_FRAME_SIZE];
/* RGB565 output — written only by camera_task; VGLite GPU reads via AXI. */
CY_SECTION(".cy_socmem_camera_dma") static uint8_t __attribute__((aligned(64)))
    s_cam_display_buf[CAMERA_FRAME_SIZE];

typedef struct
{
    volatile uint32_t num_bytes;
    volatile uint8_t  buff_ready;
} video_buffer_t;

static uint8_t                 *s_yuv_bufs[NUM_IMAGE_BUFFERS];
static video_buffer_t           s_image_buff[NUM_IMAGE_BUFFERS];
static volatile uint8_t         s_last_buffer  = 0;

static SemaphoreHandle_t        s_mutex        = NULL;
static SemaphoreHandle_t        s_frame_sem    = NULL;  /* callback -> camera_task */
static QueueHandle_t            s_video_mbox   = NULL;  /* device-ready events     */
static QueueHandle_t            s_state_mbox   = NULL;  /* disconnect/error events */
static USBH_NOTIFICATION_HOOK   s_usbh_hook;
static volatile uint8_t         s_device_connected = 0;

static const uint8_t           *s_latest       = NULL;
static volatile uint32_t        s_frame_count  = 0;

camera_status_t s_cam_status = CAMERA_STATUS_IDLE;

camera_status_t camera_get_status(void)      { return s_cam_status; }
uint32_t        camera_get_frame_count(void) { return s_frame_count; }

const uint8_t *camera_get_frame(void)
{
    const uint8_t *p = NULL;
    if (s_mutex != NULL)
    {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        p = s_latest;
        xSemaphoreGive(s_mutex);
    }
    return p;
}

/* Called once at startup.  The USB camera enumerates asynchronously, so this
 * only prepares buffers; status goes to RUNNING immediately (= subsystem
 * ready) so the UI spawns camera_task, which brings up the USB host stack.
 * The UI shows "Starting camera..." until the first frame arrives. */
void camera_hw_init(void)
{
    s_cam_status = CAMERA_STATUS_STARTING;

    s_mutex     = xSemaphoreCreateMutex();
    s_frame_sem = xSemaphoreCreateBinary();
    if ((s_mutex == NULL) || (s_frame_sem == NULL))
    {
        s_cam_status = CAMERA_STATUS_ERROR;
        return;
    }

    s_yuv_bufs[0] = s_cam_buf0;
    s_yuv_bufs[1] = s_cam_buf1;
    memset(s_cam_buf0,        0, CAMERA_FRAME_SIZE);
    memset(s_cam_buf1,        0, CAMERA_FRAME_SIZE);
    memset(s_cam_display_buf, 0, CAMERA_FRAME_SIZE);

    printf("[Camera] USB camera mode — buf0=%p buf1=%p disp=%p\r\n",
           (void *)s_cam_buf0, (void *)s_cam_buf1, (void *)s_cam_display_buf);
    fflush(stdout);

    s_cam_status = CAMERA_STATUS_RUNNING;
}

/*******************************************************************************
* emUSB-Host callbacks (run in USBH task context, not ISR)
*******************************************************************************/
static void usb_add_remove_cb(void *p_context, U8 dev_index, USBH_DEVICE_EVENT event)
{
    (void)p_context;
    uint8_t ev;

    switch (event)
    {
        case USBH_DEVICE_EVENT_ADD:
            s_device_connected = 1;
            printf("[Camera] USB video device added (index %d)\r\n", dev_index);
            for (uint32_t i = 0; i < NUM_IMAGE_BUFFERS; i++)
            {
                s_image_buff[i].buff_ready = 0;
                s_image_buff[i].num_bytes  = 0;
            }
            s_last_buffer = 0;
            __DMB();
            (void)xQueueSend(s_video_mbox, &dev_index, 0);
            break;

        case USBH_DEVICE_EVENT_REMOVE:
            s_device_connected = 0;
            printf("[Camera] USB video device removed\r\n");
            for (uint32_t i = 0; i < NUM_IMAGE_BUFFERS; i++)
            {
                s_image_buff[i].buff_ready = 0;
                s_image_buff[i].num_bytes  = 0;
            }
            __DMB();
            ev = 0xFF;
            (void)xQueueSend(s_state_mbox, &ev, 0);
            break;

        default:
            break;
    }
}

static void usb_video_data_cb(USBH_VIDEO_DEVICE_HANDLE h_device,
                              USBH_VIDEO_STREAM_HANDLE h_stream,
                              USBH_STATUS status, const U8 *p_data,
                              unsigned num_bytes, U32 flags,
                              void *p_user_data_context)
{
    (void)h_device;
    (void)p_user_data_context;

    static size_t  frame_bytes      = 0;
    static uint8_t throw_away_frame = 0;
    static uint8_t current_buffer   = 0;
    static uint32_t error_count     = 0;

    if (status == USBH_STATUS_SUCCESS)
    {
        error_count = 0;
        bool end_of_frame = (flags & USBH_UVC_END_OF_FRAME) == USBH_UVC_END_OF_FRAME;

        if (frame_bytes + num_bytes > CAMERA_FRAME_SIZE)
        {
            throw_away_frame = 1;
            frame_bytes      = 0;
        }

        if (throw_away_frame)
        {
            if (end_of_frame)
            {
                frame_bytes = 0;
                /* Stay in throw-away mode while the next buffer is still busy */
                throw_away_frame = s_image_buff[current_buffer].buff_ready;
            }
            USBH_VIDEO_Ack(h_stream);
            return;
        }

        memcpy(s_yuv_bufs[current_buffer] + frame_bytes, p_data, num_bytes);
        frame_bytes += num_bytes;

        if (end_of_frame)
        {
            if (frame_bytes == CAMERA_FRAME_SIZE)
            {
                s_last_buffer = current_buffer;
                s_image_buff[current_buffer].buff_ready = 1;
                s_image_buff[current_buffer].num_bytes  = frame_bytes;
                __DMB();

                current_buffer   = (current_buffer + 1) % NUM_IMAGE_BUFFERS;
                throw_away_frame = s_image_buff[current_buffer].buff_ready;
                xSemaphoreGive(s_frame_sem);
            }
            /* else: short frame — discard silently */
            frame_bytes = 0;
        }
        USBH_VIDEO_Ack(h_stream);
    }
    else
    {
        error_count++;
        if ((error_count % 100U) == 1U)
        {
            printf("[Camera] USB transfer error %s (count %lu)\r\n",
                   USBH_GetStatusStr(status), (unsigned long)error_count);
        }

        if ((status == USBH_STATUS_DEVICE_REMOVED) || (error_count >= 1000U))
        {
            uint8_t ev = 0xFE;
            (void)xQueueSend(s_state_mbox, &ev, 0);
            error_count = 0;
        }
        else
        {
            I8 stopped;
            if ((USBH_VIDEO_GetStreamState(h_stream, &stopped) == USBH_STATUS_SUCCESS)
                && (stopped == 1))
            {
                (void)USBH_VIDEO_RestartStream(h_stream);
            }
        }
        frame_bytes      = 0;
        throw_away_frame = 0;
        USBH_VIDEO_Ack(h_stream);
    }
}

/*******************************************************************************
* YUYV 422 -> RGB565 (BT.601, integer approximation)
*******************************************************************************/
static inline uint16_t yuv_to_rgb565(int32_t y, int32_t cr_r, int32_t cb_g,
                                     int32_t cr_g, int32_t cb_b)
{
    int32_t r = y + cr_r;
    int32_t g = y - cb_g - cr_g;
    int32_t b = y + cb_b;
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (b < 0) b = 0; else if (b > 255) b = 255;
    return (uint16_t)(((r & 0xF8U) << 8) | ((g & 0xFCU) << 3) | (b >> 3));
}

static void yuyv_to_rgb565(const uint8_t *src, uint16_t *dst, uint32_t pixels)
{
    for (uint32_t i = 0; i < pixels; i += 2U)
    {
        int32_t y0 = src[0];
        int32_t u  = (int32_t)src[1] - 128;
        int32_t y1 = src[2];
        int32_t v  = (int32_t)src[3] - 128;
        src += 4;

        int32_t cr_r = (359 * v) >> 8;             /* 1.402  * V */
        int32_t cb_g = (88 * u) >> 8;              /* 0.344  * U */
        int32_t cr_g = (183 * v) >> 8;             /* 0.714  * V */
        int32_t cb_b = (454 * u) >> 8;             /* 1.772  * U */

        *dst++ = yuv_to_rgb565(y0, cr_r, cb_g, cr_g, cb_b);
        *dst++ = yuv_to_rgb565(y1, cr_r, cb_g, cr_g, cb_b);
    }
}

/*******************************************************************************
* Stream setup + frame pump (camera_task context)
*******************************************************************************/
static bool select_stream_config(USBH_VIDEO_DEVICE_HANDLE h_device,
                                 uint32_t preferred_interval,
                                 USBH_VIDEO_STREAM_CONFIG *cfg)
{
    USBH_VIDEO_INPUT_HEADER_INFO hdr;
    USBH_VIDEO_FORMAT_INFO format;
    USBH_VIDEO_FRAME_INFO  frame;
    bool found = false;

    if (USBH_VIDEO_GetInputHeader(h_device, &hdr) != USBH_STATUS_SUCCESS)
    {
        return false;
    }

    for (unsigned i = 0; (i < hdr.bNumFormats) && !found; i++)
    {
        if (USBH_VIDEO_GetFormatInfo(h_device, i, &format) != USBH_STATUS_SUCCESS)
        {
            continue;
        }
        if (format.FormatType != USBH_VIDEO_VS_FORMAT_UNCOMPRESSED)
        {
            continue;
        }
        unsigned n_frames = format.u.UncompressedFormat.bNumFrameDescriptors;
        for (unsigned j = 0; (j < n_frames) && !found; j++)
        {
            if (USBH_VIDEO_GetFrameInfo(h_device, i, j, &frame) != USBH_STATUS_SUCCESS)
            {
                continue;
            }
            printf("[Camera]   format %u frame %u: %ux%u\r\n",
                   i, j, frame.wWidth, frame.wHeight);
            if ((frame.wWidth == CAMERA_FRAME_W) && (frame.wHeight == CAMERA_FRAME_H)
                && (frame.bFrameIntervalType > 0))
            {
                cfg->FormatIdx        = i;
                cfg->FrameIdx         = j;
                cfg->FrameIntervalIdx = 1;  /* first listed interval as fallback */
                for (unsigned k = 0; k < frame.bFrameIntervalType; k++)
                {
                    if (frame.u.dwFrameInterval[k] == preferred_interval)
                    {
                        cfg->FrameIntervalIdx = k + 1;
                        break;
                    }
                }
                found = true;
            }
        }
    }
    return found;
}

static void run_video_device(U8 dev_index)
{
    USBH_VIDEO_DEVICE_HANDLE h_device;
    USBH_VIDEO_INTERFACE_INFO iface_info;
    USBH_VIDEO_STREAM_CONFIG stream_cfg;
    USBH_VIDEO_STREAM_HANDLE stream;
    uint32_t preferred_interval = FRAME_INTERVAL_30FPS_0P3;
    uint8_t  ev;

    memset(&h_device, 0, sizeof(h_device));
    memset(&stream,   0, sizeof(stream));

    if (USBH_VIDEO_Open(dev_index, &h_device) != USBH_STATUS_SUCCESS)
    {
        printf("[Camera] USBH_VIDEO_Open failed\r\n");
        return;
    }

    if (USBH_VIDEO_GetInterfaceInfo(h_device, &iface_info) == USBH_STATUS_SUCCESS)
    {
        printf("[Camera] VID=0x%04X PID=0x%04X\r\n",
               iface_info.VendorId, iface_info.ProductId);
        if (iface_info.VendorId == LOGI_C920_VID)
        {
            preferred_interval = FRAME_INTERVAL_10FPS;
        }
        else if ((iface_info.VendorId == HBV_CAM_2P0_VID)
                 && (iface_info.ProductId == HBV_CAM_2P0_PID))
        {
            preferred_interval = FRAME_INTERVAL_30FPS;
        }
    }

    memset(&stream_cfg, 0, sizeof(stream_cfg));
    if (!select_stream_config(h_device, preferred_interval, &stream_cfg))
    {
        printf("[Camera] no uncompressed %ux%u mode — camera unsupported\r\n",
               CAMERA_FRAME_W, CAMERA_FRAME_H);
        s_cam_status = CAMERA_STATUS_NOT_FOUND;
        while (s_device_connected)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        USBH_VIDEO_Close(h_device);
        return;
    }

    stream_cfg.Flags          = 0;
    stream_cfg.pfDataCallback = usb_video_data_cb;
    xQueueReset(s_state_mbox);

    if (USBH_VIDEO_OpenStream(h_device, &stream_cfg, &stream) != USBH_STATUS_SUCCESS)
    {
        printf("[Camera] USBH_VIDEO_OpenStream failed\r\n");
        USBH_VIDEO_Close(h_device);
        return;
    }

    printf("[Camera] USB camera streaming — %ux%u YUYV\r\n",
           CAMERA_FRAME_W, CAMERA_FRAME_H);
    s_cam_status = CAMERA_STATUS_RUNNING;

    uint32_t frames = 0;
    for (;;)
    {
        /* Convert any completed YUYV frame to RGB565 and publish it. */
        if (xSemaphoreTake(s_frame_sem, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            uint8_t idx = s_last_buffer;
            if (s_image_buff[idx].buff_ready)
            {
                yuyv_to_rgb565(s_yuv_bufs[idx], (uint16_t *)s_cam_display_buf,
                               CAMERA_FRAME_W * CAMERA_FRAME_H);
                s_image_buff[idx].buff_ready = 0;
                __DMB();

                if (frames == 0)
                {
                    printf("[Cam] First USB frame! buf=%p rgb=[%02x %02x %02x %02x]\r\n",
                           (void *)s_yuv_bufs[idx],
                           s_cam_display_buf[0], s_cam_display_buf[1],
                           s_cam_display_buf[2], s_cam_display_buf[3]);
                    fflush(stdout);
                }
                frames++;

                /* Flush D-cache so the VGLite GPU (AXI master, bypasses the
                   CPU cache) reads the freshly converted frame. */
                #if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT != 0)
                SCB_CleanDCache_by_Addr((uint32_t *)s_cam_display_buf,
                                        CAMERA_FRAME_SIZE);
                #endif

                xSemaphoreTake(s_mutex, portMAX_DELAY);
                s_latest = s_cam_display_buf;
                s_frame_count++;
                xSemaphoreGive(s_mutex);
            }
        }

        /* Disconnect / stream-error notification from USBH callbacks. */
        if (xQueueReceive(s_state_mbox, &ev, 0) == pdTRUE)
        {
            printf("[Camera] stream ended (event 0x%02X)\r\n", ev);
            break;
        }
    }

    USBH_VIDEO_Close(h_device);
}

/* USB host bring-up + device event loop.  Spawned by the UI exactly like the
 * old DVP frame-monitor task; never exits. */
void camera_task(void *arg)
{
    (void)arg;

    printf("[Cam] camera_task started (USB host mode)\r\n");
    fflush(stdout);

    if ((s_mutex == NULL) || (s_frame_sem == NULL))
    {
        printf("[Cam] camera_task exiting — hw_init failed\r\n");
        vTaskDelete(NULL);
        return;
    }

    s_video_mbox = xQueueCreate(4, sizeof(uint8_t));
    s_state_mbox = xQueueCreate(2, sizeof(uint8_t));
    if ((s_video_mbox == NULL) || (s_state_mbox == NULL))
    {
        s_cam_status = CAMERA_STATUS_ERROR;
        vTaskDelete(NULL);
        return;
    }

    USBH_Init();
    if (xTaskCreate((TaskFunction_t)USBH_Task, "USBH_Task",
                    USBH_TASK_STACK_WORDS, NULL,
                    TASK_PRIO_USBH_MAIN, NULL) != pdPASS
        || xTaskCreate((TaskFunction_t)USBH_ISRTask, "USBH_isr",
                       USBH_TASK_STACK_WORDS, NULL,
                       TASK_PRIO_USBH_ISR, NULL) != pdPASS)
    {
        printf("[Cam] USBH task creation failed\r\n");
        s_cam_status = CAMERA_STATUS_ERROR;
        vTaskDelete(NULL);
        return;
    }

    USBH_VIDEO_Init();
    USBH_VIDEO_AddNotification(&s_usbh_hook, usb_add_remove_cb, NULL);

    printf("[Cam] USB host ready — waiting for camera on J2\r\n");
    fflush(stdout);

    for (;;)
    {
        U8 dev_index;
        if (xQueueReceive(s_video_mbox, &dev_index, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            run_video_device(dev_index);
            /* Back to waiting: keep status RUNNING so the UI keeps polling;
               frame counter simply stops advancing until reconnect. */
            s_cam_status = CAMERA_STATUS_RUNNING;
        }
    }
}
