/**
  ******************************************************************************
  * @file    py32f0xx_hal_msp.c
  * @author  MCU Application Team
  * @brief   This file provides code for the MSP Initialization
  *          and de-Initialization codes.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2023 Puya Semiconductor Co.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by Puya under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* External functions --------------------------------------------------------*/

/**
  * @brief Initialize global MSP
  */
void HAL_MspInit(void)
{
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();
}

/**
  * @brief Initialize I2C MSP
  */
void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
  GPIO_InitTypeDef GPIO_InitStruct[4] = {0};

  __HAL_RCC_I2C_CLK_ENABLE();                                 /* Enable I2C clock */
  __HAL_RCC_GPIOA_CLK_ENABLE();                               /* Enable GPIOA clock */
	__HAL_RCC_GPIOB_CLK_ENABLE();																/* Enable GPIOB clock */

  /**I2C pin Configuration
  PA3     ------> I2C1_SCL
  PA2     ------> I2C1_SDA*/
	/*GPIO_InitStruct[0].Pin = GPIO_PIN_2;
  GPIO_InitStruct[0].Mode = GPIO_MODE_AF_OD;                  
  GPIO_InitStruct[0].Pull = GPIO_PULLUP;                     
  GPIO_InitStruct[0].Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct[0].Alternate = GPIO_AF12_I2C;                  
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct[0]);                
	
	GPIO_InitStruct[1].Pin = GPIO_PIN_3;
  GPIO_InitStruct[1].Mode = GPIO_MODE_AF_OD;                  
  GPIO_InitStruct[1].Pull = GPIO_PULLUP;                      
  GPIO_InitStruct[1].Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct[1].Alternate = GPIO_AF12_I2C;                 
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct[1]);*/

  GPIO_InitStruct[0].Pin = GPIO_PIN_10;
  GPIO_InitStruct[0].Mode = GPIO_MODE_AF_OD;                  
  GPIO_InitStruct[0].Pull = GPIO_PULLUP;                     
  GPIO_InitStruct[0].Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct[0].Alternate = GPIO_AF12_I2C;                  
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct[0]);                
	                 
	
	GPIO_InitStruct[1].Pin = GPIO_PIN_6;
  GPIO_InitStruct[1].Mode = GPIO_MODE_AF_OD;                    
  GPIO_InitStruct[1].Pull = GPIO_PULLUP;                       
  GPIO_InitStruct[1].Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct[1].Alternate = GPIO_AF6_I2C; 
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct[1]);
	
	GPIO_InitStruct[2].Pin = GPIO_PIN_13;
  GPIO_InitStruct[2].Mode = GPIO_MODE_ANALOG;                  
  //GPIO_InitStruct[2].Pull = GPIO_PULLUP;                      
  //GPIO_InitStruct[2].Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  //GPIO_InitStruct[2].Alternate = NULL;                 
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct[2]);                  
	
	GPIO_InitStruct[3].Pin = GPIO_PIN_14;
  GPIO_InitStruct[3].Mode = GPIO_MODE_ANALOG;                 
  //GPIO_InitStruct[3].Pull = GPIO_PULLUP;                       
  //GPIO_InitStruct[3].Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  //GPIO_InitStruct[3].Alternate = NULL;                   
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct[3]);
  /*Reset I2C*/
  __HAL_RCC_I2C_FORCE_RESET();
  __HAL_RCC_I2C_RELEASE_RESET();
}

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
