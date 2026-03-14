cmake_minimum_required(VERSION 3.5)

project(DesktopPet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

add_executable(DesktopPet WIN32 Main.cpp)
# 同時加上 WIN32 (隱藏 Windows 黑視窗) 和 MACOSX_BUNDLE (產生 Mac App)
add_executable(DesktopPet WIN32 MACOSX_BUNDLE Main.cpp)
target_link_libraries(DesktopPet PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)