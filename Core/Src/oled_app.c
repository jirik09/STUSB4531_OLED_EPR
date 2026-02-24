#include "oled_app.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "dbg_print.h"

#define SECOND_LINE_Y 20

void OLED_App_Init(void)
{
    ssd1306_Init();
    ssd1306_Fill(0x00);
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString("STUSB4531", Font_11x18, 0x01);
    ssd1306_SetCursor(0, SECOND_LINE_Y);
    ssd1306_WriteString("USB-PD init...", Font_7x10, 0x01);
    ssd1306_UpdateScreen();
}

void OLED_App_ShowNegotiating(uint8_t pe_fsm)
{
    const char *state_name;
    switch (pe_fsm)
    {
        case 0x00: state_name = "PE_INIT";      break;
        case 0x01: state_name = "SOFT_RESET";   break;
        case 0x02: state_name = "HARD_RESET";   break;
        case 0x09: state_name = "SNK_STARTUP";  break;
        case 0x0A: state_name = "SNK_DISCOVER"; break;
        case 0x0B: state_name = "SNK_WAIT_CAP"; break;
        case 0x0C: state_name = "SNK_EVAL_CAP"; break;
        case 0x0D: state_name = "SNK_SEL_CAP";  break;
        case 0x0E: state_name = "SNK_SEL_CAP2"; break;
        case 0x0F: state_name = "SNK_TRANS";    break;
        case 0x10: state_name = "SNK_READY!";   break;
        case 0x11: state_name = "SNK_GET_CAP";  break;
        case 0x12: state_name = "SNK_READY!";   break;
        default:   state_name = "PE_UNKNOWN";   break;
    }

    char buf[12];
    ssd1306_Fill(0x00);
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString("STUSB4531", Font_11x18, 0x01);
    ssd1306_SetCursor(0, SECOND_LINE_Y);
    ssd1306_WriteString(state_name, Font_7x10, 0x01);
    /*sprintf(buf, "PE:0x%02X", pe_fsm);
    ssd1306_SetCursor(0, 22);
    ssd1306_WriteString(buf, Font_7x10, 0x01);*/
    ssd1306_UpdateScreen();
}

void OLED_App_Update(STUSB4531_Status_t *status)
{
    char buffer[32];
    
    ssd1306_Fill(0x00);
    
    if (!status->initialized)
    {
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString("STUSB Init...", Font_11x18, 0x01);
        ssd1306_UpdateScreen();
        return;
    }
    
    if (!status->attached)
    {
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString("No USB-C", Font_11x18, 0x01);
        ssd1306_SetCursor(0, SECOND_LINE_Y);
        ssd1306_WriteString("using 5V", Font_7x10, 0x01);
        ssd1306_UpdateScreen();
        return;
    }
    
    if (!status->pd_capable)
    {
        if (status->cc_current_ma > 0)
        {
            /* Show Rp-advertised current on line 1, label on line 2 */
            {
                uint8_t p = buf_cat_str(buffer, 0, "5V ");
                p = buf_cat_int(buffer, p, (int32_t)(status->cc_current_ma / 1000));
                p = buf_cat_str(buffer, p, ".");
                p = buf_cat_int(buffer, p, (int32_t)((status->cc_current_ma % 1000) / 100));
                buf_cat_str(buffer, p, "A");
            }
            ssd1306_SetCursor(0, 0);
            ssd1306_WriteString(buffer, Font_11x18, 0x01);
            ssd1306_SetCursor(0, SECOND_LINE_Y);
            ssd1306_WriteString("USB-C (no PD)", Font_7x10, 0x01);
        }
        else
        {
            ssd1306_SetCursor(0, 0);
            ssd1306_WriteString("No USB-PD", Font_11x18, 0x01);
            ssd1306_SetCursor(0, SECOND_LINE_Y);
            ssd1306_WriteString("using 5V", Font_7x10, 0x01);
        }
        ssd1306_UpdateScreen();
        return;
    }
    
    // PD Capable
    if (status->epr_ready)
    {
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString("48V EPR", Font_11x18, 0x01);
        ssd1306_SetCursor(81, 8);
        ssd1306_WriteString("Ready", Font_7x10, 0x01);
        ssd1306_SetCursor(0, SECOND_LINE_Y);
        ssd1306_WriteString("max 240W: Charging", Font_7x10, 0x01);
        ssd1306_UpdateScreen();
        return;
    }
    
    // Display negotiated parameters (from actively negotiated PDO)
    if (status->pd_capable && status->negotiated_pdo.voltage_mv > 0)
    {
        {
            uint8_t p = buf_cat_int(buffer, 0, (int32_t)(status->negotiated_pdo.voltage_mv / 1000));
            p = buf_cat_str(buffer, p, "V ");
            p = buf_cat_int(buffer, p, (int32_t)(status->negotiated_pdo.current_ma / 1000));
            p = buf_cat_str(buffer, p, ".");
            p = buf_cat_int(buffer, p, (int32_t)((status->negotiated_pdo.current_ma % 1000) / 100));
            buf_cat_str(buffer, p, "A");
        }
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString(buffer, Font_11x18, 0x01);
        ssd1306_SetCursor(0, SECOND_LINE_Y);
        if(status->negotiated_pdo.voltage_mv >15000){
            ssd1306_WriteString("Charging", Font_7x10, 0x01);
        }
        else{
            ssd1306_WriteString("Too low voltage", Font_7x10, 0x01);
        }
        
    }
    else if (status->num_pdos > 0)
    {
        uint8_t sel = status->selected_pdo_index;
        {
            uint8_t p = buf_cat_int(buffer, 0, (int32_t)(status->pdos[sel].voltage_mv / 1000));
            p = buf_cat_str(buffer, p, "V ");
            p = buf_cat_int(buffer, p, (int32_t)(status->pdos[sel].current_ma / 1000));
            p = buf_cat_str(buffer, p, ".");
            p = buf_cat_int(buffer, p, (int32_t)((status->pdos[sel].current_ma % 1000) / 100));
            buf_cat_str(buffer, p, "A");
        }
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString(buffer, Font_11x18, 0x01);
                ssd1306_SetCursor(0, SECOND_LINE_Y);
        ssd1306_WriteString("USB-PD ok", Font_7x10, 0x01);
    }
    
    ssd1306_UpdateScreen();
}
