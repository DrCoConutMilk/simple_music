#ifndef APP_CONTROLLER_HPP
#define APP_CONTROLLER_HPP

#include <string>
#include <vector>
#include <filesystem>
#include <atomic>
#include <thread>
#include <mutex>
#include "MusicPlayer.hpp"

namespace fs = std::filesystem;

enum class AppState { PLAYING, MAIN_MENU, SETTINGS_MENU, SET_MODE, SET_DIR, PLAYLIST };
enum class PlayMode { SEQUENTIAL, SHUFFLE };

class AppController {
public:
    AppController();
    ~AppController();

    void init();
    void playbackLoop(); // 后台线程函数

    // 动作接口
    void nextSong();
    void prevSong();
    void togglePause();
    void playAtIndex(int index);
    void updateConfigDir(const std::string& new_path);
    void togglePlayMode();

    // 数据获取 (供UI读取)
    AppState state = AppState::PLAYING;
    PlayMode mode = PlayMode::SEQUENTIAL;
    std::string music_dir = "./music";
    std::vector<std::string> current_playlist;
    int currentIndex = 0;

    bool isInitialState = true; // 标志位：是否为刚启动状态
    
    MusicPlayer& getPlayer() { return player; }
    std::mutex& getMutex() { return dataMutex; }
    bool isRunning() { return running; }
    void stop() { running = false; }

private:
    void loadConfig();
    void saveConfig();
    void scanMusic();
    void applyPlayMode();
    fs::path getConfigFilePath();

    MusicPlayer player;
    std::vector<std::string> original_list;
    std::atomic<bool> running{true};
    std::atomic<bool> needLoad{false};
    std::thread playerThread;
    std::mutex dataMutex;
};

#endif