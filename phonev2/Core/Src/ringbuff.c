/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "usart.h"

extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;

#define BUFF_SIZE 20

volatile uint8_t buffer1[BUFF_SIZE];
volatile uint8_t buffer2[BUFF_SIZE];
uint8_t bufferIdx = 0;
uint8_t isOK = 0;
/* Timeout is in milliseconds */
int32_t TIMEOUT = 0;

void UARTSend(char* data) {
  uint16_t size = strlen(data);
  if (HAL_UART_Transmit_DMA(&huart1, (uint8_t*)data, size)) {
    Error_Handler();
  }
}

HAL_StatusTypeDef UARTWaitOK(void) {
  TIMEOUT = 10;
  while(TIMEOUT) {
    if (isOK) {
      return HAL_OK;
    }
  }
  return HAL_TIMEOUT;
}

/* Initialize the Ring Buffer */
void UARTReset (void)
{
  HAL_UART_Abort(&huart1);

  memset((void* )buffer1, '\0', BUFF_SIZE);
  memset((void* )buffer2, '\0', BUFF_SIZE);
  bufferIdx = 0;
  isOK = 0;
  if (HAL_UARTEx_ReceiveToIdle_DMA(&huart1, (bufferIdx ? (uint8_t* )buffer2 : (uint8_t* )buffer1), BUFF_SIZE)){
    Error_Handler();
  }
  __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  char* currentBuff = (bufferIdx ? (char*)buffer2 : (char*)buffer1);
  
  for (uint8_t i = 1; i < Size; i++) {
    if (currentBuff[i - 1] == 'O' && currentBuff[i] == 'K') {
      isOK = 1;
    }
  }
  
  // start new rx
  if (bufferIdx) {
    bufferIdx = 0;
  } else {
    bufferIdx = 1;
  }
  if (HAL_UARTEx_ReceiveToIdle_DMA(&huart1, (bufferIdx ? (uint8_t* )buffer2 : (uint8_t* )buffer1), BUFF_SIZE)){
    Error_Handler();
  }
  __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);  
}