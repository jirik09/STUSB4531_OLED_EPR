/**
  ******************************************************************************
  * @file    stusb4531_display.h
  * @brief   STUSB4531 Display Layer Header
  * @note    Functions for displaying/printing STUSB4531 data
  ******************************************************************************
  */

#ifndef STUSB4531_DISPLAY_H
#define STUSB4531_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32c0xx_hal.h"
#include <stdint.h>

/**
  * @brief Read and display detailed PDO information from DPM_SRC_PDO registers
  * @param hi2c I2C handle
  * @param num_pdos Number of valid PDOs
  * @param pe_fsm Policy Engine FSM state
  * @param pd_status PD status register value
  * @note Requires printf support (UART output)
  */
void STUSB4531_DisplayDetailedPDOs(I2C_HandleTypeDef *hi2c, uint8_t num_pdos, uint8_t pe_fsm, uint8_t pd_status);

/**
  * @brief Display sink configuration registers
  * @param hi2c I2C handle
  * @note Shows what power levels the device is configured to REQUEST (sink side)
  */
void STUSB4531_DisplaySinkConfiguration(I2C_HandleTypeDef *hi2c);

#ifdef __cplusplus
}
#endif

#endif /* STUSB4531_DISPLAY_H */
