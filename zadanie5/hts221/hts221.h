/*
 * hts221.h
 *
 *  Created on: 5 Nov 2026
 *      Author: Sambi
 *
 *  Kniznica pre senzor teploty a vlhkosti HTS221 (doska X-NUCLEO-IKS01A1).
 */

#ifndef HTS221_H_
#define HTS221_H_

#include <stdint.h>

/* I2C adresa v 8-bit tvare (7-bit 0x5F posunuta o 1 dolava) */
#define HTS221_I2C_ADDR         0xBEU

/* Registre */
#define HTS221_WHO_AM_I_ADDR    0x0FU
#define HTS221_WHO_AM_I_VALUE   0xBCU
#define HTS221_CTRL_REG1        0x20U
#define HTS221_HUMIDITY_OUT_L   0x28U
#define HTS221_TEMP_OUT_L       0x2AU
#define HTS221_CALIB_START      0x30U   /* 0x30 az 0x3F = kalibracia */

#define HTS221_AUTO_INCREMENT   0x80U   /* MSB adresy registra = auto-increment */

/* CTRL_REG1: PD=1 (zapnuty), BDU=1, ODR=01 (1 Hz) */
#define HTS221_CTRL_REG1_VALUE  0x85U

/* Typ callback funkcie pre citanie/zapis cez I2C (napr. i2c_read / i2c_write) */
typedef uint8_t (*hts221_rw_t)(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint8_t len);

void    hts221_register_callbacks(hts221_rw_t read_cb, hts221_rw_t write_cb);
uint8_t hts221_init(void);                       /* 0 = OK, 1 = chyba */
uint8_t hts221_read_temperature(float *temp_c);  /* stupne C */
uint8_t hts221_read_humidity(float *hum_rh);     /* % rel. vlhkosti */

#endif /* HTS221_H_ */
