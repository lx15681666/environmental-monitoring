#include "Int_AHT20.h"

#define AHT20_ADDRESS (0x38 << 1) // AHT20 I2C address shifted for STM32 HAL
// #define AHT20_CMD_INIT 0xBE
#define AHT20_CMD_MEASURE 0xAC
#define AHT20_CMD_SOFT_RESET 0xBA

char *json_data; // json数据

static AHT20_Value aht20_value = {0}; // AHT20 温湿度数据;

char *build_report_data(float temp_value, float humidity_value, uint64_t timetemp)
{
    // debugger_printf("temp_value: %f, humidity_value: %f, timetemp: %llu", temp_value, humidity_value, timetemp);
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        return NULL;
    }

    if (cJSON_AddStringToObject(root, "id", "123") == NULL ||
        cJSON_AddStringToObject(root, "version", "1.0") == NULL)
    {
        cJSON_Delete(root);
        return NULL;
    }

    char value_text[16];
    snprintf(value_text, sizeof(value_text), "%.1f", (double)temp_value);
    cJSON *params = cJSON_CreateObject();
    cJSON *temp = cJSON_CreateObject();
    cJSON *humidity = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "params", params);
    cJSON_AddItemToObject(params, "temp", temp);
    cJSON_AddRawToObject(temp, "value", value_text);
    cJSON_AddNumberToObject(temp, "time", (double)timetemp);

    memset(value_text, 0, sizeof(value_text));
    snprintf(value_text, sizeof(value_text), "%.2f", (double)humidity_value);
    cJSON_AddItemToObject(params, "humidity", humidity);
    cJSON_AddRawToObject(humidity, "value", value_text);
    cJSON_AddNumberToObject(humidity, "time", (double)timetemp); // 添加时间戳

    // debugger_printf("humidity is value_text: %s", value_text);
    if (params == NULL || temp == NULL)
    {
        cJSON_Delete(root);
        return NULL;
    }
    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;
}

uint8_t AHT20_ReadTemperatureAndHumidity()
{
    uint8_t read_buffer[6] = {0}; // 用于存储读取的数据
    uint32_t temp_raw = 0, hum_raw = 0;

    // 发送测量命令
    if (HAL_I2C_Master_Transmit(&hi2c1, AHT20_ADDRESS, (uint8_t[]){AHT20_CMD_MEASURE, 0x33, 0x00}, 3, 100) != HAL_OK)
    {
        return 1; // 发送命令失败
    }

    vTaskDelay(80); // 等待测量完成
    for (uint8_t retry = 0; retry < 10; retry++)
    {
        if (HAL_I2C_Master_Receive(&hi2c1, AHT20_ADDRESS, read_buffer, 6, 100) != HAL_OK)
        {
            return 2;
        }
        if ((read_buffer[0] & 0x80U) == 0U)
        {
            break;
        }
        if (retry == 9)
        {
            return 3;
        }
        vTaskDelay(10);
    }
    if (read_buffer[0] & 0x80U)
    {
        return 3; // 数据仍未准备好
    }
    // 解析湿度数据
    hum_raw = ((uint32_t)(read_buffer[1]) << 12) |
              ((uint32_t)(read_buffer[2]) << 4) |
              ((uint32_t)(read_buffer[3]) >> 4);
    temp_raw = ((uint32_t)(read_buffer[3] & 0x0F) << 16) |
               ((uint32_t)(read_buffer[4]) << 8) |
               ((uint32_t)(read_buffer[5]));
    // 转换为实际值
    aht20_value.humidity = (float)hum_raw * 100.0f / 1048576.0f;
    aht20_value.temperature = (float)temp_raw * 200.0f / 1048576.0f - 50.0f;
    return HAL_OK;
}

/**
 * @brief 获取json数据
 * @return json数据指针，失败返回NULL
 */
char *get_json_data(record_t record)
{
    return build_report_data(record.temp / 100.0f, record.humi / 100.0f, record.timestamp);
}

/**
 * 构建records
 */

static uint8_t seq = 0;
uint8_t create_record(record_t *record)
{
    uint8_t status = AHT20_ReadTemperatureAndHumidity();
    // debugger_printf("create_record status: %f, %f\n", aht20_value.temperature * 100 , aht20_value.humidity * 100);
    if (status == HAL_OK) // 先获取数据，再构建record_t数据
    {
        record->magic = 0xA5A5;                       // 记录有效性
        record->temp = aht20_value.temperature * 100; // 温度
        record->humi = aht20_value.humidity * 100;    // 湿度
        record->seq = seq++;                          // 序号
        record->timestamp = Time_GetUnixMs();         // 获取当前系统时间戳
        return HAL_OK;
    }
    return HAL_ERROR; // 失败
}