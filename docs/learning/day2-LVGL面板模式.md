# 第 2 天：LVGL 面板模式 — UI 开发套路

**目标：能独立新建一个 LVGL 面板**

## 1. 你已有的基础知识

| 已学课程 | 今天会用到的知识点 |
|---------|-------------------|
| LVGL | 控件创建、样式设置、事件回调、容器层级 |
| 文件 IO | JSON 文件读写（结合 settings_config.c） |

## 2. 核心模式 1：面板创建四步法

每个面板（如空调面板、设置面板）都遵循完全相同的四步模板。以 `ui/car_ui_ac_panel.c` 为例：

### 步骤 1：创建容器

```c
ac_cont = lv_obj_create(parent);                       // 创建容器
lv_obj_set_size(ac_cont, CENTER_W, ZONE_H);            // 440×460
lv_obj_set_pos(ac_cont, MARGIN + SIDEBAR_W + GAP, MARGIN); // x=90, y=10
lv_obj_set_style_bg_color(ac_cont, lv_color_hex(0x1A1A2E), 0); // 深蓝灰背景
lv_obj_set_style_border_width(ac_cont, 0, 0);          // 无边框
lv_obj_set_style_radius(ac_cont, 12, 0);               // 圆角 12px
lv_obj_set_style_pad_all(ac_cont, 20, 0);              // 内边距 20px
```

**口诀**：size → pos → bg → border → radius → pad

### 步骤 2：添加标题

```c
lv_obj_t *title = lv_label_create(ac_cont);
lv_label_set_text(title, "空调控制");
lv_obj_set_style_text_color(title, lv_color_hex(0xFF6D00), 0); // 橙色
lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
APPLY_ZH_18(title);  // 应用 18px 中文字体
```

### 步骤 3：添加控件（slider / button / label）

**滑块模式**：
```c
// 3a. 创建滑块行容器
lv_obj_t *section = lv_obj_create(ac_cont);
lv_obj_set_size(section, CENTER_W - 40, 110);         // 子容器尺寸

// 3b. 数值标签
temp_label = lv_label_create(section);
lv_label_set_text_fmt(temp_label, "温度  %d °C", g_ac_temperature);
lv_obj_align(temp_label, LV_ALIGN_TOP_MID, 0, 50);

// 3c. 滑块
temp_slider = lv_slider_create(section);
lv_slider_set_range(temp_slider, 16, 32);             // 范围
lv_slider_set_value(temp_slider, g_ac_temperature, LV_ANIM_OFF);
lv_obj_align(temp_slider, LV_ALIGN_BOTTOM_MID, 0, 0);
style_ac_slider(temp_slider);                         // 样式函数
lv_obj_add_event_cb(temp_slider, on_temp_changed_cb,     // 回调
                    LV_EVENT_VALUE_CHANGED, NULL);
```

**按钮模式**（多选一）：
```c
// 创建 4 个模式按钮，选中的高亮橙色
for (int i = 0; i < 4; i++) {
    mode_btns[i] = create_mode_btn(grid, mode_names[i]);
    lv_obj_add_event_cb(mode_btns[i], on_mode_clicked_cb,
                        LV_EVENT_CLICKED, (void *)(intptr_t)i);
}
// update_mode_btn_styles() — 遍历按钮，选中→橙色，未选中→深灰
```

### 步骤 4：初始隐藏

```c
lv_obj_add_flag(ac_cont, LV_OBJ_FLAG_HIDDEN);
```

## 3. 核心模式 2：滑块样式函数

项目中所有滑块用同一个样式函数。对比阅读：

| 文件 | 函数名 | 位置 |
|------|--------|------|
| `ui/car_ui_ac_panel.c` | `style_ac_slider()` | 第 48 行 |
| `ui/settings_ui.c` | `style_slider()` | 第 63 行 |
| `media/vp_player_ui.c` | `style_slider()` | 第 224 行 |

三者的代码几乎一样：
```c
static void style_slider(lv_obj_t *s) {
    lv_obj_set_height(s, 30);                             // 触摸区高 30px
    lv_obj_set_style_height(s, 4, LV_PART_MAIN);          // 轨道视觉高 4px
    // 轨道：深灰 #3A3A4E
    lv_obj_set_style_bg_color(s, 0x3A3A4E, LV_PART_MAIN);
    // 已填充段：橙色 #FF6D00
    lv_obj_set_style_bg_color(s, 0xFF6D00, LV_PART_INDICATOR);
    // 滑块小球：橙色 8×8px
    lv_obj_set_style_bg_color(s, 0xFF6D00, LV_PART_KNOB);
    lv_obj_set_style_width(s, 8, LV_PART_KNOB);
}
```

