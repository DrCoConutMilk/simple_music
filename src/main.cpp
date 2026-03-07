#include <ncurses.h>
#include <clocale>
#include <thread>
#include <chrono>
#include <vector>
#include <filesystem>
#include <algorithm>
#include "AppController.hpp"
#include "UIHelpers.hpp"

namespace fs = std::filesystem;

// --- 全局变量 ---
AppController ctrl;
PageMenu main_menu_page;
PageMenu playlist_manager_page;
PageMenu playlist_menu_page;    // 歌单功能菜单
PageMenu playlist_view_page;    // 歌曲浏览菜单
PageMenu playlist_edit_page;
PageMenu add_to_playlist_page;
PageMenu sort_menu_page;
AppState previous_state = AppState::PLAYING;
int current_selected_playlist_index = -1;  // 当前选中的歌单索引
int current_operating_song_index = -1;     // 当前操作的歌曲索引
std::string song_to_add_path = "";         // 要添加的歌曲路径（如果为空，则添加当前播放的歌曲）

// --- 辅助函数 ---
void enterHelp() {
    previous_state = ctrl.state;
    ctrl.state = AppState::HELP;
}

// --- 渲染函数 ---
void renderPlaying() {
    auto& player = ctrl.getPlayer();
    std::string mode_name = (ctrl.mode == PlayMode::SHUFFLE ? "乱序" : "顺序");
    
    // 显示当前歌单信息
    std::string playlist_name = "无歌单";
    if (ctrl.currentPlaylistIndex >= 0 && ctrl.currentPlaylistIndex < (int)ctrl.playlists.size()) {
        playlist_name = ctrl.playlists[ctrl.currentPlaylistIndex]->name;
    }
    
    mvprintw(1, 2, "歌单: %s | 模式: %s", playlist_name.c_str(), mode_name.c_str());

    if (ctrl.currentPlaylistIndex < 0 || ctrl.currentPlaylistIndex >= (int)ctrl.playlists.size() || 
        ctrl.playlists[ctrl.currentPlaylistIndex]->empty()) {
        mvprintw(LINES / 2, (COLS - 20) / 2, "--- 暂无歌曲 ---");
        mvprintw(LINES / 2 + 1, (COLS - 30) / 2, "请按 [M] 进入菜单选择歌单");
    } else {
        auto& song = player.getCurrentSong();
        double elapsed = player.getElapsedSeconds();

        mvprintw(3, 2, "当前播放：[%d/%d] %s - %s",
                 ctrl.currentSongIndex + 1, (int)ctrl.playlists[ctrl.currentPlaylistIndex]->size(),
                 song.title.c_str(), song.artist.c_str());

        // 歌词显示
        int lyricIdx = -1;
        for (int i = 0; i < (int)song.lyrics.size(); ++i) {
            if (elapsed >= song.lyrics[i].timestamp)
                lyricIdx = i;
            else
                break;
        }
        for (int offset = -1; offset <= 1; ++offset) {
            int idx = lyricIdx + offset;
            if (idx >= 0 && idx < (int)song.lyrics.size()) {
                if (offset == 0)
                    attron(COLOR_PAIR(1) | A_BOLD);
                mvprintw(8 + offset * 2, 4 + (offset == 0 ? 0 : 3), "%s%s", 
                        (offset == 0 ? ">> " : ""), song.lyrics[idx].text.c_str());
                if (offset == 0)
                    attroff(COLOR_PAIR(1) | A_BOLD);
            }
        }

        // 进度条
        int barWidth = std::max(10, COLS - 20);
        int pos = (song.duration > 0) ? (int)(elapsed / song.duration * barWidth) : 0;
        mvprintw(LINES - 2, 2, "%02d:%02d [", (int)elapsed / 60, (int)elapsed % 60);
        for (int i = 0; i < barWidth; ++i)
            addch(i < pos ? '=' : (i == pos ? '>' : ' '));
        printw("] %02d:%02d", (int)song.duration / 60, (int)song.duration % 60);
    }
    mvprintw(LINES - 4, 2, "[H]帮助");
}

void renderMainMenu() {
    std::vector<std::string> options = {
        "返回播放",
        "播放列表",
        "歌单管理器",
        "添加到歌单",
        "设置"
    };
    drawPageMenu("主菜单", options, main_menu_page, false);
}

