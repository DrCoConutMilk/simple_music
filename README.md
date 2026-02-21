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


### 计划中

-   [ √ ] 设置界面
-   [ √ ] 应用内添加播放目录
-   [ √ ] 分离界面渲染和音乐播放线程
-   [ x ] 歌曲封面显示 //部分终端不支持，已放弃
-   [   ] 播放列表排序功能
-   [   ] 自定义歌单

