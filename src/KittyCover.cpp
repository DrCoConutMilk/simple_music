#include "KittyCover.hpp"

#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/id3v2tag.h>
#include <taglib/mpegfile.h>

#include <array>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {
constexpr unsigned image_id = 7391;

std::string base64(const std::vector<unsigned char>& input) {
    static constexpr char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    output.reserve((input.size() + 2) / 3 * 4);
    for (size_t i = 0; i < input.size(); i += 3) {
        unsigned value = static_cast<unsigned>(input[i]) << 16;
        if (i + 1 < input.size()) value |= static_cast<unsigned>(input[i + 1]) << 8;
        if (i + 2 < input.size()) value |= static_cast<unsigned>(input[i + 2]);
        output.push_back(table[(value >> 18) & 63]);
        output.push_back(table[(value >> 12) & 63]);
        output.push_back(i + 1 < input.size() ? table[(value >> 6) & 63] : '=');
        output.push_back(i + 2 < input.size() ? table[value & 63] : '=');
    }
    return output;
}

bool isPng(const std::vector<unsigned char>& data) {
    static constexpr std::array<unsigned char, 8> signature{0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    return data.size() >= signature.size() &&
           std::equal(signature.begin(), signature.end(), data.begin());
}

bool readFile(const fs::path& path, std::vector<unsigned char>& data) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return !data.empty();
}

bool commandExists(const char* command) {
    const char* path = std::getenv("PATH");
    if (!path) return false;
    std::string paths(path);
    size_t start = 0;
    while (start <= paths.size()) {
        const size_t end = paths.find(':', start);
        const std::string directory = paths.substr(start, end == std::string::npos ? end : end - start);
        if (!directory.empty() && ::access((fs::path(directory) / command).c_str(), X_OK) == 0) return true;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return false;
}

std::string shellQuote(const std::string& value) {
    std::string quoted = "'";
    for (char c : value) quoted += c == '\'' ? "'\\''" : std::string(1, c);
    return quoted + "'";
}
}

KittyCover::KittyCover() {
    supported = std::getenv("KITTY_WINDOW_ID") != nullptr ||
                (std::getenv("TERM") && std::string(std::getenv("TERM")).find("kitty") != std::string::npos);
}

KittyCover::~KittyCover() {
    clear();
}

void KittyCover::clear() {
    if (displayed) {
        std::cout << "\033_Ga=d,d=i,i=" << image_id << "\033\\" << std::flush;
    }
    displayed = false;
    displayed_song.clear();
    displayed_cols = 0;
    displayed_lines = 0;
}

void KittyCover::update(bool visible, const std::string& song_path, int terminal_cols, int terminal_lines) {
    const bool fits = terminal_cols >= 70 && terminal_lines >= 24;
    if (!supported || !visible || !fits || song_path.empty()) {
        clear();
        return;
    }
    if (displayed_song == song_path && displayed_cols == terminal_cols &&
        displayed_lines == terminal_lines) return;

    clear();
    std::vector<unsigned char> png;
    if (!loadCover(song_path, png)) {
        displayed_song = song_path;
        displayed_cols = terminal_cols;
        displayed_lines = terminal_lines;
        return;
    }
    display(png);
    displayed = true;
    displayed_song = song_path;
    displayed_cols = terminal_cols;
    displayed_lines = terminal_lines;
}

bool KittyCover::loadCover(const std::string& song_path, std::vector<unsigned char>& png) {
    std::vector<unsigned char> data;
    std::string mime_type;
    if (extractEmbeddedCover(song_path, data, mime_type) && convertToPng(data, mime_type, png)) return true;

    const fs::path directory = fs::path(song_path).parent_path();
    static constexpr const char* names[] = {"cover.png", "Cover.png", "folder.png", "Folder.png",
                                            "cover.jpg", "Cover.jpg", "folder.jpg", "Folder.jpg",
                                            "cover.jpeg", "Cover.jpeg"};
    for (const char* name : names) {
        const fs::path candidate = directory / name;
        if (!readFile(candidate, data)) continue;
        std::string extension = candidate.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        mime_type = extension == ".png" ? "image/png" : "image/jpeg";
        if (convertToPng(data, mime_type, png)) return true;
    }
    return false;
}

