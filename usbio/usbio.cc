// Copyright 2025 Harrison W.

#include "usbio.hh"

#include <iostream>
#include <fstream>
#include <string>
#include <glob.h>
#include <cstring>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>

CameraHandle open_camera(const std::string& hint) {
    // std::ifstream file(hint);
    int fd = open(hint.c_str(), O_RDWR);
    if (fd != -1) {
        // close the file descriptor
        close(fd);
        // Convert file to CameraHandle as needed
        return CameraHandle(hint);
    }
    // Try opening with /dev/ prefix
    std::string dev_path = "/dev/" + hint;
    fd = open(dev_path.c_str(), O_RDWR);
    if (fd != -1) {
        close(fd);
        // Convert file descriptor to CameraHandle
        return CameraHandle(dev_path);
    }

    glob_t glob_result;
    glob("/dev/video*", GLOB_TILDE, nullptr, &glob_result);

    for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
        int fd = open(glob_result.gl_pathv[i], O_RDWR);
        if (fd != -1) {
            struct v4l2_capability video_info;
            if (ioctl(fd, VIDIOC_QUERYCAP, &video_info) != -1) {
                auto card = convert_to_string(video_info.card, sizeof(video_info.card));
                auto bus_info = convert_to_string(video_info.bus_info, sizeof(video_info.bus_info));

                std::cout << "Info: Card: " << card
                          << "\nBus:  " << bus_info
                          << "\ndc " << std::hex << (video_info.device_caps & 0x800000) << std::dec << std::endl;

                if ((strstr(card.c_str(), hint.c_str()) ||
                     strstr(bus_info.c_str(), hint.c_str())) &&
                    (video_info.device_caps & 0x800000) == 0) {
                    close(fd);
                    // Convert fd to CameraHandle as needed
                    return CameraHandle(glob_result.gl_pathv[i]);
                }
            }
            close(fd);
        }
    }
    globfree(&glob_result);
    throw CameraError("No camera found");
}