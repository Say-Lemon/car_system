# 第 3 天：多线程 + IPC — 音乐播放器核心

**目标：理解 Linux 多线程编程在实际项目中的应用**

**重点文件：`media/music_controller.c` — 整个项目最有技术含量的文件**

## 1. 你已有的基础知识

| 已学课程 | 今天会用到的知识点 |
|---------|-------------------|
| 系统编程 | `fork()`, `pipe()`, `dup2()`, `execlp()` |
| 系统编程 | `pthread_create()`, `pthread_mutex_lock()`, `pthread_cond_wait()` |
| 系统编程 | `signal()`, `kill()`, `usleep()` |
| 文件 IO | `fdopen()`, `fgets()`, `write()`, `fprintf()` |

## 2. 整体架构：4 线程 + 1 子进程

```
                     ┌──────────────────────┐
                     │   main (LVGL UI)      │  主线程
                     └──────┬───────────────┘
                            │ 点击播放
                            ▼
              ┌─────────────────────────┐
              │   Launch 线程 (临时)      │  fork + pipe
              │   每次切歌创建并 detach  │  启动 mplayer
              └──────┬────┬─────────────┘
                     │    │
          stdin pipe │    │ stdout pipe
           (写命令)  │    │ (读应答)
                     ▼    ▼
     ┌─────────────┐    ┌──────────────┐
     │ Writer 线程  │    │  Reader 线程  │
     │ 每秒发查询   │    │ 解析 ANS_*   │
     │ get_time_pos │    │ 更新全局变量  │
     │ get_percent  │    │ 检测 EOF     │
     └─────────────┘    └──────┬───────┘
                               │
                         mplayer 子进程
                         /dev/dsp (音频)
```

**为什么需要 4 个线程？** 因为 mplayer 是"一问一答"模式——你发 `get_time_pos\n` 命令，它回复 `ANS_TIME_POSITION=12.3`。你不能在发命令时阻塞等待回复，所以读和写分在两个线程。

## 3. 核心模式 1：fork + pipe 启动子进程

```c
// 第一步：创建两个管道
int in_pipe[2], out_pipe[2];
pipe(in_pipe);   // [0]=读端 [1]=写端  → 父进程写 in_pipe[1]，子进程读 in_pipe[0]
pipe(out_pipe);  // [0]=读端 [1]=写端  → 子进程写 out_pipe[1]，父进程读 out_pipe[0]

// 第二步：fork 子进程
pid_t pid = fork();
if (pid == 0) {
    // 子进程：重定向 stdin/stdout 到管道
    close(in_pipe[1]);   close(out_pipe[0]);
    dup2(in_pipe[0],  STDIN_FILENO);   // stdin = 管道读端
    dup2(out_pipe[1], STDOUT_FILENO);  // stdout = 管道写端
    close(in_pipe[0]);   close(out_pipe[1]);

    // 执行 mplayer（替换当前进程映像）
    execlp("mplayer", "mplayer", "-quiet", "-slave", "-novideo",
           g_music_path[g_music_index], NULL);
    _exit(1);  // execlp 成功后不会执行到这里
}

// 父进程：关闭不用的管道端
close(in_pipe[0]);   close(out_pipe[1]);

// 保留 in_pipe[1] 用于写命令，out_pipe[0] 用于读应答
music_mplayer_pid = pid;
g_fd_music_fifo = in_pipe[1];
g_fp_music_mplayer = fdopen(out_pipe[0], "r");
```

**对照系统编程课**：这就是 fork+pipe+dup2+exec 四件套的标准用法。复习一下：
1. `pipe()` 创建通信通道
2. `fork()` 创建子进程
3. `dup2()` 重定向标准输入/输出到管道
4. `execlp()` 执行新程序

## 4. 核心模式 2：Reader 线程 — 解析 mplayer 应答

```c
void *music_reader_thread(void *arg) {
    char line[256];
    while (1) {
        fgets(line, sizeof(line), g_fp_music_mplayer);  // 阻塞读取一行

        if (strstr(line, "ANS_TIME_POSITION")) {
            sscanf(line, "ANS_TIME_POSITION=%f", &g_music_time_pos);
        }
        else if (strstr(line, "ANS_PERCENT_POSITION")) {
            sscanf(line, "ANS_PERCENT_POSITION=%d", &g_music_percent_pos);
        }
        else if (strstr(line, "ANS_LENGTH")) {
            sscanf(line, "ANS_LENGTH=%f", &g_music_time_length);
        }
    }
}
```

**mplayer 的 slave 协议**：以 `ANS_` 开头的行是命令响应。常见的：
| 命令 | 响应格式 |
|------|---------|
| `get_time_pos` | `ANS_TIME_POSITION=45.2` |
| `get_percent_pos` | `ANS_PERCENT_POSITION=67` |
| `get_time_length` | `ANS_LENGTH=240.0` |
| `seek 50 1` | 跳到 50% 位置 |
| `volume 5 1` | 设置音量为 5 |
| `pause` | 暂停 |

## 5. 核心模式 3：暂停/恢复的同步难题

这是项目中最精妙的部分。

**朴素的错误做法**（为什么不行）：
```c
// ❌ 错误：竞态条件！
pthread_kill(reader, SIGUSR1);   // 通知 reader 暂停
// reader 的 SIGUSR1 handler:
void handler() {
    pthread_cond_wait(&cond);     // reader 在这等待
}
pthread_cond_signal(&cond);      // 唤醒 reader
// 问题：如果 signal 在 cond_wait 之前到达，信号丢失，reader 永久阻塞！
```

