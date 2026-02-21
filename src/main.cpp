#include <ncurses.h>
#include <clocale>
#include <thread>
#include <chrono>
#include <vector>
#include <filesystem>
#include "AppController.hpp"

namespace fs = std::filesystem;

// --- UI辅助函数 ---
void drawScrollingMenu(const std::string& title, const std::vector<std::string>& options, int highlight) {
    int total = (int)options.size();
    mvprintw(1, 2, "--- %s ---", title.c_str());
    if (total == 0) {
        mvprintw(3, 4, "[列表为空]");
    } else {
        int max_rows = LINES - 8;
        int start_index = (highlight >= max_rows) ? highlight - max_rows + 1 : 0;
        for (int i = 0; i < (max_rows) && (start_index + i) < total; ++i) {
            int idx = start_index + i;
            if (idx == highlight) attron(A_REVERSE);
            mvprintw(3 + i, 4, "%s %s", (idx == highlight ? ">" : " "), options[idx].substr(0, std::max(0, COLS - 10)).c_str());
            if (idx == highlight) attroff(A_REVERSE);
        }
    }
    mvprintw(LINES - 4, 2, "[ENTER]选择 | [ESC]返回上级");
}

std::string inputField(const std::string& prompt) {
    // 临时进入阻塞模式进行输入
    echo(); curs_set(1); nodelay(stdscr, FALSE);
    char buf[256];
    erase(); // 清屏，避免菜单干扰输入框显示
    mvprintw(LINES / 2, 4, "%s", prompt.c_str());
    refresh();
    getstr(buf);
    noecho(); curs_set(0); nodelay(stdscr, TRUE);
    return std::string(buf);
}

void renderPlayer(AppController& ctrl) {
    auto& player = ctrl.getPlayer();
    std::string mode_name = (ctrl.mode == PlayMode::SHUFFLE ? "乱序" : "顺序");
    mvprintw(1, 2, "目录: %s | 模式: %s", ctrl.music_dir.c_str(), mode_name.c_str());

    if (ctrl.current_playlist.empty()) {
        mvprintw(LINES / 2, (COLS - 20) / 2, "--- 暂无歌曲 ---");
        mvprintw(LINES / 2 + 1, (COLS - 30) / 2, "请按 [M] 进入设置修改目录");
    } else {
        auto& song = player.getCurrentSong();
        double elapsed = player.getElapsedSeconds();
        
        mvprintw(3, 2, "当前播放：[%d/%d] %s - %s", 
                 ctrl.currentIndex + 1, (int)ctrl.current_playlist.size(), 
                 song.title.c_str(), song.artist.c_str());

        int lyricIdx = -1;
        for (int i = 0; i < (int)song.lyrics.size(); ++i) {
            if (elapsed >= song.lyrics[i].timestamp) lyricIdx = i; else break;
        }
        for (int offset = -1; offset <= 1; ++offset) {
            int idx = lyricIdx + offset;
            if (idx >= 0 && idx < (int)song.lyrics.size()) {
                if (offset == 0) attron(COLOR_PAIR(1) | A_BOLD);
                mvprintw(8 + offset * 2, 4 + (offset == 0 ? 0 : 3), "%s%s", (offset == 0 ? ">> " : ""), song.lyrics[idx].text.c_str());
                if (offset == 0) attroff(COLOR_PAIR(1) | A_BOLD);
            }
        }

        int barWidth = std::max(10, COLS - 20);
        int pos = (song.duration > 0) ? (int)(elapsed / song.duration * barWidth) : 0;
        mvprintw(LINES - 2, 2, "%02d:%02d [", (int)elapsed / 60, (int)elapsed % 60);
        for (int i = 0; i < barWidth; ++i) addch(i < pos ? '=' : (i == pos ? '>' : ' '));
        printw("] %02d:%02d", (int)song.duration / 60, (int)song.duration % 60);
    }
    mvprintw(LINES - 4, 2, "[M]菜单 | [空格]暂停 | [左右]切歌 | [Q]退出");
}

