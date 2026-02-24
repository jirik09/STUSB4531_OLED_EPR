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
}

void UART_App_PrintComprehensiveStatus(STUSB4531_ComprehensiveStatus_t *comp_status)
{
    printf("\r\n========== COMPREHENSIVE STATUS ==========\r\n");
    
    // CC Status - decode Rp advertised current from active CC line
    {
        static const uint16_t rp_ma[4] = {0, 900, 1500, 3000};
        uint8_t orient = (comp_status->cc_status >> 7) & 0x01;
        uint8_t cc_state = orient ? ((comp_status->cc_status >> 2) & 0x03)
                                  : (comp_status->cc_status & 0x03);
        uint16_t rp_current = rp_ma[cc_state];
        uint8_t attached = (comp_status->cc_status >> 6) & 0x01;
        if (attached && rp_current > 0)
            printf("CC Status (0x1A) = 0x%02X  CC%d Attached  Rp=%d.%dA\r\n",
                   comp_status->cc_status, orient ? 2 : 1,
                   rp_current / 1000, (rp_current % 1000) / 100);
        else
            printf("CC Status (0x1A) = 0x%02X  CC%d %s\r\n",
                   comp_status->cc_status, orient ? 2 : 1,
                   attached ? "Attached" : "Detached");
    }
    
    // PD Status (0x1B) - Note: this register does NOT contain contract/connected flags
    // per the STUSB4531 datasheet. Actual bits: data_role(0), vconn_on(5), vconn_src(6)
    printf("PD Status (0x1B) = 0x%02X  %s\r\n",
           comp_status->pd_status,
           (comp_status->pd_status & 0x01) ? "DFP" : "UFP");
    
    // Type-C FSM
    printf("Type-C FSM (0x20) = 0x%02X ", comp_status->typec_fsm);
    switch(comp_status->typec_fsm) {
        case 0x00: printf("(Unattached.SNK)\r\n"); break;
        case 0x01: printf("(AttachWait.SNK)\r\n"); break;
        case 0x02: printf("(Attached.SNK)\r\n"); break;
        case 0x03: printf("(DebugAccessory.SNK)\r\n"); break;
        default: printf("(Unknown/Other)\r\n"); break;
    }
    
    // Policy Engine FSM (PE_FSM values per STUSB4531 datasheet)
    printf("PE FSM (0x21) = 0x%02X ", comp_status->pe_fsm);
    switch(comp_status->pe_fsm) {
        case 0x00: printf("(PE_INIT)\r\n"); break;
        case 0x01: printf("(PE_SOFT_RESET)\r\n"); break;
        case 0x02: printf("(PE_HARD_RESET)\r\n"); break;
        case 0x09: printf("(PE_SNK_STARTUP)\r\n"); break;
        case 0x0A: printf("(PE_SNK_DISCOVERY)\r\n"); break;
        case 0x0B: printf("(PE_SNK_WAIT_FOR_CAPABILITIES)\r\n"); break;
        case 0x0C: printf("(PE_SNK_EVALUATE_CAPABILITIES)\r\n"); break;
        case 0x0D: printf("(PE_SNK_SELECT_CAPABILITIES_1)\r\n"); break;
        case 0x0E: printf("(PE_SNK_SELECT_CAPABILITIES_2)\r\n"); break;
        case 0x0F: printf("(PE_SNK_TRANSITION_SINK)\r\n"); break;
        case 0x10: printf("(PE_SNK_READY) <-- PD contract active\r\n"); break;
        case 0x11: printf("(PE_SNK_GET_SOURCE_CAP)\r\n"); break;
        case 0x12: printf("(PE_SNK_READY_SENDING) <-- PD contract active\r\n"); break;
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
    printf("VBUS Status (0x19) = 0x%02X  ~%d.%dV\r\n",
           comp_status->vbus_status,
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
    /* NUM_PDO (0x52): bits [6:4] = NUM_SRC_PDO (received from source)
     *                  bits [1:0] = NUM_SNK_FIX_PDO (locally configured)  */
    uint8_t num_src_pdo = (comp_status->num_pdo >> 4) & 0x07;
    uint8_t num_snk_pdo = comp_status->num_pdo & 0x03;
    printf("Number of PDOs (0x52) = 0x%02X  SRC_PDO=%d  SNK_FIX_PDO=%d\r\n",
           comp_status->num_pdo, num_src_pdo, num_snk_pdo);

    // Display detailed PDO information if source PDOs have been received
    if (num_src_pdo > 0)
    {
        STUSB4531_DisplayDetailedPDOs(&hi2c1, num_src_pdo,
                                     comp_status->pe_fsm, comp_status->pd_status);
    }
    
    // Display sink configuration registers
    //STUSB4531_DisplaySinkConfiguration(&hi2c1);
    
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
        printf("  %d.%dV  %d.%dA  %ldmW\r\n",
               nego_pdo.voltage_mv / 1000, (nego_pdo.voltage_mv % 1000) / 100,
               nego_pdo.current_ma / 1000, (nego_pdo.current_ma % 1000) / 100,
               nego_pdo.power_mw);
    }
    
    printf("==========================================\r\n");
}

void UART_App_PrintPDOs(STUSB4531_Status_t *status)
{
    printf("\r\n--- PDO Information ---\r\n");
    printf("Number of PDOs: %d\r\n", status->num_pdos);
    for (uint8_t i = 0; i < status->num_pdos; i++)
    {
        printf("PDO %d: %d.%dV %d.%dA (%ldmW) %s\r\n",
               i + 1,
               status->pdos[i].voltage_mv / 1000, (status->pdos[i].voltage_mv % 1000) / 100,
               status->pdos[i].current_ma / 1000, (status->pdos[i].current_ma % 1000) / 100,
               status->pdos[i].power_mw,
               (i == status->selected_pdo_index) ? "<-- SELECTED" : "");
    }
    printf("Selected PDO #%d with %ldmW\r\n", status->selected_pdo_index + 1, 
           status->pdos[status->selected_pdo_index].power_mw);
}
