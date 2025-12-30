# MT6000配置客户端 - 代码接口文档

## 概述

本文档说明MT6000配置客户端的函数接口。该客户端通过UDP广播发现服务器，然后建立TCP连接进行配置管理操作。

## 常量定义

```c
#define DISCOVERY_PORT 1900                    // UDP发现端口
#define SERVER_TCP_PORT 8888                   // 服务器TCP端口
#define DISCOVERY_MESSAGE "M-SEARCH * ..."     // SSDP发现消息
#define DISCOVERY_RESPONSE_PREFIX "HTTP/1.1 200 OK"  // 服务器响应前缀
#define MAX_BUFFER 1024                        // 缓冲区大小
```

---

## 工具函数

### trim_newline
```c
static void trim_newline(char *s);
```
**功能**: 去除字符串末尾的换行符（`\n` 和 `\r`）

**参数**: 
- `s`: 要处理的字符串指针

**说明**: 如果字符串为NULL，直接返回

---

### set_recv_timeout
```c
static int set_recv_timeout(int sockfd, int seconds);
```
**功能**: 设置socket接收超时时间

**参数**: 
- `sockfd`: socket文件描述符
- `seconds`: 超时秒数

**返回值**: 
- 成功：返回0
- 失败：返回-1

---

## 服务器发现模块

### discover_server_ip
```c
static int discover_server_ip(char *server_ip, size_t ip_buf_len);
```
**功能**: 通过UDP广播发现服务器IP地址

**参数**: 
- `server_ip`: 输出缓冲区，用于存储发现的服务器IP地址（如 "192.168.1.10"）
- `ip_buf_len`: 缓冲区大小

**返回值**: 
- 成功：返回0，并将服务器IP写入`server_ip`
- 失败：返回-1

**说明**: 
- 使用UDP广播到255.255.255.255
- 发送SSDP M-SEARCH消息
- 等待服务器回复（超时时间5秒）
- 验证回复前缀为"HTTP/1.1 200 OK"
- 从回复的源地址获取服务器IP

---

## TCP连接与命令收发模块

### connect_to_server
```c
static int connect_to_server(const char *server_ip, int port);
```
**功能**: 建立TCP连接到服务器

**参数**: 
- `server_ip`: 服务器IP地址字符串
- `port`: 服务器端口号

**返回值**: 
- 成功：返回TCP socket文件描述符（>= 0）
- 失败：返回-1

**说明**: 创建TCP socket并连接到指定服务器的端口

---

### send_command_and_read_line
```c
static int send_command_and_read_line(int sockfd, const char *cmd, char *resp, size_t resp_size);
```
**功能**: 发送一条命令并接收一行响应（以`\n`结束）

**参数**: 
- `sockfd`: TCP socket文件描述符
- `cmd`: 要发送的命令字符串
- `resp`: 响应缓冲区，用于存储服务器回复
- `resp_size`: 响应缓冲区大小

**返回值**: 
- 成功：返回0
- 失败：返回-1

**说明**: 
- 发送命令到服务器
- 循环接收字符直到收到`\n`或缓冲区满
- 响应字符串以`\0`结尾

---

### client_get_param
```c
static int client_get_param(int sockfd, const char *param_name);
```
**功能**: 查询配置参数值

**参数**: 
- `sockfd`: TCP socket文件描述符
- `param_name`: 要查询的参数名（如 "SSID4"）

**返回值**: 
- 成功：返回0
- 失败：返回-1

**说明**: 
- 发送`GET <param_name>\n`命令
- 解析服务器响应
- 响应格式：`OK: <key>=<value>` 或 `ERROR: <reason>`
- 自动打印查询结果

---

### client_set_param
```c
static int client_set_param(int sockfd, const char *param_name, const char *value);
```
**功能**: 修改配置参数值

**参数**: 
- `sockfd`: TCP socket文件描述符
- `param_name`: 要修改的参数名（如 "SSID4"）
- `value`: 新的参数值

**返回值**: 
- 成功：返回0
- 失败：返回-1

**说明**: 
- 发送`SET <param_name> <value>\n`命令
- 解析服务器响应
- 响应格式：`OK: <key>=<value>` 或 `ERROR: <reason>`
- 自动打印设置结果

