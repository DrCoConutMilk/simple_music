#include <ncurses.h>
#include <clocale>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <random>
#include <thread>
#include <vector>
#include <cstdlib> // 必须引入，用于 getenv
#include <nlohmann/json.hpp>
#include "MusicPlayer.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

enum class AppState { PLAYING, MAIN_MENU, SETTINGS_MENU, SET_MODE, SET_DIR, PLAYLIST };
enum class PlayMode { SEQUENTIAL, SHUFFLE };

struct AppConfig {
    std::string music_dir = "./music";
    PlayMode mode = PlayMode::SEQUENTIAL;
    std::vector<std::string> original_list;
    std::vector<std::string> current_playlist;
    int currentIndex = 0;
    AppState state = AppState::PLAYING;
};

// --- 获取跨平台的配置文件路径 ---
fs::path getConfigFilePath() {
    const char* home_env = std::getenv("HOME");
    fs::path config_path;

    if (home_env) {
        // 遵循 XDG 标准：~/.config/软件名/config.json
        config_path = fs::path(home_env) / ".config" / "simple_music_player";
    } else {
        // 回退方案：当前目录
        config_path = fs::current_path();
    }

    // 确保配置文件夹存在
    if (!fs::exists(config_path)) {
        fs::create_directories(config_path);
    }

    return config_path / "config.json";
}

// --- 配置持久化 ---
void saveConfig(const AppConfig& cfg) {
    json j;
    j["music_directory"] = cfg.music_dir;
    j["play_mode"] = (cfg.mode == PlayMode::SHUFFLE ? "shuffle" : "sequential");
    
    std::ofstream o(getConfigFilePath());
    if (o.is_open()) {
        o << j.dump(4);
    }
}

void initConfig(AppConfig& cfg) {
    fs::path p = getConfigFilePath();
    
    if (!fs::exists(p)) {
        saveConfig(cfg);
        return;
    }

    std::ifstream i(p);
    try {
        json j = json::parse(i);
        cfg.music_dir = j.value("music_directory", "./music");
        std::string m = j.value("play_mode", "sequential");
        cfg.mode = (m == "shuffle" ? PlayMode::SHUFFLE : PlayMode::SEQUENTIAL);
    } catch (...) {
        saveConfig(cfg);
    }
}

// --- 列表逻辑 ---
void scanMusic(AppConfig& cfg) {
    cfg.original_list.clear();
    // 允许使用相对路径或绝对路径
    fs::path m_path = cfg.music_dir;
    if (!fs::exists(m_path)) {
        try {
            fs::create_directories(m_path);
        } catch (...) {
            return; // 无法创建目录则返回
        }
    }

    for (const auto& entry : fs::directory_iterator(m_path)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower); // 统一转小写进行匹配
        if (ext == ".mp3" || ext == ".flac") {
            cfg.original_list.push_back(entry.path().string());
        }
    }
    std::sort(cfg.original_list.begin(), cfg.original_list.end());
}

void applyPlayMode(AppConfig& cfg) {
    std::string current_song = (cfg.current_playlist.empty() || cfg.currentIndex >= (int)cfg.current_playlist.size()) 
                                ? "" : cfg.current_playlist[cfg.currentIndex];
    
    cfg.current_playlist = cfg.original_list;
    if (cfg.current_playlist.empty()) {
        cfg.currentIndex = 0;
        return;
    }

    if (cfg.mode == PlayMode::SHUFFLE) {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(cfg.current_playlist.begin(), cfg.current_playlist.end(), g);
    }
    
    auto it = std::find(cfg.current_playlist.begin(), cfg.current_playlist.end(), current_song);
    cfg.currentIndex = (it != cfg.current_playlist.end()) ? (int)std::distance(cfg.current_playlist.begin(), it) : 0;
}

