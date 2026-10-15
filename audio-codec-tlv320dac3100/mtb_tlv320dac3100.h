/***********************************************************************************************
 * \file mtb_tlv320dac3100.h
 *
 * \brief
 *     This file contains the function prototypes and constants used
 *     in mtb_tlv320dac3100.c. This driver is intended for the TLV320DAC3100 audio codec.
 *
 * \note The TLV320DAC3100 supports these I2C speed grades:
 *       * standard mode (100 kbit/s)
 *       * full speed (400 kbit/s)
 *
 ***************************************************************************************************
 * \copyright
 * Copyright 2025 Cypress Semiconductor Corporation (an Infineon company)
 *
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
 **************************************************************************************************/

#pragma once

#include <stdint.h>
#include "cy_result.h"
#include "mtb_hal_i2c.h"

#if defined(__cplusplus)
extern "C"
{
#endif /* #if defined(__cplusplus) */


#ifndef TLV320DAC3100_PROCESSING_BLOCK
    #define TLV320DAC3100_PROCESSING_BLOCK    (0x01)
#endif /* #ifndef TLV320DAC3100_PROCESSING_BLOCK */

#define TLV320DAC3100_PAGE_NUM_Pos                                    (8)
/* Register address mask */
#define TLV320DAC3100_REG_ADDR_Pos                                    (0)
/* I2C address of the codec */
#define MTB_TLV320DAC3100_I2C_ADDR                                    (0x18u)
/* Select Page 0 */
#define SELECT_TLV320DAC3100_PAGE_0                                   (0x00u)
/* Self Clearing SW Reset Register */
#define SELF_CLEARING_TLV320DAC3100_SW_RST                            (0x01u)
/* Config Codec PLL CLKIN PIN */
#define CFG_TLV320DAC3100_CODEC_PLL_CLKIN_PIN                         (0x00u)
/* Config D Value MSB Bit */
#define CFG_TLV320DAC3100_D_VALUE_MSB_BIT                             (0x00u)
/* Config PLL P and R Values and Power Down PLL */
#define CFG_TLV320DAC3100_PLL_P_R_VALS_POWER_DOWN                     (0x13u)
/* Enable processing block for adaptive filtering */
#define ENABLE_TLV320DAC3100_ADAPTIVE_FILTER                          (0x04u)
/* Volume Enable Control Register */
#define ENABLE_TLV320DAC3100_VOLUME_CONTROL                           (0x00u)
/* Headphone Output Power Down */
#define ENABLE_TLV320DAC3100_HEADPHONE_DRIVER                         (0x04u)
/* Set Driver Power-On Time and Driver Ramp Up Time */
#define SET_TLV320DAC3100_RAMP_UP_POWER_ON_TIME                       (0x4Eu)
/* Assign Mixer values to left and right channels */
#define ASSIGN_TLV320DAC3100_MIXER_VALUES_CH                          (0x44u)
/* Unmute the HPL Driver */
#define UNMUTE_TLV320DAC3100_HPL_DRIVER                               (0x06u)
/* Unmute the HPR Driver */
#define UNMUTE_TLV320DAC3100_HPR_DRIVER                               (0x06u)
/* Set Output Gain Unmute Class-D Driver and not apply gain */
#define SET_TLV320DAC3100_CLASS_D_SPEAKER_DRIVER                      (0x1Du)
/* HPL and HPR Power Up Driver */
#define POWER_UP_TLV320DAC3100_HPL_HPR_OP_DRIVER                      (0xC4u)
/* Class D Output Driver Power Up */
#define POWER_UP_TLV320DAC3100_CLASS_D_OP_DRIVER                      (0x86u)
/* Set Left Headphone gain to 18 db */
#define SET_TLV320DAC3100_HPL_GAIN_18_DB                              (0x92u)
/* Set Right Headphone gain to 18 db */
#define SET_TLV320DAC3100_HPR_GAIN_18_DB                              (0x92u)
/* Set Left Speaker gain to 18 db */
#define SET_TLV320DAC3100_SPK_GAIN_18_DB                              (0x92u)
/* Poer up left and right DAC channel and set DAC Data path */
#define POWER_UP_TLV320DAC3100_LEFT_RIGHT_CH_SET_DPATH                (0xD4u)
/* Set Left Volume gain to -22 db */
#define SET_TLV320DAC3100_LEFT_VOL_GAIN_22_DB                         (0xE4u)
/* Set Right Volume gain to 18 db */
#define SET_TLV320DAC3100_RIGHT_VOL_GAIN_22_DB                        (0xE4u)
/* Unmute the left and right channel */
#define UNMUTE_TLV320DAC3100_LEFT_RIGHT_CH                            (0x00u)
/* Power Down NDAC */
#define POWER_DOWN_TLV320DAC3100_NDAC                                 (0x80u)
/* Power Down MDAC */
#define POWER_DOWN_TLV320DAC3100_MDAC                                 (0x80u)
/* Result Failure Macro */
#define CY_RSLT_FAILURE                                               (0x01u)
/* Maximum DAC Clock Frequency */
#define MAX_TLV320DAC3100_CLK_FREQ_HZ                                 (49152000)
/* Maximum DAC Mode Clock Frequency */
#define MAX_TLV320DAC3100_MOD_CLK_FREQ_HZ                             (6200000)

/* Merging register address and page number used by @ref mtb_tlv320dac3100_write_byte */
#define TLV320DAC3100_PAGE_AND_REG_VAL(page, addr)     \
    (((page) << TLV320DAC3100_PAGE_NUM_Pos) | ((addr) << TLV320DAC3100_REG_ADDR_Pos)) \
/* Initialization failure error */
#define CY_RSLT_TLV320DAC3100_INIT_FAIL \
    (CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_BOARD_HARDWARE_TLV320DAC3100, 0))
