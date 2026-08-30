cmake_minimum_required(VERSION 3.16)
project(cutefishdecoration LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)

find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Gui Widgets)
find_package(KF6CoreAddons REQUIRED)
find_package(KDecoration3 REQUIRED)

set(decoration_SRCS
    decoration.cpp
    button.cpp
    resources.qrc
)

add_library(cutefishdecoration MODULE ${decoration_SRCS})

# KWin looks the decoration plugin up by file name (the "library" key of the
# [org.kde.kdecoration2] group in kwinrc), so the file has to be named after the
# plugin id.
set_target_properties(cutefishdecoration PROPERTIES
    PREFIX ""
    OUTPUT_NAME "org.cutefish.decoration")

target_link_libraries(cutefishdecoration
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        KF6::CoreAddons
        KDecoration3::KDecoration
)

if(NOT KDE_INSTALL_PLUGINDIR)
    find_program(QT6_QTPATHS_EXECUTABLE qtpaths6)
    if(QT6_QTPATHS_EXECUTABLE)
        execute_process(COMMAND "${QT6_QTPATHS_EXECUTABLE}" --query QT_INSTALL_PLUGINS
            OUTPUT_VARIABLE KDE_INSTALL_PLUGINDIR OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
endif()

if(NOT KDE_INSTALL_PLUGINDIR)
    set(KDE_INSTALL_PLUGINDIR "${CMAKE_INSTALL_LIBDIR}/qt6/plugins")
endif()

install(TARGETS cutefishdecoration
    DESTINATION "${KDE_INSTALL_PLUGINDIR}/org.kde.kdecoration3")
