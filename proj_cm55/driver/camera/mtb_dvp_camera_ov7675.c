/******************************************************************************
 * \file mtb_dvp_camera_ov7675.c
 *
 * \brief
 *     This file contains the functions for interacting with the
 *     OV7675 camera using DVP interface.
 *
 ********************************************************************************
 * \copyright
 * Copyright 2025 Cypress Semiconductor Corporation
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *******************************************************************************/

#include "mtb_dvp_camera_ov7675.h"
#include "cy_mcwdt.h"
#include "cybsp.h"

#include <stdio.h>


/*******************************************************************************
 * Macros
 ******************************************************************************/
#define I2C_CMD_DELAY_US    (2000)
#define BUFFER_COUNT        (2)
#define NUM_BYTES           (1)
#define I2C_TIMEOUT         (100)


/*******************************************************************************
* Global variables
*******************************************************************************/
__attribute__((section(".cy_sharedmem")))
__attribute((used))    uint8_t line_buffer[BUFFER_COUNT][LINE_SIZE];
static bool row_buffer_flag = false;
static bool frame_buffer_flag = false;
// static uint8_t* image_frames = NULL;
static uint8_t* image_frames_0 = NULL;
static uint8_t* image_frames_1 = NULL;
static bool* _frame_ready = NULL;
static bool* _active_frame = NULL;
static cy_stc_scb_i2c_context_t* camera_i2c_context = NULL;


/*******************************************************************************
 * Camera settings
 ******************************************************************************/
/* OV7675 resolution options */
const ov7675_resolution_t OV7675_VGA = { 640, 480 };
const ov7675_resolution_t OV7675_QVGA = { 320, 240 };

/* Night mode initialization structure data */
const ov7675_output_format_config_t OV7675_FORMAT_RGB565 = { 0x04, 0xd0 };
const ov7675_output_format_config_t OV7675_FORMAT_RGB555 = { 0x04, 0xf0 };

/* resolution initialization structure data */
const ov7675_resolution_config_t OV7675_RESOLUTION_VGA = { 0x00 };    /*!< 640 x 480 */

const ov7675_resolution_config_t OV7675_RESOLUTION_QVGA = { 0x10 };    /*!< 320 x 240 */

/* Special effects configuration initialization structure data */
const ov7675_windowing_config_t OV7675_WINDOW_VGA = { 0x36, 0x13, 0x01, 0x0a, 0x02, 0x7a };
const ov7675_windowing_config_t OV7675_WINDOW_QVGA = { 0x80, 0x15, 0x03, 0x00, 0x03, 0x7b };

/* Frame rate initialization structure data */

const ov7675_frame_rate_config_t OV7675_30FPS_24MHZ_XCLK = { 0x00, 0x4a };
const ov7675_frame_rate_config_t OV7675_15FPS_24MHZ_XCLK = { 0x01, 0x4a };
const ov7675_frame_rate_config_t OV7675_05FPS_24MHZ_XCLK = { 0x04, 0x4a };

ov7675_handler_t s_Ov7675CameraHandler =
{
    .i2cBase       = CYBSP_I2C_CAM_CONTROLLER_HW,
    .i2cDeviceAddr = OV7675_I2C_ADDR
};

ov7675_config_t s_Ov7675CameraConfig =
{
    .outputFormat   = (ov7675_output_format_config_t*)&OV7675_FORMAT_RGB565,
    .resolution     =
    {
        .width      = OV7675_FRAME_WIDTH,
        .height     = OV7675_FRAME_HEIGHT,
    },
    .frameRate      = (ov7675_frame_rate_config_t*)&OV7675_05FPS_24MHZ_XCLK
};

/******************************************************************************/


/*******************************************************************************
* Function Prototypes
*******************************************************************************/
cy_rslt_t mtb_dvp_cam_configure(void);
cy_rslt_t mtb_dvp_cam_dma_init(void);
cy_rslt_t mtb_dvp_cam_start_xclk(void);
void mtb_dvp_cam_intr_init(void);
void mtb_dvp_cam_intr_callback(void);
cy_rslt_t mtb_dvp_cam_axi_dmac_init(void);

cy_rslt_t master_write(CySCB_Type* base, cy_stc_scb_i2c_context_t* context,
                       uint16_t dev_addr,
                       const uint8_t* data, uint16_t size, uint32_t timeout,
                       bool send_stop);
cy_rslt_t master_read(CySCB_Type* base, cy_stc_scb_i2c_context_t* context,
                      uint16_t dev_addr,
                      uint8_t* data, uint16_t size, uint32_t timeout, bool send_stop);
cy_rslt_t I2C_Read_OV7675_Reg(CySCB_Type* base, uint8_t device_addr, uint8_t regAddr,
                              uint8_t* rxBuff, uint32_t rxSize);
cy_rslt_t I2C_Write_OV7675_Reg(CySCB_Type* base, uint8_t device_addr, uint8_t regAddr,
                               uint8_t val);
ov7675_status_t OV7675_ModifyReg(ov7675_handler_t* handle, uint8_t reg, uint8_t clrMask,
                                 uint8_t val);
ov7675_status_t OV7675_Init(ov7675_handler_t* handle, const ov7675_config_t* config);
ov7675_status_t OV7675_Configure(ov7675_handler_t* handle, const ov7675_config_t* config);
ov7675_status_t OV7675_OutputFormat(ov7675_handler_t* handle,
                                    const ov7675_output_format_config_t* outputFormatConfig);
ov7675_status_t OV7675_Resolution(ov7675_handler_t* handle, const ov7675_resolution_t* resolution);
ov7675_status_t OV7675_SetWindow(ov7675_handler_t* handle,
                                 const ov7675_windowing_config_t* windowingConfig);
ov7675_status_t OV7675_FrameRateAdjustment(ov7675_handler_t* handle,
                                           const ov7675_frame_rate_config_t* frameRateConfig);


