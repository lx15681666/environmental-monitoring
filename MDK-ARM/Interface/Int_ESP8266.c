
#include "Int_ESP8266.h"

#define WIFI_NAME "HUAWEI-R1G3O4"
#define WIFI_PASSWORD "qazwsxedc"
#define PRODUCT_ID "ZcizCASxvs"
#define CLIENT_ID "stm32f407_esp8266"
#define DEVICE_NAME "stm32f407_esp8266"
#define MQTT_PASSWORD "version=2018-10-31&res=products%2FZcizCASxvs%2Fdevices%2Fstm32f407_esp8266&et=1895232083&method=md5&sign=8i4WA2y9utMjNwnAoHW7zw%3D%3D"
#define MQTT_Link_Address "mqtts.heclouds.com"
#define MQTT_Link_Port 1883

char cmd[256];

EspState esp_state = ESP_STATE_QUERY_STATE;

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

static void Esp8266_StartWait(EspState wait_state, uint32_t timeout_ms)
{
    esp_state = wait_state;
    esp_deadline = HAL_GetTick() + timeout_ms;
}

static void Esp8266_CommandFailed(void)
{
    esp_retry_count++;
    if (esp_retry_count >= 3U)
    {
        // 连续失败后进入错误状态，等待一段时间再重新初始化模块
        debugger_printf("ESP8266 state failed: %d\r\n", esp_state);
        esp_state = ESP_STATE_QUERY_STATE;
        esp_deadline = HAL_GetTick() + 5000U;
    }
    else
    {
        esp_state = ESP_STATE_SEND_AT;
    }
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
    esp_state = ESP_STATE_QUERY_STATE;
    esp_retry_count = 0;
    publish_pending = 0;
}

