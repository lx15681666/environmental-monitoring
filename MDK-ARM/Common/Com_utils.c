#include "Com_utils.h"

#include "FreeRTOS.h"
#include "task.h"

/* 时间基准：授时那一刻的 UTC 秒 + 当时的系统 tick */
static volatile uint32_t   s_unix_base = 0;
static volatile TickType_t s_tick_base = 0;
static volatile uint8_t    s_synced77    = 0;

static int month_of(const char *s)
{
    static const char *m[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int i;

    for (i = 0; i < 12; i++)
    {
        if (strncmp(s, m[i], 3) == 0)
        {
            return i; /* 0 ~ 11 */
        }
    }
    return -1;
}

/* 公历日期 -> 1970-01-01 起算的天数（Howard Hinnant 算法，不依赖 libc） */
static uint32_t days_from_civil(int y, unsigned m, unsigned d)
{
    int era;
    unsigned yoe, doy, doe;

    y -= (m <= 2);
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = (unsigned)(y - era * 400);
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

    return (uint32_t)(era * 146097 + (int)doe - 719468);
}

/* 年(如2026) 月(1~12) 日 时 分 秒 -> Unix 秒(UTC) */
static uint32_t civil_to_unix(int y, int mon, int d, int h, int mi, int s)
{
    return days_from_civil(y, (unsigned)mon, (unsigned)d) * 86400UL +
           (uint32_t)h * 3600UL +
           (uint32_t)mi * 60UL +
           (uint32_t)s;
}

uint8_t Time_ParseSntp(const char *buf, uint32_t *out_unix)
{
    const char *p;
    char wday[8] = {0};
    char mon[8]  = {0};
    int d = 0, h = 0, mi = 0, s = 0, y = 0;
    int m;
    uint32_t unix_sec;

    if (buf == NULL || out_unix == NULL)
    {
        return 0;
    }

    p = strstr(buf, "+CIPSNTPTIME:");
    if (p == NULL)
    {
        return 0;
    }
    p += strlen("+CIPSNTPTIME:");

    if (sscanf(p, "%3s %3s %d %d:%d:%d %d", wday, mon, &d, &h, &mi, &s, &y) != 7)
    {
        return 0;
    }

    m = month_of(mon);
    if (m < 0 || y < 2025) /* 年份不对 = 还没同步到网络时间 */
    {
        return 0;
    }

    /* month_of 返回 0~11，civil_to_unix 需要 1~12，所以 +1
       ESP8266 用 AT+CIPSNTPCFG=1,8 配置为北京时间，这里减 8 小时转成 UTC */
    unix_sec = civil_to_unix(y, m + 1, d, h, mi, s) - 8UL * 3600UL;

    *out_unix = unix_sec;
    return 1;
}

void Time_SetUnix(uint32_t unix_sec)
{
    taskENTER_CRITICAL();
    s_unix_base = unix_sec;
    s_tick_base = xTaskGetTickCount();
    s_synced77    = 1;
    taskEXIT_CRITICAL();
}

uint8_t Time_IsSynced(void)
{
    return s_synced77;
}

uint32_t Time_GetUnixSec(void)
{
    TickType_t delta;

    if (!s_synced77)
    {
        return 0;
    }

    delta = xTaskGetTickCount() - s_tick_base;
    return s_unix_base + (uint32_t)(delta / configTICK_RATE_HZ);
}

uint64_t Time_GetUnixMs(void)
{
    TickType_t delta;
    if (!s_synced77)
    {
        return 0;
    }

    delta = xTaskGetTickCount() - s_tick_base;
    return (uint64_t)s_unix_base * 1000ULL + (uint64_t)delta * 1000ULL / configTICK_RATE_HZ;
}