bool KittyCover::extractEmbeddedCover(const std::string& song_path,
                                      std::vector<unsigned char>& data,
                                      std::string& mime_type) {
    std::string extension = fs::path(song_path).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (extension == ".mp3") {
        TagLib::MPEG::File file(song_path.c_str());
        if (!file.isValid() || !file.ID3v2Tag()) return false;
        const auto frames = file.ID3v2Tag()->frameList("APIC");
        TagLib::ID3v2::AttachedPictureFrame* selected = nullptr;
        for (auto* frame : frames) {
            auto* picture = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(frame);
            if (!picture) continue;
            if (!selected || picture->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover) selected = picture;
            if (picture->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover) break;
        }
        if (!selected) return false;
        const auto bytes = selected->picture();
        data.assign(bytes.begin(), bytes.end());
        mime_type = selected->mimeType().to8Bit(true);
        return !data.empty();
    }

    if (extension == ".flac") {
        TagLib::FLAC::File file(song_path.c_str());
        if (!file.isValid()) return false;
        const auto pictures = file.pictureList();
        TagLib::FLAC::Picture* selected = nullptr;
        for (auto* picture : pictures) {
            if (!selected || picture->type() == TagLib::FLAC::Picture::FrontCover) selected = picture;
            if (picture->type() == TagLib::FLAC::Picture::FrontCover) break;
        }
        if (!selected) return false;
        const auto bytes = selected->data();
        data.assign(bytes.begin(), bytes.end());
        mime_type = selected->mimeType().to8Bit(true);
        return !data.empty();
    }
    return false;
}

bool KittyCover::convertToPng(const std::vector<unsigned char>& data,
                              const std::string& mime_type,
                              std::vector<unsigned char>& png) {
    if (isPng(data) || mime_type == "image/png") {
        png = data;
        return true;
    }
    if (!commandExists("ffmpeg")) return false;

    char input_template[] = "/tmp/smp-cover-in-XXXXXX";
    char output_template[] = "/tmp/smp-cover-out-XXXXXX";
    const int input_fd = mkstemp(input_template);
    const int output_fd = mkstemp(output_template);
    if (input_fd < 0 || output_fd < 0) {
        if (input_fd >= 0) { close(input_fd); std::remove(input_template); }
        if (output_fd >= 0) { close(output_fd); std::remove(output_template); }
        return false;
    }
    close(output_fd);
    size_t total_written = 0;
    while (total_written < data.size()) {
        const ssize_t written = write(input_fd, data.data() + total_written,
                                      data.size() - total_written);
        if (written <= 0) break;
        total_written += static_cast<size_t>(written);
    }
    close(input_fd);
    if (total_written != data.size()) {
        std::remove(input_template);
        std::remove(output_template);
        return false;
    }

    const std::string command = "ffmpeg -loglevel error -y -i " + shellQuote(input_template) +
                                " -frames:v 1 -f image2 -vcodec png " + shellQuote(output_template);
    const bool converted = std::system(command.c_str()) == 0 && readFile(output_template, png);
    std::remove(input_template);
    std::remove(output_template);
    return converted;
}

void KittyCover::display(const std::vector<unsigned char>& png) {
    const std::string encoded = base64(png);
    constexpr size_t chunk_size = 4096;
    std::cout << "\0337\033[" << row << ';' << column << 'H';
    for (size_t offset = 0; offset < encoded.size(); offset += chunk_size) {
        const bool more = offset + chunk_size < encoded.size();
        std::cout << "\033_G";
        if (offset == 0) {
            std::cout << "a=T,f=100,t=d,i=" << image_id << ",p=" << image_id
                      << ",c=" << width << ",r=" << height << ",q=2,m=" << (more ? 1 : 0) << ';';
        } else {
            std::cout << "m=" << (more ? 1 : 0) << ';';
        }
        std::cout.write(encoded.data() + offset, std::min(chunk_size, encoded.size() - offset));
        std::cout << "\033\\";
    }
    std::cout << "\0338" << std::flush;
}