void renderPlaylistManager() {
    std::vector<std::string> options;
    
    // 添加歌单列表
    for (const auto& playlist : ctrl.playlists) {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "%s (%zu 首)", 
                playlist->name.c_str(), playlist->size());
        options.push_back(buffer);
    }
    
    // 添加功能选项（如果有歌单，添加分隔线）
    if (!options.empty()) {
        options.push_back("--- 功能 ---");
    }
    options.push_back("创建歌单");
    options.push_back("返回主菜单");
    
    drawPageMenu("歌单管理器", options, playlist_manager_page, false);
}

// 简化版本 - 只确保编译通过
void renderPlaylistMenu() {
    if (current_selected_playlist_index < 0 || 
        current_selected_playlist_index >= (int)ctrl.playlists.size()) {
        return;
    }
    
    auto& playlist = ctrl.playlists[current_selected_playlist_index];
    std::vector<std::string> options = {
        "播放此歌单",
        "浏览歌曲",
        "重命名歌单",
        "排序歌单",
        "删除歌单",
        "返回歌单管理器"
    };
    
    char title[128];
    snprintf(title, sizeof(title), "歌单: %s (%zu 首)", playlist->name.c_str(), playlist->size());
    drawPageMenu(title, options, playlist_menu_page, false);
}

void renderPlaylistView() {
    if (current_selected_playlist_index < 0 || 
        current_selected_playlist_index >= (int)ctrl.playlists.size()) {
        return;
    }
    
    auto& playlist = ctrl.playlists[current_selected_playlist_index];
    std::vector<std::string> options;
    
    // 添加歌曲列表
    for (const auto& song : playlist->getSongs()) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "%s - %s", 
                song.title.c_str(), song.artist.c_str());
        options.push_back(buffer);
    }
    
    // 如果没有歌曲，显示提示
    if (options.empty()) {
        options.push_back("--- 暂无歌曲 ---");
    }
    
    char title[128];
    snprintf(title, sizeof(title), "歌单: %s - 歌曲列表", playlist->name.c_str());
    drawPageMenu(title, options, playlist_view_page, false);
}

void renderSongOperationMenu() {
    if (current_selected_playlist_index < 0 || 
        current_selected_playlist_index >= (int)ctrl.playlists.size() ||
        current_operating_song_index < 0) {
        return;
    }
    
    auto& playlist = ctrl.playlists[current_selected_playlist_index];
    if (current_operating_song_index >= (int)playlist->size()) {
        return;
    }
    
    const auto& song = playlist->getSongs()[current_operating_song_index];
    char song_info[256];
    snprintf(song_info, sizeof(song_info), "%s - %s", 
             song.title.c_str(), song.artist.c_str());
    
    std::vector<std::string> options = {
        "播放此歌曲",
        "从歌单删除",
        "添加到指定歌单",
        "返回歌曲列表"
    };
    
    char title[256];
    snprintf(title, sizeof(title), "歌曲操作: %s", song_info);
    drawPageMenu(title, options, main_menu_page, false); // 使用main_menu_page作为临时页面
}

void renderPlaylistEdit() {
    std::vector<std::string> options = {
        "从目录添加歌曲",
        "创建空歌单",
        "返回歌单管理器"
    };
    drawPageMenu("创建歌单", options, playlist_edit_page, false);
}

void renderAddToPlaylist() {
    std::vector<std::string> options;
    for (const auto& playlist : ctrl.playlists) {
        options.push_back(playlist->name);
    }
    options.push_back("--- 功能 ---");
    options.push_back("返回播放");
    drawPageMenu("添加到歌单", options, add_to_playlist_page, false);
}

void renderSortMenu() {
    std::vector<std::string> options = {
        "按歌名排序",
        "按歌手排序", 
        "按专辑排序",
        "按文件名排序",
        "按修改时间排序",
        "返回歌单浏览"
    };
    drawPageMenu("排序方式", options, sort_menu_page, false);
}

void renderSettings() {
    std::vector<std::string> options = {
        "播放模式",
        "返回主菜单"
    };
    drawPageMenu("设置", options, main_menu_page, false);
}

