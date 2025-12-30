#include "config_parser.h"
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// 创建配置管理器
config_manager_t* config_manager_create(void) {
    config_manager_t *manager = (config_manager_t *)malloc(sizeof(config_manager_t));
    if (manager) {
        manager->head = NULL;
        manager->count = 0;
        manager->has_default = 0;
    }
    return manager;
}

// 销毁配置管理器
void config_manager_destroy(config_manager_t *manager) {
    if (!manager) return;
    
    config_item_t *current = manager->head;
    while (current) {
        config_item_t *next = current->next;
        free(current);
        current = next;
    }
    free(manager);
}

// 从文件加载配置
int config_load_from_file(config_manager_t *manager, const char *filename) {
    if (!manager || !filename) {
        return -1;
    }

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "无法打开文件 %s: %s\n", filename, strerror(errno));
        return -1;
    }

    char line[MAX_LINE_LEN];
    int line_num = 0;
    int first_line = 1;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        
        // 去除换行符
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
            len--;
        }
        if (len > 0 && line[len - 1] == '\r') {
            line[len - 1] = '\0';
            len--;
        }

        // 跳过空行
        if (len == 0) {
            continue;
        }

        // 跳过注释行（以#开头）
        if (line[0] == '#') {
            continue;
        }

        // 检查第一行是否是"Default"（没有等号，且内容就是"Default"）
        if (first_line && strcmp(line, "Default") == 0) {
            manager->has_default = 1;
            first_line = 0;
            continue; // 保存标记，但不作为键值对存储
        }

        first_line = 0;

        // 查找等号
        char *equal_sign = strchr(line, '=');
        if (!equal_sign) {
            continue; // 没有等号，跳过（除了第一行的"Default"）
        }

        // 分割键值对
        *equal_sign = '\0';
        char *key = line;
        char *value = equal_sign + 1;

        // 去除键的前后空白
        while (*key == ' ' || *key == '\t') key++;
        char *key_end = key + strlen(key) - 1;
        while (key_end > key && (*key_end == ' ' || *key_end == '\t')) {
            *key_end = '\0';
            key_end--;
        }

        // 去除值的前后空白
        while (*value == ' ' || *value == '\t') value++;
        char *value_end = value + strlen(value) - 1;
        while (value_end > value && (*value_end == ' ' || *value_end == '\t')) {
            *value_end = '\0';
            value_end--;
        }

        // 检查键和值是否为空
        if (strlen(key) == 0) {
            continue;
        }

        // 创建配置项
        config_item_t *item = (config_item_t *)malloc(sizeof(config_item_t));
        if (!item) {
            fprintf(stderr, "内存分配失败\n");
            fclose(fp);
            return -1;
        }

        strncpy(item->key, key, MAX_KEY_LEN - 1);
        item->key[MAX_KEY_LEN - 1] = '\0';
        strncpy(item->value, value, MAX_VALUE_LEN - 1);
        item->value[MAX_VALUE_LEN - 1] = '\0';
        item->next = manager->head;
        manager->head = item;
        manager->count++;
    }

    fclose(fp);
    printf("成功加载 %d 个配置项\n", manager->count);
    return 0;
}

// 保存配置到文件
int config_save_to_file(config_manager_t *manager, const char *filename) {
    if (!manager || !filename) {
        return -1;
    }

    // 创建目录（如果不存在）
    char dir_path[512];
    strncpy(dir_path, filename, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';
    
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        // 递归创建目录
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "mkdir -p %s", dir_path);
        system(cmd);
    }

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "无法创建文件 %s: %s\n", filename, strerror(errno));
        return -1;
    }

    // 如果有"Default"标记，先写入"Default"这一行
    if (manager->has_default) {
        fprintf(fp, "Default\n");
    }

    // 遍历链表并写入文件（反向遍历以保持原始顺序）
    // 由于新项添加到头部，需要反转顺序来保持原始文件顺序
    if (manager->count > 0) {
        config_item_t **items = (config_item_t **)malloc(sizeof(config_item_t *) * manager->count);
        if (!items) {
            fprintf(stderr, "内存分配失败\n");
            fclose(fp);
            return -1;
        }

        config_item_t *current = manager->head;
        int idx = manager->count - 1;
        while (current && idx >= 0) {
            items[idx] = current;
            current = current->next;
            idx--;
        }

        for (int i = 0; i < manager->count; i++) {
            fprintf(fp, "%s=%s\n", items[i]->key, items[i]->value);
        }

        free(items);
    }

    fclose(fp);
    printf("成功保存 %d 个配置项到 %s\n", manager->count, filename);
    return 0;
}

// 获取配置值
int config_get_value(config_manager_t *manager, const char *key, char *value, size_t value_size) {
    if (!manager || !key || !value) {
        return -1;
    }

    config_item_t *current = manager->head;
    while (current) {
        if (strcmp(current->key, key) == 0) {
            strncpy(value, current->value, value_size - 1);
            value[value_size - 1] = '\0';
            return 0;
        }
        current = current->next;
    }

    return -1; // 未找到
}

// 设置配置值（如果不存在则创建）
int config_set_value(config_manager_t *manager, const char *key, const char *value) {
    if (!manager || !key || !value) {
        return -1;
    }

    // 先查找是否已存在
    config_item_t *current = manager->head;
    while (current) {
        if (strcmp(current->key, key) == 0) {
            // 更新现有值
            strncpy(current->value, value, MAX_VALUE_LEN - 1);
            current->value[MAX_VALUE_LEN - 1] = '\0';
            return 0;
        }
        current = current->next;
    }

    // 不存在，创建新项
    config_item_t *item = (config_item_t *)malloc(sizeof(config_item_t));
    if (!item) {
        return -1;
    }

    strncpy(item->key, key, MAX_KEY_LEN - 1);
    item->key[MAX_KEY_LEN - 1] = '\0';
    strncpy(item->value, value, MAX_VALUE_LEN - 1);
    item->value[MAX_VALUE_LEN - 1] = '\0';
    item->next = manager->head;
    manager->head = item;
    manager->count++;

    return 0;
}

// 打印所有配置（调试用）
void config_print_all(config_manager_t *manager) {
    if (!manager) return;

    config_item_t *current = manager->head;
    while (current) {
        printf("%s=%s\n", current->key, current->value);
        current = current->next;
    }
}
