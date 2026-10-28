/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define NUM_SENSORS 49
#define RX_BUFFER_SIZE 3
#define TIMEOUT_DURATION 100

#define PACKET_SIZE (NUM_SENSORS * 2) // 98 Bytes

#define FPS 30
#define FRAME_TIMER_PERIOD (37000 / (4 * FPS))

// 30us: 130 -- 620uA
// 50us: 450 -- 680uA total system
// 100us: 1250 -- 800uA
// 200us: 2800 -- 1030 uA
// 400us: 6000 -- 1.55 mA
// 500us: 7800 --1.86mA // at this point, enter stop mode during settling time. This was consuming around 1.1mA
#define SETTLING_TIME_PERIOD 450

#define ENABLE_RF
//#define RX_TESTER
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint16_t readoutBuffer[NUM_SENSORS]; // Buffer for entire frame
uint8_t rxDataBuffer[RX_BUFFER_SIZE]; // RX buffer for a single sensor
uint8_t txDataBuffer[RX_BUFFER_SIZE]; // TX buffer for a single sensor
uint8_t spi2TxBuffer[PACKET_SIZE];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_SPI2_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM6_Init(void);
static void MX_LPTIM1_Init(void);
/* USER CODE BEGIN PFP */
void set_mux(uint8_t addr);
void readout_sensors();
void transfer_to_rf();

static void SPI2_start_tx_dma(uint8_t *tx_buf, uint16_t len);
static void SPI1_start_rx_dma();
static void start_frame_timer();
static void start_settling_time_timer();
static void enter_stop_mode();

static void deinit_spi1();
static void reinit_spi1();
static void deinit_spi2();
static void reinit_spi2();
static void deinit_readout_pins();
static void reinit_readout_pins();
static void deinit_rf_pins();
static void reinit_rf_pins();
static void deinit_peripherals();
static void reinit_peripherals();
static void reinit_tim6();
static void deinit_tim6();

#ifdef RX_TESTER
void create_sequential_packet();
uint16_t packet_counter = 0;
#endif

volatile uint8_t spi1_rx_done = 0;
volatile uint8_t spi2_tx_done = 0;
volatile uint8_t addr = 0;
volatile uint8_t lptimer_wakeup_flag = 0;
volatile uint8_t tim6_wakeup_flag = 0;
volatile uint8_t rf_spi_ready_flag = 0;
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

  /* SysTick_IRQn interrupt configuration */
//  NVIC_SetPriority(SysTick_IRQn, 3);

  /* USER CODE BEGIN Init */
	for (int i = 0; i < NUM_SENSORS; i++) {
		readoutBuffer[i] = 0;
	}
	for (int i = 0; i < PACKET_SIZE; i++) {
		spi2TxBuffer[i] = 0;
	}
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI2_Init();
  MX_SPI1_Init();
  MX_TIM6_Init();
  MX_LPTIM1_Init();
  /* USER CODE BEGIN 2 */
#ifdef ENABLE_RF
	// Boot RF microcontroller
	LL_GPIO_SetPinMode(RF_RESET_GPIO_Port, RF_RESET_Pin, LL_GPIO_MODE_ANALOG);
	for (volatile int i = 0; i < 10000; i++) {
	  __NOP();
	}
#endif

	deinit_spi1();
	deinit_spi2();
	deinit_readout_pins();
	deinit_rf_pins();
	deinit_tim6();

	start_frame_timer();

	// Use HSI on wakeup
	LL_RCC_SetClkAfterWakeFromStop(LL_RCC_STOP_WAKEUPCLOCK_HSI);
	LL_PWR_SetRegulModeLP(LL_PWR_REGU_LPMODES_LOW_POWER);
	LL_FLASH_EnableSleepPowerDown();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  deinit_peripherals();
	  while (!lptimer_wakeup_flag)
		  enter_stop_mode();
	  lptimer_wakeup_flag = 0;
	  reinit_peripherals();

	  readout_sensors();

#ifdef RX_TESTER
	  create_sequential_packet();
#endif

#ifdef ENABLE_RF
	  transfer_to_rf();
