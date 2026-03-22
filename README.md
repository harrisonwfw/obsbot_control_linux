# Camera Control

A Qt 6 desktop application for controlling an **OBSBOT-style** UVC camera on **Linux** over **Video4Linux2** (`/dev/video*`). It uses V4L2 for streaming (optional) and for pan, tilt, zoom, image controls, and OBSBOT extension-unit commands (AI tracking modes).

## Features

- **Pan, tilt, zoom** and standard image controls (brightness, contrast, etc.) via V4L2.
- **AI / tracking mode** selection using UVC extension-unit traffic (unit 2 / selector 6) aligned with this project’s status decoding.
- **Live preview** (YUYV → RGB) when the app can allocate capture buffers and start streaming.
- **Hybrid startup**: reads the camera when possible, otherwise falls back to **QSettings**; startup source counts are shown in the UI.
- **Hardware → UI sync**: a timer periodically refreshes sliders and the AI combo from `try_get_*` reads, skipping sliders while you drag them.
- **Shared use with other apps**: if another program (e.g. Google Meet) is already streaming, the app can still **attach for control** without local preview when the driver allows it (`set_format` may return `EBUSY`; buffers are skipped).
- **Release device**: closes the app’s V4L2 handle so another app can open the camera, while the app keeps running and **retries** attachment on a timer.
- **Robust device discovery**: probes `/dev/video*`, matches card/bus names reliably (fixed-size V4L2 fields, case-insensitive substring match), sorts nodes so **capture-capable** devices are preferred with **lower `/dev/videoN` first** for full preview + PTZ when the camera is free; fallbacks exist for name mismatches and edge cases.

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

## Running and configuration

### `CAMERA_CONTROL_HINT`

The GUI reads **`CAMERA_CONTROL_HINT`** from the environment (default string: `OBSBOT Tiny`). It is used to pick which `/dev/video*` node belongs to your camera:

- If the value is a path that opens (e.g. `/dev/video0` or `video0` resolved under `/dev/`), that node is used.
- Otherwise all `/dev/video*` devices are probed; names are matched against the V4L2 **card** and **bus_info** strings (case-insensitive, substring).
- Special values **`auto`**, **`*`**, or an **empty** hint (after trimming) mean: choose the first suitable **video capture** node in the sorted probe order.

Example:

```bash
CAMERA_CONTROL_HINT=/dev/video0 ./build/gui/camera-control
```

### Device selection order (summary)

1. Openable devices are probed; each node’s **VIDEO_CAPTURE** capability and metadata are recorded.
2. **Capture nodes** are tried before non-capture (metadata/auxiliary) nodes.
3. Among capture nodes, **lower** `/dev/videoN` is preferred first so the primary stream node (with PTZ controls) is used when nothing else holds the device.
4. If the hint matches no name, the code can fall back to a **uvcvideo** capture node, then any capture node, then (with a warning) a non-capture node where V4L2 PTZ may not work.

### Using the camera while another app is streaming

On Linux, behavior depends on the driver:

- Often a **second open** of the same `/dev/video*` is possible for **control** while another app streams; **`VIDIOC_S_FMT`** may then fail with **EBUSY**. This app treats that as **control-only**: no mmap, no in-app preview, **Start/Stop** disabled, but sliders and AI mode remain enabled when ioctls succeed.
- If **`applyHybridSettings`** cannot push some values at startup (e.g. device busy), the error is logged and the connection is **kept** so you can still try the controls manually.
- True simultaneous **streaming** in two apps from the same node is usually not supported; use **Release device** in this app if you need to hand the capture handle to a browser, then let the app reconnect when the device is free.

### Timers

- **Reconnect**: if the camera is not attached, the app retries opening it every few seconds.
- **Hardware poll**: periodically refreshes the UI from the camera without triggering save debounce while dragging sliders.

### Persisted settings

Application and camera-related values are stored via **Qt `QSettings`** (organization `camera-control`, application `Camera Control`). Pan/tilt/zoom, image sliders, and AI mode are saved with debouncing when you change controls.

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
| `libs/` | Camera command/status logic (`Camera`, UVC helpers, V4L2 control IDs) |
| `usbio/` | V4L2 open/discovery (`open_camera`), `CameraHandle`, `set_format`, capability helpers |

## Bazel

A `WORKSPACE` and `BUILD` files exist for Bazel, but the Bazel graph is **not complete** in this repository (e.g. missing Qt rule support under `tools/build_rules/`). Use **CMake** for builds unless you restore those pieces.

## License

See file headers in the source (e.g. `Copyright 2025 Harrison W.` in `gui/gui.cc`).