/*******************************************************************************
* Function Name: master_write
*******************************************************************************/
cy_rslt_t master_write(CySCB_Type* base, cy_stc_scb_i2c_context_t* context,
                       uint16_t dev_addr,
                       const uint8_t* data, uint16_t size, uint32_t timeout,
                       bool send_stop)
{
    cy_rslt_t status = ((*context).state == CY_SCB_I2C_IDLE)
        ? Cy_SCB_I2C_MasterSendStart(base, dev_addr, CY_SCB_I2C_WRITE_XFER, timeout, context)
        : Cy_SCB_I2C_MasterSendReStart(base, dev_addr, CY_SCB_I2C_WRITE_XFER, timeout, context);

    if (status == CY_SCB_I2C_SUCCESS)
    {
        while (size > 0)
        {
            status = Cy_SCB_I2C_MasterWriteByte(base, *data, timeout, context);
            if (status != CY_SCB_I2C_SUCCESS)
            {
                break;
            }
            --size;
            ++data;
        }
    }

    if (send_stop)
    {
        /* SCB in I2C mode is very time sensitive.               */
        /* In practice we have to request STOP after each block, */
        /* otherwise it may break the transmission               */
        Cy_SCB_I2C_MasterSendStop(base, timeout, context);
    }

    return status;
}


/*******************************************************************************
* Function Name: master_read
*******************************************************************************/
cy_rslt_t master_read(CySCB_Type* base, cy_stc_scb_i2c_context_t* context,
                      uint16_t dev_addr,
                      uint8_t* data, uint16_t size, uint32_t timeout, bool send_stop)
{
    cy_en_scb_i2c_command_t ack = CY_SCB_I2C_ACK;

    /* Start transaction, send dev_addr */
    cy_rslt_t status = (*context).state == CY_SCB_I2C_IDLE
        ? Cy_SCB_I2C_MasterSendStart(base, dev_addr, CY_SCB_I2C_READ_XFER, timeout, context)
        : Cy_SCB_I2C_MasterSendReStart(base, dev_addr, CY_SCB_I2C_READ_XFER, timeout, context);
    if (status == CY_SCB_I2C_SUCCESS)
    {
        while (size > 0)
        {
            if (size == 1)
            {
                ack = CY_SCB_I2C_NAK;
            }
            status = Cy_SCB_I2C_MasterReadByte(base, ack, (uint8_t*)data, timeout, context);
            if (status != CY_SCB_I2C_SUCCESS)
            {
                break;
            }
            --size;
            ++data;
        }
    }

    if (send_stop)
    {
        /* SCB in I2C mode is very time sensitive.               */
        /* In practice we have to request STOP after each block, */
        /* otherwise it may break the transmission               */
        Cy_SCB_I2C_MasterSendStop(base, timeout, context);
    }
    return status;
}


/*******************************************************************************
* Function Name: delay
*******************************************************************************/
static void delay(int microseconds)
{
    Cy_SysLib_DelayUs(microseconds);
}


/*******************************************************************************
* Function Name: I2C_Read_OV7675_Reg
*******************************************************************************/
cy_rslt_t I2C_Read_OV7675_Reg(CySCB_Type* base, uint8_t device_addr, uint8_t regAddr,
                              uint8_t* rxBuff, uint32_t rxSize)
{
    cy_rslt_t status;
    uint8_t RegBuffer[1];
    RegBuffer[0] = regAddr;
    status = master_write(base, camera_i2c_context, (uint16_t)device_addr, RegBuffer,
                          NUM_BYTES, I2C_TIMEOUT, true);
    if (CY_RSLT_SUCCESS == status)
    {
        status = master_read(base, camera_i2c_context, (uint16_t)device_addr, rxBuff,
                             (uint16_t)rxSize, I2C_TIMEOUT, true);
        if (CY_RSLT_SUCCESS != status)
        {
            return status;
        }
    }
    return status;
}


/*******************************************************************************
* Function Name: I2C_Write_OV7675_Reg
*******************************************************************************/
cy_rslt_t I2C_Write_OV7675_Reg(CySCB_Type* base, uint8_t device_addr, uint8_t regAddr,
                               uint8_t val)
{
    cy_rslt_t status;
    uint8_t abWriteBuffer[2];
    abWriteBuffer[0] = regAddr;
    abWriteBuffer[1] = val;
    status = master_write(base, camera_i2c_context, (uint16_t)device_addr,
                          abWriteBuffer, BUFFER_COUNT, I2C_TIMEOUT, true);
    return status;
}


/*******************************************************************************
* Function Name: OV7675_ModifyReg
*******************************************************************************/
ov7675_status_t OV7675_ModifyReg(ov7675_handler_t* handle, uint8_t reg, uint8_t clrMask,
                                 uint8_t val)
{
    ov7675_status_t status = kStatus_OV7675_Success;

    cy_rslt_t ret_val = 0U;
    uint8_t reg_val = 0U;
    uint8_t tmp = 0U;
    ret_val = I2C_Read_OV7675_Reg(handle->i2cBase, handle->i2cDeviceAddr, reg, &reg_val, NUM_BYTES);
    if (ret_val != kStatus_OV7675_Success)
    {
        return kStatus_OV7675_Fail;
    }
    tmp = ~clrMask;
    reg_val &= tmp;
    reg_val |= val;
    ret_val = I2C_Write_OV7675_Reg(handle->i2cBase, handle->i2cDeviceAddr, reg, reg_val);
    if (ret_val != kStatus_OV7675_Success)
    {
        return kStatus_OV7675_Fail;
    }
    return status;
}

