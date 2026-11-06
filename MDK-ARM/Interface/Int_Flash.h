#ifndef INT_FLASH_H
#define INT_FLASH_H

#include "main.h"
#include "string.h"
#include "crc.h"
#include "Com_config.h"
#include "Com_debugger.h"

#define TOTAL_RECORD_IN_11SECTOR_NUMBER ((128000U / 24U) - 1) // 11号扇区总共可存的记录数据数量

#define DATA_FLASH_SECTOR FLASH_SECTOR_11 // 11号扇区

#define DATA_FLASH_BASE_ADDR 0x080e0000 // 扇区首地址

#define RECORD_CRC_WORDS (offsetof(record_t, timestamp) / 4U) // 5

#define RECORD_MAGIC 0xA5A5

#define CACHE_MAX_RECORDS 20 // 最大缓存记录数

HAL_StatusTypeDef flash_unlock(); // flash擦除，读写解锁

HAL_StatusTypeDef flash_lock(); // flash，上锁

/**
 * 擦除扇区
 */
HAL_StatusTypeDef flash_erase_sector();

// 写入数据 (按字写入，需确保地址已擦除)
HAL_StatusTypeDef flash_write_word(uint32_t addr, uint32_t data);

record_t *flash_read(uint32_t addr);

/**
 * @brief 计算32为数据buffer的32为数据校验值
 */
uint32_t crc32(uint32_t pBuffer[], uint32_t BufferLength);

/**
 * @brief 写一条数据记录
 */

uint8_t flash_write_record(record_t *record);

/**
 * @brief 读一条数据记录
 */

record_t *flash_read_record();

/**
 * @brief 假如mcu断电之后恢复控制块，继续补传数据
 */
void recover_cache_ctl(cache_ctrl_t *cache_control);

/**
 * @brief 离线缓存record
 *
 */

void offline_cache_record(record_t record_data);

/**
 * @brief 删除数据记录
 */

void flash_delete_record();

/**
 * @brief 补传数据 如果缓冲有数据，且目前处于空闲在线状态
 *
 */

record_t *resend_data();

void cacl_recover_cache_ctl(); // 获取控制块


/**
 * @brief 检查11扇区是否写满和还有没有数据没有处理，是否该清除flash
 * 0 可擦除， 1 不可擦除
 */
uint8_t is_sctor11_full_check();

#endif