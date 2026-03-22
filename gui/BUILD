load("//tools/build_rules:qt.bzl", "qt_cc_library")

package(default_visibility = ["//visibility:public"])


qt_cc_library(
    name = "gui",
    src = "gui.cc",
    hdr = "gui.hh",
    deps = [
        "//libs:camera_control",
        "//usbio:usbio",
        "@qt//:qt_widgets",
    ],
)

cc_binary(
    name = "camera_control_gui",
    srcs = ["camera_control_gui.cc"],
    deps = [
        ":gui",
    ],
)


cc_test(
    name = "camera_control_gui_test",
    srcs = ["camera_control_gui_test.cc"],
    deps = [
        ":camera_control_gui",
    ],
)