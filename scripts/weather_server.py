#!/usr/bin/env python3
"""
智能车机系统 — 真实天气数据 TCP 服务器

从 wttr.in 免费 API 获取实时天气，发送到 GEC6818 开发板。

启动方式:
    python3 scripts/weather_server.py

    指定城市:
    python3 scripts/weather_server.py --city 深圳

    指定端口:
    python3 scripts/weather_server.py --port 9999
"""

import socket
import select
import time
import signal
import sys
import json
import argparse
import urllib.request
import urllib.error
import urllib.parse

# ============================================================
# 天气描述 英 → 中 翻译表
# ============================================================
WEATHER_CN = {
    "sunny":                "晴",
    "clear":                "晴",
    "partly cloudy":        "多云",
    "cloudy":               "阴",
    "overcast":             "阴",
    "mist":                 "雾",
    "fog":                  "雾",
    "freezing fog":         "雾",
    "light drizzle":        "小雨",
    "patchy light drizzle": "小雨",
    "light rain":           "小雨",
    "patchy light rain":    "阵雨",
    "moderate rain":        "中雨",
    "heavy rain":           "大雨",
    "moderate or heavy rain":"大雨",
    "light rain shower":    "阵雨",
    "moderate or heavy rain shower": "雷阵雨",
    "thunderstorm":         "雷阵雨",
    "torrential rain shower":"暴雨",
    "patchy rain possible": "可能有雨",
    "light snow":           "小雪",
    "moderate snow":        "中雪",
    "heavy snow":           "大雪",
}

def translate_weather(english_desc):
    """将英文天气描述翻译为中文（大小写不敏感）"""
    return WEATHER_CN.get(english_desc.strip().lower(), "多云")

# ============================================================
# 天气数据获取
# ============================================================

def fetch_weather(city):
    """从 wttr.in 获取实时天气，返回 dict 或 None"""
    url = f"https://wttr.in/{urllib.parse.quote(city)}?format=j1"
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "car-system"})
        with urllib.request.urlopen(req, timeout=10) as resp:
            data = json.loads(resp.read().decode("utf-8"))

        current = data["current_condition"][0]
        area = data["nearest_area"][0]

        weather_en = current["weatherDesc"][0]["value"]
        weather_cn = translate_weather(weather_en.strip().lower())

        # 取城市中文名（优先 areaName 的第一个值）
        location = area["areaName"][0]["value"]
        # 如果返回的是英文/拼音，用 region 的最高层级
        region = area["region"][0]["value"] if area["region"] else location

        return {
            "weather":  weather_cn,
            "temp":     int(current["temp_C"]),
            "location": city,  # 直接用用户指定的城市名，更干净
            "time":     int(time.time()),
        }
    except Exception as e:
        print(f"[Server] 获取天气失败: {e}")
        return None

# ============================================================
# TCP 服务器
# ============================================================

running = True

def handle_signal(sig, frame):
    global running
    print("\n[Server] 收到信号，正在关闭...")
    running = False

def main():
    parser = argparse.ArgumentParser(description="车机天气数据服务器")
    parser.add_argument("--host", default="0.0.0.0", help="监听地址 (默认 0.0.0.0)")
    parser.add_argument("--port", type=int, default=8888, help="监听端口 (默认 8888)")
    parser.add_argument("--city", default="广州", help="城市名 (默认 广州)")
    parser.add_argument("--interval", type=int, default=10, help="天气获取间隔秒数 (默认 10)")
    args = parser.parse_args()

    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((args.host, args.port))
    server.listen(1)
    print(f"[Server] 监听 {args.host}:{args.port}，城市={args.city}")
    print(f"[Server] 等待车机连接...")

    client = None
    weather_data = None
    last_fetch = 0

    while running:
        # 非阻塞等待客户端连接
        readable, _, _ = select.select([server], [], [], 1.0)

        if server in readable:
            client, addr = server.accept()
            print(f"[Server] 客户端已连接: {addr[0]}:{addr[1]}")
            # 延迟 2 秒，等待开发板 recv 就绪
            time.sleep(2)
            if weather_data:
                try:
                    client.sendall(weather_data)
                    print(f"[Server] 发送: {weather_data.decode().strip()}")
                except (BrokenPipeError, ConnectionResetError):
                    client.close()
                    client = None

        # 定期获取天气
        now = time.time()
        if now - last_fetch >= args.interval:
            last_fetch = now
            new_data = fetch_weather(args.city)
            if new_data:
                weather_data = (json.dumps(new_data, ensure_ascii=False) + "\n").encode("utf-8")
                print(f"[Server] 已缓存: {json.dumps(new_data, ensure_ascii=False)}")

        # 向已连接客户端发送最新数据
        if client and weather_data:
            try:
                client.sendall(weather_data)
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
