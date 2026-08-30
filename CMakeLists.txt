cmake_minimum_required(VERSION 3.5)

project(cutefish-kwin-plugins)

set(PROJECT_NAME "cutefish-kwin")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)

set(CUTEFISH_KWIN_CONFIG_DIR "/etc/xdg" CACHE PATH "Directory for system KWin configuration")
set(CUTEFISH_KWIN_DATA_DIR "${CMAKE_INSTALL_DATAROOTDIR}/kwin" CACHE PATH "Directory for KWin scripts and tabbox layouts")

add_subdirectory(plugins)

install(FILES config/kglobalshortcutsrc DESTINATION "${CUTEFISH_KWIN_CONFIG_DIR}")
install(FILES config/kwinrc DESTINATION "${CUTEFISH_KWIN_CONFIG_DIR}")
install(FILES config/kwinrulesrc DESTINATION "${CUTEFISH_KWIN_CONFIG_DIR}")

install(DIRECTORY scripts/cutefishlauncher DESTINATION "${CUTEFISH_KWIN_DATA_DIR}/scripts")
install(DIRECTORY scripts/cutefish_squash DESTINATION "${CUTEFISH_KWIN_DATA_DIR}/effects")
install(DIRECTORY scripts/cutefish_scale DESTINATION "${CUTEFISH_KWIN_DATA_DIR}/effects")
install(DIRECTORY scripts/cutefish_popups DESTINATION "${CUTEFISH_KWIN_DATA_DIR}/effects")
install(DIRECTORY tabbox/cutefish_thumbnail DESTINATION "${CUTEFISH_KWIN_DATA_DIR}/tabbox")
