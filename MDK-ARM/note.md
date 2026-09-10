1. 在keil工程里面可以直接在.uvprojx的配置文件中直接配置魔法棒里面的相关配置，例如基本的IncludePath和工程组Group或者GroupItem相关的c文件和头文件。

2. 要想实现串口打印必须使用交叉连接开发板和st-link的tx和rx，然后必须共地。还要在keil工程的配置文件开启魔法棒中的target面板中的Use MicroLIB

2. 串口通信是点对点通信，利用串口交换数据不需要地址。

3. 串口的接收中断是一次性的当接收到一个字符如果不再次调用HAL_UART_Receive_IT()这个启用接收中断的函数的话，那么接下来的接收完成一个字节之后就不再会触发中断。

4. 可同时收发的通信协议为全双工，同一时间要么收，要么发的是半双工。共用一根时钟信号线的是同步，像串口这中约定好波特率的大小以及传输字节大小的是异步。

5. I2C通信SDA线和CLK线都要接4.7K的上拉电阻到3.3v。

6. sprintf 是一个C语言标准库函数，它的名字可以拆解为 "String Print Formatted"（格式化字符串打印）把一堆不同类型的数据（整数、字符串等），按照你指定的格式，组合成一个完整的字符串，然后存放到一个字符数组（缓冲区）里。
sprintf(cmd, "AT+MQTTUSERCFG=0,1,\"NULL\",\"%s\",\"%s\",0,0,\"\"", product_id, password);cmd就是存放最终字符串的“缓冲区”（一个字符数组）

7. #include <string.h>  // 提供 strstr 函数
#include <stdlib.h>  // 提供 atoi 函数

8. AT+GMR\r\n指令发送给WiFi模块返回其烧录的固件版本信息

9. 利用串口通信将esp8266WiFi模块将数据，上报到移动OneNet物联网平台的具体步骤：
	1. 首先发送AT指令查看WiFi模块是否正常工作，返回OK代表正常。
	2. 发送指令AT+GMR指令查看WiFi模块烧录的固件版本信息，一般1.几的版本是不支持MQTT的指令的，或者指令老旧，无法订阅物联网的主题，烧录的具体步骤：利用串口通信烧录固件，下载安信可的固件烧录工具ESP_DOWNLOAD_TOOL，网址：https://docs.ai-thinker.com/esp8266/，下载第1471号固件，下载完成之后先将WiFi模块的GPIO0接gnd，再给WiFi通电，此时进入固件烧录模式，打开烧录工具在第一行点击三个点按钮...，选中下载固件bin文件，@后面填入0x0000的地址，点击start开始烧录等待完成，烧录完成之后拔掉GPIO0与gnd的连线，重新上电即可。
	3. 发送AT+CWMODE=1指令将WiFi模块设置为station模式返回OK
	4. 发送AT+CWJAP="WIFI_NAME","WIFI_PASSWORD",WIFI_NAME：WiFi的名称，WIFI_PASSWORD：WiFi的密码,名称和密码一定要输对，一定一定，esp8266仅支持2.4GHZ频率的通信，5GHZ不支持，连不上。最后返回OK。
	5. 发送AT+MQTTUSERCFG=0,1,"stm32f407_esp8266(设备id)","ZcizCASxvs(产品id)","version=2018-10-31&res=products%2FZcizCASxvs%2Fdevices%2Fstm32f407_esp8266&et=1895232083&method=md5&sign=8i4WA2y9utMjNwnAoHW7zw%3D%3D(利用设备密钥计算的token)",0,0,""，返回OK证明设置MQTT用户成功。
	6. 发送AT+MQTTCONNCFG=0,60,0,"","",0,0设置mqtt Broker连接属性（必须设置，否则无法连接MQTT Broker）返回OK
	7. AT+MQTTCONN=0,"mqtts.heclouds.com",1883,1连接 MQTT Broker，返回OK
	8. AT+MQTTCONN?  查询 MQTT Broker连接状态，返回AT+MQTTCONN?+MQTTCONN:0,4,1,"mqtts.heclouds.com","1883","",1 OK 其中4代表已连接MQTT Broker
	9. 发送AT+MQTTSUB=0,"$sys/ZcizCASxvs/stm32f407_esp8266/thing/property/post/reply",0 订阅“设备属性上报响应”主题,返回OK
	10. AT+MQTTSUB=0,"$sys/ZcizCASxvs/stm32f407_esp8266/thing/property/post/set",0 订阅“设备属性设置请求”主题，返回OK
	11. 经过9和10步骤之后再次发送AT+MQTTCONN? 返回的状态应该是6表示已订阅主题
	12. AT+MQTTPUBRAW=0,"$sys/ZcizCASxvs/stm32f407_esp8266/thing/property/post",115(数据的字符长度),0,0 // 进入数据模式，发送指定长度到“设备属性上报请求”主题之后自动退出 返回OK
	13. 直接发送{"id": "123","version": "1.0","params": {"temp":{"value": 23.6}}}
	14. 在发送数据之前，必须要在产品管理的产品开发里面创建自定义功能的物模型。跟你要发送的属性名称（temp）保持一致。发送值的返回也必须保持在物模型的设置值范围内，不然报错。
	15. cJSON轻量的JSON的c语言库，是要动态分配内存的，如果mdk工程中的startup_stm32f407xx.s中的Heap_Size太小的话，是无法正常生成JSON数据的，
