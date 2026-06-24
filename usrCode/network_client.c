/**
 * @file network_client.c
 * @brief TCP 客户端线程 — 接收 JSON 天气数据并同步到 UI
 *
 * 数据流:
 *   connect(SERVER_IP:SERVER_PORT) → recv 循环 → 缓冲找 '\n'
 *   → cJSON 解析 → g_net_mutex → 写入全局变量 → lv_async_call
 *
 * 架构遵循 can_simulator.c 模式: detached 线程 + mutex + lv_async_call。
 * 锁仅覆盖 strncpy/int 赋值（最小临界区），cJSON 解析在锁外完成。
 */

#include "network_client.h"
#include "app_config.h"
#include "car_ui_status.h"
#include "cJSON/cJSON.h"
#include "lvgl/lvgl.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

/* ========== 常量 ========== */
#define NET_RECONNECT_SEC  3
#define NET_RECV_BUF_SIZE  512

/* ========== 内部辅助 ========== */

/** 解析一行 JSON 并更新全局变量（锁内仅赋值，锁外解析） */
static void parse_and_update(const char *json_str)
{
    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        const char *err = cJSON_GetErrorPtr();
        if (err) fprintf(stderr, "[NET] JSON 解析失败: %s\n", err);
        return;
    }

    /* 提取字段到临时变量（锁外，避免阻塞 UI 读取） */
    char   weather_tmp[32]  = "";
    int    temp_tmp         = g_temp_celsius;   /* 默认保持当前值 */
    char   location_tmp[64] = "";

    cJSON *w = cJSON_GetObjectItem(root, "weather");
    if (cJSON_IsString(w)) strncpy(weather_tmp, w->valuestring, sizeof(weather_tmp) - 1);

    cJSON *t = cJSON_GetObjectItem(root, "temp");
    if (cJSON_IsNumber(t)) temp_tmp = (int)t->valuedouble;

    cJSON *l = cJSON_GetObjectItem(root, "location");
    if (cJSON_IsString(l)) strncpy(location_tmp, l->valuestring, sizeof(location_tmp) - 1);

    cJSON_Delete(root);

    /* 临界区：仅做内存拷贝/赋值 */
    pthread_mutex_lock(&g_net_mutex);
    if (weather_tmp[0])  strncpy(g_weather_str,  weather_tmp,  sizeof(g_weather_str)  - 1);
    if (location_tmp[0]) strncpy(g_location_str, location_tmp, sizeof(g_location_str) - 1);
    g_temp_celsius = temp_tmp;
    pthread_mutex_unlock(&g_net_mutex);

    /* 推送 UI 刷新到主线程 */
    lv_async_call(ui_refresh_status_labels, NULL);
}

/* ========== 网络线程主循环 ========== */
static void *network_client_thread(void *arg)
{
    (void)arg;

    int                sock = -1;
    struct sockaddr_in addr;
    char               recv_buf[NET_RECV_BUF_SIZE];
    int                buf_len = 0;

    /* 预填充地址结构（不变） */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(SERVER_PORT);
    addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    while (1) {
        /* ---- 连接阶段 ---- */
        while (sock < 0) {
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                perror("[NET] socket");
                sleep(NET_RECONNECT_SEC);
                continue;
            }

            printf("[NET] 正在连接 %s:%d ...\n", SERVER_IP, SERVER_PORT);
            if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
                fprintf(stderr, "[NET] 连接失败 (%s)，%d 秒后重试\n",
                        strerror(errno), NET_RECONNECT_SEC);
                close(sock);
                sock = -1;
                sleep(NET_RECONNECT_SEC);
            }
        }

        printf("[NET] 已连接到服务器\n");
        buf_len = 0;

        /* ---- 接收循环 ---- */
        while (1) {
            int n = recv(sock, recv_buf + buf_len,
                         sizeof(recv_buf) - buf_len - 1, 0);

            if (n <= 0) {
                if (n == 0)
                    fprintf(stderr, "[NET] 服务器断开连接\n");
                else
                    fprintf(stderr, "[NET] recv 错误: %s\n", strerror(errno));
                break;  /* 跳出接收循环，进入重连 */
            }

            buf_len += n;
            recv_buf[buf_len] = '\0';

            /* 扫描完整行（以 '\n' 结束） */
            char *nl;
            while ((nl = memchr(recv_buf, '\n', buf_len)) != NULL) {
                *nl = '\0';  /* 截断为 C 字符串 */
                parse_and_update(recv_buf);

                /* 移动剩余数据到缓冲区头部 */
                int remaining = buf_len - (int)(nl - recv_buf) - 1;
                if (remaining > 0)
                    memmove(recv_buf, nl + 1, remaining);
                buf_len = remaining;
            }

            /* 缓冲区保护：无换行且接近上限则丢弃 */
            if (buf_len >= (int)sizeof(recv_buf) - 1) {
                fprintf(stderr, "[NET] 缓冲区溢出，丢弃 %d 字节\n", buf_len);
                buf_len = 0;
            }
        }

        /* 断开，准备重连 */
        close(sock);
        sock = -1;
        sleep(NET_RECONNECT_SEC);
    }

    return NULL;
}

/* ========== 对外接口 ========== */
void network_client_start(void)
{
    pthread_t tid;
    pthread_create(&tid, NULL, network_client_thread, NULL);
    pthread_detach(tid);
    printf("[NET] 网络接收线程已启动\n");
}