/* Clock configuration failure error */
#define CY_RSLT_TLV320DAC3100_CLK_CONFIG_FAIL \
    (CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_BOARD_HARDWARE_TLV320DAC3100, 1))
/* Invalid parameter error */
#define CY_RSLT_TLV320DAC3100_BAD_PARAM_FAIL \
    (CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_BOARD_HARDWARE_TLV320DAC3100, 2))


/* Enumeration for the TLV320DAC3100 I2C Interface Register Addresses */
typedef enum
{
    /* Page 0 */
    TLV320DAC3100_P0_REG_PAGE_CTRL =
        TLV320DAC3100_PAGE_AND_REG_VAL(0, 0),    /**< 0x00: Page Control Register */

    TLV320DAC3100_P0_REG_SW_RST,                 /**< 0x01: Software Reset */
    TLV320DAC3100_P0_REG_CLK_GEN_MUX =
        TLV320DAC3100_PAGE_AND_REG_VAL(0, 4),    /**< 0x04: Clock-Gen muxing */

    TLV320DAC3100_P0_REG_CLK_PLL_PR,             /**< 0x05: PLL P and R Values */
    TLV320DAC3100_P0_REG_CLK_PLL_J,              /**< 0x06: PLL J-Value */
    TLV320DAC3100_P0_REG_CLK_PLL_D_MSB,          /**< 0x07: PLL D-Value MSB */
    TLV320DAC3100_P0_REG_NDAC_VAL =
        TLV320DAC3100_PAGE_AND_REG_VAL(0, 11),   /**< 0x0B: DAC NDAC_VAL */

    TLV320DAC3100_P0_REG_MDAC_VAL,               /**< 0x0C: DAC MDAC_VAL */
    TLV320DAC3100_P0_REG_DAC_DOSR_MSB,           /**< 0x0D: DAC DOSR_VAL MSB */
    TLV320DAC3100_P0_REG_DAC_DOSR_LSB,           /**< 0x0E: DAC DOSR_VAL LSB */
    TLV320DAC3100_P0_REG_CLKOUT_MUX =
        TLV320DAC3100_PAGE_AND_REG_VAL(0, 25),   /**< 0x19: CLKOUT MUX */

    TLV320DAC3100_P0_REG_CLKOUT_M_VAL,           /**< 0x1A: CLKOUT M_VAL */
    TLV320DAC3100_P0_REG_CODEC_INTERFACE_1,      /**< 0x1B: Codec Interface Control 1 */
    TLV320DAC3100_P0_REG_BCLK_N_VAL =
        TLV320DAC3100_PAGE_AND_REG_VAL(0, 30),   /**< 0x1E: BCLK N_VAL */

    TLV320DAC3100_P0_REG_CSI1_CTL,               /**< 0x1F: Codec Secondary Interface Control 1 */
    TLV320DAC3100_P0_REG_CSI2_CTL,               /**< 0x20: Codec Secondary Interface Control 2 */
    TLV320DAC3100_P0_REG_CSI3_CTL,               /**< 0x21: Codec Secondary Interface Control 3 */
    TLV320DAC3100_P0_REG_I2C_BUS_CND,            /**< 0x22: I2C Bus Condition */
    TLV320DAC3100_P0_REG_DAC_BSEL =
        TLV320DAC3100_PAGE_AND_REG_VAL(0, 60),   /**< 0x3C: DAC Processing Block Selection */

    TLV320DAC3100_P0_REG_DAC_DPATH =
        TLV320DAC3100_PAGE_AND_REG_VAL(0, 63),   /**< 0x3F: DAC Data-Path Setup */

    TLV320DAC3100_P0_REG_DAC_VOL_CTL,            /**< 0x40: DAC Volume Control */
    TLV320DAC3100_P0_REG_DAC_LVOL_CTL,           /**< 0x41: DAC Left Volume Control */
    TLV320DAC3100_P0_REG_DAC_RVOL_CTL,           /**< 0x42: DAC Right Volume Control */
    TLV320DAC3100_P0_REG_VOL_MICDET_CTL =
        TLV320DAC3100_PAGE_AND_REG_VAL(0, 116),  /**< 0x74: MICDET-Pin SAR ADC - Volume Control */

    TLV320DAC3100_P0_REG_VOL_MICDET_GAIN,        /**< 0x75: VOL/MICDET-Pin Gain */

    /* Page 1 */
    TLV320DAC3100_P1_REG_HP_DRV =
        TLV320DAC3100_PAGE_AND_REG_VAL(1, 31),  /**< 0x1F: Headphone Drivers */

    TLV320DAC3100_P1_REG_CLS_D_SPK_AMP,     /**< 0x20: Class-D Speaker Amplifier */
    TLV320DAC3100_P1_REG_HP_OUT_POP,        /**< 0x21: HP Output Drivers POP Removal Settings */
    TLV320DAC3100_P1_REG_PGA_PER_CTL,       /**< 0x22: Driver PGA Ramp-Down Period Control */
    TLV320DAC3100_P1_REG_DAC_LR_OUT_MIX,    /**< 0x23: DAC_L and DAC_R Output Mixer Routing */
    TLV320DAC3100_P1_REG_HP_LAV,            /**< 0x24: Left Analog Volume to HPL */
    TLV320DAC3100_P1_REG_HP_RAV,            /**< 0x25: Right Analog Volume to HPR */
    TLV320DAC3100_P1_REG_SP_LAV,            /**< 0x26: Left Analog Volume to SPK */
    TLV320DAC3100_P1_REG_SP_RESERVED,       /**< 0x27: Reserved */
    TLV320DAC3100_P1_REG_HPL_DRV,           /**< 0x28: HPL Driver */
    TLV320DAC3100_P1_REG_HPR_DRV,           /**< 0x29: HPR Driver */
    TLV320DAC3100_P1_REG_CLS_D_SPK_DRV,     /**< 0x2A: Class-D Speaker (SPK) Driver */
    TLV320DAC3100_P1_REG_HP_DRV_CTL =
        TLV320DAC3100_PAGE_AND_REG_VAL(1, 44), /**< 0x2C: HP Driver Control */

    /* Page 8 */
    TLV320DAC3100_P8_REG_DAC_COEF_RAM =
        TLV320DAC3100_PAGE_AND_REG_VAL(8, 1)   /**< 0x01: DAC Coefficient RAM Control */
} mtb_tlv320dac3100_reg_t;

