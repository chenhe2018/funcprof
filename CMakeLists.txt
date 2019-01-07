#--------------------------------------------------------------------
#    File: CMakeLists.txt
#    Description: CMake Configuration File
#--------------------------------------------------------------------

# minimum required CMake version
cmake_minimum_required(VERSION 3.2)

#--------------------------------------------------------------------
# project setting
#--------------------------------------------------------------------
project(funcprof)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-Wall -g -Wno-deprecated -std=c++11")
#set(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin)
#set(EXECUTABLE_OUTPUT_PATH  ${CMAKE_SOURCE_DIR}/bin)

#--------------------------------------------------------------------
# enviroment variables
#--------------------------------------------------------------------


#--------------------------------------------------------------------
# dependent libs
#--------------------------------------------------------------------


#--------------------------------------------------------------------
# target components
#--------------------------------------------------------------------

add_library(funcprof SHARED profile.cpp)
target_link_libraries(funcprof dl pthread)

add_executable(monitor monitor.cpp)
target_link_libraries(monitor funcprof dl pthread)

add_executable(sample sample.cpp)
target_link_libraries(sample funcprof dl pthread)