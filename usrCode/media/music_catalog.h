/**
 * @file music_catalog.h
 * @brief 音乐文件目录扫描
 */

#ifndef MUSIC_CATALOG_H
#define MUSIC_CATALOG_H

#define MAX_MUSIC_FILES 200
#define MUSIC_PATH_LEN   512

extern char g_music_path[MAX_MUSIC_FILES][MUSIC_PATH_LEN];
extern int  g_music_count;

void music_catalog_scan(void);

#endif /* MUSIC_CATALOG_H */