typedef enum
{
    TLV320DAC3100_HP_AUDIO_OUTPUT = 0,
    TLV320DAC3100_SPK_AUDIO_OUTPUT
} mtb_tlv320dac3100_audio_output_t;

/* Enumeration for supported sample rates for DAC in the TLV320DAC3100 audio codec. */
typedef enum
{
    TLV320DAC3100_DAC_SAMPLE_RATE_192_KHZ    = 192000,   /**< 192 KHz sample rate */
    TLV320DAC3100_DAC_SAMPLE_RATE_176_4_KHZ  = 176400,   /**< 176.4 KHz sample rate */
    TLV320DAC3100_DAC_SAMPLE_RATE_96_KHZ     = 96000,    /**< 96 KHz sample rate */
    TLV320DAC3100_DAC_SAMPLE_RATE_88_2_KHZ   = 88200,    /**< 88.2 KHz sample rate */
    TLV320DAC3100_DAC_SAMPLE_RATE_48_KHZ     = 48000,    /**< 48 KHz sample rate */
    TLV320DAC3100_DAC_SAMPLE_RATE_44_1_KHZ   = 44100,    /**< 44.1 KHz sample rate */
    TLV320DAC3100_DAC_SAMPLE_RATE_32_KHZ     = 32000,    /**< 32 KHz sample rate */
    TLV320DAC3100_DAC_SAMPLE_RATE_24_KHZ     = 24000,    /**< 24 KHz sample rate */
    TLV320DAC3100_DAC_SAMPLE_RATE_22_05_KHZ  = 22050,    /**< 22.05 KHz sample rate */
    TLV320DAC3100_DAC_SAMPLE_RATE_16_KHZ     = 16000,    /**< 16 KHz sample rate */
    TLV320DAC3100_DAC_SAMPLE_RATE_12_KHZ     = 12000,    /**< 12 KHz sample rate */
    TLV320DAC3100_DAC_SAMPLE_RATE_11_025_KHZ = 11025,    /**< 11.025 KHz sample rate */
    TLV320DAC3100_DAC_SAMPLE_RATE_8_KHZ      = 8000      /**< 8 KHz sample rate */
} mtb_tlv320dac3100_dac_sample_rate_t;

