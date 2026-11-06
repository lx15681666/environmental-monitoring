
#include "Int_ESP8266.h"

#define WIFI_NAME "HUAWEI-R1G3O4"
#define WIFI_PASSWORD "qazwsxedc"
#define PRODUCT_ID "ZcizCASxvs"
#define CLIENT_ID "stm32f407_esp8266"
#define DEVICE_NAME "stm32f407_esp8266"
#define MQTT_PASSWORD "version=2018-10-31&res=products%2FZcizCASxvs%2Fdevices%2Fstm32f407_esp8266&et=1895232083&method=md5&sign=MOaOjwClixZ0tSs3oy0l1A%3D%3D"
#define MQTT_Link_Address "mqtts.heclouds.com"
#define MQTT_Link_Port 1883

char cmd[256];

EspState esp_state = AT;

// OTA升级信息结构体
static OTAJSON_t ota_json = {0};

sendState state = SEND;

extern ESP8266_Connect_State connectState; // 进入ESP_STATE_READY之后才算在线ON_LINE

// 状态机上下文：不保存阻塞等待，而是保存当前状态和超时时刻
static uint32_t esp_deadline = 0;
static uint8_t esp_retry_count = 0;
// 传感器产生数据后先放入缓存，由状态机异步发布
static uint8_t publish_pending = 0;
static char publish_buffer[256];

static uint8_t Esp8266_DeadlineExpired(void)
{
    // 使用有符号差值，正确处理 HAL_GetTick() 溢出
    return (int32_t)(HAL_GetTick() - esp_deadline) >= 0;
}

static void Esp8266_StartWait(uint32_t timeout_ms)
{
    // esp_state = wait_state;
    esp_deadline = HAL_GetTick() + timeout_ms;
}
static uint8_t Esp8266_ResponseContains(const char *keyword)
{
    return keyword != NULL && strstr((char *)rx_wifi_buffer, keyword) != NULL;
}

static void Esp8266_ClearResponse(void)
{
    __disable_irq();
    memset(rx_wifi_buffer, 0, sizeof(rx_wifi_buffer));
    rx_wifi_index = 0;
    __enable_irq();
}

void Esp8266_init(void) // esp初始化
{
    Esp8266_ClearResponse();
    esp_state = QUERY_STATE;
    esp_retry_count = 0;
    publish_pending = 0;
}