/* Registers */
#define REG_GAIN	0x00	/* Gain lower 8 bits (rest in vref) */
#define REG_BLUE	0x01	/* blue gain */
#define REG_RED		0x02	/* red gain */
#define REG_VREF	0x03	/* Pieces of GAIN, VSTART, VSTOP */
#define REG_COM1	0x04	/* Control 1 */
#define  COM1_CCIR656	  0x40  /* CCIR656 enable */
#define REG_BAVE	0x05	/* U/B Average level */
#define REG_GbAVE	0x06	/* Y/Gb Average level */
#define REG_AECHH	0x07	/* AEC MS 5 bits */
#define REG_RAVE	0x08	/* V/R Average level */
#define REG_COM2	0x09	/* Control 2 */
#define  COM2_SSLEEP	  0x10	/* Soft sleep mode */
#define REG_PID		0x0a	/* Product ID MSB */
#define REG_VER		0x0b	/* Product ID LSB */
#define REG_COM3	0x0c	/* Control 3 */
#define  COM3_SWAP	  0x40	  /* Byte swap */
#define  COM3_SCALEEN	  0x08	  /* Enable scaling */
#define  COM3_DCWEN	  0x04	  /* Enable downsamp/crop/window */
#define REG_COM4	0x0d	/* Control 4 */
#define REG_COM5	0x0e	/* All "reserved" */
#define REG_COM6	0x0f	/* Control 6 */
#define REG_AECH	0x10	/* More bits of AEC value */
#define REG_CLKRC	0x11	/* Clocl control */
#define   CLK_EXT	  0x40	  /* Use external clock directly */
#define   CLK_SCALE	  0x3f	  /* Mask for internal clock scale */
#define REG_COM7	0x12	/* Control 7 */
#define   COM7_RESET	  0x80	  /* Register reset */
#define   COM7_FMT_MASK	  0x38
#define   COM7_FMT_VGA	  0x00
#define	  COM7_FMT_CIF	  0x20	  /* CIF format */
#define   COM7_FMT_QVGA	  0x10	  /* QVGA format */
#define   COM7_FMT_QCIF	  0x08	  /* QCIF format */
#define	  COM7_RGB	  0x04	  /* bits 0 and 2 - RGB format */
#define	  COM7_YUV	  0x00	  /* YUV */
#define	  COM7_BAYER	  0x01	  /* Bayer format */
#define	  COM7_PBAYER	  0x05	  /* "Processed bayer" */
#define REG_COM8	0x13	/* Control 8 */
#define   COM8_FASTAEC	  0x80	  /* Enable fast AGC/AEC */
#define   COM8_AECSTEP	  0x40	  /* Unlimited AEC step size */
#define   COM8_BFILT	  0x20	  /* Band filter enable */
#define   COM8_AGC	  0x04	  /* Auto gain enable */
#define   COM8_AWB	  0x02	  /* White balance enable */
#define   COM8_AEC	  0x01	  /* Auto exposure enable */
#define REG_COM9	0x14	/* Control 9  - gain ceiling */
#define REG_COM10	0x15	/* Control 10 */
#define   COM10_HSYNC	  0x40	  /* HSYNC instead of HREF */
#define   COM10_PCLK_HB	  0x20	  /* Suppress PCLK on horiz blank */
#define   COM10_HREF_REV  0x08	  /* Reverse HREF */
#define   COM10_VS_LEAD	  0x04	  /* VSYNC on clock leading edge */
#define   COM10_VS_NEG	  0x02	  /* VSYNC negative */
#define   COM10_HS_NEG	  0x01	  /* HSYNC negative */
#define REG_HSTART	0x17	/* Horiz start high bits */
#define REG_HSTOP	0x18	/* Horiz stop high bits */
#define REG_VSTART	0x19	/* Vert start high bits */
#define REG_VSTOP	0x1a	/* Vert stop high bits */
#define REG_PSHFT	0x1b	/* Pixel delay after HREF */
#define REG_MIDH	0x1c	/* Manuf. ID high */
#define REG_MIDL	0x1d	/* Manuf. ID low */
#define REG_MVFP	0x1e	/* Mirror / vflip */
#define   MVFP_MIRROR	  0x20	  /* Mirror image */
#define   MVFP_FLIP	  0x10	  /* Vertical flip */

#define REG_AEW		0x24	/* AGC upper limit */
#define REG_AEB		0x25	/* AGC lower limit */
#define REG_VPT		0x26	/* AGC/AEC fast mode op region */
#define REG_HSYST	0x30	/* HSYNC rising edge delay */
#define REG_HSYEN	0x31	/* HSYNC falling edge delay */
#define REG_HREF	0x32	/* HREF pieces */
#define REG_TSLB	0x3a	/* lots of stuff */
#define   TSLB_YLAST	  0x04	  /* UYVY or VYUY - see com13 */
#define REG_COM11	0x3b	/* Control 11 */
#define   COM11_NIGHT	  0x80	  /* NIght mode enable */
#define   COM11_NMFR	  0x60	  /* Two bit NM frame rate */
#define   COM11_HZAUTO	  0x10	  /* Auto detect 50/60 Hz */
#define	  COM11_50HZ	  0x08	  /* Manual 50Hz select */
#define   COM11_EXP	  0x02
#define REG_COM12	0x3c	/* Control 12 */
#define   COM12_HREF	  0x80	  /* HREF always */
#define REG_COM13	0x3d	/* Control 13 */
#define   COM13_GAMMA	  0x80	  /* Gamma enable */
#define	  COM13_UVSAT	  0x40	  /* UV saturation auto adjustment */
#define   COM13_UVSWAP	  0x01	  /* V before U - w/TSLB */
#define REG_COM14	0x3e	/* Control 14 */
#define   COM14_DCWEN	  0x10	  /* DCW/PCLK-scale enable */
#define REG_EDGE	0x3f	/* Edge enhancement factor */
#define REG_COM15	0x40	/* Control 15 */
#define   COM15_R10F0	  0x00	  /* Data range 10 to F0 */
#define	  COM15_R01FE	  0x80	  /*            01 to FE */
#define   COM15_R00FF	  0xc0	  /*            00 to FF */
#define   COM15_RGB565	  0x10	  /* RGB565 output */
#define   COM15_RGB555	  0x30	  /* RGB555 output */
#define REG_COM16	0x41	/* Control 16 */
#define   COM16_AWBGAIN   0x08	  /* AWB gain enable */
#define REG_COM17	0x42	/* Control 17 */
#define   COM17_AECWIN	  0xc0	  /* AEC window - must match COM4 */
#define   COM17_CBAR	  0x08	  /* DSP Color bar */

