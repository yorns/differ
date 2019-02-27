#include <ncurses.h>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <fstream>
#include <regex>
//#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <deque>
#include <boost/process.hpp>
#include <menu.h>
#include <memory>

// g++ -std=c++14 differ.cpp -O0 -g -o differ -lncurses
// g++ -std=c++14 -I/opt/boost/include differ.cpp -O0 -g -pthread -o differ -lncurses -L /opt/boost/lib -lboost_filesystem -lboost_system

// forgotten ncurses things:
#define KEY_RETURN 0xa


using namespace std::chrono_literals;

struct DiffBlock {
    uint32_t m_pStart_orig;
    uint32_t m_pEnd_orig;
    uint32_t m_pStart_new;
    uint32_t m_pEnd_new;
    std::string m_cad;
    std::vector<std::string> m_minusLine;
    std::vector<std::string> m_plusLine;
    DiffBlock(uint32_t start1, uint32_t end1, uint32_t start2, uint32_t end2, std::string& cad)
        : m_pStart_orig(start1), m_pEnd_orig(end1), m_pStart_new(start2), m_pEnd_new(end2), m_cad(cad) {}
};

struct DiffFile {

    std::string m_fromFilename;
    std::string m_toFilename;

    std::vector<DiffBlock> m_diffBlock;

};

#define LINEMAXLEN 180

void writeLine(const std::string& line, uint32_t pos) {

    uint32_t m{0};
    uint signcnt {0};

    size_t lastNonEmpty = line.find_last_not_of(' ');
    if (lastNonEmpty == std::string::npos)
        lastNonEmpty = 0;

    for (char sign : line) {
        mvaddch(pos-1, m++, sign);
        if (lastNonEmpty > m) {
            if (sign != ' ' && signcnt++ < 3)
                std::this_thread::sleep_for(20ms);
            else
                std::this_thread::sleep_for(5ms);

            if (sign == ' ') signcnt = 0;
        }
        refresh();            /* Print it on to the real screen */
    }

}

void printDiff(std::vector<DiffBlock>& diffBlocks, std::deque<std::string>& display)
{
    curs_set(1);
    for (auto dBlock : diffBlocks) {

        uint32_t j{0};
        uint32_t l{0};

        int32_t size_orig{0};
        int32_t size_new{0};

        if (dBlock.m_cad == "c") {
            size_orig = dBlock.m_pEnd_orig - dBlock.m_pStart_orig + 1;
            size_new = dBlock.m_pEnd_new - dBlock.m_pStart_new + 1;
        }
        else if (dBlock.m_cad == "a") {
            size_orig = 0;
            size_new = dBlock.m_pEnd_new - dBlock.m_pStart_new + 1;
            // correct missleading value
            dBlock.m_pEnd_orig = dBlock.m_pStart_orig;
        }
        else if (dBlock.m_cad == "d") {
            size_orig = dBlock.m_pEnd_orig - dBlock.m_pStart_orig + 1;
            size_new = 0;
            // correct missleading value
            dBlock.m_pEnd_new = dBlock.m_pStart_new;
        }

        if (size_new > 0 /* not only delete case */) {
            for (auto i{dBlock.m_pStart_new}; i < dBlock.m_pEnd_new + 1; ++i) {

                // three cases: 1) overwrite 2) add x lines and write them 3) delete x line

                // overwrite
                if (j < size_orig) {
                    display[i - 1] = std::string(LINEMAXLEN, ' ');
                }

                // this is a new line
                if (j >= size_orig) {
                    display.insert(display.begin() + i - 1, std::string(LINEMAXLEN, ' '));
                }

                l = 0;
                for (auto line : display) {
                    mvprintw(l++, 0, line.c_str());
                }
                refresh();
                std::this_thread::sleep_for(10ms);

                l = 0;
                std::string line = dBlock.m_plusLine[j++];

                writeLine(line, i);
                display[i - 1] = line;
                l++;

                std::this_thread::sleep_for(10ms);

            }
        }
        // are there lines to delete
        if (size_new < size_orig) {
            int32_t diffSize = size_orig - size_new;
            for(uint32_t i{0}; i<diffSize; ++i) {
                display.erase(display.begin() + dBlock.m_pEnd_new );
                l = 0;
                for (auto line : display) {
                    mvprintw(l++, 0, line.c_str());
                }
                refresh();
                std::this_thread::sleep_for(50ms);
            }
        }

    }
    curs_set(0);
}