#endif
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_1);
  while(LL_FLASH_GetLatency()!= LL_FLASH_LATENCY_1)
  {
  }
  LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE2);
  while (LL_PWR_IsActiveFlag_VOS() != 0)
  {
  }
  LL_RCC_HSI_Enable();

   /* Wait till HSI is ready */
  while(LL_RCC_HSI_IsReady() != 1)
  {

  }
  LL_RCC_HSI_SetCalibTrimming(16);
  LL_RCC_LSI_Enable();

   /* Wait till LSI is ready */
  while(LL_RCC_LSI_IsReady() != 1)
  {

  }
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI)
  {

  }

//  LL_Init1msTick(16000000);

  LL_SetSystemCoreClock(16000000);
  LL_RCC_SetLPTIMClockSource(LL_RCC_LPTIM1_CLKSOURCE_LSI);
}

/**
  * @brief LPTIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_LPTIM1_Init(void)
{

  /* USER CODE BEGIN LPTIM1_Init 0 */

  /* USER CODE END LPTIM1_Init 0 */

  /* Peripheral clock enable */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_LPTIM1);

  /* LPTIM1 interrupt Init */
  NVIC_SetPriority(LPTIM1_IRQn, 0);
  NVIC_EnableIRQ(LPTIM1_IRQn);

  /* USER CODE BEGIN LPTIM1_Init 1 */

  /* USER CODE END LPTIM1_Init 1 */
  LL_LPTIM_SetClockSource(LPTIM1, LL_LPTIM_CLK_SOURCE_INTERNAL);
  LL_LPTIM_SetPrescaler(LPTIM1, LL_LPTIM_PRESCALER_DIV4);
  LL_LPTIM_SetPolarity(LPTIM1, LL_LPTIM_OUTPUT_POLARITY_REGULAR);
  LL_LPTIM_SetUpdateMode(LPTIM1, LL_LPTIM_UPDATE_MODE_IMMEDIATE);
  LL_LPTIM_SetCounterMode(LPTIM1, LL_LPTIM_COUNTER_MODE_INTERNAL);
  LL_LPTIM_TrigSw(LPTIM1);
  /* USER CODE BEGIN LPTIM1_Init 2 */

  /* USER CODE END LPTIM1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  LL_SPI_InitTypeDef SPI_InitStruct = {0};

  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Peripheral clock enable */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);

  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);
  /**SPI1 GPIO Configuration
  PA12   ------> SPI1_MOSI
  PB3   ------> SPI1_SCK
  PB4   ------> SPI1_MISO
  */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_12;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_0;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LL_GPIO_PIN_3;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_0;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LL_GPIO_PIN_4;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_0;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* SPI1 DMA Init */

  /* SPI1_RX Init */
  LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_2, LL_DMA_REQUEST_1);

  LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_2, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

  LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_2, LL_DMA_PRIORITY_LOW);

  LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MODE_NORMAL);

  LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_PERIPH_NOINCREMENT);

  LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MEMORY_INCREMENT);

  LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_2, LL_DMA_PDATAALIGN_BYTE);

  LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MDATAALIGN_BYTE);

  /* SPI1_TX Init */
  LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_3, LL_DMA_REQUEST_1);

  LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_3, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);

  LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_3, LL_DMA_PRIORITY_LOW);

  LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_3, LL_DMA_MODE_NORMAL);

  LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_3, LL_DMA_PERIPH_NOINCREMENT);

  LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_3, LL_DMA_MEMORY_INCREMENT);

  LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_3, LL_DMA_PDATAALIGN_BYTE);

  LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_3, LL_DMA_MDATAALIGN_BYTE);

  /* USER CODE BEGIN SPI1_Init 1 */
  LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_2);
  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  SPI_InitStruct.TransferDirection = LL_SPI_FULL_DUPLEX;
  SPI_InitStruct.Mode = LL_SPI_MODE_MASTER;
  SPI_InitStruct.DataWidth = LL_SPI_DATAWIDTH_8BIT;
  SPI_InitStruct.ClockPolarity = LL_SPI_POLARITY_LOW;
  SPI_InitStruct.ClockPhase = LL_SPI_PHASE_2EDGE;
  SPI_InitStruct.NSS = LL_SPI_NSS_SOFT;
  SPI_InitStruct.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV8;
  SPI_InitStruct.BitOrder = LL_SPI_MSB_FIRST;
  SPI_InitStruct.CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;
  SPI_InitStruct.CRCPoly = 7;
  LL_SPI_Init(SPI1, &SPI_InitStruct);
  LL_SPI_SetStandard(SPI1, LL_SPI_PROTOCOL_MOTOROLA);
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  LL_SPI_InitTypeDef SPI_InitStruct = {0};

  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Peripheral clock enable */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_SPI2);

  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);
  /**SPI2 GPIO Configuration
  PB13   ------> SPI2_SCK
  PB15   ------> SPI2_MOSI
  */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_13;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_0;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LL_GPIO_PIN_15;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_0;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* SPI2 DMA Init */

  /* SPI2_TX Init */
  LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_5, LL_DMA_REQUEST_2);

  LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_5, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);

  LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_5, LL_DMA_PRIORITY_LOW);

  LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_5, LL_DMA_MODE_NORMAL);

  LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_5, LL_DMA_PERIPH_NOINCREMENT);

  LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_5, LL_DMA_MEMORY_INCREMENT);

  LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_5, LL_DMA_PDATAALIGN_BYTE);

  LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_5, LL_DMA_MDATAALIGN_BYTE);

  /* USER CODE BEGIN SPI2_Init 1 */
  LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_5);
  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  SPI_InitStruct.TransferDirection = LL_SPI_FULL_DUPLEX;
  SPI_InitStruct.Mode = LL_SPI_MODE_MASTER;
  SPI_InitStruct.DataWidth = LL_SPI_DATAWIDTH_8BIT;
  SPI_InitStruct.ClockPolarity = LL_SPI_POLARITY_LOW;
  SPI_InitStruct.ClockPhase = LL_SPI_PHASE_1EDGE;
  SPI_InitStruct.NSS = LL_SPI_NSS_SOFT;
  SPI_InitStruct.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV4;
  SPI_InitStruct.BitOrder = LL_SPI_MSB_FIRST;
  SPI_InitStruct.CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;
  SPI_InitStruct.CRCPoly = 7;
  LL_SPI_Init(SPI2, &SPI_InitStruct);
  LL_SPI_SetStandard(SPI2, LL_SPI_PROTOCOL_MOTOROLA);
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  LL_TIM_InitTypeDef TIM_InitStruct = {0};

  /* Peripheral clock enable */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM6);

  /* TIM6 interrupt Init */
  NVIC_SetPriority(TIM6_DAC_IRQn, 0);
  NVIC_EnableIRQ(TIM6_DAC_IRQn);

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  TIM_InitStruct.Prescaler = 0;
  TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
  TIM_InitStruct.Autoreload = SETTLING_TIME_PERIOD;
  LL_TIM_Init(TIM6, &TIM_InitStruct);
  LL_TIM_DisableARRPreload(TIM6);
  LL_TIM_SetTriggerOutput(TIM6, LL_TIM_TRGO_RESET);
  LL_TIM_DisableMasterSlaveMode(TIM6);
  /* USER CODE BEGIN TIM6_Init 2 */
  LL_TIM_SetOnePulseMode(TIM6, LL_TIM_ONEPULSEMODE_SINGLE);
  /* USER CODE END TIM6_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* Init with LL driver */
  /* DMA controller clock enable */
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);

  /* DMA interrupt init */
  /* DMA1_Channel2_3_IRQn interrupt configuration */
  NVIC_SetPriority(DMA1_Channel2_3_IRQn, 0);
  NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
  /* DMA1_Channel4_5_6_7_IRQn interrupt configuration */
  NVIC_SetPriority(DMA1_Channel4_5_6_7_IRQn, 0);
  NVIC_EnableIRQ(DMA1_Channel4_5_6_7_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  LL_EXTI_InitTypeDef EXTI_InitStruct = {0};
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOC);
  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOH);
  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);

  /**/
  LL_GPIO_ResetOutputPin(CTR_A0_GPIO_Port, CTR_A0_Pin);

  /**/
  LL_GPIO_ResetOutputPin(CTR_A1_GPIO_Port, CTR_A1_Pin);

  /**/
  LL_GPIO_ResetOutputPin(CTR_A2_GPIO_Port, CTR_A2_Pin);

  /**/
  LL_GPIO_ResetOutputPin(CTR_A3_GPIO_Port, CTR_A3_Pin);

  /**/
  LL_GPIO_ResetOutputPin(CTR_A4_GPIO_Port, CTR_A4_Pin);

  /**/
  LL_GPIO_ResetOutputPin(TIA_ENBL_GPIO_Port, TIA_ENBL_Pin);

  /**/
  LL_GPIO_ResetOutputPin(CTR_A5_GPIO_Port, CTR_A5_Pin);

  /**/
  LL_GPIO_ResetOutputPin(RF_RESET_GPIO_Port, RF_RESET_Pin);

  /**/
  LL_GPIO_ResetOutputPin(RF_WAKEUP_GPIO_Port, RF_WAKEUP_Pin);

  /**/
  LL_GPIO_ResetOutputPin(CTR_A6_GPIO_Port, CTR_A6_Pin);

  /**/
  LL_GPIO_ResetOutputPin(CTR_A7_GPIO_Port, CTR_A7_Pin);

  /**/
  LL_GPIO_SetOutputPin(SPI1_CS_GPIO_Port, SPI1_CS_Pin);

  /**/
  LL_GPIO_SetOutputPin(SPI2_CS_GPIO_Port, SPI2_CS_Pin);

  /**/
  GPIO_InitStruct.Pin = LL_GPIO_PIN_13;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LL_GPIO_PIN_14;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LL_GPIO_PIN_15;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LL_GPIO_PIN_0;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LL_GPIO_PIN_1;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = CTR_A0_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(CTR_A0_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = CTR_A1_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(CTR_A1_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = CTR_A2_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(CTR_A2_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = CTR_A3_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(CTR_A3_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = CTR_A4_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(CTR_A4_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = SPI1_CS_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(SPI1_CS_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = TIA_ENBL_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(TIA_ENBL_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = CTR_A5_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(CTR_A5_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = HARV_VOLT_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(HARV_VOLT_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = RF_RESET_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(RF_RESET_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LL_GPIO_PIN_2;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LL_GPIO_PIN_10;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = RF_WAKEUP_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(RF_WAKEUP_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = SPI2_CS_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(SPI2_CS_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LL_GPIO_PIN_14;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = CTR_A6_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(CTR_A6_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = CTR_A7_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(CTR_A7_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LL_GPIO_PIN_10;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LL_GPIO_PIN_11;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LL_GPIO_PIN_15;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LL_GPIO_PIN_5;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LL_GPIO_PIN_6;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LL_GPIO_PIN_7;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LL_GPIO_PIN_8;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /**/
  LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTB, LL_SYSCFG_EXTI_LINE9);

  /**/
  LL_GPIO_SetPinPull(RF_SPI_READY_GPIO_Port, RF_SPI_READY_Pin, LL_GPIO_PULL_NO);

  /**/
  LL_GPIO_SetPinMode(RF_SPI_READY_GPIO_Port, RF_SPI_READY_Pin, LL_GPIO_MODE_INPUT);

  /**/
  EXTI_InitStruct.Line_0_31 = LL_EXTI_LINE_9;
  EXTI_InitStruct.LineCommand = ENABLE;
  EXTI_InitStruct.Mode = LL_EXTI_MODE_IT;
  EXTI_InitStruct.Trigger = LL_EXTI_TRIGGER_RISING;
  LL_EXTI_Init(&EXTI_InitStruct);

  /* EXTI interrupt init*/
  NVIC_SetPriority(EXTI4_15_IRQn, 0);
  NVIC_EnableIRQ(EXTI4_15_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void set_mux(uint8_t address) {
    // First clear all CTR pins
    LL_GPIO_ResetOutputPin(
    		CTR_A0_GPIO_Port,
			CTR_A0_Pin | CTR_A1_Pin | CTR_A2_Pin | CTR_A3_Pin | CTR_A4_Pin | CTR_A5_Pin | CTR_A6_Pin | CTR_A7_Pin);

    if (address < 48) {
    	// Lower 4 bits
		LL_GPIO_SetOutputPin(CTR_A0_GPIO_Port, (uint32_t) (address & 0xF));
		if (address < 16)
			LL_GPIO_SetOutputPin(CTR_A4_GPIO_Port, CTR_A4_Pin);
		else if (address < 32)
			LL_GPIO_SetOutputPin(CTR_A5_GPIO_Port, CTR_A5_Pin);
		else
			LL_GPIO_SetOutputPin(CTR_A6_GPIO_Port, CTR_A6_Pin);
    } else if (address == 48) {
    	LL_GPIO_SetOutputPin(CTR_A7_GPIO_Port, CTR_A7_Pin);
    }
}

void readout_sensors() {
#if (SETTLING_TIME_PERIOD > 0)
	reinit_tim6();
#endif
	reinit_spi1();
	reinit_readout_pins();

	// J12, Mux 11, addr=10
	// J13, Mux 12, addr=11
	LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_2, LL_SPI_DMA_GetRegAddr(SPI1));
	LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_2, (uint32_t) rxDataBuffer);

	// Configure TX
	LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_3, LL_SPI_DMA_GetRegAddr(SPI1));
	LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_3, (uint32_t) txDataBuffer);

	LL_SPI_EnableDMAReq_RX(SPI1);
	LL_SPI_EnableDMAReq_TX(SPI1);

	addr = 0;
	set_mux(addr);
	LL_GPIO_SetOutputPin(SPI1_CS_GPIO_Port, SPI1_CS_Pin); // Set CS
	LL_GPIO_SetOutputPin(TIA_ENBL_GPIO_Port, TIA_ENBL_Pin); // Set TIA ENABLE

	while (addr < NUM_SENSORS) {
#if (SETTLING_TIME_PERIOD > 0)
		// Wait for settling time
		start_settling_time_timer();
		while (!tim6_wakeup_flag)
			__WFI();
		tim6_wakeup_flag = 0;
#endif

		// Start SPI readout
		SPI1_start_rx_dma();
		while (!spi1_rx_done) {
			__WFI();
		}
		spi1_rx_done = 0;

		readoutBuffer[addr] = ((rxDataBuffer[0] << 16) | (rxDataBuffer[1] << 8) | (rxDataBuffer[2])) >> 2;
		addr++;
	}

	LL_GPIO_ResetOutputPin(TIA_ENBL_GPIO_Port, TIA_ENBL_Pin);

	LL_SPI_DisableDMAReq_RX(SPI1);
	LL_SPI_DisableDMAReq_TX(SPI1);

#if (SETTLING_TIME_PERIOD > 0)
	deinit_tim6();
#endif
	deinit_spi1();
	deinit_readout_pins();
}

void transfer_to_rf() {
	reinit_spi2();
	reinit_rf_pins();

	// Transfer readoutBuffer to spi2TxBuffer
	for (int i = 0; i < NUM_SENSORS; i++) {
		spi2TxBuffer[i * 2] = (uint8_t) (readoutBuffer[i] & 0xFF);
		spi2TxBuffer[i * 2 + 1] = (uint8_t) ((readoutBuffer[i] >> 8) & 0xFF);
	}

	rf_spi_ready_flag = 0;
	LL_GPIO_SetOutputPin(RF_WAKEUP_GPIO_Port, RF_WAKEUP_Pin);

	// Sleep until RF MCU signals ready — wakes via EXTI
	deinit_peripherals();
	while (!rf_spi_ready_flag)
		enter_stop_mode();
	reinit_peripherals();

	SPI2_start_tx_dma(spi2TxBuffer, PACKET_SIZE);
	while (!spi2_tx_done) {
		__WFI();
	}
	spi2_tx_done = 0;

	LL_GPIO_ResetOutputPin(RF_WAKEUP_GPIO_Port, RF_WAKEUP_Pin);

	deinit_spi2();
	deinit_rf_pins();
}

#ifdef RX_TESTER
void create_sequential_packet() {
	readoutBuffer[0] = packet_counter;
	for (uint16_t i = 1; i < NUM_SENSORS; i++) {
		readoutBuffer[i] = i;
	}
	packet_counter++;
}
#endif

static void SPI1_start_rx_dma() {
	LL_DMA_ClearFlag_GI2(DMA1);

	// Configure RX
	LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_2, RX_BUFFER_SIZE);

	// Configure TX
	LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_3, RX_BUFFER_SIZE);

    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_2);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_3);

	// CS Falling edge and then start SPI receive
	LL_GPIO_ResetOutputPin(SPI1_CS_GPIO_Port, SPI1_CS_Pin);
    LL_SPI_Enable(SPI1);

	// Disable current TIA
	LL_GPIO_ResetOutputPin(TIA_ENBL_GPIO_Port, TIA_ENBL_Pin);
}


void SPI1_RX_DMA_Callback() {
	if (LL_DMA_IsActiveFlag_TC2(DMA1)) {
		LL_GPIO_SetOutputPin(SPI1_CS_GPIO_Port, SPI1_CS_Pin); // Set CS

		set_mux(addr + 1);

		LL_GPIO_SetOutputPin(TIA_ENBL_GPIO_Port, TIA_ENBL_Pin); // Set TIA ENABLE

		LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_2);
		LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_3);

		LL_DMA_ClearFlag_TC2(DMA1);

		LL_SPI_Disable(SPI1);

		spi1_rx_done = 1;
	} else {
		Error_Handler();
	}
}


static void SPI2_start_tx_dma(uint8_t *tx_buf, uint16_t len) {
	LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_5);

	LL_DMA_ClearFlag_GI5(DMA1);

	// Configure TX
	LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_5, LL_SPI_DMA_GetRegAddr(SPI2));
	LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_5, (uint32_t) tx_buf);
	LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_5, len);

	// CS low
	LL_GPIO_ResetOutputPin(SPI2_CS_GPIO_Port, SPI2_CS_Pin);

    LL_SPI_EnableDMAReq_TX(SPI2);

    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_5);

    LL_SPI_Enable(SPI2);
}

