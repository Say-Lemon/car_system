/**
 * @file music_catalog.c
 * @brief 扫描 /mmcblk/music 获取 .mp3 文件列表
 */
#include "music_catalog.h"
#include "app_config.h"
#include <string.h>

char g_music_path[MAX_MUSIC_FILES][MUSIC_PATH_LEN];
int  g_music_count = 0;

void music_catalog_scan(void)
{
    DIR *dir = opendir(MUSIC_DIR);
    if (dir == NULL) {
        printf("[MusicCatalog] 无法打开 %s\n", MUSIC_DIR);
        g_music_count = 0;
        return;
    }

    g_music_count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && g_music_count < MAX_MUSIC_FILES) {
        if (entry->d_type == DT_DIR) continue;
        const char *name = entry->d_name;
        const char *dot = strrchr(name, '.');
        if (dot && strcasecmp(dot, ".mp3") == 0) {
            snprintf(g_music_path[g_music_count], MUSIC_PATH_LEN,
                     "%s/%s", MUSIC_DIR, name);
            g_music_count++;
        }
    }
    closedir(dir);
    printf("[MusicCatalog] 找到 %d 首歌曲\n", g_music_count);
}