/*
 * This matrix defines how the colors are generated, must be
 * tweaked to adjust hue and saturation.
 *
 * Order: v-red, v-green, v-blue, u-red, u-green, u-blue
 *
 * They are nine-bit signed quantities, with the sign bit
 * stored in 0x58.  Sign for v-red is bit 0, and up from there.
 */
#define	REG_CMATRIX_BASE 0x4f
#define   CMATRIX_LEN 6
#define REG_CMATRIX_SIGN 0x58


#define REG_BRIGHT	0x55	/* Brightness */
#define REG_CONTRAS	0x56	/* Contrast control */

#define REG_GFIX	0x69	/* Fix gain control */

#define REG_DBLV	0x6b	/* PLL control an debugging */
#define   DBLV_BYPASS	  0x0a	  /* Bypass PLL */
#define   DBLV_X4	  0x4a	  /* clock x4 */
#define   DBLV_X6	  0x8a	  /* clock x6 */
#define   DBLV_X8	  0xca	  /* clock x8 */

#define REG_SCALING_XSC	0x70	/* Test pattern and horizontal scale factor */
#define   TEST_PATTTERN_0 0x80
#define REG_SCALING_YSC	0x71	/* Test pattern and vertical scale factor */
#define   TEST_PATTTERN_1 0x80

#define REG_REG76	0x76	/* OV's name */
#define   R76_BLKPCOR	  0x80	  /* Black pixel correction enable */
#define   R76_WHTPCOR	  0x40	  /* White pixel correction enable */

#define REG_RGB444	0x8c	/* RGB 444 control */
#define   R444_ENABLE	  0x02	  /* Turn on RGB444, overrides 5x5 */
#define   R444_RGBX	  0x01	  /* Empty nibble at end */

#define REG_HAECC1	0x9f	/* Hist AEC/AGC control 1 */
#define REG_HAECC2	0xa0	/* Hist AEC/AGC control 2 */

#define REG_BD50MAX	0xa5	/* 50hz banding step limit */
#define REG_HAECC3	0xa6	/* Hist AEC/AGC control 3 */
#define REG_HAECC4	0xa7	/* Hist AEC/AGC control 4 */
#define REG_HAECC5	0xa8	/* Hist AEC/AGC control 5 */
#define REG_HAECC6	0xa9	/* Hist AEC/AGC control 6 */
#define REG_HAECC7	0xaa	/* Hist AEC/AGC control 7 */
#define REG_BD60MAX	0xab	/* 60hz banding step limit */

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

static struct regval_list ov7670_default_regs[] = {
	{ REG_COM7, COM7_RESET },
/*
 * Clock scale: 3 = 15fps
 *              2 = 20fps
 *              1 = 30fps
 */
//	{ REG_CLKRC, 0x1 },	/* OV: clock scale (30 fps) */
//	{ REG_TSLB,  0x04 },	/* OV */
//	{ REG_COM7, 0 },	/* VGA */
	/*
	 * Set the hardware window.  These values from OV don't entirely
	 * make sense - hstop is less than hstart.  But they work...
	 */
	{ REG_HSTART, 0x13 },	{ REG_HSTOP, 0x01 },
	{ REG_HREF, 0xb6 },	{ REG_VSTART, 0x02 },
	{ REG_VSTOP, 0x7a },	{ REG_VREF, 0x0a },

	{ REG_COM3, 0 },	{ REG_COM14, 0 },
	/* Mystery scaling numbers */
	{ REG_SCALING_XSC, 0x3a },
	{ REG_SCALING_YSC, 0x35 },
	{ 0x72, 0x11 },		{ 0x73, 0xf0 },
	{ 0xa2, 0x02 },		{ REG_COM10, 0x0 },

	/* Gamma curve values */
	{ 0x7a, 0x20 },		{ 0x7b, 0x10 },
	{ 0x7c, 0x1e },		{ 0x7d, 0x35 },
	{ 0x7e, 0x5a },		{ 0x7f, 0x69 },
	{ 0x80, 0x76 },		{ 0x81, 0x80 },
	{ 0x82, 0x88 },		{ 0x83, 0x8f },
	{ 0x84, 0x96 },		{ 0x85, 0xa3 },
	{ 0x86, 0xaf },		{ 0x87, 0xc4 },
	{ 0x88, 0xd7 },		{ 0x89, 0xe8 },

	/* AGC and AEC parameters.  Note we start by disabling those features,
	   then turn them only after tweaking the values. */
	{ REG_COM8, COM8_FASTAEC | COM8_AECSTEP | COM8_BFILT },
	{ REG_GAIN, 0 },	{ REG_AECH, 0 },
	{ REG_COM4, 0x40 }, /* magic reserved bit */
	{ REG_COM9, 0x18 }, /* 4x gain + magic rsvd bit */
	{ REG_BD50MAX, 0x05 },	{ REG_BD60MAX, 0x07 },
	{ REG_AEW, 0x95 },	{ REG_AEB, 0x33 },
	{ REG_VPT, 0xe3 },	{ REG_HAECC1, 0x78 },
	{ REG_HAECC2, 0x68 },	{ 0xa1, 0x03 }, /* magic */
	{ REG_HAECC3, 0xd8 },	{ REG_HAECC4, 0xd8 },
	{ REG_HAECC5, 0xf0 },	{ REG_HAECC6, 0x90 },
	{ REG_HAECC7, 0x94 },
	{ REG_COM8, COM8_FASTAEC|COM8_AECSTEP|COM8_BFILT|COM8_AGC|COM8_AEC },

	/* Almost all of these are magic "reserved" values.  */
	{ REG_COM5, 0x61 },	{ REG_COM6, 0x4b },
	{ 0x16, 0x02 },		{ REG_MVFP, 0x07 },
	{ 0x21, 0x02 },		{ 0x22, 0x91 },
	{ 0x29, 0x07 },		{ 0x33, 0x0b },
	{ 0x35, 0x0b },		{ 0x37, 0x1d },
	{ 0x38, 0x71 },		{ 0x39, 0x2a },
	{ REG_COM12, 0x78 },	{ 0x4d, 0x40 },
	{ 0x4e, 0x20 },		{ REG_GFIX, 0 },
	{ 0x6b, 0x4a },		{ 0x74, 0x10 },
	{ 0x8d, 0x4f },		{ 0x8e, 0 },
	{ 0x8f, 0 },		{ 0x90, 0 },
	{ 0x91, 0 },		{ 0x96, 0 },
	{ 0x9a, 0 },		{ 0xb0, 0x84 },
	{ 0xb1, 0x0c },		{ 0xb2, 0x0e },
	{ 0xb3, 0x82 },		{ 0xb8, 0x0a },

