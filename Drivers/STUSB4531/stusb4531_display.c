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
#include "dbg_print.h"

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
    
    print_str("\r\n========== DETAILED PDO INFORMATION ==========\r\n");
    print_str("Number of valid PDOs: "); print_number(num_pdos); print_str("\r\n\r\n");
    
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
                print_str("--- PDO "); print_number(i + 1);
                print_str(" (Register 0x"); print_hex8(reg_addr); print_str(") ---\r\n");
                print_str("Raw value: 0x"); print_hex32(raw_pdo); print_nl();

                if (raw_pdo == 0)
                {
                    print_str("  (Empty/Not used)\r\n\r\n");
                    continue;
                }
                
                uint8_t pdo_type = (raw_pdo >> 30) & 0x03;
                switch(pdo_type)
                {
                    case 0x00:
                    {
                        print_str("PDO Type: Fixed Supply\r\n");

                        voltage_raw = (raw_pdo >> 10) & 0x3FF;
                        voltage_mv = voltage_raw * 50;
                        print_str("  Voltage: ");
                        print_number(voltage_mv / 1000); print_str(".");
                        print_number((voltage_mv % 1000) / 100); print_str("V\r\n");

                        current_raw = raw_pdo & 0x3FF;
                        current_ma = current_raw * 10;
                        print_str("  Current: ");
                        print_number(current_ma / 1000); print_str(".");
                        print_number((current_ma % 1000) / 100); print_str("A\r\n");
                        
                        /*power_mw = ((uint32_t)voltage_mv * current_ma) / 1000;
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
                        printf("Bits 21-20 (Peak Current): %d\r\n", peak_current);*/
                        break;
                    }
                        
                    case 0x01:
                    {
                        print_str("PDO Type: Battery Supply\r\n");
                        
                        /*max_voltage_raw = (raw_pdo >> 20) & 0x3FF;
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
                               max_power_raw, max_power_raw, max_power_mw);*/
                        break;
                    }
                        
                    case 0x02:
                    {
                        print_str("PDO Type: Variable Supply\r\n");

                        max_voltage_raw = (raw_pdo >> 20) & 0x3FF;
                        max_voltage_mv = max_voltage_raw * 50;
                        print_str("  Max Voltage: "); print_number(max_voltage_mv); print_str("mV\r\n");

                        min_voltage_raw = (raw_pdo >> 10) & 0x3FF;
                        min_voltage_mv = min_voltage_raw * 50;
                        print_str("  Min Voltage: "); print_number(min_voltage_mv); print_str("mV\r\n");

                        current_raw = raw_pdo & 0x3FF;
                        current_ma = current_raw * 10;
                        print_str("  Max Current: ");
                        print_number(current_ma / 1000); print_str(".");
                        print_number((current_ma % 1000) / 100); print_str("A\r\n");
                        break;
                    }
                        
                    case 0x03:
                    {
                        print_str("PDO Type: Augmented/PPS\r\n");

                        apdo_type = (raw_pdo >> 28) & 0x03;
                        print_str("  APDO Type: "); print_number(apdo_type); print_nl();

                        pps_max_v = (raw_pdo >> 17) & 0xFF;
                        print_str("  Max Voltage: "); print_number(pps_max_v * 100); print_str("mV\r\n");

                        pps_min_v = (raw_pdo >> 8) & 0xFF;
                        print_str("  Min Voltage: "); print_number(pps_min_v * 100); print_str("mV\r\n");

                        pps_max_i = raw_pdo & 0x7F;
                        print_str("  Max Current: ");
                        print_number((pps_max_i * 50) / 1000); print_str(".");
                        print_number(((pps_max_i * 50) % 1000) / 100); print_str("A\r\n");
                        break;
                    }
                }
                
                print_str("\r\n");
            }
        }
        else
        {
            print_str("ERROR: Failed to read PDO "); print_number(i + 1);
            print_str(" from register 0x"); print_hex8(reg_addr); print_nl();
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
        print_str("Diagnostic Info:\r\n");
        print_str("  Current PE FSM: 0x"); print_hex8(pe_fsm); print_str(" ");
        /*switch(pe_fsm) { ... }*/
        print_str("  Expected: 0x10 (PE_SNK_READY) for valid PD contract\r\n\r\n");
        print_str("  PD Status (0x1B) = 0x"); print_hex8(pd_status);
        print_str("  "); print_str((pd_status & 0x01) ? "DFP" : "UFP"); print_nl();
    }

    print_str("==============================================\r\n");
}

/**
  * @brief Display sink configuration registers
  */
