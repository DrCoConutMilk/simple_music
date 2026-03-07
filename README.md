# Simple Music Player

一个基于 C++ 开发的轻量级终端音乐播放器, 支持歌词解析与显示

<img width="400" height="350" alt="image" src="https://github.com/user-attachments/assets/71d2e8d5-8448-466b-8e2b-3475ed26dfee" />


本项目大部分内容由AI辅助生成

## 项目简介

在linux上，我一直找不到一个能很好的解析歌曲元数据（包括歌词）的音乐播放器，因此产生了自己写一个的想法。

------------------------------------------------------------------------

## 目录结构

``` text
simple_music/
├── build.sh
├── CMakeLists.txt
├── include
│   ├── AppController.hpp
│   └── MusicPlayer.hpp
├── LICENSE
├── music
├── README.md
└── src
    ├── AppController.cpp
    ├── main.cpp
    └── MusicPlayer.cpp
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

## 任务清单

### 已完成 (v0.2.0)

-   [ √ ] 设置界面
-   [ √ ] 分离界面渲染和音乐播放线程
-   [ √ ] 歌单系统 (替换目录播放)
-   [ √ ] 翻页菜单系统
-   [ √ ] 帮助系统 (H键)
-   [ √ ] 歌单排序功能
-   [ x ] 歌曲封面显示 //部分终端不支持，已放弃

### 歌单功能详情

1. **歌单管理**:
   - 创建歌单 (从目录添加或创建空歌单)
   - 查看、编辑、浏览播放和删除歌单
   - 修改歌单名称
   - 在浏览歌单时删除歌曲

2. **歌曲管理**:
   - 播放时将当前歌曲添加到歌单 (自动查重)
   - 支持 MP3 和 FLAC 格式
   - 自动解析歌曲元数据 (标题、艺术家、专辑)

3. **排序功能**:
   - 按歌名、歌手、专辑、文件名或最后修改时间排序
   - 支持正序和倒序排序

4. **界面改进**:
   - 翻页菜单 (PgUp/PgDn 翻页，↑↓ 在当前页移动)
   - 帮助系统 (按 H 键显示当前界面可用操作)
   - 统一的按键提示

### 数据存储

- 配置文件: `~/.config/simple_music_player/config.json`
- 歌单文件: `~/.config/simple_music_player/song_lists/playlist_*.json`
- 每个歌单独立保存，便于备份和管理