void renderPlayMode() {
    std::vector<std::string> options = {
        "顺序播放",
        "乱序播放",
        "返回设置"
    };
    drawPageMenu("播放模式", options, main_menu_page, false);
}

// --- 输入处理 ---
void handlePlayingInput(int ch) {
    if (ch == 'm' || ch == 'M') {
        ctrl.state = AppState::MAIN_MENU;
        main_menu_page.selected_index = 0;
    } else if (ch == ' ') {
        ctrl.togglePause();
    } else if (ch == KEY_RIGHT) {
        ctrl.nextSong();
    } else if (ch == KEY_LEFT) {
        ctrl.prevSong();
    } else if (ch == 'a' || ch == 'A') {
        if (!ctrl.getCurrentSongPath().empty()) {
            ctrl.state = AppState::ADD_TO_PLAYLIST;
            add_to_playlist_page.selected_index = 0;
            add_to_playlist_page.update(ctrl.playlists.size());
        }
    } else if (ch == 'h' || ch == 'H') {
        enterHelp();
    }
}

void handleMainMenuInput(int ch) {
    if (ch == KEY_UP) {
        main_menu_page.moveUp();
    } else if (ch == KEY_DOWN) {
        main_menu_page.moveDown();
    } else if (ch == KEY_PPAGE) {
        main_menu_page.prevPage();
    } else if (ch == KEY_NPAGE) {
        main_menu_page.nextPage();
    } else if (ch == '\n' || ch == 13) {
        switch (main_menu_page.selected_index) {
            case 0:
                ctrl.state = AppState::PLAYING;
                break;
            case 1:
                // 播放列表：浏览当前播放的歌单
                if (ctrl.currentPlaylistIndex >= 0 && ctrl.currentPlaylistIndex < (int)ctrl.playlists.size()) {
                    current_selected_playlist_index = ctrl.currentPlaylistIndex;
                    ctrl.state = AppState::PLAYLIST_VIEW;
                    auto& playlist = ctrl.playlists[current_selected_playlist_index];
                    playlist_view_page.update(playlist->size());
                    playlist_view_page.current_page = 0;
                    playlist_view_page.selected_index = ctrl.currentSongIndex; // 选中当前播放的歌曲
                }
                break;
            case 2:
                ctrl.state = AppState::PLAYLIST_MANAGER;
                playlist_manager_page.selected_index = 0;
                playlist_manager_page.update(ctrl.playlists.size());
                break;
            case 3:
                // 添加到歌单
                if (ctrl.currentPlaylistIndex >= 0 && ctrl.currentPlaylistIndex < (int)ctrl.playlists.size()) {
                    auto& current_playlist = ctrl.playlists[ctrl.currentPlaylistIndex];
                    if (ctrl.currentSongIndex >= 0 && ctrl.currentSongIndex < (int)current_playlist->size()) {
                        // 清空歌曲路径，表示添加当前播放的歌曲
                        song_to_add_path = "";
                        // 进入添加到歌单界面
                        ctrl.state = AppState::ADD_TO_PLAYLIST;
                        add_to_playlist_page.selected_index = 0;
                        add_to_playlist_page.update(ctrl.playlists.size());
                    }
                }
                break;
            case 4:
                ctrl.state = AppState::SETTINGS_MENU;
                main_menu_page.selected_index = 0;
                break;
        }
    } else if (ch == 'h' || ch == 'H') {
        enterHelp();
    } else if (ch == 'q' || ch == 'Q') {
        // Q键返回播放界面
        ctrl.state = AppState::PLAYING;
    }
}

