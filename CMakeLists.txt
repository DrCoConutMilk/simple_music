# 1. 基础设置
cmake_minimum_required(VERSION 3.10)
project(smp) # 项目名也改为 smp

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2. 查找依赖
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(TAGLIB REQUIRED taglib)
find_package(nlohmann_json REQUIRED)
find_package(Threads REQUIRED)

set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)

find_path(SDL2_MIXER_INCLUDE_DIR SDL_mixer.h PATH_SUFFIXES SDL2)
find_library(SDL2_MIXER_LIBRARY NAMES SDL2_mixer)

# 3. 收集源文件
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.hpp" "include/*.h")

# 4. 创建可执行文件 - 启动命令现在是 smp
# ---------------------------------------------------------
add_executable(smp ${SOURCES} ${HEADERS})
# ---------------------------------------------------------

# 5. 配置包含目录和链接 (Target 全部替换为 smp)
target_include_directories(smp PRIVATE 
    include
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIR}
    ${TAGLIB_INCLUDE_DIRS}
    ${CURSES_INCLUDE_DIRS}
)

target_link_libraries(smp 
    PRIVATE
    ${SDL2_LIBRARIES}
    ${SDL2_MIXER_LIBRARY}
    ${TAGLIB_LIBRARIES}
    nlohmann_json::nlohmann_json
    ${CURSES_LIBRARIES}
    Threads::Threads
    stdc++fs
)

# 6. 安装与打包配置
# ---------------------------------------------------------
# 这行决定了安装后二进制文件的位置：/usr/bin/smp
install(TARGETS smp DESTINATION bin)

set(CPACK_PACKAGE_NAME "simple-music-player")
set(CPACK_PACKAGE_VERSION "0.1.3")
set(CPACK_PACKAGE_CONTACT "drcoconutmilk@gmail.com")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "DrCoConutMilk")
set(CPACK_GENERATOR "DEB")

# 自动分析 .so 依赖（如 SDL2, TagLib 等）
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON) 

include(CPack)