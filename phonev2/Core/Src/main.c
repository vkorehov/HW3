/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
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
#include <string.h>
#include <stdio.h>

#include "main.h"
#include "dac.h"
#include "dma.h"
#include "rtc.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

void RingReset (void);
extern TIM_HandleTypeDef htim6;
extern RTC_HandleTypeDef hrtc;

extern uint32_t sine_wave;
extern const uint16_t loop[13050];

void UARTReset(void);
void UARTSend(char* data);
HAL_StatusTypeDef UARTWaitOK(void);

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

void SIM800_PowerUP(void) {
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET); // # PWR Key
  HAL_Delay(1050); //1050mS
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET); // # PWR Key  
}

void DAC_Disable(void);
void DAC_Enable(void);

uint8_t SIM800_TestSound(void) {
  // turn ON Audio
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);
  // tie to GND DAC  
  DAC_Disable();

  UARTReset();  
  UARTSend("AT+CMICBIAS=1\r\n"); // 1 turn On bias, 2 turn Off bias
  if (UARTWaitOK() != HAL_OK) {
    // failed to adjust mic level
    return 1;
  }  
  
  UARTReset();  
  UARTSend("AT+CMIC=0,15\r\n");// 0 == 0db, 15 == +22.5dB
  if (UARTWaitOK() != HAL_OK) {
    // failed to adjust mic level
    return 1;
  }
        
  UARTReset();  
  UARTSend("AT+SIDET=0,8\r\n"); //  0 == 0db .. 15 == +22,5dB
  if (UARTWaitOK() != HAL_OK) {
    // failed to adjust side tone level
    return 1;
  }

  UARTReset();
  UARTSend("AT+CLVL=40\r\n"); // 0-100 
  if (UARTWaitOK() != HAL_OK) {
    // failed to adjust output gain level
    return 1;
  }
  
  UARTReset();
  UARTSend("AT+STTONE=1,8,5000\r\n"); //  0 == 0db .. 15 == +22,5dB
  if (UARTWaitOK() != HAL_OK) {
    // failed to adjust side tone level
    return 1;
  }
  return 0;
}

uint8_t SIM800_Call(void) {
  // turn ON Audio
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);
  // tie to GND DAC  
  DAC_Disable();

  UARTReset();  
  UARTSend("AT+CMICBIAS=1\r\n"); // 1 turn On bias, 2 turn Off bias
  if (UARTWaitOK() != HAL_OK) {
    // failed to adjust mic level
    return 1;
  }  
  
  UARTReset();  
  UARTSend("AT+CMIC=0,15\r\n");// 0 == 0db, 15 == +22.5dB
  if (UARTWaitOK() != HAL_OK) {
    // failed to adjust mic level
    return 1;
  }
        
  UARTReset();  
  UARTSend("AT+SIDET=0,5\r\n"); //  0 == 0db .. 15 == +22,5dB
  if (UARTWaitOK() != HAL_OK) {
    // failed to adjust side tone level
    return 1;
  }
  
  UARTReset();
  UARTSend("AT+CLVL=80\r\n"); // 0-100 
  if (UARTWaitOK() != HAL_OK) {
    // failed to adjust output gain level
    return 1;
  }
  
  UARTReset();  
  UARTSend("ATD27087740;\r\n"); // 29616171
  if (UARTWaitOK() != HAL_OK) {
    // failed to dial
    return 1;
  }
  // turn OFF Audio
  //HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
  return 0;
}

uint8_t SIM800_CellInfo(void) {
  UARTReset();
  UARTSend("AT+CGREG?\r\n");
  if (UARTWaitOK() != HAL_OK) {
    // failed to register
    return 2;
  }
  UARTReset();
  UARTSend("AT+COPS?\r\n");
  if (UARTWaitOK() != HAL_OK) {
    // failed to get home network info
    return 3;
  }
  UARTReset();
  UARTSend("AT+CSQ\r\n");
  if (UARTWaitOK() != HAL_OK) {
    // failed to get signal quality
    return 4;
  }
  return 0;
}


uint8_t SIM800_EnsureUP(void) {
  UARTReset();
  UARTSend("AT\r\n");
  if (UARTWaitOK() != HAL_OK) {
    SIM800_PowerUP();
  }
  uint8_t retries = 1;
  while(retries-- > 0) {
    UARTReset();
    UARTSend("AT+CMEE=1\r\n");// enable detailed errors
    if (UARTWaitOK() != HAL_OK) {
      // no response from module
      return 1;
    }
    // Check SIM ok
    UARTReset();
    UARTSend("AT+CPIN?\r\n");
    if (UARTWaitOK() != HAL_OK) {
      // Error occured? try to powercycle module!
      SIM800_PowerUP();
      HAL_Delay(1050); //1050mS      
      UARTReset();
      UARTSend("AT\r\n");
      if (UARTWaitOK() != HAL_OK) {
        SIM800_PowerUP();
      }
    } else {
      break;
    }
  }
  if (retries == 0xff) { // -1
    // failed to initialize module with sim card
    return 100;
  }
  return 0; // success
}

void SIM800_Wakeup(void) {
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET); // set DTR low , 50ms wait is required
  // intelligent wait
  while(1) {
    UARTReset();    
    UARTSend("AT\r\n");
    if (UARTWaitOK() == HAL_OK) {
      break;
    }
  }
}

void SIM800_Sleep(void) {
  UARTReset();
  UARTSend("AT+CSCLK=1\r\n");
  if (UARTWaitOK() != HAL_OK) {
    Error_Handler();
  }
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET); // set DTR high
}

