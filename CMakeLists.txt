# 提高要求的版本，消滅黃字警告
cmake_minimum_required(VERSION 3.15)

project(DesktopPet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 讓 CMake 自動處理 Qt 的 MOC 機制
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 尋找 Qt6 的核心、圖形介面與網路模組
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# === 關鍵在這裡！確保整個檔案只有這一行 add_executable ===
# WIN32 負責隱藏 Windows 黑視窗，MACOSX_BUNDLE 負責打包 Mac 的 .app
add_executable(DesktopPet WIN32 MACOSX_BUNDLE Main.cpp resources.qrc)

# 連結 Qt 函式庫
target_link_libraries(DesktopPet PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)