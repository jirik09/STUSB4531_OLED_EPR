/**
  ******************************************************************************
  * @file    stusb4531_comm.c
  * @brief   STUSB4531 Communication Layer Implementation
  * @note    Pure I2C communication functions - no logic, no display
  ******************************************************************************
  */

#include "stusb4531_comm.h"

/**
  * @brief Write a single byte to STUSB4531 register
  */
HAL_StatusTypeDef STUSB4531_WriteReg(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t data)
{
    return HAL_I2C_Mem_Write(hi2c, STUSB4531_I2C_ADDRESS, reg, I2C_MEMADD_SIZE_8BIT, &data, 1, I2C_TIMEOUT);
}

/**
  * @brief Read a single byte from STUSB4531 register
  */
HAL_StatusTypeDef STUSB4531_ReadReg(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t *data)
{
    return HAL_I2C_Mem_Read(hi2c, STUSB4531_I2C_ADDRESS, reg, I2C_MEMADD_SIZE_8BIT, data, 1, I2C_TIMEOUT);
}

/**
  * @brief Read multiple bytes from STUSB4531 registers
  */
HAL_StatusTypeDef STUSB4531_ReadRegs(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t *data, uint16_t len)
{
    return HAL_I2C_Mem_Read(hi2c, STUSB4531_I2C_ADDRESS, reg, I2C_MEMADD_SIZE_8BIT, data, len, I2C_TIMEOUT);
}

/**
  * @brief Write multiple bytes to STUSB4531 registers
  */
HAL_StatusTypeDef STUSB4531_WriteRegs(I2C_HandleTypeDef *hi2c, uint8_t reg, const uint8_t *data, uint16_t len)
{
    return HAL_I2C_Mem_Write(hi2c, STUSB4531_I2C_ADDRESS, reg, I2C_MEMADD_SIZE_8BIT, (uint8_t *)data, len, I2C_TIMEOUT);
}
