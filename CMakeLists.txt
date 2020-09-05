# CMakeList.txt : CMake project for cpp-coroutine-job, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project (cpp-coroutine-job)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_CXX_FLAGS "/await")

file(GLOB "*.h" "*.cpp" fsource)
# Add source to this project's executable.
add_executable (cpp-coroutine-job 
   "main.cpp" 
   "utils.hpp" 
   "schedule/event.cpp"
   "schedule/scheduler.cpp"
 "span.hpp")

# TODO: Add tests and install targets if needed.

# target_include_directories(cpp-coroutine-job public "${PROJECT_SOURCE_DIR}")
# target_include_directories(cpp-coroutine-job public "${PROJECT_SOURCE_DIR}/schedule")
