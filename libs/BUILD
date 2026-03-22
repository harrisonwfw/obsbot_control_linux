package(default_visibility = ["//visibility:public"])

cc_library(
    name = "camera_control",
    hdrs = ["camera_control.hh"],
    deps = ["//usbio:usbio"],
)

cc_test(
    name = "camera_control_test",
    srcs = ["camera_control_test.cc"],
    deps = [
        ":camera_control",
        "@gtest//:gtest_main",
        "@gtest//:gtest",
    ],
)

