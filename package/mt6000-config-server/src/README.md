# MT6000 WiFi配置服务器

这是一个运行在MT6000E路由器（OpenWrt系统）上的WiFi配置管理服务器，用于读取、修改和保存WiFi配置文件。

## 功能特性

1. **配置文件管理**
   - 启动时自动从 `/etc/wireless/mediatek/mt7993.b0.dat` 读取配置
   - 支持键值对格式的配置项（如 `SSID4=GL-BE10000-6d8-MLO-Guest`）
   - 保留文件第一行的"Default"标记（如果存在）
   - 将修改后的配置保存到 `/tmp/etc/wireless/mediatek/mt7993.b0.dat`

2. **网络服务**
   - TCP服务器，默认监听端口 8888
   - 支持多客户端并发连接
   - 使用线程处理每个客户端请求

3. **SSDP设备发现**
   - 实现SSDP协议，客户端可以自动发现服务器
   - 周期性发送NOTIFY消息
   - 响应M-SEARCH请求

4. **协议命令**
   - `GET <key>` - 获取指定配置项的值
   - `SET <key> <value>` - 设置配置项的值
   - `SAVE` - 保存配置到文件
   - `EXIT` - 断开客户端连接

## 编译

### 本地编译（测试用）

```bash
make
```

### OpenWrt交叉编译

1. 在OpenWrt SDK环境中，设置交叉编译工具链：

```bash
export CC=mips-openwrt-linux-gcc  # 根据实际架构调整
make openwrt-build
```

2. 或者使用OpenWrt的标准Makefile系统（需要在OpenWrt package目录下创建）

## 安装

1. 编译完成后，在 OpenWrt SDK 的`/bin/packages/<架构>/utils`目录下找到生成的 ipk 包

2. 使用scp将 ipk 包上传到路由器：
`scp mt6000-config-server_1.0-1_<架构>.ipk root@<路由器IP>:/tmp`

3. 在路由器上安装：
`opkg install /tmp/mt6000-config-server_1.0-1_<架构>.ipk`

4. 启用并启动服务：
`/etc/init.d/mt6000-config-server enable`
`/etc/init.d/mt6000-config-server start`

## 使用方法

### 启动服务器

```bash
# 使用默认端口8888
./mt6000-config-server

# 指定端口
./mt6000-config-server 9999
```

### 客户端连接示例

使用 `telnet` 或 `nc` (netcat) 连接：

```bash
telnet <路由器IP> 8888
```

或使用 `nc`：

```bash
nc <路由器IP> 8888
```

### 命令示例

连接成功后，可以发送以下命令：

```
# 获取SSID4的值
GET SSID4
# 响应: OK: SSID4=GL-BE10000-6d8-MLO-Guest

# 修改SSID4的值
SET SSID4 MyNewWiFiName
# 响应: OK: SSID4=MyNewWiFiName

# 保存配置到文件
SAVE
# 响应: OK: 配置已保存到 /tmp/etc/wireless/mediatek/mt7993.b0.dat

# 断开连接
EXIT
```

### 编程示例（虚拟机客户端）

#### C语言客户端示例（虚拟机中运行）

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BUFFER_SIZE 2048

int main(int argc, char *argv[]) {
    const char *server_ip = "192.168.1.100";  // 虚拟机的IP地址
    int server_port = 8888;
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    
    // 创建socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket创建失败");
        return 1;
    }
    
    // 设置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "无效的IP地址: %s\n", server_ip);
        close(sock);
        return 1;
    }
    
    // 连接到服务器
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("连接失败");
        close(sock);
        return 1;
    }
    
    printf("已连接到服务器 %s:%d\n", server_ip, server_port);
    
    // 发送GET命令
    snprintf(buffer, sizeof(buffer), "GET SSID4\n");
    send(sock, buffer, strlen(buffer), 0);
    recv(sock, buffer, sizeof(buffer) - 1, 0);
    printf("响应: %s", buffer);
    
    // 发送SET命令
    snprintf(buffer, sizeof(buffer), "SET SSID4 MyNewSSID\n");
    send(sock, buffer, strlen(buffer), 0);
    recv(sock, buffer, sizeof(buffer) - 1, 0);
    printf("响应: %s", buffer);
    
    // 发送SAVE命令
    snprintf(buffer, sizeof(buffer), "SAVE\n");
    send(sock, buffer, strlen(buffer), 0);
    recv(sock, buffer, sizeof(buffer) - 1, 0);
    printf("响应: %s", buffer);
    
    // 关闭连接
    close(sock);
    printf("连接已关闭\n");
    
    return 0;
}
```

## SSDP发现协议

服务器实现了SSDP（Simple Service Discovery Protocol），客户端可以通过以下方式发现服务器：

### M-SEARCH请求示例

```bash
echo -e "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nST: urn:schemas-upnp-org:device:MT6000ConfigServer:1\r\n\r\n" | nc -u 239.255.255.250 1900
```

服务器会响应包含 `LOCATION` 头的HTTP响应，其中包含服务器的IP地址和端口。

## 配置文件格式

配置文件 `/etc/wireless/mediatek/mt7993.b0.dat` 格式为键值对，每行一个配置项：

```
Default
SSID1=MyWiFi1
SSID2=MyWiFi2
SSID3=MyWiFi3
SSID4=GL-BE10000-6d8-MLO-Guest
```

- 第一行的 `Default` 标记会被保留（如果存在）
- 以 `#` 开头的行被视为注释，会被忽略
- 空行会被忽略
- 键值对使用 `=` 分隔
- 键和值的前后空白会被自动去除
- 值可以为空（如 `AccessControlList0=`）

## 注意事项

1. 服务器启动时会尝试从源位置复制配置文件，如果源文件不存在，将使用空配置
2. 修改配置后需要调用 `SAVE` 命令才会保存到文件
3. 保存的文件路径为 `/tmp/etc/wireless/mediatek/mt7993.b0.dat`（需要确保目录存在）
4. 文件第一行的"Default"标记会被自动保留
4. 服务器以多线程方式运行，支持多个客户端同时连接
5. 使用 `Ctrl+C` 或发送 `SIGTERM` 信号可以安全关闭服务器

## 故障排除

### 端口被占用

如果默认端口8888被占用，可以使用其他端口：

```bash
./mt6000-config-server 9999
```

### 权限问题

确保可执行文件有执行权限：

```bash
chmod +x mt6000-config-server
```

### 配置文件不存在

如果配置文件不存在，服务器会尝试创建空文件，但需要确保目录存在且有写入权限。

## 许可证

本项目为内部使用，请根据实际情况添加许可证信息。

