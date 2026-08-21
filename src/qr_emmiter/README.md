# QR Timecode Synchronization Utility

## Overview

This module provides a **QR-code–based time synchronization utility** designed to visually embed precise system timestamps into video streams. It is intended to be used alongside sensor logging tools (e.g., ATI force/torque logging) to enable **offline, frame-accurate synchronization** across heterogeneous data sources.

The utility generates dynamically updating QR codes that encode timestamps and optional metadata, which can be captured by standard cameras without requiring hardware triggers, genlock, or ROS integration.

---

## Purpose and Motivation

During experimental recordings (e.g., at Altinay and TOFAS sites), multiple cameras and sensors are often used simultaneously:

- Cameras (GoPro, Nikon, etc.) have independent clocks
- Some devices do not support external synchronization
- Wireless synchronization is unreliable or forbidden

This QR utility solves the problem by:

- displaying a **machine-readable timestamp** directly in the camera’s field of view,
- ensuring all modalities share a **common Unix-time reference**,
- enabling robust **post-hoc synchronization** during offline processing.

---

## Key Features

- Generates QR codes containing system timestamps
- Supports **millisecond or microsecond precision**
- Configurable update rate (Hz)
- Human-readable text overlay (optional)
- Designed for capture by rolling-shutter cameras
- Works with standard displays (HDMI monitor, tablet, laptop screen)
- Complements CSV-based sensor logging pipelines

---


## Build

```bash
sudo apt install build-essential libqrencode-dev
make
```



## Default Example Usage

```bash
./xqr_time_sync --ms --hz 30 --text --payload "t=%llu"
```

This configuration:
- encodes the Unix timestamp in milliseconds,
- updates the QR code at **30 Hz**,
- displays the timestamp as text,
- embeds the timestamp in the QR payload using the format `t=<timestamp>`.

---

## Typical Workflow

1. Run the QR utility on a laptop or tablet connected to a display.
2. Place the display within the field of view of all cameras.
3. Record video streams normally.
4. Simultaneously record sensor data (e.g., force/torque) using system timestamps.
5. During offline processing:
   - decode QR codes per video frame,
   - align video frames to sensor samples via Unix time.

---

## Design Philosophy

- **Visual synchronization first**: no special hardware required
- **Deterministic timestamps**: system clock as the single source of truth
- **Offline-friendly**: optimized for post-processing pipelines
- **Minimal dependencies**: simple C / JavaScript implementation

This approach has proven robust in environments where tight hardware synchronization is impractical.

---

## Notes

- Actual effective update rate is limited by display refresh and camera frame rate.
- Rolling shutter effects should be considered during decoding.
- For best results, use a high-contrast display and avoid motion blur.

---

## Intended Use

This utility is intended for **research and experimental data acquisition**, particularly for:
- multimodal dataset creation,
- human–robot interaction experiments,
- manipulation and tool-use studies,
- synchronization of video, force, and pose data.

---

## License / Usage

This code is intended for **research and experimental use**.
