# Simple Music Player

一个基于 C++ 开发的轻量级终端音乐播放器, 支持歌词解析与显示

<img width="400" height="350" alt="image" src="https://github.com/user-attachments/assets/71d2e8d5-8448-466b-8e2b-3475ed26dfee" />


本项目大部分内容由AI辅助生成

## 项目简介

在linux上，我一直找不到一个能很好的解析歌曲元数据（包括歌词）的音乐播放器，因此产生了自己写一个的想法。

**当前版本：v1.1.0**  
**发布日期：2026-03-12**

------------------------------------------------------------------------

## 目录结构

``` text
simple_music/
├── build.sh                    # 构建脚本
├── CMakeLists.txt              # CMake构建配置
├── include/                    # 头文件目录
│   ├── AppController.hpp       # 应用控制器
│   ├── MusicPlayer.hpp         # 音乐播放器
│   ├── Playlist.hpp            # 歌单管理
│   └── UIHelpers.hpp           # UI辅助函数
├── LICENSE                     # 许可证 (GPL v3)
├── README.md                   # 项目说明
├── USAGE.md                    # 使用说明
└── src/                        # 源代码目录
    ├── AppController.cpp       # 应用控制器实现
    ├── main.cpp                # 主程序入口
    ├── MusicPlayer.cpp         # 音乐播放器实现
    ├── Playlist.cpp            # 歌单管理实现
    └── UIHelpers.cpp           # UI辅助函数实现
```

------------------------------------------------------------------------

## 构建依赖

``` bash
SDL2 
SDL2_mixer 
TagLib 
NcursesW 
nlohmann_json 
CMake 
```          

------------------------------------------------------------------------

## 构建流程

``` bash
bash ./build.sh
```

构建完成后：
- 可执行文件：`build/smp`
- DEB安装包：`build/simple-music-player-*.deb`

------------------------------------------------------------------------

## 任务清单

### 已完成 (v1.1.0)

-   [ √ ] 设置界面
-   [ √ ] 分离界面渲染和音乐播放线程
-   [ √ ] 歌单系统 (替换目录播放)
-   [ √ ] 翻页菜单系统
-   [ √ ] 帮助系统 (H键)
-   [ √ ] 歌单排序功能
-   [ √ ] 乱序播放功能
-   [ √ ] 单曲循环功能
-   [ √ ] 快进快退功能
-   [ x ] 歌曲封面显示 //部分终端不支持，已放弃

------------------------------------------------------------------------

## 数据存储

程序数据存储在 `~/.config/simple_music_player/` 目录:

```
~/.config/simple_music_player/
├── config.json              # 主配置文件
└── song_lists/              # 歌单目录
    ├── playlist_0.json      # 歌单0
    ├── playlist_1.json      # 歌单1
    └── ...
```

### 配置文件格式
```json
{
  "play_mode": "sequential",  // 或 "shuffle"
  "current_playlist_index": 0,
  "current_song_index": 0,
  "playlists_meta": [
    {
      "index": 0,
      "name": "默认歌单",
      "created_time": 1741348800,
      "modified_time": 1741348800
    }
  ]
}
```

### 歌单文件格式
```json
{
  "name": "歌单名称",
  "created_time": 1741348800,
  "modified_time": 1741348800,
  "songs": [
    {
      "path": "/path/to/song.mp3",
      "title": "歌曲标题",
      "artist": "艺术家",
      "album": "专辑",
      "modified_time": 1741348800
    }
  ]
}
```

------------------------------------------------------------------------

## 许可证

本项目采用 **GNU General Public License v3.0** 许可证 - 查看 [LICENSE](LICENSE) 文件了解详情