void handlePlaylistManagerInput(int ch) {
    if (ch == KEY_UP) {
        playlist_manager_page.moveUp();
        // 如果选中了分隔线，继续向上移动
        int selected = playlist_manager_page.selected_index;
        int playlist_count = ctrl.playlists.size();
        if (!ctrl.playlists.empty() && selected == playlist_count) {
            // 选中了分隔线，向上移动一位
            playlist_manager_page.selected_index = playlist_count - 1;
        }
    } else if (ch == KEY_DOWN) {
        playlist_manager_page.moveDown();
        // 如果选中了分隔线，继续向下移动
        int selected = playlist_manager_page.selected_index;
        int playlist_count = ctrl.playlists.size();
        if (!ctrl.playlists.empty() && selected == playlist_count) {
            // 选中了分隔线，向下移动一位
            playlist_manager_page.selected_index = playlist_count + 1;
        }
    } else if (ch == KEY_PPAGE) {
        playlist_manager_page.prevPage();
    } else if (ch == KEY_NPAGE) {
        playlist_manager_page.nextPage();
    } else if (ch == '\n' || ch == 13) {
        int selected = playlist_manager_page.selected_index;
        int playlist_count = ctrl.playlists.size();
        
        // 计算功能选项的偏移（考虑分隔线）
        int separator_offset = (!ctrl.playlists.empty()) ? 1 : 0;
        
        if (selected < playlist_count) {
            // 选择歌单 - 进入歌单功能菜单
            ctrl.state = AppState::PLAYLIST_MENU;
            current_selected_playlist_index = selected;
            // 注意：这里不更新 ctrl.currentPlaylistIndex
            // currentPlaylistIndex 只在用户选择"播放此歌单"时才更新
            // 重置页面菜单状态，从第一个选项开始
            playlist_menu_page.current_page = 0;
            playlist_menu_page.selected_index = 0;
        } else if (selected == playlist_count + separator_offset) {
            // "创建歌单"选项（跳过分隔线）
            ctrl.state = AppState::PLAYLIST_EDIT;
            playlist_edit_page.selected_index = 0;
        } else if (selected == playlist_count + separator_offset + 1) {
            // "返回主菜单"选项
            ctrl.state = AppState::MAIN_MENU;
        }
    } else if (ch == 'h' || ch == 'H') {
        enterHelp();
    } else if (ch == 'q' || ch == 'Q') {
        ctrl.state = AppState::MAIN_MENU;
    }
}

void handlePlaylistMenuInput(int ch) {
    if (ch == KEY_UP) {
        playlist_menu_page.moveUp();
    } else if (ch == KEY_DOWN) {
        playlist_menu_page.moveDown();
    } else if (ch == KEY_PPAGE) {
        playlist_menu_page.prevPage();
    } else if (ch == KEY_NPAGE) {
        playlist_menu_page.nextPage();
    } else if (ch == '\n' || ch == 13) {
        switch (playlist_menu_page.selected_index) {
            case 0:
                // 播放此歌单
                if (current_selected_playlist_index >= 0 && 
                    current_selected_playlist_index < (int)ctrl.playlists.size()) {
                    auto& playlist = ctrl.playlists[current_selected_playlist_index];
                    if (!playlist->empty()) {
                        ctrl.currentPlaylistIndex = current_selected_playlist_index;
                        ctrl.currentSongIndex = 0;
                        ctrl.state = AppState::PLAYING;
                    }
                }
                break;
            case 1:
                // 浏览歌曲
                ctrl.state = AppState::PLAYLIST_VIEW;
                if (current_selected_playlist_index >= 0 && 
                    current_selected_playlist_index < (int)ctrl.playlists.size()) {
                    auto& playlist = ctrl.playlists[current_selected_playlist_index];
                    playlist_view_page.update(playlist->size());
                    playlist_view_page.current_page = 0;
                    playlist_view_page.selected_index = 0;
                }
                break;
            case 2:
                // 重命名歌单
                if (current_selected_playlist_index >= 0 && 
                    current_selected_playlist_index < (int)ctrl.playlists.size()) {
                    std::string new_name = inputField("输入新名称: ");
                    if (!new_name.empty()) {
                        ctrl.renamePlaylist(current_selected_playlist_index, new_name);
                    }
                }
                break;
            case 3:
                // 排序歌单
                ctrl.state = AppState::PLAYLIST_SORT;
                sort_menu_page.selected_index = 0;
                break;
            case 4:
                // 删除歌单
                if (current_selected_playlist_index >= 0 && 
                    current_selected_playlist_index < (int)ctrl.playlists.size()) {
                    ctrl.deletePlaylist(current_selected_playlist_index);
                    ctrl.state = AppState::PLAYLIST_MANAGER;
                    playlist_manager_page.update(ctrl.playlists.size());
                    current_selected_playlist_index = -1;
                }
                break;
            case 5:
                // 返回歌单管理器
                ctrl.state = AppState::PLAYLIST_MANAGER;
                current_selected_playlist_index = -1;
                break;
        }
    } else if (ch == 'h' || ch == 'H') {
        enterHelp();
    } else if (ch == 'q' || ch == 'Q') {
        ctrl.state = AppState::PLAYLIST_MANAGER;
        current_selected_playlist_index = -1;
    }
}

