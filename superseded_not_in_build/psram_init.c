/*******************************************************************************
* File Name: psram_init.c
*
* Description: Initialises SMIF1 for XIP access to the on-board S70KS1283
*              Octal-DDR PSRAM (64 MB, mapped at 0x64000000).
*
*  This replaces the mtb_serial_memory library that is absent in this project.
*  The PSRAM device config (commands, timing) is transcribed verbatim from the
*  demo5 QSPI-configurator output for the same KIT_PSE84_AI board.
*
*  Call order: cybsp_init() → psram_init() → vTaskStartScheduler()
*******************************************************************************/

#include "psram_init.h"
#include "cy_smif.h"
#include "cy_smif_memslot.h"
#include "cycfg_peripherals.h"
#include "cy_utils.h"
#include <stdio.h>

#define SMIF1_INIT_TIMEOUT_USEC  1000u

/*──────────────────────────────────────────────────────────────────────────────
 * S70KS1283 command structures  (Octal DDR, 4-byte address)
 *──────────────────────────────────────────────────────────────────────────────*/

static cy_stc_smif_mem_cmd_t s_psram_readCmd =
{
    .command              = 0xEEU,
    .cmdWidth             = CY_SMIF_WIDTH_OCTAL,
    .addrWidth            = CY_SMIF_WIDTH_OCTAL,
    .mode                 = 0xFFFFFFFFU,
    .modeWidth            = CY_SMIF_WIDTH_NA,
    .dummyCycles          = 15U,
    .dataWidth            = CY_SMIF_WIDTH_OCTAL,
#if (CY_IP_MXSMIF_VERSION >= 2)
    .dataRate             = CY_SMIF_DDR,
    .dummyCyclesPresence  = CY_SMIF_PRESENT_1BYTE,
    .modePresence         = CY_SMIF_NOT_PRESENT,
    .modeH                = 0x00,
    .modeRate             = CY_SMIF_SDR,
    .addrRate             = CY_SMIF_DDR,
    .cmdPresence          = CY_SMIF_PRESENT_2BYTE,
    .commandH             = 0xEE,
    .cmdRate              = CY_SMIF_DDR,
#endif
};

static cy_stc_smif_mem_cmd_t s_psram_writeEnCmd =
{
    .command              = 0x06U,
    .cmdWidth             = CY_SMIF_WIDTH_OCTAL,
    .addrWidth            = CY_SMIF_WIDTH_NA,
    .mode                 = 0xFFFFFFFFU,
    .modeWidth            = CY_SMIF_WIDTH_NA,
    .dummyCycles          = 0U,
    .dataWidth            = CY_SMIF_WIDTH_NA,
#if (CY_IP_MXSMIF_VERSION >= 2)
    .dataRate             = CY_SMIF_SDR,
    .dummyCyclesPresence  = CY_SMIF_NOT_PRESENT,
    .modePresence         = CY_SMIF_NOT_PRESENT,
    .modeH                = 0x00,
    .modeRate             = CY_SMIF_SDR,
    .addrRate             = CY_SMIF_SDR,
    .cmdPresence          = CY_SMIF_PRESENT_2BYTE,
    .commandH             = 0x06,
    .cmdRate              = CY_SMIF_DDR,
#endif
};

static cy_stc_smif_mem_cmd_t s_psram_writeDisCmd =
{
    .command              = 0x04U,
    .cmdWidth             = CY_SMIF_WIDTH_OCTAL,
    .addrWidth            = CY_SMIF_WIDTH_NA,
    .mode                 = 0xFFFFFFFFU,
    .modeWidth            = CY_SMIF_WIDTH_NA,
    .dummyCycles          = 0U,
    .dataWidth            = CY_SMIF_WIDTH_NA,
#if (CY_IP_MXSMIF_VERSION >= 2)
    .dataRate             = CY_SMIF_SDR,
    .dummyCyclesPresence  = CY_SMIF_NOT_PRESENT,
    .modePresence         = CY_SMIF_NOT_PRESENT,
    .modeH                = 0x00,
    .modeRate             = CY_SMIF_SDR,
    .addrRate             = CY_SMIF_SDR,
    .cmdPresence          = CY_SMIF_PRESENT_2BYTE,
    .commandH             = 0x04,
    .cmdRate              = CY_SMIF_DDR,
#endif
};

static cy_stc_smif_mem_cmd_t s_psram_programCmd =
{
    .command              = 0xDEU,
    .cmdWidth             = CY_SMIF_WIDTH_OCTAL,
    .addrWidth            = CY_SMIF_WIDTH_OCTAL,
    .mode                 = 0xFFFFFFFFU,
    .modeWidth            = CY_SMIF_WIDTH_NA,
    .dummyCycles          = 15U,
    .dataWidth            = CY_SMIF_WIDTH_OCTAL,
#if (CY_IP_MXSMIF_VERSION >= 2)
    .dataRate             = CY_SMIF_DDR,
    .dummyCyclesPresence  = CY_SMIF_PRESENT_1BYTE,
    .modePresence         = CY_SMIF_NOT_PRESENT,
    .modeH                = 0x00,
    .modeRate             = CY_SMIF_SDR,
    .addrRate             = CY_SMIF_DDR,
    .cmdPresence          = CY_SMIF_PRESENT_2BYTE,
    .commandH             = 0xDE,
    .cmdRate              = CY_SMIF_DDR,
#endif
};

