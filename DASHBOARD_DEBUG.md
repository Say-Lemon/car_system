# Phase 1: 基础界面框架 — 调试记录

## 项目概述

智能车机系统 Phase 1，在 GEC6818 (ARM Cortex-A53, 800×480) + LVGL v9.0-dev 上实现 4 区域仪表盘主界面。

## 布局结构

```
800 × 480 (10px 边距 + 10px 区间距)

  10    70      10       440          10      250       10
┌────┬───────┬────┬──────────────┬────┬──────────────┬────┐
│    │ 侧边栏 │    │              │    │  时间天气     │    │
│    │ 5 圆形 │    │   仪表盘      │    │  140×250     │    │
│    │ 图标键 │    │  440×460     │    ├──────────────┤    │
│    │ 70×460 │    │  car.png背景 │    │  音乐控制     │    │
│    │       │    │  +车速/油量   │    │  310×250     │    │
│    │       │    │  +转速叠加    │    │              │    │
└────┴───────┴────┴──────────────┴────┴──────────────┴────┘
```

## 问题与解决

### 1. 中文字体乱码

**现象**：所有中文显示为方框。

**原因**：LVGL 默认只带英文字体（Montserrat），不含 CJK 字形。

**解决**：用 `lv_font_conv` 从系统 NotoSansSC.ttf 生成自定义中文位图字体 `zh_cn_font`（16px 4bpp），通过 `APPLY_ZH_FONT` 宏应用。

**字符覆盖**：从所有源文件中提取 500+ 汉字 + 特殊符号（°℃☀♫），加上后续新增的占位文字（番禺晴转多云手等），共 526 字符。

**后续优化**：生成 14/18px 多尺寸中文（`zh_cn_14`, `zh_cn_18`），用 `APPLY_ZH_14/18` 区分大小。

### 2. 图标字体

**现象**：特殊图标（☀♫📍）显示为方框或乱码。

**解决**：
- 侧边栏图标：Segoe MDL2（⏻🔊🔈）、Segoe UI Symbol（❄雪花）、FontAwesome simsun（☰菜单）
- 各自独立字体文件：`font_icons_20.c`（20px）、`font_snowflake_26.c`（26px）
- 图标字体包含 ASCII 数字和基本符号，可独立渲染

**教训**：LVGL 一个控件只能设一种字体。如果中文和图标需要不同大小，必须合并到一个字体文件或分开使用。

### 3. 中间仪表盘图片

#### 3.1 BMP 格式不兼容

**现象**：`car.bmp` 能 fopen 但 LVGL 不显示。

**原因**：LVGL BMP 解码器只支持 24-bit 无压缩 BMP。

**解决**：改用 PNG。

#### 3.2 PNG 文件系统加载失败

**现象**：`S:car.png` 显示 "No data"。

**根因链**（逐层排查）：

| 层级 | 问题 | 修复 |
|------|------|------|
| ① 驱动未注册 | `lv_fs_stdio_init()` 未被调用 | `main.c` 中 `lv_init()` 后调用 |
| ② 路径前缀错误 | `lv_conf.h` 中 `LV_FS_STDIO_PATH "/"` 导致 `S:car.png` → `fopen("/car.png")` 而不是 `fopen("./car.png")` | 改为 `"./"` |
| ③ lodepng 内存不兼容 | lodepng 反复 `realloc` 在 LVGL 内置 first-fit 分配器上失败，扩大 16MB 仍不足 | **不可修复** |

**最终方案**：嵌入式 C 数组。将 car.png (440×460) 转为 `lv_img_dsc_t` 编译进二进制，完全绕过文件系统和解码器。

#### 3.3 stride 字段编译错误

`LVGL v9.0-dev` 的 `lv_img_header_t` 无 `stride` 字段。

#### 3.4 颜色偏蓝

LVGL `LV_COLOR_DEPTH 32` 像素字节序是 **BGRA**（蓝绿红Alpha），而非 ARGB。红蓝通道互换导致偏蓝。

#### 3.5 图片圆角

`lv_obj_set_style_clip_corner(cont, true, 0)` 将容器子元素裁剪到 `radius` 圆角。

### 4. 中文字体大小不可变

**现象**：设置 `lv_font_montserrat_XX` 对中文无效，始终 16px。

**原因**：`APPLY_ZH_FONT` 最终设置 `zh_cn_font`（16px），覆盖了前面的 montserrat 设置。LVGL 一个 label 只能一种字体。

**解决**：生成多尺寸中文字体（`zh_cn_14`, `zh_cn_18`），用 `APPLY_ZH_14/18` 按需选择。

### 5. `lv_font_montserrat_32` 未声明

**现象**：编译错误 `'lv_font_montserrat_32' undeclared`。

**原因**：字体 .c 文件存在但 `lv_conf.h` 中 `LV_FONT_MONTSERRAT_32` 未设为 1（默认 0）。

**解决**：在 `lv_conf.h` 中启用对应字体宏。

### 6. 文字阴影 API 不存在

**现象**：链接错误 `undefined reference to 'lv_obj_set_style_text_shadow_*'`。

**原因**：LVGL v9.0-dev 无此 API（后续版本添加）。

**解决**：移除所有 shadow 调用，纯文字显示。

### 7. 📍 Emoji 渲染警告

**现象**：`glyph dsc. not found for U+1F4CD`。

**原因**：U+1F4CD 是 Emoji，在 16px 4bpp 字体中无法渲染。

**解决**：移除 📍 字符，改用纯文字。

### 8. 音乐控制进度条样式

- 细条化：`lv_obj_set_height(slider, 4)`
- 橙色填充：`LV_PART_INDICATOR` + `#FF6D00`
- 橙色小圆球：`LV_PART_KNOB` + `8×8` 尺寸
- 透明控制按钮：`bg_opa = LV_OPA_TRANSP`

### 9. 开发板系统时间

日期显示为 2015/1/1 是开发板 RTC 未同步，非代码问题。

## 字体文件汇总

| 文件 | 大小 | 用途 |
|------|------|------|
| `font_zh_cn.c` | 16px 448KB | 默认中文 |
| `font_zh_cn_14.c` | 14px 370KB | 小字中文 |
| `font_zh_cn_18.c` | 18px 521KB | 大字中文 |
| `font_icons_20.c` | 20px 22KB | 侧边栏电源/音量图标 |
| `font_snowflake_26.c` | 26px 4KB | 空调雪花 |
| `car_img_data.c` | 440×460 BGRA 5MB | 仪表盘背景图 |

## 项目文件清单 (usrCode/)

| 文件 | 职责 |
|------|------|
| `main.c` | LVGL/fbdev/evdev 初始化 + 主循环 |
| `app_config.h` | 公共宏、布局参数、全局状态声明 |
| `car_ui.c/h` | 4 区域布局编排 |
| `car_ui_sidebar.c/h` | 左侧圆形图标栏 |
| `car_ui_dashboard.c/h` | 中间仪表盘（图片+数据叠加） |
| `car_ui_status.c/h` | 右上时间天气卡片 |
| `car_ui_music_bar.c/h` | 右下音乐控制卡片 |
| `font_zh_cn.c/h` | 多尺寸中文 + 图标字体 |
| `font_icons_20.c/h` | 侧边栏图标字体 |
| `font_snowflake_26.c` | 雪花图标字体 |
| `car_img_data.c` | 仪表盘背景图（嵌入式） |