void handlePlaylistViewInput(int ch) {
    // 首先检查当前选择的歌单索引是否有效
    if (current_selected_playlist_index < 0 || 
        current_selected_playlist_index >= (int)ctrl.playlists.size()) {
        // 无效索引，返回歌单管理器
        if (ch == 'q' || ch == 'Q') {
            ctrl.state = AppState::PLAYLIST_MANAGER;
        }
        return;
    }
    
    auto& playlist = ctrl.playlists[current_selected_playlist_index];
    int song_count = playlist->size();
    
    if (ch == KEY_UP) {
        playlist_view_page.moveUp();
    } else if (ch == KEY_DOWN) {
        playlist_view_page.moveDown();
    } else if (ch == KEY_PPAGE) {
        playlist_view_page.prevPage();
    } else if (ch == KEY_NPAGE) {
        playlist_view_page.nextPage();
    } else if (ch == '\n' || ch == 13) {
        int selected = playlist_view_page.selected_index;
        
        if (selected < song_count) {
            // 记录选中的歌曲索引，然后打开歌曲操作菜单
            current_operating_song_index = selected;
            ctrl.state = AppState::SONG_OPERATION_MENU;
        }
    } else if (ch == 'h' || ch == 'H') {
        enterHelp();
    } else if (ch == 'q' || ch == 'Q') {
        // 返回歌单功能菜单
        ctrl.state = AppState::PLAYLIST_MENU;
    }
}

void handleSongOperationMenuInput(int ch) {
    if (ch == KEY_UP) {
        main_menu_page.moveUp();
    } else if (ch == KEY_DOWN) {
        main_menu_page.moveDown();
    } else if (ch == KEY_PPAGE) {
        main_menu_page.prevPage();
    } else if (ch == KEY_NPAGE) {
        main_menu_page.nextPage();
    } else if (ch == '\n' || ch == 13) {
        switch (main_menu_page.selected_index) {
            case 0:
                // 播放此歌曲
                if (current_selected_playlist_index >= 0 && 
                    current_selected_playlist_index < (int)ctrl.playlists.size() &&
                    current_operating_song_index >= 0) {
                    auto& playlist = ctrl.playlists[current_selected_playlist_index];
                    if (current_operating_song_index < (int)playlist->size()) {
                        ctrl.currentPlaylistIndex = current_selected_playlist_index;
                        ctrl.currentSongIndex = current_operating_song_index;
                        ctrl.requestLoad();
                        ctrl.state = AppState::PLAYING;
                    }
                }
                break;
            case 1:
                // 从歌单删除
                if (current_selected_playlist_index >= 0 && 
                    current_selected_playlist_index < (int)ctrl.playlists.size() &&
                    current_operating_song_index >= 0) {
                    ctrl.removeSongFromPlaylist(current_selected_playlist_index, current_operating_song_index);
                    // 返回歌曲列表
                    ctrl.state = AppState::PLAYLIST_VIEW;
                    // 更新歌曲列表显示
                    auto& playlist = ctrl.playlists[current_selected_playlist_index];
                    playlist_view_page.update(playlist->size());
                    // 重置选中索引
                    if (playlist_view_page.selected_index >= (int)playlist->size()) {
                        playlist_view_page.selected_index = std::max(0, (int)playlist->size() - 1);
                    }
                }
                break;
            case 2:
                // 添加到指定歌单
                if (current_selected_playlist_index >= 0 && 
                    current_selected_playlist_index < (int)ctrl.playlists.size() &&
                    current_operating_song_index >= 0) {
                    auto& playlist = ctrl.playlists[current_selected_playlist_index];
                    if (current_operating_song_index < (int)playlist->size()) {
                        // 获取要添加的歌曲路径
                        const auto& song = playlist->getSongs()[current_operating_song_index];
                        song_to_add_path = song.path;
                        // 进入添加到歌单界面
                        ctrl.state = AppState::ADD_TO_PLAYLIST;
                        add_to_playlist_page.selected_index = 0;
                        add_to_playlist_page.update(ctrl.playlists.size());
                    }
                }
                break;
            case 3:
                // 返回歌曲列表
                ctrl.state = AppState::PLAYLIST_VIEW;
                break;
        }
    } else if (ch == 'h' || ch == 'H') {
        enterHelp();
    } else if (ch == 'q' || ch == 'Q') {
        // 返回歌曲列表
        ctrl.state = AppState::PLAYLIST_VIEW;
    }
}

