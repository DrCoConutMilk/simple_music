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
    bool use_dynamic_paging = false;  // 是否使用动态分页
    int dynamic_items_per_page = 20;  // 动态计算的每页项目数
    
    void update(int total) {
        total_items = total;
        if (selected_index >= total_items) {
            selected_index = total_items > 0 ? total_items - 1 : 0;
        }
        // 根据是否使用动态分页来计算当前页面
        if (use_dynamic_paging) {
            current_page = selected_index / dynamic_items_per_page;
        } else {
            current_page = selected_index / items_per_page;
        }
    }
    
    // 设置动态分页
    void setDynamicPaging(bool use_dynamic, int dynamic_items = 20) {
        use_dynamic_paging = use_dynamic;
        if (use_dynamic) {
            dynamic_items_per_page = dynamic_items;
        }
    }
    
    // 获取当前使用的每页项目数
    int getEffectiveItemsPerPage() const {
        return use_dynamic_paging ? dynamic_items_per_page : items_per_page;
    }
    
    int getPageCount() const {
        int effective_items = getEffectiveItemsPerPage();
        return (total_items + effective_items - 1) / effective_items;
    }
    
    int getPageStart() const {
        int effective_items = getEffectiveItemsPerPage();
        return current_page * effective_items;
    }
    
    int getPageEnd() const {
        int effective_items = getEffectiveItemsPerPage();
        return std::min(getPageStart() + effective_items, total_items);
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