	/* More reserved magic, some of which tweaks white balance */
	{ 0x43, 0x0a },		{ 0x44, 0xf0 },
	{ 0x45, 0x34 },		{ 0x46, 0x58 },
	{ 0x47, 0x28 },		{ 0x48, 0x3a },
	{ 0x59, 0x88 },		{ 0x5a, 0x88 },
	{ 0x5b, 0x44 },		{ 0x5c, 0x67 },
	{ 0x5d, 0x49 },		{ 0x5e, 0x0e },
	{ 0x6c, 0x0a },		{ 0x6d, 0x55 },
	{ 0x6e, 0x11 },		{ 0x6f, 0x9f }, /* "9e for advance AWB" */
	{ 0x6a, 0x40 },		{ REG_BLUE, 0x40 },
	{ REG_RED, 0x60 },
	{ REG_COM8, COM8_FASTAEC|COM8_AECSTEP|COM8_BFILT|COM8_AGC|COM8_AEC|COM8_AWB },

	/* Matrix coefficients */
	{ 0x4f, 0x80 },		{ 0x50, 0x80 },
	{ 0x51, 0 },		{ 0x52, 0x22 },
	{ 0x53, 0x5e },		{ 0x54, 0x80 },
	{ 0x58, 0x9e },

	{ REG_COM16, COM16_AWBGAIN },	{ REG_EDGE, 0 },
	{ 0x75, 0x05 },		{ 0x76, 0xe1 },
	{ 0x4c, 0 },		{ 0x77, 0x01 },
	{ REG_COM13, 0xc3 },	{ 0x4b, 0x09 },
	{ 0xc9, 0x60 },		{ REG_COM16, 0x38 },
	{ 0x56, 0x40 },

	{ 0x34, 0x11 },		{ REG_COM11, COM11_EXP|COM11_HZAUTO },
	{ 0xa4, 0x88 },		{ 0x96, 0 },
	{ 0x97, 0x30 },		{ 0x98, 0x20 },
	{ 0x99, 0x30 },		{ 0x9a, 0x84 },
	{ 0x9b, 0x29 },		{ 0x9c, 0x03 },
	{ 0x9d, 0x4c },		{ 0x9e, 0x3f },
	{ 0x78, 0x04 },

	/* Extra-weird stuff.  Some sort of multiplexor register */
	{ 0x79, 0x01 },		{ 0xc8, 0xf0 },
	{ 0x79, 0x0f },		{ 0xc8, 0x00 },
	{ 0x79, 0x10 },		{ 0xc8, 0x7e },
	{ 0x79, 0x0a },		{ 0xc8, 0x80 },
	{ 0x79, 0x0b },		{ 0xc8, 0x01 },
	{ 0x79, 0x0c },		{ 0xc8, 0x0f },
	{ 0x79, 0x0d },		{ 0xc8, 0x20 },
	{ 0x79, 0x09 },		{ 0xc8, 0x80 },
	{ 0x79, 0x02 },		{ 0xc8, 0xc0 },
	{ 0x79, 0x03 },		{ 0xc8, 0x40 },
	{ 0x79, 0x05 },		{ 0xc8, 0x30 },
	{ 0x79, 0x26 },

	{ 0xff, 0xff },	/* END MARKER */
};

static int write_conf_array(ov7675_handler_t* handle, struct regval_list *vals)
{
	while (vals->reg_num != 0xff || vals->value != 0xff) {

		if (I2C_Write_OV7675_Reg(handle->i2cBase, handle->i2cDeviceAddr, vals->reg_num,
				vals->value) != 0) return -1;
		delay(I2C_CMD_DELAY_US);
		vals++;
	}
	return 0;
}


/*******************************************************************************
* Function Name: OV7675_Init
*******************************************************************************/
ov7675_status_t OV7675_Init(ov7675_handler_t* handle, const ov7675_config_t* config)
{
    ov7675_status_t status = kStatus_OV7675_Success;

    uint8_t u8TempVal0, u8TempVal1;

    /* Reset Device */
    I2C_Write_OV7675_Reg(handle->i2cBase, handle->i2cDeviceAddr, OV7675_COM7_REG,
                         OV7675_COM7_RESET_MASK);
    /* wait for at least 1ms */
    delay(I2C_CMD_DELAY_US);

    if (write_conf_array(handle, ov7670_default_regs) != 0) return kStatus_OV7675_Fail;

    /* Read product ID number MSB */
    if (I2C_Read_OV7675_Reg(handle->i2cBase, handle->i2cDeviceAddr, OV7675_PID_REG, &u8TempVal0,
                            NUM_BYTES) != kStatus_OV7675_Success)
    {
        return kStatus_OV7675_Fail;
    }
    /* Read product ID number MSB */
    if (I2C_Read_OV7675_Reg(handle->i2cBase, handle->i2cDeviceAddr, OV7675_VER_REG, &u8TempVal1,
                            NUM_BYTES) != kStatus_OV7675_Success)
    {
        return kStatus_OV7675_Fail;
    }
    if ((u8TempVal0 != OV7675_PID_NUM) && (u8TempVal1 != OV7675_VER_NUM))
    {
        return kStatus_OV7675_Fail;
    }

    /* Read OV7675_MIDH_REG */
    uint8_t manufacturer_high = 0;
	if (I2C_Read_OV7675_Reg(handle->i2cBase, handle->i2cDeviceAddr, OV7675_MIDH_REG, &manufacturer_high,
							NUM_BYTES) != kStatus_OV7675_Success)
	{
		return kStatus_OV7675_Fail;
	}
	printf("manufacturer_high = 0x%x\r\n", manufacturer_high);

    /* NULL pointer means default setting. */
    if (config != NULL)
    {
        I2C_Write_OV7675_Reg(handle->i2cBase, handle->i2cDeviceAddr, OV7675_COM10_REG,
                             OV7675_COM10_PCLK_HB_MASK | OV7675_COM10_HREF_REV_MASK);
        OV7675_Configure(handle, config);

//        I2C_Write_OV7675_Reg(handle->i2cBase, handle->i2cDeviceAddr, 0x71,
//        		0x80);
    }


    return status;
}