void handlePlaylistEditInput(int ch) {
    if (ch == KEY_UP) {
        playlist_edit_page.moveUp();
    } else if (ch == KEY_DOWN) {
        playlist_edit_page.moveDown();
    } else if (ch == KEY_PPAGE) {
        playlist_edit_page.prevPage();
    } else if (ch == KEY_NPAGE) {
        playlist_edit_page.nextPage();
    } else if (ch == '\n' || ch == 13) {
        switch (playlist_edit_page.selected_index) {
            case 0: {
                // 从目录添加歌曲
                std::string name = inputField("输入歌单名称: ");
                if (!name.empty()) {
                    ctrl.createPlaylist(name);
                    int new_index = ctrl.playlists.size() - 1;
                    std::string dir = inputField("输入目录路径: ");
                    if (!dir.empty()) {
                        ctrl.addSongsFromDirectory(new_index, dir);
                    }
                    ctrl.state = AppState::PLAYLIST_MANAGER;
                    playlist_manager_page.selected_index = new_index;
                    playlist_manager_page.update(ctrl.playlists.size());
                }
                break;
            }
            case 1: {
                // 创建空歌单
                std::string name = inputField("输入歌单名称: ");
                if (!name.empty()) {
                    ctrl.createPlaylist(name);
                    ctrl.state = AppState::PLAYLIST_MANAGER;
                    playlist_manager_page.update(ctrl.playlists.size());
                }
                break;
            }
            case 2:
                // 返回歌单管理器
                ctrl.state = AppState::PLAYLIST_MANAGER;
                break;
        }
    } else if (ch == 'h' || ch == 'H') {
        enterHelp();
    } else if (ch == 'q' || ch == 'Q') {
        ctrl.state = AppState::PLAYLIST_MANAGER;
    }
}

void handleAddToPlaylistInput(int ch) {
    if (ch == KEY_UP) {
        add_to_playlist_page.moveUp();
        // 如果选中了分隔线，继续向上移动
        int selected = add_to_playlist_page.selected_index;
        int playlist_count = ctrl.playlists.size();
        if (selected == playlist_count) {
            // 选中了分隔线，向上移动一位
            add_to_playlist_page.selected_index = playlist_count - 1;
        }
    } else if (ch == KEY_DOWN) {
        add_to_playlist_page.moveDown();
        // 如果选中了分隔线，继续向下移动
        int selected = add_to_playlist_page.selected_index;
        int playlist_count = ctrl.playlists.size();
        if (selected == playlist_count) {
            // 选中了分隔线，向下移动一位
            add_to_playlist_page.selected_index = playlist_count + 1;
        }
    } else if (ch == KEY_PPAGE) {
        add_to_playlist_page.prevPage();
    } else if (ch == KEY_NPAGE) {
        add_to_playlist_page.nextPage();
    } else if (ch == '\n' || ch == 13) {
        int selected = add_to_playlist_page.selected_index;
        int playlist_count = ctrl.playlists.size();
        
        if (selected < playlist_count) {
            // 选择歌单 - 添加歌曲
            if (!song_to_add_path.empty()) {
                // 添加指定的歌曲
                ctrl.addSongToPlaylist(selected, song_to_add_path);
            } else {
                // 添加当前播放的歌曲
                ctrl.addCurrentSongToPlaylist(selected);
            }
            // 清空歌曲路径
            song_to_add_path = "";
            // 返回来源界面
            if (ctrl.state == AppState::SONG_OPERATION_MENU) {
                ctrl.state = AppState::PLAYLIST_VIEW;
            } else {
                ctrl.state = AppState::PLAYING;
            }
        } else if (selected == playlist_count + 1) {
            // "返回播放"选项（跳过分隔线）
            ctrl.state = AppState::PLAYING;
        }
    } else if (ch == 'h' || ch == 'H') {
        enterHelp();
    } else if (ch == 'q' || ch == 'Q') {
        ctrl.state = AppState::PLAYING;
    }
}

