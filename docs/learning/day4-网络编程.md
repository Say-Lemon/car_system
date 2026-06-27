# 第 4 天：网络编程 — JSON + TCP 通信

**目标：理解嵌入式设备如何与 PC 服务端通信**

## 1. 你已有的基础知识

| 已学课程 | 今天会用到的知识点 |
|---------|-------------------|
| 网络编程 | `socket()`, `connect()`, `recv()`, `send()` |
| 网络编程 | `struct sockaddr_in`, `htons()`, `inet_addr()` |
| 文件 IO | JSON 格式、cJSON 解析库 |

## 2. 端到端数据流

```
┌──────────────────────────────────────────────────────────────────┐
│ PC (192.168.137.1)                                                │
│                                                                    │
│   weather_server.py                                                │
│     1. urllib.request → wttr.in API → 获取广州天气 JSON            │
│     2. 英→中翻译 (Weather_CN dict)                                 │
│     3. 添加 time 字段 (int(time.time()))                           │
│     4. socket.sendall(json_str + '\n')  → 每 10 秒一次              │
│                                                                    │
│   原始 JSON 格式:                                                   │
│   {"weather":"雷阵雨","temp":34,"location":"广州","time":1782303797} │
└──────────────────────┬───────────────────────────────────────────┘
                       │ TCP 8888 端口
                       ▼
┌──────────────────────────────────────────────────────────────────┐
│ GEC6818 开发板                                                     │
│                                                                    │
│   network/network_client.c                                         │
│     network_client_thread (detached 后台线程):                      │
│     1. socket() + connect() → 连接服务器                            │
│     2. recv() 循环 → 缓冲累积直到 '\n'                              │
│     3. cJSON_Parse() 解析 JSON                                     │
│     4. pthread_mutex_lock(&g_net_mutex) → 写全局变量                │
│     5. lv_async_call(ui_refresh_status_labels) → 安全推送到 UI 线程  │
│     6. 首次收到 time 字段 → settimeofday() 同步系统时钟              │
└──────────────────────────────────────────────────────────────────┘
```

## 3. C 端代码详解

打开 `network/network_client.c`，关键分三个部分：

### 部分 1：连接与重连

```c
static void *network_client_thread(void *arg) {
    int sock = -1;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);        // 8888 → 网络字节序
    addr.sin_addr.s_addr = inet_addr(SERVER_IP); // "192.168.137.1" → 32位整数

    while (1) {
        // 连接循环：失败则 3 秒后重试
        while (sock < 0) {
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
                close(sock); sock = -1;
                sleep(3);  // 等待 3 秒重试
            }
        }
        printf("[NET] 已连接到服务器\n");
```

**对照网络编程课**：这就是标准的 TCP 客户端四步：
1. `socket()` — 创建套接字
2. `connect()` — 连接服务器
3. `recv()` — 接收数据
4. `close()` — 断开

多了一个外层 `while(1)` 实现**自动重连**。

### 部分 2：缓冲与拆包

TCP 是**流式协议** — 数据像水流一样，没有"消息边界"。服务器发 `"hello\n"` 和 `"world\n"`，客户端可能一次收到 `"hello\nwor"`，第二次收到 `"ld\n"`。

所以需要**自己拆包**：

```c
char recv_buf[512];
int buf_len = 0;

while (1) {
    int n = recv(sock, recv_buf + buf_len, sizeof(recv_buf)-buf_len-1, 0);
    buf_len += n;
    recv_buf[buf_len] = '\0';

    // 找换行符('\n')——这就是我们定义的"消息边界"
    char *nl;
    while ((nl = memchr(recv_buf, '\n', buf_len)) != NULL) {
        *nl = '\0';                           // 截断为 C 字符串
        parse_and_update(recv_buf);           // 处理完整的一行 JSON

        // 把剩余数据移到缓冲区头部
        int remaining = buf_len - (nl - recv_buf) - 1;
        memmove(recv_buf, nl + 1, remaining);
        buf_len = remaining;
    }
}
```

