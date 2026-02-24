/**
  ******************************************************************************
  * @file    dbg_print.h
  * @brief   Lightweight UART debug print — no stdio / no printf
  *
  * Flash-efficient primitives for MCUs that cannot afford newlib printf.
  *
  * Usage:
  *   dbg_print_init(&huart2);    // once, before any print call
  *
  *   print_str("text\r\n");      // literal / pre-formatted text
  *   print_number(-1234);        // signed 32-bit decimal
  *   print_uint(5000U);          // unsigned 32-bit decimal
  *   print_hex8(0xA5);           // 2-digit uppercase hex  (e.g. "A5")
  *   print_hex_n(0xABC, 3);      // N-digit uppercase hex  (e.g. "0ABC")
  *   print_hex32(0x1234ABCDU);   // 8-digit uppercase hex  (e.g. "1234ABCD")
  *   print_double(3.14, 2);      // decimal with precision  (e.g. "3.14")
  *   print_nl();                 // shortcut for "\r\n"
  ******************************************************************************
  */

#ifndef DBG_PRINT_H
#define DBG_PRINT_H

#include "stm32c0xx_hal.h"
#include <stdint.h>

/* ---- Initialisation ---- */
void dbg_print_init(UART_HandleTypeDef *huart);

/* ---- Core primitives ---- */
void print_str(const char *s);          /**< transmit a NUL-terminated string */
void print_nl(void);                    /**< transmit "\r\n" */
void print_number(int32_t n);           /**< signed decimal integer */
void print_uint(uint32_t n);            /**< unsigned decimal integer */
void print_hex8(uint8_t n);             /**< 2-digit uppercase hex, no "0x" prefix */
void print_hex_n(uint32_t n, uint8_t digits); /**< N-digit uppercase hex, no prefix */
void print_hex32(uint32_t n);           /**< 8-digit uppercase hex, no "0x" prefix */
void print_double(double d, uint8_t precision); /**< decimal, 'precision' fractional digits */

/* ---- Buffer helpers (for OLED / no-UART string building, no sprintf needed) ---- */
uint8_t buf_cat_str(char *buf, uint8_t pos, const char *s);  /**< append string to buf[pos], return new pos */
uint8_t buf_cat_int(char *buf, uint8_t pos, int32_t n);      /**< append decimal int,    return new pos */
uint8_t buf_cat_uint(char *buf, uint8_t pos, uint32_t n);    /**< append decimal uint,   return new pos */

#endif /* DBG_PRINT_H */
