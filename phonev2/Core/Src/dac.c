/**
  ******************************************************************************
  * @file    dac.c
  * @brief   This file provides code for the configuration
  *          of the DAC instances.
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

/* Includes ------------------------------------------------------------------*/
#include "dac.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

DAC_HandleTypeDef hdac;
DMA_HandleTypeDef hdma_dac;
TIM_HandleTypeDef htim6;
uint8_t cc = 0;
uint8_t hcc = 0;
uint8_t err = 0;

void DAC_DMAHalfConvCpltDual(DMA_HandleTypeDef *hdma) {
  hcc++;
}

void DAC_DMAConvCpltDual(DMA_HandleTypeDef *hdma) {
  cc++;
}

void DAC_DMAErrorDual(DMA_HandleTypeDef *hdma) {
  err++;
}



/**
  * @brief  Enables DAC and starts conversion of channel.
  * @param  hdac pointer to a DAC_HandleTypeDef structure that contains
  *         the configuration information for the specified DAC.
  * @param  pData The source Buffer address.
  * @param  Length The length of data to be transferred from memory to DAC peripheral
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DAC_Start_DMA_Dual(DAC_HandleTypeDef *hdac, uint32_t *pData, uint32_t Length)
{
  HAL_StatusTypeDef status;
  uint32_t tmpreg = 0U;
  
  /* Check the parameters */
  
  /* Process locked */
  __HAL_LOCK(hdac);
  
  /* Change DAC state */
  hdac->State = HAL_DAC_STATE_BUSY;
  
  /* Set the DMA transfer complete callback */
  hdac->DMA_Handle1->XferCpltCallback = DAC_DMAConvCpltDual;
  
  /* Set the DMA half transfer complete callback */
  hdac->DMA_Handle1->XferHalfCpltCallback = DAC_DMAHalfConvCpltDual;
  
  /* Set the DMA error callback */
  hdac->DMA_Handle1->XferErrorCallback = DAC_DMAErrorDual;
  
  /* Enable the selected DAC DMA request */
  SET_BIT(hdac->Instance->CR, DAC_CR_DMAEN1);
  
  /* Get DHR12LD address */
  tmpreg = (uint32_t)&hdac->Instance->DHR12LD;
  
  /* Enable the DMA channel */
  /* Enable the DAC DMA underrun interrupt */
  __HAL_DAC_ENABLE_IT(hdac, DAC_IT_DMAUDR1);
  
  /* Enable the DMA channel */
  status = HAL_DMA_Start_IT(hdac->DMA_Handle1, (uint32_t)pData, tmpreg, Length);
  
  /* Process Unlocked */
  __HAL_UNLOCK(hdac);
  
  if (status == HAL_OK)
  {
    /* Enable the Peripheral */
    __HAL_DAC_ENABLE(hdac, DAC_CHANNEL_1);
    __HAL_DAC_ENABLE(hdac, DAC_CHANNEL_2);    
  }
  else
  {
    hdac->ErrorCode |= HAL_DAC_ERROR_DMA;
  }
  
  /* Return function status */
  return status;
}

/**
  * @brief  Disables DAC and stop conversion of channel.
  * @param  hdac pointer to a DAC_HandleTypeDef structure that contains
  *         the configuration information for the specified DAC.
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_DAC_Stop_DMA_Dual(DAC_HandleTypeDef *hdac)
{
  /* Disable the selected DAC channel DMA request */
  hdac->Instance->CR &= ~(DAC_CR_DMAEN1);

  /* Disable the Peripheral */
  __HAL_DAC_DISABLE(hdac, DAC_CHANNEL_1);
  __HAL_DAC_DISABLE(hdac, DAC_CHANNEL_2);

  /* Disable the DMA channel */

  /* Disable the DMA channel */
  (void)HAL_DMA_Abort(hdac->DMA_Handle1);

  /* Disable the DAC DMA underrun interrupt */
  __HAL_DAC_DISABLE_IT(hdac, DAC_IT_DMAUDR1);

  /* Change DAC state */
  hdac->State = HAL_DAC_STATE_READY;

  /* Return function status */
  return HAL_OK;
}

/* DAC init function */
void MX_DAC_Init(void)
{
  // configure timer6
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 10;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 2000;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  //__HAL_TIM_ENABLE_DMA(TIM2, TIM_DMA_TRIGGER)
  //htim6.Instance->EGR = TIM_EGR_UG;
    
  DAC_ChannelConfTypeDef sConfig = {0};
  /** DAC Initialization
  */
  hdac.Instance = DAC;
  if (HAL_DAC_Init(&hdac) != HAL_OK)
  {
    Error_Handler();
  }

  sConfig.DAC_Trigger = DAC_TRIGGER_T6_TRGO;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;

  /** DAC channel OUT1 config
  */  
  if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /** DAC channel OUT2 config
  */
  if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }  
}

void HAL_DAC_MspInit(DAC_HandleTypeDef* dacHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(dacHandle->Instance==DAC)
  {
    /* DAC clock enable */
    __HAL_RCC_DAC_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**DAC GPIO Configuration
    PA4     ------> DAC_OUT1
    PA5     ------> DAC_OUT2
    */
    GPIO_InitStruct.Pin = GPIO_PIN_4|GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* DAC DMA Init */
    hdma_dac.Instance = DMA1_Channel2;
    hdma_dac.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_dac.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_dac.Init.MemInc = DMA_MINC_ENABLE;
    hdma_dac.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    hdma_dac.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    hdma_dac.Init.Mode = DMA_CIRCULAR;
    hdma_dac.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_dac) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(dacHandle,DMA_Handle1,hdma_dac);
  }
}

void HAL_DAC_MspDeInit(DAC_HandleTypeDef* dacHandle)
{

  if(dacHandle->Instance==DAC)
  {
    /* Peripheral clock disable */
    __HAL_RCC_DAC_CLK_DISABLE();

    /**DAC GPIO Configuration
    PA4     ------> DAC_OUT1
    PA5     ------> DAC_OUT2
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_4|GPIO_PIN_5);

    /* DAC DMA DeInit */
    HAL_DMA_DeInit(dacHandle->DMA_Handle1);
  }
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* tim_baseHandle)
{

  if(tim_baseHandle->Instance==TIM6)
  {
    /* TIM6 clock enable */
    __HAL_RCC_TIM6_CLK_ENABLE();

    /* TIM6 interrupt Init */
    HAL_NVIC_SetPriority(TIM6_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM6_IRQn);
  }
}

void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef* tim_baseHandle)
{

  if(tim_baseHandle->Instance==TIM6)
  {
    /* Peripheral clock disable */
    __HAL_RCC_TIM6_CLK_DISABLE();

    /* TIM6 interrupt Deinit */
    HAL_NVIC_DisableIRQ(TIM6_IRQn);
  }
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
