/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    i2c.c
  * @brief   This file provides code for the configuration
  *          of the I2C instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "i2c.h"

/* USER CODE BEGIN 0 */
#define I2C_TIMEOUT  100000U        /* cca desiatky ms pri 8 MHz */

static uint8_t          *i2c_rx_buf = 0;   /* kam uklada prerusenie */
static uint8_t           i2c_rx_len = 0;   /* kolko bajtov cakame   */
static volatile uint8_t  i2c_rx_idx = 0;   /* kolko uz prislo       */

/* Ukonci prenos po chybe (NACK alebo timeout). */
static void i2c_abort(void)
{
    LL_I2C_DisableIT_RX(I2C1);
    if (!LL_I2C_IsActiveFlag_STOP(I2C1))
        LL_I2C_GenerateStopCondition(I2C1);
    for (uint32_t t = I2C_TIMEOUT; t && !LL_I2C_IsActiveFlag_STOP(I2C1); t--) {}
    LL_I2C_ClearFlag_NACK(I2C1);
    LL_I2C_ClearFlag_STOP(I2C1);
}

/* Caka na priznak; pri NACK alebo timeoute ukonci funkciu s chybou 1. */
#define I2C_WAIT(cond)                                              \
    do {                                                            \
        uint32_t t_ = I2C_TIMEOUT;                                  \
        while (!(cond)) {                                           \
            if (LL_I2C_IsActiveFlag_NACK(I2C1) || --t_ == 0U) {     \
                i2c_abort();                                        \
                return 1;                                           \
            }                                                       \
        }                                                           \
    } while (0)
/* USER CODE END 0 */

/* I2C1 init function */
void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  LL_I2C_InitTypeDef I2C_InitStruct = {0};

  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
  /**I2C1 GPIO Configuration
  PB6   ------> I2C1_SCL
  PB7   ------> I2C1_SDA
  */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_6|LL_GPIO_PIN_7;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_4;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* Peripheral clock enable */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C1);

  /* I2C1 interrupt Init */
  NVIC_SetPriority(I2C1_EV_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
  NVIC_EnableIRQ(I2C1_EV_IRQn);

  /* USER CODE BEGIN I2C1_Init 1 */
  /* interne pull-up rezistory na SDA a SCL (ako na cviceni) */
  LL_GPIO_SetPinPull(GPIOB, LL_GPIO_PIN_6, LL_GPIO_PULL_UP);
  LL_GPIO_SetPinPull(GPIOB, LL_GPIO_PIN_7, LL_GPIO_PULL_UP);
  /* USER CODE END I2C1_Init 1 */

  /** I2C Initialization
  */
  LL_I2C_EnableAutoEndMode(I2C1);
  LL_I2C_DisableOwnAddress2(I2C1);
  LL_I2C_DisableGeneralCall(I2C1);
  LL_I2C_EnableClockStretching(I2C1);
  I2C_InitStruct.PeripheralMode = LL_I2C_MODE_I2C;
  I2C_InitStruct.Timing = 0x00201D2B;
  I2C_InitStruct.AnalogFilter = LL_I2C_ANALOGFILTER_ENABLE;
  I2C_InitStruct.DigitalFilter = 0;
  I2C_InitStruct.OwnAddress1 = 0;
  I2C_InitStruct.TypeAcknowledge = LL_I2C_ACK;
  I2C_InitStruct.OwnAddrSize = LL_I2C_OWNADDRESS1_7BIT;
  LL_I2C_Init(I2C1, &I2C_InitStruct);
  LL_I2C_SetOwnAddress2(I2C1, 0, LL_I2C_OWNADDRESS2_NOMASK);
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/* USER CODE BEGIN 1 */
/* Precita len bajtov od registra reg_addr zariadenia dev_addr (8-bit tvar adresy).
 * Ak chce senzor auto-increment, nastavi si ho v reg_addr sam.
 * Navratova hodnota: 0 = OK, 1 = chyba.                                         */
uint8_t i2c_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint8_t len)
{
    if (data == 0 || len == 0U) return 1;

    i2c_rx_buf = data;
    i2c_rx_len = len;
    i2c_rx_idx = 0;

    /* 1. faza: START + adresa zariadenia (W) + adresa registra, bez STOP */
    LL_I2C_HandleTransfer(I2C1, dev_addr, LL_I2C_ADDRSLAVE_7BIT, 1,
                          LL_I2C_MODE_SOFTEND, LL_I2C_GENERATE_START_WRITE);
    I2C_WAIT(LL_I2C_IsActiveFlag_TXIS(I2C1));
    LL_I2C_TransmitData8(I2C1, reg_addr);
    I2C_WAIT(LL_I2C_IsActiveFlag_TC(I2C1));

    /* 2. faza: repeated START + adresa (R) + len bajtov, potom automaticky STOP.
     *    Bajty uklada prerusenie I2C1_EV do i2c_rx_buf.                        */
    LL_I2C_EnableIT_RX(I2C1);
    LL_I2C_HandleTransfer(I2C1, dev_addr, LL_I2C_ADDRSLAVE_7BIT, len,
                          LL_I2C_MODE_AUTOEND, LL_I2C_GENERATE_START_READ);
    I2C_WAIT(LL_I2C_IsActiveFlag_STOP(I2C1));

    LL_I2C_DisableIT_RX(I2C1);
    LL_I2C_ClearFlag_STOP(I2C1);

    return (i2c_rx_idx == len) ? 0 : 1;
}

/* Zapise len bajtov od registra reg_addr. Navrat: 0 = OK, 1 = chyba. */
uint8_t i2c_write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint8_t len)
{
    if (data == 0 || len == 0U || len > 254U) return 1;

    LL_I2C_HandleTransfer(I2C1, dev_addr, LL_I2C_ADDRSLAVE_7BIT, len + 1U,
                          LL_I2C_MODE_AUTOEND, LL_I2C_GENERATE_START_WRITE);
    I2C_WAIT(LL_I2C_IsActiveFlag_TXIS(I2C1));
    LL_I2C_TransmitData8(I2C1, reg_addr);

    for (uint8_t i = 0; i < len; i++) {
        I2C_WAIT(LL_I2C_IsActiveFlag_TXIS(I2C1));
        LL_I2C_TransmitData8(I2C1, data[i]);
    }
    I2C_WAIT(LL_I2C_IsActiveFlag_STOP(I2C1));
    LL_I2C_ClearFlag_STOP(I2C1);
    return 0;
}

/* Vola sa z I2C1_EV_IRQHandler v stm32f3xx_it.c */
void i2c_ev_callback(void)
{
    if (LL_I2C_IsActiveFlag_RXNE(I2C1)) {
        uint8_t b = LL_I2C_ReceiveData8(I2C1);
        if (i2c_rx_idx < i2c_rx_len) {
            i2c_rx_buf[i2c_rx_idx++] = b;
        }
    }
}
/* USER CODE END 1 */

