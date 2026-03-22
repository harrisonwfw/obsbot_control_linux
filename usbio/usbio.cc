// Copyright 2025 Harrison W.

#include "usbio.hh"

#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <climits>
#include <optional>
#include <glob.h>
#include <cstring>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>

namespace {

int video_index_from_path(const char* path) {
    const char* base = std::strrchr(path, '/');
    base = base ? base + 1 : path;
    if (std::strncmp(base, "video", 5) != 0) {
        return -1;
    }
    return std::atoi(base + 5);
}

/** v4l2_capability fields are fixed-size; stop at first NUL so matching is reliable. */
std::string sanitize_v4l_field(const std::string& raw) {
    const size_t z = raw.find('\0');
    std::string s = z == std::string::npos ? raw : raw.substr(0, z);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
        s.pop_back();
    }
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
        ++i;
    }
    return s.substr(i);
}

std::string trim_hint(std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
        s.erase(0, 1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
        s.pop_back();
    }
    return s;
}

std::string ascii_lower(std::string s) {
    for (char& ch : s) {
        auto c = static_cast<unsigned char>(ch);
        if (c >= 'A' && c <= 'Z') {
            ch = static_cast<char>(c - 'A' + 'a');
        }
    }
    return s;
}

bool ascii_iequals(const std::string& a, const std::string& b) {
    return ascii_lower(a) == ascii_lower(b);
}

bool icase_field_contains(const std::string& field_raw, const std::string& needle_trimmed) {
    if (needle_trimmed.empty()) {
        return false;
    }
    const std::string field = ascii_lower(sanitize_v4l_field(field_raw));
    const std::string needle = ascii_lower(needle_trimmed);
    return field.find(needle) != std::string::npos;
}

bool driver_is_uvc(const std::string& driver_raw) {
    const std::string d = ascii_lower(sanitize_v4l_field(driver_raw));
    return d.find("uvc") != std::string::npos;
}

struct VideoDevProbe {
    std::string path;
    int index{};
    bool has_capture{};
    std::string card_raw;
    std::string bus_raw;
    std::string driver_raw;
};

/** Prefer capture nodes (PTZ / V4L2 controls live there), lowest /dev/videoN first when idle.
 *  Non-capture nodes (metadata, IR, etc.) come last, higher index first — for when another app
 *  holds /dev/video0 and only a secondary node is openable. */
void sort_probes_for_open_priority(std::vector<VideoDevProbe>& probes) {
    std::sort(probes.begin(), probes.end(), [](const VideoDevProbe& a, const VideoDevProbe& b) {
        if (a.has_capture != b.has_capture) {
            return a.has_capture > b.has_capture;
        }
        if (a.has_capture) {
            const int ai = a.index >= 0 ? a.index : INT_MAX;
            const int bi = b.index >= 0 ? b.index : INT_MAX;
            return ai < bi;
        }
        const int ai = a.index >= 0 ? a.index : -1;
        const int bi = b.index >= 0 ? b.index : -1;
        return ai > bi;
    });
}

}  // namespace

CameraHandle open_camera(const std::string& hint) {
    int fd = open(hint.c_str(), O_RDWR);
    if (fd != -1) {
        close(fd);
        return CameraHandle(hint);
    }
    std::string dev_path = "/dev/" + hint;
    fd = open(dev_path.c_str(), O_RDWR);
    if (fd != -1) {
        close(fd);
        return CameraHandle(dev_path);
    }

    glob_t glob_result {};
    const int glob_status = glob("/dev/video*", GLOB_TILDE, nullptr, &glob_result);
    if (glob_status != 0 && glob_status != GLOB_NOMATCH) {
        globfree(&glob_result);
        throw CameraError("No camera found");
    }

    // Copy paths before globfree — gl_pathv is invalidated by globfree.
    std::vector<std::string> paths;
    if (glob_status == 0) {
        paths.reserve(glob_result.gl_pathc);
        for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
            paths.emplace_back(glob_result.gl_pathv[i]);
        }
    }
    globfree(&glob_result);

    std::vector<VideoDevProbe> probes;
    probes.reserve(paths.size());
    for (const std::string& path : paths) {
        const int pfd = open(path.c_str(), O_RDWR);
        if (pfd == -1) {
            continue;
        }
        struct v4l2_capability cap {};
        if (ioctl(pfd, VIDIOC_QUERYCAP, &cap) == -1) {
            close(pfd);
            continue;
        }
        VideoDevProbe p;
        p.path = path;
        p.index = video_index_from_path(path.c_str());
        p.has_capture = (v4l2_effective_caps(cap) & V4L2_CAP_VIDEO_CAPTURE) != 0;
        p.card_raw = convert_to_string(cap.card, sizeof(cap.card));
        p.bus_raw = convert_to_string(cap.bus_info, sizeof(cap.bus_info));
        p.driver_raw = convert_to_string(cap.driver, sizeof(cap.driver));
        probes.push_back(std::move(p));
        close(pfd);
    }

    if (probes.empty()) {
        throw CameraError("No camera found");
    }

    sort_probes_for_open_priority(probes);

    const std::string hint_trim = trim_hint(hint);
    const bool auto_pick =
        hint_trim.empty() || ascii_iequals(hint_trim, "auto") || ascii_iequals(hint_trim, "*");

    auto try_open_path = [](const std::string& path) -> bool {
        const int tfd = open(path.c_str(), O_RDWR);
        if (tfd == -1) {
            return false;
        }
        close(tfd);
        return true;
    };

    for (const VideoDevProbe& p : probes) {
        if (auto_pick && !p.has_capture) {
            continue;
        }
        if (!try_open_path(p.path)) {
            continue;
        }

        std::cout << "Info: Card: " << sanitize_v4l_field(p.card_raw) << "\nBus:  " << sanitize_v4l_field(p.bus_raw)
                  << "\nDriver: " << sanitize_v4l_field(p.driver_raw) << "\nPath: " << p.path.c_str()
                  << (p.has_capture ? "" : " (no VIDEO_CAPTURE)") << std::endl;

        const bool name_match =
            auto_pick || icase_field_contains(p.card_raw, hint_trim) || icase_field_contains(p.bus_raw, hint_trim);

        if (name_match) {
            if (!p.has_capture) {
                std::cerr
                    << "open_camera: \"" << hint_trim << "\" matched " << p.path
                    << " but it has no VIDEO_CAPTURE — V4L2 pan/tilt usually need the primary capture node "
                       "(often /dev/video0). Close apps using that node or set CAMERA_CONTROL_HINT to it.\n";
            }
            return CameraHandle(p.path);
        }
    }

    for (const VideoDevProbe& p : probes) {
        if (!p.has_capture || !driver_is_uvc(p.driver_raw)) {
            continue;
        }
        if (!try_open_path(p.path)) {
            continue;
        }
        std::cerr << "open_camera: hint \"" << hint_trim
                  << "\" matched no device name; using UVC capture node " << p.path << std::endl;
        return CameraHandle(p.path);
    }

    for (const VideoDevProbe& p : probes) {
        if (!p.has_capture) {
            continue;
        }
        if (!try_open_path(p.path)) {
            continue;
        }
        std::cerr << "open_camera: using first available capture device " << p.path << std::endl;
        return CameraHandle(p.path);
    }

    for (const VideoDevProbe& p : probes) {
        if (!try_open_path(p.path)) {
            continue;
        }
        std::cerr << "open_camera: only non-capture nodes available for \"" << hint_trim << "\" — opened "
                  << p.path << " (pan/tilt via V4L2 may not work; close other apps using the main video node).\n";
        return CameraHandle(p.path);
    }

    throw CameraError("No camera found");
}