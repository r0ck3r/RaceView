cmake_minimum_required(VERSION 3.16)
project(raceview VERSION 0.99.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets DBus LinguistTools)

set(DEPS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/.deps/usr/usr)

add_executable(raceview
    src/main.cpp
    src/MpvPlayer.cpp
    src/MpvPlayer.h
    src/MpvWidget.cpp
    src/MpvWidget.h
    src/PlayerWindow.cpp
    src/PlayerWindow.h
    src/SyncController.cpp
    src/SyncController.h
    src/ControlPanel.cpp
    src/ControlPanel.h
    src/OsdWidget.cpp
    src/OsdWidget.h
    src/SeekSlider.h
    src/Icons.h
    src/Mpris.cpp
    src/Mpris.h
    src/MprisAdaptors.h
    resources.qrc
)

target_include_directories(raceview PRIVATE ${DEPS_DIR}/include)
target_link_directories(raceview PRIVATE ${DEPS_DIR}/lib64)
target_link_libraries(raceview PRIVATE Qt6::Widgets Qt6::OpenGLWidgets Qt6::DBus mpv)
target_compile_definitions(raceview PRIVATE RACEVIEW_VERSION="${PROJECT_VERSION}")

set_target_properties(raceview PROPERTIES
    BUILD_RPATH ${DEPS_DIR}/lib64
    INSTALL_RPATH "$ORIGIN/../.deps/usr/usr/lib64"
)

set(TS_FILES raceview_ru.ts raceview_de.ts raceview_es.ts raceview_fr.ts)
qt_add_translation(QM_FILES ${TS_FILES})
add_custom_target(translations ALL DEPENDS ${QM_FILES})

install(TARGETS raceview RUNTIME DESTINATION bin)
install(FILES raceview.desktop DESTINATION share/applications)
install(FILES icons/raceview.svg DESTINATION share/icons/hicolor/scalable/apps)
install(FILES ${QM_FILES} DESTINATION share/raceview/translations)
