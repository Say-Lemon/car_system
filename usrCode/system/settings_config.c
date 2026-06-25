/**
 * @file settings_config.c
 * @brief config.json 读写实现 — cJSON 序列化/反序列化
 *
 * JSON 格式: {"volume":10,"brightness":70,"auto_off_sec":30,"bt_name":"GEC6818-CAR"}
 */

#include "settings_config.h"
#include "app_config.h"
#include "cJSON/cJSON.h"
#include <stdio.h>

/* ========== 加载 ========== */
void settings_config_load(void)
{
    FILE *fp = fopen(CONFIG_FILE, "r");
    if (fp == NULL) {
        printf("[Config] %s 不存在，创建默认配置\n", CONFIG_FILE);
        settings_config_save();
        return;
    }

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    if (len <= 0 || len > 4096) { fclose(fp); settings_config_save(); return; }
    fseek(fp, 0, SEEK_SET);

    char *buf = (char *)malloc(len + 1);
    if (!buf) { fclose(fp); return; }
    fread(buf, 1, len, fp);
    buf[len] = '\0';
    fclose(fp);

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) { settings_config_save(); return; }

    cJSON *v = cJSON_GetObjectItem(root, "volume");
    if (cJSON_IsNumber(v)) g_sys_volume = (int)v->valuedouble;

    cJSON *b = cJSON_GetObjectItem(root, "brightness");
    if (cJSON_IsNumber(b)) g_sys_brightness = (int)b->valuedouble;

    cJSON *a = cJSON_GetObjectItem(root, "auto_off_sec");
    if (cJSON_IsNumber(a)) g_auto_screen_off_sec = (int)a->valuedouble;

    cJSON *bt = cJSON_GetObjectItem(root, "bt_name");
    if (cJSON_IsString(bt)) strncpy(g_bluetooth_name, bt->valuestring, sizeof(g_bluetooth_name) - 1);

    cJSON_Delete(root);
    printf("[Config] 已加载配置: volume=%d brightness=%d auto_off=%d bt=%s\n",
           g_sys_volume, g_sys_brightness, g_auto_screen_off_sec, g_bluetooth_name);
}

/* ========== 保存 ========== */
void settings_config_save(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "volume",         g_sys_volume);
    cJSON_AddNumberToObject(root, "brightness",     g_sys_brightness);
    cJSON_AddNumberToObject(root, "auto_off_sec",   g_auto_screen_off_sec);
    cJSON_AddStringToObject(root, "bt_name",        g_bluetooth_name);

    char *json = cJSON_PrintUnformatted(root);
    if (json) {
        FILE *fp = fopen(CONFIG_FILE, "w");
        if (fp) {
            fputs(json, fp);
            fclose(fp);
            printf("[Config] 已保存: %s\n", json);
        } else {
            fprintf(stderr, "[Config] 无法写入 %s\n", CONFIG_FILE);
        }
        free(json);
    }
    cJSON_Delete(root);
}
