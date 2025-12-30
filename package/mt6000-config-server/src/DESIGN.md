# MT6000 WiFi配置服务器设计文档

## 1. 系统概述

### 1.1 项目背景
MT6000 WiFi配置服务器是一个运行在MT6000E路由器（OpenWrt系统）上的网络服务程序，用于管理WiFi配置文件。该服务器通过TCP/IP协议提供配置管理服务，支持客户端远程查询、修改和保存WiFi配置参数。

### 1.2 系统目标
- 提供远程配置管理接口
- 支持多客户端并发访问
- 保持配置文件格式完整性（保留"Default"标记）
- 提供设备自动发现功能（SSDP协议）

### 1.3 技术栈
- **编程语言**: C语言
- **操作系统**: Linux (OpenWrt)
- **网络协议**: TCP/IP, SSDP (UPnP)
- **并发模型**: POSIX Threads
- **编译系统**: Make

## 2. 系统架构

### 2.1 整体架构

```
┌─────────────────────────────────────────────────────────┐
│                    MT6000 Config Server                  │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  ┌──────────────┐      ┌──────────────┐                │
│  │  Main Module │      │  SSDP Module │                │
│  │   (main.c)   │      │   (ssdp.c)   │                │
│  └──────┬───────┘      └──────────────┘                │
│         │                                               │
│         ▼                                               │
│  ┌──────────────────────────────────────┐              │
│  │        Server Module (server.c)       │              │
│  │  ┌────────────────────────────────┐  │              │
│  │  │   TCP Socket Server            │  │              │
│  │  │   - Listens on port 8888       │  │              │
│  │  │   - Multi-threaded client      │  │              │
│  │  │     handling                   │  │              │
│  │  └──────────────┬─────────────────┘  │              │
│  │                 │                     │              │
│  │  ┌──────────────▼──────────────┐     │              │
│  │  │   Command Processor         │     │              │
│  │  │   - GET/SET/SAVE/EXIT       │     │              │
│  │  └──────────────┬──────────────┘     │              │
│  └─────────────────┼─────────────────────┘              │
│                    │                                     │
│                    ▼                                     │
│         ┌──────────────────────┐                        │
│         │  Config Parser       │                        │
│         │    (config_parser.c) │                        │
│         │  - File I/O          │                        │
│         │  - Key-Value Store   │                        │
│         │  - Default marker    │                        │
│         └──────────────────────┘                        │
│                    │                                     │
│                    ▼                                     │
│         /etc/wireless/mediatek/mt7993.b0.dat            │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

### 2.2 模块划分

系统采用模块化设计，主要包含以下模块：

1. **主程序模块 (main.c)**
   - 程序入口
   - 系统初始化
   - 信号处理
   - 生命周期管理

2. **服务器模块 (server.c/h)**
   - TCP服务器实现
   - 客户端连接管理
   - 命令处理
   - 协议实现

3. **配置解析模块 (config_parser.c/h)**
   - 配置文件读取
   - 配置文件保存
   - 键值对管理
   - "Default"标记处理

4. **SSDP模块 (ssdp.c/h)**
   - SSDP协议实现
   - 设备发现服务
   - 多播通信

## 3. 数据结构设计

### 3.1 配置项节点

```c
typedef struct config_item {
    char key[MAX_KEY_LEN];      // 配置键（最大256字节）
    char value[MAX_VALUE_LEN];  // 配置值（最大512字节）
    struct config_item *next;   // 链表指针
} config_item_t;
```

**设计说明**:
- 使用链表存储配置项，便于动态增减
- 键值对固定大小，避免内存碎片
- 支持字符串键值对格式

### 3.2 配置管理器

```c
typedef struct {
    config_item_t *head;     // 链表头指针
    int count;               // 配置项数量
    int has_default;         // 是否包含"Default"标记
} config_manager_t;
```

**设计说明**:
- 使用链表管理所有配置项
- `has_default`标志确保"Default"行在保存时被正确写入第一行
- 单例模式，全局唯一实例

### 3.3 客户端信息

```c
typedef struct {
    int fd;                      // 客户端socket文件描述符
    struct sockaddr_in addr;     // 客户端地址信息
} client_info_t;
```

**设计说明**:
- 每个客户端连接对应一个实例
- 在独立线程中处理，避免阻塞主服务器

## 4. 接口设计

### 4.1 配置解析模块接口

#### 4.1.1 配置管理器生命周期

```c
// 创建配置管理器
config_manager_t* config_manager_create(void);

