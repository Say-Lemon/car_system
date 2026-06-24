#!/usr/bin/env python3
"""
智能车机系统 — 天气数据 TCP 服务器

向 GEC6818 开发板提供模拟天气数据。
每 5 秒发送一行 JSON，包含天气、温度、位置。
场景循环轮换，模拟真实天气变化。

启动方式:
    python3 scripts/weather_server.py

    或指定端口:
    python3 scripts/weather_server.py --port 9999
"""

import socket
import select
import signal
import sys
import time
import json
import argparse

# ============================================================
# 天气场景（每 5 秒轮换一个）
# ============================================================
SCENARIOS = [
    {"weather": "晴",      "temp": 32, "location": "广州"},
    {"weather": "多云",    "temp": 28, "location": "广州"},
    {"weather": "阴",      "temp": 24, "location": "番禺"},
    {"weather": "小雨",    "temp": 22, "location": "番禺"},
    {"weather": "阵雨",    "temp": 20, "location": "深圳"},
    {"weather": "雷阵雨",  "temp": 18, "location": "深圳"},
    {"weather": "多云",    "temp": 26, "location": "广州"},
    {"weather": "晴",      "temp": 30, "location": "番禺"},
]

running = True


def handle_signal(sig, frame):
    global running
    print("\n[Server] 收到信号，正在关闭...")
    running = False


def main():
    parser = argparse.ArgumentParser(description="车机天气数据服务器")
    parser.add_argument("--host", default="0.0.0.0", help="监听地址 (默认 0.0.0.0)")
    parser.add_argument("--port", type=int, default=8888, help="监听端口 (默认 8888)")
    parser.add_argument("--interval", type=int, default=5, help="发送间隔秒数 (默认 5)")
    args = parser.parse_args()

    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((args.host, args.port))
    server.listen(1)
    print(f"[Server] 监听 {args.host}:{args.port}，等待车机连接...")

    client = None
    scenario_idx = 0

    while running:
        # 非阻塞等待客户端连接
        readable, _, _ = select.select([server], [], [], 1.0)

        if server in readable:
            client, addr = server.accept()
            print(f"[Server] 客户端已连接: {addr[0]}:{addr[1]}")
            # 立即发送首条数据
            scenario = SCENARIOS[scenario_idx % len(SCENARIOS)]
            data = json.dumps(scenario, ensure_ascii=False) + "\n"
            try:
                client.sendall(data.encode("utf-8"))
                print(f"[Server] 发送: {data.strip()}")
            except (BrokenPipeError, ConnectionResetError):
                client.close()
                client = None
                continue
            scenario_idx += 1

        # 向已连接客户端发送数据
        if client:
            scenario = SCENARIOS[scenario_idx % len(SCENARIOS)]
            data = json.dumps(scenario, ensure_ascii=False) + "\n"
            try:
                client.sendall(data.encode("utf-8"))
                print(f"[Server] 发送: {data.strip()}")
                scenario_idx += 1
            except (BrokenPipeError, ConnectionResetError, OSError):
                print("[Server] 客户端断开")
                client.close()
                client = None

        time.sleep(args.interval)

    # 清理
    if client:
        client.close()
    server.close()
    print("[Server] 已关闭")


if __name__ == "__main__":
    main()
