/**
 * @file can_simulator.c
 * @brief CAN 总线数据模拟 — 驾驶循环状态机 + 燃油消耗
 *
 * 驾驶状态机：
 *   IDLE(怠速) → ACCELERATING(加速) → CRUISING(巡航) → DECELERATING(减速) → IDLE...
 *
 * 数据更新周期：200ms（5Hz，足以驱动仪表盘刷新）
 * 所有全局变量写入前加 g_can_mutex。
 */

#include "can_simulator.h"
#include "app_config.h"
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdint.h>

/* ---- 驾驶状态 ---- */
typedef enum {
    DRIVE_IDLE,
    DRIVE_ACCELERATING,
    DRIVE_CRUISING,
    DRIVE_DECELERATING
} drive_state_t;

static drive_state_t g_drive_state = DRIVE_IDLE;

/* 状态计时器（tick 数，每 tick=200ms → 5 ticks=1s） */
static int g_state_ticks  = 0;
static int g_target_ticks = 0;   /* 当前阶段目标 tick 数 */
static int g_target_speed = 0;   /* 加速阶段目标速度 */

/* 燃油消耗累计（使用浮点避免截断误差） */
static float g_fuel_accum = 80.0f;

/* ---- 简易伪随机（避免依赖 srand/rand 进程级状态） ---- */
static unsigned int g_lcg_seed = 0;
static void sim_srand(unsigned int s) { g_lcg_seed = s; }
static int sim_rand_range(int lo, int hi)
{
    g_lcg_seed = g_lcg_seed * 1103515245u + 12345u;
    return lo + (int)(g_lcg_seed % (unsigned int)(hi - lo + 1));
}

/* ---- 驾驶循环前缀声明 ---- */
static void sim_tick_accelerating(void);
static void sim_tick_cruising(void);
static void sim_tick_decelerating(void);
static void sim_tick_idle(void);

/* ================================================================
 *  主循环
 * ================================================================ */
static void *can_simulator_thread(void *arg)
{
    (void)arg;

    /* 用时间 + tid 种子化简易随机 */
    sim_srand((unsigned int)time(NULL) ^ (unsigned int)(uintptr_t)&arg);

    while (1) {
        pthread_mutex_lock(&g_can_mutex);

        /* ---- 状态机调度 ---- */
        switch (g_drive_state) {
        case DRIVE_IDLE:          sim_tick_idle();          break;
        case DRIVE_ACCELERATING:  sim_tick_accelerating();  break;
        case DRIVE_CRUISING:      sim_tick_cruising();      break;
        case DRIVE_DECELERATING:  sim_tick_decelerating();  break;
        }

        /* ---- 计算转速：speed=0 → RPM=0（熄火），高速 ~6600 RPM ---- */
        if (g_speed_kmh == 0) {
            g_engine_rpm = 0;
        } else {
            g_engine_rpm = g_speed_kmh * 55 + sim_rand_range(-100, 100);
            if (g_engine_rpm < 600)  g_engine_rpm = 600;
            if (g_engine_rpm > 7000) g_engine_rpm = 7000;
        }

        /* ---- 计算油耗（约 1-6%/min，与转速正相关） ---- */
        float rpm_factor = (float)g_engine_rpm / 4000.0f;
        g_fuel_accum -= 0.005f + rpm_factor * 0.01f;  /* 每 tick 消耗 */
        if (g_fuel_accum < 5.0f) g_fuel_accum = 5.0f;
        g_fuel_percent = (int)g_fuel_accum;

        pthread_mutex_unlock(&g_can_mutex);

        usleep(200000);  /* 200ms 周期 */
    }
    return NULL;
}

/* ================================================================
 *  状态 tick 函数
 * ================================================================ */

static void sim_tick_idle(void)
{
    g_speed_kmh = 0;
    g_state_ticks++;

    /* 怠速 2~5 秒后起步 */
    if (g_state_ticks >= g_target_ticks) {
        g_drive_state = DRIVE_ACCELERATING;
        g_state_ticks = 0;
        g_target_speed = sim_rand_range(80, 120);
        g_target_ticks = g_target_speed / sim_rand_range(2, 5) + 5;
    }
}

static void sim_tick_accelerating(void)
{
    /* 加速 2~5 km/h */
    int delta = sim_rand_range(2, 5);
    g_speed_kmh += delta;
    g_state_ticks++;

    /* 到达目标速度 → 巡航 */
    if (g_speed_kmh >= g_target_speed) {
        g_speed_kmh = g_target_speed;
        g_drive_state = DRIVE_CRUISING;
        g_state_ticks = 0;
        /* 巡航 3~8 秒 (15~40 ticks) */
        g_target_ticks = sim_rand_range(15, 40);
    }
}

static void sim_tick_cruising(void)
{
    /* 轻微速度波动 */
    g_speed_kmh += sim_rand_range(-1, 1);
    if (g_speed_kmh < 60) g_speed_kmh = 60;  /* 巡航下限 */
    g_state_ticks++;

    if (g_state_ticks >= g_target_ticks) {
        g_drive_state = DRIVE_DECELERATING;
        g_state_ticks = 0;
    }
}

static void sim_tick_decelerating(void)
{
    int delta = sim_rand_range(2, 5);
    g_speed_kmh -= delta;
    g_state_ticks++;

    if (g_speed_kmh <= 0) {
        g_speed_kmh = 0;
        g_drive_state = DRIVE_IDLE;
        g_state_ticks = 0;
        /* 怠速 2~5 秒 (10~25 ticks) */
        g_target_ticks = sim_rand_range(10, 25);
    }
}

/* ================================================================
 *  对外接口
 * ================================================================ */

void can_simulator_start(void)
{
    /* 初始化状态 */
    g_drive_state   = DRIVE_IDLE;
    g_state_ticks   = 0;
    g_target_ticks  = 10;  /* 首轮怠速 ~2 秒 */
    g_target_speed  = 100;
    g_fuel_accum    = (float)g_fuel_percent;

    pthread_t tid;
    pthread_create(&tid, NULL, can_simulator_thread, NULL);
    pthread_detach(tid);

    printf("[CAN] 模拟线程已启动\n");
}