// 销毁配置管理器（释放所有资源）
void config_manager_destroy(config_manager_t *manager);
```

**config_manager_create**
- **功能**: 创建配置管理器实例
- **返回值**: 
  - 成功：返回配置管理器指针
  - 失败：返回NULL

**config_manager_destroy**
- **功能**: 销毁配置管理器，释放所有资源
- **参数**: 
  - `manager`: 配置管理器指针

---

#### 4.1.2 文件操作接口

```c
// 从文件加载配置
// 返回值: 0成功, -1失败
int config_load_from_file(config_manager_t *manager, const char *filename);

// 保存配置到文件
// 返回值: 0成功, -1失败
int config_save_to_file(config_manager_t *manager, const char *filename);
```

**config_load_from_file**
- **功能**: 从文件加载配置
- **参数**: 
  - `manager`: 配置管理器指针
  - `filename`: 配置文件路径
- **返回值**: 
  - 成功：返回0
  - 失败：返回-1
- **说明**: 会自动检测并保留第一行的"Default"标记

**config_save_to_file**
- **功能**: 保存配置到文件
- **参数**: 
  - `manager`: 配置管理器指针
  - `filename`: 配置文件路径（如果目录不存在会自动创建）
- **返回值**: 
  - 成功：返回0
  - 失败：返回-1
- **说明**: 如果存在"Default"标记，会将其写入第一行

---

#### 4.1.3 配置项操作接口

```c
// 获取配置值
// 返回值: 0成功, -1未找到
int config_get_value(config_manager_t *manager, const char *key, 
                     char *value, size_t value_size);

// 设置配置值（不存在则创建）
// 返回值: 0成功, -1失败
int config_set_value(config_manager_t *manager, const char *key, 
                     const char *value);

// 打印所有配置（调试用）
void config_print_all(config_manager_t *manager);
```

**config_get_value**
- **功能**: 获取指定配置项的值
- **参数**: 
  - `manager`: 配置管理器指针
  - `key`: 配置键名
  - `value`: 输出缓冲区，用于存储配置值
  - `value_size`: 缓冲区大小
- **返回值**: 
  - 成功：返回0
  - 未找到：返回-1

**config_set_value**
- **功能**: 设置配置项的值（如果不存在则创建）
- **参数**: 
  - `manager`: 配置管理器指针
  - `key`: 配置键名
  - `value`: 配置值
- **返回值**: 
  - 成功：返回0
  - 失败：返回-1

**config_print_all**
- **功能**: 打印所有配置项（调试用）
- **参数**: 
  - `manager`: 配置管理器指针

---

### 4.2 服务器模块接口

```c
// 初始化TCP服务器
// 参数: port - 监听端口
// 返回值: 服务器socket描述符，失败返回-1
int server_init(int port);

// 运行服务器（阻塞）
// 参数: server_fd - 服务器socket描述符
void server_run(int server_fd);

// 处理客户端连接（线程函数）
void* handle_client(void *arg);

// 处理命令
// 参数: cmd - 命令字符串
//       response - 响应缓冲区
//       response_size - 缓冲区大小
// 返回值: 0正常, 1客户端应断开, -1错误
int process_command(const char *cmd, char *response, size_t response_size);