**项目的正确做法**（标志位 + 信号中断 fgets）：
```c
// ===== 暂停 =====
g_music_io_paused = 1;                   // 1. 设标志位
pthread_kill(g_tid_music_read, SIGUSR1); // 2. 发信号中断 fgets
usleep(50000);                           // 3. 等待 reader 响应
kill(music_mplayer_pid, SIGSTOP);       // 4. 停止 mplayer 进程

// Reader 主循环：
if (g_music_io_paused) {                 // 检查标志位
    pthread_mutex_lock(&g_music_rd_mutex);
    pthread_cond_wait(&g_music_rd_cond, &g_music_rd_mutex); // 阻塞等待
    pthread_mutex_unlock(&g_music_rd_mutex);
    continue;
}

fgets(line, ..., g_fp_music_mplayer);    // 阻塞读取

// 信号处理（仅设标志位）：
void handler(int sig) {
    g_reader_sig_flag = 1;               // 设标志（async-signal-safe）
}
// fgets 被信号中断后返回 NULL+EINTR
// reader 检查 g_reader_sig_flag → 继续检查 g_music_io_paused → cond_wait
```

**为什么这样是安全的？**
- `pthread_kill` 发信号 → 中断 `fgets` 阻塞
- Reader 从 `fgets` 返回 → 检查标志位 → 进入 `cond_wait` 
- 主线程 `usleep(50000)` 等待 → 然后 `SIGSTOP`
- 恢复时：`SIGCONT` → `cond_signal` → reader 继续

## 6. 核心模式 4：Seek（拖动进度条）的无锁方案

```c
// 旧版（有竞态）：需要暂停 reader + writer，condvar 同步
// 新版（无竞态）：直接发命令
music_send_cmd("seek %d 1\n", target_pct);  // seek <val> 1 = 绝对百分比
```

**为什么旧版会卡死？** seek 时需要暂停读写线程避免进度乱跳。但暂停用信号+condvar 链式触发（reader 暂停→通知 writer 暂停），存在"信号在 cond_wait 之前发出"的窗口 → writer 永久阻塞。

**新版为什么可以？** `seek <val> 1` 是**绝对百分比**跳转，mplayer 内部处理，不需要暂停任何线程。这是理解 mplayer slave 协议细节的重要性。

## 7. 自动下一首

```c
// Reader 线程中
if (g_music_percent_pos >= 98) {
    g_music_eof = true;  // 设标志位，不直接切歌！
}

// 主线程 500ms 定时器中
if (g_music_eof) {
    g_music_eof = false;
    music_play_next();   // 在主线程安全切歌
}
```

**为什么不在 Reader 线程直接切歌？** 因为切歌会调用 `music_play_current()` → `lv_label_set_text()` 等 LVGL 函数。**LVGL 不是线程安全的！所有 UI 操作必须在主线程执行。**

## 8. 动手任务

### 任务 1：画线程时序图
画一张图，展示播放一首歌的完整生命周期：
```
用户点播放 → Launch线程(fork)  → reader(data) → writer(cmd)
                                → ...播放中...  
                                → percent≥98% → 设 g_music_eof
→ 主线程定时器 → music_play_next → Launch(fork) → ...

标注每个步骤在哪个线程执行
```

### 任务 2：验证同步机制
在 `music_toggle_pause()` 的暂停和恢复分支各加一个 `printf`，观察终端输出。看看暂停→恢复的时序。

### 任务 3：对比音乐和视频播放器
打开 `media/vp_playback_controller.c`，对比音乐和视频播放器的 fork+pipe 实现。找出相同点和不同点。


学习心得:
1.  用 双匿名管道 通信比 匿名管道popen+命名管道FIFO 好

2. 进程创建 fork+pipe+close+dup2+exec 五件套的标准用法。复习一下：
 `pipe()` 创建通信匿名管道
 `fork()` 创建子进程
 `close()` 关闭无用的读/写端
 `dup2()` 重定向标准输入/输出到管道
 `execlp()` 执行新程序

3.  当前项目3 用的是双匿名管道
因为 mplayer 只认 stdin/stdout。子进程(mplayer)需要将管道复制到stdin/stdout:
dup2(in_pipe[0],  STDIN_FILENO);   // fd 3 复制到 fd 0 (stdin)
dup2(out_pipe[1], STDOUT_FILENO);  // fd 6 复制到 fd 1 (stdout)

4. 原来项目2（视频播放器）用的是匿名管道popen+命名管道FIFO
写入（发送命令）→ FIFO 命名管道，不是 stdin
snprintf(cmd, sizeof(cmd),
         "mplayer -quiet -slave -zoom -x 680 -y 362 -input file=%s \"%s\"",
         FIFO_PATH, video_path[video_index]);
关键参数是 -input file=/tmp/mplayer_control，它告诉 MPlayer：从命名管道 FIFO 读取 slave 命令，而不是从 stdin 读。

读取（接收应答）→ stdout 匿名管道
fp_mplayer = popen(cmd, "r");   // "r" = 捕获 stdout
popen(cmd, "r") 创建了一个匿名管道，连接 MPlayer 的 stdout。读线程通过 fgets 读取 MPlayer 输出的 ANS_* 应答行
本质上就是 dup2(pipe[1], STDOUT_FILENO) 把子进程的 stdout 重定向到了管道，只是 popen() 帮你把这些步骤封装好了

