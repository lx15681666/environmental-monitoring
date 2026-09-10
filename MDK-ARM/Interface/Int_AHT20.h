#ifndef INT_AHT20_H
#define INT_AHT20_H

#include <string.h>
#include <stdio.h>
#include "i2c.h"
#include "Int_ESP8266.h"
#include <stdlib.h>
#include "cJSON.h"
#include "Com_debugger.h"
extern void AHT20Task(void *arg); // 获取温度任务
#endif