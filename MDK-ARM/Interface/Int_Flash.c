#include "Int_Flash.h"
extern cache_ctrl_t cache_ctrl; // 环形队列

extern ESP8266_Connect_State connectState;

HAL_StatusTypeDef flash_unlock() // flash擦除，读写解锁
{
    return HAL_FLASH_Unlock();
}

HAL_StatusTypeDef flash_lock() // flash，上锁
{
    return HAL_FLASH_Lock();
}

FLASH_EraseInitTypeDef EraseInitStruct; // flash扇区擦除初始化结构体
/**
 * 擦除扇区
 */
HAL_StatusTypeDef flash_erase_sector()
{
    flash_unlock();
    // 定义返回擦除错误的扇区缓冲变量，
    uint32_t SectorError = 0;
    // 擦除方式
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    // 选择的扇区
    EraseInitStruct.Sector = DATA_FLASH_SECTOR;
    // 擦除扇区的数量
    EraseInitStruct.NbSectors = 1;
    // 电压范围配置2.7V - 3.6V
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    // 执行擦除操作，如果所有扇区擦除成功返回0，若不返回0，则会将指定扇区的索引写入SectorError变量
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK)
    {
        debugger_printf("erase failed");
        return HAL_ERROR;
    }
    flash_lock();
    return HAL_OK;
}

// 写入数据 (按字写入，需确保地址已擦除)
HAL_StatusTypeDef flash_write_word(uint32_t addr, uint32_t data)
{
    return HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, data);
}

// 读取数据 (直接从内存地址读取)
record_t *flash_read(uint32_t addr)
{
    return (record_t *)addr;
}

/**
 * @brief 计算32为数据buffer的32为数据校验值
 */
uint32_t crc32(uint32_t pBuffer[], uint32_t BufferLength)
{
    return HAL_CRC_Calculate(&hcrc, pBuffer, BufferLength);
}

/**
 * @brief 写一条数据记录
 */

uint8_t flash_write_record(record_t *record)
{
    if (cache_ctrl.count >= TOTAL_RECORD_IN_11SECTOR_NUMBER) // 写满再擦
    {
        // 达到缓存数据限制
        cache_ctrl.read_index = (cache_ctrl.read_index + 1) % TOTAL_RECORD_IN_11SECTOR_NUMBER;
        cache_ctrl.count--;
    }
    record->magic = RECORD_MAGIC;
    record->crc = crc32((uint32_t *)record, RECORD_CRC_WORDS);
    // debugger_printf("crc cal write %d\n", crc32((uint32_t *)record, RECORD_CRC_WORDS));
    // 下一个写入的地址等于基地址+总的缓存所占字节空间的大小*写入索引（已写入的数据条数）
    uint32_t addr = DATA_FLASH_BASE_ADDR + cache_ctrl.write_index * sizeof(record_t);
    flash_unlock();
    for (int i = 0; i < sizeof(record_t) / 4; i++)
    {
        // 每次写入一个字（4字节）
        flash_write_word(addr + i * 4, ((uint32_t *)record)[i]);
    }
    flash_lock();
    // 控制块每次写入数据的时候更新
    cache_ctrl.write_index = (cache_ctrl.write_index + 1) % TOTAL_RECORD_IN_11SECTOR_NUMBER;
    cache_ctrl.count++;
    return 0;
}

/**
 * @brief 读一条数据记录
 * @return 0成功，1失败或数据错误，丢弃
 */

record_t *flash_read_record()
{
    if (cache_ctrl.count == 0)
        return NULL;
    uint32_t addr = DATA_FLASH_BASE_ADDR + cache_ctrl.read_index * sizeof(record_t);
    record_t *record = flash_read(addr);
    if (record->magic != RECORD_MAGIC)
        return NULL;
    if (record->crc != crc32((uint32_t *)record, RECORD_CRC_WORDS))
        return NULL;
    return record;
}

/**
 * @brief 删除数据记录
 */

void flash_delete_record()
{
    if (cache_ctrl.count != 0)
    {

        uint32_t addr = DATA_FLASH_BASE_ADDR + cache_ctrl.read_index * sizeof(record_t);
        flash_unlock();
        flash_write_word(addr, 0x00000000); // 清除 magic
        flash_lock();
        cache_ctrl.read_index = (cache_ctrl.read_index + 1) % TOTAL_RECORD_IN_11SECTOR_NUMBER;
        cache_ctrl.count--;
    }
}

/**
 * @brief 检查11扇区是否写满和还有没有数据没有处理，是否该清除flash
 * 0 可擦除， 1 不可擦除
 */
uint8_t is_sctor11_full_check()
{
    // 是否有数据没有处理
    if (cache_ctrl.count > 0)
    {
        return 1;
    }
    // 如果要写的位置到了最大可写数量时，必须擦除11flash
    return cache_ctrl.write_index >= TOTAL_RECORD_IN_11SECTOR_NUMBER ? 0 : 1;
}

/**
 * @brief 假如mcu断电之后恢复控制块，继续补传数据
 */
void recover_cache_ctl(cache_ctrl_t *cache_control)
{
    for (uint16_t i = 0; i < TOTAL_RECORD_IN_11SECTOR_NUMBER; i++)
    {
        uint32_t addr = DATA_FLASH_BASE_ADDR + (i * sizeof(record_t));
        record_t *record = flash_read(addr);
        if (record->magic == RECORD_MAGIC && record->crc == crc32((uint32_t *)record, RECORD_CRC_WORDS))
        {
            cache_control->count = i + 1;
            continue;
        }
        if (record->magic == 0xFFFF)
        {
            cache_control->write_index = i;
            break;
        }
        if (record->magic == 0)
        {
            cache_control->read_index = i + 1;
            continue;
        }
    }
}

/**
 * @brief 离线缓存record
 *
 */

void offline_cache_record(record_t record_data)
{
    flash_write_record(&record_data);
}

/**
 * @brief 补传数据 如果缓冲有数据，且目前处于空闲在线状态
 *
 */

record_t *resend_data()
{
    printf("block:count=%d, read_index=%d, write_index=%d\n", cache_ctrl.count, cache_ctrl.read_index, cache_ctrl.write_index);
    if (cache_ctrl.count > 0) // 证明有可读数据
    {
        return flash_read_record();
    }
    return NULL;
}

void cacl_recover_cache_ctl() // 获取控制块
{
    recover_cache_ctl(&cache_ctrl);
}