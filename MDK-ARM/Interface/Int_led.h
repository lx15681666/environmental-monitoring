#ifndef INT_LED_H
#define INT_LED_H
#include "gpio.h"

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} LED_t;

void led_on(LED_t *led);     // 开灯
void led_off(LED_t *led);    // 关灯
void led_toggle(LED_t *led); // 反转

#endif // INT_LED_H