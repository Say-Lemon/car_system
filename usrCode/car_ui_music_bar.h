/**
 * @file car_ui_music_bar.h
 * @brief 右下音乐播放状态栏 — 歌名/进度条/控制按钮
 *
 * Phase 1: 静态控件，按钮仅打印日志
 * Phase 2: 绑定 mplayer 控制器，进度条与歌名实时更新
 */

#ifndef CAR_UI_MUSIC_BAR_H
#define CAR_UI_MUSIC_BAR_H

#include "lvgl/lvgl.h"

/** 在 parent 内创建音乐控制栏 */
void car_ui_music_bar_create(lv_obj_t *parent);

/** 更新歌名显示（Phase 2 调用） */
void car_ui_music_bar_update_song(const char *name);

/** 更新进度（Phase 2 调用） */
void car_ui_music_bar_update_progress(int percent, float pos_sec, float len_sec);

/** 控件句柄 */
extern lv_obj_t *g_music_bar_label_song;
extern lv_obj_t *g_music_bar_label_artist;
extern lv_obj_t *g_music_bar_label_time;
extern lv_obj_t *g_music_bar_progress;
extern lv_obj_t *g_music_bar_btn_play;
extern lv_obj_t *g_music_bar_btn_prev;
extern lv_obj_t *g_music_bar_btn_next;
extern lv_obj_t *g_music_bar_play_label;  /* play/pause 按钮内的文字对象 */

#endif /* CAR_UI_MUSIC_BAR_H */
