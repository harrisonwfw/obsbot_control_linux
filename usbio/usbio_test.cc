// Copyright 2025 Harrison W.

#include "usbio.hh"

#include <gtest/gtest.h>


TEST(V4L2Capability, Query) {
    // Example usage
    int fd = open("/dev/video0", O_RDWR);
    if (fd == -1) {
        perror("Failed to open device");
    }

    v4l2_capability cap;
    std::error_code ec = V4L2Capability::query(fd, cap);
    if (ec) {
        std::cerr << "Error querying capabilities: " << ec.message() << std::endl;
    } else {
        std::cout << "Driver: " << convert_to_string(cap.driver, sizeof(cap.driver)) << "\n"
                  << "Card: " << convert_to_string(cap.card, sizeof(cap.card)) << "\n"
                  << "Bus Info: " << convert_to_string(cap.bus_info, sizeof(cap.bus_info)) << std::endl;
    }

    close(fd);
}


