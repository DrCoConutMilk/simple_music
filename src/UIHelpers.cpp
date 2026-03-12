#include "UIHelpers.hpp"
#include <ncurses.h>
#include <clocale>
#include <algorithm>

// 帮助信息定义
const HelpInfo PLAYING_HELP = {
    "播放界面帮助",
    {
        {"空格", "播放/暂停"},
        {"← →", "上一首/下一首"},
        {"[", "快退5秒"},
        {"]", "快进5秒"},
        {"M", "主菜单"},
        {"A", "添加到歌单"},
        {"H", "帮助"},
        {"Q", "退出程序"}
    }
};

const HelpInfo MAIN_MENU_HELP = {
    "主菜单帮助",
    {
        {"↑ ↓", "上下移动"},
        {"PgUp/PgDn", "翻页"},
        {"Enter", "选择"},
        {"H", "帮助"},
        {"Q", "返回播放"}
    }
};

const HelpInfo PLAYLIST_MANAGER_HELP = {
    "歌单管理器帮助",
    {
        {"↑ ↓", "上下移动"},
        {"PgUp/PgDn", "翻页"},
        {"Enter", "选择歌单"},
        {"H", "帮助"},
        {"Q", "返回主菜单"}
    }
};

const HelpInfo PLAYLIST_MENU_HELP = {
    "歌单功能菜单帮助",
    {
        {"↑ ↓", "上下移动"},
        {"PgUp/PgDn", "翻页"},
        {"Enter", "选择功能"},
        {"H", "帮助"},
        {"Q", "返回歌单管理器"}
    }
};

const HelpInfo PLAYLIST_VIEW_HELP = {
    "歌单浏览帮助",
    {
        {"↑ ↓", "上下移动"},
        {"PgUp/PgDn", "翻页"},
        {"Enter", "播放歌曲"},
        {"H", "帮助"},
        {"Q", "返回功能菜单"}
    }
};

const HelpInfo SONG_OPERATION_MENU_HELP = {
    "歌曲操作菜单帮助",
    {
        {"↑ ↓", "上下移动"},
        {"PgUp/PgDn", "翻页"},
        {"Enter", "选择操作"},
        {"H", "帮助"},
        {"Q", "返回歌曲列表"}
    }
};

const HelpInfo PLAYLIST_EDIT_HELP = {
    "创建歌单帮助",
    {
        {"↑ ↓", "上下移动"},
        {"PgUp/PgDn", "翻页"},
        {"Enter", "选择"},
        {"H", "帮助"},
        {"Q", "返回歌单管理器"}
    }
};

const HelpInfo ADD_TO_PLAYLIST_HELP = {
    "添加到歌单帮助",
    {
        {"↑ ↓", "上下移动"},
        {"PgUp/PgDn", "翻页"},
        {"Enter", "添加到歌单"},
        {"H", "帮助"},
        {"Q", "返回播放"}
    }
};

const HelpInfo SORT_MENU_HELP = {
    "排序方式帮助",
    {
        {"↑ ↓", "上下移动"},
        {"PgUp/PgDn", "翻页"},
        {"Enter", "选择排序方式"},
        {"H", "帮助"},
        {"Q", "返回歌单浏览"}
    }
};

const HelpInfo SETTINGS_HELP = {
    "设置帮助",
    {
        {"↑ ↓", "上下移动"},
        {"PgUp/PgDn", "翻页"},
        {"Enter", "选择"},
        {"H", "帮助"},
        {"Q", "返回主菜单"}
    }
};

const HelpInfo PLAY_MODE_HELP = {
    "播放模式帮助",
    {
        {"↑ ↓", "上下移动"},
        {"Enter", "选择"},
        {"H", "帮助"},
        {"Q", "返回设置"}
    }
};

void drawPageMenu(const std::string& title, const std::vector<std::string>& options, 
                  PageMenu& page_menu, bool show_numbers) {
    page_menu.update(options.size());
    
    int start_y = 1;
    mvprintw(start_y, 2, "--- %s ---", title.c_str());
    
    if (options.empty()) {
        mvprintw(start_y + 2, 4, "[列表为空]");
    } else {
        int page_start = page_menu.getPageStart();
        int page_end = page_menu.getPageEnd();
        
        for (int i = page_start; i < page_end; ++i) {
            int display_idx = i - page_start;
            if (i == page_menu.selected_index) {
                attron(A_REVERSE);
            }
            
            std::string display_text;
            if (show_numbers) {
                char buffer[64];
                snprintf(buffer, sizeof(buffer), "%3d. %s", i + 1, options[i].c_str());
                display_text = buffer;
            } else {
                display_text = options[i];
            }
            
            // 截断以适应屏幕宽度
            int max_width = COLS - 10;
            if ((int)display_text.length() > max_width) {
                display_text = display_text.substr(0, max_width - 3) + "...";
            }
            
            mvprintw(start_y + 2 + display_idx, 4, "%s %s", 
                    (i == page_menu.selected_index ? ">" : " "), display_text.c_str());
            
            if (i == page_menu.selected_index) {
                attroff(A_REVERSE);
            }
        }
        
        // 显示页码信息
        int page_count = page_menu.getPageCount();
        if (page_count > 1) {
            mvprintw(start_y + 2 + page_menu.items_per_page + 1, 2, 
                    "第 %d/%d 页 (共 %d 项)", 
                    page_menu.current_page + 1, page_count, page_menu.total_items);
        }
    }
    
    // 底部提示 - 只显示 [H] 帮助
    mvprintw(LINES - 4, 2, "[H]帮助");
}

void drawHelp(const HelpInfo& help) {
    int start_y = 1;
    mvprintw(start_y, 2, "--- %s ---", help.title.c_str());
    
    // 计算最大按键名称长度
    int max_key_len = 0;
    for (const auto& cmd : help.commands) {
        int len = cmd.first.length();
        if (len > max_key_len) {
            max_key_len = len;
        }
    }
    
    // 确保最小宽度为6，最大宽度不超过20
    max_key_len = std::max(6, std::min(max_key_len, 20));
    
    int y = start_y + 2;
    for (const auto& cmd : help.commands) {
        // 使用动态计算的宽度进行对齐
        mvprintw(y, 4, "%-*s : %s", max_key_len, cmd.first.c_str(), cmd.second.c_str());
        y++;
    }
    
    mvprintw(LINES - 4, 2, "按任意键返回...");
}

std::string inputField(const std::string& prompt) {
    // 临时进入阻塞模式进行输入
    echo();
    curs_set(1);
    nodelay(stdscr, FALSE);
    char buf[256];
    erase(); // 清屏，避免菜单干扰输入框显示
    mvprintw(LINES / 2, 4, "%s", prompt.c_str());
    refresh();
    getstr(buf);
    noecho();
    curs_set(0);
    nodelay(stdscr, TRUE);
    
    return std::string(buf);
}