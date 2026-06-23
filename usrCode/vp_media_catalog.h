/**
 * @file media_catalog.h
 * @brief 本地视频目录扫描模块接口
 *
 * 启动时在 ./video 目录下查找 .avi / .mp4 / .mkv 文件，
 * 将完整路径填入 video_path[]，供播放列表与 mplayer 命令行使用。
 */

#ifndef MEDIA_CATALOG_H
#define MEDIA_CATALOG_H

/** 待扫描目录，默认 "./video"（相对可执行文件工作目录） */
extern char local_video_path[];

/** 最多缓存 100 个视频路径，每条最长 1024 字节 */
extern char video_path[100][1024];

/** 本次扫描到的有效视频文件个数 */
extern int video_num;

/**
 * @brief 遍历 local_video_path，填充 video_path 与 video_num
 *
 * 跳过以 '.' 开头的隐藏项；仅匹配包含指定扩展名的文件名。
 * 目录打开失败时 perror 并返回，video_num 保持为 0。
 */
void media_catalog_scan(void);

#endif /* MEDIA_CATALOG_H */
