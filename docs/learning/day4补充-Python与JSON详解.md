# Day 4 补充 — Python 脚本与 JSON/cJSON 详解

## 1. JSON 是什么？

JSON（JavaScript Object Notation）是一种**纯文本数据格式**，用来在不同程序之间传递数据。

### 1.1 一个 JSON 就是一串文本

```json
{"weather":"晴","temp":28,"location":"广州","time":1782303797}
```

**JSON 和 C 语言的对比**：

| JSON | C 语言 |
|------|--------|
| `{ }` | `struct` |
| `"key":"value"` | `成员名 = 值` |
| `"字符串"` | `char s[] = "..."` |
| `数字` | `int` / `float` |
| `true/false` | `bool` |

**上面那串 JSON 用 C 语言表达就是**：

```c
struct Weather {
    char weather[32];
    int  temp;
    char location[64];
    int  time;
} data = {
    .weather  = "晴",
    .temp     = 28,
    .location = "广州",
    .time     = 1782303797
};
```

JSON 就是把这行数据的"文本版"发过去，接收方再"把文本还原成数据"。

### 1.2 JSON 的基本规则

```json
{
    "key1": "字符串值",           // 双引号包起来
    "key2": 123,                  // 数字不包引号
    "key3": true,                 // 布尔值
    "key4": [1, 2, 3],           // 数组（方括号）
    "key5": { "nested": true }    // 嵌套对象（花括号）
}
```

**必须记住**：JSON 键名和字符串值都用**双引号**，不能用单引号。数字不加引号。

---

## 2. Python 脚本是怎么跑起来的？

### 2.1 Python 是什么

Python 是一种解释型语言，不需要编译。写好 `.py` 文件直接运行：

```bash
python weather_server.py
# 或
python3 weather_server.py
```

**和 C 的区别**：

| | C | Python |
|---|-----|--------|
| 编译 | 需要 `arm-linux-gcc` | 不需要 |
| 运行 | 运行编译好的二进制 | 一行行解释执行 |
| 变量 | `int x = 5;` | `x = 5` |
| 函数 | `void foo(int a)` | `def foo(a):` |
| 缩进 | `{ }` 大括号 | 缩进（空格数必须一致） |
| 分号 | 必须 `;` | 不需要 |

### 2.2 weather_server.py 逐段解读

**第 1 段：导入模块**（相当于 C 的 `#include`）

```python
import socket    # 网络通信   → 相当于 #include <sys/socket.h>
import json      # JSON 处理  → 相当于 #include "cJSON.h"
import time      # 时间函数   → 相当于 #include <time.h>
import urllib.request  # HTTP 请求 → 相当于 curl 命令
```

**第 2 段：定义翻译表**（相当于 C 的 switch-case 或查找表）

```python
WEATHER_CN = {
    "sunny":      "晴",
    "cloudy":     "阴",
    "light rain": "小雨",
    # ... 共 20+ 条
}
# 这相当于 C 语言里的：
# char *translate(char *en) {
#     if (strcmp(en, "sunny") == 0) return "晴";
#     if (strcmp(en, "cloudy") == 0) return "阴";
#     ...
# }
```

**第 3 段：获取真实天气**

```python
def fetch_weather(city):
    url = f"https://wttr.in/{city}?format=j1"  # 构造 URL
    with urllib.request.urlopen(url) as resp:   # 发 HTTP 请求
        data = json.loads(resp.read())           # 解析返回的 JSON

    # 提取需要的字段
    weather_en = data["current_condition"][0]["weatherDesc"][0]["value"]
    weather_cn = WEATHER_CN.get(weather_en, "多云")  # 翻译

    return {
        "weather":  weather_cn,
        "temp":     int(data["current_condition"][0]["temp_C"]),
        "location": city,
        "time":     int(time.time()),
    }
```

**这段代码做了什么**：
1. 构造 URL：`https://wttr.in/广州?format=j1`
2. 发 HTTP GET 请求（相当于浏览器打开这个网址）
3. 服务器返回一大坨 JSON（天气数据）
4. `json.loads()` 把 JSON 文本解析成 Python 字典
5. 提取温度、天气描述
6. 从英文翻译成中文
7. 返回一个 Python 字典

**第 4 段：TCP 服务器**

```python
server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)  # 创建 socket
server.bind(('0.0.0.0', 8888))   # 绑定端口
server.listen(1)                   # 开始监听

while True:
    client, addr = server.accept()  # 等待客户端连接（阻塞）
    # ...发数据...
    client.sendall(json_str)        # 发送
```

**和 C 语言网络编程的对照**：

|       Python           |              C                 |
|------------------------|--------------------------------|
| `socket.socket()`      | `socket(AF_INET, SOCK_STREAM)` |
| `server.bind(...)`     | `bind(sock, ...)`              |
| `server.listen(1)`     | `listen(sock, 1)`              |
| `server.accept()`      | `accept(sock, ...)`            |
| `client.sendall(data)` | `send(sock, buf, len, 0)`      |

