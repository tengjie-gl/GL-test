// Simple TCP client with UDP broadcast discovery
// 适用于 Linux/类 Unix 系统（虚拟机或开发板）。Windows 需稍作修改为 Winsock。
//
// 功能：
// 1. 使用 UDP 广播查找服务器 IP（选用固定发现端口）
// 2. 建立 TCP 连接到服务器
// 3. 支持：
//    a) 查询配置项参数值（例：SSID4）
//    b) 修改参数值（例：SSID4）
//    c) 让服务器写入当前全部选项到文件
// 4. 可被多个客户端同时运行（并发主要由服务器负责）

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <time.h>

// ====================== 可根据实际服务器修改的参数 ======================

// UDP 广播发现服务器的端口（服务器需要在该端口监听 UDP）
#define DISCOVERY_PORT 1900
// 服务器的 TCP 端口（be3600mt 上的 TCP 服务端口）
#define SERVER_TCP_PORT 8888
// 广播报文内容（服务器应识别此报文并回复）
#define DISCOVERY_MESSAGE "M-SEARCH * HTTP/1.1\r\n" \
                          "HOST: 239.255.255.250:1900\r\n" \
                          "MAN: \"ssdp:discover\"\r\n" \
                          "MX: 3\r\n" \
                          "ST: upnp:rootdevice\r\n" \
                          "\r\n"
// 服务器回复内容前缀（可自定义，只要客户端和服务器一致即可）
#define DISCOVERY_RESPONSE_PREFIX "HTTP/1.1 200 OK"

// 应用层协议命令格式约定（示例，需与服务器实现保持一致）：
// 查询参数:   "GET <param>\n"  -> 回复: "OK <value>\n" 或 "ERR <reason>\n"
// 设置参数:   "SET <param> <value>\n"  -> 回复: "OK\n" 或 "ERR <reason>\n"
// 写入文件:   "SAVE\n"  -> 回复: "OK\n" 或 "ERR <reason>\n"

#define MAX_BUFFER 1024

// ====================== 工具函数 ======================

static void trim_newline(char *s) {
    if (!s) return;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

// 设置 socket 为超时（接收）
static int set_recv_timeout(int sockfd, int seconds) {
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt(SO_RCVTIMEO)");
        return -1;
    }
    return 0;
}

// ====================== 服务器发现（UDP 广播） ======================

// 通过 UDP 广播发现服务器 IP
// 成功返回 0，并将服务器 IP 字符串写入 server_ip（如 "192.168.1.10"）
static int discover_server_ip(char *server_ip, size_t ip_buf_len) {
    int sockfd;
    struct sockaddr_in broadcast_addr;
    char buf[MAX_BUFFER];

    if (!server_ip || ip_buf_len == 0) {
        fprintf(stderr, "discover_server_ip: invalid buffer\n");
        return -1;
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket(UDP)");
        return -1;
    }

    int broadcast_enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        perror("setsockopt(SO_BROADCAST)");
        close(sockfd);
        return -1;
    }

    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(DISCOVERY_PORT);
    // 向本地网段广播：255.255.255.255（要求虚拟机/板子为桥接网卡）
    broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    // 设置接收超时，避免永久阻塞
    if (set_recv_timeout(sockfd, 5) < 0) {
        close(sockfd);
        return -1;
    }

    printf("发送 UDP 广播查找服务器...\n");

    ssize_t sent = sendto(sockfd,
                          DISCOVERY_MESSAGE,
                          strlen(DISCOVERY_MESSAGE),
                          0,
                          (struct sockaddr *)&broadcast_addr,
                          sizeof(broadcast_addr));
    if (sent < 0) {
        perror("sendto");
        close(sockfd);
        return -1;
    }

    // 等待服务器回复
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    ssize_t n = recvfrom(sockfd, buf, sizeof(buf) - 1, 0,
                         (struct sockaddr *)&from_addr, &from_len);
    if (n < 0) {
        perror("recvfrom (discovery timeout or error)");
        close(sockfd);
        return -1;
    }
    buf[n] = '\0';

    printf("收到服务器发现回应: \"%s\" 来自 %s\n",
           buf, inet_ntoa(from_addr.sin_addr));

    // 简单校验前缀
    if (strncmp(buf, DISCOVERY_RESPONSE_PREFIX, strlen(DISCOVERY_RESPONSE_PREFIX)) != 0) {
        fprintf(stderr, "发现回应内容不匹配，期望前缀: %s\n", DISCOVERY_RESPONSE_PREFIX);
        close(sockfd);
        return -1;
    }

    // 使用对端 IP 作为服务器 IP
    strncpy(server_ip, inet_ntoa(from_addr.sin_addr), ip_buf_len - 1);
    server_ip[ip_buf_len - 1] = '\0';

    close(sockfd);
    return 0;
}

// ====================== TCP 连接与命令收发 ======================

// 连接到服务器 TCP
static int connect_to_server(const char *server_ip, int port) {
    int sockfd;
    struct sockaddr_in serv_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket(TCP)");
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sockfd);
        return -1;
    }

    printf("连接服务器 %s:%d...\n", server_ip, port);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    printf("TCP 连接已建立。\n");
    return sockfd;
}

// 发送一条命令，并接收一行响应（以 '\n' 结束）
static int send_command_and_read_line(int sockfd, const char *cmd, char *resp, size_t resp_size) {
    if (!cmd || !resp || resp_size == 0) {
        return -1;
    }

    size_t cmd_len = strlen(cmd);
    ssize_t sent = send(sockfd, cmd, cmd_len, 0);
    if (sent < 0) {
        perror("send");
        return -1;
    }

    // 简单行读取：循环 recv，直到读到 '\n' 或缓冲区满
    size_t total = 0;
    while (total < resp_size - 1) {
        char c;
        ssize_t n = recv(sockfd, &c, 1, 0);
        if (n < 0) {
            perror("recv");
            return -1;
        } else if (n == 0) {
            // 对端关闭
            break;
        }
        resp[total++] = c;
        if (c == '\n') {
            break;
        }
    }
    resp[total] = '\0';
    return 0;
}

