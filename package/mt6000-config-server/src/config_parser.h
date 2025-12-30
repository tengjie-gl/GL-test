#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_KEY_LEN 256
#define MAX_VALUE_LEN 512
#define MAX_LINE_LEN 1024

// 配置项节点
typedef struct config_item {
    char key[MAX_KEY_LEN];
    char value[MAX_VALUE_LEN];
    struct config_item *next;
} config_item_t;

// 配置管理器
typedef struct {
    config_item_t *head;
    int count;
    int has_default;
} config_manager_t;

// 函数声明
config_manager_t* config_manager_create(void);
void config_manager_destroy(config_manager_t *manager);
int config_load_from_file(config_manager_t *manager, const char *filename);
int config_save_to_file(config_manager_t *manager, const char *filename);
int config_get_value(config_manager_t *manager, const char *key, char *value, size_t value_size);
int config_set_value(config_manager_t *manager, const char *key, const char *value);
void config_print_all(config_manager_t *manager);

#endif // CONFIG_PARSER_H