你会发现 Python 版本的每一行，在 C 版本里都有一一对应。Python 只是把 C 的繁琐步骤封装得更简洁。

---

## 3. cJSON 库 — C 语言怎么处理 JSON

### 3.1 为什么需要 cJSON

C 语言没有内置的 JSON 处理能力。`{"weather":"晴"}` 这段文本在 C 里只是一串 char 数组，需要自己解析。cJSON 就是做这件事的库。

**cJSON 做了什么**：

```
输入的文本: {"weather":"晴","temp":28}
               ↓ cJSON_Parse()
         ┌─────────────────┐
         │ cJSON 对象树     │    ← 一棵内存中的树
         │ weather = "晴"   │
         │ temp = 28        │
         └─────────────────┘
               ↓ cJSON_GetObjectItem()
         C 变量: char weather[32], int temp
```

### 3.2 解析（JSON 文本 → C 变量）— `parse_and_update`

打开 `network/network_client.c`，这是项目中唯一用 cJSON 的地方：

```c
static void parse_and_update(const char *json_str)
{
    // 第 1 步：解析 JSON 文本 → 内存树
    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        // 解析失败（格式不对）
        return;
    }

    // 第 2 步：从树里提取各个字段
    cJSON *w = cJSON_GetObjectItem(root, "weather");  // 找 "weather"
    if (cJSON_IsString(w))
        strncpy(weather_tmp, w->valuestring, 31);     // 取其字符串值

    cJSON *t = cJSON_GetObjectItem(root, "temp");     // 找 "temp"
    if (cJSON_IsNumber(t))
        temp_tmp = (int)t->valuedouble;               // 取其数值

    // 第 3 步：释放树
    cJSON_Delete(root);

    // 第 4 步：把临时变量赋值给全局变量（加锁保护）
    pthread_mutex_lock(&g_net_mutex);
    strncpy(g_weather_str, weather_tmp, 31);
    g_temp_celsius = temp_tmp;
    pthread_mutex_unlock(&g_net_mutex);
}
```

**cJSON 的树结构**（可视化的）：

```
JSON: {"weather":"晴","temp":28,"location":"广州"}

解析后的 cJSON 树:
                  root (Object)
                 /      |       \
         "weather"   "temp"    "location"
            |          |          |
          "晴"         28      "广州"
        (string)    (number)   (string)

cJSON_GetObjectItem(root, "temp") → 返回指向 "28" 节点的指针
cJSON_IsNumber(t)                 → 检查这个节点是不是数字类型
t->valuedouble                    → 取出数字值（cJSON 用 double 存数字）
```

### 3.3 序列化（C 变量 → JSON 文本）— `settings_config_save`

```c
void settings_config_save(void)
{
    // 第 1 步：创建空的 JSON 对象
    cJSON *root = cJSON_CreateObject();

    // 第 2 步：往里面加字段
    cJSON_AddNumberToObject(root, "volume",   g_sys_volume);       // "volume":10
    cJSON_AddNumberToObject(root, "brightness", g_sys_brightness); // "brightness":70
    cJSON_AddStringToObject(root, "bt_name",  g_bluetooth_name);  // "bt_name":"GEC6818-CAR"

    // 第 3 步：把树 "打印" 成文本
    char *json = cJSON_PrintUnformatted(root);
    // json 现在指向: {"volume":10,"brightness":70,"bt_name":"GEC6818-CAR"}

    // 第 4 步：写入文件
    FILE *fp = fopen("./config.json", "w");
    fputs(json, fp);
    fclose(fp);

    // 第 5 步：释放
    free(json);          // cJSON_Print 分配的内存要手动释放
    cJSON_Delete(root);  // cJSON_Create 分配的内存
}
```

**和 Python 版本的完全对应**：

| Python                 | cJSON (C)                                 |
|------------------------|-------------------------------------------|
| `json.loads(text)`     | `cJSON_Parse(text)`                       |
| `data["weather"]`      | `cJSON_GetObjectItem(root, "weather")`    |
| `json.dumps(dict)`     | `cJSON_PrintUnformatted(root)`            |
| 自动垃圾回收            | `cJSON_Delete(root)` + `free(json)`       |

---

## 4. 数据流完整追踪