// 查询参数值
static int client_get_param(int sockfd, const char *param_name) {
    char cmd[MAX_BUFFER];
    char resp[MAX_BUFFER];

    snprintf(cmd, sizeof(cmd), "GET %s\n", param_name);

    if (send_command_and_read_line(sockfd, cmd, resp, sizeof(resp)) < 0) {
        fprintf(stderr, "发送 GET 命令失败。\n");
        return -1;
    }

    printf("服务器响应: %s", resp);
    // 应用层可进一步解析 "OK " / "ERR "
    if (strncmp(resp, "OK ", 3) == 0) {
        printf("参数 %s 的值为: %s\n", param_name, resp + 3);
    } else if (strncmp(resp, "ERR", 3) == 0) {
        printf("查询失败: %s\n", resp + 4);
    }
    return 0;
}

// 修改参数值
static int client_set_param(int sockfd, const char *param_name, const char *value) {
    char cmd[MAX_BUFFER];
    char resp[MAX_BUFFER];

    snprintf(cmd, sizeof(cmd), "SET %s %s\n", param_name, value);

    if (send_command_and_read_line(sockfd, cmd, resp, sizeof(resp)) < 0) {
        fprintf(stderr, "发送 SET 命令失败。\n");
        return -1;
    }

    printf("服务器响应: %s", resp);
    if (strncmp(resp, "OK", 2) == 0) {
        printf("参数 %s 修改成功。\n", param_name);
    } else if (strncmp(resp, "ERR", 3) == 0) {
        printf("修改失败: %s\n", resp + 4);
    }
    return 0;
}

// 让服务器写入当前全部选项到文件
static int client_save_to_file(int sockfd) {
    char cmd[MAX_BUFFER];
    char resp[MAX_BUFFER];

    snprintf(cmd, sizeof(cmd), "SAVE\n");

    if (send_command_and_read_line(sockfd, cmd, resp, sizeof(resp)) < 0) {
        fprintf(stderr, "发送 SAVE 命令失败。\n");
        return -1;
    }

    printf("服务器响应: %s", resp);
    if (strncmp(resp, "OK", 2) == 0) {
        printf("服务器已将当前配置写入文件。\n");
    } else if (strncmp(resp, "ERR", 3) == 0) {
        printf("写文件失败: %s\n", resp + 4);
    }
    return 0;
}

// ====================== 客户端主逻辑（简单菜单） ======================

static void print_menu(void) {
    printf("\n==== be3600mt 配置客户端 ====\n");
    printf("1. 查询参数值（示例：SSID4）\n");
    printf("2. 修改参数值（示例：SSID4）\n");
    printf("3. 让服务器写入当前全部选项到文件\n");
    printf("4. 退出\n");
    printf("请选择功能(1-4): ");
}

int main(int argc, char *argv[]) {
    char server_ip[64] = {0};
    int sockfd = -1;

    // 1. 发现服务器 IP（如果需要，你也可以通过 argv 传 IP，绕过发现）
    if (argc >= 2) {
        // 用户手动指定服务器 IP
        strncpy(server_ip, argv[1], sizeof(server_ip) - 1);
        server_ip[sizeof(server_ip) - 1] = '\0';
        printf("使用指定服务器 IP: %s\n", server_ip);
    } else {
        if (discover_server_ip(server_ip, sizeof(server_ip)) != 0) {
            fprintf(stderr, "通过广播未能发现服务器，请确认服务器已开启并实现发现协议。\n");
            return 1;
        }
        printf("发现服务器 IP: %s\n", server_ip);
    }

    // 2. 建立 TCP 连接
    sockfd = connect_to_server(server_ip, SERVER_TCP_PORT);
    if (sockfd < 0) {
        return 1;
    }

    // 3. 循环处理用户输入指令
    int running = 1;
    while (running) {
        print_menu();
        char line[64];
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        trim_newline(line);
        int choice = atoi(line);

        switch (choice) {
            case 1: {
                char param[128];
                printf("请输入要查询的参数名（例如 SSID4）: ");
                if (!fgets(param, sizeof(param), stdin)) {
                    break;
                }
                trim_newline(param);
                if (strlen(param) == 0) {
                    printf("参数名不能为空。\n");
                    break;
                }
                client_get_param(sockfd, param);
                break;
            }
            case 2: {
                char param[128];
                char value[256];
                printf("请输入要修改的参数名（例如 SSID4）: ");
                if (!fgets(param, sizeof(param), stdin)) {
                    break;
                }
                trim_newline(param);
                if (strlen(param) == 0) {
                    printf("参数名不能为空。\n");
                    break;
                }
                printf("请输入新值: ");
                if (!fgets(value, sizeof(value), stdin)) {
                    break;
                }
                trim_newline(value);
                if (strlen(value) == 0) {
                    printf("值不能为空。\n");
                    break;
                }
                client_set_param(sockfd, param, value);
                break;
            }
            case 3:
                client_save_to_file(sockfd);
                break;
            case 4:
                running = 0;
                break;
            default:
                printf("无效选择，请输入 1-4。\n");
                break;
        }
    }

    if (sockfd >= 0) {
        close(sockfd);
    }

    printf("客户端退出。\n");
    return 0;
}


