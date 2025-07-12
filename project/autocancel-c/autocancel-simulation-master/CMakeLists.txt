cmake_minimum_required(VERSION 3.10)
project(autocancel)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/module ${CMAKE_MODULE_PATH})

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2")

find_package(ClangFormat)

include_directories(libs/include)
link_directories(libs)

add_subdirectory(libs)
#add_subdirectory(tests)