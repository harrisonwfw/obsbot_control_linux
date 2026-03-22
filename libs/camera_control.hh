// Copyright 2025 Harrison W.

#pragma once

#include "usbio/usbio.hh"

#include <optional>
#include <iostream>
#include <vector>
#include <array>
#include <stdexcept>
#include <string>
#include <optional>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <linux/videodev2.h>
#include <sys/ioctl.h>

constexpr uint8_t AUTO_EXP_CMD[18] = {0xaa, 0x25, 0x16, 0x00, 0x0c, 0x00, 0x58, 0x91, 0x0a, 0x02, 0x82, 0x29, 0x05, 0x00, 0xb2, 0xaf, 0x02, 0x04};
constexpr uint8_t MANUAL_EXP_CMD[18] = {0xaa, 0x25, 0x15, 0x00, 0x0c, 0x00, 0xa8, 0x9e, 0x0a, 0x02, 0x82, 0x29, 0x05, 0x00, 0xf9, 0x27, 0x01, 0x32};
constexpr uint8_t EXPOSURE_GLOBAL_CMD[3] = {0x03, 0x01, 0x00};
constexpr uint8_t EXPOSURE_FACE_CMD[3] = {0x03, 0x01, 0x01};

#define OBSBOT_PAN_ABSOLUTE 0x009a0908
#define OBSBOT_TILT_ABSOLUTE 0x009a0909
#define OBSBOT_ZOOM_ABSOLUTE 0x009a090d
#define OBSBOT_PAN_SPEED 0x009a0920
#define OBSBOT_TILT_SPEED 0x009a0921
#define OBSBOT_BRIGHTNESS 0x00980900
#define OBSBOT_CONTRAST 0x00980901
#define OBSBOT_SATURATION 0x00980902
#define OBSBOT_HUE 0x00980903
#define OBSBOT_SHARPNESS 0x0098091b
#define OBSBOT_GAIN 0x00980913
#define OBSBOT_WHITE_BALANCE_TEMPERATURE 0x0098091a
#define OBSBOT_BACKLIGHT_COMPENSATION 0x0098091c

enum class AIMode {
    NoTracking,
    NormalTracking,
    UpperBody,
    CloseUp,
    Headless,
    LowerBody,
    DeskMode,
    Whiteboard,
    Hand,
    Group,
};

enum class ExposureMode {
    Manual,
    Global,
    Face
};

enum class TrackingMode {
    Headroom,
    Standard,
    Motion,
};

class CameraStatus {
public:
    AIMode ai_mode;
    bool hdr_on;

    static CameraStatus decode(const std::vector<uint8_t>& bytes) {
        if (bytes.size() < 28) {
            throw std::runtime_error("Insufficient data to decode CameraStatus");
        }

        uint8_t m = bytes[0x18];
        uint8_t n = bytes[0x1c];

        AIMode ai_mode;
        switch (m) {
            case 0:
                ai_mode = AIMode::NoTracking;
                break;
            case 2:
                switch (n) {
                    case 0: ai_mode = AIMode::NormalTracking; break;
                    case 1: ai_mode = AIMode::UpperBody; break;
                    case 2: ai_mode = AIMode::CloseUp; break;
                    case 3: ai_mode = AIMode::Headless; break;
                    case 4: ai_mode = AIMode::LowerBody; break;
                    default: throw std::runtime_error("Invalid AI mode");
                }
                break;
            case 5:
                ai_mode = AIMode::DeskMode;
                break;
            case 4:
                ai_mode = AIMode::Whiteboard;
                break;
            case 6:
                ai_mode = AIMode::Hand;
                break;
            case 1:
                ai_mode = AIMode::Group;
                break;
            default: {
                if (m > 6) {
                    ai_mode = AIMode::NoTracking;
                } else {
                    throw std::runtime_error("Invalid AI mode");
                }
            }
        }

        bool hdr_on = bytes[0x6] != 0;

        return CameraStatus{ai_mode, hdr_on};
    }
};


