#ifndef PLAYLIST_HPP
#define PLAYLIST_HPP

#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

enum class SortBy {
    TITLE,
    ARTIST,
    ALBUM,
    FILENAME,
    MODIFIED_TIME
};

enum class SortOrder {
    ASCENDING,
    DESCENDING
};

struct SongEntry {
    std::string path;
    std::string title;
    std::string artist;
    std::string album;
    std::time_t modified_time;
    
    // 从文件路径获取元数据
    void loadMetadata();
    
    // JSON 序列化
    json toJson() const;
    static SongEntry fromJson(const json& j);
};

class Playlist {
public:
    Playlist() = default;
    Playlist(const std::string& name);
    
    // 基本信息
    std::string name;
    std::time_t created_time;
    std::time_t modified_time;
    
    // 歌曲管理
    void addSong(const std::string& path);
    void addSong(const SongEntry& song);
    void removeSong(int index);
    bool containsSong(const std::string& path) const;
    
    // 排序
    void sort(SortBy by, SortOrder order);
    
    // 获取歌曲
    const std::vector<SongEntry>& getSongs() const { return songs; }
    std::vector<SongEntry>& getSongs() { return songs; }
    size_t size() const { return songs.size(); }
    bool empty() const { return songs.empty(); }
    
    // 清空
    void clear() { songs.clear(); }
    
    // JSON 序列化
    json toJson() const;
    static Playlist fromJson(const json& j);
    
private:
    std::vector<SongEntry> songs;
};

#endif // PLAYLIST_HPP