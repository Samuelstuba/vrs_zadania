/*
 * lps25hb.h
 *
 *  Created on: 5 Nov 2026
 *      Author: Sambi
 *
 *  Kniznica pre tlakovy senzor LPS25HB (doska X-NUCLEO-IKS01A1).
 */

#ifndef LPS25HB_H_
#define LPS25HB_H_

#include <stdint.h>

/* I2C adresa v 8-bit tvare, zavisi od pinu SA0 */
#define LPS25HB_I2C_ADDR_HIGH    0xBAU   /* SA0 = 1 (7-bit 0x5D) */
#define LPS25HB_I2C_ADDR_LOW     0xB8U   /* SA0 = 0 (7-bit 0x5C) */

/* Registre */
#define LPS25HB_WHO_AM_I_ADDR    0x0FU
#define LPS25HB_WHO_AM_I_VALUE   0xBDU
#define LPS25HB_CTRL_REG1        0x20U
#define LPS25HB_PRESS_OUT_XL     0x28U   /* 0x28, 0x29, 0x2A = 24-bit tlak */

#define LPS25HB_AUTO_INCREMENT   0x80U

/* CTRL_REG1: PD=1, ODR=001 (1 Hz), BDU=1 */
#define LPS25HB_CTRL_REG1_VALUE  0x94U

typedef uint8_t (*lps25hb_rw_t)(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint8_t len);

void    lps25hb_register_callbacks(lps25hb_rw_t read_cb, lps25hb_rw_t write_cb);
uint8_t lps25hb_init(void);                                  /* 0 = OK, 1 = chyba */
uint8_t lps25hb_read_pressure(float *pressure_hpa);          /* hPa */
float   lps25hb_altitude(float pressure_hpa, float p0_hpa);  /* m, voci tlaku p0 */

#endif /* LPS25HB_H_ */
