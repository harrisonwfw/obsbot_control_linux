workspace(name = "camera_control")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

# Protocol Buffers
# http_archive(
#     name = "com_google_protobuf",
#     sha256 = "d0f5f605d0d656007ce6c8b5a82df3037e1d8fe8b121ed42e536f569dec16113",
#     strip_prefix = "protobuf-3.14.0",
#     urls = [
#         "https://github.com/protocolbuffers/protobuf/archive/v3.14.0.tar.gz",
#     ],
# )

# load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
# protobuf_deps()

# gRPC
# http_archive(
#     name = "com_github_grpc_grpc",
#     sha256 = "d6277f77e0bb922d3f6f56c0f93292bb4cfabfc3c92b31ee5ccea0e100303612",
#    strip_prefix = "grpc-1.38.0",
#    urls = [
#        "https://github.com/grpc/grpc/archive/v1.38.0.tar.gz",
#    ],
# )

# load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
# grpc_deps()

# load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")
# grpc_extra_deps()

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "gtest",
    urls = ["https://github.com/google/googletest/archive/release-1.11.0.tar.gz"],
    strip_prefix = "googletest-release-1.11.0",
)

# OpenCV
git_repository(
    name = "opencv",
    remote = "https://github.com/opencv/opencv.git",
    tag = "4.5.2",
)

new_local_repository(
    name = "qt",
    path = "/usr/include/x86_64-linux-gnu/qt6",
    build_file = "BUILD.qt"
)