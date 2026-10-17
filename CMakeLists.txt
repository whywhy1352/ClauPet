cmake_minimum_required(VERSION 3.21)
project(ClaudePet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(claude-pet main.cpp "Petwindow.h" "Petwindow.cpp" "Petstate.h")
qt_add_resources(claude-pet "pet_image" PREFIX "/" FILES 
assets/pet/idle.png
assets/pet/working.png
assets/pet/waiting.png
assets/pet/error.png
)
target_link_libraries(claude-pet PRIVATE Qt6::Widgets)

if(MSVC)
    target_compile_options(claude-pet PRIVATE /utf-8)
endif()