// --- UI 渲染 ---
void drawScrollingMenu(const std::string& title, const std::vector<std::string>& options, int highlight) {
    erase();
    int total = (int)options.size();
    if (total == 0) {
        mvprintw(1, 2, "--- %s ---", title.c_str());
        mvprintw(3, 4, "[列表为空]");
    } else {
        int max_rows = LINES - 6;
        int start_index = (highlight >= max_rows) ? highlight - max_rows + 1 : 0;
        mvprintw(1, 2, "--- %s (%d/%d) ---", title.c_str(), highlight + 1, total);
        for (int i = 0; i < max_rows && (start_index + i) < total; ++i) {
            int idx = start_index + i;
            if (idx == highlight) attron(A_REVERSE);
            mvprintw(3 + i, 4, "%s %s", (idx == highlight ? ">" : " "), options[idx].substr(0, std::max(0, COLS-10)).c_str());
            if (idx == highlight) attroff(A_REVERSE);
        }
    }
    mvprintw(LINES - 2, 2, "ENTER: 选择 | ESC: 返回上级");
    refresh();
}

std::string inputField(const std::string& prompt) {
    echo(); curs_set(1); nodelay(stdscr, FALSE);
    char buf[256];
    mvprintw(LINES/2, 4, "%s", prompt.c_str());
    getstr(buf);
    noecho(); curs_set(0); nodelay(stdscr, TRUE);
    return std::string(buf);
}

void renderPlayer(MusicPlayer& player, const AppConfig& cfg) {
    erase();
    std::string mode_name = (cfg.mode == PlayMode::SHUFFLE ? "乱序" : "顺序");
    mvprintw(1, 2, "目录: %s", cfg.music_dir.c_str());
    mvprintw(3, 2, "模式: %s", mode_name.c_str());

    if (cfg.current_playlist.empty()) {
        mvprintw(LINES/2, (COLS-20)/2, "--- 暂无歌曲 ---");
        mvprintw(LINES/2 + 1, (COLS-30)/2, "请按 [M] 进入设置修改目录");
    } else {
        auto& song = player.getCurrentSong();
        double elapsed = player.getElapsedSeconds();

        mvprintw(2, 2, "当前播放：[%d/%d] %s - %s", cfg.currentIndex + 1, (int)cfg.current_playlist.size(),
                 song.title.c_str(), song.artist.c_str());

        // 歌词滚动
        int lyricIdx = -1;
        for (int i = 0; i < (int)song.lyrics.size(); ++i) {
            if (elapsed >= song.lyrics[i].timestamp) lyricIdx = i;
            else break;
        }
        for (int offset = -1; offset <= 1; ++offset) {
            int idx = lyricIdx + offset;
            if (idx >= 0 && idx < (int)song.lyrics.size()) {
                if (offset == 0) attron(COLOR_PAIR(1) | A_BOLD);
                mvprintw(7 + offset * 2, 4 + (offset == 0 ? 0 : 3), "%s%s", (offset == 0 ? ">> " : ""), song.lyrics[idx].text.c_str());
                if (offset == 0) attroff(COLOR_PAIR(1) | A_BOLD);
            }
        }

        // 进度条
        int barWidth = std::max(10, COLS - 20);
        int pos = (song.duration > 0) ? (int)(elapsed / song.duration * barWidth) : 0;
        mvprintw(LINES - 2, 2, "%02d:%02d [", (int)elapsed/60, (int)elapsed%60);
        for(int i=0; i<barWidth; ++i) addch(i < pos ? '=' : (i == pos ? '>' : ' '));
        printw("] %02d:%02d", (int)song.duration/60, (int)song.duration%60);
    }

    mvprintw(LINES - 4, 2, "[M]菜单 | [空格]暂停 | [左右]切歌 | [Q]退出");
    refresh();
}