void handleSortMenuInput(int ch) {
    if (ch == KEY_UP) {
        sort_menu_page.moveUp();
    } else if (ch == KEY_DOWN) {
        sort_menu_page.moveDown();
    } else if (ch == KEY_PPAGE) {
        sort_menu_page.prevPage();
    } else if (ch == KEY_NPAGE) {
        sort_menu_page.nextPage();
    } else if (ch == '\n' || ch == 13) {
        int selected = sort_menu_page.selected_index;
        
        if (selected < 5) {
            // 选择排序方式
            SortBy by;
            switch (selected) {
                case 0: by = SortBy::TITLE; break;
                case 1: by = SortBy::ARTIST; break;
                case 2: by = SortBy::ALBUM; break;
                case 3: by = SortBy::FILENAME; break;
                case 4: by = SortBy::MODIFIED_TIME; break;
                default: by = SortBy::TITLE;
            }
            
            std::string order_str = inputField("排序顺序 (A:升序, D:降序): ");
            SortOrder order = SortOrder::ASCENDING;
            if (!order_str.empty() && (order_str[0] == 'd' || order_str[0] == 'D')) {
                order = SortOrder::DESCENDING;
            }
            
            ctrl.sortPlaylist(current_selected_playlist_index, by, order);
            ctrl.state = AppState::PLAYLIST_MENU;
        } else if (selected == 5) {
            // "返回歌单功能菜单"
            ctrl.state = AppState::PLAYLIST_MENU;
        }
    } else if (ch == 'h' || ch == 'H') {
        enterHelp();
    } else if (ch == 'q' || ch == 'Q') {
        ctrl.state = AppState::PLAYLIST_VIEW;
    }
}

void handleSettingsInput(int ch) {
    if (ch == KEY_UP) {
        main_menu_page.moveUp();
    } else if (ch == KEY_DOWN) {
        main_menu_page.moveDown();
    } else if (ch == KEY_PPAGE) {
        main_menu_page.prevPage();
    } else if (ch == KEY_NPAGE) {
        main_menu_page.nextPage();
    } else if (ch == '\n' || ch == 13) {
        if (main_menu_page.selected_index == 0) {
            ctrl.state = AppState::SET_MODE;
            main_menu_page.selected_index = (ctrl.mode == PlayMode::SEQUENTIAL ? 0 : 1);
        } else if (main_menu_page.selected_index == 1) {
            ctrl.state = AppState::MAIN_MENU;
        }
    } else if (ch == 'h' || ch == 'H') {
        enterHelp();
    } else if (ch == 'q' || ch == 'Q') {
        ctrl.state = AppState::MAIN_MENU;
    }
}

void handlePlayModeInput(int ch) {
    if (ch == KEY_UP || ch == KEY_DOWN) {
        main_menu_page.selected_index = 1 - main_menu_page.selected_index;
    } else if (ch == '\n' || ch == 13) {
        if (main_menu_page.selected_index == 0) {
            ctrl.togglePlayMode();
        }
        ctrl.state = AppState::SETTINGS_MENU;
        main_menu_page.selected_index = 0;
    } else if (ch == 'h' || ch == 'H') {
        enterHelp();
    } else if (ch == 'q' || ch == 'Q') {
        ctrl.state = AppState::SETTINGS_MENU;
    }
}

