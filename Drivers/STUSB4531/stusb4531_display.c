/**
  ******************************************************************************
  * @file    stusb4531_display.c
  * @brief   STUSB4531 Display Layer Implementation
  * @note    Functions for displaying/printing STUSB4531 data
  ******************************************************************************
  */

#include "stusb4531_display.h"
#include "stusb4531_comm.h"
#include "stusb4531_api.h"
#include "stusb4531_logic.h"
#include <stdio.h>

extern int printf(const char *format, ...);

/**
  * @brief Read and display detailed PDO information from DPM_SRC_PDO registers
  */
void STUSB4531_DisplayDetailedPDOs(I2C_HandleTypeDef *hi2c, uint8_t num_pdos, uint8_t pe_fsm, uint8_t pd_status)
{
    uint16_t voltage_raw, voltage_mv;
    uint16_t current_raw, current_ma;
    uint16_t max_voltage_raw, max_voltage_mv;
    uint16_t min_voltage_raw, min_voltage_mv;
    uint16_t max_power_raw;
    uint32_t max_power_mw;
    uint32_t power_mw;
    uint8_t dual_role_power, usb_suspend, unconstrained_power, usb_comms, dual_role_data, peak_current;
    uint8_t apdo_type;
    uint16_t pps_max_v, pps_min_v;
    uint8_t pps_max_i;
    
    printf("\r\n========== DETAILED PDO INFORMATION ==========\r\n");
    printf("Reading from DPM_SRC_PDO registers (0xA0-0xB8)\r\n");
    printf("Note: These registers store PDOs received from the power supply\r\n");
    printf("      They are populated automatically during USB-PD negotiation\r\n\r\n");
    printf("Number of valid PDOs: %d\r\n\r\n", num_pdos);
    
    for (uint8_t i = 0; i < STUSB4531_MAX_PDOS; i++)
    {
        uint8_t reg_addr = STUSB4531_DPM_SRC_PDO1_ADD + (i * 4);
        uint8_t pdo_bytes[4];
        
        if (STUSB4531_ReadRegs(hi2c, reg_addr, pdo_bytes, 4) == HAL_OK)
        {
            uint32_t raw_pdo = pdo_bytes[0] | 
                              (pdo_bytes[1] << 8) | 
                              (pdo_bytes[2] << 16) | 
                              (pdo_bytes[3] << 24);
            
            if (raw_pdo != 0 || i < num_pdos)
            {
                printf("--- PDO %d (Register 0x%02X) ---\r\n", i + 1, reg_addr);
                printf("Raw value: 0x%08lX\r\n", raw_pdo);
                
                if (raw_pdo == 0)
                {
                    printf("  (Empty/Not used)\r\n\r\n");
                    continue;
                }
                
                uint8_t pdo_type = (raw_pdo >> 30) & 0x03;
                printf("Bits 31-30 (PDO Type): %d (", pdo_type);
                
                switch(pdo_type)
                {
                    case 0x00:
                    {
                        printf("Fixed Supply)\r\n");
                        
                        voltage_raw = (raw_pdo >> 10) & 0x3FF;
                        voltage_mv = voltage_raw * 50;
                        printf("Bits 10-19 (Voltage): 0x%03X = %d units × 50mV = %dmV (%d.%dV)\r\n",
                               voltage_raw, voltage_raw, voltage_mv, 
                               voltage_mv / 1000, (voltage_mv % 1000) / 100);
                        
                        current_raw = raw_pdo & 0x3FF;
                        current_ma = current_raw * 10;
                        printf("Bits 0-9 (Max Current): 0x%03X = %d units × 10mA = %dmA (%d.%dA)\r\n",
                               current_raw, current_raw, current_ma,
                               current_ma / 1000, (current_ma % 1000) / 100);
                        
                        power_mw = ((uint32_t)voltage_mv * current_ma) / 1000;
                        printf("Calculated Power: %dmW (%d.%dW)\r\n",
                               power_mw, power_mw / 1000, (power_mw % 1000) / 100);
                        
                        dual_role_power = (raw_pdo >> 29) & 0x01;
                        usb_suspend = (raw_pdo >> 28) & 0x01;
                        unconstrained_power = (raw_pdo >> 27) & 0x01;
                        usb_comms = (raw_pdo >> 26) & 0x01;
                        dual_role_data = (raw_pdo >> 25) & 0x01;
                        peak_current = (raw_pdo >> 20) & 0x03;
                        
                        printf("Bit 29 (Dual-Role Power): %d\r\n", dual_role_power);
                        printf("Bit 28 (USB Suspend Supported): %d\r\n", usb_suspend);
                        printf("Bit 27 (Unconstrained Power): %d\r\n", unconstrained_power);
                        printf("Bit 26 (USB Communications Capable): %d\r\n", usb_comms);
                        printf("Bit 25 (Dual-Role Data): %d\r\n", dual_role_data);
                        printf("Bits 21-20 (Peak Current): %d\r\n", peak_current);
                        break;
                    }
                        
                    case 0x01:
                    {
                        printf("Battery Supply)\r\n");
                        
                        max_voltage_raw = (raw_pdo >> 20) & 0x3FF;
                        max_voltage_mv = max_voltage_raw * 50;
                        printf("Bits 20-29 (Max Voltage): 0x%03X = %d units × 50mV = %dmV\r\n",
                               max_voltage_raw, max_voltage_raw, max_voltage_mv);
                        
                        min_voltage_raw = (raw_pdo >> 10) & 0x3FF;
                        min_voltage_mv = min_voltage_raw * 50;
                        printf("Bits 10-19 (Min Voltage): 0x%03X = %d units × 50mV = %dmV\r\n",
                               min_voltage_raw, min_voltage_raw, min_voltage_mv);
                        
                        max_power_raw = raw_pdo & 0x3FF;
                        max_power_mw = max_power_raw * 250;
                        printf("Bits 0-9 (Max Power): 0x%03X = %d units × 250mW = %dmW\r\n",
                               max_power_raw, max_power_raw, max_power_mw);
                        break;
                    }
                        
                    case 0x02:
                    {
                        printf("Variable Supply)\r\n");
                        
                        max_voltage_raw = (raw_pdo >> 20) & 0x3FF;
                        max_voltage_mv = max_voltage_raw * 50;
                        printf("Bits 20-29 (Max Voltage): 0x%03X = %d units × 50mV = %dmV\r\n",
                               max_voltage_raw, max_voltage_raw, max_voltage_mv);
                        
                        min_voltage_raw = (raw_pdo >> 10) & 0x3FF;
                        min_voltage_mv = min_voltage_raw * 50;
                        printf("Bits 10-19 (Min Voltage): 0x%03X = %d units × 50mV = %dmV\r\n",
                               min_voltage_raw, min_voltage_raw, min_voltage_mv);
                        
                        current_raw = raw_pdo & 0x3FF;
                        current_ma = current_raw * 10;
                        printf("Bits 0-9 (Max Current): 0x%03X = %d units × 10mA = %dmA\r\n",
                               current_raw, current_raw, current_ma);
                        break;
                    }
                        
                    case 0x03:
                    {
                        printf("Augmented/PPS)\r\n");
                        
                        apdo_type = (raw_pdo >> 28) & 0x03;
                        printf("Bits 29-28 (APDO Type): %d\r\n", apdo_type);
                        
                        pps_max_v = (raw_pdo >> 17) & 0xFF;
                        printf("Bits 17-24 (Max Voltage): 0x%02X = %d units × 100mV = %dmV\r\n",
                               pps_max_v, pps_max_v, pps_max_v * 100);
                        
                        pps_min_v = (raw_pdo >> 8) & 0xFF;
                        printf("Bits 8-15 (Min Voltage): 0x%02X = %d units × 100mV = %dmV\r\n",
                               pps_min_v, pps_min_v, pps_min_v * 100);
                        
                        pps_max_i = raw_pdo & 0x7F;
                        printf("Bits 0-6 (Max Current): 0x%02X = %d units × 50mA = %dmA\r\n",
                               pps_max_i, pps_max_i, pps_max_i * 50);
                        break;
                    }
                }
                
                printf("\r\n");
            }
        }
        else
        {
            printf("ERROR: Failed to read PDO %d from register 0x%02X\r\n", i + 1, reg_addr);
        }
    }
    
    bool has_empty_pdos = false;
    for (uint8_t i = 0; i < num_pdos && i < STUSB4531_MAX_PDOS; i++)
    {
        uint8_t reg_addr = STUSB4531_DPM_SRC_PDO1_ADD + (i * 4);
        uint8_t pdo_bytes[4];
        
        if (STUSB4531_ReadRegs(hi2c, reg_addr, pdo_bytes, 4) == HAL_OK)
        {
            uint32_t raw_pdo = pdo_bytes[0] | (pdo_bytes[1] << 8) | 
                              (pdo_bytes[2] << 16) | (pdo_bytes[3] << 24);
            if (raw_pdo == 0)
            {
                has_empty_pdos = true;
                break;
            }
        }
    }
    
    if (has_empty_pdos && num_pdos > 0)
    {
        printf("Diagnostic Info:\r\n");
        printf("  Current PE FSM: 0x%02X ", pe_fsm);
        switch(pe_fsm) {
            case 0x00: printf("(PE_SNK_Startup - Initial state)\r\n"); break;
            case 0x01: printf("(PE_SNK_Discovery)\r\n"); break;
            case 0x02: printf("(PE_SNK_Wait_for_Capabilities)\r\n"); break;
            case 0x03: printf("(PE_SNK_Evaluate_Capability)\r\n"); break;
            case 0x04: printf("(PE_SNK_Select_Capability)\r\n"); break;
            case 0x10: printf("(PE_SNK_Ready - Normal state) OK\r\n"); break;
            case 0x12: printf("(PE_SNK_Ready_Sending) OK\r\n"); break;
            default: printf("(State 0x%02X)\r\n", pe_fsm); break;
        }
        printf("  Expected: 0x10 (PE_SNK_READY) for valid PD contract\r\n\r\n");
        /* PD_STATUS (0x1B): bits are data_role(0), vconn_on(5), vconn_src(6)
         * Per STUSB4531 datasheet - NO contract/connected flags in this register. */
        printf("  PD Status (0x1B) = 0x%02X\r\n", pd_status);
        printf("    Bit 0 (DATA_ROLE): %s\r\n", (pd_status & 0x01) ? "DFP" : "UFP");
        printf("    Bit 5 (VCONN_ON): %d\r\n", (pd_status >> 5) & 0x01);
        printf("    Bit 6 (VCONN_SRC): %d\r\n", (pd_status >> 6) & 0x01);
    }
    
    printf("==============================================\r\n");
}

