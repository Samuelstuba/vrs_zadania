/*
 * lps25hb.c
 *
 *  Created on: 5 Nov 2026
 *      Author: Sambi
 */

#include "lps25hb.h"
#include <math.h>

static lps25hb_rw_t i2c_read_cb  = 0;
static lps25hb_rw_t i2c_write_cb = 0;
static uint8_t      lps25hb_addr = LPS25HB_I2C_ADDR_HIGH;

void lps25hb_register_callbacks(lps25hb_rw_t read_cb, lps25hb_rw_t write_cb)
{
    i2c_read_cb  = read_cb;
    i2c_write_cb = write_cb;
}

static uint8_t lps25hb_read_regs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    if (i2c_read_cb == 0) return 1;
    if (len > 1U) reg |= LPS25HB_AUTO_INCREMENT;
    return i2c_read_cb(lps25hb_addr, reg, buf, len);
}

static uint8_t lps25hb_write_reg(uint8_t reg, uint8_t value)
{
    if (i2c_write_cb == 0) return 1;
    return i2c_write_cb(lps25hb_addr, reg, &value, 1);
}

/* Over WHO_AM_I na aktualne nastavenej adrese */
static uint8_t lps25hb_check_who_am_i(void)
{
    uint8_t who = 0;
    if (lps25hb_read_regs(LPS25HB_WHO_AM_I_ADDR, &who, 1) != 0U) return 1;
    return (who == LPS25HB_WHO_AM_I_VALUE) ? 0 : 1;
}

uint8_t lps25hb_init(void)
{
    /* skus obe mozne adresy */
    lps25hb_addr = LPS25HB_I2C_ADDR_HIGH;
    if (lps25hb_check_who_am_i() != 0U) {
        lps25hb_addr = LPS25HB_I2C_ADDR_LOW;
        if (lps25hb_check_who_am_i() != 0U) return 1;
    }

    return lps25hb_write_reg(LPS25HB_CTRL_REG1, LPS25HB_CTRL_REG1_VALUE);
}

uint8_t lps25hb_read_pressure(float *pressure_hpa)
{
    uint8_t b[3];
    if (pressure_hpa == 0) return 1;
    if (lps25hb_read_regs(LPS25HB_PRESS_OUT_XL, b, 3) != 0U) return 1;

    /* 24-bit dvojkovy doplnok -> 32-bit */
    int32_t raw = (int32_t)((uint32_t)b[2] << 16 | (uint32_t)b[1] << 8 | b[0]);
    if (raw & 0x00800000) raw -= 0x01000000;

    *pressure_hpa = (float)raw / 4096.0f;
    return 0;
}

/* Barometricky vzorec: vyska v metroch voci referencnemu tlaku p0 */
float lps25hb_altitude(float pressure_hpa, float p0_hpa)
{
    if (p0_hpa <= 0.0f) return 0.0f;
    return 44330.0f * (1.0f - powf(pressure_hpa / p0_hpa, 0.190295f)); /* 1/5.255 */
}
