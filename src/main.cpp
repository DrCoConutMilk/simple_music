#include <ncurses.h>
#include <clocale>
#include <thread>
#include <chrono>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cwchar>
#include <climits>
#include "AppController.hpp"
#include "UIHelpers.hpp"
#include "KittyCover.hpp"

namespace fs = std::filesystem;

// UTF-8 与终端显示列宽辅助函数
std::wstring toWide(const std::string& text) {
    std::mbstate_t state{};
    const char* source = text.c_str();
    const size_t length = std::mbsrtowcs(nullptr, &source, 0, &state);
    if (length == static_cast<size_t>(-1)) return L"";

    std::vector<wchar_t> buffer(length + 1);
    state = std::mbstate_t{};
    source = text.c_str();
    std::mbsrtowcs(buffer.data(), &source, buffer.size(), &state);
    return std::wstring(buffer.data(), length);
}

std::string fromWide(const std::wstring& text) {
    std::string result;
    std::mbstate_t state{};
    char buffer[MB_LEN_MAX];
    for (wchar_t c : text) {
        const size_t bytes = std::wcrtomb(buffer, c, &state);
        if (bytes == static_cast<size_t>(-1)) {
            state = std::mbstate_t{};
            continue;
        }
        result.append(buffer, bytes);
    }
    return result;
}

int terminalWidth(wchar_t c) {
    const int width = ::wcwidth(c);
    return width < 0 ? 1 : width;
}

int displayWidth(const std::wstring& text) {
    int width = 0;
    for (wchar_t c : text) width += terminalWidth(c);
    return width;
}

std::wstring takeColumns(const std::wstring& text, size_t start, int max_columns) {
    std::wstring result;
    int columns = 0;
    for (size_t i = start; i < text.size(); ++i) {
        const int width = terminalWidth(text[i]);
        if (columns + width > max_columns) break;
        result += text[i];
        columns += width;
    }
    return result;
}

std::string fitDisplayText(const std::string& text, int max_columns) {
    if (max_columns <= 0) return "";
    const std::wstring wide = toWide(text);
    if (wide.empty() && !text.empty())
        return text.substr(0, std::min(text.size(), static_cast<size_t>(max_columns)));
    if (displayWidth(wide) <= max_columns) return text;

    const std::wstring suffix = L"...";
    return fromWide(takeColumns(wide, 0, std::max(0, max_columns - 3)) + suffix);
}

std::string marqueeText(const std::string& text, int max_columns, long long tick, size_t slot) {
    const std::wstring wide = toWide(text);
    if (displayWidth(wide) <= max_columns) return text;

    struct MarqueeState {
        std::string text;
        long long changed_at = 0;
    };
    static std::vector<MarqueeState> states(2);
    if (slot >= states.size()) states.resize(slot + 1);
    auto& state = states[slot];
    if (text != state.text) {
        state.text = text;
        state.changed_at = tick;
    }

    constexpr long long pause_ticks = 6; // 6 × 250ms
    const long long elapsed_ticks = std::max(0LL, tick - state.changed_at);
    const size_t offset = elapsed_ticks <= pause_ticks
        ? 0
        : static_cast<size_t>(elapsed_ticks - pause_ticks);
    const std::wstring loop = wide + L"     " + wide;
    const size_t cycle = wide.size() + 5;
    return fromWide(takeColumns(loop, cycle == 0 ? 0 : offset % cycle, max_columns));
}

std::vector<std::string> wrapDisplayText(const std::string& text, int max_columns) {
    std::vector<std::string> lines;
    const std::wstring wide = toWide(text);
    if (wide.empty()) return lines;

    size_t start = 0;
    while (start < wide.size()) {
        std::wstring line;
        int columns = 0;
        size_t end = start;
        size_t last_space = std::wstring::npos;
        for (; end < wide.size(); ++end) {
            const int width = terminalWidth(wide[end]);
            if (columns + width > max_columns) break;
            line += wide[end];
            columns += width;
            if (wide[end] == L' ') last_space = end;
        }
        if (end < wide.size() && last_space != std::wstring::npos && last_space >= start) {
            end = last_space + 1;
            line = wide.substr(start, last_space - start);
        }
        if (end == start) {
            line = wide.substr(start, 1);
            ++end;
        }
        while (!line.empty() && line.back() == L' ') line.pop_back();
        lines.push_back(fromWide(line));
        start = end;
        while (start < wide.size() && wide[start] == L' ') ++start;
    }
    return lines;
}

