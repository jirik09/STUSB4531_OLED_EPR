#include "oled_app.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <stdio.h>

void OLED_App_Init(void)
{
    ssd1306_Init();
    ssd1306_Fill(0x00);
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString("STUSB4531", Font_11x18, 0x01);
    ssd1306_SetCursor(0, 18);
    ssd1306_WriteString("USB-PD negotiation", Font_7x10, 0x01);
    ssd1306_UpdateScreen();
}

void OLED_App_Update(STUSB4531_Status_t *status)
{
    char buffer[32];
    
    ssd1306_Fill(0x00);
    
    if (!status->initialized)
    {
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString("STUSB Init...", Font_7x10, 0x01);
        ssd1306_UpdateScreen();
        return;
    }
    
    if (!status->attached)
    {
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString("No USB-C", Font_7x10, 0x01);
        ssd1306_UpdateScreen();
        return;
    }
    
    if (!status->pd_capable)
    {
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString("No USB-PD", Font_7x10, 0x01);
        ssd1306_UpdateScreen();
        return;
    }
    
    // PD Capable
    if (status->epr_ready)
    {
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString("EPR ready", Font_7x10, 0x01);
        ssd1306_SetCursor(0, 11);
        ssd1306_WriteString("up to 48V (240W)", Font_7x10, 0x01);
    }
    else
    {
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString("Negotiated USB-PD", Font_7x10, 0x01);
    }
    
    // Display negotiated parameters
    if (status->num_pdos > 0)
    {
        uint8_t sel = status->selected_pdo_index;
        sprintf(buffer, "%dV %dmA", 
                status->pdos[sel].voltage_mv / 1000,
                status->pdos[sel].current_ma);
        ssd1306_SetCursor(0, 22);
        ssd1306_WriteString(buffer, Font_7x10, 0x01);
    }
    
    ssd1306_UpdateScreen();
}
