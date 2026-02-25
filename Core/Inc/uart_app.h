#ifndef UART_APP_H
#define UART_APP_H

#include "stusb4531_logic.h"

void UART_App_Init(void);
void UART_App_PrintStatus(STUSB4531_Status_t *status);
void UART_App_PrintComprehensiveStatus(STUSB4531_ComprehensiveStatus_t *comp_status);
void UART_App_PrintAlert(uint8_t alert_status);
void UART_App_PrintPDOs(STUSB4531_Status_t *status);
void UART_App_PrintEPR(STUSB4531_EPR_t *epr, STUSB4531_Status_t *status);

#endif // UART_APP_H