void SPI2_TX_DMA_Callback() {
	LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_5);

	LL_DMA_ClearFlag_TC5(DMA1);

	LL_SPI_Disable(SPI2);

	LL_SPI_DisableDMAReq_TX(SPI2);

	// CS high
	LL_GPIO_SetOutputPin(SPI2_CS_GPIO_Port, SPI2_CS_Pin);

	spi2_tx_done = 1;
}

void start_frame_timer(void) {
    LL_LPTIM_ClearFlag_ARRM(LPTIM1);
    LL_LPTIM_EnableIT_ARRM(LPTIM1);

    LL_LPTIM_Enable(LPTIM1);
    LL_LPTIM_SetAutoReload(LPTIM1, FRAME_TIMER_PERIOD);

    LL_LPTIM_StartCounter(LPTIM1, LL_LPTIM_OPERATING_MODE_CONTINUOUS);
}

void start_settling_time_timer() {
    LL_TIM_SetCounter(TIM6, 0);
    LL_TIM_ClearFlag_UPDATE(TIM6);
    LL_TIM_EnableIT_UPDATE(TIM6);
    LL_TIM_EnableCounter(TIM6);
}

void enter_stop_mode() {
	LL_PWR_SetPowerMode(LL_PWR_MODE_STOP);
	LL_LPM_EnableDeepSleep();
	LL_PWR_ClearFlag_WU();
	__WFI();

	// clear deep sleep bit after waking up
	LL_LPM_EnableSleep();
}

