/**
  ******************************************************************************
  * @file    stusb4531_logic.h
  * @brief   STUSB4531 Business Logic Layer Header
  * @note    Pure logic functions without display/printf
  ******************************************************************************
  */

#ifndef STUSB4531_LOGIC_H
#define STUSB4531_LOGIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32c0xx_hal.h"
#include "stusb4531_api.h"
#include <stdbool.h>

/* Maximum number of PDOs supported */
#define STUSB4531_MAX_PDOS  7

/* PDO Types */
#define PDO_TYPE_FIXED      0x00
#define PDO_TYPE_BATTERY    0x01
#define PDO_TYPE_VARIABLE   0x02
#define PDO_TYPE_APDO       0x03

/**
  * @brief PDO structure
  */
typedef struct
{
    uint32_t raw;
    uint8_t type;
    uint16_t voltage_mv;
    uint16_t current_ma;
    uint32_t power_mw;
} STUSB4531_PDO_t;

/**
  * @brief Device status structure
  */
typedef struct
{
    bool initialized;
    bool attached;
    bool pd_capable;
    bool epr_ready;
    uint8_t num_pdos;
    STUSB4531_PDO_t pdos[STUSB4531_MAX_PDOS];
    uint8_t selected_pdo_index;
} STUSB4531_Status_t;

/**
  * @brief Comprehensive status structure
  */
typedef struct
{
    uint8_t cc_status;
    uint8_t pd_status;
    uint8_t alert_status;
    uint8_t typec_fsm;
    uint8_t pe_fsm;
    uint8_t vbus_fsm;
    uint8_t vbus_status;
    uint8_t monitoring_status;
    uint8_t hw_fault_status;
    uint8_t num_pdo;
    uint32_t rdo;
    uint32_t negotiated_pdo;
} STUSB4531_ComprehensiveStatus_t;

/* Logic Functions - No printf statements */

/**
  * @brief Initialize the STUSB4531 driver
  */
HAL_StatusTypeDef STUSB4531_Init(I2C_HandleTypeDef *hi2c);

/**
  * @brief Check if device is attached and PD capable
  */
HAL_StatusTypeDef STUSB4531_CheckStatus(I2C_HandleTypeDef *hi2c, STUSB4531_Status_t *status);

/**
  * @brief Parse a raw PDO value according to USB PD specification
  */
void STUSB4531_ParsePDO(uint32_t raw_pdo, STUSB4531_PDO_t *pdo);

/**
  * @brief Read and parse source PDOs
  */
HAL_StatusTypeDef STUSB4531_ReadSourcePDOs(I2C_HandleTypeDef *hi2c, STUSB4531_Status_t *status);

/**
  * @brief Select the highest power PDO
  */
uint8_t STUSB4531_SelectHighestPowerPDO(STUSB4531_Status_t *status);

/**
  * @brief Read all comprehensive status information
  */
HAL_StatusTypeDef STUSB4531_ReadComprehensiveStatus(I2C_HandleTypeDef *hi2c, STUSB4531_ComprehensiveStatus_t *comp_status);

/**
  * @brief Request source capabilities from attached source
  */
HAL_StatusTypeDef STUSB4531_GetSourceCapabilities(I2C_HandleTypeDef *hi2c);

/**
  * @brief Read device ID (from DEVICE_HW register)
  */
HAL_StatusTypeDef STUSB4531_ReadDeviceID(I2C_HandleTypeDef *hi2c, uint8_t *device_id);

/**
  * @brief Clear alert status register
  */
HAL_StatusTypeDef STUSB4531_ClearAlerts(I2C_HandleTypeDef *hi2c, uint8_t alert_mask);

/**
  * @brief Read and decode alert status
  */
HAL_StatusTypeDef STUSB4531_ReadAlertStatus(I2C_HandleTypeDef *hi2c, uint8_t *alert_status);

/**
  * @brief Perform soft reset of STUSB4531
  */
HAL_StatusTypeDef STUSB4531_SoftReset(I2C_HandleTypeDef *hi2c);

#ifdef __cplusplus
}
#endif

#endif /* STUSB4531_LOGIC_H */