int main() {
    setlocale(LC_ALL, "");
    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE); nodelay(stdscr, TRUE); curs_set(0);
    start_color(); init_pair(1, COLOR_CYAN, COLOR_BLACK);

    AppConfig cfg;
    initConfig(cfg); // 现在会加载 ~/.config/CoCoPlayer/config.json
    scanMusic(cfg);
    applyPlayMode(cfg);

    MusicPlayer player;
    bool quit = false;
    int highlight = 0;
    bool needLoad = !cfg.current_playlist.empty();

    while (!quit) {
        if (needLoad && !cfg.current_playlist.empty()) {
            player.load(cfg.current_playlist[cfg.currentIndex]);
            player.play();
            needLoad = false;
        }

        if (!cfg.current_playlist.empty() && !player.isPlaying() && !player.isPaused() && cfg.state == AppState::PLAYING) {
            cfg.currentIndex = (cfg.currentIndex + 1) % cfg.current_playlist.size();
            needLoad = true;
        }

        int ch = getch();
        if (ch == 'q') quit = true;

        if (ch == 27) { // ESC
            switch (cfg.state) {
                case AppState::MAIN_MENU: cfg.state = AppState::PLAYING; break;
                case AppState::SETTINGS_MENU: cfg.state = AppState::MAIN_MENU; highlight = 1; break;
                case AppState::SET_MODE: 
                case AppState::SET_DIR: cfg.state = AppState::SETTINGS_MENU; highlight = 0; break;
                case AppState::PLAYLIST: cfg.state = AppState::MAIN_MENU; highlight = 2; break;
                default: break;
            }
        }

        switch (cfg.state) {
            case AppState::PLAYING:
                if (ch == 'm') { cfg.state = AppState::MAIN_MENU; highlight = 0; }
                if (!cfg.current_playlist.empty()) {
                    if (ch == ' ') player.isPaused() ? player.resume() : player.pause();
                    if (ch == KEY_RIGHT) { cfg.currentIndex = (cfg.currentIndex + 1) % cfg.current_playlist.size(); needLoad = true; }
                    if (ch == KEY_LEFT) { cfg.currentIndex = (cfg.currentIndex - 1 + (int)cfg.current_playlist.size()) % cfg.current_playlist.size(); needLoad = true; }
                }
                renderPlayer(player, cfg);
                break;

            case AppState::MAIN_MENU:
                drawScrollingMenu("主菜单", {"当前播放", "设置", "播放列表"}, highlight);
                if (ch == KEY_UP) highlight = (highlight - 1 + 3) % 3;
                if (ch == KEY_DOWN) highlight = (highlight + 1) % 3;
                if (ch == '\n' || ch == 13) {
                    if (highlight == 0) cfg.state = AppState::PLAYING;
                    else if (highlight == 1) { cfg.state = AppState::SETTINGS_MENU; highlight = 0; }
                    else if (highlight == 2) { cfg.state = AppState::PLAYLIST; highlight = cfg.currentIndex; }
                }
                break;

            case AppState::SETTINGS_MENU:
                drawScrollingMenu("设置", {"播放模式", "修改播放目录"}, highlight);
                if (ch == KEY_UP) highlight = (highlight - 1 + 2) % 2;
                if (ch == KEY_DOWN) highlight = (highlight + 1) % 2;
                if (ch == '\n' || ch == 13) {
                    if (highlight == 0) { cfg.state = AppState::SET_MODE; highlight = (cfg.mode == PlayMode::SEQUENTIAL ? 0 : 1); }
                    else cfg.state = AppState::SET_DIR;
                }
                break;

            case AppState::SET_MODE:
                drawScrollingMenu("播放模式", {"顺序播放", "乱序播放"}, highlight);
                if (ch == KEY_UP || ch == KEY_DOWN) highlight = 1 - highlight;
                if (ch == '\n' || ch == 13) {
                    cfg.mode = (highlight == 0 ? PlayMode::SEQUENTIAL : PlayMode::SHUFFLE);
                    saveConfig(cfg);
                    applyPlayMode(cfg);
                    cfg.state = AppState::SETTINGS_MENU; highlight = 0;
                }
                break;

            case AppState::SET_DIR: {
                std::string new_path = inputField("输入新目录路径: ");
                if (!new_path.empty()) {
                    cfg.music_dir = new_path;
                    saveConfig(cfg);
                    scanMusic(cfg);
                    applyPlayMode(cfg);
                    needLoad = !cfg.current_playlist.empty();
                    if (needLoad) cfg.currentIndex = 0;
                }
                cfg.state = AppState::SETTINGS_MENU; highlight = 1;
                break;
            }

            case AppState::PLAYLIST: {
                std::vector<std::string> names;
                for(auto& p : cfg.current_playlist) names.push_back(fs::path(p).filename().string());
                drawScrollingMenu("播放列表", names, highlight);
                if (!names.empty()) {
                    if (ch == KEY_UP) highlight = (highlight - 1 + (int)names.size()) % (int)names.size();
                    if (ch == KEY_DOWN) highlight = (highlight + 1) % (int)names.size();
                    if (ch == '\n' || ch == 13) {
                        cfg.currentIndex = highlight;
                        needLoad = true;
                        cfg.state = AppState::PLAYING;
                    }
                }
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    endwin();
    return 0;
}