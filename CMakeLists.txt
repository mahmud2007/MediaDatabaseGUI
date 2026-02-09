cmake_minimum_required(VERSION 3.20)
project(MediaDatabaseGUI)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(MediaDatabaseGUI main.cpp)

set(CMAKE_PREFIX_PATH "/opt/homebrew")
find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System)

target_link_libraries(MediaDatabaseGUI PRIVATE
        SFML::Graphics
        SFML::Window
        SFML::System
)