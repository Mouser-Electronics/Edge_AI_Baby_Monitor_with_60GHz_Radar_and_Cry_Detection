/***********************************************************************************************//**
 * \file mtb_tlv320dac3100.c
 *
 * \brief
 *     This file contains the TLV320DAC3100 codec control APIs.
 *
 *
 ********************************************************************************
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
 *******************************************************************************/

#include <stdbool.h>
#include "mtb_tlv320dac3100.h"
#include "cy_result.h"

#if defined(__cplusplus)
extern "C"
{
#endif /* #if defined(__cplusplus) */

/******************************************************************************
* Macros
******************************************************************************/
#define _TLV320DAC3100_I2S_MCTRL_WRD_SIZE_POS     (4)

/******************************************************************************
* Global variables
******************************************************************************/
static mtb_hal_i2c_t* _mtb_dac3100_i2c_ptr = NULL;
uint8_t ndac_div = 1;
uint8_t mdac_div = 1;
uint16_t dosr_div = 2;

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
cy_rslt_t mtb_tlv320dac3100_init(mtb_hal_i2c_t* i2c_inst)
{
    if (i2c_inst == NULL)
    {
        return CY_RSLT_TLV320DAC3100_INIT_FAIL;
    }

    _mtb_dac3100_i2c_ptr = i2c_inst;

    return CY_RSLT_SUCCESS;
}


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
void mtb_tlv320dac3100_free(void)
{
    _mtb_dac3100_i2c_ptr = NULL;
}


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
                                               mtb_tlv320dac3100_audio_output_t audio_output)
{
    uint32_t mdac_freq, ndac_freq, dac_fs;
    cy_rslt_t result = CY_RSLT_SUCCESS;


    if (TLV320DAC3100_SPK_AUDIO_OUTPUT < audio_output)
    {
        return CY_RSLT_FAILURE;
    }

    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P0_REG_PAGE_CTRL,
                                          SELECT_TLV320DAC3100_PAGE_0);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }

    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P0_REG_SW_RST,
                                          SELF_CLEARING_TLV320DAC3100_SW_RST);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    /* Clock settings */
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P0_REG_CLK_GEN_MUX,
                                          CFG_TLV320DAC3100_CODEC_PLL_CLKIN_PIN);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P0_REG_CLK_PLL_D_MSB,
                                          CFG_TLV320DAC3100_D_VALUE_MSB_BIT);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P0_REG_CLK_PLL_PR,
                                          CFG_TLV320DAC3100_PLL_P_R_VALS_POWER_DOWN);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    /* Set sample rate for the codec
        The next formula used for DAC_Fs calculation:

                        CLK_IN
        DAC_Fs = --------------------
                   NDAC x MDAC x DOSR

        The maximum frequencies for the main clocks:
        - DAC_CLK:     <= 49.152 MHz  (DAC_CLK is getting from NDAC divider)
        - DAC_MOD_CLK:  = 6.758  MHz  (DAC_MOD_CLK is getting from MDAC divider)
     */
    ndac_freq = mclk_hz;
    while ((ndac_freq / ndac_div) > MAX_TLV320DAC3100_CLK_FREQ_HZ)
    {
        ndac_div++;
    }

    /* Set MDAC divider
     * DOSR is limited in its range by the following condition:
     * 2.8 MHz < DOSR * DAC_Fs < 6.2 MHz
     * So the frequency after MDAC divider should be in this range
     */
    mdac_freq = (ndac_freq / ndac_div);
    while ((mdac_freq / mdac_div) > MAX_TLV320DAC3100_MOD_CLK_FREQ_HZ)
    {
        mdac_div++;
    }
    /* Set DOSR divider to achieve desired sample rate */
    dac_fs = (mdac_freq / mdac_div);
    while ((dac_fs / dosr_div) > (uint32_t)sample_rate)
    {
        dosr_div++;
    }

    /* The datasheet recommends to use even DOSR divider.
     * If the DOSR divider is odd, decrease it by 1.
     */
    if (dosr_div % 2 != 0)
    {
        dosr_div--;
    }

    /* Set DOSR MSB and LSB values
     * DOSR is a 10-bit register, so get only 2 MSB from the divider value
     */
    result =
        mtb_tlv320dac3100_write_byte(TLV320DAC3100_P0_REG_DAC_DOSR_MSB, ((dosr_div >> 8) & 0x3));
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P0_REG_DAC_DOSR_LSB, (dosr_div & 0xFF));
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    /* I2S interface, BLCK input, WCLK input */
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P0_REG_CODEC_INTERFACE_1,
                                          (word_length << _TLV320DAC3100_I2S_MCTRL_WRD_SIZE_POS));
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P0_REG_DAC_BSEL,
                                          TLV320DAC3100_PROCESSING_BLOCK);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    /* Adaptive filtering enabled */
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P8_REG_DAC_COEF_RAM,
                                          ENABLE_TLV320DAC3100_ADAPTIVE_FILTER);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    /* Volume control through register on */
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P0_REG_VOL_MICDET_CTL,
                                          ENABLE_TLV320DAC3100_VOLUME_CONTROL);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    /* Headphone output Power Down */
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P1_REG_HP_DRV,
                                          ENABLE_TLV320DAC3100_HEADPHONE_DRIVER);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    /* Driver-power-on-time=1.22s Driver ramp up time= 3.9ms */
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P1_REG_HP_OUT_POP,
                                          SET_TLV320DAC3100_RAMP_UP_POWER_ON_TIME);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    /* DAC_L to left channel mixer and DAC_R to right channel mixer */
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P1_REG_DAC_LR_OUT_MIX,
                                          ASSIGN_TLV320DAC3100_MIXER_VALUES_CH);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    /* HPL unmute, 0dB, gain not applied */
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P1_REG_HPL_DRV,
                                          UNMUTE_TLV320DAC3100_HPL_DRIVER);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    /* HPR unmute, 0dB, gain not applied */
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P1_REG_HPR_DRV,
                                          UNMUTE_TLV320DAC3100_HPR_DRIVER);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    /* Output gain 24dB, Class-D driver unmute, gain not applied */
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P1_REG_CLS_D_SPK_DRV,
                                          SET_TLV320DAC3100_CLASS_D_SPEAKER_DRIVER);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    /* Headphone output Power up */
    if (audio_output == TLV320DAC3100_HP_AUDIO_OUTPUT)
    {
        result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P1_REG_HP_DRV,
                                              POWER_UP_TLV320DAC3100_HPL_HPR_OP_DRIVER);
        if (result != CY_RSLT_SUCCESS)
        {
            return result;
        }
    }
    /* Speaker output Power up */
    if (audio_output == TLV320DAC3100_SPK_AUDIO_OUTPUT)
    {
        result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P1_REG_CLS_D_SPK_AMP,
                                              POWER_UP_TLV320DAC3100_CLASS_D_OP_DRIVER);
        if (result != CY_RSLT_SUCCESS)
        {
            return result;
        }
    }
    /* Left Channel analog routed to output HPL = 18 (0-127) */
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P1_REG_HP_LAV,
                                          SET_TLV320DAC3100_HPL_GAIN_18_DB);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    /* Right Channel analog routed to output HPR = 18 (0-127) */
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P1_REG_HP_RAV,
                                          SET_TLV320DAC3100_HPR_GAIN_18_DB);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    /* Left Channel analog routed to output SPL = 18 (0-127) */
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P1_REG_SP_LAV,
                                          SET_TLV320DAC3100_SPK_GAIN_18_DB);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    /* Power up Left and Right channel */
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P0_REG_DAC_DPATH,
                                          POWER_UP_TLV320DAC3100_LEFT_RIGHT_CH_SET_DPATH);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    /* DAC Left volume gain = -22dB */
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P0_REG_DAC_LVOL_CTL,
                                          SET_TLV320DAC3100_LEFT_VOL_GAIN_22_DB);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    /* DAC Right volume gain = -22dB */
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P0_REG_DAC_RVOL_CTL,
                                          SET_TLV320DAC3100_RIGHT_VOL_GAIN_22_DB);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    /* Unmute the Left-Right channel */
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P0_REG_DAC_VOL_CTL,
                                          UNMUTE_TLV320DAC3100_LEFT_RIGHT_CH);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    return result;
}


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
*  Status of Write register
*
******************************************************************************/
cy_rslt_t mtb_tlv320dac3100_write_byte(mtb_tlv320dac3100_reg_t reg, uint8_t data)
{
    if (NULL == _mtb_dac3100_i2c_ptr)
    {
        return CY_RSLT_FAILURE;
    }

    cy_rslt_t rslt;

    uint8_t  page[] =
        { TLV320DAC3100_P0_REG_PAGE_CTRL, (reg >> TLV320DAC3100_PAGE_NUM_Pos) & 0xFF };
    rslt =
        mtb_hal_i2c_controller_write(_mtb_dac3100_i2c_ptr, MTB_TLV320DAC3100_I2C_ADDR, page,
                                     sizeof(page),
                                     0,
                                     true);

    if (CY_RSLT_SUCCESS == rslt)
    {
        uint8_t buf[] = { (reg & 0xFF), data };
        rslt =
            mtb_hal_i2c_controller_write(_mtb_dac3100_i2c_ptr, MTB_TLV320DAC3100_I2C_ADDR, buf,
                                         sizeof(buf),
                                         0,
                                         true);
    }
    if (CY_RSLT_SUCCESS != rslt)
    {
        return rslt;
    }
    return CY_RSLT_SUCCESS;
}


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
cy_rslt_t mtb_tlv320dac3100_read_byte(mtb_tlv320dac3100_reg_t reg, uint8_t* data)
{
    if (NULL == _mtb_dac3100_i2c_ptr)
    {
        return CY_RSLT_FAILURE;
    }

    cy_rslt_t rslt;
    uint8_t   read_data = 0;

    uint8_t  page[] =
        { TLV320DAC3100_P0_REG_PAGE_CTRL, (reg >> TLV320DAC3100_PAGE_NUM_Pos) & 0xFF };
    rslt =
        mtb_hal_i2c_controller_write(_mtb_dac3100_i2c_ptr, MTB_TLV320DAC3100_I2C_ADDR, page,
                                     sizeof(page),
                                     0,
                                     true);

    if (CY_RSLT_SUCCESS == rslt)
    {
        uint8_t buf[] = { (reg & 0xFF), read_data };
        rslt =
            mtb_hal_i2c_controller_write(_mtb_dac3100_i2c_ptr, MTB_TLV320DAC3100_I2C_ADDR, buf,
                                         sizeof(buf),
                                         0,
                                         true);
    }

    if (CY_RSLT_SUCCESS != rslt)
    {
        return rslt;
    }

    rslt =
        mtb_hal_i2c_controller_read(_mtb_dac3100_i2c_ptr, MTB_TLV320DAC3100_I2C_ADDR, &read_data, 1,
                                    0,
                                    true);
    if (CY_RSLT_SUCCESS != rslt)
    {
        return rslt;
    }

    *data = read_data;
    return CY_RSLT_SUCCESS;
}


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
* Return: None
*
******************************************************************************/
cy_rslt_t mtb_tlv320dac3100_set(mtb_tlv320dac3100_reg_t reg, uint8_t mask)
{
    cy_rslt_t rslt;
    uint8_t data;

    rslt = mtb_tlv320dac3100_read_byte(reg, &data);
    if (rslt != CY_RSLT_SUCCESS)
    {
        return rslt;
    }
    data = data | mask;
    rslt = mtb_tlv320dac3100_write_byte(reg, data);
    if (rslt != CY_RSLT_SUCCESS)
    {
        return rslt;
    }
    return rslt;
}


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
* Return: None
*
******************************************************************************/
cy_rslt_t mtb_tlv320dac3100_clear(mtb_tlv320dac3100_reg_t reg, uint8_t mask)
{
    cy_rslt_t rslt;
    uint8_t data;

    rslt = mtb_tlv320dac3100_read_byte(reg, &data);
    if (rslt != CY_RSLT_SUCCESS)
    {
        return rslt;
    }
    data = data & ~mask;
    rslt = mtb_tlv320dac3100_write_byte(reg, data);
    if (rslt != CY_RSLT_SUCCESS)
    {
        return rslt;
    }
    return rslt;
}


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
* Return: None
*
******************************************************************************/
cy_rslt_t mtb_tlv320dac3100_adjust_speaker_output_volume(uint8_t volume)
{
    uint8_t out = volume|0x80;
    cy_rslt_t result;
    /* DAC Left volume gain = -22dB 0 -127 */
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P0_REG_DAC_LVOL_CTL, out);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    /* DAC Right volume gain = -22dB */
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P0_REG_DAC_RVOL_CTL, out);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    return result;
}


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
* Return: None
*
******************************************************************************/
cy_rslt_t mtb_tlv320dac3100_adjust_headphone_output_volume(uint8_t volume)
{
    uint8_t out = volume | 0x80;
    cy_rslt_t result;
    /* Left Channel analog routed to output HPL (0-127) */
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P1_REG_HP_LAV, out);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    /* Right Channel analog routed to output HPR (0-127) */
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P1_REG_HP_RAV, out);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    return result;
}


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
* Return: None
*
******************************************************************************/
cy_rslt_t mtb_tlv320dac3100_activate(void)
{
    cy_rslt_t result;
    /* NDAC and MDAC power up */
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P0_REG_NDAC_VAL,
                                          POWER_DOWN_TLV320DAC3100_NDAC | ndac_div);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    result = mtb_tlv320dac3100_write_byte(TLV320DAC3100_P0_REG_MDAC_VAL,
                                          POWER_DOWN_TLV320DAC3100_MDAC | mdac_div);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    return result;
}


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
* Return: None
*
******************************************************************************/
cy_rslt_t mtb_tlv320dac3100_deactivate(void)
{
    cy_rslt_t result;
    /* NDAC and MDAC power down */
    result = mtb_tlv320dac3100_clear(TLV320DAC3100_P0_REG_NDAC_VAL, POWER_DOWN_TLV320DAC3100_NDAC);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    result = mtb_tlv320dac3100_clear(TLV320DAC3100_P0_REG_MDAC_VAL, POWER_DOWN_TLV320DAC3100_MDAC);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }
    return result;
}


#if defined(__cplusplus)
}
#endif /* #if defined(__cplusplus) */

/* [] END OF FILE */
