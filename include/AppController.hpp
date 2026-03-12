#ifndef APP_CONTROLLER_HPP
#define APP_CONTROLLER_HPP

#include <string>
#include <vector>
#include <filesystem>
#include <atomic>
#include <thread>
#include <mutex>
#include <memory>
#include "MusicPlayer.hpp"
#include "Playlist.hpp"

namespace fs = std::filesystem;

enum class AppState { 
    PLAYING, 
    MAIN_MENU, 
    SETTINGS_MENU, 
    SET_MODE, 
    PLAYLIST_MANAGER, 
    PLAYLIST_MENU,      // 歌单功能菜单
    PLAYLIST_CREATE, 
    PLAYLIST_EDIT, 
    PLAYLIST_VIEW,      // 歌单浏览（查看歌单内容）
    CURRENT_PLAYLIST_VIEW, // 当前播放列表视图
    CURRENT_PLAYLIST_SONG_MENU, // 当前播放列表歌曲操作菜单
    SONG_OPERATION_MENU, // 歌曲操作菜单（歌单浏览）
    PLAYLIST_SORT,
    ADD_TO_PLAYLIST,
    HELP 
};

enum class PlayMode { SEQUENTIAL, SHUFFLE, SINGLE };

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
    void togglePlayMode();

    // 歌单管理
    void createPlaylist(const std::string& name);
    void deletePlaylist(int index);
    void renamePlaylist(int index, const std::string& new_name);
    void addSongsFromDirectory(int playlist_index, const std::string& dir_path);
    void addCurrentSongToPlaylist(int playlist_index);
    void addSongToPlaylist(int playlist_index, const std::string& song_path);
    void removeSongFromPlaylist(int playlist_index, int song_index);
    void sortPlaylist(int playlist_index, SortBy by, SortOrder order);
    
    // 数据获取 (供UI读取)
    AppState state = AppState::PLAYING;
    PlayMode mode = PlayMode::SEQUENTIAL;
    std::vector<std::shared_ptr<Playlist>> playlists;
    int currentPlaylistIndex = -1; // 当前播放的歌单索引
    int currentSongIndex = 0;      // 当前播放的歌曲索引

    // 乱序播放相关
    std::vector<int> shuffleOrder; // 乱序索引映射：shuffleOrder[乱序索引] = 原始索引
    bool needShuffleUpdate = false; // 需要更新乱序列表
    
    // 获取乱序列表（供UI使用）
    const std::vector<int>& getShuffleOrder() const { return shuffleOrder; }
    
    MusicPlayer& getPlayer() { return player; }
    std::mutex& getMutex() { return dataMutex; }
    bool isRunning() { return running; }
    void stop() { running = false; }
    void requestLoad() { needLoad = true; }
    
    // 获取当前播放模式
    PlayMode getPlayMode() const { return mode; }
    
    // 获取当前播放列表（用于兼容旧代码）
    std::vector<std::string> getCurrentPlaylistPaths() const;
    
    // 获取当前歌曲路径
    std::string getCurrentSongPath() const;
    
    // 获取当前播放的歌曲（考虑乱序模式）
    const SongEntry& getCurrentSong() const;
    
    // 获取当前播放列表的歌曲数量（考虑乱序模式）
    int getCurrentPlaylistSize() const;
    
    // 获取歌曲（考虑乱序模式）
    const SongEntry& getSongAt(int index) const;
    
    // 根据歌曲路径查找索引（考虑乱序模式）
    int findSongIndexByPath(const std::string& path) const;
    
private:
    // 生成乱序播放列表
    void generateShuffleOrder();

private:
    void loadConfig();
    void saveConfig();
    void loadPlaylists();
    void savePlaylist(int index);
    void deletePlaylistFile(int index);
    void scanDirectoryForSongs(const std::string& dir_path, std::vector<std::string>& result);
    fs::path getConfigFilePath();
    fs::path getPlaylistsDir();
    fs::path getPlaylistFilePath(int index);

    MusicPlayer player;
    std::atomic<bool> running{true};
    std::atomic<bool> needLoad{false};
    std::atomic<bool> isStartingUp{true}; // 是否为启动状态
    std::thread playerThread;
    std::mutex dataMutex;
};

#endif