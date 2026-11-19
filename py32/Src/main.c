/**
  ******************************************************************************
  * @file    main.c
  * @author  MCU Application Team
  * @brief   Main program body
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
#include <string.h>

/* Private define ------------------------------------------------------------*/
/*
 * Configuration is stored in the last 1KB of the 64KB flash (PY32F003x8,
 * which is what the Keil project and the 8KB SRAM build target).  The old
 * address 0x08008000 sat in the middle of the code area; a larger image could
 * have erased part of the program.
 */
#define FLASH_USER_START_ADDR     0x0800FC00U

/*
 * Button is wired to the I2C SDA pin on this board.
 *   short press : auto -> NTSC(60Hz) -> PAL(50Hz) -> auto
 *   long  press : DVI <-> HDMI output mode
 */
#define Button_GPIO_Port          GPIOA
#define Button_GPIO_Pin           GPIO_PIN_13

#define DEFAULT_OUTPUT_MODE       1U   /* 0 = DVI, 1 = HDMI */
#define DEFAULT_TIMING_MODE       0U   /* 0 = auto, 1 = force 800x480@60, 2 = force 800x480@50 */

/* Private variables ---------------------------------------------------------*/
GPIO_InitTypeDef button_init = {
    .Pin  = Button_GPIO_Pin,
    .Mode = GPIO_MODE_INPUT,
    .Pull = GPIO_PULLUP
};

static uint32_t cfg[2];                                        /* [0]=DVI/HDMI, [1]=timing mode */
static uint32_t cfg_page[FLASH_PAGE_SIZE / 4];                 /* full 128-byte page for HAL */

/* Private function prototypes -----------------------------------------------*/
static void APP_SystemClockConfig(void);
static void sil9022_init(uint8_t out_mode, uint8_t timing_mode);
static void APP_FlashErase(void);
static void APP_FlashProgramCfg(void);
static void APP_FlashLoadCfg(void);
static void save_config(void);
static void button_task(void);
static void sil9022_poll(void);

/**
  * @brief  Main program.
  * @retval int
  */
int main(void)
{
  /* Reset of all peripherals, Initializes the Systick */
  HAL_Init();
  APP_SystemClockConfig();

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* Load output mode / timing mode, repairing a blank or invalid page. */
  APP_FlashLoadCfg();

  /* SII9022A initialization */
  sil9022_init((uint8_t)cfg[0], (uint8_t)cfg[1]);

  while (1)
  {
    button_task();        /* optional user override */
    sil9022_poll();       /* HPD + AMT630A PAL/NTSC timing tracking */
  }
}

/**
  * @brief   Configure system clock
  * @param   None
  * @retval  None
  */
static void APP_SystemClockConfig(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /* Oscillator configuration */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;                                                    /* Enable HSI */
#if defined(RCC_HSIDIV_SUPPORT)
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;                                                    /* No HSI division */
#endif
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_24MHz;
  RCC_OscInitStruct.HSEState = RCC_HSE_OFF;                                                   /* Disable HSE */
  RCC_OscInitStruct.LSIState = RCC_LSI_OFF;                                                   /* Disable LSI */

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  /* Initialize CPU, AHB, and APB bus clocks */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSISYS;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    APP_ErrorHandler();
  }
}

/**
  * @brief  Initialize the SII9022A and start TMDS.
  * @param  out_mode    0 = DVI, 1 = HDMI (with AVI InfoFrame)
  * @param  timing_mode 0 = auto, 1 = 800x480@60, 2 = 800x480@50
  */
static void sil9022_init(uint8_t out_mode, uint8_t timing_mode)
{
  uint8_t i;
  uint8_t vmode;

  I2C_GPIO_Init();

  /* SII9022A needs a short time after power-on before TPI is accessible. */
  HAL_Delay(100);

  vmode = (timing_mode == 2U) ? HDMI_800_480_50 : HDMI_800_480_60;

  for (i = 0; i < 3U; i++)
  {
    siHdmiTx_VideoSel(vmode);
    if (siHdmiTx_TPI_Init())
    {
      break;
    }
    HAL_Delay(50);
  }

  siHdmiTx_Init(out_mode);
  siHdmiTx_PowerStateD0fromD2();

  HAL_Delay(100);

  /* Correct the TPI timing immediately if the transmitter can measure it. */
  if (timing_mode == 0U)
  {
    siHdmiTx_UpdateTimingFromInput();
  }

  /* Give SDA back to the button input. */
  HAL_GPIO_Init(Button_GPIO_Port, &button_init);
}

/**
  * @brief  Erase the configuration page.
  * @param  None
  * @retval None
  */