static void deinit_spi1() {
    // Set SPI1 pins to analog
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_12, LL_GPIO_MODE_ANALOG); // MOSI
    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_3,  LL_GPIO_MODE_ANALOG); // SCK
    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_4,  LL_GPIO_MODE_ANALOG); // MISO
    LL_GPIO_SetPinMode(SPI1_CS_GPIO_Port, SPI1_CS_Pin, LL_GPIO_MODE_ANALOG); // CS

    LL_APB2_GRP1_DisableClock(LL_APB2_GRP1_PERIPH_SPI1);
}

static void reinit_spi1() {
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);

    // Restore SPI1 pins
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_12, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_3,  LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_4,  LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinMode(SPI1_CS_GPIO_Port, SPI1_CS_Pin, LL_GPIO_MODE_OUTPUT);
}

static void deinit_spi2() {
    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_13, LL_GPIO_MODE_ANALOG); // SCK
    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_15, LL_GPIO_MODE_ANALOG); // MOSI
    LL_GPIO_SetPinMode(SPI2_CS_GPIO_Port, SPI2_CS_Pin, LL_GPIO_MODE_ANALOG); // CS
    LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_SPI2);
}

static void reinit_spi2() {
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_SPI2);
    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_13, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_15, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinMode(SPI2_CS_GPIO_Port, SPI2_CS_Pin, LL_GPIO_MODE_OUTPUT);
}


