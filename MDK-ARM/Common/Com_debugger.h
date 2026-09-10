#ifndef COM_DEBUGGER_H
#define COM_DEBUGGER_H

#include "usart.h"
#include "stdio.h"
#include "stdarg.h"
#include "string.h"

// 日志输出打印，比比较占用cpu资源，按照115200比特率计算，大概10个字节需要1ms的时间

// 日志输出打印开关
#define DEBUGGER_PRINTF_ON 1

#ifdef DEBUGGER_PRINTF_ON
// 只打印文件名和行号
#define __FILE_NAME__ (strrchr(__FILE__, '\\') ? (strrchr(__FILE__, '\\') + 1) : __FILE__)
#define __FILE_NAME (strrchr(__FILE_NAME__, '/') ? (strrchr(__FILE_NAME__, '/') + 1) : __FILE__)

#define debugger_printf(format, ...) printf("[%s:%d]" format, __FILE_NAME, __LINE__, ##__VA_ARGS__)

#else

#define debugger_printf(format, ...)

#endif

#endif