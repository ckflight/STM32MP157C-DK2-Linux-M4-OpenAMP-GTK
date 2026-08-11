
#include "main.h"
#include "openamp.h"
#include "virt_uart.h"

VIRT_UART_HandleTypeDef huart0;
uint32_t counter = 0;

IPCC_HandleTypeDef hipcc;
I2C_HandleTypeDef hi2c5;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_IPCC_Init(void);
int MX_OPENAMP_Init(int RPMsgRole, rpmsg_ns_bind_cb ns_bind_cb);
static void MX_I2C5_Init(void);

// Important Note For data sharing from m4 to linux with opeanamp virtio:
// Enable ipcc and hsem and their interrupt at rcc.
// Also make project structure basic not advanced at mx

#define MPU6050_ADDR			(0x68 << 1)
#define MPU6050_PWR_MGMT_1    	0x6B
#define MPU6050_GYRO_CONFIG   	0x1B
#define MPU6050_ACCEL_CONFIG  	0x1C
#define MPU6050_ACCEL_XOUT_H    0x3B

typedef struct __attribute__((packed))
{
	uint16_t header; // to send header to linux with gyro acc data for sync

	int16_t ax;
	int16_t ay;
	int16_t az;

    int16_t gx;
    int16_t gy;
    int16_t gz;

} SensorPacket;

SensorPacket sensor_packet;

bool is_user2_button_pressed = false;

HAL_StatusTypeDef MPU6050_Init(void)
{
    uint8_t data;

    /* Wake MPU6050 */
    data = 0x00;
    if (HAL_I2C_Mem_Write(&hi2c5, MPU6050_ADDR, MPU6050_PWR_MGMT_1, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY) != HAL_OK) return HAL_ERROR;

    /* Gyroscope: ±2000 deg/s */
    data = 0x18;
    if (HAL_I2C_Mem_Write(&hi2c5, MPU6050_ADDR, MPU6050_GYRO_CONFIG, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY) != HAL_OK) return HAL_ERROR;

    /* Accelerometer: ±16 g */
    data = 0x18;
    if (HAL_I2C_Mem_Write(&hi2c5, MPU6050_ADDR, MPU6050_ACCEL_CONFIG, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY) != HAL_OK) return HAL_ERROR;

    return HAL_OK;
}

HAL_StatusTypeDef MPU6050_ReadAll(SensorPacket *p)
{
    uint8_t data[14];

    if (HAL_I2C_Mem_Read(&hi2c5, MPU6050_ADDR, MPU6050_ACCEL_XOUT_H, I2C_MEMADD_SIZE_8BIT, data, 14, HAL_MAX_DELAY) != HAL_OK) return HAL_ERROR;

    p->ax = (int16_t)((data[0]  << 8) | data[1]);
    p->ay = (int16_t)((data[2]  << 8) | data[3]);
    p->az = (int16_t)((data[4]  << 8) | data[5]);

    p->gx = (int16_t)((data[8]  << 8) | data[9]);
    p->gy = (int16_t)((data[10] << 8) | data[11]);
    p->gz = (int16_t)((data[12] << 8) | data[13]);

    return HAL_OK;
}

int main(void)
{

  HAL_Init();

  if(IS_ENGINEERING_BOOT_MODE())
  {
    /* Configure the system clock */
    SystemClock_Config();
  }
  else
  {
    /* IPCC initialisation */
    MX_IPCC_Init();
    /* OpenAmp initialisation ---------------------------------*/
    MX_OPENAMP_Init(RPMSG_REMOTE, NULL);
    VIRT_UART_Init(&huart0);

  }

  MX_GPIO_Init();
  MX_I2C5_Init();

  HAL_Delay(100);

  MPU6050_Init();

  sensor_packet.header = 0xA55A;

  is_user2_button_pressed = false;

  while (1)
  {
      OPENAMP_check_for_message();

      if(MPU6050_ReadAll(&sensor_packet) == HAL_OK)
      {
          VIRT_UART_Transmit(&huart0, (uint8_t *)&sensor_packet, sizeof(sensor_packet));
          HAL_GPIO_TogglePin(GPIOH, GPIO_PIN_7);
      }

      if (is_user2_button_pressed)
      {
    	  is_user2_button_pressed = false;
          HAL_GPIO_TogglePin(GPIOH, GPIO_PIN_7);
          HAL_Delay(100);

      }

      HAL_Delay(5);
  }

}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDivValue = RCC_HSI_DIV1;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL3.PLLSource = RCC_PLL3SOURCE_HSI;
  RCC_OscInitStruct.PLL3.PLLM = 4;
  RCC_OscInitStruct.PLL3.PLLN = 25;
  RCC_OscInitStruct.PLL3.PLLP = 2;
  RCC_OscInitStruct.PLL3.PLLQ = 2;
  RCC_OscInitStruct.PLL3.PLLR = 2;
  RCC_OscInitStruct.PLL3.PLLRGE = RCC_PLL3IFRANGE_1;
  RCC_OscInitStruct.PLL3.PLLFRACV = 0;
  RCC_OscInitStruct.PLL3.PLLMODE = RCC_PLL_INTEGER;
  RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** RCC Clock Config
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_ACLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3|RCC_CLOCKTYPE_PCLK4
                              |RCC_CLOCKTYPE_PCLK5;
  RCC_ClkInitStruct.AXISSInit.AXI_Clock = RCC_AXISSOURCE_HSI;
  RCC_ClkInitStruct.AXISSInit.AXI_Div = RCC_AXI_DIV1;
  RCC_ClkInitStruct.MCUInit.MCU_Clock = RCC_MCUSSOURCE_PLL3;
  RCC_ClkInitStruct.MCUInit.MCU_Div = RCC_MCU_DIV1;
  RCC_ClkInitStruct.APB4_Div = RCC_APB4_DIV1;
  RCC_ClkInitStruct.APB5_Div = RCC_APB5_DIV1;
  RCC_ClkInitStruct.APB1_Div = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2_Div = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB3_Div = RCC_APB3_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief IPCC Initialization Function
  * @param None
  * @retval None
  */
static void MX_IPCC_Init(void)
{

  /* USER CODE BEGIN IPCC_Init 0 */

  /* USER CODE END IPCC_Init 0 */

  /* USER CODE BEGIN IPCC_Init 1 */

  /* USER CODE END IPCC_Init 1 */
  hipcc.Instance = IPCC;
  if (HAL_IPCC_Init(&hipcc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN IPCC_Init 2 */

  /* USER CODE END IPCC_Init 2 */

}
/**
  * @brief I2C5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C5_Init(void)
{

  /* USER CODE BEGIN I2C5_Init 0 */

  /* USER CODE END I2C5_Init 0 */

  /* USER CODE BEGIN I2C5_Init 1 */

  /* USER CODE END I2C5_Init 1 */
  hi2c5.Instance = I2C5;
  hi2c5.Init.Timing = 0x009034B6;
  hi2c5.Init.OwnAddress1 = 0;
  hi2c5.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c5.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c5.Init.OwnAddress2 = 0;
  hi2c5.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c5.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c5.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c5) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c5, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c5, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C5_Init 2 */

  /* USER CODE END I2C5_Init 2 */

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
  __HAL_RCC_GPIOH_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin : PH7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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

