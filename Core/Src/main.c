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
#include "ssd1306_fonts.h"		// i2c1 - 0x3C  - prohodit i2c1 s i2c2
#include <stdio.h>
#include "ssd1306.h"
#include "stusb4531_api.h"
#include "oled_app.h"
#include "uart_app.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MAX_DISPLAY_PDOS  2  // Maximum number of PDOs to display on OLED (32px height)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart2;

PCD_HandleTypeDef hpcd_USB_DRD_FS;

/* USER CODE BEGIN PV */
STUSB4531_Status_t stusb_status = {0};
volatile uint8_t stusb_alert_flag = 0;  // Flag set by interrupt
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USB_PCD_Init(void);
static void MX_ADC1_Init(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// UART printf support
int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
}

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
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  MX_USB_PCD_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */

  // Initialize UART debug output
  UART_App_Init();

  OLED_App_Init();

  uint8_t i;
  for(i = 0; i < 10; i++) // Short delay before starting (for debugging)
  {
    ssd1306_FillCircle(i*10+10, 30, 1, 0x01);
    ssd1306_UpdateScreen();
    HAL_Delay(100);
  }
  
  // Initialize STUSB4531
  printf("STUSB4531: Initializing...\r\n");
  
  HAL_StatusTypeDef init_status = STUSB4531_Init(&hi2c1);
  if (init_status == HAL_OK)
  {
      stusb_status.initialized = true;
      printf("STUSB4531: Initialization OK\r\n");
      
      // Read and display device ID
      uint8_t device_id = 0xFF;
      if (STUSB4531_ReadDeviceID(&hi2c1, &device_id) == HAL_OK)
      {
          printf("STUSB4531: Device ID = 0x%02X\r\n", device_id);
      }
      
      // Read initial alert status
      uint8_t initial_alert;
      if (STUSB4531_ReadAlertStatus(&hi2c1, &initial_alert) == HAL_OK)
      {
          printf("STUSB4531: Initial Alert Status = 0x%02X\r\n", initial_alert);
          if (initial_alert != 0)
          {
              printf("  Clearing initial alerts...\r\n");
              HAL_StatusTypeDef clear_status = STUSB4531_ClearAlerts(&hi2c1, initial_alert);
              if (clear_status == HAL_OK)
              {
                  // Wait a bit and read again to verify
                  HAL_Delay(10);
                  uint8_t verify_alert;
                  if (STUSB4531_ReadAlertStatus(&hi2c1, &verify_alert) == HAL_OK)
                  {
                      printf("  Alert status after clear: 0x%02X\r\n", verify_alert);
                      if (verify_alert != 0)
                      {
                          printf("  WARNING: Alerts persist after clearing\r\n");
                      }
                  }
              }
              else
              {
                  printf("  ERROR: Failed to clear alerts (status=%d)\r\n", clear_status);
              }
          }
      }
  }
  else
  {
      printf("STUSB4531: Initialization FAILED (status=%d)\r\n", init_status);
  }
  

  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  GPIO_PinState last_sink_en = GPIO_PIN_SET;
  
  while (1)
  {
	  // Update LED (PA0) to mirror Sink_EN (PB4) - LED is active low
	  GPIO_PinState sink_en_state = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4);
	  if (sink_en_state != last_sink_en)
	  {
		  // LED is active low, so invert the Sink_EN state
		  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, sink_en_state);
		  last_sink_en = sink_en_state;
		  printf("Sink_EN changed: %s (LED: %s)\r\n", 
		         sink_en_state == GPIO_PIN_RESET ? "LOW" : "HIGH",
		         sink_en_state == GPIO_PIN_RESET ? "ON" : "OFF");
	  }
	  
	  // Check if alert interrupt occurred
	  if (stusb_alert_flag)
	  {
	      stusb_alert_flag = 0;  // Clear flag
	      
	      // Read and display alert status
	      uint8_t alert_status;
	      if (STUSB4531_ReadAlertStatus(&hi2c1, &alert_status) == HAL_OK)
	      {
	          UART_App_PrintAlert(alert_status);
	          
	          // Clear the alerts
	          STUSB4531_ClearAlerts(&hi2c1, alert_status);
	      }
	  }
	  
	  // Check STUSB4531 status
	  HAL_StatusTypeDef status_result = STUSB4531_CheckStatus(&hi2c1, &stusb_status);
	  
	  // Read comprehensive status
	  static uint8_t last_cc_status = 0xFF;
	  static uint8_t last_typec_fsm = 0xFF;
	  static uint8_t last_pe_fsm = 0xFF;
	  STUSB4531_ComprehensiveStatus_t comp_status;
	  
	  if (STUSB4531_ReadComprehensiveStatus(&hi2c1, &comp_status) == HAL_OK)
	  {
	      // Print status if anything changed
	      if (comp_status.cc_status != last_cc_status || 
	          comp_status.typec_fsm != last_typec_fsm ||
	          comp_status.pe_fsm != last_pe_fsm ||
	          comp_status.alert_status != 0)
	      {
	          UART_App_PrintComprehensiveStatus(&comp_status);
	          
	          last_cc_status = comp_status.cc_status;
	          last_typec_fsm = comp_status.typec_fsm;
	          last_pe_fsm = comp_status.pe_fsm;
	      }
	  }
	  else
	  {
	      printf("ERROR: Failed to read comprehensive status (I2C error)\r\n");
	  }
	  
	  // If PD capable, request and read source capabilities
	  if (stusb_status.attached && stusb_status.pd_capable)
	  {
	      // Request source capabilities
	      printf("Requesting source capabilities...\r\n");
	      STUSB4531_GetSourceCapabilities(&hi2c1);
	      HAL_Delay(100); // Wait for response
	      
	      // Read the PDOs
	      if (STUSB4531_ReadSourcePDOs(&hi2c1, &stusb_status) == HAL_OK)
	      {
	          // Select highest power PDO
	          uint8_t selected = STUSB4531_SelectHighestPowerPDO(&stusb_status);
	          
	          // Print PDO information
	          static uint8_t last_num_pdos = 0;
	          if (stusb_status.num_pdos != last_num_pdos)
	          {
	              UART_App_PrintPDOs(&stusb_status);
	              last_num_pdos = stusb_status.num_pdos;
	          }
	      }
	      else
	      {
	          printf("Failed to read PDOs\r\n");
	      }
	  }
	  
	  OLED_App_Update(&stusb_status);
	  
	  HAL_Delay(500); // Short delay for loop (was 2000ms)
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
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSIUSB48;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_SEQ_FIXED;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.LowPowerAutoPowerOff = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_1CYCLE_5;
  hadc1.Init.OversamplingMode = DISABLE;
  hadc1.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x10805D88;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief USB Initialization Function
  * @param None
  * @retval None
  */
