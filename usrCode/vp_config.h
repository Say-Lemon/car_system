/**
 * @file vp_config.h
 * @brief 视频播放器配置（复用 player_project）
 */
#ifndef VP_CONFIG_H
#define VP_CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <math.h>

#define FIFO_PATH   "/tmp/video_control"
#define VIDEO_DIR   "/mmcblk/video"

#endif
