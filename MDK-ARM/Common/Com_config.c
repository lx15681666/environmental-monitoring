#include "Com_config.h"

/* ---------------------------------------------------------------------------
 * Com_config.h 中全局变量的唯一定义
 * 所有其他地方通过头文件中的 extern 声明来引用这些变量。
 * ------------------------------------------------------------------------ */
AHT20_Connect_State AHT20State = READ_SUCCESS; // AHT20 是否在线（默认 0，与原先保持一致）

ESP8266_Connect_State connectState = OFF_LINE; // ESP8266 是否在线

cache_ctrl_t cache_ctrl = {0};                 // 环形队列控制块