// --- 全局变量 ---
AppController ctrl;
KittyCover kitty_cover;
PageMenu main_menu_page;
PageMenu playlist_manager_page;
PageMenu playlist_menu_page;    // 歌单功能菜单
PageMenu playlist_view_page;    // 歌单歌曲浏览菜单
PageMenu current_playlist_page;  // 当前播放列表菜单
PageMenu playlist_edit_page;
PageMenu add_to_playlist_page;
PageMenu sort_menu_page;
AppState previous_state = AppState::PLAYING;
int current_selected_playlist_index = -1;  // 当前选中的歌单索引
int current_operating_song_index = -1;     // 当前操作的歌曲索引（歌单浏览）
int current_playlist_song_index = -1;      // 当前播放列表中选中的歌曲索引
std::string song_to_add_path = "";         // 要添加的歌曲路径（如果为空，则添加当前播放的歌曲）
SortBy selected_sort_by = SortBy::TITLE;   // 选择的排序方式
PageMenu sort_order_page;                 // 排序顺序菜单页面

// --- 辅助函数 ---
void enterHelp() {
    previous_state = ctrl.state;
    ctrl.state = AppState::HELP;
}

// --- 渲染函数 ---
void renderPlaying() {
    auto& player = ctrl.getPlayer();
    const bool cover_area = kitty_cover.isSupported() && COLS >= 70 && LINES >= 24;
    const int content_x = cover_area ? KittyCover::column + KittyCover::width + 3 : 2;
    const int content_width = std::max(10, COLS - content_x - 2);
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const long long title_tick = std::chrono::duration_cast<std::chrono::milliseconds>(now).count() / 250;

    std::string mode_name;
    switch (ctrl.mode) {
        case PlayMode::SEQUENTIAL: mode_name = "顺序"; break;
        case PlayMode::SHUFFLE: mode_name = "乱序"; break;
        case PlayMode::SINGLE: mode_name = "单曲循环"; break;
    }

    std::string playlist_name = "无歌单";
    if (ctrl.currentPlaylistIndex >= 0 && ctrl.currentPlaylistIndex < static_cast<int>(ctrl.playlists.size()))
        playlist_name = ctrl.playlists[ctrl.currentPlaylistIndex]->name;

    const std::string status = fitDisplayText(
        "歌单: " + playlist_name + " | 模式: " + mode_name + " | 音量: " +
        std::to_string(ctrl.getVolumeUnlocked()) + "%", COLS - 4);
    mvprintw(0, 2, "%s", status.c_str());

    if (ctrl.currentPlaylistIndex < 0 || ctrl.currentPlaylistIndex >= static_cast<int>(ctrl.playlists.size()) ||
        ctrl.playlists[ctrl.currentPlaylistIndex]->empty()) {
        mvprintw(LINES / 2, std::max(0, (COLS - 20) / 2), "--- 暂无歌曲 ---");
        mvprintw(LINES / 2 + 1, std::max(0, (COLS - 30) / 2), "请按 [M] 进入菜单选择歌单");
    } else {
        const auto& player_song = player.getCurrentSong();
        const double elapsed = player.getElapsedSeconds();
        const auto& song_info = ctrl.getCurrentSong();

        const std::string song_number = "[" + std::to_string(ctrl.currentSongIndex + 1) + "/" +
                                        std::to_string(ctrl.getCurrentPlaylistSize()) + "] ";
        const std::string title = marqueeText(song_number + song_info.title,
                                               content_width, title_tick, 0);
        const std::string artist_album = marqueeText(song_info.artist + " · " + song_info.album,
                                                      content_width, title_tick, 1);
        mvprintw(2, content_x, "%s", title.c_str());
        mvprintw(4, content_x, "%s", artist_album.c_str());

        const int lyric_prefix_x = content_x;
        const int lyric_text_x = content_x + 3;
        const int lyric_width = std::max(4, content_width - 3);
        const int lyric_start_y = 6;
        constexpr int lyric_rows = 2;
        constexpr int lyric_gap = 1;

        if (player_song.lyrics.empty()) {
            if (elapsed > 0.1) {
                attron(COLOR_PAIR(2) | A_DIM);
                mvprintw(lyric_start_y, lyric_text_x, "未找到歌词");
                attroff(COLOR_PAIR(2) | A_DIM);
            }
        } else {
            int lyric_idx = -1;
            for (int i = 0; i < static_cast<int>(player_song.lyrics.size()); ++i) {
                if (elapsed >= player_song.lyrics[i].timestamp) lyric_idx = i;
                else break;
            }

            for (int offset = -1; offset <= 1; ++offset) {
                const int idx = lyric_idx + offset;
                if (idx < 0 || idx >= static_cast<int>(player_song.lyrics.size())) continue;

                const int text_width = lyric_width;
                const auto lines = wrapDisplayText(player_song.lyrics[idx].text, text_width);
                if (lines.empty()) continue;

                const size_t window_count = lines.size() > lyric_rows ? lines.size() - lyric_rows + 1 : 1;
                const double current_lyric_time = lyric_idx >= 0
                    ? player_song.lyrics[lyric_idx].timestamp : 0.0;
                const double visible_seconds = std::max(0.0, elapsed - current_lyric_time);
                const size_t scroll_tick = visible_seconds <= 1.5
                    ? 0
                    : static_cast<size_t>((visible_seconds - 1.5) / 1.2);
                const size_t first_line = lines.size() > lyric_rows
                    ? scroll_tick % (window_count + 1) : 0;
                const size_t scroll_start = first_line == window_count ? 0 : first_line;
                const int block_y = lyric_start_y + (offset + 1) * (lyric_rows + lyric_gap);

                if (offset == 0) attron(COLOR_PAIR(1) | A_BOLD);
                for (int row = 0; row < lyric_rows; ++row) {
                    const size_t line_index = scroll_start + static_cast<size_t>(row);
                    if (line_index >= lines.size() || block_y + row >= LINES - 4) break;
                    if (offset == 0 && row == 0) {
                        mvprintw(block_y + row, lyric_prefix_x, ">>");
                    }
                    mvprintw(block_y + row, lyric_text_x, "%s", lines[line_index].c_str());
                }
                if (offset == 0) attroff(COLOR_PAIR(1) | A_BOLD);
            }
        }

        const int bar_width = std::max(10, COLS - 20);
        const int pos = player_song.duration > 0
            ? static_cast<int>(elapsed / player_song.duration * bar_width) : 0;
        mvprintw(LINES - 2, 2, "%02d:%02d [", static_cast<int>(elapsed) / 60,
                 static_cast<int>(elapsed) % 60);
        for (int i = 0; i < bar_width; ++i) addch(i < pos ? '=' : (i == pos ? '>' : ' '));
        printw("] %02d:%02d", player_song.duration / 60, player_song.duration % 60);
    }
    mvprintw(LINES - 4, 2, "[H]帮助");
}