void EnterSleep(void) {
  DISPLAY_Sleep();
  SIM800_Sleep();
  // 
  HAL_EnableDBGStandbyMode();
  //HAL_PWR_EnableBkUpAccess();
  HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);
  HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN2);  
  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);  
  HAL_PWR_EnterSTANDBYMode();
}

extern uint8_t cc;
extern uint8_t hcc;
extern uint8_t err;
uint32_t cunter = 0;

uint8_t getChargerState(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};  
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct); // 
  HAL_Delay(10);
  uint8_t weak_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6);

  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct); // 
  HAL_Delay(10);
  
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET); // 2k resistor GND
  uint8_t strong_gnd_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET); // 2k resistor 3V3
  uint8_t strong_3v3_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6);  
  
  if (weak_state == 1 && strong_gnd_state == 0 && strong_3v3_state == 1) {
    return 0; // HiZ, no adapter is connected
  }
  if (weak_state == 0 && strong_gnd_state == 0 && strong_3v3_state == 0) {
    return 1; // Charging
  }
  if (weak_state == 0 && strong_gnd_state == 0 && strong_3v3_state == 1) {
    return 2; // Charging Finished
  }
  return 0; // unexpected state
}

void RingOn(void) {
  // turn ON Audio
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);   
  if (HAL_TIM_Base_Start(&htim6) != HAL_OK) {
    Error_Handler();    
  }
  if (HAL_DAC_Start_DMA_Dual(&hdac, (uint32_t*)loop, sizeof(loop)/4) != HAL_OK) {
    Error_Handler();
  }
}

void RingOff(void) {
  // turn OFF Audio
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
  if (HAL_DAC_Stop_DMA_Dual(&hdac) != HAL_OK) {
    Error_Handler();
  }  
  if (HAL_TIM_Base_Stop(&htim6) != HAL_OK) {
    Error_Handler();
  }
}

uint8_t kbd_a;
uint8_t kbd_b;
uint8_t kbd_c;
uint8_t kbd_d;

uint8_t alarm = 0;
//void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc) {
//  alarm++;
//}

void MX_RTC_SetTimestamp(uint8_t year, uint8_t month, uint8_t day, uint8_t weekday,
                         uint8_t hours, uint8_t minutes, uint8_t seconds);
/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{  
  HAL_Init();
  //
  __HAL_RCC_PWR_CLK_ENABLE();
  uint8_t wakeup = 0;
  if (__HAL_PWR_GET_FLAG(PWR_FLAG_WU) == SET)
  {
    wakeup = 1;
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
  }
  
  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  
  MX_DMA_Init();
  MX_DAC_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();

  if (wakeup == 1) {
     SIM800_Wakeup(); // wakeup, in case in from sleep, etc
  }
  
  MX_RTC_Init();  
  
  if (__HAL_RTC_ALARM_GET_FLAG(&hrtc, RTC_FLAG_ALRAF) == SET) {
    alarm++;
    __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRAF);
  }
  
  if (wakeup == 0) { // initialize Display only after real poweron    
    DISPLAY_Init();    
    MX_RTC_SetTimestamp(
     21, 10, 2, 6,
     11, 05, 0);  
    __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRAF); // clear alarms     
  } else {
    DISPLAY_ReInit();
  }
  
  uint8_t simStatus = SIM800_EnsureUP();

  char buff[32] = {0};
  RTC_TimeTypeDef sTime;

  if (HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK) {
    Error_Handler();
  }

  if (wakeup == 0) { // initialize alarm once 
    // Set Alarm
    RTC_AlarmTypeDef sAlarm = {0};
    sAlarm.Alarm = RTC_ALARM_A;
    sAlarm.AlarmTime = sTime;
    sAlarm.AlarmTime.Seconds += 15;
    sAlarm.AlarmMask = RTC_ALARMMASK_NONE;
    sAlarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;
    sAlarm.AlarmDateWeekDay = 2;
    if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BIN) != HAL_OK) {
      Error_Handler();
    }
  }
  
  uint8_t prevDa = 0;
  uint8_t ringState = 0;
  while (1)
  {
    if (__HAL_RTC_ALARM_GET_FLAG(&hrtc, RTC_FLAG_ALRAF) == SET) {
      alarm++;
      __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRAF);
    }
    cunter++;
    uint8_t da = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1);
    if (da == 1 && prevDa == 0) {
      kbd_a = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13);
      kbd_b = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12);
      kbd_c = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_11);
      kbd_d = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2);      
      //if (kbd_a && kbd_b && kbd_c && kbd_d && !ringState) {
      //  RingOn();
      //  ringState = 1;
      //} else if (kbd_a && kbd_b && kbd_c && kbd_d && ringState) {
      //  RingOff();
      //  ringState = 0;        
      //}
      if (!kbd_a && kbd_b && kbd_c && kbd_d) {
        SIM800_Call();
      }      
    }
    prevDa = da;
    
    uint8_t ch_state = getChargerState();
    if (HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK) {
      Error_Handler();
    }

    ssd1306_Fill(Black);
    ssd1306_SetCursor(0,0);
    sprintf(buff, "h=%d m=%d s=%d", sTime.Hours, sTime.Minutes, sTime.Seconds);
    ssd1306_WriteString(buff, 10, 7, Font7x10, White);
    ssd1306_SetCursor(0,16);
    sprintf(buff, "ss=%d a=%d b=%d c=%d d=%d", simStatus, kbd_a, kbd_b, kbd_c, kbd_d);
    ssd1306_WriteString(buff, 10, 7, Font7x10, White);        
    ssd1306_UpdateScreen();
    
    if (cunter == 200) {
      //EnterSleep();
    }

  }
  /* USER CODE END 3 */
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL3;
  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLL_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}


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
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
