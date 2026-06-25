/**
 * @file can_simulator.h
 * @brief CAN 总线数据模拟 — 驾驶循环线程接口
 *
 * Phase 4: 在后台线程模拟车速/转速/油量变化，仪表盘通过 LVGL 定时器读取。
 *
 * 线程模型：
 *   can_simulator 线程 (200ms 周期) → 写 g_speed_kmh / g_fuel_percent / g_engine_rpm
 *   LVGL 定时器 (200ms)             → 读全局变量 → 更新仪表盘 UI
 *   二者通过 g_can_mutex 保护共享数据。
 */

#ifndef CAN_SIMULATOR_H
#define CAN_SIMULATOR_H

/** 启动 CAN 模拟线程（detached，会话期间持续运行） */
void can_simulator_start(void);

#endif /* CAN_SIMULATOR_H */
