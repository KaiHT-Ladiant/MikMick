cmake_minimum_required(VERSION 3.16)

project(MikMick
    VERSION 0.2.0
    DESCRIPTION "All-in-one screen capture, image editor and graphic tools for Linux"
    HOMEPAGE_URL "https://github.com/KaiHT-Ladiant/MikMick"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(MIKMICK_WITH_X11 "Use XCB for window picking, global hotkeys and auto-scroll" ON)
option(MIKMICK_BUILD_TESTS "Build unit tests" ON)

include(GNUInstallDirs)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Network PrintSupport)
find_package(Qt6 6.2 QUIET OPTIONAL_COMPONENTS DBus)

set(MIKMICK_HAVE_X11 OFF)
if(MIKMICK_WITH_X11)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(XCB IMPORTED_TARGET xcb xcb-keysyms xcb-xtest xcb-xfixes)
        if(XCB_FOUND)
            set(MIKMICK_HAVE_X11 ON)
        endif()
    endif()
endif()

message(STATUS "MikMick: X11/XCB support = ${MIKMICK_HAVE_X11}, DBus portal = ${Qt6DBus_FOUND}")

configure_file(src/core/Version.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h @ONLY)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(MIKMICK_WARNINGS -Wall -Wextra -Wno-unused-parameter)
endif()

# Each module is a static library so it can be compiled and tested on its own.
function(mikmick_module name dir)
    file(GLOB _sources CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/${dir}/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/${dir}/*.h)
    if(NOT _sources MATCHES "\\.cpp")
        set(_empty ${CMAKE_CURRENT_BINARY_DIR}/generated/empty_${name}.cpp)
        file(WRITE ${_empty} "")
        list(APPEND _sources ${_empty})
    endif()
    add_library(${name} STATIC ${_sources})
    target_include_directories(${name} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}/generated)
    target_compile_options(${name} PRIVATE ${MIKMICK_WARNINGS})
    target_compile_definitions(${name} PUBLIC QT_NO_KEYWORDS QT_USE_QSTRINGBUILDER)
endfunction()

mikmick_module(mm_core core)
target_link_libraries(mm_core PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::PrintSupport)
if(Qt6DBus_FOUND)
    target_link_libraries(mm_core PUBLIC Qt6::DBus)
    target_compile_definitions(mm_core PUBLIC MIKMICK_HAVE_DBUS)
endif()
if(MIKMICK_HAVE_X11)
    target_link_libraries(mm_core PUBLIC PkgConfig::XCB)
    target_compile_definitions(mm_core PUBLIC MIKMICK_HAVE_X11)
endif()

# Development aid: -DMIKMICK_ONLY_MODULE=editor compiles core plus that one module, so a
# module can be checked while the others are being changed.
set(MIKMICK_ONLY_MODULE "" CACHE STRING "Compile only core and the given module (ui/capture/editor/tools/options)")

if(MIKMICK_ONLY_MODULE)
    mikmick_module(mm_${MIKMICK_ONLY_MODULE} ${MIKMICK_ONLY_MODULE})
    target_link_libraries(mm_${MIKMICK_ONLY_MODULE} PUBLIC mm_core)
    return()
endif()

mikmick_module(mm_ui ui)
target_link_libraries(mm_ui PUBLIC mm_core)

mikmick_module(mm_capture capture)
target_link_libraries(mm_capture PUBLIC mm_ui)

mikmick_module(mm_editor editor)
target_link_libraries(mm_editor PUBLIC mm_ui)

mikmick_module(mm_tools tools)
target_link_libraries(mm_tools PUBLIC mm_capture)

mikmick_module(mm_options options)
target_link_libraries(mm_options PUBLIC mm_capture)

file(GLOB MIKMICK_APP_SOURCES CONFIGURE_DEPENDS src/app/*.cpp src/app/*.h)
if(MIKMICK_APP_SOURCES)
    add_executable(mikmick ${MIKMICK_APP_SOURCES})
    target_link_libraries(mikmick PRIVATE mm_editor mm_tools mm_options mm_capture mm_ui mm_core)
    target_compile_options(mikmick PRIVATE ${MIKMICK_WARNINGS})
    install(TARGETS mikmick RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(FILES packaging/io.github.kaiht_ladiant.MikMick.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/io.github.kaiht_ladiant.MikMick.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES packaging/mikmick.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

if(MIKMICK_BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt)
    enable_testing()
    add_subdirectory(tests)
endif()
