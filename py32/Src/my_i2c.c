#include "my_i2c.h"

// ?? I2C ACK ? NACK
#define I2C_ACK GPIO_PIN_RESET
#define I2C_NACK GPIO_PIN_SET

GPIO_InitTypeDef GPIO_InitStruct2 = {
    .Pin = I2C3_SDA_Pin,
    .Mode = GPIO_MODE_OUTPUT_OD,
    .Pull = GPIO_NOPULL,
    .Speed = GPIO_SPEED_FREQ_VERY_HIGH};
GPIO_InitTypeDef GPIO_InitStruct3 = {
	  .Pin = I2C3_SDA_Pin,
    .Mode = GPIO_MODE_INPUT,
    .Pull = GPIO_NOPULL
};

static inline void I2C_Delay(void);
static void I2C_BusRecovery(void);


void I2C_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // ???SCL??
    GPIO_InitStruct.Pin = I2C3_SCL_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(I2C3_SCL_GPIO_Port, &GPIO_InitStruct);

    // ???SDA??
    GPIO_InitStruct.Pin = I2C3_SDA_Pin;
    HAL_GPIO_Init(I2C3_SDA_GPIO_Port, &GPIO_InitStruct);
		
		GPIO_InitTypeDef GPIO_InitStruct1 = {0};
		GPIO_InitStruct1.Pin = GPIO_PIN_10;
    GPIO_InitStruct1.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct1);
		
		GPIO_InitStruct1.Pin = GPIO_PIN_6;
    GPIO_InitStruct1.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct1);

    // ?SCL?SDA??
    HAL_GPIO_WritePin(I2C3_SCL_GPIO_Port, I2C3_SCL_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(I2C3_SDA_GPIO_Port, I2C3_SDA_Pin, GPIO_PIN_SET);

    /* Release a bus stuck low by a previous reset/power-down. */
    I2C_BusRecovery();
}

//????
static inline void I2C_Set_SDA_Output(void)
{
    HAL_GPIO_Init(I2C3_SDA_GPIO_Port, &GPIO_InitStruct2);
}

// ????
static inline void I2C_Set_SDA_Input(void)
{
    HAL_GPIO_Init(I2C3_SDA_GPIO_Port, &GPIO_InitStruct3);
}

// ??SCL???
static  void I2C_Set_SCL(GPIO_PinState state)
{
    HAL_GPIO_WritePin(I2C3_SCL_GPIO_Port, I2C3_SCL_Pin, state);
}

// ??SDA???
static inline void I2C_Set_SDA(GPIO_PinState state)
{
    /*
     * Set the output latch first, then switch the pin to output.  The old
     * order re-initialised the pin (driving the previous ODR value) before
     * writing the new level, which produced a short wrong level on SDA at
     * every bit when the bus was running at 100kHz+.
     */
    HAL_GPIO_WritePin(I2C3_SDA_GPIO_Port, I2C3_SDA_Pin, state);
    I2C_Set_SDA_Output();
}

// ??SDA???
static inline GPIO_PinState I2C_Read_SDA(void)
{
    I2C_Set_SDA_Input();  // ???????
    return HAL_GPIO_ReadPin(I2C3_SDA_GPIO_Port, I2C3_SDA_Pin);
}

/*
 * I2C half-bit delay.
 *
 * ~5us at 24MHz with the default 20 iterations  ->  about 100kHz SCL.
 * Override I2C_DELAY_LOOPS in the compiler defines if the board's pull-ups
 * are weak (increase) or if a faster bus is needed (decrease).
 *
 * The original implementation used HAL_Delay(1), i.e. 2ms per I2C bit.
 * A full SII9022A register write then took about 20ms and the complete
 * init took longer than 10 seconds (there is also a 10s delay inside
 * StartTPI).  Many HDMI sinks had already timed out before TMDS started.
 */
#ifndef I2C_DELAY_LOOPS
#define I2C_DELAY_LOOPS     20U
#endif

static inline void I2C_Delay(void)
{
    for (volatile uint8_t i = 0; i < I2C_DELAY_LOOPS; i++)
    {
        /* volatile loop counter is enough to keep the delay */
    }
}

/*
 * Clock SCL nine times with SDA released, then issue a STOP.  This is the
 * standard recovery for a slave that was reset in the middle of a read and
 * is still holding SDA low.
 */
static void I2C_BusRecovery(void)
{
    uint8_t i;

    HAL_GPIO_WritePin(I2C3_SDA_GPIO_Port, I2C3_SDA_Pin, GPIO_PIN_SET);

    for (i = 0; i < 9U; i++)
    {
        HAL_GPIO_WritePin(I2C3_SCL_GPIO_Port, I2C3_SCL_Pin, GPIO_PIN_RESET);
        I2C_Delay();
        HAL_GPIO_WritePin(I2C3_SCL_GPIO_Port, I2C3_SCL_Pin, GPIO_PIN_SET);
        I2C_Delay();
    }

    /* STOP condition */
    HAL_GPIO_WritePin(I2C3_SDA_GPIO_Port, I2C3_SDA_Pin, GPIO_PIN_RESET);
    I2C_Delay();
    HAL_GPIO_WritePin(I2C3_SCL_GPIO_Port, I2C3_SCL_Pin, GPIO_PIN_SET);
    I2C_Delay();
    HAL_GPIO_WritePin(I2C3_SDA_GPIO_Port, I2C3_SDA_Pin, GPIO_PIN_SET);
    I2C_Delay();
}

