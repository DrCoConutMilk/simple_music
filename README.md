# Simple Music Player

一个基于 C++ 开发的轻量级终端音乐播放器

本项目大部分内容由AI辅助生成

## 项目简介

在linux上，我一直找不到一个能很好的解析歌曲元数据（包括歌词）的音乐播放器，因此产生了自己写一个的想法。

------------------------------------------------------------------------

## 目录结构

``` text
simple_music/
├── main.cpp            # 核心逻辑：音频控制、歌词解析与 Ncurses 渲染
├── CMakeLists.txt      # 跨平台构建脚本
├── config.json         # 运行时配置：指定歌曲目录与模式
├── LICENSE             # 项目授权协议
├── README.md           # 本文档
└── build/              # 编译产物目录
    └── simple_music    # 编译生成的二进制执行文件
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
    "music_directory": "/home/user/Music",
    "play_mode": "sequential"
}
```


## 任务清单


### 计划中

-   [ ] 设置界面
-   [ ] 应用内添加播放目录
-   [ ] 播放列表排序功能