void ESP8266_Task(void)
{
    // 该函数每次只推进一个小步骤，不能在这里使用长时间 while 或 HAL_Delay
    switch (esp_state)
    {
    case ESP_STATE_SEND_AT:
        // 发送命令后立即返回，下一轮再检查响应
        ESP8266_SendCommand("AT");
        Esp8266_StartWait(ESP_STATE_WAIT_AT, 3000);
        break;

    case ESP_STATE_WAIT_AT:
        if (Esp8266_ResponseContains("OK"))
        {
            debugger_printf("buffer is %s\r\n", rx_wifi_buffer);
            esp_retry_count = 0;
            esp_state = ESP_STATE_SEND_MODE;
        }
        else if (Esp8266_ResponseContains("ERROR") || Esp8266_DeadlineExpired())
        {
            Esp8266_CommandFailed();
        }
        break;

    case ESP_STATE_SEND_MODE:
        ESP8266_SendCommand("AT+CWMODE=1");
        Esp8266_StartWait(ESP_STATE_WAIT_MODE, 3000);
        break;

    case ESP_STATE_WAIT_MODE:
        if (Esp8266_ResponseContains("OK"))
        {
            debugger_printf("buffer is %s\r\n", rx_wifi_buffer);
            esp_retry_count = 0;
            esp_state = ESP_STATE_SEND_WIFI;
        }
        else if (Esp8266_ResponseContains("ERROR") || Esp8266_DeadlineExpired())
        {
            Esp8266_CommandFailed();
        }
        break;

    case ESP_STATE_SEND_WIFI:
        sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"", WIFI_NAME, WIFI_PASSWORD);
        ESP8266_SendCommand(cmd);
        Esp8266_StartWait(ESP_STATE_WAIT_WIFI, 20000);
        break;

    case ESP_STATE_WAIT_WIFI:
        // 连接 WiFi 需要等待异步事件，WIFI GOT IP 才表示已拿到地址
        if (Esp8266_ResponseContains("WIFI GOT IP"))
        {
            debugger_printf("buffer is %s\r\n", rx_wifi_buffer);
            esp_retry_count = 0;
            esp_state = ESP_STATE_QUERY_STATE; // wifi连接成功就进入查询mqtt的状态
        }
        else if (Esp8266_ResponseContains("ERROR") ||
                 Esp8266_ResponseContains("FAIL") ||
                 Esp8266_DeadlineExpired())
        {
            Esp8266_CommandFailed();
        }
        break;
    case ESP_STATE_QUERY_STATE:
        sprintf(cmd, "AT+MQTTCONN?");
        ESP8266_SendCommand(cmd);
        Esp8266_StartWait(ESP_STATE_WAIT_QUERY_STATE, 7000);
        break;
    case ESP_STATE_WAIT_QUERY_STATE:
        if (Esp8266_ResponseContains("OK"))
        {
            debugger_printf("query buffer is %s\r\n", rx_wifi_buffer);
            esp_retry_count = 0;
            int mqtt_status = Get_MQTT_Connect_Status();
            debugger_printf("mqtt_status is%d\r\n", mqtt_status);
            if (mqtt_status == 0)
            {
                esp_state = ESP_STATE_SEND_MQTT_USER;
            }
            else if (mqtt_status == 3)
            {
                esp_deadline = HAL_GetTick() + 3000;
                esp_state = ESP_STATE_WAIT_MQTT_STATUS;
            }
            else if (mqtt_status == 4)
            {
                esp_state = ESP_STATE_SEND_PUBLISH_PROPERTY_RES;
            }
            else if (mqtt_status == 6)
            {
                esp_state = ESP_STATE_READY; // 一切准备就绪
            }
            else
            {
                esp_state = ESP_STATE_SEND_MQTT_USER;
            }
        }
        else if (Esp8266_ResponseContains("ERROR") || Esp8266_DeadlineExpired())
        {
            debugger_printf("Query buffer is %s\r\n", rx_wifi_buffer);
            Esp8266_CommandFailed();
        }
        break;
    case ESP_STATE_WAIT_MQTT_STATUS: // 等待为6
        if (Esp8266_DeadlineExpired())
        {
            esp_state = ESP_STATE_QUERY_STATE;
        }
        break;
    case ESP_STATE_SEND_MQTT_USER:
        sprintf(cmd, "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"",
                DEVICE_NAME, PRODUCT_ID, MQTT_PASSWORD);
        ESP8266_SendCommand(cmd);
        Esp8266_StartWait(ESP_STATE_WAIT_MQTT_USER, 3000);
        break;

    case ESP_STATE_WAIT_MQTT_USER:
        if (Esp8266_ResponseContains("OK"))
        {
            debugger_printf("buffer is %s\r\n", rx_wifi_buffer);
            esp_state = ESP_STATE_SEND_MQTT_CFG;
        }
        else if (Esp8266_ResponseContains("ERROR") || Esp8266_DeadlineExpired())
        {
            debugger_printf("ESP_STATE_WAIT_MQTT_USER buffer is %s\r\n", rx_wifi_buffer);
            Esp8266_CommandFailed();
        }
        break;

    case ESP_STATE_SEND_MQTT_CFG:
        ESP8266_SendCommand("AT+MQTTCONNCFG=0,60,0,\"\",\"\",0,0");
        Esp8266_StartWait(ESP_STATE_WAIT_MQTT_CFG, 3000);
        break;

    case ESP_STATE_WAIT_MQTT_CFG:
        if (Esp8266_ResponseContains("OK"))
        {
            debugger_printf("buffer is %s\r\n", rx_wifi_buffer);
            esp_state = ESP_STATE_SEND_MQTT_CONN;
        }
        else if (Esp8266_ResponseContains("ERROR") || Esp8266_DeadlineExpired())
        {
            Esp8266_CommandFailed();
        }
        break;

    case ESP_STATE_SEND_MQTT_CONN:
        sprintf(cmd, "AT+MQTTCONN=0,\"%s\",%d,1", MQTT_Link_Address, MQTT_Link_Port);
        ESP8266_SendCommand(cmd);
        Esp8266_StartWait(ESP_STATE_WAIT_MQTT_CONN, 10000);
        break;
    case ESP_STATE_WAIT_MQTT_CONN:
        if (Esp8266_ResponseContains("OK"))
        {
            debugger_printf("buffer is %s\r\n", rx_wifi_buffer);
            esp_retry_count = 0;
            esp_state = ESP_STATE_SEND_PUBLISH_PROPERTY_RES;
            debugger_printf("ESP8266 MQTT ready\r\n");
        }
        else if (Esp8266_ResponseContains("ERROR") || Esp8266_DeadlineExpired())
        {
            debugger_printf("AT+MQTTCONN=0 buffer is %s\r\n", rx_wifi_buffer);
            Esp8266_CommandFailed();
        }
        break;

    case ESP_STATE_SEND_PUBLISH_PROPERTY_RES:
        sprintf(cmd, "AT+MQTTSUB=0,\"$sys/%s/%s/thing/property/post/reply\",0", PRODUCT_ID, DEVICE_NAME);
        ESP8266_SendCommand(cmd);
        Esp8266_StartWait(ESP_STATE_WAIT_PUBLISH_PROPERTY_RES, 10000);
        break;

    case ESP_STATE_WAIT_PUBLISH_PROPERTY_RES:
        if (Esp8266_ResponseContains("OK"))
        {
            esp_retry_count = 0;
            esp_state = ESP_STATE_SEND_PUBLISH_PROPERTY_SET;
        }
        else if (Esp8266_ResponseContains("ERROR") || Esp8266_DeadlineExpired())
        {
            esp_state = ESP_STATE_SEND_MQTT_USER;
        }
        break;

    case ESP_STATE_SEND_PUBLISH_PROPERTY_SET:
        sprintf(cmd, "AT+MQTTSUB=0,\"$sys/%s/%s/thing/property/post/set\",0", PRODUCT_ID, DEVICE_NAME);
        ESP8266_SendCommand(cmd);
        Esp8266_StartWait(ESP_STATE_WAIT_PUBLISH_PROPERTY_SET, 10000);
        break;

    case ESP_STATE_WAIT_PUBLISH_PROPERTY_SET:
        if (Esp8266_ResponseContains("OK"))
        {
            esp_retry_count = 0;
            esp_state = ESP_STATE_READY;
        }
        else if (Esp8266_ResponseContains("ERROR") || Esp8266_DeadlineExpired())
        {
            esp_state = ESP_STATE_SEND_MQTT_USER;
        }
        break;

    case ESP_STATE_READY:
        // 建链完成后，等待传感器任务提交新的 JSON 数据
        if (publish_pending)
        {
            esp_state = ESP_STATE_PUBLISH_SEND;
        }
        break;

    case ESP_STATE_PUBLISH_SEND:
        // RAW 发布先发送头命令，模块返回 > 后才能发送 JSON 内容
        sprintf(cmd, "AT+MQTTPUBRAW=0,\"$sys/%s/%s/thing/property/post\",%d,0,0",
                PRODUCT_ID, DEVICE_NAME, strlen(publish_buffer));
        ESP8266_SendCommand(cmd);
        Esp8266_StartWait(ESP_STATE_PUBLISH_WAIT_PROMPT, 3000);
        break;

    case ESP_STATE_PUBLISH_WAIT_PROMPT:
        // > 表示 ESP8266 已经进入 payload 接收模式
        if (Esp8266_ResponseContains(">"))
        {
            debugger_printf("buffer is %s\r\n", rx_wifi_buffer);
            ESP8266_SendRaw(publish_buffer);
            Esp8266_StartWait(ESP_STATE_PUBLISH_WAIT_RESULT, 5000);
        }
        else if (Esp8266_ResponseContains("ERROR") || Esp8266_DeadlineExpired())
        {
            debugger_printf("MQTT publish prompt failed\r\n");
            publish_pending = 0;
            esp_state = ESP_STATE_QUERY_STATE;
        }
        break;

    case ESP_STATE_PUBLISH_WAIT_RESULT:
        // payload 发送完成后等待模块确认发布结果
        if (Esp8266_ResponseContains("success") || Esp8266_ResponseContains("OK"))
        {
            debugger_printf("MQTT publish success\r\n");
            debugger_printf("ESP_STATE_PUBLISH_WAIT_RESULT buffer is %s\r\n", rx_wifi_buffer);
            publish_pending = 0;
            esp_state = ESP_STATE_READY;
        }
        else if (Esp8266_DeadlineExpired())
        {
            publish_pending = 0;
            esp_state = ESP_STATE_QUERY_STATE;
        }
        break;

        // case ESP_STATE_ERROR:
        //     // 错误状态不阻塞主循环，达到重试时间后从 AT 开始恢复
        //     if (Esp8266_DeadlineExpired())
        //     {
        //         Esp8266_ClearResponse();
        //         esp_retry_count = 0;
        //         esp_state = ESP_STATE_SEND_AT;
        //     }
        //     break;

    default:
        esp_state = ESP_STATE_QUERY_STATE;
        break;
    }
}

int Get_MQTT_Connect_Status(void) // 获取 MQTT 连接状态
{
    char *p = strstr((char *)rx_wifi_buffer, "+MQTTCONN:");
    int status = 0;
    if (p != NULL)
    {
        p += strlen("+MQTTCONN:");    // 移动指针到状态码位置
        sscanf(p, "%*d,%d", &status); // 解析状态码
        return status;
    }
    else
    {
        return 0; // 未找到状态码，返回0表示未连接
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
