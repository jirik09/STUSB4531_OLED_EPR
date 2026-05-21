/**
  ******************************************************************************
  * @file    stusb4531_comm.h
  * @brief   STUSB4531 Communication Layer Header
  * @note    Pure I2C communication functions - no logic, no display
  ******************************************************************************
  */

#ifndef STUSB4531_COMM_H
#define STUSB4531_COMM_H

#include "stm32c0xx_hal.h"

/* I2C Address */
#define STUSB4531_I2C_ADDRESS  0x50  // 7-bit address 0x28, shifted left

/* Timeout */
#define I2C_TIMEOUT  100  // I2C timeout in ms

/**
  * @brief Write a single byte to STUSB4531 register
  * @param hi2c: I2C handle
  * @param reg: Register address
  * @param data: Data byte to write
  * @return HAL status
  */
HAL_StatusTypeDef STUSB4531_WriteReg(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t data);

/**
  * @brief Read a single byte from STUSB4531 register
  * @param hi2c: I2C handle
  * @param reg: Register address
  * @param data: Pointer to store read byte
  * @return HAL status
  */
HAL_StatusTypeDef STUSB4531_ReadReg(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t *data);

/**
  * @brief Read multiple bytes from STUSB4531 registers
  * @param hi2c: I2C handle
  * @param reg: Starting register address
  * @param data: Pointer to buffer to store read data
  * @param len: Number of bytes to read
  * @return HAL status
  */
HAL_StatusTypeDef STUSB4531_ReadRegs(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t *data, uint16_t len);

/**
  * @brief Write multiple bytes to STUSB4531 registers
  * @param hi2c: I2C handle
  * @param reg: Starting register address
  * @param data: Pointer to data to write
  * @param len: Number of bytes to write
  * @return HAL status
  */
HAL_StatusTypeDef STUSB4531_WriteRegs(I2C_HandleTypeDef *hi2c, uint8_t reg, const uint8_t *data, uint16_t len);

#endif /* STUSB4531_COMM_H */
