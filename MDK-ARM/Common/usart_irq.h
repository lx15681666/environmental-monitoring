#ifndef USART_IRQ_H
#define USART_IRQ_H
#include "main.h"
#include "usart.h"
#include <string.h> // 提供 strstr 函数
#include <stdlib.h> // 提供 atoi 函数
#include <stdio.h>  // 提供 printf 函数
#include "Com_debugger.h"
#define RX_BUFFER_SIZE 1024
// extern uint8_t rx_buffer[RX_BUFFER_SIZE];
extern uint8_t rx_wifi_buffer[RX_BUFFER_SIZE];
extern volatile uint16_t rx_wifi_index;
extern volatile uint8_t rx_wifi_byte;
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void ESP8266_SendCommand(const char *cmd, uint8_t select);
void ESP8266_SendRaw(const char *data);

/**
 * @brief 清空buffer
 */

void ESP8266_ClearBuffer(void);

#endif