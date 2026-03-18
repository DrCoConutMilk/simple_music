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
    
    // 快进快退接口
    bool seekForward(double seconds = 5.0);
    bool seekBackward(double seconds = 5.0);
    bool seekTo(double position);

    // 状态查询接口
    bool isPaused() const;
    bool isPlaying() const;
    double getElapsedSeconds() const;
    const SongInfo& getCurrentSong() const { return currentSong; }
    std::string getCurrentFilePath() const { return currentFilePath; }
    
    // 音量控制接口
    void setVolume(int volume); // 0-100
    int getVolume() const { return currentVolume; }
    void increaseVolume();      // 增加5%
    void decreaseVolume();      // 减少5%

private:
    void parseLyrics(const std::string& path);
    double parseTime(const std::string& t);
    std::string fetchEmbeddedLyrics(const std::string& path);

    Mix_Music* music = nullptr;
    SongInfo currentSong;
    std::string currentFilePath;
    
    // 进度计算相关变量
    Uint32 startTicks = 0;
    Uint32 pauseTotalTicks = 0;
    Uint32 pauseStartTicks = 0;
    
    // 快进快退频率限制（每0.25秒最多1次）
    Uint32 lastSeekTime = 0;
    
    // 音量控制
    int currentVolume = 80; // 默认音量80%
};

#endif