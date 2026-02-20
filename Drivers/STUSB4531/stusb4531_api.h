/**
  ******************************************************************************
  * @file    stusb4531_api.h
  * @brief   STUSB4531 API Header - Main Application Interface
  * @note    Includes all layers: Communication, Logic, and Display
  ******************************************************************************
  */

#ifndef __STUSB4531_API_H
#define __STUSB4531_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32c0xx_hal.h"
#include <stdint.h>
#include <stdbool.h>
#include "stusb4531.h"  // Include manufacturer's register definitions

/* Include all layers */
#include "stusb4531_comm.h"      // Communication layer (I2C)
#include "stusb4531_logic.h"     // Logic layer (business logic)
#include "stusb4531_display.h"   // Display layer (UART output)

/* Legacy compatibility - redirect to comm layer */
#define STUSB4531_I2C_ADDRESS       (0x28 << 1)  // 7-bit address 0x28 shifted for HAL

/* 
 * Note: All data structures and function prototypes are now defined in their respective layer headers:
 * - stusb4531_comm.h: Communication layer functions (I2C operations)
 * - stusb4531_logic.h: Logic layer structures and functions (data processing)
 * - stusb4531_display.h: Display layer functions (UART output)
 * 
 * This header serves as a convenience include that brings in all layers.
 */

#ifdef __cplusplus
}
#endif

#endif /* __STUSB4531_API_H */
