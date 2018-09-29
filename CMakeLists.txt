cmake_minimum_required(VERSION 3.2)
project(funcprof)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-Wall -g -Wno-deprecated -std=c++11")

add_library(funcprof SHARED profile.cpp)
target_link_libraries(funcprof dl pthread)

add_executable(monitor monitor.cpp)
target_link_libraries(monitor funcprof dl pthread)

add_executable(sample sample.cpp)
target_link_libraries(sample funcprof dl pthread)