#include "Playlist.hpp"
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/mpegfile.h>
#include <taglib/flacfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/xiphcomment.h>
#include <iostream>
#include <fstream>

void SongEntry::loadMetadata() {
    // 获取文件修改时间
    try {
        auto ftime = fs::last_write_time(path);
        // 使用更兼容的方式处理文件时间
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        modified_time = std::chrono::system_clock::to_time_t(sctp);
    } catch (...) {
        modified_time = 0;
    }
    
    // 获取文件名（备用）
    std::string filename = fs::path(path).filename().string();
    
    // 使用 TagLib 获取元数据
    TagLib::FileRef file(path.c_str());
    if (!file.isNull() && file.tag()) {
        TagLib::Tag* tag = file.tag();
        title = tag->title().toCString(true);
        artist = tag->artist().toCString(true);
        album = tag->album().toCString(true);
        
        // 如果元数据为空，使用文件名
        if (title.empty()) title = filename;
        if (artist.empty()) artist = "未知艺术家";
        if (album.empty()) album = "未知专辑";
    } else {
        // 无法读取元数据，使用文件名
        title = filename;
        artist = "未知艺术家";
        album = "未知专辑";
    }
}

json SongEntry::toJson() const {
    json j;
    j["path"] = path;
    j["title"] = title;
    j["artist"] = artist;
    j["album"] = album;
    j["modified_time"] = modified_time;
    return j;
}

SongEntry SongEntry::fromJson(const json& j) {
    SongEntry song;
    song.path = j.value("path", "");
    song.title = j.value("title", "");
    song.artist = j.value("artist", "");
    song.album = j.value("album", "");
    song.modified_time = j.value("modified_time", 0);
    return song;
}

Playlist::Playlist(const std::string& name) : name(name) {
    auto now = std::chrono::system_clock::now();
    created_time = std::chrono::system_clock::to_time_t(now);
    modified_time = created_time;
}

void Playlist::addSong(const std::string& path) {
    if (containsSong(path)) {
        return; // 避免重复添加
    }
    
    SongEntry song;
    song.path = path;
    song.loadMetadata();
    songs.push_back(song);
    modified_time = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
}

void Playlist::addSong(const SongEntry& song) {
    if (containsSong(song.path)) {
        return; // 避免重复添加
    }
    songs.push_back(song);
    modified_time = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
}

void Playlist::removeSong(int index) {
    if (index >= 0 && index < (int)songs.size()) {
        songs.erase(songs.begin() + index);
        modified_time = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());
    }
}

bool Playlist::containsSong(const std::string& path) const {
    return std::any_of(songs.begin(), songs.end(),
        [&path](const SongEntry& song) { return song.path == path; });
}

void Playlist::sort(SortBy by, SortOrder order) {
    std::sort(songs.begin(), songs.end(), [by, order](const SongEntry& a, const SongEntry& b) {
        bool less_than = false;
        
        switch (by) {
            case SortBy::TITLE:
                less_than = a.title < b.title;
                break;
            case SortBy::ARTIST:
                less_than = a.artist < b.artist;
                break;
            case SortBy::ALBUM:
                less_than = a.album < b.album;
                break;
            case SortBy::FILENAME:
                less_than = fs::path(a.path).filename().string() < fs::path(b.path).filename().string();
                break;
            case SortBy::MODIFIED_TIME:
                less_than = a.modified_time < b.modified_time;
                break;
        }
        
        return (order == SortOrder::ASCENDING) ? less_than : !less_than;
    });
}

json Playlist::toJson() const {
    json j;
    j["name"] = name;
    j["created_time"] = created_time;
    j["modified_time"] = modified_time;
    
    json songs_json = json::array();
    for (const auto& song : songs) {
        songs_json.push_back(song.toJson());
    }
    j["songs"] = songs_json;
    
    return j;
}

Playlist Playlist::fromJson(const json& j) {
    Playlist playlist(j.value("name", "未命名歌单"));
    playlist.created_time = j.value("created_time", 0);
    playlist.modified_time = j.value("modified_time", 0);
    
    if (j.contains("songs") && j["songs"].is_array()) {
        for (const auto& song_json : j["songs"]) {
            playlist.songs.push_back(SongEntry::fromJson(song_json));
        }
    }
    
    return playlist;
}