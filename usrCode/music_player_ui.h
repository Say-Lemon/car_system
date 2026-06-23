/**
 * @file music_player_ui.h
 * @brief 全屏音乐播放界面
 */

#ifndef MUSIC_PLAYER_UI_H
#define MUSIC_PLAYER_UI_H

#include "lvgl/lvgl.h"

lv_obj_t *music_player_ui_create(void);
void music_player_ui_close(void);

#endif
