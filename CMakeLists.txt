# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.21)	# 要求的最低 CMake 版本
project(ClauPet LANGUAGES CXX)			# 项目名，使用 C++

set(CMAKE_CXX_STANDARD 17)				# 使用 C++17 标准
set(CMAKE_CXX_STANDARD_REQUIRED ON)		# 编译器必须支持 C++17，不支持就报错

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)	# 找到 Qt6 的两个模块：Widgets（窗口）、Network（网络）

add_executable(ClauPet main.cpp "Petwindow.h" "Petwindow.cpp" "Petstate.h")	# 生成程序 ClauPet.exe，由这些源文件组成
set_target_properties(ClauPet PROPERTIES WIN32_EXECUTABLE ON)	# 做成窗口程序：启动时不再弹出黑色控制台
# 把宠物素材编译进 .exe，程序里用 ":/assets/pet/xxx.webp" 这样的路径读取（注意：必须写在 add_executable 之后）
# 5 段动画（4 种状态 + 被拖动）由 QMovie 播放；idle.png 是托盘图标用的静态图
qt_add_resources(ClauPet "pet_image" PREFIX "/" FILES
assets/pet/idle.png
assets/pet/idle.webp
assets/pet/working.webp
assets/pet/waiting.webp
assets/pet/error.webp
assets/pet/dragging.webp
assets/pet/idle_to_working.webp	
assets/pet/working_to_idle.webp	
assets/pet/idle_to_waiting.webp
assets/pet/idle_to_error.webp
assets/pet/working_to_waiting.webp
assets/pet/working_to_error.webp
assets/pet/waiting_to_idle.webp
assets/pet/waiting_to_working.webp
assets/pet/waiting_to_error.webp
assets/pet/error_to_idle.webp
assets/pet/error_to_working.webp
assets/pet/error_to_waiting.webp
)
target_link_libraries(ClauPet PRIVATE Qt6::Widgets Qt6::Network)	# 链接 Qt 的这两个模块

# MSVC 编译器默认按系统编码读取源码，加 /utf-8 让它按 UTF-8 读取，中文字符串和注释才不会乱码
if(MSVC)
    target_compile_options(ClauPet PRIVATE /utf-8)
endif()
