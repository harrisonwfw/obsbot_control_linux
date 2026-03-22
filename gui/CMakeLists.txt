# gui/CMakeLists.txt

# Add the usbio library
add_executable(gui
    gui.cc
)
set_target_properties(gui PROPERTIES
    OUTPUT_NAME camera-control
)

# Smaller release binaries on GCC/Clang (optional; harmless on other toolchains)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(gui PRIVATE $<$<CONFIG:Release>:-s>)
endif()
target_include_directories(gui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..  # Include usbio directory
)

# link qt
target_link_libraries(gui
    PRIVATE
        usbio
        camera_control
        Qt6::Core
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::MultimediaWidgets
)