**关键知识点**：`LV_PART_MAIN / LV_PART_INDICATOR / LV_PART_KNOB`

每个 LVGL 控件有多个"部件"，可以分别设置样式：
- `LV_PART_MAIN` — 控件主体（轨道的底色）
- `LV_PART_INDICATOR` — 指示器（已填充的橙色段）
- `LV_PART_KNOB` — 滑块小球

## 4. 核心模式 3：面板互斥（toggle）

中间区域 (440×460) 被三个面板共享：仪表盘 ←→ 空调面板 ←→ 应用菜单 ←→ 设置面板。它们如何互斥？

**空调面板 toggle**：
```c
void car_ui_ac_panel_toggle(void) {
    if (ac_visible) {
        lv_obj_add_flag(ac_cont, LV_OBJ_FLAG_HIDDEN);      // 隐藏自己
        lv_obj_clear_flag(g_dashboard_cont, LV_OBJ_FLAG_HIDDEN); // 显示仪表盘
    } else {
        lv_obj_add_flag(g_dashboard_cont, LV_OBJ_FLAG_HIDDEN);  // 隐藏仪表盘
        app_menu_ui_hide();                                    // 隐藏对手
        lv_obj_clear_flag(ac_cont, LV_OBJ_FLAG_HIDDEN);       // 显示自己
    }
}
```

**每个面板的"合同"**（接口规范）：
```c
void xxx_create(lv_obj_t *parent);   // 创建面板（初始隐藏）
void xxx_toggle(void);               // 切换显示/隐藏
void xxx_hide(void);                 // 强制隐藏（供其他面板互斥调用）
```

你看 `app_menu_ui.c`、`car_ui_ac_panel.c`、`settings_ui.c` 三个文件，全都遵守这份合同。

## 5. 核心模式 4：应用菜单的卡片布局

```c
// 创建 Flex 网格容器
lv_obj_t *grid = lv_obj_create(menu_cont);
lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);    // 行排列，溢出换行
lv_obj_set_flex_align(grid,
    LV_FLEX_ALIGN_SPACE_EVENLY,   // 水平均匀分布
    LV_FLEX_ALIGN_SPACE_EVENLY,   // 垂直均匀分布
    LV_FLEX_ALIGN_CENTER);        // 交叉轴居中

// 添加卡片
create_app_card(grid, "乐",  "音乐播放器", on_music);
create_app_card(grid, "视",  "视频播放器", on_video);
create_app_card(grid, "设",  "系统设置",   on_settings);
```

**卡片内部结构**：
```
┌─────────────┐
│    乐        │  ← 16px 橙色图标(居中偏上)
│             │
│  音乐播放器  │  ← 18px 白色标签(居中偏下)
└─────────────┘
  160 × 140
```

## 6. 项目颜色规范

所有代码统一使用这些颜色值，记住它们：

| 用途 | 颜色值 | 宏/常量 |
|------|--------|---------|
| 屏幕背景 | `#0A0A1A` | 深黑蓝 |
| 面板/容器底 | `#1A1A2E` | 深蓝灰 |
| 按钮/卡片底 | `#2A2A3E` | 中深灰蓝 |
| 强调色（标题/滑块/按钮） | `#FF6D00` | 橙色 |
| 滑块轨道 | `#3A3A4E` | 深灰 |
| 主文字 | `lv_color_white()` | 白色 |
| 次要文字 | `#AAAAAA` / `#888888` | 灰色 |
| 范围标签 | `#666666` | 暗灰 |

## 7. 动手任务

### 任务 1：手工创建一个面板
新建文件 `ui/test_panel.c/h`，实现"合同"三个函数，创建（隐藏）、toggle、hide。面板里放一个标签"你好 LVGL"和一个按钮。

提示：复制 `ui/car_ui_ac_panel.c` 的前 30 行作为模板，改标题和内容。

### 任务 2：追踪面板切换流程
按以下顺序操作：侧边栏 AC 键 → 侧边栏菜单键 → 点击"设置"卡片 → 侧边栏菜单键。

画出每次操作后各面板的可见状态（dashboard/ac/menu/settings 谁显示谁隐藏）。

### 任务 3：找到重复代码
`style_slider()` 函数在三个文件中出现了三次。找出它们的具体位置，思考：怎么消除重复？（提示：提取到一个公共头文件）
