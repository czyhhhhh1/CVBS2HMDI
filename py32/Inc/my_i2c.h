#pragma once

#include "main.h"
#include "py32f0xx_hal.h"

#define I2C3_SCL_Pin GPIO_PIN_14
#define I2C3_SDA_Pin GPIO_PIN_13
#define I2C3_SCL_GPIO_Port GPIOA
#define I2C3_SDA_GPIO_Port GPIOA

void I2C_GPIO_Init(void);
void I2C_Start(void);
void I2C_Stop(void);
HAL_StatusTypeDef I2C_Send_Byte(uint8_t byte);
uint8_t I2C_Read_Byte(GPIO_PinState ack);
// I2C API????
HAL_StatusTypeDef G_I2C_Mem_Write(uint8_t devAddr, uint8_t memAddr, uint8_t *pData, uint16_t size);
HAL_StatusTypeDef G_I2C_Mem_Read(uint8_t devAddr, uint8_t memAddr, uint8_t *pData, uint16_t size);