DiffFile diffParser_d(std::istream& file) {

    DiffFile diffFile;
    std::string line;

    static const std::regex pattern_startLine1{"^(\\d*)([cad])(\\d*)$"};
    static const std::regex pattern_startLine2{"^(\\d*),(\\d*)([cad])(\\d*)$"};
    static const std::regex pattern_startLine3{"^(\\d*)([cad])(\\d*),(\\d*)$"};
    static const std::regex pattern_startLine4{"^(\\d*),(\\d*)([cad])(\\d*),(\\d*)$"};

    std::smatch match{};

    enum class Direction {
        in,
        out
    };

    while (std::getline(file, line)) {

        std::string cad;
        if (std::regex_search(line, match, pattern_startLine1)) {
            uint32_t start1{boost::lexical_cast<uint32_t>(match[1].str())};
            cad = match[2].str();
            uint32_t start2{boost::lexical_cast<uint32_t>(match[3].str())};
            uint32_t end1{start1};
            uint32_t end2{start2};
            diffFile.m_diffBlock.push_back(DiffBlock(start1, end1, start2, end2, cad));
        } else if (std::regex_search(line, match, pattern_startLine2)) {
            uint32_t start1{boost::lexical_cast<uint32_t>(match[1].str())};
            uint32_t start2{boost::lexical_cast<uint32_t>(match[4].str())};
            cad = match[3].str();
            uint32_t end1{boost::lexical_cast<uint32_t>(match[2].str())};
            uint32_t end2{start2};
            diffFile.m_diffBlock.push_back(DiffBlock(start1, end1, start2, end2, cad));
        } else if (std::regex_search(line, match, pattern_startLine3)) {
            uint32_t start1{boost::lexical_cast<uint32_t>(match[1].str())};
            uint32_t start2{boost::lexical_cast<uint32_t>(match[3].str())};
            cad = match[2].str();
            uint32_t end1{start1};
            uint32_t end2{boost::lexical_cast<uint32_t>(match[4].str())};
            diffFile.m_diffBlock.push_back(DiffBlock(start1, end1, start2, end2, cad));
        } else if (std::regex_search(line, match, pattern_startLine4)) {
            uint32_t start1{boost::lexical_cast<uint32_t>(match[1].str())};
            uint32_t start2{boost::lexical_cast<uint32_t>(match[4].str())};
            cad = match[3].str();
            uint32_t end1{boost::lexical_cast<uint32_t>(match[2].str())};
            uint32_t end2{boost::lexical_cast<uint32_t>(match[5].str())};
            diffFile.m_diffBlock.push_back(DiffBlock(start1, end1, start2, end2, cad));
        }
        else if (line.size() >= 2 && line.substr(0,2) == "< ") {
            diffFile.m_diffBlock.back().m_minusLine.push_back(line.substr(2)+std::string(LINEMAXLEN - line.length(), ' '));

        }
        else if (line.size() >= 2 && line.substr(0,2) == "> ") {
            diffFile.m_diffBlock.back().m_plusLine.push_back(line.substr(2)+std::string(LINEMAXLEN - line.length(), ' '));
        }

    }

    return diffFile;
}

DiffFile createDiffInformation(const std::string& filenameA, const std::string& filenameB)
{
    boost::process::ipstream output;
    std::error_code ec;
    boost::process::system(boost::process::search_path("diff"), filenameA, filenameB, ec,
                           boost::process::std_out > output);

    if (ec)
        return DiffFile();

    DiffFile diffFile = diffParser_d(output);

    return diffFile;
}

void compileInformation(const std::string& filename, WINDOW* win)
{
    boost::process::ipstream output;

    boost::process::child c1(boost::process::search_path("clang++"), "--std=c++14", filename,
                            (boost::process::std_out & boost::process::std_err) > output);

    scrollok(win,TRUE);

    std::string execCall = "clang++ --std=c++14 "+filename;
    mvwprintw(win, 2, 2, execCall.c_str());

    std::string line;
    uint32_t l{4};

    while (c1.running() && std::getline(output, line) && !line.empty()) {
        if (line.length() > 54)
            line = line.substr(0,57);
        mvwprintw(win, l++, 2, line.c_str());
        wrefresh(win);
    }
    c1.wait();

    l++;
    mvwprintw(win, l++, 2, "---  compile finished   ---");
    l++;
    wrefresh(win);

    if (c1.exit_code() != 0) {
        return;
    }

    boost::process::ipstream output2;
    boost::process::child c2("./a.out", (boost::process::std_out & boost::process::std_err) > output2);

    std::string exec = "a.out";
    mvwprintw(win, l++, 2, execCall.c_str());

    while (c2.running() && std::getline(output2, line) && !line.empty()) {
        if (line.length() > 54)
            line = line.substr(0,57);
        mvwprintw(win, l++, 2, line.c_str());
        wrefresh(win);
    }
    c2.wait();

    l++;
    mvwprintw(win, l++, 2, "---  execute finished  ---");
    l++;
    wrefresh(win);

}


