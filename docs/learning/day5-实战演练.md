# 第 5 天：实战演练 — 从需求到代码

**目标：能独立对一个模块做修改**

## 1. 回顾：你在这个项目中通过 AI 完成的所有需求

| 需求 | 涉及文件 | 核心技术点 |
|------|---------|-----------|
| "时间显示错误" → 2015/1/1 | `network_client.c` | `settimeofday()`, `setenv("TZ")`, `tzset()` |
| "天气是模拟数据" → 接入 wttr.in | `weather_server.py` | `urllib.request`, JSON API, 英中翻译表 |
| "中文字符缺失" → 方框 | `font_zh_cn_18.c` | `lv_font_conv`, `NotoSansSC-VF.ttf`, `--symbols` |
| "视频播放闪烁" | `car_ui_dashboard.c`, `car_ui_status.c`, `car_ui_music_bar.c` | `g_video_overlay_active` 标志位保护 |
| "config.json 保存无效" | `main.c` | `settings_config_load()` 调用时机：必须在 UI 创建之前 |
| "自动熄屏不生效" | `car_ui_sidebar.c` | LVGL 遮罩替代硬件背光 |
| "待机无法唤醒" | `car_ui_sidebar.c` | 遮罩层拦截触摸事件 → 在遮罩上加 CLICKABLE |
| "连接后时间同步慢" | `network_client.c`, `car_ui_status.c` | `lv_async_call` 即时刷新时钟 |
| "WSL2 IP 变化" | `weather_server.py` | 从 WSL2 → Windows 直接运行 |
| "二进制太大" (3MB→1.2MB) | `lv_conf.h`, `Makefile`, `car_img_data.c` | 删除无用字体、LTO、gc-sections、BMP 文件加载 |

## 2. 理解"AI 协助开发"的正确姿势

你在这个项目中的角色不是"写代码"，而是"做决策"：

```
你的角色                      AI 的角色
┌─────────────┐              ┌──────────────────┐
│ 提出需求       │──────────→│ 分析代码、设计方案  │
│ 验证结果       │←──────────│ 生成具体代码        │
│ 发现问题       │──────────→│ 调试修复            │
│ 最终决策       │            │                    │
└─────────────┘              └──────────────────┘
```

**你能独立做的**（不需要 AI）：
- 看懂代码的逻辑流程（因为你知道 fork/pipe/线程/socket 的基础）
- 判断 AI 给的方案是否合理（因为你知道项目的架构约束）
- 发现 UI 层面的 bug 并描述给 AI

**需要 AI 辅助的**：
- 记住所有 API 的名称和参数（`lv_obj_set_style_bg_opa` 这种）
- 跨多个文件的重构
- 排查复杂 bug（如竞态条件）

## 3. 实战任务 A：给应用菜单加一个新卡片

**需求**：在应用菜单加一个"导航"卡片，点击后显示"功能开发中"。

**步骤**：

1. 打开 `ui/app_menu_ui.c`

2. 找到卡片创建代码（第 103-105 行）：
```c
create_app_card(grid, "乐",  "音乐播放器", on_music);
create_app_card(grid, "视",  "视频播放器", on_video);
create_app_card(grid, "设",  "系统设置",   on_settings);
```

3. 添加新卡片：
```c
create_app_card(grid, "导",  "导航", on_nav);
```

4. 添加回调函数：
```c
static void on_nav(lv_event_t *e) {
    (void)e;
    app_menu_ui_toggle();  // 返回仪表盘
    // 弹出一个提示（或者什么都不做）
    printf("[App] 导航功能开发中\n");
}
```

5. 编译部署，观察应用菜单多了一个卡片。

**思考**：当前 `create_app_card` 的参数是 `(parent, icon, label, callback)`。如果要做不同尺寸的卡片，怎么扩展？

## 4. 实战任务 B：让设置面板的"自动熄屏"真正生效

**现状**：`g_auto_screen_off_sec` 可以保存和读取，但设置面板在恢复时不会更新 roller/button。

**需求**：打开设置面板时，熄屏按钮显示当前保存的值。

**提示**：找到 `settings_ui_open()` 函数，在 `lv_obj_clear_flag` 之前，加入：
```c
// 刷新熄屏选项为当前值
for (int i = 0; i < 4; i++) {
    if (off_values[i] == g_auto_screen_off_sec) {
        off_sel = i;
        break;
    }
}
update_off_btn_styles();
```

## 5. 实战任务 C：加一个新的天气数据项

**需求**：在天气数据中加入"湿度"（humidity），显示在状态栏。

**步骤**：

1. **C 端** — `core/app_config.h` 添加全局变量：
```c
extern int g_humidity;  // 湿度 0-100%
```

2. **C 端** — `core/main.c` 定义并初始化：
```c
int g_humidity = 50;
```

3. **网络端** — `network/network_client.c` 在 `parse_and_update` 中解析：
```c
cJSON *h = cJSON_GetObjectItem(root, "humidity");
if (cJSON_IsNumber(h)) humidity_tmp = (int)h->valuedouble;
// 在临界区内：
g_humidity = humidity_tmp;
```

4. **UI 端** — `ui/car_ui_status.c` 在 `ui_refresh_status_labels` 中显示：
```c
lv_label_set_text_fmt(g_label_weather, "%s  %d°  %s  湿度%d%%",
    g_location_str[0] ? g_location_str : "番禺",
    g_temp_celsius,
    g_weather_str[0] ? g_weather_str : "晴",
    g_humidity);
```

5. **服务端** — `weather_server.py` 在 JSON 中加入：
```python
return {
    "weather": weather_cn,
    "temp": int(current["temp_C"]),
    "location": city,
    "time": int(time.time()),
    "humidity": int(current["humidity"]),  # 新增
}
```

**试着独立完成 1-5 步**（遇到卡壳再问 AI）。

## 6. 学习建议

**从现在开始**，试着用以下方式跟 AI 协作：
1. 先自己读代码，理解大概逻辑（15 分钟）
2. 描述你的需求，让 AI 给方案（5 分钟）
3. 自己尝试改代码（15 分钟）
4. 编译 → 看效果 → 遇到错误再问 AI

**不要**直接说"帮我加个功能"，而要说"我想在 xxx 文件加 xxx，你帮我看看需要改哪些地方"。这样你才能真的学到东西。
