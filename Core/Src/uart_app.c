#include "uart_app.h"
#include "stusb4531_comm.h"
#include "stusb4531_display.h"
#include "dbg_print.h"

extern I2C_HandleTypeDef hi2c1;

void UART_App_Init(void)
{
    print_str("\r\n=== STUSB4531 USB-PD Demo ===\r\n");
    print_str("Initializing...\r\n");
}

void UART_App_PrintAlert(uint8_t alert_status)
{
    print_str("\r\n*** ALERT Interrupt! Status = 0x"); print_hex8(alert_status); print_str(" ***\r\n");
}

void UART_App_PrintComprehensiveStatus(STUSB4531_ComprehensiveStatus_t *comp_status)
{
    print_str("\r\n========== COMPREHENSIVE STATUS ==========\r\n");

    // CC Status - decode Rp advertised current from active CC line
    {
        static const uint16_t rp_ma[4] = {0, 900, 1500, 3000};
        uint8_t orient = (comp_status->cc_status >> 7) & 0x01;
        uint8_t cc_state = orient ? ((comp_status->cc_status >> 2) & 0x03)
                                  : (comp_status->cc_status & 0x03);
        uint16_t rp_current = rp_ma[cc_state];
        uint8_t attached = (comp_status->cc_status >> 6) & 0x01;
        print_str("CC Status (0x1A) = 0x"); print_hex8(comp_status->cc_status);
        print_str("  CC"); print_number(orient ? 2 : 1);
        if (attached && rp_current > 0)
        {
            print_str(" Attached  Rp=");
            print_number(rp_current / 1000); print_str("."); print_number((rp_current % 1000) / 100);
            print_str("A\r\n");
        }
        else
        {
            print_str(" "); print_str(attached ? "Attached" : "Detached"); print_nl();
        }
    }

    // PD Status
    print_str("PD Status (0x1B) = 0x"); print_hex8(comp_status->pd_status);
    print_str("  "); print_str((comp_status->pd_status & 0x01) ? "DFP" : "UFP"); print_nl();

    // Type-C FSM
    print_str("Type-C FSM (0x20) = 0x"); print_hex8(comp_status->typec_fsm); print_str(" ");
    switch(comp_status->typec_fsm) {
        case 0x00: print_str("(Unattached.SNK)\r\n"); break;
        case 0x01: print_str("(AttachWait.SNK)\r\n"); break;
        case 0x02: print_str("(Attached.SNK)\r\n"); break;
        case 0x03: print_str("(DebugAccessory.SNK)\r\n"); break;
        default:   print_str("(Unknown/Other)\r\n"); break;
    }

    // Policy Engine FSM
    print_str("PE FSM (0x21) = 0x"); print_hex8(comp_status->pe_fsm); print_str(" ");
    switch(comp_status->pe_fsm) {
        case 0x00: print_str("(PE_INIT)\r\n"); break;
        case 0x01: print_str("(PE_SOFT_RESET)\r\n"); break;
        case 0x02: print_str("(PE_HARD_RESET)\r\n"); break;
        case 0x09: print_str("(PE_SNK_STARTUP)\r\n"); break;
        case 0x0A: print_str("(PE_SNK_DISCOVERY)\r\n"); break;
        case 0x0B: print_str("(PE_SNK_WAIT_FOR_CAPABILITIES)\r\n"); break;
        case 0x0C: print_str("(PE_SNK_EVALUATE_CAPABILITIES)\r\n"); break;
        case 0x0D: print_str("(PE_SNK_SELECT_CAPABILITIES_1)\r\n"); break;
        case 0x0E: print_str("(PE_SNK_SELECT_CAPABILITIES_2)\r\n"); break;
        case 0x0F: print_str("(PE_SNK_TRANSITION_SINK)\r\n"); break;
        case 0x10: print_str("(PE_SNK_READY) <-- PD contract active\r\n"); break;
        case 0x11: print_str("(PE_SNK_GET_SOURCE_CAP)\r\n"); break;
        case 0x12: print_str("(PE_SNK_READY_SENDING) <-- PD contract active\r\n"); break;
        default:   print_str("(State 0x"); print_hex8(comp_status->pe_fsm); print_str(")\r\n"); break;
    }

    // VBUS FSM
    print_str("VBUS FSM (0x22) = 0x"); print_hex8(comp_status->vbus_fsm); print_str(" ");
    switch(comp_status->vbus_fsm) {
        case 0x00: print_str("(vSafe0V)\r\n"); break;
        case 0x01: print_str("(vSafe5V)\r\n"); break;
        case 0x02: print_str("(VbusLow)\r\n"); break;
        case 0x03: print_str("(VbusHigh)\r\n"); break;
        default:   print_str("(Unknown)\r\n"); break;
    }

    // VBUS Status
    {
        uint32_t vbus_v10 = (uint32_t)comp_status->vbus_status * 2;
        print_str("VBUS Status (0x19) = 0x"); print_hex8(comp_status->vbus_status);
        print_str("  ~"); print_uint(vbus_v10 / 10); print_str("."); print_uint(vbus_v10 % 10);
        print_str("V\r\n");
    }

    // Monitoring Status
    if (comp_status->monitoring_status != 0)
    {
        print_str("Monitoring Status (0x16) = 0x"); print_hex8(comp_status->monitoring_status); print_nl();
    }

    // HW Fault Status
    if (comp_status->hw_fault_status != 0)
    {
        print_str("HW Fault Status (0x1D) = 0x"); print_hex8(comp_status->hw_fault_status); print_nl();
        if (comp_status->hw_fault_status & 0x01) print_str("  - VCONN OVP\r\n");
        if (comp_status->hw_fault_status & 0x02) print_str("  - VCONN OCP\r\n");
    }

    // Alert Status
    if (comp_status->alert_status != 0)
    {
        print_str("Alert Status (0x10) = 0x"); print_hex8(comp_status->alert_status);
        print_str(" (will be cleared on interrupt)\r\n");
        if (comp_status->alert_status & 0x01) print_str("  - PD/Type-C status changed\r\n");
        if (comp_status->alert_status & 0x02) print_str("  - Port status changed\r\n");
        if (comp_status->alert_status & 0x04) print_str("  - PD message received\r\n");
        if (comp_status->alert_status & 0x10) print_str("  - Hardware reset detected\r\n");
    }

    // PDO counts
    uint8_t num_src_pdo = (comp_status->num_pdo >> 4) & 0x07;
    uint8_t num_snk_pdo = comp_status->num_pdo & 0x03;
    print_str("Number of PDOs (0x52) = 0x"); print_hex8(comp_status->num_pdo);
    print_str("  SRC_PDO="); print_number(num_src_pdo);
    print_str("  SNK_FIX_PDO="); print_number(num_snk_pdo); print_nl();

    if (num_src_pdo > 0)
    {
        STUSB4531_DisplayDetailedPDOs(&hi2c1, num_src_pdo,
                                     comp_status->pe_fsm, comp_status->pd_status);
    }

    // RDO
    if (comp_status->rdo != 0)
    {
        print_str("RDO (0xBC) = 0x"); print_hex32(comp_status->rdo); print_nl();
        uint8_t obj_pos = (comp_status->rdo >> 28) & 0x07;
        print_str("  Requested PDO position: "); print_number(obj_pos); print_nl();
    }

    // Negotiated PDO
    if (comp_status->negotiated_pdo != 0)
    {
        print_str("Negotiated PDO (0xC4) = 0x"); print_hex32(comp_status->negotiated_pdo); print_nl();
        STUSB4531_PDO_t nego_pdo;
        STUSB4531_ParsePDO(comp_status->negotiated_pdo, &nego_pdo);
        print_str("  ");
        print_number((int32_t)(nego_pdo.voltage_mv / 1000)); print_str(".");
        print_number((int32_t)((nego_pdo.voltage_mv % 1000) / 100)); print_str("V  ");
        print_number((int32_t)(nego_pdo.current_ma / 1000)); print_str(".");
        print_number((int32_t)((nego_pdo.current_ma % 1000) / 100)); print_str("A  ");
        print_uint(nego_pdo.power_mw); print_str("mW\r\n");
    }

    print_str("==========================================\r\n");
}

void UART_App_PrintPDOs(STUSB4531_Status_t *status)
{
    print_str("\r\n--- PDO Information ---\r\n");
    print_str("Number of PDOs: "); print_number(status->num_pdos); print_nl();
    for (uint8_t i = 0; i < status->num_pdos; i++)
    {
        print_str("PDO "); print_number(i + 1); print_str(": ");
        print_number((int32_t)(status->pdos[i].voltage_mv / 1000)); print_str(".");
        print_number((int32_t)((status->pdos[i].voltage_mv % 1000) / 100)); print_str("V ");
        print_number((int32_t)(status->pdos[i].current_ma / 1000)); print_str(".");
        print_number((int32_t)((status->pdos[i].current_ma % 1000) / 100)); print_str("A (");
        print_uint(status->pdos[i].power_mw); print_str("mW)");
        if (i == status->selected_pdo_index) print_str(" <-- SELECTED");
        print_nl();
    }
    print_str("Selected PDO #"); print_number(status->selected_pdo_index + 1);
    print_str(" with "); print_uint(status->pdos[status->selected_pdo_index].power_mw);
    print_str("mW\r\n");
}
