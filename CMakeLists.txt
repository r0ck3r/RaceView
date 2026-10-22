cmake_minimum_required(VERSION 3.16)
project(raceview VERSION 0.99.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets Svg LinguistTools)

# MPRIS2 (D-Bus) is a Linux-only desktop integration.
set(RACEVIEW_MPRIS OFF)
if(UNIX AND NOT APPLE)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    set(RACEVIEW_MPRIS ON)
endif()

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

target_link_libraries(raceview PRIVATE Qt6::Widgets Qt6::OpenGLWidgets Qt6::Svg mpv)
target_compile_definitions(raceview PRIVATE RACEVIEW_VERSION="${PROJECT_VERSION}")

# Optional prebuilt libmpv dev root. Used on Windows CI, where MSYS2 no
# longer ships an mpv package. The shinchiro mpv-dev archive is flat:
#   <root>/include/mpv/*.h, <root>/libmpv.dll.a, <root>/libmpv-2.dll
set(MPV_ROOT "" CACHE PATH "Prebuilt libmpv dev root (include/ + libmpv.dll.a)")
if(MPV_ROOT)
    target_include_directories(raceview PRIVATE "${MPV_ROOT}/include")
    target_link_directories(raceview PRIVATE "${MPV_ROOT}")
endif()

if(RACEVIEW_MPRIS)
    target_compile_definitions(raceview PRIVATE RACEVIEW_MPRIS)
    target_link_libraries(raceview PRIVATE Qt6::DBus)
endif()

# Bundled libmpv lives in .deps only on Linux. On Windows the prebuilt mpv
# dev package is supplied via -DMPV_ROOT (see the MPV_ROOT block above).
if(UNIX AND NOT APPLE)
    target_include_directories(raceview PRIVATE ${DEPS_DIR}/include)
    target_link_directories(raceview PRIVATE ${DEPS_DIR}/lib64)
    set_target_properties(raceview PROPERTIES
        BUILD_RPATH ${DEPS_DIR}/lib64
        INSTALL_RPATH "$ORIGIN/../.deps/usr/usr/lib64"
    )
endif()

# No console window on Windows.
set_target_properties(raceview PROPERTIES WIN32_EXECUTABLE TRUE)

set(TS_FILES raceview_ru.ts raceview_de.ts raceview_es.ts raceview_fr.ts)
qt_add_translation(QM_FILES ${TS_FILES})
add_custom_target(translations ALL DEPENDS ${QM_FILES})

install(TARGETS raceview RUNTIME DESTINATION bin)
if(UNIX AND NOT APPLE)
    install(FILES raceview.desktop DESTINATION share/applications)
    install(FILES icons/raceview.svg DESTINATION share/icons/hicolor/scalable/apps)
endif()
install(FILES ${QM_FILES} DESTINATION share/raceview/translations)
