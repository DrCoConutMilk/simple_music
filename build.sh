#!/bin/bash

# Simple Music Player 构建脚本
# 版本：v1.1.2

set -e  # 遇到错误时退出

echo "=== Simple Music Player 构建脚本 ==="
echo "版本: v1.1.2"
echo ""

# 检查是否已经存在 build 目录
if [ -d "build" ]; then
    echo "检测到已存在的 build 目录"
    echo "建议删除旧的构建目录以避免冲突"
    echo ""
    
    read -p "是否要移除 build 目录并重新构建？(y/N): " answer
    
    # 转换为小写
    answer=$(echo "$answer" | tr '[:upper:]' '[:lower:]')
    
    if [ "$answer" = "y" ] || [ "$answer" = "yes" ]; then
        echo "正在移除 build 目录..."
        rm -rf build
        echo "build 目录已移除"
    else
        echo "构建已取消"
        exit 0
    fi
fi

echo ""
echo "开始构建 Simple Music Player v1.1.2..."
echo ""

# 创建构建目录
echo "创建 build 目录..."
mkdir -p build
cd build

# 运行 CMake
echo "运行 CMake 配置..."
cmake ../

# 编译
echo "编译项目..."
make -j $(nproc)

# 打包
echo "生成安装包..."
cpack

echo ""
echo "=== 构建完成 ==="
echo ""
echo "生成的文件："
echo "1. 可执行文件: build/smp"
echo "2. DEB安装包: build/simple-music-player-1.1.2-Linux.deb"
echo ""
echo "使用方法："
echo "1. 运行程序: ./build/smp"
echo "2. 安装DEB包: sudo dpkg -i build/simple-music-player-1.1.2-Linux.deb"
echo ""