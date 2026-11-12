cmake_minimum_required(VERSION 3.20)
project(5my104n_toolz VERSION 1.1.29 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)


add_executable(5my104n_toolz
    src/main.cpp
    src/MainWindow.cpp
    src/ToolCard.cpp
    src/CompilerDialog.cpp
    src/FolderScanner.cpp
    src/ScriptRunner.cpp
    src/ThemeManager.cpp
    src/I18N.cpp
    src/AnsiToHtml.cpp
    src/SmyFormat.cpp
    src/RadarWidget.cpp
    src/ScriptEditor.cpp
    src/SettingsPanel.cpp
)

target_link_libraries(5my104n_toolz PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network
    OpenSSL::Crypto ZLIB::ZLIB)