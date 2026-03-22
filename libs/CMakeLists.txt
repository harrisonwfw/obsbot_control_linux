# libs/CMakeLists.txt

# Add the camera_control library
add_library(camera_control
    camera_control.cc
    camera_control.hh
)

# Link any necessary libraries (if needed)
target_include_directories(camera_control
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}  # Include current directory for headers
        ${CMAKE_CURRENT_SOURCE_DIR}/..  # Include usbio directory
)

# Link any necessary libraries (if needed)
target_link_libraries(camera_control PRIVATE usbio)

# Add tests if needed
enable_testing()
add_executable(camera_control_test
    camera_control_test.cc
)
target_include_directories(camera_control_test
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}  # Include current directory for headers
        ${CMAKE_CURRENT_SOURCE_DIR}/..  # Include usbio directory
)
# Link Google Test to the test executable
target_link_libraries(camera_control_test
    PRIVATE
        camera_control
        gtest
        gtest_main
        usbio
)