int main() {
    setlocale(LC_ALL, "");
    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE); nodelay(stdscr, TRUE); curs_set(0);
    start_color(); 
    init_pair(1, COLOR_CYAN, COLOR_BLACK);

    AppController ctrl;
    int highlight = 0;

    while (ctrl.isRunning()) {
        // --- 1. 处理输入逻辑 (Input) ---
        int ch = getch();
        if (ch != ERR) {
            if (ch == 'q') ctrl.stop();

            if (ch == 27) { // ESC 逻辑
                switch (ctrl.state) {
                    case AppState::MAIN_MENU: ctrl.state = AppState::PLAYING; break;
                    case AppState::SETTINGS_MENU: ctrl.state = AppState::MAIN_MENU; highlight = 1; break;
                    case AppState::SET_MODE: 
                    case AppState::SET_DIR: ctrl.state = AppState::SETTINGS_MENU; highlight = 0; break;
                    case AppState::PLAYLIST: ctrl.state = AppState::MAIN_MENU; highlight = 2; break;
                    default: break;
                }
            }

            switch (ctrl.state) {
                case AppState::PLAYING:
                    if (ch == 'm') { ctrl.state = AppState::MAIN_MENU; highlight = 0; }
                    if (ch == ' ') ctrl.togglePause();
                    if (ch == KEY_RIGHT) ctrl.nextSong();
                    if (ch == KEY_LEFT) ctrl.prevSong();
                    break;

                case AppState::MAIN_MENU:
                    if (ch == KEY_UP) highlight = (highlight - 1 + 3) % 3;
                    if (ch == KEY_DOWN) highlight = (highlight + 1) % 3;
                    if (ch == '\n' || ch == 13) {
                        if (highlight == 0) ctrl.state = AppState::PLAYING;
                        else if (highlight == 1) { ctrl.state = AppState::SETTINGS_MENU; highlight = 0; }
                        else if (highlight == 2) { 
                            std::lock_guard<std::mutex> lock(ctrl.getMutex());
                            ctrl.state = AppState::PLAYLIST; 
                            highlight = ctrl.currentIndex; 
                        }
                    }
                    break;

                case AppState::SETTINGS_MENU:
                    if (ch == KEY_UP) highlight = (highlight - 1 + 2) % 2;
                    if (ch == KEY_DOWN) highlight = (highlight + 1) % 2;
                    if (ch == '\n' || ch == 13) {
                        if (highlight == 0) { 
                            ctrl.state = AppState::SET_MODE; 
                            highlight = (ctrl.mode == PlayMode::SEQUENTIAL ? 0 : 1); 
                        }
                        else if (highlight == 1) {
                            // 关键改进：在这里直接调用，不等待下一轮循环
                            std::string new_path = inputField("输入新目录路径: ");
                            if (!new_path.empty()) ctrl.updateConfigDir(new_path);
                            // 输入完成后自动回到设置菜单
                            highlight = 1; 
                        }
                    }
                    break;

                case AppState::SET_MODE:
                    if (ch == KEY_UP || ch == KEY_DOWN) highlight = 1 - highlight;
                    if (ch == '\n' || ch == 13) {
                        ctrl.togglePlayMode();
                        ctrl.state = AppState::SETTINGS_MENU; highlight = 0;
                    }
                    break;

                case AppState::PLAYLIST: {
                    std::lock_guard<std::mutex> lock(ctrl.getMutex());
                    int size = (int)ctrl.current_playlist.size();
                    if (size > 0) {
                        if (ch == KEY_UP) highlight = (highlight - 1 + size) % size;
                        if (ch == KEY_DOWN) highlight = (highlight + 1) % size;
                        if (ch == '\n' || ch == 13) {
                            ctrl.playAtIndex(highlight);
                            ctrl.state = AppState::PLAYING;
                        }
                    }
                    break;
                }
                default: break;
            }
        }

        // --- 2. 渲染逻辑 (Render) ---
        erase();
        {
            std::lock_guard<std::mutex> lock(ctrl.getMutex());
            switch (ctrl.state) {
                case AppState::PLAYING:
                    renderPlayer(ctrl);
                    break;
                case AppState::MAIN_MENU:
                    drawScrollingMenu("主菜单", {"返回播放", "设置", "播放列表"}, highlight);
                    break;
                case AppState::SETTINGS_MENU:
                    drawScrollingMenu("设置", {"播放模式", "修改播放目录"}, highlight);
                    break;
                case AppState::SET_MODE:
                    drawScrollingMenu("播放模式", {"顺序播放", "乱序播放"}, highlight);
                    break;
                case AppState::PLAYLIST: {
                    std::vector<std::string> names;
                    for (auto& p : ctrl.current_playlist) names.push_back(fs::path(p).filename().string());
                    drawScrollingMenu("播放列表", names, highlight);
                    break;
                }
                default: break;
            }
        }
        refresh();
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    endwin();
    return 0;
}