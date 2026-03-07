#ifndef UIHELPERS_HPP
#define UIHELPERS_HPP

#include <string>
#include <vector>
#include <functional>

// 分页菜单结构
struct PageMenu {
    int current_page = 0;
    int items_per_page = 20;
    int selected_index = 0;
    int total_items = 0;
    
    void update(int total) {
        total_items = total;
        if (selected_index >= total_items) {
            selected_index = total_items > 0 ? total_items - 1 : 0;
        }
        current_page = selected_index / items_per_page;
    }
    
    int getPageCount() const {
        return (total_items + items_per_page - 1) / items_per_page;
    }
    
    int getPageStart() const {
        return current_page * items_per_page;
    }
    
    int getPageEnd() const {
        return std::min(getPageStart() + items_per_page, total_items);
    }
    
    void nextPage() {
        int page_count = getPageCount();
        if (current_page < page_count - 1) {
            current_page++;
            selected_index = getPageStart();
        }
    }
    
    void prevPage() {
        if (current_page > 0) {
            current_page--;
            selected_index = getPageStart();
        }
    }
    
    void moveUp() {
        if (selected_index > getPageStart()) {
            selected_index--;
        } else if (current_page > 0) {
            prevPage();
            selected_index = getPageEnd() - 1;
        }
    }
    
    void moveDown() {
        if (selected_index < getPageEnd() - 1) {
            selected_index++;
        } else if (current_page < getPageCount() - 1) {
            nextPage();
            selected_index = getPageStart();
        }
    }
};

// 帮助信息结构
struct HelpInfo {
    std::string title;
    std::vector<std::pair<std::string, std::string>> commands; // 按键 -> 描述
};

// 通用帮助信息
extern const HelpInfo PLAYING_HELP;
extern const HelpInfo MAIN_MENU_HELP;
extern const HelpInfo PLAYLIST_MANAGER_HELP;
extern const HelpInfo PLAYLIST_MENU_HELP;
extern const HelpInfo PLAYLIST_VIEW_HELP;
extern const HelpInfo SONG_OPERATION_MENU_HELP;
extern const HelpInfo PLAYLIST_EDIT_HELP;
extern const HelpInfo ADD_TO_PLAYLIST_HELP;
extern const HelpInfo SORT_MENU_HELP;
extern const HelpInfo SETTINGS_HELP;
extern const HelpInfo PLAY_MODE_HELP;

// 绘制分页菜单
void drawPageMenu(const std::string& title, const std::vector<std::string>& options, 
                  PageMenu& page_menu, bool show_numbers = true);

// 绘制帮助信息
void drawHelp(const HelpInfo& help);

// 输入字段
std::string inputField(const std::string& prompt);

#endif // UIHELPERS_HPP