#include "uart_app.h"
#include "stusb4531_comm.h"
#include "stusb4531_display.h"
#include <stdio.h>

extern I2C_HandleTypeDef hi2c1;

void UART_App_Init(void)
{
    printf("\r\n=== STUSB4531 USB-PD Demo ===\r\n");
    printf("Initializing...\r\n");
}

void UART_App_PrintAlert(uint8_t alert_status)
{
    printf("\r\n*** ALERT Interrupt! Status = 0x%02X ***\r\n", alert_status);
    printf("  Bit 0 (PD/Type-C): %d\r\n", (alert_status & 0x01) ? 1 : 0);
    printf("  Bit 1 (Port Status): %d\r\n", (alert_status & 0x02) ? 1 : 0);
    printf("  Bit 2 (PD Msg Recv): %d\r\n", (alert_status & 0x04) ? 1 : 0);
    printf("  Bit 4 (HW Reset): %d\r\n", (alert_status & 0x10) ? 1 : 0);
}

void UART_App_PrintComprehensiveStatus(STUSB4531_ComprehensiveStatus_t *comp_status)
{
    printf("\r\n========== COMPREHENSIVE STATUS ==========\r\n");
    
    // CC Status
    printf("CC Status (0x1A) = 0x%02X\r\n", comp_status->cc_status);
    printf("  Bit 0 (CC1 state): %d\r\n", (comp_status->cc_status >> 0) & 0x03);
    printf("  Bit 2 (CC2 state): %d\r\n", (comp_status->cc_status >> 2) & 0x03);
    printf("  Bit 4 (VCONN supply): %d\r\n", (comp_status->cc_status >> 4) & 0x03);
    printf("  Bit 6 (Attached): %d\r\n", (comp_status->cc_status >> 6) & 0x01);
    printf("  Bit 7 (Orientation): %d (CC%d)\r\n", 
           (comp_status->cc_status >> 7) & 0x01,
           (comp_status->cc_status & 0x80) ? 2 : 1);
    
    // PD Status
    printf("PD Status (0x1B) = 0x%02X\r\n", comp_status->pd_status);
    printf("  Bit 0 (CONNECTED): %d\r\n", (comp_status->pd_status >> 0) & 0x01);
    printf("  Bit 1 (CONTRACT_ESTABLISHED): %d\r\n", (comp_status->pd_status >> 1) & 0x01);
    printf("  Bit 2 (EXPLICIT_CONTRACT): %d\r\n", (comp_status->pd_status >> 2) & 0x01);
    
    // Type-C FSM
    printf("Type-C FSM (0x20) = 0x%02X ", comp_status->typec_fsm);
    switch(comp_status->typec_fsm) {
        case 0x00: printf("(Unattached.SNK)\r\n"); break;
        case 0x01: printf("(AttachWait.SNK)\r\n"); break;
        case 0x02: printf("(Attached.SNK)\r\n"); break;
        case 0x03: printf("(DebugAccessory.SNK)\r\n"); break;
        default: printf("(Unknown/Other)\r\n"); break;
    }
    
    // Policy Engine FSM
    printf("PE FSM (0x21) = 0x%02X ", comp_status->pe_fsm);
    switch(comp_status->pe_fsm) {
        case 0x00: printf("(PE_SNK_Startup)\r\n"); break;
        case 0x01: printf("(PE_SNK_Discovery)\r\n"); break;
        case 0x02: printf("(PE_SNK_Wait_for_Capabilities)\r\n"); break;
        case 0x03: printf("(PE_SNK_Evaluate_Capability)\r\n"); break;
        case 0x04: printf("(PE_SNK_Select_Capability)\r\n"); break;
        case 0x05: printf("(PE_SNK_Transition_Sink)\r\n"); break;
        case 0x06: printf("(PE_SNK_Ready)\r\n"); break;
        default: printf("(State 0x%02X)\r\n", comp_status->pe_fsm); break;
    }
    
    // VBUS FSM  
    printf("VBUS FSM (0x22) = 0x%02X ", comp_status->vbus_fsm);
    switch(comp_status->vbus_fsm) {
        case 0x00: printf("(vSafe0V)\r\n"); break;
        case 0x01: printf("(vSafe5V)\r\n"); break;
        case 0x02: printf("(VbusLow)\r\n"); break;
        case 0x03: printf("(VbusHigh)\r\n"); break;
        default: printf("(Unknown)\r\n"); break;
    }
    
    // VBUS Status
    printf("VBUS Status (0x19) = 0x%02X\r\n", comp_status->vbus_status);
    printf("  VBUS voltage: ~%d.%dV\r\n", 
           (comp_status->vbus_status * 2) / 10,
           (comp_status->vbus_status * 2) % 10);
    
    // Monitoring Status
    if (comp_status->monitoring_status != 0)
    {
        printf("Monitoring Status (0x16) = 0x%02X\r\n", comp_status->monitoring_status);
    }
    
    // HW Fault Status
    if (comp_status->hw_fault_status != 0)
    {
        printf("HW Fault Status (0x1D) = 0x%02X\r\n", comp_status->hw_fault_status);
        if (comp_status->hw_fault_status & 0x01) printf("  - VCONN OVP\r\n");
        if (comp_status->hw_fault_status & 0x02) printf("  - VCONN OCP\r\n");
    }
    
    // Alert Status (Don't clear here - only clear on interrupt)
    if (comp_status->alert_status != 0)
    {
        printf("Alert Status (0x10) = 0x%02X (will be cleared on interrupt)\r\n", comp_status->alert_status);
        if (comp_status->alert_status & 0x01) printf("  - PD/Type-C status changed\r\n");
        if (comp_status->alert_status & 0x02) printf("  - Port status changed\r\n");
        if (comp_status->alert_status & 0x04) printf("  - PD message received\r\n");
        if (comp_status->alert_status & 0x10) printf("  - Hardware reset detected\r\n");
    }
    
    // PDO Information
    printf("Number of PDOs (0x52) = %d\r\n", comp_status->num_pdo);
    
    // Display detailed PDO information if PDOs are available
    if (comp_status->num_pdo > 0)
    {
        STUSB4531_DisplayDetailedPDOs(&hi2c1, comp_status->num_pdo, 
                                     comp_status->pe_fsm, comp_status->pd_status);
    }
    
    // Display sink configuration registers
    STUSB4531_DisplaySinkConfiguration(&hi2c1);
    
    // RDO (Request Data Object)
    if (comp_status->rdo != 0)
    {
        printf("RDO (0xBC) = 0x%08lX\r\n", comp_status->rdo);
        uint8_t obj_pos = (comp_status->rdo >> 28) & 0x07;
        printf("  Requested PDO position: %d\r\n", obj_pos);
    }
    
    // Negotiated PDO
    if (comp_status->negotiated_pdo != 0)
    {
        printf("Negotiated PDO (0xC4) = 0x%08lX\r\n", comp_status->negotiated_pdo);
        STUSB4531_PDO_t nego_pdo;
        STUSB4531_ParsePDO(comp_status->negotiated_pdo, &nego_pdo);
        printf("  Voltage: %dmV, Current: %dmA, Power: %ldmW\r\n",
               nego_pdo.voltage_mv, nego_pdo.current_ma, nego_pdo.power_mw);
    }
    
    printf("==========================================\r\n");
}

void UART_App_PrintPDOs(STUSB4531_Status_t *status)
{
    printf("\r\n--- PDO Information ---\r\n");
    printf("Number of PDOs: %d\r\n", status->num_pdos);
    for (uint8_t i = 0; i < status->num_pdos; i++)
    {
        printf("PDO %d: %dmV %dmA (%ldmW) %s\r\n",
               i + 1,
               status->pdos[i].voltage_mv,
               status->pdos[i].current_ma,
               status->pdos[i].power_mw,
               (i == status->selected_pdo_index) ? "<-- SELECTED" : "");
    }
    printf("Selected PDO #%d with %ldmW\r\n", status->selected_pdo_index + 1, 
           status->pdos[status->selected_pdo_index].power_mw);
}
