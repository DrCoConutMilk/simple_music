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
    loadPlaylists();
    
    // 不再创建默认歌单，用户需要手动创建
    if (currentPlaylistIndex >= 0 && currentPlaylistIndex < (int)playlists.size()) {
        needLoad = true;
    }
}

void AppController::playbackLoop() {
    while (running) {
        std::string path_to_load = "";
        
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            
            if (currentPlaylistIndex >= 0 && currentPlaylistIndex < (int)playlists.size()) {
                auto& playlist = playlists[currentPlaylistIndex];
                if (!playlist->empty()) {
                    // 自动切歌判断逻辑
                    // 如果当前没在播，且没暂停
                    if (!player.isPlaying() && !player.isPaused()) {
                        
                        if (isInitialState) {
                            // 启动后的第一次：直接加载当前索引(0)，不自增
                            needLoad = true;
                            isInitialState = false; 
                        } else {
                            // 之后的情况：说明上一首歌真的播完了，切下一首
                            currentSongIndex = (currentSongIndex + 1) % playlist->size();
                            needLoad = true;
                        }
                    }

                    if (needLoad) {
                        path_to_load = playlist->getSongs()[currentSongIndex].path;
                        needLoad = false;
                        // 如果是手动触发的加载，也要关闭初始状态标记
                        isInitialState = false; 
                    }
                }
            }
        }

        if (!path_to_load.empty()) {
            player.load(path_to_load);
            player.play();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// --- 目录扫描 ---
void AppController::scanDirectoryForSongs(const std::string& dir_path, std::vector<std::string>& result) {
    result.clear();
    fs::path m_path = dir_path;
    if (!fs::exists(m_path)) return;

    for (const auto& entry : fs::directory_iterator(m_path)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".mp3" || ext == ".flac") {
            result.push_back(entry.path().string());
        }
    }
    std::sort(result.begin(), result.end());
}

// --- 接口实现 ---
void AppController::nextSong() { 
    std::lock_guard<std::mutex> l(dataMutex); 
    if (currentPlaylistIndex >= 0 && currentPlaylistIndex < (int)playlists.size()) {
        auto& playlist = playlists[currentPlaylistIndex];
        if (!playlist->empty()) {
            currentSongIndex = (currentSongIndex + 1) % playlist->size();
            needLoad = true;
        }
    }
}

void AppController::prevSong() { 
    std::lock_guard<std::mutex> l(dataMutex); 
    if (currentPlaylistIndex >= 0 && currentPlaylistIndex < (int)playlists.size()) {
        auto& playlist = playlists[currentPlaylistIndex];
        if (!playlist->empty()) {
            currentSongIndex = (currentSongIndex - 1 + playlist->size()) % playlist->size();
            needLoad = true;
        }
    }
}

void AppController::togglePause() { player.isPaused() ? player.resume() : player.pause(); }

void AppController::playAtIndex(int index) { 
    std::lock_guard<std::mutex> l(dataMutex); 
    currentSongIndex = index; 
    needLoad = true; 
}

void AppController::togglePlayMode() {
    mode = (mode == PlayMode::SEQUENTIAL ? PlayMode::SHUFFLE : PlayMode::SEQUENTIAL);
    saveConfig();
    // 如果当前有播放列表，重新应用播放模式
    if (currentPlaylistIndex >= 0 && currentPlaylistIndex < (int)playlists.size()) {
        auto& playlist = playlists[currentPlaylistIndex];
        if (mode == PlayMode::SHUFFLE) {
            // 保存当前歌曲
            std::string current_song = playlist->empty() ? "" : playlist->getSongs()[currentSongIndex].path;
            
            // 创建临时副本并打乱
            auto songs = playlist->getSongs();
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(songs.begin(), songs.end(), g);
            
            // 更新歌单
            playlist->clear();
            for (const auto& song : songs) {
                playlist->addSong(song);
            }
            
            // 找到当前歌曲的新位置
            auto it = std::find_if(songs.begin(), songs.end(), 
                [&current_song](const SongEntry& song) { return song.path == current_song; });
            if (it != songs.end()) {
                currentSongIndex = std::distance(songs.begin(), it);
            }
        }
    }
}

// --- 歌单管理 ---
void AppController::createPlaylist(const std::string& name) {
    std::lock_guard<std::mutex> lock(dataMutex);
    auto new_playlist = std::make_shared<Playlist>(name);
    playlists.push_back(new_playlist);
    savePlaylist(playlists.size() - 1);
}

void AppController::deletePlaylist(int index) {
    std::lock_guard<std::mutex> lock(dataMutex);
    if (index >= 0 && index < (int)playlists.size()) {
        // 删除对应的文件
        deletePlaylistFile(index);
        
        // 从内存中删除
        playlists.erase(playlists.begin() + index);
        
        // 更新当前播放索引
        if (currentPlaylistIndex == index) {
            currentPlaylistIndex = -1;
            currentSongIndex = 0;
        } else if (currentPlaylistIndex > index) {
            currentPlaylistIndex--;
        }
        
        // 保存配置（更新元信息）
        saveConfig();
    }
}

void AppController::renamePlaylist(int index, const std::string& new_name) {
    std::lock_guard<std::mutex> lock(dataMutex);
    if (index >= 0 && index < (int)playlists.size()) {
        playlists[index]->name = new_name;
        savePlaylist(index);
    }
}

void AppController::addSongsFromDirectory(int playlist_index, const std::string& dir_path) {
    std::lock_guard<std::mutex> lock(dataMutex);
    if (playlist_index >= 0 && playlist_index < (int)playlists.size()) {
        std::vector<std::string> songs;
        scanDirectoryForSongs(dir_path, songs);
        for (const auto& song_path : songs) {
            playlists[playlist_index]->addSong(song_path);
        }
        savePlaylist(playlist_index);
    }
}

void AppController::addCurrentSongToPlaylist(int playlist_index) {
    std::lock_guard<std::mutex> lock(dataMutex);
    if (playlist_index >= 0 && playlist_index < (int)playlists.size()) {
        auto& playlist = playlists[playlist_index];
        std::string current_path = player.getCurrentFilePath();
        if (!current_path.empty()) {
            playlist->addSong(current_path);
            savePlaylist(playlist_index);
        }
    }
}

void AppController::removeSongFromPlaylist(int playlist_index, int song_index) {
    std::lock_guard<std::mutex> lock(dataMutex);
    if (playlist_index >= 0 && playlist_index < (int)playlists.size()) {
        auto& playlist = playlists[playlist_index];
        playlist->removeSong(song_index);
        if (currentPlaylistIndex == playlist_index) {
            if (song_index == currentSongIndex) {
                // 删除的是当前播放的歌曲
                if (playlist->empty()) {
                    currentSongIndex = 0;
                    needLoad = false;
                } else if (currentSongIndex >= (int)playlist->size()) {
                    currentSongIndex = playlist->size() - 1;
                }
            } else if (song_index < currentSongIndex) {
                currentSongIndex--;
            }
        }
        savePlaylist(playlist_index);
    }
}

void AppController::sortPlaylist(int playlist_index, SortBy by, SortOrder order) {
    std::lock_guard<std::mutex> lock(dataMutex);
    if (playlist_index >= 0 && playlist_index < (int)playlists.size()) {
        auto& playlist = playlists[playlist_index];
        std::string current_song = playlist->empty() ? "" : playlist->getSongs()[currentSongIndex].path;
        
        playlist->sort(by, order);
        
        // 更新当前歌曲索引
        if (!current_song.empty()) {
            const auto& songs = playlist->getSongs();
            auto it = std::find_if(songs.begin(), songs.end(),
                [&current_song](const SongEntry& song) { return song.path == current_song; });
            if (it != songs.end()) {
                currentSongIndex = std::distance(songs.begin(), it);
            }
        }
        savePlaylist(playlist_index);
    }
}

std::vector<std::string> AppController::getCurrentPlaylistPaths() const {
    std::vector<std::string> result;
    if (currentPlaylistIndex >= 0 && currentPlaylistIndex < (int)playlists.size()) {
        const auto& playlist = playlists[currentPlaylistIndex];
        for (const auto& song : playlist->getSongs()) {
            result.push_back(song.path);
        }
    }
    return result;
}

std::string AppController::getCurrentSongPath() const {
    if (currentPlaylistIndex >= 0 && currentPlaylistIndex < (int)playlists.size()) {
        const auto& playlist = playlists[currentPlaylistIndex];
        if (currentSongIndex >= 0 && currentSongIndex < (int)playlist->size()) {
            return playlist->getSongs()[currentSongIndex].path;
        }
    }
    return "";
}

// --- 文件路径相关 ---
fs::path AppController::getConfigFilePath() {
    const char* home_env = std::getenv("HOME");
    fs::path p = home_env ? fs::path(home_env) / ".config" / "simple_music_player" : fs::current_path();
    if (!fs::exists(p)) fs::create_directories(p);
    return p / "config.json";
}

fs::path AppController::getPlaylistsDir() {
    fs::path config_dir = getConfigFilePath().parent_path();
    fs::path playlists_dir = config_dir / "song_lists";
    if (!fs::exists(playlists_dir)) {
        fs::create_directories(playlists_dir);
    }
    return playlists_dir;
}

fs::path AppController::getPlaylistFilePath(int index) {
    if (index < 0 || index >= (int)playlists.size()) {
        return fs::path();
    }
    std::string filename = "playlist_" + std::to_string(index) + ".json";
    return getPlaylistsDir() / filename;
}

// --- 配置持久化 ---
void AppController::saveConfig() {
    json j;
    j["play_mode"] = (mode == PlayMode::SHUFFLE ? "shuffle" : "sequential");
    j["current_playlist_index"] = currentPlaylistIndex;
    j["current_song_index"] = currentSongIndex;
    
    // 只保存歌单的元信息（名称、索引映射）
    json playlists_meta = json::array();
    for (size_t i = 0; i < playlists.size(); ++i) {
        json meta;
        meta["index"] = i;
        meta["name"] = playlists[i]->name;
        meta["created_time"] = playlists[i]->created_time;
        meta["modified_time"] = playlists[i]->modified_time;
        playlists_meta.push_back(meta);
    }
    j["playlists_meta"] = playlists_meta;
    
    std::ofstream o(getConfigFilePath());
    if (o.is_open()) o << j.dump(4);
}

void AppController::loadConfig() {
    fs::path p = getConfigFilePath();
    if (!fs::exists(p)) return;
    std::ifstream i(p);
    try {
        json j = json::parse(i);
        mode = (j.value("play_mode", "sequential") == "shuffle" ? PlayMode::SHUFFLE : PlayMode::SEQUENTIAL);
        currentPlaylistIndex = j.value("current_playlist_index", -1);
        currentSongIndex = j.value("current_song_index", 0);
        
        // 注意：这里不加载歌单内容，只加载元信息
        // 歌单内容在 loadPlaylists() 中单独加载
    } catch (...) {}
}

void AppController::loadPlaylists() {
    playlists.clear();
    
    // 首先加载配置文件中的歌单元信息
    fs::path config_file = getConfigFilePath();
    if (!fs::exists(config_file)) return;
    
    std::ifstream i(config_file);
    try {
        json j = json::parse(i);
        if (j.contains("playlists_meta") && j["playlists_meta"].is_array()) {
            for (const auto& meta : j["playlists_meta"]) {
                int index = meta.value("index", -1);
                std::string name = meta.value("name", "未命名歌单");
                
                // 创建歌单对象
                auto playlist = std::make_shared<Playlist>(name);
                playlist->created_time = meta.value("created_time", 0);
                playlist->modified_time = meta.value("modified_time", 0);
                
                // 尝试从单独的文件加载歌曲
                fs::path playlist_file = getPlaylistsDir() / ("playlist_" + std::to_string(index) + ".json");
                if (fs::exists(playlist_file)) {
                    std::ifstream pf(playlist_file);
                    try {
                        json playlist_json = json::parse(pf);
                        *playlist = Playlist::fromJson(playlist_json);
                    } catch (...) {
                        // 如果文件损坏，至少保留歌单名称
                    }
                }
                
                playlists.push_back(playlist);
            }
        }
    } catch (...) {}
}

void AppController::savePlaylist(int index) {
    if (index < 0 || index >= (int)playlists.size()) {
        return;
    }
    
    fs::path playlist_file = getPlaylistsDir() / ("playlist_" + std::to_string(index) + ".json");
    std::ofstream o(playlist_file);
    if (o.is_open()) {
        o << playlists[index]->toJson().dump(4);
    }
    
    // 更新配置文件中的元信息
    saveConfig();
}

void AppController::deletePlaylistFile(int index) {
    fs::path playlist_file = getPlaylistsDir() / ("playlist_" + std::to_string(index) + ".json");
    if (fs::exists(playlist_file)) {
        fs::remove(playlist_file);
    }
    
    // 重命名后面的歌单文件
    for (int i = index + 1; i < (int)playlists.size() + 1; ++i) {
        fs::path old_file = getPlaylistsDir() / ("playlist_" + std::to_string(i) + ".json");
        fs::path new_file = getPlaylistsDir() / ("playlist_" + std::to_string(i - 1) + ".json");
        if (fs::exists(old_file)) {
            fs::rename(old_file, new_file);
        }
    }
}