/* Enumeration for supported I2S word sizes in Right Justified Mode for the TLV320DAC3100 audio
   codec. */
typedef enum
{
    TLV320DAC3100_I2S_WORD_SIZE_16 = 0,   /**< 16 bit word size */
    TLV320DAC3100_I2S_WORD_SIZE_20,       /**< 20 bit word size */
    TLV320DAC3100_I2S_WORD_SIZE_24,       /**< 24 bit word size */
    TLV320DAC3100_I2S_WORD_SIZE_32        /**< 32 bit word size */
} mtb_tlv320dac3100_i2s_word_size_t;


/******************************************************************************
* Function Name: mtb_tlv320dac3100_init
******************************************************************************
* Summary:
* Initialize the I2C communication with the audio codec and do basic configuration of
* the codec.
*
* Parameters:
*  i2c_inst    I2C instance to use for communicating with the TLV320DAC3100 audio codec.
*              The object should be pre-initialized by the user with help of corresponding driver's
*
* Return:
*  Status of execution
*
******************************************************************************/
cy_rslt_t mtb_tlv320dac3100_init(mtb_hal_i2c_t* i2c_inst);

/******************************************************************************
* Function Name: mtb_tlv320dac3100_free
******************************************************************************
* Summary:
* Frees up any resources allocated by the codec as part of
* \ref mtb_tlv320dac3100_init().
*
* Parameters: None
*
* Return: None
*
******************************************************************************/
void mtb_tlv320dac3100_free(void);

/******************************************************************************
* Function Name: mtb_tlv320dac3100_configure_clocking
******************************************************************************
* Summary:
* This function configure internal clock dividers to achieve desired sample rate.
*
* Parameters:
*  mclk_hz          Input MCLK frequency in HZ
*  sample_rate      Desired sample rate
*  word_length      I2S word length
*  audio_output     Desired audio output
*
* Return:
*  Status of execution
*
******************************************************************************/
cy_rslt_t mtb_tlv320dac3100_configure_clocking(uint32_t mclk_hz,
                                               mtb_tlv320dac3100_dac_sample_rate_t sample_rate,
                                               mtb_tlv320dac3100_i2s_word_size_t word_length,
                                               mtb_tlv320dac3100_audio_output_t audio_output);

