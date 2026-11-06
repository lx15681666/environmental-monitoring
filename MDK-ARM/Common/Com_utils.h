#ifndef COM_UTILS_H
#define COM_UTILS_H

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "main.h"

uint8_t Time_ParseSntp(const char *buf, uint32_t *out_unix);

/* 时间基准：授时成功后保存，之后靠系统 tick 推演 */
void     Time_SetUnix(uint32_t unix_sec);
uint8_t  Time_IsSynced(void);
uint32_t Time_GetUnixSec(void);
uint64_t Time_GetUnixMs(void);

#endif