class OBSBotWebCam {
    virtual bool set_ai_mode(AIMode mode) = 0;
    virtual std::optional<AIMode> get_ai_mode() = 0;
    virtual bool set_hdr_mode(bool mode) = 0;
    virtual bool set_exposure_mode(ExposureMode mode) = 0;
};

std::string hex_encode(const uint8_t* data, size_t size) {
    std::ostringstream oss;
    for (size_t i = 0; i < size; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    return oss.str();
}

class Camera {
public:
    CameraHandle handle;

    Camera(const std::string& hint) : handle(open_camera(hint)) {}

    std::error_code info() {
        return handle.info();
    }

    CameraStatus get_status() {
        std::array<uint8_t, 60> data;
        get_cur(0x2, 0x6, data);
        return CameraStatus::decode(std::vector<uint8_t>(data.begin(), data.end()));
    }

    void set_ai_mode(AIMode mode) {
        std::array<uint8_t, 4> cmd;
        switch (mode) {
            case AIMode::NoTracking: cmd = {0x16, 0x02, 0x00, 0x00}; break;
            case AIMode::NormalTracking: cmd = {0x16, 0x02, 0x02, 0x00}; break;
            case AIMode::UpperBody: cmd = {0x16, 0x02, 0x02, 0x01}; break;
            case AIMode::DeskMode: cmd = {0x16, 0x02, 0x05, 0x00}; break;
            case AIMode::Whiteboard: cmd = {0x16, 0x02, 0x04, 0x00}; break;
            case AIMode::Group: cmd = {0x16, 0x02, 0x01, 0x00}; break;
            case AIMode::Hand: cmd = {0x16, 0x02, 0x03, 0x00}; break;
            case AIMode::CloseUp: cmd = {0x16, 0x02, 0x02, 0x02}; break;
            case AIMode::Headless: cmd = {0x16, 0x02, 0x02, 0x03}; break;
            case AIMode::LowerBody: cmd = {0x16, 0x02, 0x02, 0x04}; break;
            default: throw std::runtime_error("Invalid AI mode");
        }
        send_cmd(0x2, 0x6, cmd);
    }

    void set_exposure_mode(ExposureMode mode) {
        switch (mode) {
            case ExposureMode::Manual:
                send_cmd(0x2, 0x2, MANUAL_EXP_CMD);
                break;
            case ExposureMode::Global:
                send_cmd(0x2, 0x2, AUTO_EXP_CMD);
                send_cmd(0x2, 0x6, EXPOSURE_GLOBAL_CMD);
                break;
            case ExposureMode::Face:
                send_cmd(0x2, 0x2, AUTO_EXP_CMD);
                send_cmd(0x2, 0x6, EXPOSURE_FACE_CMD);
                break;
        }
    }

    void set_hdr_mode(bool mode) {
        std::array<uint8_t, 3> cmd;
        switch (mode) {
            case true: cmd = {0x01, 0x01, 0x01}; break;
            case false: cmd = {0x01, 0x01, 0x00}; break;
        }
        send_cmd(0x2, 0x6, cmd);
    }

    AIMode get_ai_mode() {
        return get_status().ai_mode;
    }

    int getPan() {
        return getControl(OBSBOT_PAN_ABSOLUTE);
    }

    void setPan(int value) {
        setControl(OBSBOT_PAN_ABSOLUTE, value);
    }

    int getTilt() {
        return getControl(OBSBOT_TILT_ABSOLUTE);
    }

    void setTilt(int value) {
        setControl(OBSBOT_TILT_ABSOLUTE, value);
    }

    int getZoom() {
        return getControl(OBSBOT_ZOOM_ABSOLUTE);
    }

    void setZoom(int value) {
        setControl(OBSBOT_ZOOM_ABSOLUTE, value);
    }

    int getPanSpeed() {
        return getControl(OBSBOT_PAN_SPEED);
    }

    int getTiltSpeed() {
        return getControl(OBSBOT_TILT_SPEED);
    }

    void setBrightness(int value) {
        setControl(OBSBOT_BRIGHTNESS, value);
    }

    void setContrast(int value) {
        setControl(OBSBOT_CONTRAST, value);
    }

    void setSaturation(int value) {
        setControl(OBSBOT_SATURATION, value);
    }

    void setHue(int value) {
        setControl(OBSBOT_HUE, value);
    }

    void setSharpness(int value) {
        setControl(OBSBOT_SHARPNESS, value);
    }

    void setGain(int value) {
        setControl(OBSBOT_GAIN, value);
    }

    void setWhiteBalanceTemperature(int value) {
        setControl(OBSBOT_WHITE_BALANCE_TEMPERATURE, value);
    }

    void setBacklightCompensation(int value) {
        setControl(OBSBOT_BACKLIGHT_COMPENSATION, value);
    }


private:
    template <size_t N>
    void send_cmd(uint8_t unit, uint8_t selector, const std::array<uint8_t, N>& cmd) {
        std::array<uint8_t, 60> data = {};
        std::copy(cmd.begin(), cmd.end(), data.begin());
        set_cur(unit, selector, data);
    }

    void send_cmd(uint8_t unit, uint8_t selector, const uint8_t* cmd, size_t len = 18) {
        std::array<uint8_t, 60> data = {};
        // Copy the command to the data array
        std::copy(cmd, cmd + len, data.begin());
        set_cur(unit, selector, data);
    }

    std::error_code get_cur(uint8_t unit, uint8_t selector, std::array<uint8_t, 60>& data) {
        // always call get_len first
        auto size_result = get_len(unit, selector);
        if (!size_result) {
            return std::make_error_code(std::errc::bad_message);
        }
        size_t size = size_result.value();

        if (data.size() < size) {
            return std::make_error_code(std::errc::no_buffer_space);
        }

        // Perform the IO operation
        return io(unit, selector, UVC_GET_CUR, data);
    }

    std::error_code set_cur(uint8_t unit, uint8_t selector, std::array<uint8_t, 60>& data) {
        auto size_result = get_len(unit, selector);
        if (!size_result) {
            return std::make_error_code(std::errc::bad_message);
        }
        size_t size = size_result.value();

        if (data.size() > size) {
            return std::make_error_code(std::errc::no_buffer_space);
        }

        std::cout << static_cast<int>(unit) << " " << static_cast<int>(selector) << " " 
                  << hex_encode(data.data(), data.size()) << std::endl;

        // Perform the IO operation
        return io(unit, selector, UVC_SET_CUR, data);
    }

    std::optional<size_t> get_len(uint8_t unit, uint8_t selector) {
        std::array<uint8_t, 2> data = {};

        // Perform the IO operation to get length
        auto result = io(unit, selector, UVC_GET_LEN, data);
        if (result) {
            return std::nullopt;
        }

        return static_cast<size_t>(data[0]) | (static_cast<size_t>(data[1]) << 8);
    }

    template <size_t N>
    std::error_code io(uint8_t unit, uint8_t selector, uint8_t query, std::array<uint8_t, N>& data) {
        return handle.io(unit, selector, query, data);
    }

    int getControl(__u32 id) {
        struct v4l2_control control = {0};
        control.id = id;
        if (ioctl(handle.fd(), VIDIOC_G_CTRL, &control) == -1) {
            throw std::runtime_error("Failed to get control: " + std::to_string(id));
        }
        return control.value;
    }

    void setControl(__u32 id, int value, bool switch_mode = false) {
        struct v4l2_control control = {0};
        control.id = id;
        control.value = value;
        if (switch_mode) {
            if (ioctl(handle.fd(), VIDIOC_G_CTRL, &control) == -1) {
                throw std::runtime_error("Failed to set control: " + std::to_string(id));
            }
        }
        else {
            if (ioctl(handle.fd(), VIDIOC_S_CTRL, &control) == -1) {
                throw std::runtime_error("Failed to set control: " + std::to_string(id));
            }
        }
    }
};

