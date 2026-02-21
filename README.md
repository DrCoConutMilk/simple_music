# Simple Music Player

一个基于 C++ 开发的轻量级终端音乐播放器, 支持歌词解析与显示

<img width="396" height="302" alt="image" src="https://github.com/user-attachments/assets/1ba8b28a-e22f-4a56-baa3-e2d8924115d9" />

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


### 计划中

-   [ √ ] 设置界面
-   [ √ ] 应用内添加播放目录
-   [   ] 播放列表排序功能
-   [   ] 歌曲封面显示
-   [   ] 分离界面渲染和音乐播放线程
