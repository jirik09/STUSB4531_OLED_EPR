/**
  ******************************************************************************
  * @file    stusb4531_logic.c
  * @brief   STUSB4531 Business Logic Layer Implementation
  * @note    Pure logic functions without display/printf
  ******************************************************************************
  */

#include "stusb4531_logic.h"
#include "stusb4531_comm.h"
#include <string.h>

/**
  * @brief Initialize the STUSB4531 driver
  */
HAL_StatusTypeDef STUSB4531_Init(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef status;
    uint8_t device_hw;
    
    status = STUSB4531_ReadReg(hi2c, STUSB4531_DEVICE_HW_ADD, &device_hw);
    
    if (status == HAL_OK)
    {
        STUSB4531_ClearAlerts(hi2c, 0xFF);
        HAL_Delay(10);
        return HAL_OK;
    }
    
    return status;
}

/**
  * @brief Check if device is attached and PD capable
  *
  * @note  PD_STATUS (0x1B) does NOT contain contract/connected flags.
  *        Its bits are: data_role(0), vconn_on(5), vconn_src(6).
  *        PD contract status is determined via the PE FSM register (0x21).
  *        PE_SNK_READY (0x10) and PE_SNK_READY_SENDING (0x12) indicate
  *        an explicit PD contract is active.
  */
HAL_StatusTypeDef STUSB4531_CheckStatus(I2C_HandleTypeDef *hi2c, STUSB4531_Status_t *status)
{
    HAL_StatusTypeDef hal_status;
    uint8_t cc_status;
    uint8_t pe_fsm;

    hal_status = STUSB4531_ReadReg(hi2c, STUSB4531_CC_STATUS_ADD, &cc_status);

    if (hal_status == HAL_OK)
    {
        status->attached = (cc_status & (1 << STUSB4531_CC_STATUS_ATTACH_STATUS_SHIFT)) ? true : false;
        status->initialized = true;

        /* PD contract is active when PE FSM is in SNK_READY or later states */
        hal_status = STUSB4531_ReadReg(hi2c, STUSB4531_PE_FSM_ADD, &pe_fsm);
        if (hal_status == HAL_OK)
        {
            /* 0x10 = PE_SNK_READY, 0x11 = PE_SNK_GET_SOURCE_CAP, 0x12 = PE_SNK_READY_SENDING */
            status->pd_capable = (pe_fsm >= 0x10) ? true : false;
        }
    }

    return hal_status;
}

/**
  * @brief Parse a raw PDO value according to USB PD specification
  */
void STUSB4531_ParsePDO(uint32_t raw_pdo, STUSB4531_PDO_t *pdo)
{
    pdo->raw = raw_pdo;
    pdo->type = (raw_pdo >> 30) & 0x03;
    
    if (pdo->type == PDO_TYPE_FIXED)
    {
        uint16_t voltage_50mv = (raw_pdo >> 10) & 0x3FF;
        pdo->voltage_mv = voltage_50mv * 50;
        
        uint16_t current_10ma = raw_pdo & 0x3FF;
        pdo->current_ma = current_10ma * 10;
        
        pdo->power_mw = ((uint32_t)pdo->voltage_mv * pdo->current_ma) / 1000;
    }
    else if (pdo->type == PDO_TYPE_VARIABLE)
    {
        uint16_t max_voltage_50mv = (raw_pdo >> 20) & 0x3FF;
        pdo->voltage_mv = max_voltage_50mv * 50;
        
        uint16_t max_current_10ma = raw_pdo & 0x3FF;
        pdo->current_ma = max_current_10ma * 10;
        
        pdo->power_mw = ((uint32_t)pdo->voltage_mv * pdo->current_ma) / 1000;
    }
    else if (pdo->type == PDO_TYPE_BATTERY)
    {
        uint16_t max_voltage_50mv = (raw_pdo >> 20) & 0x3FF;
        pdo->voltage_mv = max_voltage_50mv * 50;
        
        uint16_t max_power_250mw = raw_pdo & 0x3FF;
        pdo->power_mw = max_power_250mw * 250;
        pdo->current_ma = 0;
    }
    else
    {
        pdo->voltage_mv = 0;
        pdo->current_ma = 0;
        pdo->power_mw = 0;
    }
}

/**
  * @brief Read and parse source PDOs
  */
HAL_StatusTypeDef STUSB4531_ReadSourcePDOs(I2C_HandleTypeDef *hi2c, STUSB4531_Status_t *status)
{
    HAL_StatusTypeDef hal_status;
    uint8_t pdo_data[28];
    
    hal_status = STUSB4531_ReadRegs(hi2c, STUSB4531_DPM_SRC_PDO1_ADD, pdo_data, 28);
    if (hal_status != HAL_OK)
    {
        return hal_status;
    }
    
    uint8_t num_pdos = 0;
    status->epr_ready = false;
    for (uint8_t i = 0; i < STUSB4531_MAX_PDOS; i++)
    {
        uint8_t offset = i * 4;
        uint32_t raw_pdo = pdo_data[offset] | 
                          (pdo_data[offset + 1] << 8) | 
                          (pdo_data[offset + 2] << 16) | 
                          (pdo_data[offset + 3] << 24);
        
        if (i == 0 && raw_pdo != 0)
        {
            // Check EPR Mode Capable bit (bit 23) in PDO1
            if (raw_pdo & (1 << 23))
            {
                status->epr_ready = true;
            }
        }

        if (raw_pdo != 0)
        {
            STUSB4531_ParsePDO(raw_pdo, &status->pdos[num_pdos]);
            num_pdos++;
        }
    }
    
    status->num_pdos = num_pdos;
    
    return HAL_OK;
}


