# Magician Unified Data Grabber

<img src="https://github.com/magician-project/magician_grabber/blob/main/doc/logo.jpg?raw=true" height=200/> <img src="https://github.com/magician-project/magician_grabber/blob/main/doc/grabber.png?raw=true" height=200/>

A multi-modal data acquisition tool for robotic manipulation research. Synchronously captures and stores data from GigE cameras, ATI NetFT force/torque sensors, Teensy-based accelerometers, and Arduino-based distance/lighting controllers. Supports real-time streaming to shared memory, ROS 2 topic publishing, and on-the-fly tactile feature extraction.

---

## Table of Contents

- [Dependencies & Build](#dependencies--build)
- [Binaries](#binaries)
- [Command-Line Options](#command-line-options)
- [Usage Examples](#usage-examples)

---

## Dependencies & Build

The full dependency chain (including [ARAVIS](https://github.com/AravisProject/aravis) for the GigE camera) is handled by a single script:

```bash
scripts/build.sh
```

To recompile after source changes:

```bash
make
```

### Manual ARAVIS installation

If you prefer not to use `build.sh`, ARAVIS can be built from source:

```bash
git clone https://github.com/AravisProject/aravis
cd aravis
meson build
cd build
ninja
sudo ninja install
```

---

## Binaries

| Binary | Built with | Description |
|---|---|---|
| `magician_grabber` | `make` | Standalone grabber — camera, force, accelerometer, Arduino. |
| `magician_grabber_tactile` | `make` | Same as above plus real-time [tactile feature extraction](https://github.com/magician-project/magician_grabber/tree/main/tactile_processor). |
| `rclcpp_magician_grabber` | `colcon` | Grabber with [ROS 2 topic publishing](https://github.com/magician-project/magician_grabber/blob/main/ros_magician_grabber.cpp#L122). Place the package in your ROS 2 workspace and build with `colcon build`. |

All three binaries accept the same command-line parameters described below.

---

## Command-Line Options

Configuration is entirely through command-line flags:

```
./magician_grabber [OPTIONS]
```

### General

| Option | Description |
|---|---|
| `--help` | Print this option list and exit. |
| `--simulate` | Simulate all devices (for development/testing without hardware). |
| `--forever` | Run indefinitely (no time limit). |
| `--duration <sec>` / `--time <sec>` | Stop after `<sec>` seconds. |
| `--countdown <sec>` | Wait `<sec>` seconds (with optional TTS) before starting capture. |
| `--speak` | Announce countdown steps via TTS (`festival`). |
| `--silent` | Suppress all progress messages to stdout. |
| `--unixtime` | Use Unix epoch timestamps instead of human-readable ones. |
| `--rt` | Elevate process to real-time scheduling priority (requires privileges). |
| `--I_know_what_I_am_doing` | Unlock the maximum exposure guard (normally capped at 750 µs). |

### Output

| Option | Description |
|---|---|
| `-o <path>` / `--output <path>` | Write captured data to `<path>`. Created automatically if absent. |
| `--nooutput` | Disable file output (redirects to `/dev/null`). |
| `--ram` | Write to `tmpfs/` (RAM-backed filesystem). Recommended for frame rates above 10 Hz. |
| `--compress` | Save camera frames as `.png` instead of raw `.pnm`. |

### Camera

| Option | Description |
|---|---|
| `--camera` | Enable the GigE camera. |
| `--nocamera` | Disable the GigE camera. |
| `--size <w> <h>` | Set capture resolution in pixels. |
| `--exposure <µsec>` | Set exposure time in microseconds (max 750 µs without `--I_know_what_I_am_doing`). |
| `--gain <value>` | Set camera analogue gain. |
| `--blacklevel <value>` | Set camera black level. |
| `--fps <Hz>` | Set frame rate. Use `--ram` for rates above 10 Hz. |
| `--view` / `--viewer` | Launch the live viewer (also enables streaming and camera). |

### Sensors & Devices

| Option | Description |
|---|---|
| `--all` | Enable camera, Arduino, Teensy, and ATI force sensor simultaneously. |
| `--force` | Enable the ATI NetFT force/torque sensor. |
| `--atiip <ip>` | ATI NetFT sensor IP address (default: compiled-in value). |
| `--atiport <port>` | ATI NetFT sensor port number. |
| `--accelerometer` | Enable the Teensy-based accelerometer. |
| `--teensy <path>` | Serial port for the Teensy device (default: `/dev/ttyACM0`). |
| `--distance` | Enable the Arduino distance sensor. |
| `--arduino <path>` | Serial port for the Arduino device (default: `/dev/ttyUSB0`). |
| `--noarduino` | Disable the Arduino device. |
| `--features` | Enable real-time tactile feature computation (`magician_grabber_tactile` only). |

### Lighting

| Option | Description |
|---|---|
| `--trigger` | Manually trigger a light change after each captured frame. |
| `--notrigger` | Disable manual light triggering. |
| `--rlight` | Round-robin lighting pattern. |
| `--dlight` | Lighting intensity controlled by the distance sensor reading. |
| `--tlight` | Structured patterned lighting. |

### Keyboard

| Option | Description |
|---|---|
| `--kb` | Intercept keyboard input during capture. |
| `--nokb` | Disable keyboard interception. |

### Streaming (shared memory)

Streaming publishes frames to named POSIX shared-memory segments so that viewer processes or other nodes can consume them without file I/O.

| Option | Description |
|---|---|
| `--stream` | Enable streaming to shared memory (also enables camera and Arduino, disables file output). |
| `--camerastream <name>` | Shared memory stream name for camera frames (default: `stream1`). |
| `--tactilestream <name>` | Shared memory stream name for tactile frames (default: `stream_tactile`). |

Using distinct stream names lets you run multiple grabber instances simultaneously, each serving a different camera or tactile sensor to independent consumers:

```bash
# Camera A on stream "cam_left", camera B on stream "cam_right"
./magician_grabber --stream --camerastream cam_left  --size 1920 1080
./magician_grabber --stream --camerastream cam_right --size 1920 1080
```

---

## Usage Examples

Capture camera + force + accelerometer to a timestamped directory for 60 seconds:

```bash
./magician_grabber --camera --force --accelerometer --output dataset_run1 --time 60
```

Capture all devices with a 750 µs exposure:

```bash
./magician_grabber --all --output dataset_run1 --exposure 750 --I_know_what_I_am_doing --time 60
```

High frame-rate capture to RAM (avoids disk bottleneck):

```bash
./magician_grabber --camera --fps 30 --ram --output dataset_run1 --time 60
```

Stream camera to shared memory indefinitely (no files written):

```bash
./magician_grabber --stream --camera --forever
```

Stream tactile data (ATI + Teensy) to shared memory, no camera:

```bash
./magician_grabber_tactile --stream --accelerometer --force --nocamera --noarduino --atiip 192.168.1.1
```

Stream tactile data via ROS 2:

```bash
build/rclcpp_magician_grabber/magician_grabber --stream --accelerometer --force --nocamera --noarduino --atiip 192.168.1.1
```

Run two cameras simultaneously on separate streams:

```bash
./magician_grabber --stream --camerastream cam_left  --size 1920 1080 &
./magician_grabber --stream --camerastream cam_right --size 1920 1080 &
```

Print all available options:

```bash
./magician_grabber --help
```
