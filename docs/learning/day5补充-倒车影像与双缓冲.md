# Day 5 补充 — 倒车影像模块与双缓冲技术

## 1. 倒车影像模块架构

### 1.1 和之前模块的区别

之前所有模块（空调面板、设置面板、音乐播放器等）都是 LVGL 控件。倒车影像**不是 LVGL** — 它直接操作 framebuffer。

| | LVGL 面板 | 倒车影像 |
|---|---------|---------|
| 渲染方式 | LVGL 控件树 → `fbdev_flush` | `lcd_draw_point` 直接写 fb0 |
| 冲突处理 | `g_video_overlay_active` 跳过 LVGL 刷新 | 全屏透明遮罩拦截触摸 |
| 退出方式 | toggle 函数 | 点击遮罩 → `camera_ui_close()` |

### 1.2 参考代码

基于「资料」中的 `camera_1023_yuv_v4l2` 示例。核心调用链：

```
linux_v4l2_yuyv_init("/dev/video7")     // V4L2 初始化摄像头
linux_v4l2_start_yuyv_capturing()       // 开始采集
linux_v4l2_get_yuyv_data(&jpg_buf)      // 获取一帧 → YUYV→JPEG
show_video_data(x, y, data, size)       // JPEG 解码 → 逐行写 fb0
linux_v4l2_yuyv_quit()                  // 清理
```

`show_video_data` 内部：libjpeg 解码 → `lcd_draw_point` 逐像素写 `mmap_fd`。

### 1.3 依赖库

| 文件 | 作用 |
|------|------|
| `libapi_v4l2_arm1.so` | V4L2 摄像头封装（yuyv_init/start/get/quit） |
| `libjpeg.so.8` | JPEG 解码（jpeg_read_scanlines 等） |
| `lcd.c` | LCD 驱动（framebuffer mmap + 像素绘制） |

---

## 2. 引导线 — 三段分段 + 等腰梯形

### 2.1 规格

- **线宽**: 20px（垂直于线方向的 21px 填充）
- **颜色**: 下段红 `0xFF0000`、中段黄 `0xFFFF00`、上段绿 `0x00FF00`
- **形状**: 等腰梯形，顶部不相交

```
         ╱                      ╲       ← 绿段（上 1/3）
       ╱                          ╲
     ╱         梯形空缺             ╲    ← 黄段（中 1/3）
   ╱                                ╲
 ╱                                    ╲  ← 红段（下 1/3）
(80,470)                          (720,470)
```

- 左线: (80,470) → (280,260)
- 右线: (720,470) → (520,260)

---

## 3. 双缓冲 — 消除画面撕裂

### 3.1 问题的根源

直接写 fb0 时，显示控制器在异步读取同一块内存。你边写它边读 → 屏幕看到写了一半的中间状态 → "一闪一闪"。

```
❌ 单缓冲（闪烁）：
  show_video_data → fb0（显示控制器实时看到写入过程）
  draw_guide_lines → fb0（又看到一次写入过程）
```

### 3.2 解决方案：堆内存后台缓冲

参考项目 1（相册系统）的做法：

```
✅ 双缓冲（零闪烁）：
  back_buf = malloc(800*480*4)        // 1. 堆内存，显示控制器不可见
  
  mmap_fd → back_buf                  // 2. 临时重定向
  show_video_data → back_buf           // 3. 摄像头帧写入后台
  mmap_fd → 恢复                       // 4. 恢复指针
  
  draw_one_line(back_buf, ...)        // 5. 引导线写入后台
  
  memcpy(fb0, back_buf, ...)          // 6. 一次性推送完整帧
```

**关键技巧**：`lcd.c` 中的 `mmap_fd` 是全局变量，`show_video_data` 和 `lcd_draw_point` 都通过它写 fb0。在调用 `show_video_data` 前临时把 `mmap_fd` 指向 `back_buf`，所有绘制都进后台缓冲，最后一把 `memcpy` 推送到屏幕。

### 3.3 为什么这能消除闪烁

显示控制器只在步骤 6 看到一次写入 — 而这次写入的内容是**完整合成好**的（摄像头帧 + 引导线全部画完了）。对于显示控制器来说，它看到的是一个"瞬间完成"的整帧切换，没有中间状态。

### 3.4 与硬件双缓冲的对比

| | 软件双缓冲（本项目） | 硬件双缓冲（FBIOPAN） |
|---|-------------------|---------------------|
| 缓冲区位置 | 堆内存 `malloc` | fb0 虚拟分辨率区域 |
| 切换方式 | `memcpy` 整帧 | `ioctl(FBIOPAN)` 改可见区 |
| 速度 | 受内存带宽限制（~1ms/帧） | 硬件切换（~0μs） |
| 优点 | 无硬件依赖 | 零 CPU 开销 |
| 局限 | 需要拷贝整帧 | 需要驱动支持 |

本项目 GEC6818 只有单 fb0，用软件双缓冲方案。效果完全一样。

---

## 4. 触摸退出 — 全屏透明遮罩

倒车影像渲染不走 LVGL，但触摸输入仍然由 LVGL 处理（`/dev/input/event0` → LVGL 事件系统）。

```c
// camera_ui_open() 中创建全屏透明遮罩
camera_overlay = lv_obj_create(lv_scr_act());
lv_obj_set_size(camera_overlay, 800, 480);
lv_obj_add_flag(camera_overlay, LV_OBJ_FLAG_CLICKABLE);  // 可点击
lv_obj_add_event_cb(camera_overlay, on_camera_exit_cb, LV_EVENT_CLICKED, NULL);
```

**为什么用遮罩？** 如果用 `lv_scr_act()` 注册 CLICKED，触摸可能穿透到下方的侧边栏按钮（音量+、空调、菜单等）。全屏遮罩在最上层，拦截所有触摸。

---

## 5. 动手任务

### 任务 1：改引导线颜色
找到 `draw_one_line()` 中的颜色常量（`0xFF0000` / `0xFFFF00` / `0x00FF00`），换成你喜欢的颜色组合。

### 任务 2：改引导线端点
修改 `draw_one_line` 调用的 4 个坐标参数，观察引导线形状变化：
- 如果两条线的 top 端点都改成 (400, 240)，会发生什么？
- 如果把 bottom 端点向内移动 100px，引导线会怎么变？

### 任务 3：理解 mmap_fd 重定向
在 `camera_thread` 的循环中，找到 `saved_fb = mmap_fd` 和 `mmap_fd = (int *)back_buf` 这四行代码。用注释解释每一行的作用。