/**
  * @brief Set selected_pdo_index from the RDO object position reported by STUSB4531
  */
uint8_t STUSB4531_SetSelectedFromRDO(STUSB4531_Status_t *status, uint32_t rdo)
{
    if (rdo != 0 && status->num_pdos > 0)
    {
        /* Bits [30:28] = object position, 1-indexed (PDO1 = 1, PDO2 = 2, ...) */
        uint8_t obj_pos = (rdo >> 28) & 0x07;
        if (obj_pos >= 1 && obj_pos <= status->num_pdos)
        {
            status->selected_pdo_index = obj_pos - 1;
            return status->selected_pdo_index;
        }
    }
    /* Fallback: use highest-power PDO if RDO is absent or position is out of range */
    return 0;
}

/**
  * @brief Read all comprehensive status information
  */
HAL_StatusTypeDef STUSB4531_ReadComprehensiveStatus(I2C_HandleTypeDef *hi2c, STUSB4531_ComprehensiveStatus_t *comp_status)
{
    HAL_StatusTypeDef status;
    
    status = STUSB4531_ReadReg(hi2c, STUSB4531_CC_STATUS_ADD, &comp_status->cc_status);
    if (status != HAL_OK) return status;
    
    status = STUSB4531_ReadReg(hi2c, STUSB4531_PD_STATUS_ADD, &comp_status->pd_status);
    if (status != HAL_OK) return status;
    
    status = STUSB4531_ReadReg(hi2c, STUSB4531_ALERT_STATUS_ADD, &comp_status->alert_status);
    if (status != HAL_OK) return status;
    
    status = STUSB4531_ReadReg(hi2c, STUSB4531_TYPEC_FSM_ADD, &comp_status->typec_fsm);
    if (status != HAL_OK) return status;
    
    status = STUSB4531_ReadReg(hi2c, STUSB4531_PE_FSM_ADD, &comp_status->pe_fsm);
    if (status != HAL_OK) return status;
    
    status = STUSB4531_ReadReg(hi2c, STUSB4531_VBUS_FSM_ADD, &comp_status->vbus_fsm);
    if (status != HAL_OK) return status;
    
    status = STUSB4531_ReadReg(hi2c, STUSB4531_VBUS_STATUS_ADD, &comp_status->vbus_status);
    if (status != HAL_OK) return status;
    
    status = STUSB4531_ReadReg(hi2c, STUSB4531_MONITORING_STATUS_ADD, &comp_status->monitoring_status);
    if (status != HAL_OK) return status;
    
    status = STUSB4531_ReadReg(hi2c, STUSB4531_HW_FAULT_STATUS_ADD, &comp_status->hw_fault_status);
    if (status != HAL_OK) return status;
    
    status = STUSB4531_ReadReg(hi2c, STUSB4531_NUM_PDO_ADD, &comp_status->num_pdo);
    if (status != HAL_OK) return status;

    /* NUM_PDO register: bits [6:4] = NUM_SRC_PDO (received from source)
     * bits [1:0] = NUM_SNK_FIX_PDO (locally configured sink PDOs)
     * The comprehensive status stores the raw register; the display layer
     * must extract bits [6:4] to get the source PDO count. */
    
    uint8_t rdo_data[4];
    status = STUSB4531_ReadRegs(hi2c, STUSB4531_DPM_RDO_ADD, rdo_data, 4);
    if (status == HAL_OK)
    {
        comp_status->rdo = rdo_data[0] | (rdo_data[1] << 8) | (rdo_data[2] << 16) | (rdo_data[3] << 24);
    }
    
    uint8_t nego_pdo_data[4];
    status = STUSB4531_ReadRegs(hi2c, STUSB4531_DPM_SRC_PDO_NEGOCIATED_ADD, nego_pdo_data, 4);
    if (status == HAL_OK)
    {
        comp_status->negotiated_pdo = nego_pdo_data[0] | (nego_pdo_data[1] << 8) | 
                                      (nego_pdo_data[2] << 16) | (nego_pdo_data[3] << 24);
    }
    
    return HAL_OK;
}

/**
  * @brief Request source capabilities from attached source
  */
HAL_StatusTypeDef STUSB4531_GetSourceCapabilities(I2C_HandleTypeDef *hi2c)
{
    return STUSB4531_WriteReg(hi2c, STUSB4531_COMMAND_ADD, 0x01);
}

/**
  * @brief Read device ID (from DEVICE_HW register)
  */
HAL_StatusTypeDef STUSB4531_ReadDeviceID(I2C_HandleTypeDef *hi2c, uint8_t *device_id)
{
    return STUSB4531_ReadReg(hi2c, STUSB4531_DEVICE_HW_ADD, device_id);
}

/**
  * @brief Clear alert status register
  */
HAL_StatusTypeDef STUSB4531_ClearAlerts(I2C_HandleTypeDef *hi2c, uint8_t alert_mask)
{
    return STUSB4531_WriteReg(hi2c, STUSB4531_ALERT_STATUS_ADD, alert_mask);
}

/**
  * @brief Read and decode alert status
  */
HAL_StatusTypeDef STUSB4531_ReadAlertStatus(I2C_HandleTypeDef *hi2c, uint8_t *alert_status)
{
    return STUSB4531_ReadReg(hi2c, STUSB4531_ALERT_STATUS_ADD, alert_status);
}

/**
  * @brief Perform soft reset of STUSB4531
  */
HAL_StatusTypeDef STUSB4531_SoftReset(I2C_HandleTypeDef *hi2c)
{
    return STUSB4531_WriteReg(hi2c, STUSB4531_COMMAND_ADD, 0x03);
}
