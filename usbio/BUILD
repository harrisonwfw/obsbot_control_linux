package(default_visibility = ["//visibility:public"])

cc_library(
    name = "usbio",
    srcs = ["usbio.cc"],
    hdrs = ["usbio.hh"],
)

cc_test(
    name = "usbio_test",
    srcs = ["usbio_test.cc"],
    deps = [
        "//usbio",
        "@gtest//:gtest",
        "@gtest//:gtest_main",
    ],
)

