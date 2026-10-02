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

# Binary packages: `cpack -G DEB` / `cpack -G RPM` in a build configured with
# CMAKE_INSTALL_PREFIX=/usr. Library dependencies are computed from the binary
# (dpkg-shlibdeps / rpm autoreq) so the packages follow the build distro's Qt.
set(CPACK_PACKAGE_NAME mikmick)
set(CPACK_PACKAGE_VENDOR "Kai_HT")
set(CPACK_PACKAGE_CONTACT "Kai_HT <157885033+KaiHT-Ladiant@users.noreply.github.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_DESCRIPTION
    "MikMick is a PicPick-like screen capture tool for Linux with a ribbon style image editor, color picker, color palette, magnifier, ruler, crosshair, protractor and whiteboard.")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY MikMick)
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE)
set(CPACK_STRIP_FILES ON)
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})

set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION graphics)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "qt6-qpa-plugins")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-wayland, xdg-desktop-portal, xdg-utils, curl")
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "grim")

set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE MIT)
set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
set(CPACK_RPM_PACKAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_RPM_PACKAGE_SUGGESTS "xdg-desktop-portal, curl, grim")
# Directories owned by filesystem / hicolor-icon-theme must not be claimed by the package.
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    /usr/share/applications
    /usr/share/metainfo
    /usr/share/icons
    /usr/share/icons/hicolor
    /usr/share/icons/hicolor/scalable
    /usr/share/icons/hicolor/scalable/apps)

include(CPack)