**图解**：
```
收到: {"weather":"晴"...}\n{"weath
buf:  [..................................]
       ↑                              ↑
       处理第一行 ← parse_and_update   移到头部继续累积
```

### 部分 3：JSON 解析 + 线程安全

```c
static void parse_and_update(const char *json_str) {
    // === 阶段 A：锁外解析 JSON（不阻塞 UI 线程） ===
    cJSON *root = cJSON_Parse(json_str);
    
    char weather_tmp[32] = "";
    int  temp_tmp = g_temp_celsius;  // 默认保持当前值
    
    cJSON *w = cJSON_GetObjectItem(root, "weather");
    if (cJSON_IsString(w)) strncpy(weather_tmp, w->valuestring, ...);
    
    cJSON *t = cJSON_GetObjectItem(root, "temp");
    if (cJSON_IsNumber(t)) temp_tmp = (int)t->valuedouble;
    
    cJSON_Delete(root);  // 释放 JSON 树
    
    // === 阶段 B：锁内写全局变量（极短临界区） ===
    pthread_mutex_lock(&g_net_mutex);
    strncpy(g_weather_str, weather_tmp, ...);
    g_temp_celsius = temp_tmp;
    pthread_mutex_unlock(&g_net_mutex);
    
    // === 阶段 C：异步推送到 UI 线程 ===
    lv_async_call(ui_refresh_status_labels, NULL);
}
```

**为什么解析在锁外？** 如果 `cJSON_Parse` 在锁内，解析期间 UI 线程读取天气标签会阻塞。锁的临界区应该尽可能短 — 只有 strncpy 和 int 赋值。

**`lv_async_call` 的魔法**：网络线程不能直接调 `lv_label_set_text()`（LVGL 不是线程安全的）。`lv_async_call` 把函数指针放进 LVGL 内部队列，下一次 `lv_timer_handler()` 时在主线程安全执行。

## 4. 时间同步

```c
// 首次收到服务器时间戳时同步系统时钟
static bool time_synced = false;
if (!time_synced) {
    cJSON *tm = cJSON_GetObjectItem(root, "time");
    if (cJSON_IsNumber(tm)) {
        struct timeval tv;
        tv.tv_sec = (time_t)tm->valuedouble;
        settimeofday(&tv, NULL);          // 设置系统时钟
        setenv("TZ", "CST-8", 1);         // 时区：中国标准时间 UTC+8
        tzset();                          // 应用时区设置
        lv_async_call(car_ui_status_clock_force_refresh, NULL); // 立即刷新时钟
        time_synced = true;
    }
}
```

**为什么开发板时间是 2015/1/1？** 没有 RTC 电池，每次重启时钟归零。只能从网络获取。

## 5. Python 服务端

打开 `scripts/weather_server.py`，对照网络编程课：

```python
# TCP 服务器
server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)  # 端口重用
server.bind(('0.0.0.0', 8888))
server.listen(1)

# 非阻塞 accept（用 select 避免阻塞 Ctrl+C）
readable, _, _ = select.select([server], [], [], 1.0)
if server in readable:
    client, addr = server.accept()

# 获取真实天气
url = f"https://wttr.in/{city}?format=j1"
with urllib.request.urlopen(url) as resp:
    data = json.loads(resp.read())
    weather_cn = WEATHER_CN.get(data['current_condition'][0]['weatherDesc'][0]['value'])

# 发送
client.sendall((json.dumps(weather) + '\n').encode('utf-8'))
```

## 6. 动手任务

### 任务 1：用 nc 命令测试
```bash
# PC 终端
echo '{"weather":"手动测试","temp":99,"location":"实验室","time":1234567890}' | nc -l 8888

# 开发板终端
./car_system
# 观察状态栏是否显示"实验室 99° 手动测试"
```

### 任务 2：追踪一条数据的完整路径
从服务器生成 JSON 开始，到最后 UI 更新。画出流程中每一个函数调用。

### 任务 3：修改重连时间
把 `NET_RECONNECT_SEC` 从 3 改成 1，重新编译，观察：关闭服务器后重启，客户端多久能重新连上？
