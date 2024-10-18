cmake_minimum_required(VERSION 3.5.0)
project(the-game VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
# set(CMAKE_VERBOSE_MAKEFILE ON)

add_executable(the-game main.cpp)

include(cmake/CPM.cmake)

find_package(Vulkan)
target_link_libraries(the-game Vulkan::Vulkan)

CPMAddPackage("gh:glfw/glfw#3.4")
target_link_libraries(the-game glfw)

CPMAddPackage("gh:g-truc/glm#1.0.1")
target_link_libraries(the-game glm)