/*******************************************************************************
* Function Name: OV7675_Configure
*******************************************************************************/
ov7675_status_t OV7675_Configure(ov7675_handler_t* handle, const ov7675_config_t* config)
{
    ov7675_status_t status = kStatus_OV7675_Success;

    ov7675_windowing_config_t* windowConfig;
    OV7675_OutputFormat(handle, config->outputFormat);
    OV7675_Resolution(handle, &config->resolution);
    uint32_t u32TempResolution = (config->resolution.width);
    u32TempResolution = (u32TempResolution << 16U);
    u32TempResolution |= config->resolution.height;

    switch (u32TempResolution)
    {
        case 0x028001e0:
            windowConfig = (ov7675_windowing_config_t*)&OV7675_WINDOW_VGA;
            break;

        case 0x014000f0:
            windowConfig = (ov7675_windowing_config_t*)&OV7675_WINDOW_QVGA;
            break;

        default:
            return kStatus_OV7675_Fail; /* not supported resolution */
    }
    OV7675_SetWindow(handle, windowConfig);
    OV7675_FrameRateAdjustment(handle, config->frameRate);

    return status;
}


/*******************************************************************************
* Function Name: OV7675_OutputFormat
*******************************************************************************/
ov7675_status_t OV7675_OutputFormat(ov7675_handler_t* handle,
                                    const ov7675_output_format_config_t* outputFormatConfig)
{
    ov7675_status_t status = kStatus_OV7675_Success;

    OV7675_ModifyReg(handle, OV7675_COM7_REG, OV7675_COM7_OUT_FMT_BITS, outputFormatConfig->com7);
    OV7675_ModifyReg(handle, OV7675_COM15_REG, OV7675_COM15_RGB_FMT_BITS,
                     outputFormatConfig->com15);

    /* Mirror the image */
    OV7675_ModifyReg(handle, OV7675_MVFP_REG, OV7675_MVFP_MIRROR_MASK, OV7675_MVFP_MIRROR_MASK);

    return status;
}


/*******************************************************************************
 * Function Name: OV7675_Resolution
 ********************************************************************************/
ov7675_status_t OV7675_Resolution(ov7675_handler_t* handle, const ov7675_resolution_t* resolution)
{
    ov7675_status_t status = kStatus_OV7675_Success;

    ov7675_resolution_config_t* resolution_config;
    uint32_t u32TempResolution;
    u32TempResolution = resolution->width;
    u32TempResolution = u32TempResolution << 16;
    u32TempResolution |= resolution->height;
    switch (u32TempResolution)
    {
        case 0x028001e0:
            resolution_config = (ov7675_resolution_config_t*)&OV7675_RESOLUTION_VGA;
            break;

        case 0x014000f0:
            resolution_config = (ov7675_resolution_config_t*)&OV7675_RESOLUTION_QVGA;
            break;

        default:
            return kStatus_OV7675_Fail; /*!< not supported resolution */
    }

    OV7675_ModifyReg(handle, OV7675_COM7_REG, OV7675_COM7_FMT_QVGA_MASK, resolution_config->com7);

    /* Automatically set output window after resolution change */
    OV7675_ModifyReg(handle, OV7675_TSLB_REG, OV7675_TSLB_AUTO_WIN_MASK, BIT_SET);

    return status;
}


/*******************************************************************************
* Function Name: OV7675_SetWindow
*******************************************************************************/
ov7675_status_t OV7675_SetWindow(ov7675_handler_t* handle,
                                 const ov7675_windowing_config_t* windowingConfig)
{
    ov7675_status_t status = kStatus_OV7675_Success;

    OV7675_ModifyReg(handle, OV7675_TSLB_REG, OV7675_TSLB_AUTO_WIN_MASK, BIT_CLEAR);

    I2C_Write_OV7675_Reg(handle->i2cBase, handle->i2cDeviceAddr, OV7675_HREF_REG,
                         windowingConfig->href);
    I2C_Write_OV7675_Reg(handle->i2cBase, handle->i2cDeviceAddr, OV7675_HSTART_REG,
                         windowingConfig->hstart);
    I2C_Write_OV7675_Reg(handle->i2cBase, handle->i2cDeviceAddr, OV7675_HSTOP_REG,
                         windowingConfig->hstop);
    I2C_Write_OV7675_Reg(handle->i2cBase, handle->i2cDeviceAddr, OV7675_VREF_REG,
                         windowingConfig->vref);
    I2C_Write_OV7675_Reg(handle->i2cBase, handle->i2cDeviceAddr, OV7675_VSTART_REG,
                         windowingConfig->vstart);
    I2C_Write_OV7675_Reg(handle->i2cBase, handle->i2cDeviceAddr, OV7675_VSTOP_REG,
                         windowingConfig->vstop);

    return status;
}


/*******************************************************************************
* Function Name: OV7675_FrameRateAdjustment
*******************************************************************************/
ov7675_status_t OV7675_FrameRateAdjustment(ov7675_handler_t* handle,
                                           const ov7675_frame_rate_config_t* frameRateConfig)
{
    ov7675_status_t status = kStatus_OV7675_Success;

    I2C_Write_OV7675_Reg(handle->i2cBase, handle->i2cDeviceAddr, OV7675_CLKRC_REG,
                         frameRateConfig->clkrc);
    delay(I2C_CMD_DELAY_US);
    I2C_Write_OV7675_Reg(handle->i2cBase, handle->i2cDeviceAddr, OV7675_DBLV_REG,
                         frameRateConfig->dblv);
    delay(I2C_CMD_DELAY_US);

    return status;
}


