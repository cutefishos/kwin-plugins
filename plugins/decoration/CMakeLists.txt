project (cutefishdecoration)
set(CMAKE_CXX_STANDARD 11)

find_package(KF5CoreAddons REQUIRED)
find_package(KF5Config REQUIRED)
find_package(KF5WindowSystem REQUIRED)
find_package(KDecoration2 REQUIRED)
find_package(Qt5 CONFIG REQUIRED COMPONENTS Gui Core)

set (decoration_SRCS
    decoration.cpp
    button.cpp
    resources.qrc
)

add_library (cutefishdecoration MODULE
    ${decoration_SRCS}
)

target_link_libraries (cutefishdecoration
    PUBLIC
        Qt5::Core
        Qt5::Gui
        KF5::ConfigCore
        KF5::ConfigGui
        KF5::CoreAddons
        KF5::WindowSystem

    PRIVATE
        KDecoration2::KDecoration
)

install (TARGETS cutefishdecoration
         DESTINATION ${QT_PLUGINS_DIR}/org.kde.kdecoration2)
