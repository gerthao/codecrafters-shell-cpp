cmake_minimum_required(VERSION 3.13)

project(shell-starter-cpp LANGUAGES CXX)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp src/*.hpp)

set(CMAKE_CXX_STANDARD 23) # Enable the C++23 standard
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

add_executable(shell ${SOURCE_FILES})

target_sources(shell PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES FILES
        src/command.cppm
)

target_link_libraries(shell PRIVATE readline)
