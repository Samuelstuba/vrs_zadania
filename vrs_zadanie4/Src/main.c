/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"
#include <stdio.h>
#include <string.h>

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);


/* Function processing DMA Rx data. Counts how many capital and small letters are in sentence.
 * Result is supposed to be stored in global variable of type "letter_count_" that is defined in "main.h"
 *
 * @param1 - received sign
 */
void proccesDmaData(uint8_t sign);


/* Space for your global variables. */

#define START_SIGN			'#'
#define END_SIGN			'$'
#define MAX_STRING_LEN		35		// max. number of signs after START_SIGN until END_SIGN must come
#define STATUS_PERIOD_MS	2000	// 0.5 Hz

/* Result of last valid received string */
volatile letter_count_ letter_count = {0};

/* Processing state */
static uint8_t  receiving = 0;							// 1 = START_SIGN received, waiting for END_SIGN
static uint8_t  rx_len = 0;								// number of signs received after START_SIGN
static char     rx_string[MAX_STRING_LEN + 1];			// signs between START_SIGN and END_SIGN
static uint8_t  rx_small = 0, rx_capital = 0;

/* Last valid string, handed over from callback (interrupt) to main loop */
static char     valid_string[MAX_STRING_LEN + 1];
static volatile uint8_t valid_string_ready = 0;

/* Tx buffer for USART2 DMA - must stay valid until DMA transmission ends */
static uint8_t  tx_buffer[128];

/* Millisecond counter incremented in SysTick_Handler (stm32f3xx_it.c) */
extern volatile uint32_t sys_tick_ms;

static void USART2_SendString(const char *str);
static void sendBufferStatus(void);


int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
  NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

  /* Configure the system clock */
  SystemClock_Config();
  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();

  /* Space for your local variables, callback registration ...*/

  USART2_RegisterCallback(proccesDmaData);
  LL_SYSTICK_EnableIT();

  uint32_t last_status_time = sys_tick_ms;
  char local_string[MAX_STRING_LEN + 1];
  letter_count_ local_count;

  while (1)
  {
	  /* Periodic transmission of information about DMA Rx buffer state.
	   * Transmission frequency - 0.5Hz.
	   * Message format - "Buffer capacity: %d bytes, occupied memory: %d bytes, load [in %]: %f%"
	   * Example message (what I wish to see in terminal) - Buffer capacity: 1000 bytes, occupied memory: 231 bytes, load [in %]: 23.1%
	   */

	  /* Valid text string information transmission.
	   * Transmission frequency - when new valid string is received.
	   * Message format - "Valid string: %s, lower-case: %d, upper-case: %d"
	   * Example message (what I wish to see in terminal) - Valid string: Platn15uborZnakov, lower-case: 13, upper-case: 2
	   */

	  if((sys_tick_ms - last_status_time) >= STATUS_PERIOD_MS)
	  {
		  last_status_time += STATUS_PERIOD_MS;
		  sendBufferStatus();
	  }

	  if(valid_string_ready)
	  {
		  /* copy data written in interrupt, so it can not change while it is being sent */
		  __disable_irq();
		  strcpy(local_string, valid_string);
		  local_count = letter_count;
		  valid_string_ready = 0;
		  __enable_irq();

		  char msg[100];
		  snprintf(msg, sizeof(msg), "Valid string: %s, lower-case: %d, upper-case: %d\r\n",
				  local_string, local_count.small_letter, local_count.capital_letter);
		  USART2_SendString(msg);
	  }
  }
  /* USER CODE END 3 */
}


void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_0);

  if(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_0)
  {
  Error_Handler();  
  }
  LL_RCC_HSI_Enable();

   /* Wait till HSI is ready */
  while(LL_RCC_HSI_IsReady() != 1)
  {
    
  }
  LL_RCC_HSI_SetCalibTrimming(16);
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI)
  {
  
  }
  LL_Init1msTick(8000000);
  LL_SYSTICK_SetClkSource(LL_SYSTICK_CLKSOURCE_HCLK);
  LL_SetSystemCoreClock(8000000);
}

/*
 * Implementation of function processing data received via USART.
 */
void proccesDmaData(uint8_t sign)
{
	/* Process received data */
	if(!receiving)
	{
		/* Ignore everything until START_SIGN */
		if(sign == START_SIGN)
		{
			receiving = 1;
			rx_len = 0;
			rx_small = 0;
			rx_capital = 0;
		}
		return;
	}

	if(sign == END_SIGN)
	{
		/* Valid string received -> store result */
		rx_string[rx_len] = '\0';
		strcpy(valid_string, rx_string);
		letter_count.small_letter = rx_small;
		letter_count.capital_letter = rx_capital;
		valid_string_ready = 1;
		receiving = 0;
		return;
	}

	if(rx_len >= MAX_STRING_LEN - 1)
	{
		/* END_SIGN was not received during MAX_STRING_LEN signs -> discard data, wait for new START_SIGN */
		receiving = 0;
		return;
	}

	rx_string[rx_len++] = (char)sign;

	if(sign >= 'a' && sign <= 'z')
	{
		rx_small++;
	}
	else if(sign >= 'A' && sign <= 'Z')
	{
		rx_capital++;
	}
}

/*
 * Sends string via USART2 DMA. Waits for previous transmission to finish.
 */
static void USART2_SendString(const char *str)
{
	while(USART2_IsTxBusy());

	uint16_t len = strlen(str);
	if(len > sizeof(tx_buffer)) len = sizeof(tx_buffer);

	memcpy(tx_buffer, str, len);
	USART2_PutBuffer(tx_buffer, (uint8_t)len);
}

/*
 * Sends information about DMA Rx buffer state.
 * Load is computed in integers (no float printf needed) with 2 decimal places.
 */
static void sendBufferStatus(void)
{
	char msg[100];
	uint16_t occupied = USART2_GetDmaBufferOccupied();
	uint32_t load = ((uint32_t)occupied * 10000U) / DMA_USART2_BUFFER_SIZE;	// load in % * 100

	snprintf(msg, sizeof(msg), "Buffer capacity: %d bytes, occupied memory: %d bytes, load [in %%]: %lu.%02lu%%\r\n",
			DMA_USART2_BUFFER_SIZE, occupied, (unsigned long)(load / 100), (unsigned long)(load % 100));
	USART2_SendString(msg);
}


void Error_Handler(void)
{

}

#ifdef  USE_FULL_ASSERT

void assert_failed(char *file, uint32_t line)
{ 

}

#endif

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