// 信号处理函数
void signal_handler(int sig);
```
**server_init**
- **功能**: 初始化TCP服务器
- **参数**: 
  - `port`: 监听端口号
- **返回值**: 
  - 成功：返回服务器socket文件描述符（>= 0）
  - 失败：返回-1
- **说明**: 创建socket，绑定端口，开始监听

**server_run**
- **功能**: 运行服务器主循环（阻塞）
- **参数**: 
  - `server_fd`: 服务器socket文件描述符
- **说明**: 接受客户端连接，为每个客户端创建独立线程处理

**handle_client**
- **功能**: 处理客户端连接（线程函数）
- **参数**: 
  - `arg`: 指向`client_info_t`结构体的指针，包含客户端socket和地址信息
- **返回值**: 线程函数返回值（始终为NULL）
- **说明**: 
  - 接收客户端命令
  - 调用`process_command`处理命令
  - 发送响应
  - 处理连接断开

**process_command**
- **功能**: 处理客户端命令
- **参数**: 
  - `cmd`: 命令字符串
  - `response`: 响应缓冲区
  - `response_size`: 缓冲区大小
- **返回值**: 
  - 正常处理：返回0
  - 客户端应断开（EXIT命令）：返回1
  - 错误：返回-1
- **支持的命令**:
  - `GET <key>` - 获取配置值
  - `SET <key> <value>` - 设置配置值
  - `SAVE` - 保存配置到文件
  - `EXIT` - 断开连接

**signal_handler**
- **功能**: 信号处理函数
- **参数**: 
  - `sig`: 信号编号
- **说明**: 处理SIGINT和SIGTERM信号，设置`g_running`标志为0以优雅关闭服务器

**常量定义**:
```c
#define SERVER_PORT 8888          // 默认端口
#define BUFFER_SIZE 2048          // 缓冲区大小
#define CMD_GET "GET"             // GET命令
#define CMD_SET "SET"             // SET命令
#define CMD_SAVE "SAVE"           // SAVE命令
#define CMD_EXIT "EXIT"           // EXIT命令
#define RESP_OK "OK"              // 成功响应
#define RESP_ERROR "ERROR"        // 错误响应
#define RESP_NOT_FOUND "NOT_FOUND" // 未找到响应
```

**全局变量**:
```c
extern config_manager_t *g_config_manager;  // 全局配置管理器
```

---

### 4.3 SSDP模块接口

```c
// 初始化SSDP服务
// 返回值: 0成功, -1失败
int ssdp_init(void);

// 启动SSDP服务（创建后台线程）
void ssdp_start(void);

// 停止SSDP服务
void ssdp_stop(void);
```
**ssdp_init**
- **功能**: 初始化SSDP服务
- **返回值**: 
  - 成功：返回0
  - 失败：返回-1
- **说明**: 创建UDP socket，绑定端口，加入多播组

**ssdp_start**
- **功能**: 启动SSDP服务
- **说明**: 
  - 调用`ssdp_init`初始化
  - 创建后台线程运行`ssdp_thread`
  - 周期性发送NOTIFY消息
  - 响应M-SEARCH请求

**ssdp_stop**
- **功能**: 停止SSDP服务
- **说明**: 停止后台线程，关闭socket

**常量定义**:
```c
#define SSDP_MULTICAST_ADDR "239.255.255.250"  // SSDP多播地址
#define SSDP_PORT 1900                         // SSDP端口
#define SSDP_MULTICAST_TTL 4                   // 多播TTL
```

**内部函数**（不对外暴露）:
```c
// SSDP服务线程函数
void* ssdp_thread(void *arg);
```
- **功能**: SSDP服务线程函数
- **参数**: 
  - `arg`: 线程参数（未使用）
- **返回值**: 线程函数返回值（始终为NULL）
- **说明**: 
  - 每30秒发送一次NOTIFY消息
  - 监听并响应M-SEARCH请求
  - 在`ssdp_running`为0时退出

---

### 4.4 主程序模块接口

```c
// 初始化配置文件
// 返回值: 始终返回0
int init_config_file(void);

