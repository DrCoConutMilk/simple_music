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
    // 程序退出时保存配置
    saveConfig();
}

void AppController::init() {
    loadConfig();
    loadPlaylists();
    
    // 如果当前是乱序模式且有播放列表，生成乱序列表
    if (mode == PlayMode::SHUFFLE && currentPlaylistIndex >= 0 && 
        currentPlaylistIndex < (int)playlists.size()) {
        auto& playlist = playlists[currentPlaylistIndex];
        if (!playlist->empty()) {
            generateShuffleOrder();
            
            // 确保currentSongIndex在乱序列表的有效范围内
            if (currentSongIndex < 0 || currentSongIndex >= (int)shuffleOrder.size()) {
                currentSongIndex = 0;
            }
        }
    }
    
    // 不再创建默认歌单，用户需要手动创建
    if (currentPlaylistIndex >= 0 && currentPlaylistIndex < (int)playlists.size()) {
        needLoad = true;
        isStartingUp = true; // 设置为启动状态
    }
}

void AppController::playbackLoop() {
    while (running) {
        std::string path_to_load = "";
        
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            
            // 如果需要更新乱序列表（比如刚切换到乱序模式）
            if (needShuffleUpdate) {
                if (mode == PlayMode::SHUFFLE && currentPlaylistIndex >= 0 && 
                    currentPlaylistIndex < (int)playlists.size()) {
                    auto& playlist = playlists[currentPlaylistIndex];
                    if (!playlist->empty() && shuffleOrder.empty()) {
                        generateShuffleOrder();
                    }
                }
                needShuffleUpdate = false;
            }
            
            if (currentPlaylistIndex >= 0 && currentPlaylistIndex < (int)playlists.size()) {
                auto& playlist = playlists[currentPlaylistIndex];
                if (!playlist->empty()) {
                    // 自动切歌判断逻辑
                    if (needLoad) {
                        // 需要加载歌曲（启动时恢复播放或手动切歌）
                        path_to_load = getCurrentSong().path;
                        needLoad = false;
                        // 如果是启动状态，现在应该结束了
                        if (isStartingUp) {
                            isStartingUp = false;
                        }
                    } else if (!player.isPlaying() && !player.isPaused() && !isStartingUp) {
                        // 歌曲播放完毕，根据播放模式自动处理
                        if (mode == PlayMode::SINGLE) {
                            // 单曲循环模式：重新播放当前歌曲
                            path_to_load = getCurrentSong().path;
                        } else {
                            // 顺序或乱序模式：切到下一首
                            currentSongIndex = (currentSongIndex + 1) % playlist->size();
                            path_to_load = getCurrentSong().path;
                        }
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

void AppController::seekForward() {
    player.seekForward();
}

void AppController::seekBackward() {
    player.seekBackward();
}

void AppController::playAtIndex(int index) { 
    std::lock_guard<std::mutex> l(dataMutex); 
    currentSongIndex = index; 
    needLoad = true; 
}

void AppController::togglePlayMode() {
    std::lock_guard<std::mutex> lock(dataMutex);
    
    // 记录切换前的模式
    PlayMode old_mode = mode;
    
    if (currentPlaylistIndex >= 0 && currentPlaylistIndex < (int)playlists.size()) {
        auto& playlist = playlists[currentPlaylistIndex];
        if (!playlist->empty()) {
            // 在切换模式前获取当前歌曲路径（使用旧模式）
            std::string current_song_path;
            if (old_mode == PlayMode::SEQUENTIAL) {
                // 顺序模式：直接使用currentSongIndex
                if (currentSongIndex >= 0 && currentSongIndex < (int)playlist->size()) {
                    current_song_path = playlist->getSongs()[currentSongIndex].path;
                }
            } else {
                // 乱序模式：需要通过shuffleOrder映射
                if (!shuffleOrder.empty() && 
                    currentSongIndex >= 0 && currentSongIndex < (int)shuffleOrder.size()) {
                    int original_index = shuffleOrder[currentSongIndex];
                    if (original_index >= 0 && original_index < (int)playlist->size()) {
                        current_song_path = playlist->getSongs()[original_index].path;
                    }
                }
            }
            
            // 如果没获取到歌曲路径，使用第一首
            if (current_song_path.empty() && !playlist->empty()) {
                current_song_path = playlist->getSongs()[0].path;
            }
            
            // 现在切换模式：顺序 -> 乱序 -> 单曲循环 -> 顺序
            if (old_mode == PlayMode::SEQUENTIAL) {
                mode = PlayMode::SHUFFLE;
            } else if (old_mode == PlayMode::SHUFFLE) {
                mode = PlayMode::SINGLE;
            } else {
                mode = PlayMode::SEQUENTIAL;
            }
            
            if (mode == PlayMode::SHUFFLE) {
                // 切换到乱序模式：生成乱序列表
                generateShuffleOrder();
                
                // 在乱序列表中查找当前歌曲
                int found_index = -1;
                for (size_t i = 0; i < shuffleOrder.size(); ++i) {
                    int original_index = shuffleOrder[i];
                    if (original_index >= 0 && original_index < (int)playlist->size()) {
                        if (playlist->getSongs()[original_index].path == current_song_path) {
                            found_index = i;
                            break;
                        }
                    }
                }
                
                if (found_index >= 0) {
                    currentSongIndex = found_index;
                } else {
                    // 如果没找到，从第一首开始
                    currentSongIndex = 0;
                }
            } else if (mode == PlayMode::SEQUENTIAL) {
                // 切换到顺序模式
                if (old_mode == PlayMode::SHUFFLE && !shuffleOrder.empty()) {
                    // 从乱序模式切换过来，需要找到当前歌曲在原始列表中的位置
                    if (currentSongIndex >= 0 && currentSongIndex < (int)shuffleOrder.size()) {
                        currentSongIndex = shuffleOrder[currentSongIndex];
                    }
                }
                // 清空乱序列表
                shuffleOrder.clear();
            } else {
                // 切换到单曲循环模式
                // 单曲循环模式不需要特殊处理，保持当前状态
                // 如果是从乱序模式切换过来，需要处理索引
                if (old_mode == PlayMode::SHUFFLE && !shuffleOrder.empty()) {
                    // 从乱序模式切换过来，需要找到当前歌曲在原始列表中的位置
                    if (currentSongIndex >= 0 && currentSongIndex < (int)shuffleOrder.size()) {
                        currentSongIndex = shuffleOrder[currentSongIndex];
                    }
                }
                // 清空乱序列表（单曲循环不需要乱序列表）
                shuffleOrder.clear();
            }
        }
    }
    
    saveConfig();
    needShuffleUpdate = true;
}

// 生成乱序播放列表
void AppController::generateShuffleOrder() {
    shuffleOrder.clear();
    if (currentPlaylistIndex >= 0 && currentPlaylistIndex < (int)playlists.size()) {
        auto& playlist = playlists[currentPlaylistIndex];
        int size = playlist->size();
        
        // 创建顺序索引
        for (int i = 0; i < size; ++i) {
            shuffleOrder.push_back(i);
        }
        
        // 打乱索引（使用随机种子）
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(shuffleOrder.begin(), shuffleOrder.end(), g);
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
            // 查重：如果歌曲已经在歌单中，不重复添加
            if (!playlist->containsSong(current_path)) {
                playlist->addSong(current_path);
                savePlaylist(playlist_index);
            }
        }
    }
}

void AppController::addSongToPlaylist(int playlist_index, const std::string& song_path) {
    std::lock_guard<std::mutex> lock(dataMutex);
    if (playlist_index >= 0 && playlist_index < (int)playlists.size()) {
        auto& playlist = playlists[playlist_index];
        // 查重：如果歌曲已经在歌单中，不重复添加
        if (!playlist->containsSong(song_path)) {
            playlist->addSong(song_path);
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
    return getCurrentSong().path;
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
    // 保存播放模式
    std::string mode_str;
    switch (mode) {
        case PlayMode::SEQUENTIAL:
            mode_str = "sequential";
            break;
        case PlayMode::SHUFFLE:
            mode_str = "shuffle";
            break;
        case PlayMode::SINGLE:
            mode_str = "single";
            break;
    }
    j["play_mode"] = mode_str;
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
        // 加载播放模式
        std::string mode_str = j.value("play_mode", "sequential");
        if (mode_str == "shuffle") {
            mode = PlayMode::SHUFFLE;
        } else if (mode_str == "single") {
            mode = PlayMode::SINGLE;
        } else {
            mode = PlayMode::SEQUENTIAL; // 默认值
        }
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

// --- 乱序播放相关函数实现 ---
const SongEntry& AppController::getCurrentSong() const {
    static SongEntry empty_song;
    if (currentPlaylistIndex >= 0 && currentPlaylistIndex < (int)playlists.size()) {
        auto& playlist = playlists[currentPlaylistIndex];
        if (!playlist->empty()) {
            int actual_index = currentSongIndex;
            if (mode == PlayMode::SHUFFLE && !shuffleOrder.empty()) {
                // 在乱序模式下，需要映射到原始索引
                if (currentSongIndex >= 0 && currentSongIndex < (int)shuffleOrder.size()) {
                    actual_index = shuffleOrder[currentSongIndex];
                }
            }
            if (actual_index >= 0 && actual_index < (int)playlist->size()) {
                return playlist->getSongs()[actual_index];
            }
        }
    }
    return empty_song;
}

int AppController::getCurrentPlaylistSize() const {
    if (currentPlaylistIndex >= 0 && currentPlaylistIndex < (int)playlists.size()) {
        return playlists[currentPlaylistIndex]->size();
    }
    return 0;
}

const SongEntry& AppController::getSongAt(int index) const {
    static SongEntry empty_song;
    if (currentPlaylistIndex >= 0 && currentPlaylistIndex < (int)playlists.size()) {
        auto& playlist = playlists[currentPlaylistIndex];
        if (!playlist->empty()) {
            int actual_index = index;
            if (mode == PlayMode::SHUFFLE && !shuffleOrder.empty()) {
                // 在乱序模式下，需要映射到原始索引
                if (index >= 0 && index < (int)shuffleOrder.size()) {
                    actual_index = shuffleOrder[index];
                }
            }
            if (actual_index >= 0 && actual_index < (int)playlist->size()) {
                return playlist->getSongs()[actual_index];
            }
        }
    }
    return empty_song;
}

int AppController::findSongIndexByPath(const std::string& path) const {
    if (currentPlaylistIndex >= 0 && currentPlaylistIndex < (int)playlists.size()) {
        auto& playlist = playlists[currentPlaylistIndex];
        const auto& songs = playlist->getSongs();
        
        // 首先在原始歌单中查找
        for (size_t i = 0; i < songs.size(); ++i) {
            if (songs[i].path == path) {
                // 找到原始索引，如果需要返回乱序索引
                if (mode == PlayMode::SHUFFLE && !shuffleOrder.empty()) {
                    // 在乱序列表中查找这个原始索引
                    for (size_t j = 0; j < shuffleOrder.size(); ++j) {
                        if (shuffleOrder[j] == (int)i) {
                            return j;
                        }
                    }
                }
                return i;
            }
        }
    }
    return -1;
}