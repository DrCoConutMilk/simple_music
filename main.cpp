#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <random>
#include <thread>
#include <nlohmann/json.hpp>
#include <ncurses.h> 
#include <clocale> 

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/flacfile.h>
#include <taglib/xiphcomment.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

struct LyricLine { double timestamp; std::string text; };

double parseTime(const std::string& t) {
    if (t.size() < 7 || t[0] != '[') return -1;
    try { return std::stoi(t.substr(1,2)) * 60 + std::stod(t.substr(4,5)); } catch(...) { return -1; }
}

std::string getLyrics(const std::string& path) {
    TagLib::MPEG::File m(path.c_str());
    if (m.isValid() && m.ID3v2Tag()) {
        auto f = m.ID3v2Tag()->frameList("USLT");
        if (!f.isEmpty()) return f.front()->toString().to8Bit(true);
    }
    TagLib::FLAC::File fl(path.c_str());
    if (fl.isValid() && fl.xiphComment()) {
        auto d = fl.xiphComment()->fieldListMap();
        if (d.contains("LYRICS")) return d["LYRICS"].front().to8Bit(true);
    }
    return "";
}

struct PlayerState {
    std::vector<std::string> playlist;
    int currentIndex = 0;
    bool isPaused = false;
    bool shouldQuit = false;
    bool manualSkip = false; // 用于区分自然播放完还是手动切歌
    std::string mode = "sequential";
};

int main() {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);

    // 启用颜色支持（用于高亮）
    start_color();
    init_pair(1, COLOR_CYAN, COLOR_BLACK); // 高亮歌词颜色

    std::ifstream cfgFile("config.json");
    if (!cfgFile) { endwin(); std::cerr << "Config not found\n"; return -1; }
    json cfg = json::parse(cfgFile);
    PlayerState state;
    state.mode = cfg.value("play_mode", "sequential");
    
    std::string music_dir = cfg["music_directory"].get<std::string>();
    for (const auto& entry : fs::directory_iterator(music_dir)) {
        std::string ext = entry.path().extension();
        if (ext == ".mp3" || ext == ".flac") state.playlist.push_back(entry.path().string());
    }
    
    if (state.playlist.empty()) { endwin(); std::cerr << "No music found\n"; return -1; }
    if (state.mode == "shuffle") std::shuffle(state.playlist.begin(), state.playlist.end(), std::random_device());

    SDL_Init(SDL_INIT_AUDIO);
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);

    while (!state.shouldQuit) {
        std::string path = state.playlist[state.currentIndex];
        Mix_Music* music = Mix_LoadMUS(path.c_str());
        
        TagLib::FileRef f(path.c_str());
        std::string title = f.tag()->title().to8Bit(true);
        std::string artist = f.tag()->artist().to8Bit(true);
        int totalSeconds = f.audioProperties()->length();

        auto lyrics = [](const std::string& p) {
            std::stringstream ss(getLyrics(p));
            std::vector<LyricLine> res; std::string l;
            while(std::getline(ss, l)) {
                if(l.size()>10 && std::isdigit(l[1])) res.push_back({parseTime(l.substr(0,10)), l.substr(10)});
            }
            return res;
        }(path);

        Mix_PlayMusic(music, 1);
        Uint32 startTicks = SDL_GetTicks();
        Uint32 pauseTotalTicks = 0;
        Uint32 pauseStartTicks = 0;
        state.manualSkip = false;

        // 播放循环
        while (!state.shouldQuit && !state.manualSkip) {
            // 检测音乐是否播放结束（自动切歌逻辑）
            if (!Mix_PlayingMusic() && !Mix_PausedMusic()) {
                state.currentIndex = (state.currentIndex + 1) % state.playlist.size();
                break; 
            }

            int ch = getch();
            switch (ch) {
                case ' ':
                    if (Mix_PausedMusic()) {
                        Mix_ResumeMusic();
                        pauseTotalTicks += (SDL_GetTicks() - pauseStartTicks);
                    } else {
                        Mix_PauseMusic();
                        pauseStartTicks = SDL_GetTicks();
                    }
                    break;
                case KEY_RIGHT:
                    state.currentIndex = (state.currentIndex + 1) % state.playlist.size();
                    state.manualSkip = true;
                    break;
                case KEY_LEFT:
                    state.currentIndex = (state.currentIndex - 1 + state.playlist.size()) % state.playlist.size();
                    state.manualSkip = true;
                    break;
                case 'q':
                    state.shouldQuit = true;
                    break;
            }

            double elapsed = 0;
            if (Mix_PausedMusic()) elapsed = (pauseStartTicks - startTicks - pauseTotalTicks) / 1000.0;
            else elapsed = (SDL_GetTicks() - startTicks - pauseTotalTicks) / 1000.0;

            erase();
            // 顶部：歌曲信息
            mvprintw(1, 2, "模式: %s | [%d/%d] %s - %s", 
                     state.mode.c_str(), state.currentIndex + 1, (int)state.playlist.size(), title.c_str(), artist.c_str());
            
            // 中间：三行歌词显示逻辑
            // 中间：三行歌词显示逻辑（行间距翻倍）
            int curIdx = -1;
            for (int i = 0; i < lyrics.size(); ++i) {
                if (elapsed >= lyrics[i].timestamp) curIdx = i;
                else break;
            }

            for (int offset = -1; offset <= 1; ++offset) {
                int targetIdx = curIdx + offset;
                // 修改这里：offset * 2 使得行与行之间空出一行
                int row = 10 + (offset * 2); 
                
                if (targetIdx >= 0 && targetIdx < lyrics.size()) {
                    // 清除当前行，防止上一首长歌词残留（ncurses 常用技巧）
                    move(row, 0);
                    clrtoeol(); 

                    if (offset == 0) {
                        attron(COLOR_PAIR(1) | A_BOLD);
                        // 当前歌词：居中感更强一点，或者保持偏移
                        mvprintw(row, 4, ">> %s", lyrics[targetIdx].text.c_str());
                        attroff(COLOR_PAIR(1) | A_BOLD);
                    } else {
                        // 上下歌词：稍微缩进，显示暗淡一些
                        mvprintw(row, 7, "%s", lyrics[targetIdx].text.c_str());
                    }
                }
            }

            // 底部：操作提示
            mvprintw(LINES - 4, 2, "操作: [空格]播放/暂停 | [方向左右]切歌 | [Q]退出");

            // 最底部：进度条
            int barWidth = COLS - 20; // 根据窗口宽度自动调整
            if (barWidth < 10) barWidth = 30;
            int pos = (totalSeconds > 0) ? (elapsed / totalSeconds * barWidth) : 0;
            
            mvprintw(LINES - 2, 2, "%02d:%02d [", (int)elapsed/60, (int)elapsed%60);
            for(int i=0; i<barWidth; ++i) addch(i < pos ? '=' : (i == pos ? '>' : ' '));
            printw("] %02d:%02d", totalSeconds/60, totalSeconds%60);

            refresh();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        Mix_FreeMusic(music);
    }

    Mix_CloseAudio();
    SDL_Quit();
    endwin();
    return 0;
}