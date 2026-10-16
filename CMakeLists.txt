cmake_minimum_required(VERSION 3.21)
project(ClaudePet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(claude-pet main.cpp)
target_link_libraries(claude-pet PRIVATE Qt6::Widgets)

if(MSVC)
    target_compile_options(claude-pet PRIVATE /utf-8)
endif()
