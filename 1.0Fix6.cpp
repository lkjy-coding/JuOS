#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <functional>
#include <chrono>
#include <ctime>
#include <thread>
#include <mutex>
#include <cstdint>
#include <algorithm>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#endif

// ========== 配置选项 ==========
#define JUOS_USE_UNICODE 0
#define JUOS_64BIT_TIME 1

// ========== 平台相关终端控制 ==========
class Terminal {
private:
#ifdef _WIN32
    static HANDLE hConsole;
    static HANDLE hOutput;
    static DWORD dwOriginalMode;
#endif
    static bool isWindows;
    
public:
    static void init() {
#ifdef _WIN32
        isWindows = true;
        hConsole = GetStdHandle(STD_INPUT_HANDLE);
        hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        GetConsoleMode(hConsole, &dwOriginalMode);
        DWORD dwMode = dwOriginalMode;
        dwMode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
        dwMode |= ENABLE_EXTENDED_FLAGS;
        SetConsoleMode(hConsole, dwMode);
        
        DWORD dwOutMode = 0;
        GetConsoleMode(hOutput, &dwOutMode);
        dwOutMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOutput, dwOutMode);
        SetConsoleOutputCP(CP_UTF8);
#else
        isWindows = false;
        struct termios newt;
        tcgetattr(STDIN_FILENO, &newt);
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
        
        // 启用鼠标事件
        std::cout << "\033[?1000h";
        std::cout << "\033[?1002h";
        std::cout << "\033[?1015h";
        std::cout.flush();
#endif
        std::cout << "\033[2J\033[1;1H\033[40m";
        std::cout << "\033[?25h";
        std::cout.flush();
    }
    
    static void restore() {
#ifdef _WIN32
        SetConsoleMode(hConsole, dwOriginalMode);
#else
        std::cout << "\033[?1000l";
        std::cout << "\033[?1002l";
        std::cout.flush();
        struct termios oldt;
        tcgetattr(STDIN_FILENO, &oldt);
        oldt.c_lflag |= (ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
#endif
    }

    static void setColor(uint8_t r, uint8_t g, uint8_t b) {
        std::cout << "\033[38;2;" << (int)r << ";" << (int)g << ";" << (int)b << "m";
    }

    static void setBgColor(uint8_t r, uint8_t g, uint8_t b) {
        std::cout << "\033[48;2;" << (int)r << ";" << (int)g << ";" << (int)b << "m";
    }

    static void resetColor() {
        std::cout << "\033[0m";
    }

    static void clearScreen() {
        std::cout << "\033[2J\033[1;1H";
    }

    static void setCursorPos(int x, int y) {
        std::cout << "\033[" << y << ";" << x << "H";
    }

    static void hideCursor() {
        std::cout << "\033[?25l";
    }

    static void showCursor() {
        std::cout << "\033[?25h";
    }

    static bool keyPressed() {
#ifdef _WIN32
        return _kbhit() != 0;
#else
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        return select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv) > 0;
#endif
    }

    static int getKey() {
#ifdef _WIN32
        if (!_kbhit()) return -1;
        int ch = _getch();
        if (ch == 224) {
            int ch2 = _getch();
            if (ch2 == 72) return 224 + 72;
            if (ch2 == 80) return 224 + 80;
            if (ch2 == 75) return 224 + 75;
            if (ch2 == 77) return 224 + 77;
            return ch2;
        }
        return ch;
#else
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        
        if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv) > 0) {
            int ch = getchar();
            
            if (ch == 27) {
                int ch2 = getchar();
                if (ch2 == 91) {
                    int ch3 = getchar();
                    if (ch3 == 'M' || ch3 == '<') {
                        int btn = getchar() - 32;
                        int mx = getchar() - 32;
                        int my = getchar() - 32;
                        if (btn == 0 || btn == 1) {
                            return 224 + 1000 + (my << 8) + mx;
                        }
                        return -1;
                    }
                    if (ch3 == 65) return 224 + 72;
                    if (ch3 == 66) return 224 + 80;
                    if (ch3 == 67) return 224 + 77;
                    if (ch3 == 68) return 224 + 75;
                }
                return 27;
            }
            return ch;
        }
        return -1;
