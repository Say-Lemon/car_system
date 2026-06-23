/**
 * @file player_ui.h
 * @brief 播放器 LVGL 界面：主菜单与视频播放控制页
 *
 * 对外暴露的 lv_obj_t 指针供 playback_controller / playback_monitor 读取滑条、
 * 在子线程中更新标签与进度条（须配合 mutex_lv）。
 */

#ifndef PLAYER_UI_H
#define PLAYER_UI_H

#include "lvgl/lvgl.h"
#include <stdbool.h>

/** 主菜单根容器 */
extern lv_obj_t *t0;

/**
 * 播放页控件（可能为 NULL，直到 ui_create_player_screen 执行完毕）
 * 播放页由 ui_create_close_button / ui_create_transport_controls /
 * ui_create_playlist / ui_create_playback_sliders 分块创建。
 * volume/bright/play_slider 供 playback_play_current 读初值；
 * time_*_label、play_slider 供 playback_status_reader_thread 更新。
 */
extern lv_obj_t *volume_slider;
extern lv_obj_t *bright_slider;
extern lv_obj_t *play_slider;
extern lv_obj_t *time_pos_label;
extern lv_obj_t *time_length_label;

/** 创建主菜单：扫描视频目录、显示 "video" 入口（main 启动后调用） */
void ui_show_main_menu(void);

/** 主菜单 "video" 按钮回调：创建 FIFO、打开 fd、进入播放界面 */
void ui_player_open_cb(lv_event_t *e);

/** 播放页右上角关闭：杀 mplayer、删除播放容器 */
void ui_player_close_cb(lv_event_t *e);

void video_player_launch(void);

#endif /* PLAYER_UI_H */