void renderMainMenu() {
    std::vector<std::string> options = {
        "返回播放",
        "当前播放列表",
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
    
    // 歌单浏览：总是显示原始顺序
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
    snprintf(title, sizeof(title), "歌单浏览: %s (%zu 首)", 
             playlist->name.c_str(), playlist->size());
    drawPageMenu(title, options, playlist_view_page, false);
}

void renderCurrentPlaylistView() {
    std::vector<std::string> options;
    
    // 检查是否有当前播放的歌单
    if (ctrl.currentPlaylistIndex < 0 || ctrl.currentPlaylistIndex >= (int)ctrl.playlists.size()) {
        options.push_back("--- 暂无播放列表 ---");
        options.push_back("请先选择一个歌单进行播放");
    } else {
        auto& playlist = ctrl.playlists[ctrl.currentPlaylistIndex];
        
        // 当前播放列表：根据播放模式显示
        bool is_shuffled = (ctrl.getPlayMode() == PlayMode::SHUFFLE);
        
        for (int i = 0; i < (int)playlist->size(); ++i) {
            const auto& song = ctrl.getSongAt(i); // 这个函数已经考虑了乱序模式
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "%s - %s", 
                    song.title.c_str(), song.artist.c_str());
            options.push_back(buffer);
        }
        
        // 如果没有歌曲，显示提示
        if (options.empty()) {
            options.push_back("--- 歌单为空 ---");
        }
    }
    
    char title[128];
    std::string mode_name;
    switch (ctrl.getPlayMode()) {
        case PlayMode::SEQUENTIAL:
            mode_name = "顺序";
            break;
        case PlayMode::SHUFFLE:
            mode_name = "乱序";
            break;
        case PlayMode::SINGLE:
            mode_name = "单曲循环";
            break;
    }
    std::string playlist_name = "无歌单";
    if (ctrl.currentPlaylistIndex >= 0 && ctrl.currentPlaylistIndex < (int)ctrl.playlists.size()) {
        playlist_name = ctrl.playlists[ctrl.currentPlaylistIndex]->name;
    }
    snprintf(title, sizeof(title), "当前播放列表: %s (%s)", 
             playlist_name.c_str(), mode_name.c_str());
    drawPageMenu(title, options, current_playlist_page, false);
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
    
    // 获取歌曲信息（考虑当前是否在浏览当前播放的歌单）
    const SongEntry* song_ptr = nullptr;
    if (current_selected_playlist_index == ctrl.currentPlaylistIndex && 
        ctrl.currentPlaylistIndex >= 0) {
        // 如果是当前播放的歌单，使用AppController的接口（考虑乱序模式）
        song_ptr = &ctrl.getSongAt(current_operating_song_index);
    } else {
        // 其他歌单，直接访问
        if (current_operating_song_index >= 0 && 
            current_operating_song_index < (int)playlist->size()) {
            song_ptr = &playlist->getSongs()[current_operating_song_index];
        }
    }
    
    char song_info[256] = "未知歌曲";
    if (song_ptr) {
        snprintf(song_info, sizeof(song_info), "%s - %s", 
                 song_ptr->title.c_str(), song_ptr->artist.c_str());
    }
    
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

void renderCurrentPlaylistSongMenu() {
    // 检查是否有当前播放的歌单
    if (ctrl.currentPlaylistIndex < 0 || ctrl.currentPlaylistIndex >= (int)ctrl.playlists.size() ||
        current_playlist_song_index < 0) {
        return;
    }
    
    auto& playlist = ctrl.playlists[ctrl.currentPlaylistIndex];
    if (current_playlist_song_index >= (int)playlist->size()) {
        return;
    }
    
    // 获取歌曲信息（考虑乱序模式）
    const SongEntry& song = ctrl.getSongAt(current_playlist_song_index);
    
    char song_info[256];
    snprintf(song_info, sizeof(song_info), "%s - %s", 
             song.title.c_str(), song.artist.c_str());
    
    std::vector<std::string> options = {
        "播放此歌曲",
        "从播放列表移除",
        "添加到指定歌单",
        "返回播放列表"
    };
    
    char title[256];
    snprintf(title, sizeof(title), "播放列表歌曲操作: %s", song_info);
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

void renderSortOrderMenu() {
    std::vector<std::string> options = {
        "升序 (A-Z, 旧到新)",
        "降序 (Z-A, 新到旧)",
        "返回排序方式"
    };
    drawPageMenu("排序顺序", options, sort_order_page, false);
}

void renderPlayMode() {
    std::vector<std::string> options = {
        "顺序播放",
        "乱序播放",
        "单曲循环",
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
    } else if (ch == ']' || ch == '}') {
        ctrl.seekForward();
    } else if (ch == '[' || ch == '{') {
        ctrl.seekBackward();
    } else if (ch == 'a' || ch == 'A') {
        if (!ctrl.getCurrentSongPath().empty()) {
            ctrl.state = AppState::ADD_TO_PLAYLIST;
            add_to_playlist_page.selected_index = 0;
            add_to_playlist_page.update(ctrl.playlists.size());
        }
    } else if (ch == 'h' || ch == 'H') {
        enterHelp();
    } else if (ch == '-' || ch == '_') { // -键（减号）
        ctrl.decreaseVolume();
    } else if (ch == '=' || ch == '+') { // =键（未按shift）或+键（按shift）
        ctrl.increaseVolume();
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
                // 当前播放列表：浏览当前播放的歌单（考虑播放模式）
                ctrl.state = AppState::CURRENT_PLAYLIST_VIEW;
                if (ctrl.currentPlaylistIndex >= 0 && ctrl.currentPlaylistIndex < (int)ctrl.playlists.size()) {
                    auto& playlist = ctrl.playlists[ctrl.currentPlaylistIndex];
                    current_playlist_page.update(playlist->size());
                    current_playlist_page.current_page = 0;
                    current_playlist_page.selected_index = ctrl.currentSongIndex; // 选中当前播放的歌曲
                } else {
                    current_playlist_page.update(0);
                    current_playlist_page.current_page = 0;
                    current_playlist_page.selected_index = 0;
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
                        ctrl.requestLoad(); // 请求加载歌曲
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

void handleCurrentPlaylistViewInput(int ch) {
    // 检查是否有当前播放的歌单
    if (ctrl.currentPlaylistIndex < 0 || ctrl.currentPlaylistIndex >= (int)ctrl.playlists.size()) {
        // 没有当前播放的歌单，按Q返回主菜单
        if (ch == 'q' || ch == 'Q') {
            ctrl.state = AppState::MAIN_MENU;
        }
        return;
    }
    
    auto& playlist = ctrl.playlists[ctrl.currentPlaylistIndex];
    int song_count = playlist->size();
    
    if (ch == KEY_UP) {
        current_playlist_page.moveUp();
    } else if (ch == KEY_DOWN) {
        current_playlist_page.moveDown();
    } else if (ch == KEY_PPAGE) {
        current_playlist_page.prevPage();
    } else if (ch == KEY_NPAGE) {
        current_playlist_page.nextPage();
    } else if (ch == '\n' || ch == 13) {
        int selected = current_playlist_page.selected_index;
        
        if (selected < song_count) {
            // 记录选中的歌曲索引，然后打开歌曲操作菜单
            current_playlist_song_index = selected;
            ctrl.state = AppState::CURRENT_PLAYLIST_SONG_MENU;
        }
    } else if (ch == 'h' || ch == 'H') {
        enterHelp();
    } else if (ch == 'q' || ch == 'Q') {
        // 返回主菜单
        ctrl.state = AppState::MAIN_MENU;
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
                        // 获取要添加的歌曲路径（考虑当前是否在浏览当前播放的歌单）
                        if (current_selected_playlist_index == ctrl.currentPlaylistIndex && 
                            ctrl.currentPlaylistIndex >= 0) {
                            // 如果是当前播放的歌单，使用AppController的接口（考虑乱序模式）
                            song_to_add_path = ctrl.getSongAt(current_operating_song_index).path;
                        } else {
                            // 其他歌单，直接访问
                            const auto& song = playlist->getSongs()[current_operating_song_index];
                            song_to_add_path = song.path;
                        }
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

void handleCurrentPlaylistSongMenuInput(int ch) {
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
                if (ctrl.currentPlaylistIndex >= 0 && 
                    ctrl.currentPlaylistIndex < (int)ctrl.playlists.size() &&
                    current_playlist_song_index >= 0) {
                    auto& playlist = ctrl.playlists[ctrl.currentPlaylistIndex];
                    if (current_playlist_song_index < (int)playlist->size()) {
                        ctrl.currentSongIndex = current_playlist_song_index;
                        ctrl.requestLoad();
                        ctrl.state = AppState::PLAYING;
                    }
                }
                break;
            case 1:
                // 从播放列表移除（从当前歌单删除）
                if (ctrl.currentPlaylistIndex >= 0 && 
                    ctrl.currentPlaylistIndex < (int)ctrl.playlists.size() &&
                    current_playlist_song_index >= 0) {
                    // 需要将乱序索引转换为原始索引
                    int actual_index = current_playlist_song_index;
                    if (ctrl.getPlayMode() == PlayMode::SHUFFLE && !ctrl.getShuffleOrder().empty()) {
                        if (current_playlist_song_index >= 0 && current_playlist_song_index < (int)ctrl.getShuffleOrder().size()) {
                            actual_index = ctrl.getShuffleOrder()[current_playlist_song_index];
                        }
                    }
                    
                    ctrl.removeSongFromPlaylist(ctrl.currentPlaylistIndex, actual_index);
                    
                    // 返回播放列表
                    ctrl.state = AppState::CURRENT_PLAYLIST_VIEW;
                    // 更新播放列表显示
                    auto& playlist = ctrl.playlists[ctrl.currentPlaylistIndex];
                    current_playlist_page.update(playlist->size());
                    // 重置选中索引
                    if (current_playlist_page.selected_index >= (int)playlist->size()) {
                        current_playlist_page.selected_index = std::max(0, (int)playlist->size() - 1);
                    }
                    // 更新当前歌曲索引
                    if (ctrl.currentSongIndex >= (int)playlist->size()) {
                        ctrl.currentSongIndex = std::max(0, (int)playlist->size() - 1);
                    }
                }
                break;
            case 2:
                // 添加到指定歌单
                if (ctrl.currentPlaylistIndex >= 0 && 
                    ctrl.currentPlaylistIndex < (int)ctrl.playlists.size() &&
                    current_playlist_song_index >= 0) {
                    auto& playlist = ctrl.playlists[ctrl.currentPlaylistIndex];
                    if (current_playlist_song_index < (int)playlist->size()) {
                        // 获取要添加的歌曲路径（考虑乱序模式）
                        song_to_add_path = ctrl.getSongAt(current_playlist_song_index).path;
                        // 进入添加到歌单界面
                        ctrl.state = AppState::ADD_TO_PLAYLIST;
                        add_to_playlist_page.selected_index = 0;
                        add_to_playlist_page.update(ctrl.playlists.size());
                    }
                }
                break;
            case 3:
                // 返回播放列表
                ctrl.state = AppState::CURRENT_PLAYLIST_VIEW;
                break;
        }
    } else if (ch == 'h' || ch == 'H') {
        enterHelp();
    } else if (ch == 'q' || ch == 'Q') {
        // 返回播放列表
        ctrl.state = AppState::CURRENT_PLAYLIST_VIEW;
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
            
            // 保存选择的排序方式，然后进入排序顺序菜单
            selected_sort_by = by;
            ctrl.state = AppState::SORT_ORDER_MENU;
            sort_order_page.selected_index = 0;
            sort_order_page.update(2); // 升序和降序两个选项
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

void handleSortOrderMenuInput(int ch) {
    if (ch == KEY_UP) {
        sort_order_page.moveUp();
    } else if (ch == KEY_DOWN) {
        sort_order_page.moveDown();
    } else if (ch == KEY_PPAGE) {
        sort_order_page.prevPage();
    } else if (ch == KEY_NPAGE) {
        sort_order_page.nextPage();
    } else if (ch == '\n' || ch == 13) {
        int selected = sort_order_page.selected_index;
        
        if (selected == 0) {
            // 升序
            ctrl.sortPlaylist(current_selected_playlist_index, selected_sort_by, SortOrder::ASCENDING);
            ctrl.state = AppState::PLAYLIST_MENU;
        } else if (selected == 1) {
            // 降序
            ctrl.sortPlaylist(current_selected_playlist_index, selected_sort_by, SortOrder::DESCENDING);
            ctrl.state = AppState::PLAYLIST_MENU;
        } else if (selected == 2) {
            // 返回排序方式
            ctrl.state = AppState::PLAYLIST_SORT;
        }
    } else if (ch == 'h' || ch == 'H') {
        enterHelp();
    } else if (ch == 'q' || ch == 'Q') {
        ctrl.state = AppState::PLAYLIST_SORT;
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
    if (ch == KEY_UP) {
        main_menu_page.moveUp();
    } else if (ch == KEY_DOWN) {
        main_menu_page.moveDown();
    } else if (ch == KEY_PPAGE) {
        main_menu_page.prevPage();
    } else if (ch == KEY_NPAGE) {
        main_menu_page.nextPage();
    } else if (ch == '\n' || ch == 13) {
        // 根据用户的选择设置播放模式
        PlayMode selected_mode;
        switch (main_menu_page.selected_index) {
            case 0:
                selected_mode = PlayMode::SEQUENTIAL;
                break;
            case 1:
                selected_mode = PlayMode::SHUFFLE;
                break;
            case 2:
                selected_mode = PlayMode::SINGLE;
                break;
            default:
                // 返回设置
                ctrl.state = AppState::SETTINGS_MENU;
                main_menu_page.selected_index = 0;
                return;
        }
        
        // 只有当选择的模式与当前模式不同时才调用togglePlayMode
        // 由于togglePlayMode现在是循环切换，我们需要多次调用直到达到目标模式
        while (ctrl.getPlayMode() != selected_mode) {
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
    init_pair(1, COLOR_CYAN, -1);      // 青色：当前歌词高亮
    init_pair(2, COLOR_WHITE, -1);     // 白色：可用于灰色效果（配合A_DIM）

    // 初始化页面菜单
    main_menu_page.items_per_page = 10;
    playlist_manager_page.items_per_page = 15;
    playlist_menu_page.items_per_page = 10;    // 歌单功能菜单
    playlist_view_page.items_per_page = 15;    // 歌单歌曲浏览
    current_playlist_page.items_per_page = 15; // 当前播放列表
    playlist_edit_page.items_per_page = 10;
    add_to_playlist_page.items_per_page = 15;
    sort_menu_page.items_per_page = 10;
    sort_order_page.items_per_page = 10;

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
                case AppState::CURRENT_PLAYLIST_VIEW:
                    handleCurrentPlaylistViewInput(ch);
                    break;
                case AppState::CURRENT_PLAYLIST_SONG_MENU:
                    handleCurrentPlaylistSongMenuInput(ch);
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
                case AppState::SORT_ORDER_MENU:
                    handleSortOrderMenuInput(ch);
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
        bool cover_visible = false;
        std::string cover_song;
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
                case AppState::CURRENT_PLAYLIST_VIEW:
                    renderCurrentPlaylistView();
                    break;
                case AppState::CURRENT_PLAYLIST_SONG_MENU:
                    renderCurrentPlaylistSongMenu();
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
                case AppState::SORT_ORDER_MENU:
                    renderSortOrderMenu();
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
                        case AppState::CURRENT_PLAYLIST_VIEW:
                            drawHelp(PLAYLIST_VIEW_HELP); // 使用相同的帮助
                            break;
                        case AppState::CURRENT_PLAYLIST_SONG_MENU:
                            drawHelp(SONG_OPERATION_MENU_HELP); // 使用歌曲操作菜单的帮助
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
                        case AppState::SORT_ORDER_MENU:
                            drawHelp(SORT_MENU_HELP); // 使用相同的帮助信息
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
            cover_visible = ctrl.state == AppState::PLAYING;
            if (cover_visible) cover_song = ctrl.getCurrentSongPath();
        }
        refresh();
        kitty_cover.update(cover_visible, cover_song, COLS, LINES);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    kitty_cover.clear();
    endwin();
    return 0;
}