std::string chooser(const std::vector<std::string>& choice) {

    int c;
    MENU *my_menu;

    std::vector<ITEM*> my_items;

    my_items.reserve(choice.size()+1);

    for (auto& i : choice)
        my_items.push_back(new_item(i.c_str(),""));
    my_items.push_back(NULL);

    my_menu = new_menu(my_items.data());

    auto my_menu_win = std::unique_ptr<WINDOW, std::function<void(WINDOW *)>>
            (newwin(10, 40, 4, 4),
             [](WINDOW *w) { delwin(w); });
    keypad(my_menu_win.get(), TRUE);
    set_menu_win(my_menu, my_menu_win.get());
    set_menu_sub(my_menu, derwin(my_menu_win.get(), 8, 38, 1, 1));
    set_menu_format(my_menu, 8, 1);

    /* Set menu mark to the string " * " */
    set_menu_mark(my_menu, " * ");

    /* Print a border around the main window and print a title */
    box(my_menu_win.get(), 0, 0);
    refresh();
    post_menu(my_menu);
    wrefresh(my_menu_win.get());

    while((c = wgetch(my_menu_win.get())) != KEY_RETURN)
    {   switch(c)
        {	case KEY_DOWN:
                menu_driver(my_menu, REQ_DOWN_ITEM);
                break;
            case KEY_UP:
                menu_driver(my_menu, REQ_UP_ITEM);
                break;
            case KEY_NPAGE:
                menu_driver(my_menu, REQ_SCR_DPAGE);
                break;
            case KEY_PPAGE:
                menu_driver(my_menu, REQ_SCR_UPAGE);
                break;

        }
        wrefresh(my_menu_win.get());

    }

    int pos = item_index(current_item(my_menu));
    unpost_menu(my_menu);
    wrefresh(my_menu_win.get());

    free_menu(my_menu);

    for (auto& i : my_items)
        free_item(i);

    return choice[pos];
}



std::string chooseNextFile(const std::string& path, const std::string& suffix)
{
    boost::filesystem::path someDir(path);
    boost::filesystem::directory_iterator end_iter;

    std::vector<std::string> files;

    if ( boost::filesystem::exists(someDir) && boost::filesystem::is_directory(someDir))
    {
        for( boost::filesystem::directory_iterator dir_iter(someDir) ; dir_iter != end_iter ; ++dir_iter)
        {
            if (boost::filesystem::is_regular_file(dir_iter->status()) )
            {
                std::string name{dir_iter->path().string().substr(2)};
                if (name.length() > suffix.length() && name.substr(name.length()-suffix.length()) == suffix)
                    files.push_back(name);
            }
        }
    }

    files.push_back("/dev/null");
    files.push_back("exit");
    return chooser(files);
}

int main(int argc, char* argv[]) {

    if (argc > 1) {
        std::cerr << "usage: " << argv[0];
        return 0;
    }

    initscr();            /* Start curses mode 		*/
    cbreak();            /* Line buffering disabled, Pass on
	curs_set(0);				 * everty thing to me 		*/
    keypad(stdscr, TRUE);        /* I need that nifty F1 	*/

    std::string currentFile{"/dev/null"};

    std::deque<std::string> display;
    display.resize(50);

    int c;

    while ((c = getch()) != 'q') {
        if (c == 'c') {
            {
                auto my_compile_win = std::unique_ptr<WINDOW, std::function<void(WINDOW *)>>
                        (newwin(20, 60, 4, 4),
                         [](WINDOW *w) { delwin(w); });
                /* Print a border around the main window and print a title */
                box(my_compile_win.get(), 0, 0);
                wrefresh(my_compile_win.get());
                compileInformation(currentFile, my_compile_win.get());
                getch();
            }

            int l = 0;
            for (auto line : display) {
                mvprintw(l++, 0, line.c_str());
            }
            refresh();
        }
        if (c == 'q')
            break;

        if (c == 'd') {
            std::string b = chooseNextFile("./", ".cpp");
            clear();
            refresh();
            if (b == "exit")
                break;
            DiffFile dFile = createDiffInformation(currentFile, b);
            currentFile = b;
            printDiff(dFile.m_diffBlock, display);
        }

        if (c == 't') {
            int l = 0;
            for (auto& line : display) {
                line = std::string(LINEMAXLEN, ' ');
                mvprintw(l++, 0, line.c_str());
            }
            refresh();
            currentFile = "/dev/null";
        }
    }
    endwin();            /* End curses mode		  */

    return 0;
}

/*

    for (auto i : dFile3.m_diffBlock) {
        std::cout << "new Block: " << i.m_pStart_orig << ":" << i.m_pEnd_orig << "\n";
        std::cout << "\nout lines:\n";
        for (auto l : i.m_minusLine) {
            std::cout << l << "\r";
        }
        std::cout << "\nin lines:\n";
        for (auto l : i.m_plusLine) {
            std::cout << l << "\r";
        }

    }

*/
