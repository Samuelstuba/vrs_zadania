/*
 * hts221.c
 *
 *  Created on: 5 Nov 2026
 *      Author: Sambi
 */

#include "hts221.h"

static hts221_rw_t i2c_read_cb  = 0;
static hts221_rw_t i2c_write_cb = 0;

/* Kalibracne konstanty nacitane pri inicializacii */
static float   T0_degC, T1_degC, H0_rH, H1_rH;
static int16_t T0_out, T1_out, H0_T0_out, H1_T0_out;

void hts221_register_callbacks(hts221_rw_t read_cb, hts221_rw_t write_cb)
{
    i2c_read_cb  = read_cb;
    i2c_write_cb = write_cb;
}

static uint8_t hts221_read_regs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    if (i2c_read_cb == 0) return 1;
    if (len > 1U) reg |= HTS221_AUTO_INCREMENT;
    return i2c_read_cb(HTS221_I2C_ADDR, reg, buf, len);
}

static uint8_t hts221_write_reg(uint8_t reg, uint8_t value)
{
    if (i2c_write_cb == 0) return 1;
    return i2c_write_cb(HTS221_I2C_ADDR, reg, &value, 1);
}

uint8_t hts221_init(void)
{
    uint8_t who = 0;
    uint8_t c[16];

    /* 1. je senzor pripojeny? */
    if (hts221_read_regs(HTS221_WHO_AM_I_ADDR, &who, 1) != 0U) return 1;
    if (who != HTS221_WHO_AM_I_VALUE) return 1;

    /* 2. zapnutie a nastavenie */
    if (hts221_write_reg(HTS221_CTRL_REG1, HTS221_CTRL_REG1_VALUE) != 0U) return 1;

    /* 3. kalibracia 0x30..0x3F naraz (16 bajtov) */
    if (hts221_read_regs(HTS221_CALIB_START, c, 16) != 0U) return 1;

    H0_rH   = c[0] / 2.0f;                                               /* 0x30 */
    H1_rH   = c[1] / 2.0f;                                               /* 0x31 */
    T0_degC = (float)(((uint16_t)(c[5] & 0x03U) << 8) | c[2]) / 8.0f;    /* 0x32 + 0x35 */
    T1_degC = (float)(((uint16_t)(c[5] & 0x0CU) << 6) | c[3]) / 8.0f;    /* 0x33 + 0x35 */
    H0_T0_out = (int16_t)((uint16_t)c[7]  << 8 | c[6]);                  /* 0x36-0x37 */
    H1_T0_out = (int16_t)((uint16_t)c[11] << 8 | c[10]);                 /* 0x3A-0x3B */
    T0_out    = (int16_t)((uint16_t)c[13] << 8 | c[12]);                 /* 0x3C-0x3D */
    T1_out    = (int16_t)((uint16_t)c[15] << 8 | c[14]);                 /* 0x3E-0x3F */

    return 0;
}

uint8_t hts221_read_temperature(float *temp_c)
{
    uint8_t b[2];
    if (temp_c == 0) return 1;
    if (hts221_read_regs(HTS221_TEMP_OUT_L, b, 2) != 0U) return 1;

    int16_t raw = (int16_t)((uint16_t)b[1] << 8 | b[0]);

    /* linearna interpolacia medzi kalibracnymi bodmi */
    *temp_c = T0_degC + (T1_degC - T0_degC) * (float)(raw - T0_out) / (float)(T1_out - T0_out);
    return 0;
}

uint8_t hts221_read_humidity(float *hum_rh)
{
    uint8_t b[2];
    if (hum_rh == 0) return 1;
    if (hts221_read_regs(HTS221_HUMIDITY_OUT_L, b, 2) != 0U) return 1;

    int16_t raw = (int16_t)((uint16_t)b[1] << 8 | b[0]);

    float h = H0_rH + (H1_rH - H0_rH) * (float)(raw - H0_T0_out) / (float)(H1_T0_out - H0_T0_out);
    if (h < 0.0f)   h = 0.0f;
    if (h > 100.0f) h = 100.0f;
    *hum_rh = h;
    return 0;
}
