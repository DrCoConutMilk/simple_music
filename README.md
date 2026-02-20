# Simple Music Player

一个基于 C++ 开发的轻量级终端音乐播放器, 支持歌词解析与显示

<img width="803" height="773" alt="image" src="https://github.com/user-attachments/assets/0f839001-83f7-4501-828b-5c672f736a8c" />



本项目大部分内容由AI辅助生成

## 项目简介

在linux上，我一直找不到一个能很好的解析歌曲元数据（包括歌词）的音乐播放器，因此产生了自己写一个的想法。

------------------------------------------------------------------------

## 目录结构

``` text
simple_music/
├── build 
├── CMakeLists.txt
├── include
│   └── MusicPlayer.hpp
├── LICENSE
├── README.md
└── src
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

### 创建并进入构建目录

``` bash
mkdir build && cd build
```

### 配置 CMake

``` bash
cmake ..
```

### 编译

``` bash
make
```

### 配置运行参数

编辑 `build/config.json`：

``` json
{
    "music_directory": "/home/user/音乐",
    "play_mode": "shuffle"
}
```


## 任务清单


### 计划中

-   [ √ ] 设置界面
-   [ √ ] 应用内添加播放目录
-   [   ] 播放列表排序功能
-   [   ] 歌曲封面显示
