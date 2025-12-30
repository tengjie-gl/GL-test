#include "ssdp.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <sys/select.h>

static int ssdp_fd = -1;
static volatile int ssdp_running = 0;
static pthread_t ssdp_thread_id;

// 获取本地IP地址
int get_local_ip(char *ip, size_t ip_size) {
    struct ifaddrs *ifaddrs_ptr, *ifa;
    int found = 0;

    if (getifaddrs(&ifaddrs_ptr) == -1) {
        return -1;
    }

    for (ifa = ifaddrs_ptr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;

        // 只查找IPv4地址，跳过回环接口
        if (ifa->ifa_addr->sa_family == AF_INET && 
            !(ifa->ifa_flags & IFF_LOOPBACK)) {
            struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;
            const char *addr = inet_ntoa(sin->sin_addr);
            if (addr) {
                strncpy(ip, addr, ip_size - 1);
                ip[ip_size - 1] = '\0';
                found = 1;
                break;
            }
        }
    }

    freeifaddrs(ifaddrs_ptr);
    return found ? 0 : -1;
}

// 初始化SSDP
int ssdp_init(void) {
    struct sockaddr_in addr;
    struct ip_mreq mreq;
    int opt = 1;

    // 创建UDP socket
    ssdp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ssdp_fd < 0) {
        perror("SSDP socket创建失败");
        return -1;
    }

    // 设置socket选项
    if (setsockopt(ssdp_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("SSDP setsockopt SO_REUSEADDR失败");
        close(ssdp_fd);
        ssdp_fd = -1;
        return -1;
    }

    // 绑定地址
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(SSDP_PORT);

    if (bind(ssdp_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("SSDP bind失败");
        close(ssdp_fd);
        ssdp_fd = -1;
        return -1;
    }

    // 加入多播组
    mreq.imr_multiaddr.s_addr = inet_addr(SSDP_MULTICAST_ADDR);
    mreq.imr_interface.s_addr = INADDR_ANY;

    if (setsockopt(ssdp_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("SSDP 加入多播组失败");
        close(ssdp_fd);
        ssdp_fd = -1;
        return -1;
    }

    // 设置TTL
    int ttl = SSDP_MULTICAST_TTL;
    if (setsockopt(ssdp_fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
        perror("SSDP 设置TTL失败");
    }

    printf("SSDP服务初始化成功\n");
    return 0;
}

// SSDP线程
void* ssdp_thread(void *arg) {
    char local_ip[64];
    if (get_local_ip(local_ip, sizeof(local_ip)) != 0) {
        strcpy(local_ip, "127.0.0.1");
    }

    struct sockaddr_in multicast_addr;
    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_addr.s_addr = inet_addr(SSDP_MULTICAST_ADDR);
    multicast_addr.sin_port = htons(SSDP_PORT);

    // NOTIFY消息（周期性发送，告知设备存在）
    char notify_msg[1024];
    snprintf(notify_msg, sizeof(notify_msg),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: %s:%d\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:8888/\r\n"
        "NT: urn:schemas-upnp-org:device:MT6000ConfigServer:1\r\n"
        "NTS: ssdp:alive\r\n"
        "SERVER: MT6000E/1.0 UPnP/1.0\r\n"
        "USN: uuid:MT6000-Config-Server-1::urn:schemas-upnp-org:device:MT6000ConfigServer:1\r\n"
        "\r\n",
        SSDP_MULTICAST_ADDR, SSDP_PORT, local_ip);

    // M-SEARCH响应消息
    char search_response[1024];

    char buffer[2048];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    time_t last_notify = 0;
    const time_t notify_interval = 30; // 每30秒发送一次NOTIFY

    printf("SSDP服务已启动，服务器IP: %s\n", local_ip);

    while (ssdp_running) {
        fd_set read_fds;
        struct timeval timeout;
        int ret;

        FD_ZERO(&read_fds);
        FD_SET(ssdp_fd, &read_fds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        ret = select(ssdp_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (ret > 0 && FD_ISSET(ssdp_fd, &read_fds)) {
            // 接收M-SEARCH请求
            ssize_t n = recvfrom(ssdp_fd, buffer, sizeof(buffer) - 1, 0,
                                (struct sockaddr *)&client_addr, &client_len);
            if (n > 0) {
                buffer[n] = '\0';
                
                // 检查是否是M-SEARCH请求
                if (strstr(buffer, "M-SEARCH") != NULL) {
                    // 获取客户端IP
                    char client_ip[64];
                    strncpy(client_ip, inet_ntoa(client_addr.sin_addr), sizeof(client_ip) - 1);
                    
                    snprintf(search_response, sizeof(search_response),
                        "HTTP/1.1 200 OK\r\n"
                        "CACHE-CONTROL: max-age=1800\r\n"
                        "DATE: \r\n"
                        "EXT:\r\n"
                        "LOCATION: http://%s:8888/\r\n"
                        "SERVER: MT6000E/1.0 UPnP/1.0\r\n"
                        "ST: urn:schemas-upnp-org:device:MT6000ConfigServer:1\r\n"
                        "USN: uuid:MT6000-Config-Server-1::urn:schemas-upnp-org:device:MT6000ConfigServer:1\r\n"
                        "\r\n",
                        local_ip);

                    // 发送响应到客户端
                    sendto(ssdp_fd, search_response, strlen(search_response), 0,
                           (struct sockaddr *)&client_addr, client_len);
                    
                    printf("SSDP: 响应M-SEARCH请求来自 %s\n", client_ip);
                }
            }
        }

        // 周期性发送NOTIFY消息
        time_t now = time(NULL);
        if (now - last_notify >= notify_interval) {
            sendto(ssdp_fd, notify_msg, strlen(notify_msg), 0,
                   (struct sockaddr *)&multicast_addr, sizeof(multicast_addr));
            last_notify = now;
            printf("SSDP: 发送NOTIFY消息\n");
        }
    }

    printf("SSDP服务已停止\n");
    return NULL;
}

// 启动SSDP服务
void ssdp_start(void) {
    if (ssdp_running) {
        return;
    }

    if (ssdp_init() != 0) {
        fprintf(stderr, "SSDP初始化失败\n");
        return;
    }

    ssdp_running = 1;
    if (pthread_create(&ssdp_thread_id, NULL, ssdp_thread, NULL) != 0) {
        perror("SSDP线程创建失败");
        ssdp_running = 0;
        close(ssdp_fd);
        ssdp_fd = -1;
    }
}

// 停止SSDP服务
void ssdp_stop(void) {
    if (!ssdp_running) {
        return;
    }

    ssdp_running = 0;
    pthread_join(ssdp_thread_id, NULL);

    if (ssdp_fd >= 0) {
        close(ssdp_fd);
        ssdp_fd = -1;
    }
}