/******************************************************************************
* Function Name: mtb_tlv320dac3100_adjust_speaker_output_volume
******************************************************************************
* Summary:
* This function updates the volume of both the left and right channels of the
* speaker output
*
* Parameters:
*  volume    Steps of 0.5dB
*
* Return:
*  Status of execution
*
******************************************************************************/
cy_rslt_t mtb_tlv320dac3100_adjust_speaker_output_volume(uint8_t volume);

/******************************************************************************
* Function Name: mtb_tlv320dac3100_adjust_headphone_output_volume
******************************************************************************
* Summary:
* This function updates the volume of both the left and right channels of the
* headphone output
*
* Parameters:
*  volume    Steps of 0.5dB
*
* Return:
*  Status of execution
*
******************************************************************************/
cy_rslt_t mtb_tlv320dac3100_adjust_headphone_output_volume(uint8_t volume);

/**
 * Activates the codec - This function is called in conjunction with
 * @ref mtb_tlv320dac3100_deactivate API after successful configuration update of the codec.
 */

/******************************************************************************
* Function Name: mtb_tlv320dac3100_activate
******************************************************************************
* Summary:
* Activates the codec - This function is called in conjunction with
* @ref mtb_tlv320dac3100_deactivate API after successful configuration
* update of the codec.
*
* Parameters: None
*
* Return:
*  Status of execution
*
******************************************************************************/
cy_rslt_t mtb_tlv320dac3100_activate(void);


/******************************************************************************
* Function Name: mtb_tlv320dac3100_deactivate
******************************************************************************
* Summary:
* Deactivates the codec - the configuration is retained, just the codec
* input/outputs are disabled. The function should be called before changing
* any setting in the codec over I2C
*
* Parameters: None
*
* Return:
*  Status of execution
*
******************************************************************************/
cy_rslt_t mtb_tlv320dac3100_deactivate(void);

/******************************************************************************
* Function Name: mtb_tlv320dac3100_write_byte
******************************************************************************
* Summary:
* This function writes a data byte to an audio codec register.
*
* Parameters:
*  reg   The audio codec register to update
*  data  The byte to be written to the audio codec register
*
* Return:
*  Status of write register
*
******************************************************************************/
cy_rslt_t mtb_tlv320dac3100_write_byte(mtb_tlv320dac3100_reg_t reg, uint8_t data);

/******************************************************************************
* Function Name: mtb_tlv320dac3100_set
******************************************************************************
* Summary:
* This function sets certain bits (defined as mask) in a register. This function can be used
* instead of @ref mtb_tlv320dac3100_write_byte() if change in a single or multiple selected bits in
* the register is needed and the value of other bits in the register should not be changed.
* Only the bits set to 1 in the mask are effected.
*
* Parameters:
*  reg   The audio codec register to update
*  mask  The mask used to set bits in the register
*
* Return:
*  Status of execution
*
******************************************************************************/
cy_rslt_t mtb_tlv320dac3100_set(mtb_tlv320dac3100_reg_t reg, uint8_t mask);

/******************************************************************************
* Function Name: mtb_tlv320dac3100_clear
******************************************************************************
* Summary:
* This function clears certain bits (defined as mask) in a register. This function can be used
* instead of @ref mtb_tlv320dac3100_write_byte() if change in a single or multiple selected bits in
* the register is needed and the value of other bits in the register should not be changed.
* Only the bits set to 1 in the mask are effected.
*
* Parameters:
*  reg   The audio codec register to update
*  mask  The mask used to clear bits in the register
*
* Return:
*  Status of execution
*
******************************************************************************/
cy_rslt_t mtb_tlv320dac3100_clear(mtb_tlv320dac3100_reg_t reg, uint8_t mask);

/******************************************************************************
* Function Name: mtb_tlv320dac3100_read_byte
******************************************************************************
* Summary:
* This function reads a data byte from an audio codec register
*
* Parameters:
*  reg   The audio codec register read
*  data  Pointer to the data
*
* Return:
*  Status of read register
*
******************************************************************************/
cy_rslt_t mtb_tlv320dac3100_read_byte(mtb_tlv320dac3100_reg_t reg, uint8_t* data);



#if defined(__cplusplus)
}
#endif /* #if defined(__cplusplus) */

/* [] END OF FILE */
