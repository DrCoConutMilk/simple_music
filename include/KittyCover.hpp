#ifndef KITTY_COVER_HPP
#define KITTY_COVER_HPP

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class KittyCover {
public:
    KittyCover();
    ~KittyCover();

    bool isSupported() const { return supported; }
    void update(bool visible, const std::string& song_path, int terminal_cols, int terminal_lines);
    void clear();

    static constexpr int column = 2;
    static constexpr int row = 3;
    static constexpr int width = 24;
    static constexpr int height = 12;

private:
    void loaderLoop();
    void requestCover(const std::string& song_path);
    bool loadCover(const std::string& song_path, std::vector<unsigned char>& png);
    bool extractEmbeddedCover(const std::string& song_path,
                              std::vector<unsigned char>& data,
                              std::string& mime_type);
    bool convertToPng(const std::vector<unsigned char>& data,
                      const std::string& mime_type,
                      std::vector<unsigned char>& png);
    void display(const std::vector<unsigned char>& png);

    bool supported = false;
    bool displayed = false;
    std::string displayed_song;
    std::vector<unsigned char> displayed_png;
    int displayed_cols = 0;
    int displayed_lines = 0;

    std::thread loader_thread;
    std::mutex loader_mutex;
    std::condition_variable loader_cv;
    bool loader_running = true;
    unsigned long long request_generation = 0;
    unsigned long long completed_generation = 0;
    std::string requested_song;
    std::string completed_song;
    std::vector<unsigned char> completed_png;
    bool completed_success = false;
};

#endif