// 程序入口
// 参数: argc - 参数个数
//       argv - 参数数组，argv[1]可选指定服务器端口号（默认8888）
// 返回值: 成功返回0，失败返回1
int main(int argc, char *argv[]);
```

**init_config_file**
- **功能**: 初始化配置文件
- **返回值**: 
  - 始终返回0
- **说明**: 
  - 确保配置文件目录存在
  - 如果配置文件不存在，创建包含"Default"标记的空文件
  - 配置文件路径：`/etc/wireless/mediatek/mt7993.b0.dat`

**main**
- **功能**: 程序入口
- **参数**: 
  - `argc`: 参数个数
  - `argv`: 参数数组
    - `argv[1]`: 可选，指定服务器端口号（默认8888）
- **返回值**: 
  - 成功：返回0
  - 失败：返回1
- **执行流程**:
  1. 解析命令行参数
  2. 注册信号处理器（SIGINT, SIGTERM）
  3. 初始化配置文件
  4. 创建配置管理器
  5. 加载配置文件
  6. 初始化TCP服务器
  7. 启动SSDP服务
  8. 运行服务器主循环
  9. 清理资源并退出

---

## 5. 通信协议设计

### 5.1 TCP协议

#### 5.1.1 连接方式
- **协议**: TCP/IP
- **默认端口**: 8888
- **编码**: ASCII文本，每行一个命令
- **行结束符**: `\n` 或 `\r\n`

#### 5.1.2 命令格式

| 命令 | 格式 | 说明 | 示例 |
|------|------|------|------|
| GET | `GET <key>` | 获取配置值 | `GET SSID4` |
| SET | `SET <key> <value>` | 设置配置值 | `SET SSID4 MyWiFi` |
| SAVE | `SAVE` | 保存配置到文件 | `SAVE` |
| EXIT | `EXIT` | 断开连接 | `EXIT` |

#### 5.1.3 响应格式

**成功响应**:
```
OK: <key>=<value>
```

**错误响应**:
```
ERROR: <错误描述>
NOT_FOUND: 键 '<key>' 不存在
```

**示例**:
```
客户端: GET SSID4
服务器: OK: SSID4=GL-BE10000-6d8-MLO-Guest

客户端: SET SSID4 MyNewSSID
服务器: OK: SSID4=MyNewSSID

客户端: GET NonExistent
服务器: NOT_FOUND: 键 'NonExistent' 不存在

客户端: SAVE
服务器: OK: 配置已保存到 /tmp/etc/wireless/mediatek/mt7993.b0.dat
```

### 5.2 SSDP协议

#### 5.2.1 多播地址
- **地址**: 239.255.255.250
- **端口**: 1900
- **协议**: UDP

#### 5.2.2 NOTIFY消息

服务器每30秒发送一次NOTIFY消息：

```
NOTIFY * HTTP/1.1
HOST: 239.255.255.250:1900
CACHE-CONTROL: max-age=1800
LOCATION: http://<server_ip>:8888/
NT: urn:schemas-upnp-org:device:MT6000ConfigServer:1
NTS: ssdp:alive
SERVER: MT6000E/1.0 UPnP/1.0
USN: uuid:MT6000-Config-Server-1::urn:schemas-upnp-org:device:MT6000ConfigServer:1
```

#### 5.2.3 M-SEARCH响应

客户端发送M-SEARCH请求后，服务器响应：

```
HTTP/1.1 200 OK
CACHE-CONTROL: max-age=1800
DATE: 
EXT:
LOCATION: http://<server_ip>:8888/
SERVER: MT6000E/1.0 UPnP/1.0
ST: urn:schemas-upnp-org:device:MT6000ConfigServer:1
USN: uuid:MT6000-Config-Server-1::urn:schemas-upnp-org:device:MT6000ConfigServer:1
```

## 6. 数据流设计

### 6.1 服务器启动流程

```
main()
  ├─ 解析命令行参数
  ├─ 注册信号处理器
  ├─ 初始化配置文件
  │   └─ 检查/创建配置文件
  ├─ 创建配置管理器
  ├─ 加载配置文件
  │   └─ 解析键值对
  │   └─ 检测"Default"标记
  ├─ 初始化TCP服务器
  │   └─ 创建socket
  │   └─ 绑定端口
  │   └─ 开始监听
  ├─ 启动SSDP服务
  │   └─ 创建UDP socket
  │   └─ 加入多播组
  │   └─ 创建后台线程
  └─ 进入主循环
      └─ 接受客户端连接
      └─ 为每个客户端创建线程
