#include "server.h"
#include "ssdp.h"
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>

// 初始化配置文件
// 注意：根据需求，配置文件应该从mt3600be复制到mt6000
// 但实际的文件复制操作需要在部署时手动完成，或者在系统初始化时完成
// 这里只确保配置文件存在，如果不存在则创建空文件
int init_config_file(void) {
    const char *config_file = "/etc/wireless/mediatek/mt7993.b0.dat";
    
    // 确保目录存在
    system("mkdir -p /etc/wireless/mediatek");
    
    // 检查文件是否存在
    struct stat st;
    if (stat(config_file, &st) != 0) {
        printf("配置文件 %s 不存在，创建空文件\n", config_file);
        // 创建空文件（包含Default标记）
        FILE *fp = fopen(config_file, "w");
        if (fp) {
            fprintf(fp, "Default\n");
            fclose(fp);
        } else {
            fprintf(stderr, "警告: 无法创建配置文件 %s\n", config_file);
        }
    } else {
        printf("配置文件已存在: %s\n", config_file);
    }

    return 0;
}

int main(int argc, char *argv[]) {
    int port = SERVER_PORT;
    
    // 解析命令行参数
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "无效的端口号: %s\n", argv[1]);
            return 1;
        }
    }

    // 注册信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // 初始化配置文件
    printf("初始化配置文件...\n");
    init_config_file();

    // 创建配置管理器
    g_config_manager = config_manager_create();
    if (!g_config_manager) {
        fprintf(stderr, "无法创建配置管理器\n");
        return 1;
    }

    // 加载配置文件
    const char *config_file = "/etc/wireless/mediatek/mt7993.b0.dat";
    printf("加载配置文件: %s\n", config_file);
    if (config_load_from_file(g_config_manager, config_file) != 0) {
        fprintf(stderr, "加载配置文件失败\n");
        config_manager_destroy(g_config_manager);
        return 1;
    }

    // 初始化服务器
    int server_fd = server_init(port);
    if (server_fd < 0) {
        fprintf(stderr, "服务器初始化失败\n");
        config_manager_destroy(g_config_manager);
        return 1;
    }

    // 启动SSDP发现服务
    printf("启动SSDP发现服务...\n");
    ssdp_start();

    // 运行服务器
    server_run(server_fd);

    // 清理
    printf("正在清理资源...\n");
    ssdp_stop();
    close(server_fd);
    config_manager_destroy(g_config_manager);

    printf("服务器已关闭\n");
    return 0;
}
