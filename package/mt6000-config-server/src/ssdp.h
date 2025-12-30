#ifndef SSDP_H
#define SSDP_H

#include <pthread.h>

// SSDP相关常量
#define SSDP_MULTICAST_ADDR "239.255.255.250"
#define SSDP_PORT 1900
#define SSDP_MULTICAST_TTL 4

// 函数声明
int ssdp_init(void);
void ssdp_start(void);
void ssdp_stop(void);
void* ssdp_thread(void *arg);

#endif // SSDP_H