```
┌─────────────────────────────────────────────────────────────┐
│ 天气服务器 (Python)                                          │
│                                                              │
│ 1. urllib → wttr.in API → 获取广州天气                        │
│    返回: {"current_condition":[{"temp_C":35,                 │
│            "weatherDesc":[{"value":"Partly cloudy"}]}]}       │
│                                                              │
│ 2. json.loads() 解析 → 提取 temp_C=35, value="Partly cloudy" │
│                                                              │
│ 3. 翻译: "Partly cloudy" → "多云"                             │
│                                                              │
│ 4. 构造: {"weather":"多云","temp":35,"location":"广州",        │
│            "time":1782303797}                                 │
│                                                              │
│ 5. json.dumps() → 文本 → socket.sendall() → 发到 TCP 8888    │
└──────────────────────┬──────────────────────────────────────┘
                       │ TCP 数据流
┌──────────────────────▼──────────────────────────────────────┐
│ 车机 C 程序                                                  │
│                                                              │
│ 6. recv() → 收到文本 "{\"weather\":\"多云\",...}\n"          │
│                                                              │
│ 7. cJSON_Parse() → 内存树                                    │
│                                                              │
│ 8. cJSON_GetObjectItem() → strncpy() → g_weather_str = "多云"│
│                                                              │
│ 9. lv_async_call() → ui_refresh_status_labels()              │
│     → lv_label_set_text() → 屏幕显示 "广州 35° 多云"          │
└──────────────────────────────────────────────────────────────┘
```

---

## 5. 动手任务

### 任务 1：自己写一个简单的 JSON

在 WSL2 终端里：

```bash
# Python 交互模式
python3
```

然后输入：

```python
import json

# 创建一个字典
data = {"name": "测试", "value": 123}

# 转成 JSON 文本
text = json.dumps(data, ensure_ascii=False)
print(text)   # 输出: {"name": "测试", "value": 123}

# 把文本转回字典
parsed = json.loads(text)
print(parsed["name"])  # 输出: 测试
```

### 任务 2：手动构造天气数据发给开发板

```bash
# WSL2 终端
echo '{"weather":"手动测试","temp":99,"location":"实验室","time":'$(date +%s)'}' | nc -l 8888
```

然后在开发板上启动 `car_system`，观察状态栏是否显示"实验室 99° 手动测试"。

### 任务 3：追踪 cJSON 的内存管理

打开 `system/settings_config.c`，找到 `settings_config_save()` 函数。标注每一步分配了什么内存、在哪里释放。画一个内存分配和释放的对应图。


学习心得:
1. 所以服务器端（python脚本）做的是 "大 JSON → 提取关键字段 → 小 JSON → 发给车机"。开发板收到的只有几十个字节的精简JSON，不需要处理 wttr.in 那几百行的原始数据。

2. 客户端做的是"解析小 JSON → 更新全局变量 → 更新UI"

3. 处理JSON文本的方法:
| Python                 | cJSON (C)                                 |
|------------------------|-------------------------------------------|
| `json.loads(text)`     | `cJSON_Parse(text)`                       |
| `data["weather"]`      | `cJSON_GetObjectItem(root, "weather")`    |
| `json.dumps(dict)`     | `cJSON_PrintUnformatted(root)`            | 
| 自动垃圾回收            | `cJSON_Delete(root)` + `free(json)`       |

4. Python与c语言处理JSON文本逻辑完全一样，但写法差别很大。
  同样一件事：从 JSON 里取 "weather" 的值

  Python（内置支持）：
  data = json.loads(text)                              # 解析 → 得到字典
  weather = data["weather"]                            # 直接取

  C（需要 cJSON 库）：
  cJSON *root = cJSON_Parse(text);                    // 解析 → 得到树
  cJSON *w = cJSON_GetObjectItem(root, "weather");    // 遍历树找节点
  if (cJSON_IsString(w))                              // 检查类型
      strncpy(buf, w->valuestring, 31);               // 取值
  cJSON_Delete(root);                                 // 手动释放

  核心区别就两点
  ┌──────────┬──────────────────────────────────────────┬──────────────────────────────────┐
  │          │                  Python                  │            C + cJSON             │
  ├──────────┼──────────────────────────────────────────┼──────────────────────────────────┤
  │ 解析结果  │ 直接得到 字典（和 C 的 struct 一样直观）    │ 得到一棵 树，要手动遍历           │
  ├──────────┼──────────────────────────────────────────┼──────────────────────────────────┤
  │ 内存      │ 自动垃圾回收，不用管                       │ 必须手动 cJSON_Delete() + free() │
  └──────────┴──────────────────────────────────────────┴──────────────────────────────────┘
  Python 的 json.loads() 一步到位把 JSON 转成了 Python 字典，直接用 [] 取值。C 的 cJSON多了一步"构建树→遍历树→取叶子节点→释放树"的过程。

5. 服务器与客户端之间的通信用的是TCP通信:
服务器:
|       Python           |              C                 |
|------------------------|--------------------------------|
| `socket.socket()`      | `socket(AF_INET, SOCK_STREAM)` |
| `server.bind(...)`     | `bind(sock, ...)`              |
| `server.listen(1)`     | `listen(sock, 1)`              |
| `server.accept()`      | `accept(sock, ...)`            |
| `client.sendall(data)` | `send(sock, buf, len, 0)`      |
客户端:
| `socket()` — 创建套接字
| `connect()` — 连接服务器
| `recv()` — 接收数据
| `close()` — 断开
