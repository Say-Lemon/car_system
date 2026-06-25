/**
 * @file vp_media_catalog.c
 * @brief 本地视频目录扫描实现
 *
 * 知识点：目录流 opendir / readdir / closedir，路径拼接，字符串过滤。
 * 在 ui_show_main_menu() 中调用一次，结果供 player_ui 列表与 playback_controller 使用。
 */

#include "vp_media_catalog.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>

char local_video_path[] = "/mmcblk/video";
char video_path[100][1024];
int video_num;

void media_catalog_scan(void)
{
    video_num = 0;

    DIR *dirp = opendir(local_video_path);
    if (dirp == NULL) {
        perror(local_video_path);
        return;
    }

    struct dirent *msg;
    while (1) {
        msg = readdir(dirp);
        if (msg == NULL)
            break;

        /* 跳过 . 与 .. 等隐藏项 */
        if (msg->d_name[0] == '.')
            continue;

        /* 后缀匹配（子串方式，练手项目可接受） */
        if (strstr(msg->d_name, ".avi") ||
            strstr(msg->d_name, ".mp4") ||
            strstr(msg->d_name, ".mkv")) {

            sprintf(video_path[video_num], "%s/%s", local_video_path, msg->d_name);
            puts(video_path[video_num]);
            video_num++;
        }
    }

    printf("检索视频完成 共%d\n", video_num);
    closedir(dirp);
}
