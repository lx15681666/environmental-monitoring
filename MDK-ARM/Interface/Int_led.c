#include "Int_led.h"

void led_on(LED_t *led) // 开灯
{
    HAL_GPIO_WritePin(led->port, led->pin, GPIO_PIN_SET);
}

void led_off(LED_t *led) // 关灯
{
    HAL_GPIO_WritePin(led->port, led->pin, GPIO_PIN_RESET);
}


void led_toggle(LED_t *led) // 反转
{
    HAL_GPIO_TogglePin(led->port, led->pin);
}
