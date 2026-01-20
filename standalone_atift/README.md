# ATI NetFT to CSV Logger

## Overview

This module is a lightweight C utility designed to **capture force/torque measurements from an ATI Net F/T sensor over UDP and store them in CSV format with precise system timestamps**.

The primary purpose of this utility is to support **experimental data acquisition and multimodal synchronization** during on-site recordings at **Altinay** and **TOFAS**, where force/torque data must be aligned with video streams, QR/ArUco visual timestamps, and other sensor modalities.

## Purpose and Motivation

During the Altinay–TOFAS integration and data capture sessions, force/torque data from an ATI sensor must be:

- acquired reliably over Ethernet (UDP),
- timestamped using the **system Unix time**,
- stored in a **simple, portable, lossless CSV format**,
- easily post-processed and synchronized with:
  - multiple video streams (GoPro, Nikon, etc.),
  - visually embedded QR-code timestamps,
  - offline pose estimation and marker tracking pipelines.

This tool was created to fulfill these requirements with **minimal dependencies**, **maximum transparency**, and **full timestamp precision**.

## Key Features

- Direct UDP communication with ATI Net F/T sensors
- Logs data for a **user-defined duration**
- Appends data safely to CSV
- Uses **Unix epoch timestamps in microseconds**
- Stores both:
  - system timestamp (for synchronization)
  - sensor timestamp (`ft_sequence`)
- Suitable for **high-frequency acquisition** (device/network-limited)
- No ROS, MATLAB, or external frameworks required

## CSV Output Format

Each row in the output CSV has the following format:

```
unixtimestamp_us,timestamp,Fx,Fy,Fz,Tx,Ty,Tz
```

Example:
```
1706012345123456,1522,3.654894,4.030689,-5.880650,-0.008954,-0.052791,0.044117
```


## Compile

```bash
make
```


## Usage

```bash
./atiToCSV IP PORT OUTPUT.csv DURATION_SECONDS
```

Example:
```bash
./atiToCSV 192.168.1.1 49152 force_data.csv 30
```

## Intended Use

This utility is intended for **on-site experimental recordings** where force/torque data must be synchronized with video streams and other sensors using a shared Unix timestamp.

## License / Usage

This code is intended for **research and experimental use**.
