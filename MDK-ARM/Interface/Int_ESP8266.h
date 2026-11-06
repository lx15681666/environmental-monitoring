#ifndef INT_ESP8266_H
#define INT_ESP8266_H

#include "usart_irq.h"
#include "Com_debugger.h"
#include "Com_config.h"
#include "Com_utils.h"
#include "cJSON.h"
#include "FreeRTOS.h"
#include "task.h"

typedef enum
{
    // 每个 SEND 状态只发送一次命令
    AT,
    SET_MODE,
    LINK_WIFI,
    QUERY_STATE,
    VIEW_STATE,
    SET_USER,
    SUBSCRIBE_RES,
    SET_CONFIG,
    LINK_BROKER,
    SUBSCRIBE_SET,
    // CONTROL_CMD,
    HEAD,
    PUSH,
    QUERY_TIME,
    SET_TIME,
    GET_TIME,
    READY
} EspState;

typedef enum
{
    SEND,   // 可发送
    FAILED, // 失败
    WAIT    // 等待
} sendState;

// 升级数据结构体
typedef struct
{
    _Bool isOTA;
    char md5[33]; // 32位md5 + '\0'，少一个字节就会截掉末字符
    char target[7];
    uint32_t size;
    uint32_t tid;

} OTAJSON_t;

void Esp8266_init(void);
EspState ESP8266_Task(void);
uint8_t Publish_MQTT_Message(const char *message);
void Get_MQTT_Connect_Status(void);

/**
 * @brief 获取esp8266·状态
 */
EspState get_esp_status();

uint8_t get_publish_pending();

/**
 * @brief 获取网络命令
 */
uint8_t get_net_cmd(void);

/**
 * @brief 解析云端命令
 */
void parse_command(const char *json_str, uint8_t *value);

// /**
//  * @brief 将id封装成json字符串
//  * @param id 设备ID
//  * @return JSON字符串
//  */
// char *stringfy_json(const char *id);

/**
 * @获取云端命令结构体
 */
OTAJSON_t get_ota_json(void);

/**
 * 发送连接tcp的指令
 */
// uint8_t link_tcp_server();
#endif // INT_ESP8266_H