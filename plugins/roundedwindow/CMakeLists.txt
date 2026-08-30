cmake_minimum_required(VERSION 3.16)
project(cutefish_roundedwindow LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# The plugin factory class is created by a macro, so automoc has to be taught
# about it.
list(APPEND CMAKE_AUTOMOC_MACRO_NAMES
    KWIN_EFFECT_FACTORY
    KWIN_EFFECT_FACTORY_ENABLED
    KWIN_EFFECT_FACTORY_SUPPORTED
    KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED)

include(GNUInstallDirs)

find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Gui)
find_package(KWin REQUIRED)

add_library(cutefish_roundedwindow MODULE roundedwindow.cpp)

set_target_properties(cutefish_roundedwindow PROPERTIES PREFIX "")

target_link_libraries(cutefish_roundedwindow
    PRIVATE
        Qt6::Core
        Qt6::Gui
        KWin::kwin
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

install(TARGETS cutefish_roundedwindow
    DESTINATION "${KDE_INSTALL_PLUGINDIR}/kwin/effects/plugins")