```

### 6.2 客户端请求处理流程

```
客户端连接
  ├─ 创建客户端线程
  │   └─ handle_client()
  │       ├─ 接收命令
  │       ├─ 解析命令
  │       ├─ process_command()
  │       │   ├─ GET: config_get_value()
  │       │   ├─ SET: config_set_value()
  │       │   ├─ SAVE: config_save_to_file()
  │       │   └─ EXIT: 返回断开标志
  │       ├─ 发送响应
  │       └─ 循环处理下一个命令
  └─ 客户端断开
      └─ 清理资源
```

### 6.3 配置文件读取流程

```
config_load_from_file()
  ├─ 打开文件
  ├─ 逐行读取
  │   ├─ 跳过空行和注释
  │   ├─ 检测"Default"标记（第一行）
  │   ├─ 解析键值对
  │   │   ├─ 查找等号
  │   │   ├─ 分离键和值
  │   │   └─ 去除空白
  │   └─ 添加到链表
  └─ 关闭文件
```

### 6.4 配置文件保存流程

```
config_save_to_file()
  ├─ 创建目录（如不存在）
  ├─ 打开文件（写入模式）
  ├─ 写入"Default"标记（如果存在）
  ├─ 遍历配置项链表
  │   ├─ 反转链表顺序（保持原始顺序）
  │   └─ 写入键值对
  └─ 关闭文件
```

## 7. 错误处理

### 7.1 错误分类

#### 7.1.1 系统级错误
- 文件不存在：创建空文件或返回错误
- 权限不足：返回错误信息
- 内存分配失败：返回错误，避免崩溃
- Socket错误：记录日志，继续运行

#### 7.1.2 协议级错误
- 无效命令：返回`ERROR: 未知命令`
- 参数错误：返回`ERROR: 命令格式错误`
- 键不存在：返回`NOT_FOUND`

#### 7.1.3 业务逻辑错误
- 配置项不存在：返回`NOT_FOUND`
- 保存失败：返回`ERROR: 保存失败`

### 7.2 错误处理策略

1. **资源管理**: 使用RAII原则，确保资源正确释放
2. **信号处理**: 优雅关闭，清理所有资源
3. **线程安全**: 使用互斥锁保护共享资源（如需要）
4. **日志记录**: 记录关键错误和操作

## 8. 性能考虑

### 8.1 内存管理
- 使用固定大小缓冲区，避免动态分配
- 配置项使用链表存储，内存效率高
- 及时释放不使用的资源

### 8.2 并发处理
- 每个客户端使用独立线程
- 使用线程池可优化（未来改进）
- 配置管理器目前为单例，多线程访问需加锁（当前实现中访问是顺序的）

### 8.3 文件I/O
- 读取时一次性加载到内存
- 写入时批量写入，减少I/O次数
- 使用标准C库缓冲区

### 8.4 网络性能
- TCP连接复用（客户端可保持连接）
- 命令响应简洁，减少网络开销
- SSDP使用UDP多播，效率高

## 9. 安全性考虑

### 9.1 当前实现
- 无身份验证机制
- 无访问控制
- 明文传输

### 9.2 未来改进建议
- 添加TLS/SSL加密
- 实现身份认证（用户名/密码或证书）
- 添加访问控制列表（ACL）
- 输入验证和防注入
- 日志审计

## 10. 配置文件格式

### 10.1 文件格式规范

```
Default
Key1=Value1
Key2=Value2
...
KeyN=ValueN
```

### 10.2 格式规则
1. 第一行必须是`Default`（如果存在），必须保留
2. 每行一个键值对，格式：`Key=Value`
3. 键和值之间用等号`=`分隔
4. 值可以为空：`Key=`
5. 支持注释行（以`#`开头）
6. 空行会被忽略
7. 键和值的前后空白会被自动去除

