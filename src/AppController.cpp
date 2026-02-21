#include "AppController.hpp"
#include <fstream>
#include <algorithm>
#include <random>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

AppController::AppController() {
    init();
    playerThread = std::thread(&AppController::playbackLoop, this);
}

AppController::~AppController() {
    running = false;
    if (playerThread.joinable()) playerThread.join();
}

void AppController::init() {
    loadConfig();
    scanMusic();
    applyPlayMode();
    if (!current_playlist.empty()) needLoad = true;
}

void AppController::playbackLoop() {
    while (running) {
        std::string path_to_load = "";
        
        {
            // 锁的范围仅限于读取状态，不要包含加载过程
            std::lock_guard<std::mutex> lock(dataMutex);
            
            // 自动切歌判断
            if (!current_playlist.empty() && !player.isPlaying() && !player.isPaused()) {
                currentIndex = (currentIndex + 1) % current_playlist.size();
                needLoad = true;
            }

            if (needLoad && !current_playlist.empty()) {
                path_to_load = current_playlist[currentIndex];
                needLoad = false;
            }
        }

        // 在锁之外执行耗时的加载和播放操作
        if (!path_to_load.empty()) {
            player.load(path_to_load);
            player.play();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// --- 逻辑处理 ---
void AppController::scanMusic() {
    original_list.clear();
    fs::path m_path = music_dir;
    if (!fs::exists(m_path)) return;

    for (const auto& entry : fs::directory_iterator(m_path)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".mp3" || ext == ".flac") {
            original_list.push_back(entry.path().string());
        }
    }
    std::sort(original_list.begin(), original_list.end());
}

void AppController::applyPlayMode() {
    std::string current_song = (current_playlist.empty() || currentIndex >= (int)current_playlist.size()) 
                                ? "" : current_playlist[currentIndex];
    current_playlist = original_list;
    if (current_playlist.empty()) return;

    if (mode == PlayMode::SHUFFLE) {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(current_playlist.begin(), current_playlist.end(), g);
    }
    
    auto it = std::find(current_playlist.begin(), current_playlist.end(), current_song);
    currentIndex = (it != current_playlist.end()) ? (int)std::distance(current_playlist.begin(), it) : 0;
}

// --- 接口实现 ---
void AppController::nextSong() { std::lock_guard<std::mutex> l(dataMutex); currentIndex = (currentIndex + 1) % current_playlist.size(); needLoad = true; }
void AppController::prevSong() { std::lock_guard<std::mutex> l(dataMutex); currentIndex = (currentIndex - 1 + (int)current_playlist.size()) % current_playlist.size(); needLoad = true; }
void AppController::togglePause() { player.isPaused() ? player.resume() : player.pause(); }
void AppController::playAtIndex(int index) { std::lock_guard<std::mutex> l(dataMutex); currentIndex = index; needLoad = true; }

void AppController::updateConfigDir(const std::string& new_path) {
    music_dir = new_path;
    saveConfig();
    scanMusic();
    applyPlayMode();
    needLoad = !current_playlist.empty();
}

void AppController::togglePlayMode() {
    mode = (mode == PlayMode::SEQUENTIAL ? PlayMode::SHUFFLE : PlayMode::SEQUENTIAL);
    saveConfig();
    applyPlayMode();
}

// --- 配置持久化 (省略部分重复的 getConfigFilePath/load/save 代码，逻辑同原main.cpp) ---
fs::path AppController::getConfigFilePath() {
    const char* home_env = std::getenv("HOME");
    fs::path p = home_env ? fs::path(home_env) / ".config" / "simple_music_player" : fs::current_path();
    if (!fs::exists(p)) fs::create_directories(p);
    return p / "config.json";
}

void AppController::saveConfig() {
    json j; j["music_directory"] = music_dir;
    j["play_mode"] = (mode == PlayMode::SHUFFLE ? "shuffle" : "sequential");
    std::ofstream o(getConfigFilePath()); if (o.is_open()) o << j.dump(4);
}

void AppController::loadConfig() {
    fs::path p = getConfigFilePath();
    if (!fs::exists(p)) return;
    std::ifstream i(p);
    try {
        json j = json::parse(i);
        music_dir = j.value("music_directory", "./music");
        mode = (j.value("play_mode", "sequential") == "shuffle" ? PlayMode::SHUFFLE : PlayMode::SEQUENTIAL);
    } catch (...) {}
}