void I2C_Start(void)
{
    I2C_Set_SDA(GPIO_PIN_SET);
    I2C_Delay();                    /* SDA high before SCL high */
    I2C_Set_SCL(GPIO_PIN_SET);
    I2C_Delay();
    I2C_Set_SDA(GPIO_PIN_RESET);
    I2C_Delay();                    /* START setup time */
    I2C_Set_SCL(GPIO_PIN_RESET);
    I2C_Delay();
}

void I2C_Stop(void)
{
    I2C_Set_SDA(GPIO_PIN_RESET);
    I2C_Delay();                    /* SDA setup before SCL high */
    I2C_Set_SCL(GPIO_PIN_SET);
    I2C_Delay();
    I2C_Set_SDA(GPIO_PIN_SET);
    I2C_Delay();                    /* STOP setup time */
}

// ??????,??ACK??
HAL_StatusTypeDef I2C_Send_Byte(uint8_t byte)
{
	  I2C_Set_SCL(GPIO_PIN_RESET);
    I2C_Delay();
    for (int8_t i = 7; i >= 0; i--)
    {
        GPIO_PinState bit = (byte & (1 << i)) ? GPIO_PIN_SET : GPIO_PIN_RESET;
        I2C_Set_SDA(bit);
        I2C_Delay();
        I2C_Set_SCL(GPIO_PIN_SET);
        I2C_Delay();
        I2C_Set_SCL(GPIO_PIN_RESET);
        I2C_Delay();
    }

    // ??ACK
    I2C_Set_SDA_Input();  // ??SDA?????
    I2C_Delay();
    I2C_Set_SCL(GPIO_PIN_SET);
    I2C_Delay();
    GPIO_PinState ack = I2C_Read_SDA();  // ??ACK??
    I2C_Set_SCL(GPIO_PIN_RESET);
    I2C_Delay();

    return (ack == GPIO_PIN_RESET) ? HAL_OK : HAL_ERROR;
}

// ??????,???ACK?NACK
uint8_t I2C_Read_Byte(GPIO_PinState ack)
{
    uint8_t byte = 0;

    I2C_Set_SDA_Input();  // ??SDA?????
    I2C_Delay();

    for (int8_t i = 7; i >= 0; i--)
    {
        I2C_Set_SCL(GPIO_PIN_SET);
        I2C_Delay();
        if (I2C_Read_SDA() == GPIO_PIN_SET)
        {
            byte |= (1 << i);
        }
        I2C_Set_SCL(GPIO_PIN_RESET);
        I2C_Delay();
    }

    // ??ACK?NACK
    I2C_Set_SDA(ack);  // ??SDA?????,???ACK?NACK
    I2C_Delay();
    I2C_Set_SCL(GPIO_PIN_SET);
    I2C_Delay();
    I2C_Set_SCL(GPIO_PIN_RESET);
    I2C_Delay();

    return byte;
}

// ??I2C????,??HAL_I2C_Mem_Write
HAL_StatusTypeDef G_I2C_Mem_Write(uint8_t devAddr, uint8_t memAddr, uint8_t *pData, uint16_t size)
{
    I2C_Start();

    if (I2C_Send_Byte(devAddr << 1) != HAL_OK)
    {
        I2C_Stop();
        return HAL_ERROR;
    }

    if (I2C_Send_Byte(memAddr) != HAL_OK)
    {
        I2C_Stop();
        return HAL_ERROR;
    }

    for (uint16_t i = 0; i < size; i++)
    {
        if (I2C_Send_Byte(pData[i]) != HAL_OK)
        {
            I2C_Stop();
            return HAL_ERROR;
        }
    }

    I2C_Stop();
    return HAL_OK;
}

// ??I2C????,??HAL_I2C_Mem_Read
HAL_StatusTypeDef G_I2C_Mem_Read(uint8_t devAddr, uint8_t memAddr, uint8_t *pData, uint16_t size)
{
    I2C_Start();

    if (I2C_Send_Byte(devAddr << 1) != HAL_OK)
    {
        I2C_Stop();
        return HAL_ERROR;
    }

    if (I2C_Send_Byte(memAddr) != HAL_OK)
    {
        I2C_Stop();
        return HAL_ERROR;
    }

    I2C_Start();  // ??????

    if (I2C_Send_Byte((devAddr << 1) | 0x01) != HAL_OK)
    {
        I2C_Stop();
        return HAL_ERROR;
    }

    for (uint16_t i = 0; i < size; i++)
    {
        pData[i] = I2C_Read_Byte((i == size - 1) ? I2C_NACK : I2C_ACK);
    }

    I2C_Stop();
    return HAL_OK;
}