sendState sendCommand(char *cmd, char *keyWords, EspState sendCmd, uint8_t select)
{
    if (state == SEND)
    {
        ESP8266_SendCommand(cmd, select);
        Esp8266_StartWait(5000);
        state = WAIT;
    }
    if (state == WAIT)
    {
        if (Esp8266_ResponseContains(keyWords) || Esp8266_ResponseContains("ERROR"))
        {
            debugger_log(rx_wifi_buffer);
            state = SEND;
            esp_state = sendCmd;
        }
        else if (Esp8266_DeadlineExpired()) // 超时失败
        {
            state = SEND;
            esp_state = AT;
        }
    }
    return state;
}
EspState ESP8266_Task(void)
{
    // 该函数每次只推进一个小步骤，不能在这里使用长时间 while 或 HAL_Delay
    switch (esp_state)
    {
    case AT: // 测试连通性
        sendCommand("AT", "OK", SET_MODE, 1);
        break;
    case SET_MODE: // 设置模式
        sendCommand("AT+CWMODE=1", "OK", LINK_WIFI, 1);
        break;
    case LINK_WIFI: // 连接wifi
        sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"", WIFI_NAME, WIFI_PASSWORD);
        sendCommand(cmd, "WIFI GOT IP", QUERY_STATE, 1);
        break;
    case QUERY_STATE: // 查状态
        sendCommand("AT+MQTTCONN?", "OK", VIEW_STATE, 1);
        break;
    case VIEW_STATE: // 判断状态
        Get_MQTT_Connect_Status();
        break;
    case SET_USER: // 设置MQTT用户
        sprintf(cmd, "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"",
                DEVICE_NAME, PRODUCT_ID, MQTT_PASSWORD);
        sendCommand(cmd, "OK", SET_CONFIG, 1);
        break;
    case SET_CONFIG: // 配置MQTT参数
        sendCommand("AT+MQTTCONNCFG=0,60,0,\"\",\"\",0,0", "OK", LINK_BROKER, 1);
        break;
    case LINK_BROKER: // 连接broker
        sprintf(cmd, "AT+MQTTCONN=0,\"%s\",%d,1", MQTT_Link_Address, MQTT_Link_Port);
        sendCommand(cmd, "OK", SUBSCRIBE_RES, 1);
        break;
    case SUBSCRIBE_RES: // 订阅物模型全部主题
        sprintf(cmd, "AT+MQTTSUB=0,\"$sys/%s/%s/thing/property/#\",0", PRODUCT_ID, DEVICE_NAME);
        sendCommand(cmd, "OK", READY, 1);
        break;
    case SUBSCRIBE_SET: // 订阅上传响应主题
        sendCommand(cmd, "OK", READY, 1);
        ESP8266_ClearBuffer();
        break;
    // case CONTROL_CMD: // 订阅命令主题
    //     sprintf(cmd, "AT+MQTTSUB=0,\"$sys/%s/%s/thing/property/get\", 0", PRODUCT_ID, DEVICE_NAME);
    //     sendCommand(cmd, "OK", READY, 1);
    //     break;
    case SET_TIME:
        sprintf(cmd, "AT+CIPSNTPCFG=1,8,\"%s\",\"%s\"", "cn.ntp.org.cn", "ntp.sjtu.edu.cn"); // 设置时区
        sendCommand(cmd, "OK", QUERY_TIME, 1);
        break;
    case QUERY_TIME:
        sendCommand("AT+CIPSNTPTIME?", "OK", GET_TIME, 1); // 获取时间
        break;
    case GET_TIME:;
        uint32_t sec = 0;
        if (strstr((char *)rx_wifi_buffer, "1970") != NULL)
        {
            esp_state = QUERY_TIME; // ready // 如果时间查错重新查
        }
        if (Time_ParseSntp((char *)rx_wifi_buffer, &sec))
        {
            Time_SetUnix(sec); // 记录获取时间
            esp_state = READY; // ready
        }
        break;
    case READY:
        // 建链完成后，等待传感器任务提交新的 JSON 数据
        if (publish_pending)
        {
            esp_state = HEAD;
        }
        break;
    case HEAD:
        sprintf(cmd, "AT+MQTTPUBRAW=0,\"$sys/%s/%s/thing/property/post\",%d,0,0", // 请求上报
                PRODUCT_ID, DEVICE_NAME, strlen(publish_buffer));
        sendCommand(cmd, ">", PUSH, 1);
        break;
    case PUSH:
        sendCommand(publish_buffer, "success", READY, 0);
        publish_pending = 0;
        ESP8266_ClearBuffer();
        break;
    }
    return esp_state;
}

void Get_MQTT_Connect_Status(void) // 获取 MQTT 连接状态
{
    char *p = strstr((char *)rx_wifi_buffer, "+MQTTCONN:");
    int status = 0;
    if (p != NULL)
    {
        p += strlen("+MQTTCONN:");    // 移动指针到状态码位置
        sscanf(p, "%*d,%d", &status); // 解析状态码
        debugger_printf("MQTT status: %d\n", status);
        if (status == 0)
        {
            esp_state = SET_USER;
        }
        else if (status == 3)
        {
            vTaskDelay(3000);
            esp_state = QUERY_STATE;
        }
        else if (status == 4) // 未订阅主题
        {
            esp_state = SUBSCRIBE_RES;
        }
        else if (status == 6)
        {
            esp_state = SET_TIME; // 设置时间
        }
        else
        {
            esp_state = SET_USER;
        }
    }
}

uint8_t Publish_MQTT_Message(const char *message)
// 发布 MQTT 消息 // 在获取数据的时候调用这个函数
{
    if (message == NULL || strlen(message) >= sizeof(publish_buffer))
    {
        return 0;
    }
    strcpy(publish_buffer, message);
    publish_pending = 1;
    return 1; // 仅表示消息已接收，最终结果由 ESP8266_Task() 异步打印
}
/**
 * @brief 获取esp8266·状态
 */
EspState get_esp_status()
{
    return esp_state;
}

