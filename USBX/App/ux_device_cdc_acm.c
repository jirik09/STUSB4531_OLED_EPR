/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ux_device_cdc_acm.c
  * @author  MCD Application Team
  * @brief   USBX Device applicative file
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
#include "ux_device_cdc_acm.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32c0xx_hal.h"  /* for HAL_GetTick */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* Pointer to the active CDC ACM class instance (set in Activate callback) */
UX_SLAVE_CLASS_CDC_ACM *cdc_acm_instance = UX_NULL;
volatile uint8_t        cdc_connected    = 0U;

/* Hello World one-shot TX state: 0 = idle, 1 = pending */
static volatile uint8_t s_hello_pending = 0U;
static UCHAR s_hello_buf[] = "Hello World!\r\n";

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  USBD_CDC_ACM_Activate
  *         This function is called when insertion of a CDC ACM device.
  * @param  cdc_acm_instance: Pointer to the cdc acm class instance.
  * @retval none
  */
VOID USBD_CDC_ACM_Activate(VOID *cdc_acm_instance_ptr)
{
  /* USER CODE BEGIN USBD_CDC_ACM_Activate */
  cdc_acm_instance = (UX_SLAVE_CLASS_CDC_ACM *)cdc_acm_instance_ptr;
  cdc_connected    = 1U;
  s_hello_pending  = 1U;  /* trigger "Hello World!" transmission */
  /* USER CODE END USBD_CDC_ACM_Activate */

  return;
}

/**
  * @brief  USBD_CDC_ACM_Deactivate
  *         This function is called when extraction of a CDC ACM device.
  * @param  cdc_acm_instance: Pointer to the cdc acm class instance.
  * @retval none
  */
VOID USBD_CDC_ACM_Deactivate(VOID *cdc_acm_instance_ptr)
{
  /* USER CODE BEGIN USBD_CDC_ACM_Deactivate */
  UX_PARAMETER_NOT_USED(cdc_acm_instance_ptr);
  cdc_acm_instance = UX_NULL;
  cdc_connected    = 0U;
  s_hello_pending  = 0U;
  /* USER CODE END USBD_CDC_ACM_Deactivate */

  return;
}

/**
  * @brief  USBD_CDC_ACM_ParameterChange
  *         This function is invoked to manage the CDC ACM class requests.
  * @param  cdc_acm_instance: Pointer to the cdc acm class instance.
  * @retval none
  */
VOID USBD_CDC_ACM_ParameterChange(VOID *cdc_acm_instance)
{
  /* USER CODE BEGIN USBD_CDC_ACM_ParameterChange */
  UX_PARAMETER_NOT_USED(cdc_acm_instance);
  /* USER CODE END USBD_CDC_ACM_ParameterChange */

  return;
}

/* USER CODE BEGIN 1 */

/**
  * @brief  CDC_Transmit_FS
  *         Transmit data over USB CDC (blocking with 1 s timeout).
  * @param  data: Buffer of data to send
  * @param  size: Number of bytes
  * @retval UX_SUCCESS or UX_ERROR
  */
UINT CDC_Transmit_FS(uint8_t *data, uint16_t size)
{
    ULONG  actual_length;
    UINT   status;
    uint32_t deadline;

    if (cdc_acm_instance == UX_NULL || size == 0U)
        return UX_ERROR;

    deadline = HAL_GetTick() + 1000U;
    do
    {
        ux_system_tasks_run();  /* keep standalone state machine moving */
        status = ux_device_class_cdc_acm_write_run(
                     cdc_acm_instance, (UCHAR *)data, (ULONG)size, &actual_length);
    } while ((status == UX_STATE_WAIT) && (HAL_GetTick() < deadline));

    return ((status == UX_STATE_NEXT) || (status == UX_STATE_IDLE)) ? UX_SUCCESS : UX_ERROR;
}

/**
  * @brief  CDC_Tasks_Run
  *         Drive the one-shot "Hello World!" transmit state machine.
  *         Must be called from the main loop after ux_system_tasks_run().
  */
VOID CDC_Tasks_Run(VOID)
{
    ULONG actual;
    UINT  status;

    if (s_hello_pending == 0U || cdc_acm_instance == UX_NULL)
        return;

    status = ux_device_class_cdc_acm_write_run(
                 cdc_acm_instance, s_hello_buf,
                 sizeof(s_hello_buf) - 1U, &actual);

    if (status != UX_STATE_WAIT)
        s_hello_pending = 0U;   /* done (success or error, stop retrying) */
}

/* USER CODE END 1 */