static cy_stc_smif_mem_device_cfg_t s_psram_deviceCfg =
{
    .numOfAddrBytes          = 0x04U,
    .memSize                 = 0x1000000U,
    .readCmd                 = &s_psram_readCmd,
    .writeEnCmd              = &s_psram_writeEnCmd,
    .writeDisCmd             = &s_psram_writeDisCmd,
    .eraseCmd                = 0,
    .eraseSize               = 0x1000000U,
    .chipEraseCmd            = 0,
    .programCmd              = &s_psram_programCmd,
    .programSize             = 0x1000000U,
    .readStsRegQeCmd         = 0,
    .readStsRegWipCmd        = 0,
    .writeStsRegQeCmd        = 0,
    .stsRegBusyMask          = 0x00U,
    .stsRegQuadEnableMask    = 0x00U,
    .eraseTime               = 1U,
    .chipEraseTime           = 16U,
    .programTime             = 8U,
#if (CY_SMIF_DRV_VERSION_MAJOR > 1) || (CY_SMIF_DRV_VERSION_MINOR >= 50)
    .hybridRegionCount       = 0U,
    .hybridRegionInfo        = 0,
#endif
    .readLatencyCmd          = 0,
    .writeLatencyCmd         = 0,
    .latencyCyclesRegAddr    = 0x00U,
    .latencyCyclesMask       = 0x00U,
#if (CY_IP_MXSMIF_VERSION >= 2)
    .octalDDREnableSeq       = 0,
    .readStsRegOeCmd         = 0,
    .writeStsRegOeCmd        = 0,
    .stsRegOctalEnableMask   = 0x00U,
    .octalEnableRegAddr      = 0x00U,
    .freq_of_operation       = CY_SMIF_100MHZ_OPERATION,
#endif
};

static cy_stc_smif_mem_config_t s_psram_memConfig =
{
    .slaveSelect    = CY_SMIF_SLAVE_SELECT_2,
#if (CY_IP_MXSMIF_VERSION >= 2)
    .flags          = CY_SMIF_FLAG_SMIF_REV_3 | CY_SMIF_FLAG_MEMORY_MAPPED |
                      CY_SMIF_FLAG_WR_EN | CY_SMIF_FLAG_MERGE_ENABLE,
#else
    .flags          = CY_SMIF_FLAG_MEMORY_MAPPED | CY_SMIF_FLAG_WR_EN |
                      CY_SMIF_FLAG_MERGE_ENABLE,
#endif
    .dataSelect     = CY_SMIF_DATA_SEL0,
    .baseAddress    = 0x64000000U,
    .memMappedSize  = 0x4000000U,
    .dualQuadSlots  = 0,
    .deviceCfg      = &s_psram_deviceCfg,
#if (CY_IP_MXSMIF_VERSION >= 2)
    .mergeTimeout   = CY_SMIF_MERGE_TIMEOUT_4096_CYCLES,
#endif
};

static cy_stc_smif_mem_config_t* s_psram_memConfigs[1] = { &s_psram_memConfig };

static cy_stc_smif_block_config_t s_psram_blockConfig =
{
    .memCount     = 1U,
    .memConfig    = s_psram_memConfigs,
    .majorVersion = CY_SMIF_DRV_VERSION_MAJOR,
    .minorVersion = CY_SMIF_DRV_VERSION_MINOR,
};

static cy_stc_smif_context_t s_smif1_context;

/*──────────────────────────────────────────────────────────────────────────────*/
void psram_init(void)
{
    cy_en_smif_status_t status;

    printf("[PSRAM] Starting SMIF1 init...\r\n");

    /* Disable SMIF1 for reconfiguration */
    Cy_SMIF_Disable(CYBSP_SMIF_CORE_1_PSRAM_HW);

    printf("[PSRAM] Calling Cy_SMIF_Init...\r\n");
    /* Init SMIF1 hardware using the BSP-generated config */
    status = Cy_SMIF_Init(CYBSP_SMIF_CORE_1_PSRAM_HW,
                          &CYBSP_SMIF_CORE_1_PSRAM_config,
                          SMIF1_INIT_TIMEOUT_USEC,
                          &s_smif1_context);
    if (CY_SMIF_SUCCESS != status)
    {
        printf("[PSRAM] Cy_SMIF_Init failed: 0x%x\r\n", (unsigned)status);
        CY_ASSERT(0);
    }

    printf("[PSRAM] Cy_SMIF_Init OK, enabling...\r\n");
    /* Configure data-select for slave slot 2 */
    Cy_SMIF_SetDataSelect(CYBSP_SMIF_CORE_1_PSRAM_HW,
                          s_psram_memConfig.slaveSelect,
                          s_psram_memConfig.dataSelect);

    /* Enable SMIF1 */
    Cy_SMIF_Enable(CYBSP_SMIF_CORE_1_PSRAM_HW, &s_smif1_context);

    printf("[PSRAM] Calling Cy_SMIF_MemInit...\r\n");
    /* Program device registers for memory-mapped (XIP) access */
    status = Cy_SMIF_MemInit(CYBSP_SMIF_CORE_1_PSRAM_HW,
                             &s_psram_blockConfig,
                             &s_smif1_context);
    if (CY_SMIF_SUCCESS != status)
    {
        printf("[PSRAM] Cy_SMIF_MemInit failed: 0x%x\r\n", (unsigned)status);
        CY_ASSERT(0);
    }

    printf("[PSRAM] Cy_SMIF_MemInit OK, switching to XIP...\r\n");
    /* Switch SMIF1 to XIP (memory-mapped) mode */
    Cy_SMIF_SetMode(CYBSP_SMIF_CORE_1_PSRAM_HW, CY_SMIF_MEMORY);

    printf("[PSRAM] XIP enabled at 0x64000000\r\n");
}