#endif
    }
    
    static bool getMousePos(int key, int& x, int& y) {
        if (key >= 224 + 1000) {
            int code = key - (224 + 1000);
            y = code >> 8;
            x = code & 0xFF;
            return true;
        }
        return false;
    }
};

#ifdef _WIN32
HANDLE Terminal::hConsole = GetStdHandle(STD_INPUT_HANDLE);
HANDLE Terminal::hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
DWORD Terminal::dwOriginalMode = 0;
bool Terminal::isWindows = true;
#else
bool Terminal::isWindows = false;
#endif

// ========== 64位时间 ==========
using juos_time = long long;

juos_time getCurrentJuosTime() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
}

std::string formatJuosTime(juos_time t) {
    std::time_t time = static_cast<std::time_t>(t);
    std::tm* tm = std::localtime(&time);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
    return std::string(buf);
}

// ========== 字符图标库 ==========
namespace Icons {
    const char* FOLDER = "[+]";
    const char* FILE_ICON = "[ ]";
    const char* TERMINAL_ICON = "[>_]";
    const char* CALC_ICON = "[=]";
    const char* NOTE_ICON = "[N]";
}

// ========== 图形界面基础元素 ==========
struct Rect {
    int x, y, w, h;
    Rect(int x=0, int y=0, int w=0, int h=0) : x(x), y(y), w(w), h(h) {}
    