static void deinit_readout_pins() {
    // Set mux pins to analog
    LL_GPIO_SetPinMode(CTR_A0_GPIO_Port, CTR_A0_Pin, LL_GPIO_MODE_ANALOG);
    LL_GPIO_SetPinMode(CTR_A1_GPIO_Port, CTR_A1_Pin, LL_GPIO_MODE_ANALOG);
    LL_GPIO_SetPinMode(CTR_A2_GPIO_Port, CTR_A2_Pin, LL_GPIO_MODE_ANALOG);
    LL_GPIO_SetPinMode(CTR_A3_GPIO_Port, CTR_A3_Pin, LL_GPIO_MODE_ANALOG);
    LL_GPIO_SetPinMode(CTR_A4_GPIO_Port, CTR_A4_Pin, LL_GPIO_MODE_ANALOG);
    LL_GPIO_SetPinMode(CTR_A5_GPIO_Port, CTR_A5_Pin, LL_GPIO_MODE_ANALOG);
    LL_GPIO_SetPinMode(CTR_A6_GPIO_Port, CTR_A6_Pin, LL_GPIO_MODE_ANALOG);
    LL_GPIO_SetPinMode(CTR_A7_GPIO_Port, CTR_A7_Pin, LL_GPIO_MODE_ANALOG);

    LL_GPIO_SetPinMode(TIA_ENBL_GPIO_Port, TIA_ENBL_Pin, LL_GPIO_MODE_ANALOG);
    LL_GPIO_SetPinMode(SPI1_CS_GPIO_Port, SPI1_CS_Pin, LL_GPIO_MODE_ANALOG);
}

