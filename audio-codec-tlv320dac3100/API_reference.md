# Texas Instruments TLV320DAC3100 low-power stereo audio DAC with audio processing and mono Class-D speaker amplifier driver library API reference guide

## General description

Basic set of APIs for interacting with TLV320DAC3100 low-power stereo audio DAC. 

For more information about the TLV320DAC3100 low-power stereo audio DAC, see [Texas Instruments TLV320DAC3100 low-power stereo audio DAC with audio processing](https://www.ti.com/product/TLV320DAC3100?utm_source=google&utm_medium=cpc&utm_campaign=asc-null-null-GPN_EN-cpc-pf-google-ww_en_cons&utm_content=TLV320DAC3100&ds_k=TLV320DAC3100&DCM=yes&gad_source=1&gad_campaignid=14388345080&gbraid=0AAAAAC068F2_uiPre6qxMuUbQf1WNXJ9Y&gclid=EAIaIQobChMIjs-c0I7DjgMVs8c8Ah3nsRcwEAAYASAAEgLDcPD_BwE&gclsrc=aw.ds). <br>

## Code snippets

### Snippet 1: TLV320DAC3100 low-power Stereo audio DAC driver initialization and deinitialization 

The following snippet initializes the I2C and TLV320DAC3100.

```
/* Initializes the I2C in master mode. */
      i2c_result = Cy_SCB_I2C_Init(CYBSP_I2C_CONTROLLER_HW,
                                 &CYBSP_I2C_CONTROLLER_config,
                                 &CYBSP_I2C_CONTROLLER_context);

      if (CY_SCB_I2C_SUCCESS  != i2c_result)
      {
          CY_ASSERT(0);
      }

      Cy_SCB_I2C_Enable(CYBSP_I2C_CONTROLLER_HW);

      /* Configures the I2C master interface with the desired clock frequency. */
      result = mtb_hal_i2c_setup(&CYBSP_I2C_CONTROLLER_hal_obj,
                               &CYBSP_I2C_CONTROLLER_hal_config,
                               &CYBSP_I2C_CONTROLLER_context,
                               NULL);

      if (CY_SCB_I2C_SUCCESS != i2c_result)
      {
          CY_ASSERT(0);
      }

      /* Configures the I2C block. Master/slave-specific functions only work when the block is configured to desired mode. */
      result = mtb_hal_i2c_configure(&CYBSP_I2C_CONTROLLER_hal_obj,
                                   &CYBSP_I2C_SLAVE_config);
      if (CY_RSLT_SUCCESS != result)
      {
          CY_ASSERT(0);
      }

      /* Initializes and configures the TLV320DAC3100 audio codec. */
      result = mtb_tlv320dac3100_init(&CYBSP_I2C_CONTROLLER_hal_obj);
      if (CY_SCB_I2C_SUCCESS != i2c_result)
      {
          CY_ASSERT(0);
      }

      /* Configures internal clock dividers to achieve the desired sample rate. */
      result = mtb_tlv320dac3100_configure_clocking(MCLK_HZ, TLV320DAC3100_DAC_SAMPLE_RATE_16_KHZ, TLV320DAC3100_I2S_WORD_SIZE_16, TLV320DAC3100_HP_AUDIO_OUTPUT);
      if (CY_SCB_I2C_SUCCESS != i2c_result)
      {
          CY_ASSERT(0);
      }  

```

### Snippet 2: Volume control of speaker and headphone

The following snippet shows how to control the volume of a speaker and headphone.

```

/* Deactivates TLV320DAC3100 audio codec. */
mtb_tlv320dac3100_deactivate();

/* Updates the volume of both the left and right channels of the speaker output. */
mtb_tlv320dac3100_adjust_speaker_output_volume(VOLUME_VALUE);

/* Updates the volume of both the left and right channels of the headphone output. */
mtb_tlv320dac3100_adjust_headphone_output_volume(VOLUME_VALUE);

/* Activates TLV320DAC3100 audio codec. */
mtb_tlv320dac3100_activate();

```

## Enumerations

Enumeration for the TLV320DAC3100 I2C interface register addresses.

```
typedef enum
{
    /* Page 0 */
    TLV320DAC3100_P0_REG_PAGE_CTRL =
        TLV320DAC3100_PAGE_AND_REG_VAL(0, 0),    /**< 0x00: Page control register. */

    TLV320DAC3100_P0_REG_SW_RST,                 /**< 0x01: Software reset. */
    TLV320DAC3100_P0_REG_CLK_GEN_MUX =
        TLV320DAC3100_PAGE_AND_REG_VAL(0, 4),    /**< 0x04: Clock-Gen muxing. */

    TLV320DAC3100_P0_REG_CLK_PLL_PR,             /**< 0x05: PLL P and R values. */
    TLV320DAC3100_P0_REG_CLK_PLL_J,              /**< 0x06: PLL J-value. */
    TLV320DAC3100_P0_REG_CLK_PLL_D_MSB,          /**< 0x07: PLL D-value MSB. */
    TLV320DAC3100_P0_REG_NDAC_VAL =
        TLV320DAC3100_PAGE_AND_REG_VAL(0, 11),   /**< 0x0B: DAC NDAC_VAL. */

    TLV320DAC3100_P0_REG_MDAC_VAL,               /**< 0x0C: DAC MDAC_VAL. */
    TLV320DAC3100_P0_REG_DAC_DOSR_MSB,           /**< 0x0D: DAC DOSR_VAL MSB. */
    TLV320DAC3100_P0_REG_DAC_DOSR_LSB,           /**< 0x0E: DAC DOSR_VAL LSB. */
    TLV320DAC3100_P0_REG_CLKOUT_MUX =
        TLV320DAC3100_PAGE_AND_REG_VAL(0, 25),   /**< 0x19: CLKOUT MUX. */

    TLV320DAC3100_P0_REG_CLKOUT_M_VAL,           /**< 0x1A: CLKOUT M_VAL. */
    TLV320DAC3100_P0_REG_CODEC_INTERFACE_1,      /**< 0x1B: Codec Interface Control 1. */
    TLV320DAC3100_P0_REG_BCLK_N_VAL =
        TLV320DAC3100_PAGE_AND_REG_VAL(0, 30),   /**< 0x1E: BCLK N_VAL. */

    TLV320DAC3100_P0_REG_CSI1_CTL,               /**< 0x1F: Codec Secondary Interface Control 1. */
    TLV320DAC3100_P0_REG_CSI2_CTL,               /**< 0x20: Codec Secondary Interface Control 2. */
    TLV320DAC3100_P0_REG_CSI3_CTL,               /**< 0x21: Codec Secondary Interface Control 3. */
    TLV320DAC3100_P0_REG_I2C_BUS_CND,            /**< 0x22: I2C bus condition. */
    TLV320DAC3100_P0_REG_DAC_BSEL =
        TLV320DAC3100_PAGE_AND_REG_VAL(0, 60),   /**< 0x3C: DAC processing block selection. */

    TLV320DAC3100_P0_REG_DAC_DPATH =
        TLV320DAC3100_PAGE_AND_REG_VAL(0, 63),   /**< 0x3F: DAC datapath setup. */

    TLV320DAC3100_P0_REG_DAC_VOL_CTL,            /**< 0x40: DAC volume control. */
    TLV320DAC3100_P0_REG_DAC_LVOL_CTL,           /**< 0x41: DAC left volume control. */
    TLV320DAC3100_P0_REG_DAC_RVOL_CTL,           /**< 0x42: DAC right volume control. */
    TLV320DAC3100_P0_REG_VOL_MICDET_CTL =
        TLV320DAC3100_PAGE_AND_REG_VAL(0, 116),  /**< 0x74: MICDET-pin SAR ADC - Volume control. */

    TLV320DAC3100_P0_REG_VOL_MICDET_GAIN,        /**< 0x75: VOL/MICDET-pin gain. */

    /* Page 1 */
    TLV320DAC3100_P1_REG_HP_DRV =
        TLV320DAC3100_PAGE_AND_REG_VAL(1, 31),  /**< 0x1F: Headphone drivers. */

    TLV320DAC3100_P1_REG_CLS_D_SPK_AMP,     /**< 0x20: Class-D speaker amplifier. */
    TLV320DAC3100_P1_REG_HP_OUT_POP,        /**< 0x21: HP output drivers POP removal settings. */
    TLV320DAC3100_P1_REG_PGA_PER_CTL,       /**< 0x22: Driver PGA ramp-down period control. */
    TLV320DAC3100_P1_REG_DAC_LR_OUT_MIX,    /**< 0x23: DAC_L and DAC_R output mixer routing. */
    TLV320DAC3100_P1_REG_HP_LAV,            /**< 0x24: Left analog volume to HPL. */
    TLV320DAC3100_P1_REG_HP_RAV,            /**< 0x25: Right analog volume to HPR. */
    TLV320DAC3100_P1_REG_SP_LAV,            /**< 0x26: Left analog volume to SPK. */
    TLV320DAC3100_P1_REG_SP_RESERVED,       /**< 0x27: Reserved. */
    TLV320DAC3100_P1_REG_HPL_DRV,           /**< 0x28: HPL driver. */
    TLV320DAC3100_P1_REG_HPR_DRV,           /**< 0x29: HPR driver. */
    TLV320DAC3100_P1_REG_CLS_D_SPK_DRV,     /**< 0x2A: Class-D speaker (SPK) driver. */
    TLV320DAC3100_P1_REG_HP_DRV_CTL =
        TLV320DAC3100_PAGE_AND_REG_VAL(1, 44), /**< 0x2C: HP driver control. */

    /* Page 8 */
    TLV320DAC3100_P8_REG_DAC_COEF_RAM =
        TLV320DAC3100_PAGE_AND_REG_VAL(8, 1)   /**< 0x01: DAC coefficient RAM control. */
} mtb_tlv320dac3100_reg_t;
```

Enumeration for supported sample rates for DAC in the TLV320DAC3100 audio codec.

```
typedef enum
{
    TLV320DAC3100_DAC_SAMPLE_RATE_192_KHZ    = 192000,   /**< 192 kHz sample rate. */
    TLV320DAC3100_DAC_SAMPLE_RATE_176_4_KHZ  = 176400,   /**< 176.4 kHz sample rate. */
    TLV320DAC3100_DAC_SAMPLE_RATE_96_KHZ     = 96000,    /**< 96 KHz sample rate */
    TLV320DAC3100_DAC_SAMPLE_RATE_88_2_KHZ   = 88200,    /**< 88.2 kHz sample rate. */
    TLV320DAC3100_DAC_SAMPLE_RATE_48_KHZ     = 48000,    /**< 48 kHz sample rate. */
    TLV320DAC3100_DAC_SAMPLE_RATE_44_1_KHZ   = 44100,    /**< 44.1 kHz sample rate. */
    TLV320DAC3100_DAC_SAMPLE_RATE_32_KHZ     = 32000,    /**< 32 kHz sample rate. */
    TLV320DAC3100_DAC_SAMPLE_RATE_24_KHZ     = 24000,    /**< 24 kHz sample rate. */
    TLV320DAC3100_DAC_SAMPLE_RATE_22_05_KHZ  = 22050,    /**< 22.05 kHz sample rate. */
    TLV320DAC3100_DAC_SAMPLE_RATE_16_KHZ     = 16000,    /**< 16 kHz sample rate. */
    TLV320DAC3100_DAC_SAMPLE_RATE_12_KHZ     = 12000,    /**< 12 kHz sample rate. */
    TLV320DAC3100_DAC_SAMPLE_RATE_11_025_KHZ = 11025,    /**< 11.025 kHz sample rate. */
    TLV320DAC3100_DAC_SAMPLE_RATE_8_KHZ      = 8000      /**< 8 kHz sample rate. */
} mtb_tlv320dac3100_dac_sample_rate_t;
```

Enumeration for supported I2S word sizes in Right Justified Mode for the TLV320DAC3100 audio codec.

```
typedef enum
{
    TLV320DAC3100_I2S_WORD_SIZE_16 = 0,   /**< 16-bit word size. */
    TLV320DAC3100_I2S_WORD_SIZE_20,       /**< 20-bit word size. */
    TLV320DAC3100_I2S_WORD_SIZE_24,       /**< 24-bit word size. */
    TLV320DAC3100_I2S_WORD_SIZE_32        /**< 32-bit word size. */
} mtb_tlv320dac3100_i2s_word_size_t;
```

## Functions

cy_rslt_t `mtb_tlv320dac3100_init(mtb_hal_i2c_t* i2c_inst)`
> Initializes the I2C communication with the audio codec and do basic configuration of the codec.

void `mtb_tlv320dac3100_free(void)`
> Frees up any resources allocated by the codec as part of.

cy_rslt_t ` mtb_tlv320dac3100_configure_clocking(uint32_t mclk_hz,mtb_tlv320dac3100_dac_sample_rate_t sample_rate, mtb_tlv320dac3100_i2s_word_size_t word_length,mtb_tlv320dac3100_audio_output_t audio_output)`
> Configures internal clock dividers to achieve the desired sample rate.

cy_rslt_t `mtb_tlv320dac3100_adjust_speaker_output_volume(uint8_t volume)`
> Updates the volume of both the left and right channels of the speaker output.

cy_rslt_t `mtb_tlv320dac3100_adjust_headphone_output_volume(uint8_t volume)`
> Updates the volume of both the left and right channels of the headphone output.

cy_rslt_t `mtb_tlv320dac3100_activate(void)`
> Activates the codec, which is called in conjunction with.

cy_rslt_t `mtb_tlv320dac3100_deactivate(void)`
> Deactivates the codec: The configuration is retained, just the codec
input/outputs are disabled. Call this function before changing any setting in the codec over I2C.

cy_rslt_t `mtb_tlv320dac3100_write_byte(mtb_tlv320dac3100_reg_t reg, uint8_t data)`
> Writes a data byte to an audio codec register.

cy_rslt_t `mtb_tlv320dac3100_set(mtb_tlv320dac3100_reg_t reg, uint8_t mask)`
> Sets certain bits (defined as mask) in a register.

cy_rslt_t `mtb_tlv320dac3100_clear(mtb_tlv320dac3100_reg_t reg, uint8_t mask)`
> Clears certain bits (defined as mask) in a register.

cy_rslt_t `mtb_tlv320dac3100_read_byte(mtb_tlv320dac3100_reg_t reg, uint8_t* data)`
> Reads a data byte from an audio codec register.

## Function documentation

#### mtb_tlv320dac3100_init

- cy_rslt_t `mtb_tlv320dac3100_init(mtb_hal_i2c_t* i2c_inst)`

> **Summary:**  Initializes the I2C communication and configures the TLV320DAC3100 audio codec.
>
> **Parameter:**
>  Parameters            |  Description       
>  :-------              |  :------------
>   [in] i2c_inst     |  Pointer to an initialized I2C object
> 
> Return:
>  - cy_rslt_t       :        Status of execution

#### mtb_tlv320dac3100_free

- void `mtb_tlv320dac3100_free(void)`

> **Summary:** Frees up any resources allocated by the codec as part of.
> 

#### mtb_tlv320dac3100_configure_clocking

- cy_rslt_t ` mtb_tlv320dac3100_configure_clocking(uint32_t mclk_hz,mtb_tlv320dac3100_dac_sample_rate_t sample_rate, mtb_tlv320dac3100_i2s_word_size_t word_length,mtb_tlv320dac3100_audio_output_t audio_output)`

> **Summary:** Configures internal clock dividers to achieve the desired sample rate.
> 
> **Parameter:**
>  Parameters            |  Description       
>  :-------              |  :------------
>  [in] mclk_hz          | Input MCLK frequency in Hz
>  [in] sample_rate      | Desired sample rate for the audio codec
>  [in] word_length      | I2S word length
>  [in] audio_output     | Desired output for audio
>
> Return:
>  - cy_rslt_t       :        Status of execution

#### mtb_tlv320dac3100_adjust_speaker_output_volume

- cy_rslt_t `mtb_tlv320dac3100_adjust_speaker_output_volume(uint8_t volume)`

> **Summary:** Updates the volume of both the left and right channels of the speaker output.
> 
> **Parameter:**
>  Parameters            |  Description       
>  :-------              |  :------------
>   [in] volume           | Volume in steps of 0.5 dB

#### mtb_tlv320dac3100_adjust_headphone_output_volume

- cy_rslt_t `mtb_tlv320dac3100_adjust_headphone_output_volume(uint8_t volume)`

> **Summary:** Updates the volume of both the left and right channels of the headphone output.
> 
> **Parameter:**
>  Parameters            |  Description       
>  :-------              |  :------------
>   [in] volume           | Volume in steps of 0.5 dB
>
> Return:
>  - cy_rslt_t       :        Status of execution

#### mtb_tlv320dac3100_activate

- cy_rslt_t `mtb_tlv320dac3100_activate(void)`

> **Summary:** Activates the codec, which is called in conjunction with.
>
> Return:
>  - cy_rslt_t       :        Status of execution

#### mtb_tlv320dac3100_deactivate

- cy_rslt_t `mtb_tlv320dac3100_deactivate(void)`

> **Summary:** Deactivates the codec, the configuration is retained, just the codec input/outputs are disabled. Call this function before changing any setting in the codec over I2C.
>
> Return:
>  - cy_rslt_t       :        Status of execution

#### mtb_tlv320dac3100_write_byte

- cy_rslt_t `mtb_tlv320dac3100_write_byte(mtb_tlv320dac3100_reg_t reg, uint8_t data)`

> **Summary:** Writes a data byte to an audio codec register.
> 
> **Parameter:**
>  Parameters            |  Description       
>  :-------              |  :------------
>   [in] reg              | Audio codec register to update
>   [in] data             | Byte value to write to the register
>
> Return:
>  - cy_rslt_t        :        Status of write register

#### mtb_tlv320dac3100_set

- cy_rslt_t `mtb_tlv320dac3100_set(mtb_tlv320dac3100_reg_t reg, uint8_t mask)`

> **Summary:** Sets certain bits (defined as mask) in a register.
> 
> **Parameter:**
>  Parameters            |  Description       
>  :-------              |  :------------
>   [in] reg              | Audio codec register to update
>   [in] mask             | Mask is used to set bits in the register
>
> Return:
>  - cy_rslt_t       :        Status of execution

#### mtb_tlv320dac3100_clear

- cy_rslt_t `mtb_tlv320dac3100_clear(mtb_tlv320dac3100_reg_t reg, uint8_t mask)`

> **Summary:** Clears certain bits (defined as mask) in a register.
> 
> **Parameter:**
>  Parameters            |  Description       
>  :-------              |  :------------
>   [in] reg              | Audio codec register to update
>   [in] mask             | Mask is used to clear  bits in the register
>
> Return:
>  - cy_rslt_t       :        Status of execution

#### mtb_tlv320dac3100_read_byte

- cy_rslt_t `mtb_tlv320dac3100_read_byte(mtb_tlv320dac3100_reg_t reg, uint8_t* data)`

> **Summary:** Reads a data byte from an audio codec register.
> 
> **Parameter:**
>  Parameters            |  Description       
>  :-------              |  :------------
>    [in] reg              | Register from which to read
>    [in] data             | Pointer to the data
>
> Return:
>  - cy_rslt_t        :        Status of read register
---
© 2025 - 2026, Cypress Semiconductor Corporation (an Infineon company)
