cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(wav2raw)

add_executable(wav2raw main.cpp)
target_link_libraries(wav2raw PRIVATE pthread)