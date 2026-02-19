cmake_minimum_required(VERSION 3.10)
project(TerminalPlayer)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(TAGLIB REQUIRED taglib)
find_package(nlohmann_json REQUIRED)

# 查找 ncurses，优先寻找带 'w' 的版本
find_package(Curses REQUIRED)
set(CURSES_NEED_WIDE TRUE) 

find_path(SDL2_MIXER_INCLUDE_DIR SDL_mixer.h PATH_SUFFIXES SDL2)
find_library(SDL2_MIXER_LIBRARY NAMES SDL2_mixer)

add_executable(simple_music main.cpp)

target_include_directories(simple_music PRIVATE 
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIR}
    ${TAGLIB_INCLUDE_DIRS}
    ${TAGLIB_INCLUDE_DIRS}/..
    ${CURSES_INCLUDE_DIRS}
)

# 显式链接 ncursesw (Linux 下处理中文通常需要这个 w)
target_link_libraries(simple_music 
    ${SDL2_LIBRARIES}
    ${SDL2_MIXER_LIBRARY}
    ${TAGLIB_LIBRARIES}
    nlohmann_json::nlohmann_json
    ncursesw   # <--- 直接指定 ncursesw 往往最有效
    stdc++fs 
)
