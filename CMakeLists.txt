cmake_minimum_required(VERSION 3.10)

# Set the project name and version
project(CameraControl VERSION 1.0)

# Default to Release for single-config generators (e.g. Ninja, Make)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build configuration" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# Find the required packages
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia MultimediaWidgets)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Include directories
include_directories(${Qt6_INCLUDE_DIRS})
include_directories(${Qt6Multimedia_INCLUDE_DIRS})

# Add subdirectories
add_subdirectory(usbio)
add_subdirectory(libs)
add_subdirectory(gui)

include(GNUInstallDirs)

install(TARGETS gui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/gui/camera-control.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
)