---

### client_save_to_file
```c
static int client_save_to_file(int sockfd);
```
**功能**: 让服务器将当前所有配置保存到文件

**参数**: 
- `sockfd`: TCP socket文件描述符

**返回值**: 
- 成功：返回0
- 失败：返回-1

**说明**: 
- 发送`SAVE\n`命令
- 解析服务器响应
- 响应格式：`OK: 配置已保存到 ...` 或 `ERROR: <reason>`
- 自动打印保存结果

---

## 主程序模块

### print_menu
```c
static void print_menu(void);
```
**功能**: 打印交互式菜单

**说明**: 显示客户端的操作选项：
1. 查询参数值
2. 修改参数值
3. 让服务器写入当前全部选项到文件
4. 退出

---

### main
```c
int main(int argc, char *argv[]);
```
**功能**: 程序入口

**参数**: 
- `argc`: 参数个数
- `argv`: 参数数组
  - `argv[1]`: 可选，手动指定服务器IP地址（如果提供则跳过UDP发现）

**返回值**: 
- 成功：返回0
- 失败：返回1

**执行流程**:
1. 服务器发现阶段
   - 如果提供了命令行参数，使用指定的IP地址
   - 否则通过UDP广播发现服务器
2. 建立TCP连接
   - 连接到服务器的TCP端口（默认8888）
3. 交互式菜单循环
   - 显示菜单
   - 根据用户选择执行相应操作（GET/SET/SAVE）
   - 直到用户选择退出
4. 清理资源并退出

---

## 协议说明

### 服务器发现协议（UDP）

**客户端发送**（广播到255.255.255.255:1900）:
```
M-SEARCH * HTTP/1.1\r\n
HOST: 239.255.255.250:1900\r\n
MAN: "ssdp:discover"\r\n
MX: 3\r\n
ST: upnp:rootdevice\r\n
\r\n
```

**服务器响应**（UDP）:
```
HTTP/1.1 200 OK
...
```

客户端从响应报文的源地址获取服务器IP。

---

### TCP应用层协议

**命令格式**（每行以`\n`结束）:

| 命令 | 格式 | 说明 |
|------|------|------|
| GET | `GET <param>\n` | 查询参数值 |
| SET | `SET <param> <value>\n` | 设置参数值 |
| SAVE | `SAVE\n` | 保存配置到文件 |

**响应格式**:

- 成功响应：`OK: <信息>\n`
- 错误响应：`ERROR: <错误信息>\n` 或 `NOT_FOUND: <信息>\n`

**示例**:
```
客户端: GET SSID4
服务器: OK: SSID4=GL-BE10000-6d8-MLO-Guest

客户端: SET SSID4 MyNewSSID
服务器: OK: SSID4=MyNewSSID

客户端: SAVE
服务器: OK: 配置已保存到 /tmp/etc/wireless/mediatek/mt7993.b0.dat
```

---

## 使用示例

### 编译
```bash
gcc -o client client.c
```

### 运行

**方式1：自动发现服务器**
```bash
./client
```

**方式2：手动指定服务器IP**
```bash
./client 192.168.1.100
```

### 交互式使用

程序运行后会显示菜单：
```
==== be3600mt 配置客户端 ====
1. 查询参数值（示例：SSID4）
2. 修改参数值（示例：SSID4）
3. 让服务器写入当前全部选项到文件
4. 退出
请选择功能(1-4): 
```

用户输入选项后，根据提示输入参数名和值即可。

---

## 注意事项

1. **网络要求**: 
   - 客户端需要与服务器在同一局域网内
   - UDP广播需要网络支持（桥接模式）
   - 如果无法发现服务器，可以手动指定IP地址

2. **超时设置**: 
   - UDP发现超时时间为5秒
   - 如果5秒内未收到响应，发现失败

3. **协议兼容性**: 
   - 命令和响应格式需要与服务器实现保持一致
   - 服务器需支持SSDP M-SEARCH协议用于发现

4. **错误处理**: 
   - 所有函数返回-1表示失败
   - 错误信息会通过`perror`或`printf`输出到stderr/stdout

5. **线程安全**: 
   - 当前实现为单线程，不支持并发访问
   - 多个客户端可以同时运行，但每个客户端独立连接