uint8_t get_publish_pending()
{
    return publish_pending;
}
// /**
//  * @brief 将id封装成json字符串
//  * @param id 设备ID
//  * @return JSON字符串
//  */
// char *stringfy_json(const char *id)
// {
//     cJSON *root = cJSON_CreateObject();
//     if (root == NULL)
//     {
//         return NULL;
//     }
//     if (cJSON_AddStringToObject(root, "id", id) == NULL ||
//         cJSON_AddNumberToObject(root, "code", 200) == NULL ||
//         cJSON_AddStringToObject(root, "msg", "success") == NULL)
//     {
//         cJSON_Delete(root);
//         return NULL;
//     }
//     char *json_string = cJSON_PrintUnformatted(root);
//     cJSON_Delete(root);
//     return json_string;
// }

/**
 * @brief 解析云端命令
 */

void parse_command(const char *json_str, uint8_t *value)
{
    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL)
        return;
    cJSON *id_item = cJSON_GetObjectItem(root, "id");
    if (id_item && cJSON_IsString(id_item))
    {
        sprintf(cmd, "AT+MQTTPUB=0,\"$sys/%s/%s/thing/property/set_reply\",\"{\\\"id\\\":\\\"%s\\\"\\,\\\"code\\\":200\\,\\\"msg\\\":\\\"success\\\"}\",0,0", PRODUCT_ID, DEVICE_NAME, id_item->valuestring);
    }
    cJSON *params = cJSON_GetObjectItem(root, "params");
    if (params && cJSON_IsObject(params))
    {
        cJSON *led_item = cJSON_GetObjectItem(params, "led");
        if (led_item && cJSON_IsBool(led_item))
        {
            *value = cJSON_IsTrue(led_item); // true → 1
        }
    }
    cJSON_Delete(root);
}
/**
 * @brief 解析ota信息
 */
int parse_ota_info(const char *json_str, OTAJSON_t *info)
{
    if (!json_str || !info)
        return -1;

    cJSON *root = cJSON_Parse(json_str);
    if (!root)
        return -2;

    cJSON *params = cJSON_GetObjectItem(root, "params");
    if (!params)
    {
        cJSON_Delete(root);
        return -3;
    }

    cJSON *ota = cJSON_GetObjectItem(params, "OTAJSON");
    if (!ota)
    {
        cJSON_Delete(root);
        return -4;
    }

    cJSON *item = NULL;

    item = cJSON_GetObjectItem(ota, "isOTA");
    if (item)
        info->isOTA = cJSON_IsTrue(item);

    item = cJSON_GetObjectItem(ota, "md5");
    if (item && cJSON_IsString(item))
    {
        strncpy(info->md5, item->valuestring, sizeof(info->md5) - 1);
        info->md5[sizeof(info->md5) - 1] = '\0';
    }

    item = cJSON_GetObjectItem(ota, "size");
    if (item && cJSON_IsNumber(item))
        info->size = (uint32_t)item->valuedouble;

    item = cJSON_GetObjectItem(ota, "target");
    if (item && cJSON_IsString(item))
    {
        strncpy(info->target, item->valuestring, sizeof(info->target) - 1);
        info->target[sizeof(info->target) - 1] = '\0';
    }

    item = cJSON_GetObjectItem(ota, "tid");
    if (item && cJSON_IsNumber(item))
        info->tid = (uint32_t)item->valuedouble; // ← 关键修复

    cJSON_Delete(root); // 删原始 root，会递归释放 ota
    return 0;
}

/**
 * @brief 获取云端下发命令
 */

uint8_t get_net_cmd(void)
{
    if (Esp8266_ResponseContains("}}"))
    {
        char *json_start = strstr((char *)rx_wifi_buffer, "{");
        if (json_start != NULL)
        {
            debugger_log(rx_wifi_buffer);
            uint8_t value = 3;
            parse_command(json_start, &value);
            parse_ota_info(json_start, &ota_json);
            // __disable_irq();
            esp_state = SUBSCRIBE_SET; // 进入上报状态
            // __enable_irq();
            return value;
        }
        return 2;
    }
    return 3;
}

OTAJSON_t get_ota_json(void)
{
    return ota_json;
}

/**
 * 发送连接tcp的指令
 */
// uint8_t link_tcp_server()
// {
//     ESP8266_SendCommand
//     sendCommand("AT", "OK", SET_MODE, 1);
// }