Flash：非易失性存储器，断电不丢；存放程序、常量。
RAM/SRAM：易失性读写内存，断电丢失；存放运行时数据。
栈 Stack：存放函数参数、局部变量、中断现场；自动分配和释放。
堆 Heap：供 malloc/cJSON 动态申请内存；使用后必须 free()。
Heap_Size 太小会导致 malloc() 返回 NULL，进而出现 JSON 生成失败或 %s 打印问号。

10. 向物联网平台传输数据必须跟物联网上面的物模型设置的数据类型完全一致，否则传不上去。

11.char value_text[16];
snprintf(value_text, sizeof(value_text), "%.1f", (double)temp_value);
定义一个字符串数组缓冲区，然后将temp_value数值按照，float类型保留一位小数的格式保存到value_text的缓冲区，

12. 每次在自检wifi模块的是否连接时候，如果在发送设置连接wifi的账号和密码的时候，连接成功之后不能马上去查询wifi连接mqtt broker 的状态，如果不延时的话，查询出来的mqtt broker的状态永远为3.（连接已断开）。

13. c语言中向oneNet物联网平台上报数据的时候不要最后不要\r\n。否则会返回ERROR会影响判断。
14. ABP1为单片机的低速外设总线，ABP2为单片机的高速外设总线

15. CMSIS：(Cortex Microcontroller Software Interface Standard)是 Cortex-M处理器系列的与供应商无关的硬件抽象层

16. stm32芯片cubemx的项目配置，System Core里面的SYS中的debug改为串口，Serial Wire
17. freertos的移植：主要的c文件：croutine.c, event_groups.c, list.c, queue.c, stream_buffer.c, task.c, timers.c, 主要的头文件： include里面的所有的头文件， 

18. 在keil工程里面可以直接在.uvprojx的配置文件中直接配置魔法棒里面的相关配置，例如基本的IncludePath和工程组Group或者GroupItem相关的c文件和头文件。

19. 在freeRTOS中的xTaskCreate（）创建时要根据任务空间的大小具体分配stack大小，如果代码量大所占用stack空间越大，如果分配stack空间不足的话会导致程序卡死

20. freeRTOS创建的A任务如果不在任务中使用vTaskDelay的话,则不会让出cpu的控制权,除非有和该任务优先级相等的B任务,
那么当改B执行的时候CPU的控制权交由B来控制.

