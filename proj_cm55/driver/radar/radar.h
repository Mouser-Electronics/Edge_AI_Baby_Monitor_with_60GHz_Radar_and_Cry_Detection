/*
 * radar.h
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

#ifndef DRIVER_RADAR_H_
#define DRIVER_RADAR_H_

#include <stdint.h>

/**
 * @brief Initialize radar
 * Init SPI and start frame generation
 *
 * @retval 0 Success else something wrong  happened
 */
int radar_init();

/**
 * @brief Check if radar data are available
 *
 * @retval 0 No data available
 * @retval 1 Data available -> call radar_read_data
 */
int radar_is_data_available();

/**
 * @brief Get the number of samples within a frame
 * num samples per frame = num antenna * num chirps per frame * num samples per chirp
 *
 * @retval number of samples per frame
 */
int radar_get_num_samples_per_frame();

/**
 * @brief Read radar data
 *
 * @param [in] data Address of the buffer where to store the data
 * @param [in] num_samples Number of samples to read (typically radar_get_num_samples_per_frame())
 *
 * @retval 0 Success else something wrong  happened
 */
int radar_read_data(uint16_t* data, uint16_t num_samples);


#endif /* DRIVER_RADAR_H_ */
