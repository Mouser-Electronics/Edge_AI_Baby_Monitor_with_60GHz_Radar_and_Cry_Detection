/*
 * radar.c
 *
 *  Created on: Nov 24, 2025
 *      Author: ROJ030
 *
 * Rutronik Elektronische Bauelemente GmbH Disclaimer: The evaluation board
 * including the software is for testing purposes only and,
 * because it has limited functions and limited resilience, is not suitable
 * for permanent use under real conditions. If the evaluation board is
 * nevertheless used under real conditions, this is done at one’s responsibility;
 * any liability of Rutronik is insofar excluded
 */

#include "radar.h"

// Access to the pins
#include <cybsp.h>

// Access to the BGT60TR13C API (configure, start frame, ...)
#include <xensiv_bgt60trxx_mtb.h>

// Access to the radar settings exported using the Radar Fusion GUI
#define XENSIV_BGT60TRXX_CONF_IMPL
#include "radar_settings.h"

// Compute how many samples a frame contains
#define NUM_SAMPLES_PER_FRAME (XENSIV_BGT60TRXX_CONF_NUM_SAMPLES_PER_CHIRP \
		* XENSIV_BGT60TRXX_CONF_NUM_CHIRPS_PER_FRAME * XENSIV_BGT60TRXX_CONF_NUM_RX_ANTENNAS)

#define XENSIV_BGT60TRXX_IRQ_PRIORITY                   (1U)
#define SPI_INTR_NUM            ((IRQn_Type) CYBSP_SPI_CONTROLLER_IRQ)
#define SPI_INTR_PRIORITY       (2U)

/* spi context */
cy_stc_scb_spi_context_t SPI_context;

static xensiv_bgt60trxx_mtb_t bgt60_obj;

cy_stc_sysint_t irq_cfg;

static uint16_t data_available = 0;

void SPI_Interrupt(void)
{
    Cy_SCB_SPI_Interrupt(CYBSP_SPI_CONTROLLER_HW, &SPI_context);
}

void xensiv_bgt60trxx_interrupt_handler(void)
{
    data_available = 1;
    Cy_GPIO_ClearInterrupt(CYBSP_RADAR_INT_PORT, CYBSP_RADAR_INT_NUM);
    NVIC_ClearPendingIRQ(irq_cfg.intrSrc);
}

static int _init_hw()
{
    cy_rslt_t result;

    // Enable the RADAR
    bgt60_obj.iface.scb_inst = CYBSP_SPI_CONTROLLER_HW;
    bgt60_obj.iface.spi = &SPI_context;
    bgt60_obj.iface.sel_port = CYBSP_RSPI_CS_PORT;
    bgt60_obj.iface.sel_pin = CYBSP_RSPI_CS_PIN;
    bgt60_obj.iface.rst_port = CYBSP_RADAR_RESET_PORT;
    bgt60_obj.iface.rst_pin = CYBSP_RADAR_RESET_PIN;
    bgt60_obj.iface.irq_port = CYBSP_RADAR_INT_PORT;
    bgt60_obj.iface.irq_pin = CYBSP_RADAR_INT_PIN;
    bgt60_obj.iface.irq_num = CYBSP_RADAR_INT_IRQ;

    // Reduce drive strength to improve EMI
    Cy_GPIO_SetSlewRate(CYBSP_RSPI_MOSI_PORT, CYBSP_RSPI_MOSI_PIN, CY_GPIO_SLEW_FAST);
    Cy_GPIO_SetDriveSel(CYBSP_RSPI_MOSI_PORT, CYBSP_RSPI_MOSI_PIN, CY_GPIO_DRIVE_1_8);
    Cy_GPIO_SetSlewRate(CYBSP_RSPI_CLK_PORT, CYBSP_RSPI_CLK_PIN, CY_GPIO_SLEW_FAST);
    Cy_GPIO_SetDriveSel(CYBSP_RSPI_CLK_PORT, CYBSP_RSPI_CLK_PIN, CY_GPIO_DRIVE_1_8);

    irq_cfg.intrSrc = (IRQn_Type)bgt60_obj.iface.irq_num;
    irq_cfg.intrPriority = XENSIV_BGT60TRXX_IRQ_PRIORITY;

    result = xensiv_bgt60trxx_mtb_init(&bgt60_obj,
    		register_list, XENSIV_BGT60TRXX_CONF_NUM_REGS);

    if (result != CY_RSLT_SUCCESS) return -1;

    result = xensiv_bgt60trxx_mtb_interrupt_init(&bgt60_obj, NUM_SAMPLES_PER_FRAME);
    if (result != CY_RSLT_SUCCESS) return -2;

    Cy_SysInt_Init(&irq_cfg, xensiv_bgt60trxx_interrupt_handler);

    NVIC_ClearPendingIRQ(irq_cfg.intrSrc);
    NVIC_EnableIRQ(irq_cfg.intrSrc);

    Cy_GPIO_ClearInterrupt(CYBSP_RADAR_INT_PORT, CYBSP_RADAR_INT_NUM);
    NVIC_ClearPendingIRQ(irq_cfg.intrSrc);

    Cy_SysLib_Delay(1000);

    if (xensiv_bgt60trxx_soft_reset(&bgt60_obj.dev, XENSIV_BGT60TRXX_RESET_FIFO)
    		!= XENSIV_BGT60TRXX_STATUS_OK)
	{
		return -3;
	}

    if (xensiv_bgt60trxx_start_frame(&bgt60_obj.dev, true)
    		!= XENSIV_BGT60TRXX_STATUS_OK)
	{
    	return -4;
	}

	return 0;
}

static int _init_spi(void)
{
    cy_en_scb_spi_status_t init_status = Cy_SCB_SPI_Init(CYBSP_SPI_CONTROLLER_HW,
    		&CYBSP_SPI_CONTROLLER_config, &SPI_context);

    // If the initialization fails, update status
    if ( CY_SCB_SPI_SUCCESS != init_status )
    {
        return -1;
    }
    else
    {
        cy_stc_sysint_t spiIntrConfig =
        {
            .intrSrc      = SPI_INTR_NUM,
            .intrPriority = SPI_INTR_PRIORITY,
        };

        Cy_SysInt_Init(&spiIntrConfig, &SPI_Interrupt);
        NVIC_EnableIRQ(SPI_INTR_NUM);

        // Set active target select to line 0
        Cy_SCB_SPI_SetActiveSlaveSelect(CYBSP_SPI_CONTROLLER_HW, CY_SCB_SPI_SLAVE_SELECT1);
        // Enable SPI Controller block.
        Cy_SCB_SPI_Enable(CYBSP_SPI_CONTROLLER_HW);
    }

    return 0;
}

int radar_init()
{
	if (_init_spi() != 0) return -1;
	if (_init_hw() != 0) return -2;
	return 0;
}

int radar_is_data_available()
{
	return data_available;
}

int radar_get_num_samples_per_frame()
{
	return NUM_SAMPLES_PER_FRAME;
}

int radar_read_data(uint16_t* data, uint16_t num_samples)
{
	data_available = 0;

	if (num_samples != NUM_SAMPLES_PER_FRAME) return -1;

	if (xensiv_bgt60trxx_get_fifo_data(&bgt60_obj.dev, data, num_samples) != XENSIV_BGT60TRXX_STATUS_OK)
	{
		return -2;
	}
	return 0;
}
