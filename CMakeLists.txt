cmake_minimum_required(VERSION 3.10)
project(SimpleMusicPlayer)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. 查找依赖包
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(TAGLIB REQUIRED taglib)
find_package(nlohmann_json REQUIRED)

# 显式查找线程库 (修复报错的关键)
find_package(Threads REQUIRED)

# 查找 ncurses (带宽字符支持)
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)

# 查找 SDL2_mixer
find_path(SDL2_MIXER_INCLUDE_DIR SDL_mixer.h PATH_SUFFIXES SDL2)
find_library(SDL2_MIXER_LIBRARY NAMES SDL2_mixer)

# 2. 自动搜集源文件和头文件
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.hpp" "include/*.h")

# 3. 创建可执行文件
add_executable(simple_music ${SOURCES} ${HEADERS})

# 4. 配置包含目录
target_include_directories(simple_music PRIVATE 
    include
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIR}
    ${TAGLIB_INCLUDE_DIRS}
    ${CURSES_INCLUDE_DIRS}
)

# 5. 链接库
target_link_libraries(simple_music 
    PRIVATE
    ${SDL2_LIBRARIES}
    ${SDL2_MIXER_LIBRARY}
    ${TAGLIB_LIBRARIES}
    nlohmann_json::nlohmann_json
    ${CURSES_LIBRARIES}  # 使用 Curses 自动找到的库变量
    Threads::Threads     # 现在 find_package 已调用，此目标有效
    stdc++fs             # 兼容旧版 filesystem
)