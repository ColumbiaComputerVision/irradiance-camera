/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l0xx_ll_dma.h"
#include "stm32l0xx_ll_lptim.h"
#include "stm32l0xx_ll_crs.h"
#include "stm32l0xx_ll_rcc.h"
#include "stm32l0xx_ll_bus.h"
#include "stm32l0xx_ll_system.h"
#include "stm32l0xx_ll_exti.h"
#include "stm32l0xx_ll_cortex.h"
#include "stm32l0xx_ll_utils.h"
#include "stm32l0xx_ll_pwr.h"
#include "stm32l0xx_ll_spi.h"
#include "stm32l0xx_ll_tim.h"
#include "stm32l0xx_ll_gpio.h"

#if defined(USE_FULL_ASSERT)
#include "stm32_assert.h"
#endif /* USE_FULL_ASSERT */

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void SPI1_RX_DMA_Callback();
void SPI2_TX_DMA_Callback();

extern volatile uint8_t tim6_wakeup_flag;
extern volatile uint8_t lptimer_wakeup_flag;
extern volatile uint8_t rf_spi_ready_flag;
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define CTR_A0_Pin LL_GPIO_PIN_0
#define CTR_A0_GPIO_Port GPIOA
#define CTR_A1_Pin LL_GPIO_PIN_1
#define CTR_A1_GPIO_Port GPIOA
#define CTR_A2_Pin LL_GPIO_PIN_2
#define CTR_A2_GPIO_Port GPIOA
#define CTR_A3_Pin LL_GPIO_PIN_3
#define CTR_A3_GPIO_Port GPIOA
#define CTR_A4_Pin LL_GPIO_PIN_4
#define CTR_A4_GPIO_Port GPIOA
#define SPI1_CS_Pin LL_GPIO_PIN_5
#define SPI1_CS_GPIO_Port GPIOA
#define TIA_ENBL_Pin LL_GPIO_PIN_6
#define TIA_ENBL_GPIO_Port GPIOA
#define CTR_A5_Pin LL_GPIO_PIN_7
#define CTR_A5_GPIO_Port GPIOA
#define HARV_VOLT_Pin LL_GPIO_PIN_0
#define HARV_VOLT_GPIO_Port GPIOB
#define RF_RESET_Pin LL_GPIO_PIN_1
#define RF_RESET_GPIO_Port GPIOB
#define RF_WAKEUP_Pin LL_GPIO_PIN_11
#define RF_WAKEUP_GPIO_Port GPIOB
#define SPI2_CS_Pin LL_GPIO_PIN_12
#define SPI2_CS_GPIO_Port GPIOB
#define CTR_A6_Pin LL_GPIO_PIN_8
#define CTR_A6_GPIO_Port GPIOA
#define CTR_A7_Pin LL_GPIO_PIN_9
#define CTR_A7_GPIO_Port GPIOA
#define RF_SPI_READY_Pin LL_GPIO_PIN_9
#define RF_SPI_READY_GPIO_Port GPIOB
#define RF_SPI_READY_EXTI_IRQn EXTI4_15_IRQn
#ifndef NVIC_PRIORITYGROUP_0
#define NVIC_PRIORITYGROUP_0         ((uint32_t)0x00000007) /*!< 0 bit  for pre-emption priority,
                                                                 4 bits for subpriority */
#define NVIC_PRIORITYGROUP_1         ((uint32_t)0x00000006) /*!< 1 bit  for pre-emption priority,
                                                                 3 bits for subpriority */
#define NVIC_PRIORITYGROUP_2         ((uint32_t)0x00000005) /*!< 2 bits for pre-emption priority,
                                                                 2 bits for subpriority */
#define NVIC_PRIORITYGROUP_3         ((uint32_t)0x00000004) /*!< 3 bits for pre-emption priority,
                                                                 1 bit  for subpriority */
#define NVIC_PRIORITYGROUP_4         ((uint32_t)0x00000003) /*!< 4 bits for pre-emption priority,
                                                                 0 bit  for subpriority */
#endif

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
