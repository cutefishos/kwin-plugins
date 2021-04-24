cmake_minimum_required(VERSION 3.5)

project(cutefish-kwin-plugins)

set(PROJECT_NAME "cutefish-kwin")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_subdirectory(plugins)

install(FILES config/kglobalshortcutsrc DESTINATION /etc/xdg)
install(FILES config/kwinrc DESTINATION /etc/xdg)
install(FILES config/kwinrulesrc DESTINATION /etc/xdg)

install(DIRECTORY scripts/cutefishlauncher DESTINATION /usr/share/kwin/scripts)
install(DIRECTORY scripts/cutefish_squash DESTINATION /usr/share/kwin/effects)
install(DIRECTORY scripts/cutefish_scale DESTINATION /usr/share/kwin/effects)
install(DIRECTORY tabbox/cutefish_thumbnail DESTINATION /usr/share/kwin/tabbox)