// --- 主函数 ---
int main() {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);
    start_color();
    use_default_colors();
    init_pair(1, COLOR_CYAN, -1);

    // 初始化页面菜单
    main_menu_page.items_per_page = 10;
    playlist_manager_page.items_per_page = 15;
    playlist_menu_page.items_per_page = 10;    // 歌单功能菜单
    playlist_view_page.items_per_page = 15;    // 歌曲浏览
    playlist_edit_page.items_per_page = 10;
    add_to_playlist_page.items_per_page = 15;
    sort_menu_page.items_per_page = 10;

    while (ctrl.isRunning()) {
        // 处理输入
        int ch = getch();
        if (ch != ERR) {
            // 特殊处理播放界面的 Q 键
            if (ch == 'q' || ch == 'Q') {
                if (ctrl.state == AppState::PLAYING) {
                    ctrl.stop();
                    break;
                }
            }
            
            // 其他按键处理
            switch (ctrl.state) {
                case AppState::PLAYING:
                    handlePlayingInput(ch);
                    break;
                case AppState::MAIN_MENU:
                    handleMainMenuInput(ch);
                    break;
                case AppState::PLAYLIST_MANAGER:
                    handlePlaylistManagerInput(ch);
                    break;
                case AppState::PLAYLIST_MENU:
                    handlePlaylistMenuInput(ch);
                    break;
                case AppState::PLAYLIST_VIEW:
                    handlePlaylistViewInput(ch);
                    break;
                case AppState::PLAYLIST_EDIT:
                    handlePlaylistEditInput(ch);
                    break;
                case AppState::ADD_TO_PLAYLIST:
                    handleAddToPlaylistInput(ch);
                    break;
                case AppState::PLAYLIST_SORT:
                    handleSortMenuInput(ch);
                    break;
                case AppState::SONG_OPERATION_MENU:
                    handleSongOperationMenuInput(ch);
                    break;
                case AppState::SETTINGS_MENU:
                    handleSettingsInput(ch);
                    break;
                case AppState::SET_MODE:
                    handlePlayModeInput(ch);
                    break;
                case AppState::HELP:
                    ctrl.state = previous_state;
                    break;
                default:
                    break;
            }
        }

        // 渲染界面
        erase();
        {
            std::lock_guard<std::mutex> lock(ctrl.getMutex());
            switch (ctrl.state) {
                case AppState::PLAYING:
                    renderPlaying();
                    break;
                case AppState::MAIN_MENU:
                    renderMainMenu();
                    break;
                case AppState::PLAYLIST_MANAGER:
                    renderPlaylistManager();
                    break;
                case AppState::PLAYLIST_MENU:
                    renderPlaylistMenu();
                    break;
                case AppState::PLAYLIST_VIEW:
                    renderPlaylistView();
                    break;
                case AppState::PLAYLIST_EDIT:
                    renderPlaylistEdit();
                    break;
                case AppState::ADD_TO_PLAYLIST:
                    renderAddToPlaylist();
                    break;
                case AppState::PLAYLIST_SORT:
                    renderSortMenu();
                    break;
                case AppState::SONG_OPERATION_MENU:
                    renderSongOperationMenu();
                    break;
                case AppState::SETTINGS_MENU:
                    renderSettings();
                    break;
                case AppState::SET_MODE:
                    renderPlayMode();
                    break;
                case AppState::HELP:
                    switch (previous_state) {
                        case AppState::PLAYING:
                            drawHelp(PLAYING_HELP);
                            break;
                        case AppState::MAIN_MENU:
                            drawHelp(MAIN_MENU_HELP);
                            break;
                        case AppState::PLAYLIST_MANAGER:
                            drawHelp(PLAYLIST_MANAGER_HELP);
                            break;
                        case AppState::PLAYLIST_MENU:
                            drawHelp(PLAYLIST_MENU_HELP);
                            break;
                        case AppState::PLAYLIST_VIEW:
                            drawHelp(PLAYLIST_VIEW_HELP);
                            break;
                        case AppState::PLAYLIST_EDIT:
                            drawHelp(PLAYLIST_EDIT_HELP);
                            break;
                        case AppState::SONG_OPERATION_MENU:
                            drawHelp(SONG_OPERATION_MENU_HELP);
                            break;
                        case AppState::ADD_TO_PLAYLIST:
                            drawHelp(ADD_TO_PLAYLIST_HELP);
                            break;
                        case AppState::PLAYLIST_SORT:
                            drawHelp(SORT_MENU_HELP);
                            break;
                        case AppState::SETTINGS_MENU:
                            drawHelp(SETTINGS_HELP);
                            break;
                        case AppState::SET_MODE:
                            drawHelp(PLAY_MODE_HELP);
                            break;
                        default:
                            drawHelp(MAIN_MENU_HELP);
                            break;
                    }
                    break;
                default:
                    break;
            }
        }
        refresh();
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    endwin();
    return 0;
}