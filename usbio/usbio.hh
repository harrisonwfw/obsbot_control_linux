// Copyright 2025 Harrison W.

#pragma once

#include <array>
#include <vector>
#include <string>
#include <system_error>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/uvcvideo.h> // Ensure this header is available
#include <linux/videodev2.h>
#include <iostream>
#include <fstream>

template <typename T>
inline std::string convert_to_string(const T* data, size_t size) {
    // Check if the data can be converted to a string.
    if (std::is_convertible<T, char>::value) {
        return std::string(reinterpret_cast<const char*>(data), size);
    }
    return "Cannot convert to string";
}

// Define ioctl command
constexpr uint8_t VIDIOC_QUERYCAP_MAGIC = 'V';
constexpr uint8_t VIDIOC_QUERYCAP_QUERY_MESSAGE = 0x00;
#define IOCTL_VIDEOC_QUERYCAP _IOR(VIDIOC_QUERYCAP_MAGIC, VIDIOC_QUERYCAP_QUERY_MESSAGE, struct v4l2_capability)


constexpr uint8_t UVC_RC_UNDEFINED = 0x00;
constexpr uint8_t UVC_GET_CUR = 0x81;
constexpr uint8_t UVC_SET_CUR = 0x01;
constexpr uint8_t UVC_GET_MIN = 0x82;
constexpr uint8_t UVC_GET_MAX = 0x83;
constexpr uint8_t UVC_GET_RES = 0x84;
constexpr uint8_t UVC_GET_LEN = 0x85;
constexpr uint8_t UVC_GET_INFO = 0x86;
constexpr uint8_t UVC_GET_DEF = 0x87;

// Set format
inline int set_format(int fd, int width, int height, uint32_t pix_format = V4L2_PIX_FMT_H264) {
    struct v4l2_format format = {0};
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = pix_format;
    format.fmt.pix.field = V4L2_FIELD_NONE;

    int res = ioctl(fd, VIDIOC_S_FMT, &format);
    if (res == -1) {
        return -1;
    }

    return res;
}

// Example usage of ioctl
inline int perform_ioctl(int fd, struct uvc_xu_control_query* query) {
    return ioctl(fd, UVCIOC_CTRL_QUERY, query);
}

inline int perform_videoc_querycap(int fd, struct v4l2_capability* cap) {
    return ioctl(fd, IOCTL_VIDEOC_QUERYCAP, cap);
}

/** device_caps is preferred; older drivers only fill capabilities. */
inline __u32 v4l2_effective_caps(const struct v4l2_capability& cap) {
    return cap.device_caps ? cap.device_caps : cap.capabilities;
}

/** True if this V4L2 node can allocate capture buffers / stream video. */
inline bool v4l2_fd_has_video_capture(int fd) {
    struct v4l2_capability cap {};
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        return false;
    }
    return (v4l2_effective_caps(cap) & V4L2_CAP_VIDEO_CAPTURE) != 0;
}

class V4L2Capability {
public:
    static std::error_code query(int fd, v4l2_capability& cap) {
        if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
            return std::error_code(errno, std::generic_category());
        }
        return {};
    }
};

class UvcUsbIo {
public:
    virtual ~UvcUsbIo() = default;
    virtual std::error_code info() const = 0;
    virtual std::error_code io(uint8_t unit, uint8_t selector, uint8_t query, std::vector<uint8_t>& data) = 0;
};

class CameraHandle : public UvcUsbIo {
public:
    explicit CameraHandle(const std::string& filename) : fd_(-1) {
        fd_ = open(filename.c_str(), O_RDWR);
        if (fd_ == -1) {
            throw std::runtime_error("Failed to open file descriptor");
        }
    }

    ~CameraHandle() {
        if (fd_ != -1) {
            close(fd_);
        }
    }

    std::error_code info() const override {
        v4l2_capability video_info;
        if (ioctl(fd_, VIDIOC_QUERYCAP, &video_info) == -1) {
            return std::error_code(errno, std::generic_category());
        }

        std::cout << "Card: " << convert_to_string(video_info.card, sizeof(video_info.card)) << "\n"
                  << "Bus: " << convert_to_string(video_info.bus_info, sizeof(video_info.bus_info)) << std::endl;
        return {};
    }

    std::error_code io(uint8_t unit, uint8_t selector, uint8_t query, std::vector<uint8_t>& data) override {
        if (fd_ == -1) {
            return std::make_error_code(std::errc::bad_file_descriptor);
        }
        uvc_xu_control_query query_struct = {
            unit,
            selector,
            query,
            static_cast<uint16_t>(data.size()),
            data.data()
        };

        if (ioctl(fd_, UVCIOC_CTRL_QUERY, &query_struct) == -1) {
            return std::error_code(errno, std::generic_category());
        }
        return {};
    }
    
    template <size_t N>
    std::error_code io(uint8_t unit, uint8_t selector, uint8_t query, std::array<uint8_t, N> data) {
        std::vector<uint8_t> data_vector(data.begin(), data.end());
        return io(unit, selector, query, data_vector);
    }

    int fd() const { return fd_; }

private:
    int fd_;
};

class CameraError : public std::system_error {
public:
    explicit CameraError(const std::string& message)
        : std::system_error(std::make_error_code(std::errc::no_such_device), message) {}
};

CameraHandle open_camera(const std::string& hint);

// void close_camera(CameraHandle& handle);

// void set_exposure(CameraHandle& handle, int exposure);


