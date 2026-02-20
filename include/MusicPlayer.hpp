#ifndef MUSIC_PLAYER_HPP
#define MUSIC_PLAYER_HPP

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <string>
#include <vector>
#include <algorithm>

struct LyricLine {
    double timestamp;
    std::string text;
};

struct SongInfo {
    std::string title;
    std::string artist;
    int duration = 0;
    std::vector<LyricLine> lyrics;
};

class MusicPlayer {
public:
    MusicPlayer();
    ~MusicPlayer();

    // 播放控制接口
    bool load(const std::string& path);
    void play();
    void pause();
    void resume();
    void stop();

    // 状态查询接口
    bool isPaused() const;
    bool isPlaying() const;
    double getElapsedSeconds() const;
    const SongInfo& getCurrentSong() const { return currentSong; }

private:
    void parseLyrics(const std::string& path);
    double parseTime(const std::string& t);
    std::string fetchEmbeddedLyrics(const std::string& path);

    Mix_Music* music = nullptr;
    SongInfo currentSong;
    
    // 进度计算相关变量
    Uint32 startTicks = 0;
    Uint32 pauseTotalTicks = 0;
    Uint32 pauseStartTicks = 0;
};

#endif