#include "server.h"
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

// 全局配置管理器
config_manager_t *g_config_manager = NULL;

// 客户端连接信息
typedef struct {
    int fd;
    struct sockaddr_in addr;
} client_info_t;

// 信号处理
volatile int g_running = 1;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        g_running = 0;
        printf("\n收到退出信号，正在关闭服务器...\n");
    }
}

// 初始化服务器
int server_init(int port) {
    int server_fd;
    struct sockaddr_in server_addr;
    int opt = 1;

    // 创建socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket创建失败");
        return -1;
    }

    // 设置socket选项
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt失败");
        close(server_fd);
        return -1;
    }

    // 绑定地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind失败");
        close(server_fd);
        return -1;
    }

    // 监听
    if (listen(server_fd, 10) < 0) {
        perror("listen失败");
        close(server_fd);
        return -1;
    }

    printf("服务器已启动，监听端口 %d\n", port);
    return server_fd;
}

// 处理客户端命令
int process_command(const char *cmd, char *response, size_t response_size) {
    if (!cmd || !response) {
        return -1;
    }

    char command[64];
    char key[MAX_KEY_LEN];
    char value[MAX_VALUE_LEN];
    int ret;

    // 解析命令
    if (sscanf(cmd, "%63s", command) != 1) {
        snprintf(response, response_size, "%s: 无效的命令格式", RESP_ERROR);
        return -1;
    }

    // 处理GET命令：GET <key>
    if (strcmp(command, CMD_GET) == 0) {
        if (sscanf(cmd, "%*s %255s", key) != 1) {
            snprintf(response, response_size, "%s: GET命令需要提供键名", RESP_ERROR);
            return -1;
        }

        char val[MAX_VALUE_LEN];
        ret = config_get_value(g_config_manager, key, val, sizeof(val));
        if (ret == 0) {
            snprintf(response, response_size, "%s: %s=%s", RESP_OK, key, val);
        } else {
            snprintf(response, response_size, "%s: 键 '%s' 不存在", RESP_NOT_FOUND, key);
            return -1;
        }
    }
    // 处理SET命令：SET <key> <value>
    else if (strcmp(command, CMD_SET) == 0) {
        // 跳过命令名 "SET "
        const char *ptr = cmd;
        while (*ptr != '\0' && *ptr != ' ') ptr++; // 跳过 "SET"
        while (*ptr == ' ') ptr++; // 跳过空格

        if (*ptr == '\0') {
            snprintf(response, response_size, "%s: SET命令需要提供键和值", RESP_ERROR);
            return -1;
        }

        // 提取key（到下一个空格或行尾）
        const char *key_start = ptr;
        while (*ptr != '\0' && *ptr != ' ' && *ptr != '\t') ptr++;
        
        size_t key_len = ptr - key_start;
        if (key_len >= MAX_KEY_LEN) {
            key_len = MAX_KEY_LEN - 1;
        }
        strncpy(key, key_start, key_len);
        key[key_len] = '\0';

        // 跳过空格
        while (*ptr == ' ' || *ptr == '\t') ptr++;

        if (*ptr == '\0') {
            snprintf(response, response_size, "%s: SET命令需要提供值", RESP_ERROR);
            return -1;
        }

        // 提取value（直到行尾，去除末尾空白）
        const char *value_start = ptr;
        size_t val_len = strlen(value_start);
        
        // 去除value末尾的空白
        while (val_len > 0 && (value_start[val_len - 1] == ' ' || 
                               value_start[val_len - 1] == '\t' ||
                               value_start[val_len - 1] == '\n' ||
                               value_start[val_len - 1] == '\r')) {
            val_len--;
        }
        
        if (val_len >= MAX_VALUE_LEN) {
            val_len = MAX_VALUE_LEN - 1;
        }
        strncpy(value, value_start, val_len);
        value[val_len] = '\0';

        ret = config_set_value(g_config_manager, key, value);
        if (ret == 0) {
            snprintf(response, response_size, "%s: %s=%s", RESP_OK, key, value);
        } else {
            snprintf(response, response_size, "%s: 设置失败", RESP_ERROR);
            return -1;
        }
    }
    // 处理SAVE命令：SAVE
    else if (strcmp(command, CMD_SAVE) == 0) {
        ret = config_save_to_file(g_config_manager, "/tmp/etc/wireless/mediatek/mt7993.b0.dat");
        if (ret == 0) {
            snprintf(response, response_size, "%s: 配置已保存到 /tmp/etc/wireless/mediatek/mt7993.b0.dat", RESP_OK);
        } else {
            snprintf(response, response_size, "%s: 保存失败", RESP_ERROR);
            return -1;
        }
    }
    // 处理EXIT命令
    else if (strcmp(command, CMD_EXIT) == 0) {
        snprintf(response, response_size, "%s: 再见", RESP_OK);
        return 1; // 特殊返回值表示客户端要断开
    }
    else {
        snprintf(response, response_size, "%s: 未知命令 '%s'。支持的命令: GET, SET, SAVE, EXIT", RESP_ERROR, command);
        return -1;
    }

    return 0;
}

// 处理客户端连接
void* handle_client(void *arg) {
    client_info_t *client = (client_info_t *)arg;
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    ssize_t n;

    printf("客户端连接: %s:%d\n", 
           inet_ntoa(client->addr.sin_addr), 
           ntohs(client->addr.sin_port));

    while (g_running) {
        memset(buffer, 0, sizeof(buffer));
        n = recv(client->fd, buffer, sizeof(buffer) - 1, 0);
        
        if (n <= 0) {
            if (n == 0) {
                printf("客户端断开连接: %s:%d\n",
                       inet_ntoa(client->addr.sin_addr),
                       ntohs(client->addr.sin_port));
            } else {
                perror("recv错误");
            }
            break;
        }

        // 去除换行符
        buffer[n] = '\0';
        size_t len = strlen(buffer);
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r')) {
            buffer[len - 1] = '\0';
            len--;
        }

        printf("收到命令: %s\n", buffer);

        // 处理命令
        memset(response, 0, sizeof(response));
        int ret = process_command(buffer, response, sizeof(response));
        
        // 发送响应
        strcat(response, "\n");
        if (send(client->fd, response, strlen(response), 0) < 0) {
            perror("send错误");
            break;
        }

        // 如果是EXIT命令，断开连接
        if (ret == 1) {
            break;
        }
    }

    close(client->fd);
    free(client);
    return NULL;
}

// 运行服务器
void server_run(int server_fd) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd;

    while (g_running) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (g_running) {
                perror("accept错误");
            }
            continue;
        }

        // 为每个客户端创建线程
        client_info_t *client = (client_info_t *)malloc(sizeof(client_info_t));
        if (!client) {
            perror("内存分配失败");
            close(client_fd);
            continue;
        }

        client->fd = client_fd;
        client->addr = client_addr;

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, client) != 0) {
            perror("pthread_create失败");
            close(client_fd);
            free(client);
            continue;
        }

        pthread_detach(thread);
    }
}
