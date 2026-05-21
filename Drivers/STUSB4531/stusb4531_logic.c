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

        /* Decode advertised current from CC Rp resistor value.
         * Orientation bit selects the active CC pin:
         *   bit7=0 -> CC1 in bits[1:0], bit7=1 -> CC2 in bits[3:2]
         * State values: 0=OPEN, 1=Default(900mA), 2=1.5A, 3=3.0A */
        {
            static const uint16_t cc_rp_current_ma[4] = {0, 900, 1500, 3000};
            uint8_t orientation = (cc_status >> STUSB4531_CC_STATUS_PLUG_ORIENTATION_SHIFT) & 0x01;
            uint8_t active_cc_state = orientation ? ((cc_status >> 2) & 0x03) : (cc_status & 0x03);
            status->cc_current_ma = cc_rp_current_ma[active_cc_state];
        }

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
    pdo->apdo_subtype = 0;
    pdo->min_voltage_mv = 0;

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

        uint16_t min_voltage_50mv = (raw_pdo >> 10) & 0x3FF;
        pdo->min_voltage_mv = min_voltage_50mv * 50;

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
    else /* PDO_TYPE_APDO */
    {
        pdo->apdo_subtype = (raw_pdo >> 28) & 0x03;

        if (pdo->apdo_subtype == APDO_SUBTYPE_PPS)
        {
            /* SPR Programmable Power Supply (PPS) */
            uint16_t max_v_100mv = (raw_pdo >> 17) & 0xFF;
            pdo->voltage_mv = max_v_100mv * 100;

            uint16_t min_v_100mv = (raw_pdo >> 8) & 0xFF;
            pdo->min_voltage_mv = min_v_100mv * 100;

            uint16_t max_i_50ma = raw_pdo & 0x7F;
            pdo->current_ma = max_i_50ma * 50;

            pdo->power_mw = ((uint32_t)pdo->voltage_mv * pdo->current_ma) / 1000;
        }
        else if (pdo->apdo_subtype == APDO_SUBTYPE_EPR_AVS)
        {
            /* EPR Adjustable Voltage Supply (AVS) – USB PD 3.1 */
            /* bits[25:17]: Max Voltage in 100 mV units */
            uint16_t max_v_100mv = (raw_pdo >> 17) & 0x1FF;
            pdo->voltage_mv = max_v_100mv * 100;

            /* bits[15:8]: Min Voltage in 100 mV units */
            uint16_t min_v_100mv = (raw_pdo >> 8) & 0xFF;
            pdo->min_voltage_mv = min_v_100mv * 100;

            /* bits[6:0]: PDP in 1 W units */
            uint8_t pdp_w = raw_pdo & 0x7F;
            pdo->power_mw = (uint32_t)pdp_w * 1000;
            pdo->current_ma = (pdo->voltage_mv > 0) ?
                              (uint16_t)((pdo->power_mw * 10) / (pdo->voltage_mv / 100)) : 0;
        }
        else
        {
            pdo->voltage_mv = 0;
            pdo->current_ma = 0;
            pdo->power_mw = 0;
        }
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

/* ============================================================
 *  EPR (Extended Power Range) – USB PD 3.1 implementation
 * ============================================================ */

#include "dbg_print.h"  /* For EPR debug output on UART2 */

/**
  * @brief Configure the STUSB4531 as an EPR-capable sink (240 W max)
  */
HAL_StatusTypeDef STUSB4531_ConfigureEPRSink(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef status;
    uint8_t cap_bytes[2];

    /* Set DEVICE_PDP to 240 W (maximum EPR power) */
    status = STUSB4531_WriteReg(hi2c, STUSB4531_DEVICE_PDP_ADD, 240);
    if (status != HAL_OK) return status;

    /* Enable EPR_CAPABLE bit (bit 5) in SNK_PDO_CAPABILITIES */
    status = STUSB4531_ReadRegs(hi2c, STUSB4531_SNK_PDO_CAPABILITIES_ADD, cap_bytes, 2);
    if (status != HAL_OK) return status;

    cap_bytes[0] |= (1u << STUSB4531_SNK_PDO_CAPABILITIES_EPR_CAPABLE_SHIFT);
    status = STUSB4531_WriteRegs(hi2c, STUSB4531_SNK_PDO_CAPABILITIES_ADD, cap_bytes, 2);
    return status;
}

/* ----------------------------------------------------------
 *  PRL TX/RX helpers (bypass the built-in PE AMS)
 * ---------------------------------------------------------- */

/**
  * @brief Send a raw USB PD message via the STUSB4531 protocol layer
  *
  * @param hi2c      I2C handle
  * @param header    USB PD message header (16-bit, little-endian stored internally)
  * @param data      Payload bytes (NULL for control messages with no payload)
  * @param data_len  Number of payload bytes (0 for control messages)
  */
static HAL_StatusTypeDef PRL_SendMessage(I2C_HandleTypeDef *hi2c,
                                         uint16_t           header,
                                         const uint8_t     *data,
                                         uint8_t            data_len)
{
    HAL_StatusTypeDef status;
    uint8_t header_bytes[2] = { (uint8_t)(header & 0xFF), (uint8_t)(header >> 8) };

    /* Bypass AMS so our message can be sent regardless of PE state */
    status = STUSB4531_WriteReg(hi2c, STUSB4531_COMMAND_ADD, 0x09); /* BYPASS_AMS */
    if (status != HAL_OK) return status;
    HAL_Delay(2);

    /* Number of payload bytes (TX_HEADER bytes are not counted here) */
    status = STUSB4531_WriteReg(hi2c, STUSB4531_TX_BYTE_CNT_ADD, data_len);
    if (status != HAL_OK) return status;

    /* USB PD message header */
    status = STUSB4531_WriteRegs(hi2c, STUSB4531_TX_HEADER_ADD, header_bytes, 2);
    if (status != HAL_OK) return status;

    /* Payload (data objects / extended header + body) */
    if (data != NULL && data_len > 0)
    {
        status = STUSB4531_WriteRegs(hi2c, STUSB4531_TX_DATA_OBJ_224BITS_ADD, data, data_len);
        if (status != HAL_OK) return status;
    }

    /* Trigger transmission */
    status = STUSB4531_WriteReg(hi2c, STUSB4531_COMMAND_ADD, 0x01); /* TX_SEND_MSG */
    return status;
}

/**
  * @brief Wait for a TX-complete indication in the PRL_TRANS register
  */
static HAL_StatusTypeDef PRL_WaitTxComplete(I2C_HandleTypeDef *hi2c, uint32_t timeout_ms)
{
    uint32_t deadline = HAL_GetTick() + timeout_ms;
    while (HAL_GetTick() < deadline)
    {
        uint8_t prl_trans;
        if (STUSB4531_ReadReg(hi2c, STUSB4531_PRL_TRANS_ADD, &prl_trans) != HAL_OK)
            return HAL_ERROR;
        if (prl_trans & (1u << 7)) return HAL_ERROR; /* PRL_TX_ERR */
        if (prl_trans & (1u << 3)) return HAL_OK;    /* PRL_MSG_SENT */
        HAL_Delay(1);
    }
    return HAL_TIMEOUT;
}

/**
  * @brief Wait for a message received indication, then read header + payload
  *
  * @param hi2c        I2C handle
  * @param timeout_ms  Timeout in milliseconds
  * @param rx_header   Output: received 16-bit USB PD message header
  * @param rx_data     Output buffer (must be ≥ 30 bytes)
  * @param rx_len      Output: number of payload bytes received
  */
static HAL_StatusTypeDef PRL_WaitRxMessage(I2C_HandleTypeDef *hi2c,
                                            uint32_t           timeout_ms,
                                            uint16_t          *rx_header,
                                            uint8_t           *rx_data,
                                            uint8_t           *rx_len)
{
    uint32_t deadline = HAL_GetTick() + timeout_ms;
    while (HAL_GetTick() < deadline)
    {
        uint8_t prl_trans;
        if (STUSB4531_ReadReg(hi2c, STUSB4531_PRL_TRANS_ADD, &prl_trans) != HAL_OK)
            return HAL_ERROR;

        if (prl_trans & (1u << 2)) /* PRL_MSG_RECEIVED */
        {
            uint8_t rx_cnt = 0;
            STUSB4531_ReadReg(hi2c, STUSB4531_RX_BYTE_CNT_ADD, &rx_cnt);

            uint8_t hdr_bytes[2] = {0, 0};
            STUSB4531_ReadRegs(hi2c, STUSB4531_RX_HEADER_ADD, hdr_bytes, 2);
            *rx_header = (uint16_t)hdr_bytes[0] | ((uint16_t)hdr_bytes[1] << 8);

            if (rx_cnt > 0 && rx_cnt <= 28)
            {
                STUSB4531_ReadRegs(hi2c, STUSB4531_RX_DATA_OBJ_224BITS_ADD, rx_data, rx_cnt);
                *rx_len = rx_cnt;
            }
            else
            {
                *rx_len = 0;
            }

            /* Clear PRL_MSG_RECEIVED (write 1 to clear) */
            STUSB4531_WriteReg(hi2c, STUSB4531_PRL_TRANS_ADD, 0x04);
            return HAL_OK;
        }
        HAL_Delay(1);
    }
    return HAL_TIMEOUT;
}

/* ----------------------------------------------------------
 *  Parse Source_Capabilities_Extended
 * ---------------------------------------------------------- */

/**
  * @brief Parse a Source_Capabilities_Extended payload into STUSB4531_SrcCapExt_t
  *
  * The payload starts with the 2-byte Extended Header, then the SKEDB bytes.
  */
static void ParseSrcCapExt(const uint8_t *data, uint8_t len,
                            STUSB4531_SrcCapExt_t *cap_ext)
{
    memset(cap_ext, 0, sizeof(*cap_ext));

    /* Need at least Extended Header (2 B) + VID/PID (4 B) */
    if (len < 6)
        return;

    /* Extended Header: bits[8:0] = Data_Size */
    uint16_t ext_hdr   = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    uint16_t data_size = ext_hdr & 0x01FF;

    /* Sanity: require at least 4 SKEDB bytes after the Extended Header */
    if (data_size < 4 || (data_size + 2u) > len)
        return;

    const uint8_t *skedb = &data[2]; /* SKEDB starts after Extended Header */

    cap_ext->valid = true;
    cap_ext->vid   = (uint16_t)skedb[0] | ((uint16_t)skedb[1] << 8);
    cap_ext->pid   = (uint16_t)skedb[2] | ((uint16_t)skedb[3] << 8);

    /* Source PDP: SKEDB byte 23 (present in PD 3.0+) */
    if (data_size >= 24)
        cap_ext->src_pdp_w = skedb[23];

    /* Source Max PDP for EPR: SKEDB byte 24 (added in USB PD 3.1) */
    if (data_size >= 25)
        cap_ext->src_max_pdp_w = skedb[24];

    /* Source Inputs: SKEDB byte 21 */
    if (data_size >= 22)
        cap_ext->source_inputs = skedb[21];
}

/* ----------------------------------------------------------
 *  Public EPR API
 * ---------------------------------------------------------- */

/**
  * @brief Send Get_Source_Cap_Extended control message (USB PD 3.0+)
  *
  * Header: Extended=0, NDO=0, PPR=0(Sink), Rev=3 (0b10), DPR=0(UFP), Type=0x0F
  *         = 0000_0000_1000_1111 = 0x008F
  */
HAL_StatusTypeDef STUSB4531_SendGetSrcCapExt(I2C_HandleTypeDef *hi2c)
{
    /* Control message – no payload */
    return PRL_SendMessage(hi2c, 0x008F, NULL, 0);
}

/**
  * @brief Send EPR_Mode(Enter) extended message (USB PD 3.1)
  *
  * Header:  Extended=1, NDO=0, PPR=0(Sink), Rev=3(0b10), DPR=0(UFP), Type=0x06
  *          = 1000_0000_1000_0110 = 0x8086
  * Payload: Extended Header (DataSize=4, Chunked=0) + EPR_Mode Data Object (Action=Enter=0x01)
  */
HAL_StatusTypeDef STUSB4531_SendEPRModeEnter(I2C_HandleTypeDef *hi2c)
{
    /* Extended Header (2 bytes, little-endian): bits[8:0]=4 → 0x0004 */
    /* EPR_Mode Data Object (4 bytes, little-endian): Action=0x01 in bits[31:24] → 0x01000000 */
    static const uint8_t payload[6] = {
        0x04, 0x00,              /* Extended Header: DataSize=4, Chunked=0 */
        0x00, 0x00, 0x00, 0x01   /* EPR_Mode Data Object: Action=Enter(0x01) */
    };
    return PRL_SendMessage(hi2c, 0x8086, payload, sizeof(payload));
}

/**
  * @brief Run full EPR negotiation sequence (Get_Source_Cap_Extended → EPR_Mode Entry)
  */
HAL_StatusTypeDef STUSB4531_RunEPRSequence(I2C_HandleTypeDef *hi2c,
                                           STUSB4531_Status_t *status,
                                           STUSB4531_EPR_t    *epr)
{
    HAL_StatusTypeDef hal_status;
    uint16_t rx_header;
    uint8_t  rx_data[30];
    uint8_t  rx_len;

    memset(epr, 0, sizeof(*epr));

    /* ---- Step 1: Get_Source_Cap_Extended ---- */
    print_str("[EPR] Sending Get_Source_Cap_Extended (ctrl 0x0F)...\r\n");
    hal_status = STUSB4531_SendGetSrcCapExt(hi2c);
    if (hal_status != HAL_OK)
    {
        print_str("[EPR] TX failed status="); print_number(hal_status); print_nl();
        return hal_status;
    }

    /* Wait for TX completion */
    hal_status = PRL_WaitTxComplete(hi2c, 100);
    print_str("[EPR] TX complete status="); print_number(hal_status); print_nl();

    /* Wait for Source_Capabilities_Extended response */
    print_str("[EPR] Waiting for Source_Capabilities_Extended...\r\n");
    hal_status = PRL_WaitRxMessage(hi2c, 500, &rx_header, rx_data, &rx_len);
    print_str("[EPR] RX status="); print_number(hal_status);
    if (hal_status == HAL_OK)
    {
        bool    is_extended = (rx_header >> 15) & 0x01;
        uint8_t msg_type    = rx_header & 0x1F;
        print_str("  hdr=0x"); print_hex_n(rx_header, 4);
        print_str("  ext="); print_number((int32_t)is_extended);
        print_str("  type=0x"); print_hex8(msg_type);
        print_str("  len="); print_number(rx_len); print_nl();

        if (is_extended && msg_type == USBPD_EXT_TYPE_SRC_CAP_EXT)
        {
            print_str("[EPR] Got Source_Capabilities_Extended, parsing...\r\n");
            ParseSrcCapExt(rx_data, rx_len, &epr->src_cap_ext);
            epr->src_cap_ext_valid = epr->src_cap_ext.valid;
            if (epr->src_cap_ext.valid)
            {
                print_str("[EPR] VID=0x"); print_hex_n(epr->src_cap_ext.vid, 4);
                print_str(" PID=0x"); print_hex_n(epr->src_cap_ext.pid, 4);
                print_str(" PDP="); print_number(epr->src_cap_ext.src_pdp_w); print_str("W");
                if (epr->src_cap_ext.src_max_pdp_w > 0)
                {
                    print_str(" MaxPDP="); print_number(epr->src_cap_ext.src_max_pdp_w);
                    print_str("W");
                }
                print_nl();
            }
            else
            {
                print_str("[EPR] SrcCapExt data too short to parse\r\n");
            }
        }
        else
        {
            print_str("[EPR] Unexpected response (wanted ext type 0x01)\r\n");
        }
    }
    else
    {
        print_nl();
        print_str("[EPR] No Source_Capabilities_Extended (not supported or timeout)\r\n");
    }

    HAL_Delay(50);

    /* ---- Step 2: EPR_Mode(Enter) ---- */
    print_str("[EPR] Sending EPR_Mode(Enter) (ext 0x06, action=0x01)...\r\n");
    hal_status = STUSB4531_SendEPRModeEnter(hi2c);
    if (hal_status != HAL_OK)
    {
        print_str("[EPR] TX failed status="); print_number(hal_status); print_nl();
        return hal_status;
    }

    /* Wait for TX completion */
    hal_status = PRL_WaitTxComplete(hi2c, 100);
    print_str("[EPR] TX complete status="); print_number(hal_status); print_nl();

    /* Wait for EPR_Mode response (Acked or Rejected) */
    print_str("[EPR] Waiting for EPR_Mode response (timeout 1000ms)...\r\n");
    hal_status = PRL_WaitRxMessage(hi2c, 1000, &rx_header, rx_data, &rx_len);
    print_str("[EPR] RX status="); print_number(hal_status);
    if (hal_status == HAL_OK)
    {
        bool    is_extended = (rx_header >> 15) & 0x01;
        uint8_t msg_type    = rx_header & 0x1F;
        print_str("  hdr=0x"); print_hex_n(rx_header, 4);
        print_str("  ext="); print_number((int32_t)is_extended);
        print_str("  type=0x"); print_hex8(msg_type);
        print_str("  len="); print_number(rx_len); print_nl();

        if (is_extended && msg_type == USBPD_EXT_TYPE_EPR_MODE && rx_len >= 6)
        {
            /* Action field is in bits[31:24] of the 32-bit Data Object.
             * Payload layout: byte[0:1]=Extended Header, byte[2:5]=Data Object.
             * Data Object is little-endian → Action in most-significant byte = byte[5]. */
            uint8_t action = rx_data[5];
            print_str("[EPR] EPR_Mode action=0x"); print_hex8(action); print_nl();

            if (action == EPR_MODE_ACTION_ACKED)
            {
                print_str("[EPR] EPR_Mode ACKED -- EPR mode is now active!\r\n");
                epr->epr_mode_active    = true;
                status->epr_mode_active = true;
                /* Allow source time to send new Source_Capabilities with EPR PDOs */
                HAL_Delay(200);
                /* Refresh PDO list – EPR PDOs should now appear in DPM_SRC_PDO1-7 */
                STUSB4531_ReadSourcePDOs(hi2c, status);
                status->epr_ready = true;
                print_str("[EPR] PDOs refreshed: ");
                print_number(status->num_pdos); print_str(" PDOs now available\r\n");
            }
            else if (action == EPR_MODE_ACTION_REJECTED)
            {
                print_str("[EPR] EPR_Mode REJECTED by source\r\n");
                epr->epr_rejected = true;
            }
            else
            {
                print_str("[EPR] EPR_Mode unexpected action=0x"); print_hex8(action); print_nl();
            }
        }
        else
        {
            print_str("[EPR] Unexpected msg type (wanted ext 0x06) -- raw: ");
            for (uint8_t i = 0; i < rx_len && i < 10; i++)
            {
                print_hex8(rx_data[i]); print_str(" ");
            }
            print_nl();
        }
    }
    else
    {
        print_nl();
        print_str("[EPR] No EPR_Mode response (source does not support EPR or timeout)\r\n");
    }

    return HAL_OK;
}
