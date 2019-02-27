#g++ -std=c++14 -I/opt/boost/include differ.cpp -O0 -g -pthread -o differ -lncurses -L /opt/boost/lib -lboost_filesystem -lboost_system

cmake_minimum_required(VERSION 3.3)

## name of the project / app
project(differ)

set (BOOST_ROOT /opt/boost)

find_package( Threads REQUIRED )
find_package( Boost 1.66.0 REQUIRED COMPONENTS system filesystem )

set( CURSES_NEED_NCURSES TRUE )
find_package( Curses REQUIRED )

add_executable(differ differ.cpp)
target_include_directories(differ PRIVATE ${CURSES_INCLUDE_DIR} ${Boost_INCLUDE_DIR})
target_link_libraries(differ ${Boost_LIBRARIES} ${CURSES_LIBRARIES} -lmenu Threads::Threads)
set_target_properties(differ PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO )
