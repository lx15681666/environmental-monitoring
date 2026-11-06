#include "Com_debugger.h"

// 打印重定向到串口
int fputc(int ch, FILE *f)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);
    return ch;
}

void debugger_log(uint8_t *buffer)
{
#if (DEBUGGER_PRINTF_ON == 1)
    HAL_UART_Transmit_DMA(&huart1, buffer, strlen(buffer));
    HAL_UART_Transmit_DMA(&huart1, "\r\n", strlen("\r\n"));
#endif
}


uint8_t buffer[128] = "Debugger test: Hello, World!\r\n";
uint8_t buffer1[128] = "666666666666666666666666666666\r\n";
void debugger_test()
{
    HAL_UART_Transmit_DMA(&huart1, buffer, strlen(buffer));
    HAL_Delay(1000);
    HAL_UART_Transmit_DMA(&huart1, buffer1, strlen(buffer1));
}