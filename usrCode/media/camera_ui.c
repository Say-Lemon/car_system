/**
 * @file camera_ui.c
 * @brief 倒车影像 — V4L2 摄像头实时画面
 *
 * 摄像头通过 /dev/video7 采集 YUYV，转为 JPEG，
 * libjpeg 解码后直接写 /dev/fb0（独立 mmap）。
 * g_video_overlay_active 保护 LVGL 不刷新。
 * 全屏透明遮罩拦截触摸 → 点击任意位置退出。
 */

#include "camera_ui.h"
#include "app_config.h"
#include "lcd.h"
#include "yuyv.h"
#include "lvgl/lvgl.h"
#include <pthread.h>
#include <unistd.h>
#include <string.h>

extern int *mmap_fd;  /* lcd.c 的 framebuffer 映射指针 */

static volatile bool camera_running = false;
static pthread_t camera_tid;
static lv_obj_t *camera_overlay = NULL;  /* 全屏透明遮罩 */

/* ---- 触摸退出回调 ---- */
static void on_camera_exit_cb(lv_event_t *e)
{
    (void)e;
    camera_ui_close();
}

/* ---- 引导线绘制（直接写 fb0 地址，无数组遍历） ---- */

static void draw_one_line(unsigned int *fb, int x0, int y0, int x1, int y1)
{
    int dx = x1 - x0, dy = y1 - y0;
    int total = dx*dx + dy*dy;
    int seg   = total / 3;

    for (int y = (y0 < y1 ? y0 : y1); y <= (y0 > y1 ? y0 : y1); y++) {
        if (y < 0 || y >= 480) continue;
        int cx = x0 + dx * (y - y0) / dy;
        if (cx < 0 || cx >= 800) continue;

        int dist = (cx-x0)*(cx-x0) + (y-y0)*(y-y0);
        unsigned int color;
        if      (dist < seg)       color = 0xFF0000;
        else if (dist < seg * 2)   color = 0xFFFF00;
        else                       color = 0x00FF00;

        unsigned int *row = fb + y * 800;
        int left  = cx - 10;  if (left < 0)   left = 0;
        int right = cx + 10;  if (right > 799) right = 799;
        for (int x = left; x <= right; x++) row[x] = color;
    }
}

/* ---- 后台缓冲区（双缓冲消除撕裂） ---- */
static unsigned int *back_buf = NULL;
#define FB_BYTES (800 * 480 * 4)

/* ---- 摄像头线程 ---- */
static void *camera_thread(void *arg)
{
    (void)arg;
    struct jpg_data jpg_buf;

    lcd_open();
    mmap_lcd();

    /* 分配后台缓冲区（堆内存，显示控制器不可见） */
    back_buf = (unsigned int *)malloc(FB_BYTES);

    linux_v4l2_yuyv_init("/dev/video7");
    linux_v4l2_start_yuyv_capturing();

    /* 清屏 */
    memset(mmap_fd, 0, FB_BYTES);
    if (back_buf) memset(back_buf, 0, FB_BYTES);

    while (camera_running) {
        if (linux_v4l2_get_yuyv_data(&jpg_buf) < 0) {
            usleep(10000);
            continue;
        }

        if (back_buf) {
            /* 重定向 mmap_fd → back_buf，让摄像头帧直接写入后台缓冲 */
            int *saved_fb = mmap_fd;
            mmap_fd = (int *)back_buf;

            /* 摄像头帧 → back_buf（显示控制器不可见） */
            show_video_data(80, 0, jpg_buf.jpg_data, jpg_buf.jpg_size);

            mmap_fd = saved_fb;  /* 恢复 */

            /* 引导线 → back_buf */
            draw_one_line(back_buf, 80, 470, 280, 260);
            draw_one_line(back_buf, 720, 470, 520, 260);

            /* 一次性推送完整帧到 fb0 */
            memcpy(mmap_fd, back_buf, FB_BYTES);
        }
    }

    /* 退出前清屏 */
    memset(mmap_fd, 0, FB_BYTES);

    linux_v4l2_yuyv_quit();
    lcd_close();
    free(back_buf);
    back_buf = NULL;
    printf("[Camera] 摄像头已关闭\n");
    return NULL;
}

/* ---- 对外接口 ---- */
void camera_ui_open(void)
{
    if (camera_running) return;

    g_video_overlay_active = true;
    camera_running = true;

    /* 全屏透明遮罩：拦截触摸，防止穿透到下方控件 */
    camera_overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(camera_overlay, DISP_HOR_RES, DISP_VER_RES);
    lv_obj_set_pos(camera_overlay, 0, 0);
    lv_obj_set_style_bg_opa(camera_overlay, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(camera_overlay, 0, 0);
    lv_obj_add_flag(camera_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(camera_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(camera_overlay, on_camera_exit_cb,
                        LV_EVENT_CLICKED, NULL);

    pthread_create(&camera_tid, NULL, camera_thread, NULL);
    pthread_detach(camera_tid);

    printf("[Camera] 倒车影像已开启\n");
}

void camera_ui_close(void)
{
    if (!camera_running) return;

    camera_running = false;
    usleep(100000);  /* 等待线程退出 */

    g_video_overlay_active = false;

    /* 销毁遮罩 */
    if (camera_overlay) {
        lv_obj_del(camera_overlay);
        camera_overlay = NULL;
    }

    /* 强制 LVGL 全屏重绘，覆盖残留的摄像头画面 */
    lv_obj_invalidate(lv_scr_act());

    printf("[Camera] 倒车影像已关闭\n");
}
