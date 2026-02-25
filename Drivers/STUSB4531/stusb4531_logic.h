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

/* APDO sub-types (bits [29:28] of APDO) */
#define APDO_SUBTYPE_PPS    0x00  /* SPR Programmable Power Supply */
#define APDO_SUBTYPE_EPR_AVS 0x01 /* EPR Adjustable Voltage Supply (USB PD 3.1) */

/* USB PD 3.1 EPR – control and extended message types */
#define USBPD_CTRL_TYPE_GET_SRC_CAP_EXT  0x0F  /* Get_Source_Cap_Extended (PD 3.0+) */
#define USBPD_EXT_TYPE_SRC_CAP_EXT       0x01  /* Source_Capabilities_Extended       */
#define USBPD_EXT_TYPE_EPR_MODE          0x06  /* EPR_Mode (USB PD 3.1)              */

/* EPR_Mode Data Object – Action field (bits [31:24]) */
#define EPR_MODE_ACTION_ENTER    0x01  /* Sink → Source: Enter EPR Mode  */
#define EPR_MODE_ACTION_ACKED    0x02  /* Source → Sink: EPR Mode Acked  */
#define EPR_MODE_ACTION_REJECTED 0x03  /* Source → Sink: EPR Mode Rejected */

/**
  * @brief PDO structure
  */
typedef struct
{
    uint32_t raw;
    uint8_t type;
    uint8_t apdo_subtype;   /* APDO sub-type: 0=PPS, 1=EPR_AVS */
    uint16_t voltage_mv;
    uint16_t min_voltage_mv; /* For APDO/EPR_AVS: minimum voltage */
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
    bool epr_ready;       /* Source advertises EPR capability (PDO1 bit 23) */
    bool epr_mode_active; /* EPR Mode Entry was acked by the source */
    uint8_t num_pdos;
    STUSB4531_PDO_t pdos[STUSB4531_MAX_PDOS];
    uint8_t selected_pdo_index;
    STUSB4531_PDO_t negotiated_pdo; /* The actively negotiated PDO (from DPM_SRC_PDO_NEGOCIATED) */
    uint16_t cc_current_ma;         /* Current advertised via CC Rp (900/1500/3000 mA), 0 if unknown */
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

/**
  * @brief Parsed Source_Capabilities_Extended fields (from USB PD 3.0/3.1 SKEDB)
  */
typedef struct
{
    bool     valid;
    uint16_t vid;            /* Vendor ID */
    uint16_t pid;            /* Product ID */
    uint8_t  src_pdp_w;     /* Source PDP in Watts (byte 23 of SKEDB) */
    uint8_t  src_max_pdp_w; /* Source Max PDP for EPR in Watts (byte 24, PD 3.1) */
    uint8_t  source_inputs;  /* Source Input bitmap (byte 21 of SKEDB) */
} STUSB4531_SrcCapExt_t;

/**
  * @brief EPR negotiation state and results
  */
typedef struct
{
    bool                  epr_mode_active;    /* EPR mode successfully entered */
    bool                  epr_rejected;       /* Source rejected EPR mode entry */
    bool                  src_cap_ext_valid;  /* Source_Cap_Extended was received */
    STUSB4531_SrcCapExt_t src_cap_ext;        /* Parsed extended source capabilities */
} STUSB4531_EPR_t;

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
  * @brief Set selected_pdo_index from the RDO object position reported by STUSB4531
  * @param status  Status struct whose selected_pdo_index will be updated
  * @param rdo     Raw RDO register value (from DPM_RDO)
  * @retval Index of the selected PDO (0-based), or 0 if RDO is absent/invalid
  * @note  RDO bits [30:28] carry the 1-indexed object position of the chosen PDO.
  *        Falls back to highest-power selection when RDO is 0 or out of range.
  */
uint8_t STUSB4531_SetSelectedFromRDO(STUSB4531_Status_t *status, uint32_t rdo);

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

/**
  * @brief Configure the STUSB4531 sink registers for EPR capability
  * @note  Sets DEVICE_PDP to 240 W and enables EPR_CAPABLE in SNK_PDO_CAPABILITIES.
  *        Must be called after Init and before the PD contract is established
  *        (or after a soft-reset to force re-negotiation).
  */
HAL_StatusTypeDef STUSB4531_ConfigureEPRSink(I2C_HandleTypeDef *hi2c);

/**
  * @brief Send Get_Source_Cap_Extended control message (USB PD 3.0+)
  * @note  Uses PRL TX bypass.  Source should respond with
  *        Source_Capabilities_Extended extended message.
  */
HAL_StatusTypeDef STUSB4531_SendGetSrcCapExt(I2C_HandleTypeDef *hi2c);

/**
  * @brief Send EPR_Mode(Enter) extended message (USB PD 3.1)
  * @note  Uses PRL TX bypass.  Source responds with EPR_Mode(Acked/Rejected).
  */
HAL_StatusTypeDef STUSB4531_SendEPRModeEnter(I2C_HandleTypeDef *hi2c);

/**
  * @brief Run full EPR negotiation sequence
  *
  * Performs, in order:
  *   1. Send Get_Source_Cap_Extended and parse the response.
  *   2. Send EPR_Mode(Enter) and process the Acked/Rejected response.
  *   3. If EPR was acked, refresh the DPM_SRC_PDO registers.
  *
  * @param hi2c    I2C handle
  * @param status  Device status struct (updated with EPR PDOs on success)
  * @param epr     EPR result struct filled by this function
  * @retval HAL status (HAL_OK even when EPR is rejected – check epr->epr_rejected)
  */
HAL_StatusTypeDef STUSB4531_RunEPRSequence(I2C_HandleTypeDef *hi2c,
                                           STUSB4531_Status_t *status,
                                           STUSB4531_EPR_t    *epr);

#ifdef __cplusplus
}
#endif

#endif /* STUSB4531_LOGIC_H */
