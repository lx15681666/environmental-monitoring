#ifndef APP_OTA_H
#define APP_OTA_H
#include "stdint.h"
#include "Int_ESP8266.h"
#include "FreeRTOS.h"
#include "task.h"


#define OTA_MAGIC 0x4F544146u // 魔数



// OTA标志区结构体
typedef struct
{
    uint32_t magic;     /* ① 魔数：判断标志区是否有效 */
    uint32_t boot_from; /* ② 0=从A启动，1=从B启动 */
    uint32_t ota_state; /* ③ 升级状态（状态机） */
    uint32_t version;   /* ④ 固件版本 */
    uint32_t crc32;     /* ⑤ 可选：校验整个标志区，防半写损坏 */
} OtaFlag_t;

/**
 * @brief 保存OTA升级信息到flash
 *
 */
void save_ota_json();

/**
 * @brief 读取OTA升级信息
 *
 */
void read_ota_json();

/**
 * @brief 下载任务
 */
void download_task();

/**
 * 连接tcp 数据透传
 */
void link_tcp_server();

#endif