/**
  ******************************************************************************
  * @file    dbg_print.c
  * @brief   Lightweight UART debug print — no stdio / no printf
  ******************************************************************************
  */

#include "dbg_print.h"

/* -----------------------------------------------------------------------
 * Private state
 * --------------------------------------------------------------------- */
static UART_HandleTypeDef *s_huart = NULL;

static const char s_hex[] = "0123456789ABCDEF";

/* -----------------------------------------------------------------------
 * Initialisation
 * --------------------------------------------------------------------- */
void dbg_print_init(UART_HandleTypeDef *huart)
{
    s_huart = huart;
}

/* -----------------------------------------------------------------------
 * String
 * --------------------------------------------------------------------- */
void print_str(const char *s)
{
    if (!s || !s_huart) return;
    uint16_t len = 0;
    while (s[len]) len++;
    if (len) HAL_UART_Transmit(s_huart, (const uint8_t *)s, len, HAL_MAX_DELAY);
}

void print_nl(void)
{
    print_str("\r\n");
}

/* -----------------------------------------------------------------------
 * Integers
 * --------------------------------------------------------------------- */
void print_number(int32_t n)
{
    char buf[12];                           /* -2147483648 + NUL */
    uint8_t idx = 11;
    buf[idx] = '\0';

    uint8_t negative = (n < 0);
    uint32_t u = negative ? (uint32_t)(-(n + 1)) + 1U : (uint32_t)n;

    if (u == 0) {
        buf[--idx] = '0';
    } else {
        while (u) { buf[--idx] = (char)('0' + (u % 10)); u /= 10; }
    }
    if (negative) buf[--idx] = '-';
    print_str(&buf[idx]);
}

void print_uint(uint32_t n)
{
    char buf[11];                           /* 4294967295 + NUL */
    uint8_t idx = 10;
    buf[idx] = '\0';

    if (n == 0) {
        buf[--idx] = '0';
    } else {
        while (n) { buf[--idx] = (char)('0' + (n % 10)); n /= 10; }
    }
    print_str(&buf[idx]);
}

/* -----------------------------------------------------------------------
 * Hex
 * --------------------------------------------------------------------- */
void print_hex8(uint8_t n)
{
    char buf[3];
    buf[0] = s_hex[(n >> 4) & 0xF];
    buf[1] = s_hex[n & 0xF];
    buf[2] = '\0';
    print_str(buf);
}

void print_hex_n(uint32_t n, uint8_t digits)
{
    if (digits == 0 || digits > 8) return;
    char buf[9];
    buf[digits] = '\0';
    for (int8_t i = (int8_t)(digits - 1); i >= 0; i--) {
        buf[i] = s_hex[n & 0xF];
        n >>= 4;
    }
    print_str(buf);
}

void print_hex32(uint32_t n)
{
    print_hex_n(n, 8);
}

/* -----------------------------------------------------------------------
 * Floating-point decimal (implemented without <math.h> / soft-float lib)
 * --------------------------------------------------------------------- */
void print_double(double d, uint8_t precision)
{
    if (!s_huart) return;
    if (d < 0.0) { print_str("-"); d = -d; }

    /* Integer part */
    uint32_t int_part = (uint32_t)d;
    print_uint(int_part);

    if (precision == 0) return;

    print_str(".");

    double frac = d - (double)int_part;
    for (uint8_t i = 0; i < precision; i++) {
        frac *= 10.0;
        uint8_t digit = (uint8_t)frac;
        char c[2] = { (char)('0' + digit), '\0' };
        print_str(c);
        frac -= (double)digit;
    }
}

/* -----------------------------------------------------------------------
 * Buffer helpers — build strings in RAM without sprintf
 * --------------------------------------------------------------------- */
uint8_t buf_cat_str(char *buf, uint8_t pos, const char *s)
{
    while (*s) buf[pos++] = *s++;
    buf[pos] = '\0';
    return pos;
}

uint8_t buf_cat_int(char *buf, uint8_t pos, int32_t n)
{
    char tmp[12];
    uint8_t idx = 11;
    tmp[idx] = '\0';
    uint8_t neg = (n < 0);
    uint32_t u = neg ? (uint32_t)(-(n + 1)) + 1U : (uint32_t)n;
    if (u == 0) { tmp[--idx] = '0'; }
    else { while (u) { tmp[--idx] = (char)('0' + (u % 10)); u /= 10; } }
    if (neg) tmp[--idx] = '-';
    return buf_cat_str(buf, pos, &tmp[idx]);
}

uint8_t buf_cat_uint(char *buf, uint8_t pos, uint32_t n)
{
    char tmp[11];
    uint8_t idx = 10;
    tmp[idx] = '\0';
    if (n == 0) { tmp[--idx] = '0'; }
    else { while (n) { tmp[--idx] = (char)('0' + (n % 10)); n /= 10; } }
    return buf_cat_str(buf, pos, &tmp[idx]);
}
