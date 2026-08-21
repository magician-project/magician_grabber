#!/usr/bin/env python3
"""
CSV -> Force/Torque time plot -> MP4

Requirements:
  pip install matplotlib numpy
  (and FFmpeg installed so Matplotlib can write mp4)

Usage:
  python plot_ft_csv_to_mp4.py input.csv -o out.mp4 --fps 60
"""

import argparse
import csv
import os
from typing import List, Dict

import numpy as np
import matplotlib
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation, FFMpegWriter


COLS_REQUIRED = [
    "unixtimestamp_us", "atitimestamp",
    "Fx", "Fy", "Fz",
    "Tx", "Ty", "Tz",
]


def read_csv_no_pandas(path: str) -> Dict[str, np.ndarray]:
    data: Dict[str, List[float]] = {k: [] for k in COLS_REQUIRED}

    with open(path, "r", newline="") as f:
        reader = csv.DictReader(f)
        if reader.fieldnames is None:
            raise ValueError("CSV has no header row.")
        missing = [c for c in COLS_REQUIRED if c not in reader.fieldnames]
        if missing:
            raise ValueError(f"Missing required columns: {missing}\nFound: {reader.fieldnames}")

        for row in reader:
            # Parse as float for numeric stability; timestamps are large, but still safe in float64.
            for k in COLS_REQUIRED:
                data[k].append(float(row[k]))

    # Convert to numpy
    out = {k: np.asarray(v, dtype=np.float64) for k, v in data.items()}

    # Time in milliseconds on X axis:
    # Prefer unixtimestamp_us (microseconds) -> ms and normalize to start at 0.
    t_ms = (out["unixtimestamp_us"] - out["unixtimestamp_us"][0]) / 1000.0
    out["t_ms"] = t_ms
    return out


def make_animation_mp4(
    t_ms: np.ndarray,
    fx: np.ndarray, fy: np.ndarray, fz: np.ndarray,
    tx: np.ndarray, ty: np.ndarray, tz: np.ndarray,
    out_mp4: str,
    fps: int = 60,
    dpi: int = 150,
    realtime: bool = False,
) -> None:
    """
    Writes an MP4 animation that reveals data over time.

    realtime=False:
      - Uses one frame per sample (good when sample count isn't huge).
    realtime=True:
      - Maps time to fps, producing duration ~= (t_ms[-1]/1000) seconds.
    """
    if len(t_ms) < 2:
        raise ValueError("Need at least 2 samples to plot/animate.")

    # Determine frames
    if realtime:
        duration_s = float(t_ms[-1] - t_ms[0]) / 1000.0
        nframes = max(2, int(np.ceil(duration_s * fps)))
        frame_t = np.linspace(t_ms[0], t_ms[-1], nframes)
        # For each frame time, find last index <= that time
        idx_for_frame = np.searchsorted(t_ms, frame_t, side="right") - 1
        idx_for_frame = np.clip(idx_for_frame, 0, len(t_ms) - 1)
    else:
        nframes = len(t_ms)
        idx_for_frame = np.arange(nframes, dtype=np.int64)

    fig, (axF, axT) = plt.subplots(2, 1, sharex=True, figsize=(12, 7))
    fig.suptitle("Force / Torque vs Time")

    # Precompute y-limits with a small margin
    F_all = np.concatenate([fx, fy, fz])
    T_all = np.concatenate([tx, ty, tz])

    def limits(arr: np.ndarray):
        mn = float(np.min(arr))
        mx = float(np.max(arr))
        if mn == mx:
            pad = 1.0 if mn == 0.0 else abs(mn) * 0.1
        else:
            pad = (mx - mn) * 0.05
        return mn - pad, mx + pad

    axF.set_ylabel("Force")
    axT.set_ylabel("Torque")
    axT.set_xlabel("Time (ms)")

    axF.set_ylim(*limits(F_all))
    axT.set_ylim(*limits(T_all))
    axF.grid(True, alpha=0.3)
    axT.grid(True, alpha=0.3)

    # Empty line objects to update
    (l_fx,) = axF.plot([], [], label="Fx")
    (l_fy,) = axF.plot([], [], label="Fy")
    (l_fz,) = axF.plot([], [], label="Fz")
    axF.legend(loc="upper right")

    (l_tx,) = axT.plot([], [], label="Tx")
    (l_ty,) = axT.plot([], [], label="Ty")
    (l_tz,) = axT.plot([], [], label="Tz")
    axT.legend(loc="upper right")

    # Optional vertical cursor line
    cursorF = axF.axvline(t_ms[0], linewidth=1)
    cursorT = axT.axvline(t_ms[0], linewidth=1)

    # Set x limits once
    axT.set_xlim(float(t_ms[0]), float(t_ms[-1]))

    def init():
        for l in (l_fx, l_fy, l_fz, l_tx, l_ty, l_tz):
            l.set_data([], [])
        cursorF.set_xdata([t_ms[0], t_ms[0]])
        cursorT.set_xdata([t_ms[0], t_ms[0]])
        return l_fx, l_fy, l_fz, l_tx, l_ty, l_tz, cursorF, cursorT

    def update(frame_i: int):
        idx = int(idx_for_frame[frame_i])
        x = t_ms[: idx + 1]

        l_fx.set_data(x, fx[: idx + 1])
        l_fy.set_data(x, fy[: idx + 1])
        l_fz.set_data(x, fz[: idx + 1])

        l_tx.set_data(x, tx[: idx + 1])
        l_ty.set_data(x, ty[: idx + 1])
        l_tz.set_data(x, tz[: idx + 1])

        curx = float(t_ms[idx])
        cursorF.set_xdata([curx, curx])
        cursorT.set_xdata([curx, curx])

        return l_fx, l_fy, l_fz, l_tx, l_ty, l_tz, cursorF, cursorT

    ani = FuncAnimation(
        fig,
        update,
        frames=nframes,
        init_func=init,
        interval=1000.0 / fps,
        blit=True,
    )

    # Ensure mp4 writer works
    if not out_mp4.lower().endswith(".mp4"):
        out_mp4 += ".mp4"

    writer = FFMpegWriter(fps=fps, metadata={"artist": "plot_ft_csv_to_mp4"})
    ani.save(out_mp4, writer=writer, dpi=dpi)
    plt.close(fig)


def main():
    p = argparse.ArgumentParser(description="Plot Force/Torque CSV to MP4 (no pandas).")
    p.add_argument("csv_path", help="Input CSV path.")
    p.add_argument("-o", "--out", default="ft_plot.mp4", help="Output MP4 path.")
    p.add_argument("--fps", type=int, default=60, help="Video FPS.")
    p.add_argument("--dpi", type=int, default=150, help="Output DPI.")
    p.add_argument(
        "--realtime",
        action="store_true",
        help="Match video duration to real time using timestamps (instead of 1 frame per sample).",
    )
    args = p.parse_args()

    if not os.path.isfile(args.csv_path):
        raise FileNotFoundError(args.csv_path)

    d = read_csv_no_pandas(args.csv_path)

    make_animation_mp4(
        t_ms=d["t_ms"],
        fx=d["Fx"], fy=d["Fy"], fz=d["Fz"],
        tx=d["Tx"], ty=d["Ty"], tz=d["Tz"],
        out_mp4=args.out,
        fps=args.fps,
        dpi=args.dpi,
        realtime=args.realtime,
    )

    print(f"Saved: {args.out}")


if __name__ == "__main__":
    # Use non-interactive backend for headless environments
    matplotlib.use("Agg")
    main()