    bool contains(int px, int py) {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

class Window {
public:
    std::string title;
    Rect rect;
    bool active;
    bool visible;
    uint8_t r, g, b;
    std::vector<std::string> content;
    std::vector<Rect> clickAreas;
    std::vector<std::function<void()>> clickHandlers;
    
    Window(std::string t, int x, int y, int w, int h, uint8_t cr=200, uint8_t cg=200, uint8_t cb=200)
        : title(t), rect(x,y,w,h), active(false), visible(true), r(cr), g(cg), b(cb) {
        content.resize(h-2, std::string(w-2, ' '));
    }

    void draw() {
        if (!visible) return;
        Terminal::setColor(r,g,b);
        Terminal::setCursorPos(rect.x, rect.y);
        std::cout << "+";
        for (int i=0; i<rect.w-2; ++i) std::cout << "-";
        std::cout << "+";
        Terminal::setCursorPos(rect.x+1, rect.y);
        std::cout << " " << title.substr(0, rect.w-4) << " ";
        for (int i=0; i<rect.h-2; ++i) {
            Terminal::setCursorPos(rect.x, rect.y+1+i);
            std::cout << "|";
            // 用空格填充背景色，防止黑色色块
            std::string line = content[i].substr(0, rect.w-2);
            std::cout << line;
            // 填充剩余空格
            for (int j = (int)line.length(); j < rect.w-2; j++) {
                std::cout << " ";
            }
            std::cout << "|";
        }
        Terminal::setCursorPos(rect.x, rect.y+rect.h-1);
        std::cout << "+";
        for (int i=0; i<rect.w-2; ++i) std::cout << "-";
        std::cout << "+";
        Terminal::resetColor();
    }

    void setContentLine(int line, const std::string& text) {
        if (line >=0 && line < (int)content.size()) {
            content[line] = text;
        }
    }
    
    void addClickArea(int x, int y, int w, int h, std::function<void()> handler) {
        clickAreas.push_back(Rect(rect.x + x, rect.y + y, w, h));
        clickHandlers.push_back(handler);
    }
    
    bool handleClick(int mx, int my) {
        for (size_t i = 0; i < clickAreas.size(); i++) {
            if (clickAreas[i].contains(mx, my)) {
                if (clickHandlers[i]) {
                    clickHandlers[i]();
                    return true;
                }
            }
        }
        return false;
    }
};

// ========== 文件系统模拟 ==========
struct FileEntry {
    std::string name;
    bool isDirectory;
    std::string content;
    juos_time created;
    juos_time modified;
    
    FileEntry() : name(""), isDirectory(false), content(""), created(getCurrentJuosTime()), modified(getCurrentJuosTime()) {}
    FileEntry(std::string n, bool dir=false) : name(n), isDirectory(dir), content(""), created(getCurrentJuosTime()), modified(getCurrentJuosTime()) {}
};

class VirtualFS {
public:
    std::map<std::string, FileEntry> files;
    std::string currentPath;

    VirtualFS() {
        currentPath = "/";
        files["/"] = FileEntry("/", true);
        files["/system"] = FileEntry("system", true);
        files["/system/kernel"] = FileEntry("kernel", false);
        files["/system/kernel"].content = "JuOS 1.0 Kernel (Sim)";
        files["/apps"] = FileEntry("apps", true);
        files["/apps/calc"] = FileEntry("calc", false);
        files["/apps/calc"].content = "Calculator stub";
        files["/apps/note"] = FileEntry("note", false);
        files["/apps/note"].content = "Notepad stub";
        files["/readme"] = FileEntry("readme", false);
        files["/readme"].content = "Welcome to JuOS 1.0!\nType 'help' for commands.";
        files["/usr"] = FileEntry("usr", true);
    }

    std::vector<std::string> listDir(const std::string& path) {
        std::vector<std::string> result;
        for (auto& kv : files) {
            std::string full = kv.first;
            size_t pos = full.rfind('/');
            std::string parent = (pos == 0) ? "/" : full.substr(0, pos);
            if (parent == path) {
                result.push_back(kv.second.name + (kv.second.isDirectory ? "/" : ""));
            }
        }
        return result;
    }

    bool exists(const std::string& path) {
        return files.find(path) != files.end();
    }

    std::string readFile(const std::string& path) {
        auto it = files.find(path);
        if (it != files.end() && !it->second.isDirectory) {
            return it->second.content;
        }
        return "";
    }

    bool writeFile(const std::string& path, const std::string& content) {
        auto it = files.find(path);
        if (it != files.end() && !it->second.isDirectory) {
            it->second.content = content;
            it->second.modified = getCurrentJuosTime();
            return true;
        }
        return false;
    }
};

// ========== JuOS 主系统 ==========
class JuOS {
private:
    VirtualFS fs;
    std::vector<Window> windows;
    int activeWindowIndex;
    bool running;
    std::string commandBuffer;
    bool graphicalMode;
    int cursorX, cursorY;
    bool needRedraw;
    juos_time lastTimeUpdate;
    bool inTerminal;

public:
    JuOS() : activeWindowIndex(-1), running(true), graphicalMode(true), 
             cursorX(5), cursorY(5), needRedraw(true), lastTimeUpdate(0), inTerminal(false) {
        Terminal::init();
        setupDesktop();
        drawDesktop();
    }

    ~JuOS() {
        Terminal::restore();
    }
    
    void setupDesktop() {
        Window desktop("Desktop", 1, 1, 78, 24, 100, 149, 237);
        
        // 设置桌面内容 - 每个图标占一行
        desktop.setContentLine(0, "[+]  System");
        desktop.setContentLine(1, "[+]  Apps");
        desktop.setContentLine(2, "[ ]  readme");
        desktop.setContentLine(3, "[>_] Terminal");
        desktop.setContentLine(4, "[=]  Calculator");
        desktop.setContentLine(5, "[N]  Notepad");
        desktop.setContentLine(6, "                          ");
        desktop.setContentLine(7, " Click/Tap icons to launch");
        desktop.setContentLine(8, " or use arrow keys + Enter");
        
        // 添加点击区域 - 每个图标精确区域
        // System: 行0, 列1-8
        desktop.addClickArea(1, 1, 8, 1, [this]() { launchTerminal(); });
        // Apps: 行1, 列1-8
        desktop.addClickArea(1, 2, 8, 1, [this]() { launchTerminal(); });
        // readme: 行2, 列1-8
        desktop.addClickArea(1, 3, 8, 1, [this]() { showReadme(); });
        // Terminal: 行3, 列1-8
        desktop.addClickArea(1, 4, 8, 1, [this]() { launchTerminal(); });
        // Calculator: 行4, 列1-8
        desktop.addClickArea(1, 5, 8, 1, [this]() { launchCalc(); });
        // Notepad: 行5, 列1-8
        desktop.addClickArea(1, 6, 8, 1, [this]() { launchNotepad(); });
        
        windows.push_back(desktop);
        activeWindowIndex = 0;
    }

    void drawDesktop() {
        Terminal::clearScreen();
        Terminal::setBgColor(20,30,50);
        for (int i=0; i<25; ++i) {
            Terminal::setCursorPos(1, i+1);
            for (int j=0; j<80; ++j) std::cout << " ";
        }
        Terminal::resetColor();
        for (auto& w : windows) w.draw();
        drawStatusBar();
        
        Terminal::setCursorPos(cursorX, cursorY);
        std::cout.flush();
        needRedraw = false;
        lastTimeUpdate = getCurrentJuosTime();
    }
    
    void drawStatusBar() {
        Terminal::setColor(200,200,200);
        Terminal::setCursorPos(1, 25);
        std::cout << " JuOS 1.0 | " << formatJuosTime(getCurrentJuosTime()) << " | [CMD] ";
        Terminal::resetColor();
    }
    
    void updateStatusBar() {
        juos_time now = getCurrentJuosTime();
        if (now != lastTimeUpdate) {
            Terminal::setColor(200,200,200);
            Terminal::setCursorPos(12, 25);
            std::cout << formatJuosTime(now) << " ";
            Terminal::resetColor();
            lastTimeUpdate = now;
            std::cout.flush();
        }
    }

    void run() {
        while (running) {
            updateStatusBar();
            
            if (Terminal::keyPressed()) {
                int key = Terminal::getKey();
                if (key == -1) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                
                int mx, my;
                if (Terminal::getMousePos(key, mx, my)) {
                    if (windows.size() > 0) {
                        // 从后往前检查窗口（最上层优先）
                        for (int i = windows.size() - 1; i >= 0; i--) {
                            if (windows[i].handleClick(mx, my)) {
                                break;
                            }
                        }
                    }
                    continue;
                }
                
                if (key == 27) {
                    // ESC: 如果当前有子窗口，关闭它
                    if (windows.size() > 1) {
                        windows.pop_back();
                        drawDesktop();
                    } else {
                        // 否则切换模式
                        graphicalMode = !graphicalMode;
                        if (graphicalMode) {
                            drawDesktop();
                        } else {
                            Terminal::clearScreen();
                            Terminal::setCursorPos(1,1);
                            std::cout << "JuOS 1.0 Command Line Mode" << std::endl;
                            std::cout << "Type 'exit' to return to GUI" << std::endl;
                            std::cout << "Type 'help' for commands." << std::endl;
                            std::cout << "> ";
                            std::cout.flush();
                        }
                    }
                    continue;
                }
                
                if (graphicalMode) {
                    handleGraphicalInput(key);
                } else {
                    handleCommandInput(key);
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    
    void handleGraphicalInput(int key) {
        if (key == 13) { // Enter
            handleGraphicalClick(cursorX, cursorY);
            drawDesktop();
        } else if (key == 224 + 72) { // Up
            cursorY = std::max(2, cursorY-1);
            Terminal::setCursorPos(cursorX, cursorY);
        } else if (key == 224 + 80) { // Down
            cursorY = std::min(23, cursorY+1);
            Terminal::setCursorPos(cursorX, cursorY);
        } else if (key == 224 + 75) { // Left
            cursorX = std::max(1, cursorX-2);
            Terminal::setCursorPos(cursorX, cursorY);
        } else if (key == 224 + 77) { // Right
            cursorX = std::min(78, cursorX+2);
            Terminal::setCursorPos(cursorX, cursorY);
        }
    }
    
    void handleCommandInput(int key) {
        if (key == 13) {
            std::string cmd = commandBuffer;
            commandBuffer.clear();
            std::cout << "\r\n";
            processCommand(cmd);
            std::cout << "> ";
            std::cout.flush();
        } else if (key == 8 || key == 127) {
            if (!commandBuffer.empty()) {
                commandBuffer.pop_back();
                std::cout << "\b \b";
                std::cout.flush();
            }
        } else if (key >= 32 && key <= 126) {
            commandBuffer.push_back((char)key);
            std::cout << (char)key;
            std::cout.flush();
        }
    }

    void handleGraphicalClick(int x, int y) {
        // 桌面图标点击：行2-7，列3-10
        if (y >= 2 && y <= 7 && x >= 3 && x <= 10) {
            int idx = y - 2;
            if (idx == 0) {
                launchTerminal();
            } else if (idx == 1) {
                launchTerminal();
            } else if (idx == 2) {
                showReadme();
            } else if (idx == 3) {
                launchTerminal();
            } else if (idx == 4) {
                launchCalc();
            } else if (idx == 5) {
                launchNotepad();
            }
        }
    }

    void launchTerminal() {
        // 如果已有终端在运行，不重复打开
        if (inTerminal) return;
        inTerminal = true;
        
        Window term("Terminal", 10, 4, 50, 14, 0, 255, 0);
        term.setContentLine(0, "JuOS Terminal v1.0");
        term.setContentLine(1, "Type 'help' for commands.");
        term.setContentLine(2, "Type 'exit' or ESC to close");
        term.setContentLine(3, "> ");
        windows.push_back(term);
        drawDesktop();
        
        std::string cmd;
        int line = 4;
        bool termRunning = true;
        
        while (termRunning) {
            updateStatusBar();
            
            if (Terminal::keyPressed()) {
                int key = Terminal::getKey();
                if (key == -1) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                
                // ESC 或 Ctrl+C 退出
                if (key == 27) {
                    termRunning = false;
                    break;
                }
                
                if (key == 13) { // Enter
                    if (cmd == "exit") {
                        termRunning = false;
                        break;
                    }
                    std::string output = executeCommand(cmd);
                    if (line < term.rect.h-2) {
                        term.setContentLine(line, "> " + cmd + "  ");
                        line++;
                        if (line < term.rect.h-2) {
                            std::string outLine = output;
                            if ((int)outLine.length() > term.rect.w-4) {
                                outLine = outLine.substr(0, term.rect.w-4);
                            }
                            term.setContentLine(line, outLine);
                            line++;
                        }
                    }
                    cmd.clear();
                    drawDesktop();
                    Terminal::setCursorPos(term.rect.x+2, term.rect.y+line);
                } else if (key == 8 || key == 127) {
                    if (!cmd.empty()) {
                        cmd.pop_back();
                        term.setContentLine(line-1, "> " + cmd + " ");
                        drawDesktop();
                        Terminal::setCursorPos(term.rect.x+2 + cmd.length() + 2, term.rect.y+line);
                    }
                } else if (key >= 32 && key <= 126) {
                    cmd.push_back((char)key);
                    term.setContentLine(line-1, "> " + cmd + " ");
                    drawDesktop();
                    Terminal::setCursorPos(term.rect.x+2 + cmd.length() + 2, term.rect.y+line);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        windows.pop_back();
        inTerminal = false;
        drawDesktop();
    }

    std::string executeCommand(const std::string& cmd) {
        if (cmd == "help") return "Commands: help, dir, time, echo, ver, calc, note";
        if (cmd == "dir") {
            auto list = fs.listDir(fs.currentPath);
            if (list.empty()) return "(empty)";
            std::string result;
            for (auto& s : list) result += s + " ";
            return result;
        }
        if (cmd == "time") {
            return formatJuosTime(getCurrentJuosTime());
        }
        if (cmd == "ver") {
            return "JuOS 1.0 (C) 2026";
        }
        if (cmd.find("echo ") == 0) {
            return cmd.substr(5);
        }
        if (cmd == "calc") {
            return "Calculator: 2+2=4 (sim)";
        }
        if (cmd == "note") {
            return "Notepad: edit file not implemented.";
        }
        return "Unknown command: " + cmd;
    }

    void processCommand(const std::string& cmd) {
        if (cmd == "exit") {
            graphicalMode = true;
            drawDesktop();
            return;
        }
        std::string result = executeCommand(cmd);
        std::cout << result << "\r\n";
        std::cout.flush();
    }

    void showReadme() {
        std::string content = fs.readFile("/readme");
        Window readmeWin("Readme", 15, 5, 50, 12, 200, 200, 100);
        std::stringstream ss(content);
        std::string line;
        int i = 0;
        while (std::getline(ss, line)) {
            if (i < readmeWin.rect.h-2) {
                readmeWin.setContentLine(i, line);
                i++;
            }
        }
        windows.push_back(readmeWin);
        drawDesktop();
        
        bool running = true;
        while (running) {
            updateStatusBar();
            if (Terminal::keyPressed()) {
                int key = Terminal::getKey();
                if (key == 27 || key == 13 || key == 32) {
                    running = false;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        windows.pop_back();
        drawDesktop();
    }

    void launchCalc() {
        Window calc("Calculator", 20, 6, 30, 8, 255, 200, 100);
        calc.setContentLine(0, "  [1] [2] [3]");
        calc.setContentLine(1, "  [4] [5] [6]");
        calc.setContentLine(2, "  [7] [8] [9]");
        calc.setContentLine(3, "  [0] [=] [C]");
        calc.setContentLine(4, "  Press ESC to close");
        windows.push_back(calc);
        drawDesktop();
        
        bool running = true;
        while (running) {
            updateStatusBar();
            if (Terminal::keyPressed()) {
                int key = Terminal::getKey();
                if (key == 27) {
                    running = false;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        windows.pop_back();
        drawDesktop();
    }

    void launchNotepad() {
        Window note("Notepad", 25, 7, 40, 12, 150, 200, 255);
        note.setContentLine(0, "JuOS Notepad");
        note.setContentLine(1, "Type something...");
        note.setContentLine(2, "> ");
        note.setContentLine(3, " ");
        note.setContentLine(4, "Press ESC to close");
        windows.push_back(note);
        drawDesktop();
        
        int line = 3;
        bool noteRunning = true;
        
        while (noteRunning) {
            updateStatusBar();
            
            if (Terminal::keyPressed()) {
                int key = Terminal::getKey();
                if (key == -1) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                
                if (key == 27) {
                    noteRunning = false;
                    break;
                } else if (key == 13) {
                    if (line < note.rect.h-2) {
                        line++;
                        note.setContentLine(line, " ");
                        drawDesktop();
                        Terminal::setCursorPos(note.rect.x+2, note.rect.y+line);
                    }
                } else if (key == 8 || key == 127) {
                    std::string cur = note.content[line];
                    if (!cur.empty()) {
                        cur.pop_back();
                        note.setContentLine(line, cur);
                        drawDesktop();
                        Terminal::setCursorPos(note.rect.x+2 + cur.length(), note.rect.y+line);
                    }
                } else if (key >= 32 && key <= 126) {
                    if (line < note.rect.h-2) {
                        std::string cur = note.content[line];
                        cur.push_back((char)key);
                        note.setContentLine(line, cur);
                        drawDesktop();
                        Terminal::setCursorPos(note.rect.x+2 + cur.length(), note.rect.y+line);
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        windows.pop_back();
        drawDesktop();
    }
};

// ========== 入口 ==========
int main() {
    JuOS os;
    os.run();
    return 0;
}