/*****************************************************************************
* Function Name: mtb_dvp_cam_configure
*****************************************************************************/
cy_rslt_t mtb_dvp_cam_configure(void)
{
    ov7675_status_t ov7675Status;

    /* Configure the OV7675 camera */
    ov7675Status = OV7675_Init(&s_Ov7675CameraHandler, (ov7675_config_t*)&s_Ov7675CameraConfig);

    return (cy_rslt_t)ov7675Status;
}


/*****************************************************************************
* Function Name: mtb_dvp_cam_start_xclk
*****************************************************************************/
cy_rslt_t mtb_dvp_cam_start_xclk(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    result = Cy_TCPWM_PWM_Init(CYBSP_PWM_DVP_CAM_CTRL_HW, CYBSP_PWM_DVP_CAM_CTRL_NUM,
                               &CYBSP_PWM_DVP_CAM_CTRL_config);
    if (CY_TCPWM_SUCCESS != result)
    {
        return result;
    }

    Cy_TCPWM_PWM_Enable(CYBSP_PWM_DVP_CAM_CTRL_HW, CYBSP_PWM_DVP_CAM_CTRL_NUM);
    /* Start the TCPWM block *   */
    Cy_TCPWM_TriggerStart_Single(CYBSP_PWM_DVP_CAM_CTRL_HW, CYBSP_PWM_DVP_CAM_CTRL_NUM);

    return result;
}


int counter_visr = 0;

/*****************************************************************************
* Function Name: mtb_dvp_cam_intr_callback
*****************************************************************************/
void mtb_dvp_cam_intr_callback(void)
{
    if (Cy_GPIO_GetInterruptStatus(CYBSP_DVP_CAM_HREF_PORT, CYBSP_DVP_CAM_HREF_NUM))
    {
        Cy_GPIO_ClearInterrupt(CYBSP_DVP_CAM_HREF_PORT, CYBSP_DVP_CAM_HREF_NUM);
        NVIC_ClearPendingIRQ(CYBSP_DVP_CAM_HREF_IRQ);

        Cy_DMA_Descriptor_SetDstAddress(&CYBSP_DMA_DVP_CAM_CONTROLLER_Descriptor_0,
                                        (uint8_t*)line_buffer[row_buffer_flag]);
        #if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT != 0)
        SCB_InvalidateDCache_by_Addr((uint32_t*)line_buffer[row_buffer_flag],
                                     sizeof(line_buffer[row_buffer_flag]));
        SCB_CleanDCache_by_Addr((uint32_t*)&CYBSP_DMA_DVP_CAM_CONTROLLER_Descriptor_0,
                                sizeof(CYBSP_DMA_DVP_CAM_CONTROLLER_Descriptor_0));
        #endif
        Cy_DMA_Channel_Enable(CYBSP_DMA_DVP_CAM_CONTROLLER_HW,
                              CYBSP_DMA_DVP_CAM_CONTROLLER_CHANNEL);

        row_buffer_flag = !row_buffer_flag;
		counter_visr ++;
    }

    if (Cy_GPIO_GetInterruptStatus(CYBSP_DVP_CAM_VSYNC_PORT, CYBSP_DVP_CAM_VSYNC_NUM))
    {
        Cy_GPIO_ClearInterrupt(CYBSP_DVP_CAM_VSYNC_PORT, CYBSP_DVP_CAM_VSYNC_NUM);
        NVIC_ClearPendingIRQ(CYBSP_DVP_CAM_VSYNC_IRQ);

		uint32_t * dest = (uint32_t*)image_frames_0;
		if (frame_buffer_flag) dest = (uint32_t*)image_frames_1;

        Cy_AXIDMAC_Descriptor_SetDstAddress(&CYBSP_AXIDMAC_DVP_CAM_CONTROLLER_Descriptor_0, dest);
		
        #if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT != 0)
        SCB_InvalidateDCache_by_Addr(dest, OV7675_MEMORY_BUFFER_SIZE);
        SCB_CleanDCache_by_Addr((uint32_t*)&CYBSP_AXIDMAC_DVP_CAM_CONTROLLER_Descriptor_0,
                                sizeof(CYBSP_AXIDMAC_DVP_CAM_CONTROLLER_Descriptor_0));
        #endif
        Cy_AXIDMAC_Channel_Enable(CYBSP_AXIDMAC_DVP_CAM_CONTROLLER_HW,
                                  CYBSP_AXIDMAC_DVP_CAM_CONTROLLER_CHANNEL);

        frame_buffer_flag = !frame_buffer_flag;
        *_active_frame = frame_buffer_flag;

        if (counter_visr >= 240)
        {
            *_frame_ready = true;
        }
        counter_visr = 0;
    }
}


/*****************************************************************************
* Function Name: mtb_dvp_cam_intr_init
*****************************************************************************/
void mtb_dvp_cam_intr_init(void)
{
    /* Interrupt config structure */
    cy_stc_sysint_t tIntrCfg =
    {
        .intrSrc      = CYBSP_DVP_CAM_HREF_IRQ, /* Interrupt source */
        .intrPriority = 7UL /* Interrupt priority */
    };
    /* Initialize the interrupt */
    Cy_GPIO_ClearInterrupt(CYBSP_DVP_CAM_HREF_PORT, CYBSP_DVP_CAM_HREF_PIN);
    NVIC_ClearPendingIRQ(CYBSP_DVP_CAM_HREF_IRQ);
    Cy_GPIO_ClearInterrupt(CYBSP_DVP_CAM_VSYNC_PORT, CYBSP_DVP_CAM_VSYNC_PIN);
    NVIC_ClearPendingIRQ(CYBSP_DVP_CAM_VSYNC_IRQ);

    Cy_SysInt_Init(&tIntrCfg, &mtb_dvp_cam_intr_callback);

    /* Enable the interrupt. Since both HREF and VSYNC pins have the same
     * interrupt source, enabling any one is sufficient */
    NVIC_EnableIRQ(CYBSP_DVP_CAM_HREF_IRQ);
}


