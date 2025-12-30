#ifndef SERVER_H
#define SERVER_H

#include "config_parser.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_PORT 8888
#define BUFFER_SIZE 2048

// 协议命令
#define CMD_GET "GET"
#define CMD_SET "SET"
#define CMD_SAVE "SAVE"
#define CMD_EXIT "EXIT"

// 响应码
#define RESP_OK "OK"
#define RESP_ERROR "ERROR"
#define RESP_NOT_FOUND "NOT_FOUND"

// 全局配置管理器
extern config_manager_t *g_config_manager;

// 函数声明
void signal_handler(int sig);
int server_init(int port);
void server_run(int server_fd);
void* handle_client(void *arg);
int process_command(const char *cmd, char *response, size_t response_size);


#endif // SERVER_H

