/**
 * @file settings_config.h
 * @brief 系统设置持久化 — config.json 读写接口
 *
 * Phase 7: 启动时加载设置文件，用户修改后保存为 JSON。
 * 使用 cJSON 库序列化/反序列化。
 */

#ifndef SETTINGS_CONFIG_H
#define SETTINGS_CONFIG_H

/** 从 CONFIG_FILE 加载设置到全局变量。文件不存在时用默认值并创建。 */
void settings_config_load(void);

/** 将当前全局变量保存到 CONFIG_FILE */
void settings_config_save(void);

#endif