/**
  * @brief Display sink configuration registers
  */
void STUSB4531_DisplaySinkConfiguration(I2C_HandleTypeDef *hi2c)
{
    uint8_t reg_value;
    uint16_t reg_value_16;
    uint8_t byte_low, byte_high;
    
    printf("\r\n========== SINK CONFIGURATION ==========\r\n");
    
    if (STUSB4531_ReadReg(hi2c, STUSB4531_DEVICE_PDP_ADD, &reg_value) == HAL_OK)
    {
        printf("--- DEVICE_PDP (0x51) = 0x%02X ---\r\n", reg_value);
        printf("  Power Data Path configuration\r\n");
        printf("  Bits 7-0: PDP value = %d (× 0.25W = %.2fW max)\r\n\r\n", 
               reg_value, reg_value * 0.25f);
    }
    
    if (STUSB4531_ReadReg(hi2c, STUSB4531_NUM_PDO_ADD, &reg_value) == HAL_OK)
    {
        printf("--- NUM_PDO (0x52) = 0x%02X ---\r\n", reg_value);
        printf("  Number of valid source PDOs: %d\r\n\r\n", reg_value & 0x07);
    }
    
    if (STUSB4531_ReadReg(hi2c, STUSB4531_SNK_PDO_PARAMS_ADD, &reg_value) == HAL_OK)
    {
        printf("--- SNK_PDO_PARAMS (0x53) = 0x%02X ---\r\n", reg_value);
        printf("  Sink PDO Parameters\r\n");
        uint8_t vsafe_5v_current = (reg_value >> 6) & 0x03;
        uint8_t pdo3_vsel = (reg_value >> 4) & 0x03;
        uint8_t pdo2_vsel = (reg_value >> 2) & 0x03;
        uint8_t flex_current = reg_value & 0x03;
        
        printf("  Bits 7-6 (VSAFE_5V_CURRENT): %d ", vsafe_5v_current);
        switch(vsafe_5v_current) {
            case 0: printf("(Default/900mA)\r\n"); break;
            case 1: printf("(1.5A)\r\n"); break;
            case 2: printf("(3.0A)\r\n"); break;
            case 3: printf("(5.0A)\r\n"); break;
        }
        printf("  Bits 5-4 (PDO3_VSEL): %d ", pdo3_vsel);
        if (pdo3_vsel == 0) printf("(Variable)\r\n");
        else printf("(%dV)\r\n", pdo3_vsel * 3 + 6);
        printf("  Bits 3-2 (PDO2_VSEL): %d ", pdo2_vsel);
        if (pdo2_vsel == 0) printf("(Variable)\r\n");
        else printf("(%dV)\r\n", pdo2_vsel * 3 + 6);
        printf("  Bits 1-0 (FLEX_CURRENT): %d ", flex_current);
        switch(flex_current) {
            case 0: printf("(Default)\r\n"); break;
            case 1: printf("(1.5A)\r\n"); break;
            case 2: printf("(3.0A)\r\n"); break;
            case 3: printf("(5.0A)\r\n"); break;
        }
        printf("\r\n");
    }
    
    if (STUSB4531_ReadReg(hi2c, STUSB4531_SNK_PDO_CAPABILITIES_ADD, &byte_low) == HAL_OK &&
        STUSB4531_ReadReg(hi2c, STUSB4531_SNK_PDO_CAPABILITIES_ADD + 1, &byte_high) == HAL_OK)
    {
        reg_value_16 = byte_low | (byte_high << 8);
        printf("--- SNK_PDO_CAPABILITIES (0x54-0x55) = 0x%04X ---\r\n", reg_value_16);
        printf("  Sink PDO Capabilities\r\n");
        uint8_t vsel_var_high = (reg_value_16 >> 8) & 0xFF;
        uint8_t vsel_var_low = (reg_value_16 >> 6) & 0x03;
        uint8_t epr_capable = (reg_value_16 >> 5) & 0x01;
        uint8_t drd_capable = (reg_value_16 >> 4) & 0x01;
        uint8_t usb_com_capable = (reg_value_16 >> 2) & 0x01;
        uint8_t unconstrained_power = (reg_value_16 >> 1) & 0x01;
        uint8_t higher_capability = reg_value_16 & 0x01;
        
        printf("  Bits 15-8 (VSEL_VARIABLE_HIGH): 0x%02X\r\n", vsel_var_high);
        printf("  Bits 7-6 (VSEL_VARIABLE_LOW): %d\r\n", vsel_var_low);
        printf("  Bit 5 (EPR_CAPABLE): %d (%s)\r\n", epr_capable, epr_capable ? "EPR" : "SPR");
        printf("  Bit 4 (DRD_CAPABLE): %d (%s)\r\n", drd_capable, drd_capable ? "Dual-Role Data" : "Single DR");
        printf("  Bit 2 (USB_COM_CAPABLE): %d (%s)\r\n", usb_com_capable, usb_com_capable ? "USB Data" : "No USB Data");
        printf("  Bit 1 (UNCONSTRAINED_POWER): %d\r\n", unconstrained_power);
        printf("  Bit 0 (HIGHER_CAPABILITY): %d (%s)\r\n", higher_capability, higher_capability ? "More than 5V" : "Only 5V");
        printf("\r\n");
    }
    
    if (STUSB4531_ReadReg(hi2c, STUSB4531_ALGO_ADD, &reg_value) == HAL_OK)
    {
        printf("--- ALGO (0x56) = 0x%02X ---\r\n", reg_value);
        printf("  Algorithm Configuration\r\n");
        printf("  Bits 7-0: Algorithm settings = 0x%02X\r\n\r\n", reg_value);
    }
    
    if (STUSB4531_ReadReg(hi2c, STUSB4531_SNK_APDO_FILL_1_ADD, &reg_value) == HAL_OK)
    {
        printf("--- SNK_APDO_FILL_1 (0x57) = 0x%02X ---\r\n", reg_value);
        printf("  APDO/PPS Max Voltage\r\n");
        printf("  Bits 7-0: Max Voltage = %d units (× 100mV = %dmV)\r\n\r\n", 
               reg_value, reg_value * 100);
    }
    
    if (STUSB4531_ReadReg(hi2c, STUSB4531_SNK_APDO_FILL_2_ADD, &reg_value) == HAL_OK)
    {
        printf("--- SNK_APDO_FILL_2 (0x58) = 0x%02X ---\r\n", reg_value);
        printf("  APDO/PPS Min Voltage\r\n");
        printf("  Bits 7-0: Min Voltage = %d units (× 100mV = %dmV)\r\n\r\n", 
               reg_value, reg_value * 100);
    }
    
    if (STUSB4531_ReadReg(hi2c, STUSB4531_SNK_APDO_FILL_3_ADD, &reg_value) == HAL_OK)
    {
        printf("--- SNK_APDO_FILL_3 (0x59) = 0x%02X ---\r\n", reg_value);
        printf("  APDO/PPS Max Current\r\n");
        printf("  Bits 7-0: Max Current = %d units (× 50mA = %dmA)\r\n\r\n", 
               reg_value, reg_value * 50);
    }
    
    if (STUSB4531_ReadReg(hi2c, STUSB4531_DEVICE_SETTING_ADD, &reg_value) == HAL_OK)
    {
        printf("--- DEVICE_SETTING (0x5A) = 0x%02X ---\r\n", reg_value);
        printf("  Device Settings\r\n");
        printf("  Bits 7-0: Device configuration = 0x%02X\r\n\r\n", reg_value);
    }
    
    if (STUSB4531_ReadReg(hi2c, STUSB4531_REQUEST_SRC_PDP_ADD, &reg_value) == HAL_OK)
    {
        printf("--- REQUEST_SRC_PDP (0x5C) = 0x%02X ---\r\n", reg_value);
        printf("  Requested Source PDP\r\n");
        printf("  Bits 7-0: Requested PDP = %d (× 0.25W = %.2fW)\r\n\r\n", 
               reg_value, reg_value * 0.25f);
    }

    printf("==============================================\r\n");
}
