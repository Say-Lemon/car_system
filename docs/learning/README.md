# car_system 项目学习指南

## 学习路线总览（6 天）

| 天数 | 主题 | 核心文件 | 关键词 |
|------|------|---------|--------|
| [第 1 天](day1-项目骨架.md) | 项目骨架 | `core/main.c`, `core/car_ui.c`, `core/app_config.h` | 启动流程、UI布局、framebuffer |
| [第 2 天](day2-LVGL面板模式.md) | UI开发套路 | `ui/car_ui_ac_panel.c`, `ui/app_menu_ui.c`, `ui/settings_ui.c` | 四步法、样式模式、面板互斥、Flex布局 |
| [第 3 天](day3-多线程与IPC.md) | 多线程+IPC | `media/music_controller.c` | fork+pipe、4线程模型、condvar同步、seek竞态 |
| [第 4 天](day4-网络编程.md) | 网络通信 | `network/network_client.c`, `scripts/weather_server.py` | TCP/JSON、缓冲拆包、lv_async_call、时间同步 |
| [第 5 天](day5-实战演练.md) | 独立修改代码 | 全部文件 | 加卡片、扩展天气数据、AI协作方法 |
| [第 6 天](day6-项目复盘.md) | 面试准备 | 全部文件 | 架构图、5个技术难点、简历要点 |

## 前置知识对照

| 你的课程 | 项目中的应用位置 |
|---------|----------------|
| 文件 IO | `music_catalog.c`, `settings_config.c`, `vp_media_catalog.c` |
| 系统编程 | `music_controller.c`, `music_controller.h` |
| 网络编程 | `network_client.c`, `weather_server.py` |
| LVGL | `ui/` 目录下所有文件 |
| Git 项目管理 | `.gitignore`, commit 历史 |
| 开发板配置 | `main.c` 中的 fbdev_init/evdev_init |

## 学习建议

1. **每天 2-3 小时**（读代码 1h + 动手 1h + 总结 0.5h）
2. **先读文档再读代码**：文档解释"为什么"（设计意图），代码展示"怎么做"（实现细节）
3. **多画图**：线程图、UI 布局图、数据流图
4. **改代码验证**：读完一个模块后，改一个数字，重新编译部署，看效果变化
