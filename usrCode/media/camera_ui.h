/**
 * @file camera_ui.h
 * @brief 倒车影像模块 — V4L2 摄像头实时画面
 *
 * 点击应用菜单"倒车影像"卡片进入，触摸屏幕退出。
 * 摄像头通过 /dev/video7 采集，libjpeg 解码后直接写 framebuffer。
 */

#ifndef CAMERA_UI_H
#define CAMERA_UI_H

/** 打开倒车影像（隐藏 UI，启动摄像头线程） */
void camera_ui_open(void);

/** 关闭倒车影像（停止线程，恢复 UI） */
void camera_ui_close(void);

#endif /* CAMERA_UI_H */
