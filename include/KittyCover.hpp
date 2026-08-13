#ifndef KITTY_COVER_HPP
#define KITTY_COVER_HPP

#include <string>
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
    int displayed_cols = 0;
    int displayed_lines = 0;
};

#endif