static void MX_USB_PCD_Init(void)
{

  /* USER CODE BEGIN USB_Init 0 */

  /* USER CODE END USB_Init 0 */

  /* USER CODE BEGIN USB_Init 1 */

  /* USER CODE END USB_Init 1 */
  hpcd_USB_DRD_FS.Instance = USB_DRD_FS;
  hpcd_USB_DRD_FS.Init.dev_endpoints = 8;
  hpcd_USB_DRD_FS.Init.speed = USBD_FS_SPEED;
  hpcd_USB_DRD_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_DRD_FS.Init.Sof_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.battery_charging_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.vbus_sensing_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.bulk_doublebuffer_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.iso_singlebuffer_enable = DISABLE;
  if (HAL_PCD_Init(&hpcd_USB_DRD_FS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_Init 2 */

  /* USER CODE END USB_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA0 (LED - mirrors Sink_EN, active low) */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);  // LED off initially (active low)

  /*Configure GPIO pin PB4 (SINK_EN) with pull-up */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin PB5 (ALERT) with pull-up and interrupt on falling edge */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init for Alert pin */
  HAL_NVIC_SetPriority(EXTI4_15_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/**
 * @brief EXTI line detection callback for STUSB4531 alert
 */
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_5)  // ALERT pin
    {
        stusb_alert_flag = 1;  // Set flag to handle in main loop
    }
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
