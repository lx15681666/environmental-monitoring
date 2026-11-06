#ifndef COM_CONFIG_H
#define COM_CONFIG_H

#include <stdint.h>

/**
 * 记录数据的结构体
 */
typedef struct
{
    uint16_t magic;     // 固定值 0xA5A5，用于判断记录有效性
    uint16_t seq;       // 序号，递增，用于去重和排序
    int16_t temp;       // 温度 ×100 (如 2534 表示 25.34℃)
    uint16_t humi;      // 湿度 ×100
    uint64_t timestamp; // 时间戳 (来自 NTP 或系统 tick)
    uint32_t crc; // 前面所有字节的 CRC16 校验值
} record_t;

// 构建环形队列控制结构体
typedef struct
{
    uint32_t write_index; // 下一个写入位置
    uint32_t read_index;  // 下一个读取位置
    uint32_t count;       // 当前有效记录数
} cache_ctrl_t;

// esp8266连接状态
typedef enum
{
    ON_LINE = 0,  // 在线
    OFF_LINE = 1, // 离线
    ON_LINE_IDLE  // 在空闲状态
} ESP8266_Connect_State;

// AHT20状态
typedef enum
{
    READ_SUCCESS = 0, // 读数成功/在线
    READ_FAILED = 1,  // 读数失败/离线
} AHT20_Connect_State;

typedef struct
{
    float temperature; // 温度。
    float humidity;    // 湿度。
} AHT20_Value;

/* ---------------------------------------------------------------------------
 * 全局变量声明（extern）
 * 注意：头文件里只能"声明"，不能"定义"；
 *       否则每个包含本头文件的 .c 都会生成一份实体，导致链接时报 multiply defined。
 *       真正的定义统一放在 Common/Com_config.c 中。
 * ------------------------------------------------------------------------ */

extern record_t record_data; // 记录数据

extern AHT20_Connect_State AHT20State; // AHT20 是否在线

extern ESP8266_Connect_State connectState; // ESP8266 是否在线

extern cache_ctrl_t cache_ctrl; // 环形队列控制块

#endif