### 10.3 文件路径
- **读取路径**: `/etc/wireless/mediatek/mt7993.b0.dat`
- **保存路径**: `/tmp/etc/wireless/mediatek/mt7993.b0.dat`

## 11. 部署设计

### 11.1 编译
- 使用Makefile编译
- 支持本地编译和交叉编译
- 目标架构：OpenWrt（MIPS或其他架构）

### 11.2 安装
1. 通过 OpenWrt 包管理系统安装
2. 包管理器自动完成：
- 复制可执行文件到 /usr/bin/
- 创建 init 脚本 /etc/init.d/mt6000-config-server
- 设置执行权限
3. 安装后通过/etc/init.d/mt6000-config-server enable启用服务
4. 通过/etc/init.d/mt6000-config-server start启动服务

### 11.3 运行
- 作为系统服务运行（使用procd）
- 自动重启（respawn）
- 日志输出到stdout/stderr

## 12. 测试策略

### 12.1 单元测试
- 配置解析模块测试
- 命令处理测试
- 错误处理测试

### 12.2 集成测试
- 客户端-服务器通信测试
- SSDP发现测试
- 多客户端并发测试

### 12.3 功能测试
- 配置文件读取/保存
- 配置项查询/修改
- "Default"标记保留

## 13. 扩展性设计

### 13.1 可扩展功能
- 支持批量操作（批量GET/SET）
- 支持配置导入/导出
- 支持配置备份/恢复
- 支持配置项验证
- 支持配置模板

### 13.2 协议扩展
- 支持JSON格式
- 支持HTTP RESTful API
- 支持WebSocket

## 14. 依赖关系

### 14.1 系统依赖
- POSIX兼容的系统
- pthread库
- 标准C库

### 14.2 编译依赖
- GCC或兼容编译器
- Make工具
- OpenWrt SDK（交叉编译）

## 15. 版本历史

- **v1.0**: 初始版本
  - 基本配置管理功能
  - TCP服务器
  - SSDP设备发现
  - 支持GET/SET/SAVE/EXIT命令

---

## 附录

### A. 常量定义

| 常量 | 值 | 说明 |
|------|-----|------|
| SERVER_PORT | 8888 | 默认TCP端口 |
| BUFFER_SIZE | 2048 | 缓冲区大小 |
| MAX_KEY_LEN | 256 | 键最大长度 |
| MAX_VALUE_LEN | 512 | 值最大长度 |
| MAX_LINE_LEN | 1024 | 行最大长度 |
| SSDP_MULTICAST_ADDR | 239.255.255.250 | SSDP多播地址 |
| SSDP_PORT | 1900 | SSDP端口 |

### B. 文件清单

```
mt6000-config-server/
├── Makefile                        # OpenWrt 包构建配置
├── files/
│   └── mt6000-config-server.init   # OpenWrt 启动脚本
└── src/
    ├── main.c                      # 主程序入口
    ├── server.c                    # TCP 服务器实现
    ├── server.h                    # 服务器模块头文件
    ├── config_parser.c             # 配置文件解析器实现
    ├── config_parser.h             # 配置解析器头文件
    ├── ssdp.c                      # SSDP 服务实现
    ├── ssdp.h                      # SSDP 模块头文件
    ├── Makefile                    # 本地编译配置
    ├── README.md                   # 用户文档
    └── DESIGN.md                   # 设计文档
```

### C. 参考标准
- RFC 2616: HTTP/1.1
- UPnP Device Architecture 1.0
- SSDP (Simple Service Discovery Protocol)