static void APP_FlashErase(void)
{
  uint32_t PAGEError = 0;
  FLASH_EraseInitTypeDef EraseInitStruct = {0};

  EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGEERASE;
  EraseInitStruct.PageAddress = FLASH_USER_START_ADDR;
  EraseInitStruct.NbPages     = 1;

  if (HAL_FLASHEx_Erase(&EraseInitStruct, &PAGEError) != HAL_OK)
  {
    /* Do not hang here; the old configuration is still usable. */
  }
}

/**
  * @brief  Program one full configuration page.
  * @param  None
  * @retval None
  *
  * HAL_FLASH_Program(FLASH_TYPEPROGRAM_PAGE, ...) reads a complete 128-byte
  * page from the supplied buffer.  The old code passed a 16-byte array, so it
  * programmed 112 bytes of unrelated RAM into flash.
  */
static void APP_FlashProgramCfg(void)
{
  (void)HAL_FLASH_Program(FLASH_TYPEPROGRAM_PAGE, FLASH_USER_START_ADDR, cfg_page);
}

/**
  * @brief  Load configuration, write defaults when the page is blank/invalid.
  * @param  None
  * @retval None
  */
static void APP_FlashLoadCfg(void)
{
  cfg[0] = HW32_REG(FLASH_USER_START_ADDR);
  cfg[1] = HW32_REG(FLASH_USER_START_ADDR + 4U);

  if ((cfg[0] > 1U) || (cfg[1] > 2U))
  {
    cfg[0] = DEFAULT_OUTPUT_MODE;
    cfg[1] = DEFAULT_TIMING_MODE;
    save_config();
  }
}

/**
  * @brief  Save configuration to flash.
  * @param  None
  * @retval None
  */
static void save_config(void)
{
  uint8_t i;

  for (i = 0; i < (FLASH_PAGE_SIZE / 4U); i++)
  {
    cfg_page[i] = 0xFFFFFFFFU;
  }
  cfg_page[0] = cfg[0];
  cfg_page[1] = cfg[1];

  HAL_FLASH_Unlock();
  APP_FlashErase();
  APP_FlashProgramCfg();
  HAL_FLASH_Lock();
}

/**
  * @brief  Debounced button handling.
  * @param  None
  * @retval None
  *
  * The original code used HAL_Delay(100000) (100 seconds!) and changed the
  * mode even for a very short press.
  */
static void button_task(void)
{
  static uint8_t  pressed = 0;
  static uint32_t press_tick = 0;
  GPIO_PinState level;

  level = HAL_GPIO_ReadPin(Button_GPIO_Port, Button_GPIO_Pin);

  if ((level == GPIO_PIN_RESET) && (pressed == 0U))
  {
    HAL_Delay(20);
    if (HAL_GPIO_ReadPin(Button_GPIO_Port, Button_GPIO_Pin) == GPIO_PIN_RESET)
    {
      pressed = 1;
      press_tick = HAL_GetTick();
    }
  }
  else if ((level == GPIO_PIN_SET) && (pressed != 0U))
  {
    uint32_t held = HAL_GetTick() - press_tick;
    pressed = 0;

    if (held >= 2000U)
    {
      /* Long press: DVI <-> HDMI */
      cfg[0] = (cfg[0] == 0U) ? 1U : 0U;
      save_config();
      sil9022_init((uint8_t)cfg[0], (uint8_t)cfg[1]);
    }
    else if (held >= 50U)
    {
      /* Short press: auto -> 60Hz -> 50Hz -> auto */
      cfg[1] = (cfg[1] + 1U) % 3U;
      save_config();
      sil9022_init((uint8_t)cfg[0], (uint8_t)cfg[1]);
    }
  }
}

/**
  * @brief  Poll SII9022A hot-plug status and AMT630A output timing.
  * @param  None
  * @retval None
  */
static void sil9022_poll(void)
{
  static uint32_t last_tick = 0;
  uint8_t stat;

  if ((HAL_GetTick() - last_tick) < 500U)
  {
    return;
  }
  last_tick = HAL_GetTick();

  /* Do not touch the I2C bus while the button pulls SDA low. */
  if (HAL_GPIO_ReadPin(Button_GPIO_Port, Button_GPIO_Pin) == GPIO_PIN_RESET)
  {
    return;
  }

  stat = ReadByteTPI(TPI_INTERRUPT_STATUS_REG);
  if ((stat != 0xFFU) && ((stat & (HOT_PLUG_EVENT | RX_SENSE_EVENT)) != 0U))
  {
    WriteByteTPI(TPI_INTERRUPT_STATUS_REG, stat);
    SetAVI_InfoFrames();
    siHdmiTx_PowerStateD0fromD2();
    HAL_Delay(20);
  }

  if (cfg[1] == 0U)
  {
    siHdmiTx_UpdateTimingFromInput();
  }
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void APP_ErrorHandler(void)
{
  /* infinite loop */
  while (1)
  {
  }
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     for example: printf("Wrong parameters value: file %s on line %d\r\n", file, line)  */
  /* infinite loop */
  while (1)
  {
  }
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