void STUSB4531_DisplaySinkConfiguration(I2C_HandleTypeDef *hi2c)
{
    uint8_t reg_value;
    uint16_t reg_value_16;
    uint8_t byte_low, byte_high;
    
    print_str("\r\n========== SINK CONFIGURATION ==========\r\n");

    if (STUSB4531_ReadReg(hi2c, STUSB4531_DEVICE_PDP_ADD, &reg_value) == HAL_OK)
    {
        print_str("--- DEVICE_PDP (0x51) = 0x"); print_hex8(reg_value); print_str(" ---\r\n");
        print_str("  Power Data Path configuration\r\n");
        print_str("  PDP value = "); print_number(reg_value);
        print_str(" (x0.25W = "); print_number((reg_value * 25) / 100); print_str(".");
        /* print leading zero for sub-1W part */
        uint8_t frac = (reg_value * 25) % 100;
        if (frac < 10) print_str("0");
        print_number(frac); print_str("W max)\r\n\r\n");
    }

    if (STUSB4531_ReadReg(hi2c, STUSB4531_NUM_PDO_ADD, &reg_value) == HAL_OK)
    {
        print_str("--- NUM_PDO (0x52) = 0x"); print_hex8(reg_value); print_str(" ---\r\n");
        print_str("  Number of valid source PDOs: "); print_number(reg_value & 0x07); print_str("\r\n\r\n");
    }

    if (STUSB4531_ReadReg(hi2c, STUSB4531_SNK_PDO_PARAMS_ADD, &reg_value) == HAL_OK)
    {
        print_str("--- SNK_PDO_PARAMS (0x53) = 0x"); print_hex8(reg_value); print_str(" ---\r\n");
        print_str("  Sink PDO Parameters\r\n");
        uint8_t vsafe_5v_current = (reg_value >> 6) & 0x03;
        uint8_t pdo3_vsel = (reg_value >> 4) & 0x03;
        uint8_t pdo2_vsel = (reg_value >> 2) & 0x03;
        uint8_t flex_current = reg_value & 0x03;

        print_str("  Bits 7-6 (VSAFE_5V_CURRENT): "); print_number(vsafe_5v_current); print_str(" ");
        switch(vsafe_5v_current) {
            case 0: print_str("(Default/900mA)\r\n"); break;
            case 1: print_str("(1.5A)\r\n"); break;
            case 2: print_str("(3.0A)\r\n"); break;
            case 3: print_str("(5.0A)\r\n"); break;
        }
        print_str("  Bits 5-4 (PDO3_VSEL): "); print_number(pdo3_vsel); print_str(" ");
        if (pdo3_vsel == 0) print_str("(Variable)\r\n");
        else { print_str("("); print_number(pdo3_vsel * 3 + 6); print_str("V)\r\n"); }
        print_str("  Bits 3-2 (PDO2_VSEL): "); print_number(pdo2_vsel); print_str(" ");
        if (pdo2_vsel == 0) print_str("(Variable)\r\n");
        else { print_str("("); print_number(pdo2_vsel * 3 + 6); print_str("V)\r\n"); }
        print_str("  Bits 1-0 (FLEX_CURRENT): "); print_number(flex_current); print_str(" ");
        switch(flex_current) {
            case 0: print_str("(Default)\r\n"); break;
            case 1: print_str("(1.5A)\r\n"); break;
            case 2: print_str("(3.0A)\r\n"); break;
            case 3: print_str("(5.0A)\r\n"); break;
        }
        print_nl();
    }

    if (STUSB4531_ReadReg(hi2c, STUSB4531_SNK_PDO_CAPABILITIES_ADD, &byte_low) == HAL_OK &&
        STUSB4531_ReadReg(hi2c, STUSB4531_SNK_PDO_CAPABILITIES_ADD + 1, &byte_high) == HAL_OK)
    {
        reg_value_16 = byte_low | (byte_high << 8);
        print_str("--- SNK_PDO_CAPABILITIES (0x54-0x55) = 0x"); print_hex_n(reg_value_16, 4); print_str(" ---\r\n");
        print_str("  Sink PDO Capabilities\r\n");
        uint8_t vsel_var_high = (reg_value_16 >> 8) & 0xFF;
        uint8_t vsel_var_low  = (reg_value_16 >> 6) & 0x03;
        uint8_t epr_capable   = (reg_value_16 >> 5) & 0x01;
        uint8_t drd_capable   = (reg_value_16 >> 4) & 0x01;
        uint8_t usb_com_capable     = (reg_value_16 >> 2) & 0x01;
        uint8_t unconstrained_power = (reg_value_16 >> 1) & 0x01;
        uint8_t higher_capability   = reg_value_16 & 0x01;

        print_str("  Bits 15-8 (VSEL_VARIABLE_HIGH): 0x"); print_hex8(vsel_var_high); print_nl();
        print_str("  Bits 7-6 (VSEL_VARIABLE_LOW): "); print_number(vsel_var_low); print_nl();
        print_str("  Bit 5 (EPR_CAPABLE): "); print_number(epr_capable); print_str(" ("); print_str(epr_capable ? "EPR" : "SPR"); print_str(")\r\n");
        print_str("  Bit 4 (DRD_CAPABLE): "); print_number(drd_capable); print_str(" ("); print_str(drd_capable ? "Dual-Role Data" : "Single DR"); print_str(")\r\n");
        print_str("  Bit 2 (USB_COM_CAPABLE): "); print_number(usb_com_capable); print_str(" ("); print_str(usb_com_capable ? "USB Data" : "No USB Data"); print_str(")\r\n");
        print_str("  Bit 1 (UNCONSTRAINED_POWER): "); print_number(unconstrained_power); print_nl();
        print_str("  Bit 0 (HIGHER_CAPABILITY): "); print_number(higher_capability); print_str(" ("); print_str(higher_capability ? "More than 5V" : "Only 5V"); print_str(")\r\n");
        print_nl();
    }

    if (STUSB4531_ReadReg(hi2c, STUSB4531_ALGO_ADD, &reg_value) == HAL_OK)
    {
        print_str("--- ALGO (0x56) = 0x"); print_hex8(reg_value); print_str(" ---\r\n");
        print_str("  Algorithm Configuration\r\n");
        print_str("  Algorithm settings = 0x"); print_hex8(reg_value); print_str("\r\n\r\n");
    }

    if (STUSB4531_ReadReg(hi2c, STUSB4531_SNK_APDO_FILL_1_ADD, &reg_value) == HAL_OK)
    {
        print_str("--- SNK_APDO_FILL_1 (0x57) = 0x"); print_hex8(reg_value); print_str(" ---\r\n");
        print_str("  APDO/PPS Max Voltage\r\n");
        print_str("  Max Voltage = "); print_number(reg_value); print_str(" units (x100mV = ");
        print_number(reg_value * 100); print_str("mV)\r\n\r\n");
    }

    if (STUSB4531_ReadReg(hi2c, STUSB4531_SNK_APDO_FILL_2_ADD, &reg_value) == HAL_OK)
    {
        print_str("--- SNK_APDO_FILL_2 (0x58) = 0x"); print_hex8(reg_value); print_str(" ---\r\n");
        print_str("  APDO/PPS Min Voltage\r\n");
        print_str("  Min Voltage = "); print_number(reg_value); print_str(" units (x100mV = ");
        print_number(reg_value * 100); print_str("mV)\r\n\r\n");
    }

    if (STUSB4531_ReadReg(hi2c, STUSB4531_SNK_APDO_FILL_3_ADD, &reg_value) == HAL_OK)
    {
        print_str("--- SNK_APDO_FILL_3 (0x59) = 0x"); print_hex8(reg_value); print_str(" ---\r\n");
        print_str("  APDO/PPS Max Current\r\n");
        print_str("  Max Current = "); print_number(reg_value); print_str(" units (x50mA = ");
        print_number(reg_value * 50); print_str("mA)\r\n\r\n");
    }

    if (STUSB4531_ReadReg(hi2c, STUSB4531_DEVICE_SETTING_ADD, &reg_value) == HAL_OK)
    {
        print_str("--- DEVICE_SETTING (0x5A) = 0x"); print_hex8(reg_value); print_str(" ---\r\n");
        print_str("  Device configuration = 0x"); print_hex8(reg_value); print_str("\r\n\r\n");
    }

    if (STUSB4531_ReadReg(hi2c, STUSB4531_REQUEST_SRC_PDP_ADD, &reg_value) == HAL_OK)
    {
        print_str("--- REQUEST_SRC_PDP (0x5C) = 0x"); print_hex8(reg_value); print_str(" ---\r\n");
        print_str("  Requested Source PDP\r\n");
        print_str("  Requested PDP = "); print_number(reg_value);
        print_str(" (x0.25W = "); print_number((reg_value * 25) / 100); print_str(".");
        uint8_t frac2 = (reg_value * 25) % 100;
        if (frac2 < 10) print_str("0");
        print_number(frac2); print_str("W)\r\n\r\n");
    }

    print_str("==============================================\r\n");
}
