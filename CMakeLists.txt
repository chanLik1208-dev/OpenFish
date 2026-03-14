cmake_minimum_required(VERSION 3.5)

project(DesktopPet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# 這裡的 Main.cpp 大小寫必須跟你左側的檔名一模一樣喔！
add_executable(DesktopPet Main.cpp)

target_link_libraries(DesktopPet PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)