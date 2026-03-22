# Camera Control

A Qt 6 desktop application for controlling a camera over USB on **Linux**, using **Video4Linux2** (`/dev/video*`) for capture and a small protocol layer for device commands.

## Requirements

- **CMake** 3.10 or newer  
- **C++17** compiler (GCC or Clang)  
- **Qt 6** with: Core, Widgets, Multimedia, MultimediaWidgets  
- **Linux** with V4L2 (typical desktop kernel)  
- **GoogleTest** (development packages), only if you build the test executables

On Debian/Ubuntu, installing Qt and build tools usually looks like:

```bash
sudo apt install cmake build-essential qt6-base-dev qt6-multimedia-dev \
  libgtest-dev
```

Exact package names can differ slightly by distribution.

## Build (CMake)

Configure and build from the repository root:

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

If you do not set `CMAKE_BUILD_TYPE`, CMake defaults this project to **Release** for single-config generators (Make/Ninja).

The GUI executable is named **`camera-control`** (CMake target name is still `gui`):

```bash
./build/gui/camera-control
```

## Install

Installs the application under your chosen prefix (default `/usr/local`):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
sudo cmake --install build
```

This installs:

- `bin/camera-control`
- `share/applications/camera-control.desktop`

To use a custom prefix:

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/opt/camera-control
```

Staging install (e.g. for packaging):

```bash
DESTDIR=/tmp/stage cmake --install build
```

The binary links against **system Qt 6** and other shared libraries. Machines where you run it need compatible Qt/runtime libraries installed, unless you bundle dependencies separately (e.g. with a Qt deployment tool).

## Tests

The tree includes `usbio_test` and `camera_control_test` executables built by CMake. They are not registered with CTest in the current CMake files; run them manually from the build tree, for example:

```bash
./build/usbio/usbio_test
./build/libs/camera_control_test
```

## Project layout

| Path | Role |
|------|------|
| `gui/` | Qt application UI and main entry used by CMake |
| `libs/` | Camera command/status logic |
| `usbio/` | Low-level I/O and V4L2-oriented helpers |

## Bazel

A `WORKSPACE` and `BUILD` files exist for Bazel, but the Bazel graph is **not complete** in this repository (e.g. missing Qt rule support under `tools/build_rules/`). Use **CMake** for builds unless you restore those pieces.

## License

See file headers in the source (e.g. `Copyright 2025 Harrison W.` in `gui/gui.cc`).
