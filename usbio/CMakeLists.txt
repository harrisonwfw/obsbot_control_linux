# usbio/CMakeLists.txt

# Add the usbio library
add_library(usbio
    usbio.cc
    usbio.hh
)

# Include directories for usbio
target_include_directories(usbio
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}  # Include current directory for headers
)

# Link any necessary libraries (if needed)
target_link_libraries(usbio)

# Add tests if needed
enable_testing()
add_executable(usbio_test
    usbio_test.cc
)
target_link_libraries(usbio_test
    PRIVATE
        usbio
        gtest
        gtest_main
)

