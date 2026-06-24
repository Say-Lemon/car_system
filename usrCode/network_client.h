/**
 * @file network_client.h
 * @brief 网络数据接收模块 — TCP 客户端线程接口
 *
 * Phase 6: 连接 SERVER_IP:SERVER_PORT，接收 JSON 天气数据，
 *          解析后更新全局变量，通过 lv_async_call 安全刷新 UI。
 */

#ifndef NETWORK_CLIENT_H
#define NETWORK_CLIENT_H

/** 启动网络接收线程（detached，自动重连）。
 *  连接失败时每 3 秒重试，不影响其他模块运行。 */
void network_client_start(void);

#endif