/*****************************************************************************
* Function Name: mtb_dvp_cam_dma_init
*****************************************************************************/
cy_rslt_t mtb_dvp_cam_dma_init(void)
{
    cy_rslt_t status;

    status = Cy_DMA_Descriptor_Init(&CYBSP_DMA_DVP_CAM_CONTROLLER_Descriptor_0,
                                    &CYBSP_DMA_DVP_CAM_CONTROLLER_Descriptor_0_config);
    if (status != CY_DMA_SUCCESS)
    {
        return status;
    }

    status = Cy_DMA_Channel_Init(CYBSP_DMA_DVP_CAM_CONTROLLER_HW,
                                 CYBSP_DMA_DVP_CAM_CONTROLLER_CHANNEL,
                                 &CYBSP_DMA_DVP_CAM_CONTROLLER_channelConfig);
    if (status != CY_DMA_SUCCESS)
    {
        return status;
    }

    memset(line_buffer[0], 0x0, LINE_SIZE);
    memset(line_buffer[1], 0x0, LINE_SIZE);

    Cy_DMA_Descriptor_SetSrcAddress(&CYBSP_DMA_DVP_CAM_CONTROLLER_Descriptor_0,
                                    (void*)&GPIO_PRT16->IN);

    Cy_DMA_Descriptor_SetDstAddress(&CYBSP_DMA_DVP_CAM_CONTROLLER_Descriptor_0,
                                    line_buffer[row_buffer_flag]);

    #if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT != 0)
    SCB_CleanDCache_by_Addr((uint32_t*)&GPIO_PRT16->IN, sizeof(GPIO_PRT16->IN));
    SCB_CleanDCache_by_Addr((uint32_t*)&CYBSP_DMA_DVP_CAM_CONTROLLER_Descriptor_0,
                            sizeof(CYBSP_DMA_DVP_CAM_CONTROLLER_Descriptor_0));
    #endif

    Cy_DMA_Enable(CYBSP_DMA_DVP_CAM_CONTROLLER_HW);
    Cy_DMA_Channel_Enable(CYBSP_DMA_DVP_CAM_CONTROLLER_HW, CYBSP_DMA_DVP_CAM_CONTROLLER_CHANNEL);

    return status;
}


/*****************************************************************************
* Function Name: mtb_dvp_cam_axi_dmac_init
*****************************************************************************/
cy_rslt_t mtb_dvp_cam_axi_dmac_init(void)
{
    cy_rslt_t status;

    status = Cy_AXIDMAC_Descriptor_Init(&CYBSP_AXIDMAC_DVP_CAM_CONTROLLER_Descriptor_0,
                                        &CYBSP_AXIDMAC_DVP_CAM_CONTROLLER_Descriptor_0_config);
    if (CY_AXIDMAC_SUCCESS != status)
    {
        return status;
    }

    status = Cy_AXIDMAC_Channel_Init(CYBSP_AXIDMAC_DVP_CAM_CONTROLLER_HW,
                                     CYBSP_AXIDMAC_DVP_CAM_CONTROLLER_CHANNEL,
                                     &CYBSP_AXIDMAC_DVP_CAM_CONTROLLER_channelConfig);
    if (CY_AXIDMAC_SUCCESS != status)
    {
        return status;
    }

    Cy_AXIDMAC_Descriptor_SetSrcAddress(&CYBSP_AXIDMAC_DVP_CAM_CONTROLLER_Descriptor_0,
                                        (uint32_t*)line_buffer);

    Cy_AXIDMAC_Descriptor_SetDstAddress(&CYBSP_AXIDMAC_DVP_CAM_CONTROLLER_Descriptor_0,
                                        (uint32_t*)image_frames_0);

    #if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT != 0)
    SCB_CleanDCache_by_Addr((uint32_t*)&line_buffer, sizeof(line_buffer));
    SCB_CleanDCache_by_Addr((uint32_t*)&CYBSP_AXIDMAC_DVP_CAM_CONTROLLER_Descriptor_0,
                            sizeof(CYBSP_AXIDMAC_DVP_CAM_CONTROLLER_Descriptor_0));
    #endif

    Cy_AXIDMAC_Enable(CYBSP_AXIDMAC_DVP_CAM_CONTROLLER_HW);
    Cy_AXIDMAC_Channel_Enable(CYBSP_AXIDMAC_DVP_CAM_CONTROLLER_HW,
                              CYBSP_AXIDMAC_DVP_CAM_CONTROLLER_CHANNEL);

    return status;
}


/*****************************************************************************
* Function Name: mtb_dvp_cam_ov7675_init
*****************************************************************************/
cy_rslt_t mtb_dvp_cam_ov7675_init(uint8_t* buffer_0, uint8_t* buffer_1, cy_stc_scb_i2c_context_t* i2c_instance,
                                  bool* _frame_ready_flag, bool* _active_frame_flag)
{
    cy_rslt_t status = CY_RSLT_SUCCESS;
    // image_frames = buffer;
	image_frames_0 = buffer_0;
	image_frames_1 = buffer_1;
    _frame_ready = _frame_ready_flag;
    _active_frame = _active_frame_flag;

    camera_i2c_context = i2c_instance;
    CY_ASSERT(NULL != i2c_instance);

    /* Initialize DMA DW*/
    status = mtb_dvp_cam_dma_init();
    if (CY_RSLT_SUCCESS != status)
    {
        return status;
    }

    /* Initialize AXIDMAC*/
    status = mtb_dvp_cam_axi_dmac_init();
    if (CY_RSLT_SUCCESS != status)
    {
        return status;
    }

    /* Initialize HREF interrupt */
    mtb_dvp_cam_intr_init();

    /* Initialize and start XCLK*/
    status = mtb_dvp_cam_start_xclk();
    if (CY_RSLT_SUCCESS != status)
    {
        return status;
    }

    /* Initialize the camera with set configurations */
    status = mtb_dvp_cam_configure();
    if (CY_RSLT_SUCCESS != status)
    {
        return status;
    }

    return status;
}
