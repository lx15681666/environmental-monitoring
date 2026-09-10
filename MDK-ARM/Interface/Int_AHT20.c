#include "Int_AHT20.h"

#define AHT20_ADDRESS (0x38 << 1) // AHT20 I2C address shifted for STM32 HAL
#define AHT20_CMD_INIT 0xBE
#define AHT20_CMD_MEASURE 0xAC
#define AHT20_CMD_SOFT_RESET 0xBA

float temperature = 0.0f;
float humidity = 0.0f;
typedef struct
{
    float temperature; // 温度。
    float humidity;   // 湿度。
} AHT20_Value;

// 发送指令
uint8_t AHT20_SendCmd(uint8_t cmd, uint8_t *data, uint8_t len)
{
    if (len > 2 || (len > 0 && data == NULL))
    {
        return HAL_ERROR;
    }

    if (len == 0)
    {
        return HAL_I2C_Master_Transmit(&hi2c1, AHT20_ADDRESS, &cmd, 1, 100);
    }
    else
    {
        uint8_t buf[3] = {cmd, data[0], data[1]};
        return HAL_I2C_Master_Transmit(&hi2c1, AHT20_ADDRESS, buf, len + 1, 100);
    }
}
// 读取数据
uint8_t AHT20_ReadData(uint8_t *buffer, uint8_t len)
{
    return HAL_I2C_Master_Receive(&hi2c1, AHT20_ADDRESS, buffer, len, 100);
}

// AHT20初始化
uint8_t AHT20_Init(void)
{
    uint8_t cmd_data[2] = {0x08, 0x00}; // 初始化命令数据
    uint8_t status = AHT20_SendCmd(AHT20_CMD_INIT, cmd_data, 2);
    if (status != HAL_OK)
    {
        return status;
    }
    HAL_Delay(10);
    return HAL_OK;
}
char *build_report_data(float temp_value, float humidity_value)
{
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
    memset(value_text, 0, sizeof(value_text));
    snprintf(value_text, sizeof(value_text), "%.2f", (double)humidity_value);
    cJSON_AddItemToObject(params, "humidity", humidity);
    cJSON_AddRawToObject(humidity, "value", value_text);
    debugger_printf("humidity is value_text: %s", value_text);
    if (params == NULL || temp == NULL)
    {
        cJSON_Delete(root);
        return NULL;
    }
    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;
}

uint8_t AHT20_ReadTemperatureAndHumidity(float *temperature, float *humidity)
{
    uint8_t read_buffer[6] = {0}; // 用于存储读取的数据
    uint32_t tempp_raw = 0, hum_raw = 0;

    if (temperature == NULL || humidity == NULL)
    {
        return 4;
    }

    // 发送测量命令
    if (HAL_I2C_Master_Transmit(&hi2c1, AHT20_ADDRESS, (uint8_t[]){AHT20_CMD_MEASURE, 0x33, 0x00}, 3, 100) != HAL_OK)
    {
        return 1; // 发送命令失败
    }

    HAL_Delay(80); // 等待测量完成
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
        HAL_Delay(10);
    }
    if (read_buffer[0] & 0x80U)
    {
        return 3; // 数据仍未准备好
    }
    // 解析湿度数据
    hum_raw = ((uint32_t)(read_buffer[1]) << 12) |
              ((uint32_t)(read_buffer[2]) << 4) |
              ((uint32_t)(read_buffer[3]) >> 4);
    tempp_raw = ((uint32_t)(read_buffer[3] & 0x0F) << 16) |
                ((uint32_t)(read_buffer[4]) << 8) |
                ((uint32_t)(read_buffer[5]));
    // 转换为实际值
    *humidity = (float)hum_raw * 100.0f / 1048576.0f;
    *temperature = (float)tempp_raw * 200.0f / 1048576.0f - 50.0f;
    return HAL_OK;
}

void AHT20Task(void *arg) // 获取具体温度
{
    AHT20_Value aht20_value = {0};
    uint8_t status = AHT20_ReadTemperatureAndHumidity(&aht20_value.temperature, &aht20_value.humidity);

    if (status == HAL_OK)
    {
        debugger_printf("temp is%f, humidity is %f", aht20_value.temperature, aht20_value.humidity);
        char *json_data = build_report_data(aht20_value.temperature, aht20_value.humidity);
        // printf("json_data is %s\r\n", json_data);
        if (json_data != NULL)
        {
            Publish_MQTT_Message(json_data); // 上报数据
            free(json_data);                 // 记得释放内存
        }
        else
        {
            debugger_printf("build_report_data failed: json_data is NULL\r\n");
        }
        // 上报数据
        // debugger_printf("temperature: %.2f C, humidity: %.2f %%\r\n", aht20_value.temperature, aht20_value.humidity);
    }
    else
    {
        debugger_printf("AHT20 read failed: %u\r\n", status);
    }
}
