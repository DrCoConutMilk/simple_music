#include "MusicPlayer.hpp"
#include <taglib/fileref.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/flacfile.h>
#include <taglib/xiphcomment.h>
#include <sstream>
#include <iostream>

MusicPlayer::MusicPlayer() {
    if (SDL_Init(SDL_INIT_AUDIO) < 0) return;
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
}

MusicPlayer::~MusicPlayer() {
    stop();
    Mix_CloseAudio();
    SDL_Quit();
}

bool MusicPlayer::load(const std::string& path) {
    stop(); // 加载新歌前先停止并释放旧资源
    music = Mix_LoadMUS(path.c_str());
    if (!music) return false;

    // 1. 解析元数据
    TagLib::FileRef f(path.c_str());
    if (!f.isNull() && f.tag()) {
        currentSong.title = f.tag()->title().to8Bit(true);
        currentSong.artist = f.tag()->artist().to8Bit(true);
        if (f.audioProperties()) {
            currentSong.duration = f.audioProperties()->length();
        }
    }
    
    // 2. 解析歌词
    parseLyrics(path);
    return true;
}

void MusicPlayer::play() {
    if (music) {
        Mix_PlayMusic(music, 1);
        startTicks = SDL_GetTicks();
        pauseTotalTicks = 0;
    }
}

void MusicPlayer::pause() {
    if (music && !Mix_PausedMusic()) {
        Mix_PauseMusic();
        pauseStartTicks = SDL_GetTicks();
    }
}

void MusicPlayer::resume() {
    if (music && Mix_PausedMusic()) {
        Mix_ResumeMusic();
        pauseTotalTicks += (SDL_GetTicks() - pauseStartTicks);
    }
}

void MusicPlayer::stop() {
    if (music) {
        Mix_HaltMusic();
        Mix_FreeMusic(music);
        music = nullptr;
    }
    currentSong = SongInfo(); // 重置当前歌曲信息
}

bool MusicPlayer::isPaused() const { return Mix_PausedMusic(); }

bool MusicPlayer::isPlaying() const { 
    // Mix_PlayingMusic 在暂停时也会返回真，所以这样判断更准确
    return Mix_PlayingMusic() != 0; 
}

double MusicPlayer::getElapsedSeconds() const {
    if (!isPlaying() && !isPaused()) return 0;
    Uint32 current = isPaused() ? pauseStartTicks : SDL_GetTicks();
    return (current - startTicks - pauseTotalTicks) / 1000.0;
}

std::string MusicPlayer::fetchEmbeddedLyrics(const std::string& path) {
    // 尝试解析 MP3 ID3v2
    TagLib::MPEG::File m(path.c_str());
    if (m.isValid() && m.ID3v2Tag()) {
        auto f = m.ID3v2Tag()->frameList("USLT");
        if (!f.isEmpty()) return f.front()->toString().to8Bit(true);
    }
    // 尝试解析 FLAC XiphComment
    TagLib::FLAC::File fl(path.c_str());
    if (fl.isValid() && fl.xiphComment()) {
        auto d = fl.xiphComment()->fieldListMap();
        if (d.contains("LYRICS")) return d["LYRICS"].front().to8Bit(true);
    }
    return "";
}

double MusicPlayer::parseTime(const std::string& t) {
    if (t.size() < 7 || t[0] != '[') return -1;
    try {
        // 支持 [mm:ss.xx] 格式
        return std::stoi(t.substr(1,2)) * 60 + std::stod(t.substr(4, t.find(']') - 4));
    } catch(...) { return -1; }
}

void MusicPlayer::parseLyrics(const std::string& path) {
    currentSong.lyrics.clear();
    std::string raw = fetchEmbeddedLyrics(path);
    if (raw.empty()) return;

    std::stringstream ss(raw);
    std::string line;
    while(std::getline(ss, line)) {
        size_t closePos = line.find(']');
        if(closePos != std::string::npos && line.size() > closePos + 1) {
            double ts = parseTime(line.substr(0, closePos + 1));
            if (ts >= 0) {
                currentSong.lyrics.push_back({ts, line.substr(closePos + 1)});
            }
        }
    }
    // 排序确保 UI 逻辑正常
    std::sort(currentSong.lyrics.begin(), currentSong.lyrics.end(), 
              [](const LyricLine& a, const LyricLine& b) { return a.timestamp < b.timestamp; });
}