static void reinit_readout_pins() {
    // Restore mux pins
    LL_GPIO_SetPinMode(CTR_A0_GPIO_Port, CTR_A0_Pin, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinMode(CTR_A1_GPIO_Port, CTR_A1_Pin, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinMode(CTR_A2_GPIO_Port, CTR_A2_Pin, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinMode(CTR_A3_GPIO_Port, CTR_A3_Pin, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinMode(CTR_A4_GPIO_Port, CTR_A4_Pin, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinMode(CTR_A5_GPIO_Port, CTR_A5_Pin, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinMode(CTR_A6_GPIO_Port, CTR_A6_Pin, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinMode(CTR_A7_GPIO_Port, CTR_A7_Pin, LL_GPIO_MODE_OUTPUT);

    LL_GPIO_SetPinMode(TIA_ENBL_GPIO_Port, TIA_ENBL_Pin, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinMode(SPI1_CS_GPIO_Port, SPI1_CS_Pin, LL_GPIO_MODE_OUTPUT);
}

static void deinit_rf_pins() {
    // RF pins
    LL_GPIO_SetPinMode(RF_WAKEUP_GPIO_Port, RF_WAKEUP_Pin, LL_GPIO_MODE_ANALOG);
	LL_GPIO_SetPinMode(RF_SPI_READY_GPIO_Port, RF_SPI_READY_Pin, LL_GPIO_MODE_ANALOG);
    // Do not modify RF_RESET
}

static void reinit_rf_pins() {
    // RF pins
    LL_GPIO_SetPinMode(RF_WAKEUP_GPIO_Port, RF_WAKEUP_Pin, LL_GPIO_MODE_OUTPUT);
	LL_GPIO_SetPinMode(RF_SPI_READY_GPIO_Port, RF_SPI_READY_Pin, LL_GPIO_MODE_INPUT);
    // Do not modify RF_RESET
}

static void deinit_peripherals() {
    // Disable clocks
    LL_AHB1_GRP1_DisableClock(LL_AHB1_GRP1_PERIPH_DMA1);
    LL_IOP_GRP1_DisableClock(LL_IOP_GRP1_PERIPH_GPIOA);
    LL_IOP_GRP1_DisableClock(LL_IOP_GRP1_PERIPH_GPIOB);
    LL_IOP_GRP1_DisableClock(LL_IOP_GRP1_PERIPH_GPIOC);
    LL_IOP_GRP1_DisableClock(LL_IOP_GRP1_PERIPH_GPIOH);
}

static void reinit_peripherals() {
    // Re-enable clocks
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOC);
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOH);
}

static void deinit_tim6() {
	LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_TIM6);
}

static void reinit_tim6() {
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM6);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
