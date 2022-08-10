cmake_minimum_required(VERSION 3.8)
project(ECS)

set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures for Mac OS X" FORCE)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY $<1:${PROJECT_SOURCE_DIR}/Bin/Release>)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG $<1:${PROJECT_SOURCE_DIR}/Bin/Debug>)

add_executable(ECS main.cpp)