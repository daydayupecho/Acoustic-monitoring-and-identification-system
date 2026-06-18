#!/usr/bin/env python3
"""Convert the user's custom STM32/ST-HSD-style .dat folders to NanoEdge CSV.

Fixed behavior:
- One acquisition folder -> one CSV file
- If one .dat can be cut into many windows, write many rows into the SAME CSV
- Correctly removes 4-byte transport counters stored at the START of each packet:
    [4-byte counter][payload 1600 bytes]
- Removes repeated [int16 samples][8-byte double timestamp] frames
- Exports fixed-length windows as CSV rows or columns
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List, Optional, Tuple


@dataclass
class ParseInfo:
    packet_payload_bytes: int = 0
    packet_count: int = 0
    phase_bytes: int = 0
    samples_per_ts: int = 0
    timestamps_found: int = 0
    tail_samples: int = 0
    total_samples: int = 0


def _load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def _find_component_cfg(device_config: dict, sensor_name: str) -> dict:
    devices = device_config.get("devices", [])
    if not devices:
        raise ValueError("device_config.json has no devices[]")
    components = devices[0].get("components", [])
    for comp in components:
        if sensor_name in comp:
            return comp[sensor_name]
    raise ValueError(f"sensor '{sensor_name}' not found in device_config.json")


def _strip_transport_counters(raw: bytes, acq_info: dict, comp_cfg: dict) -> Tuple[bytes, int, int]:
    """Remove 4-byte protocol counters from packetized files.

    IMPORTANT:
    For the user's current files, actual packet layout is:
        [4-byte counter][payload]
    not:
        [payload][4-byte counter]
    """
    interface = acq_info.get("interface", None)
    payload_bytes = 0
    if interface == 1:
        payload_bytes = int(comp_cfg.get("usb_dps", 0) or 0)
    elif interface == 0:
        payload_bytes = int(comp_cfg.get("sd_dps", 0) or 0)

    if payload_bytes <= 0:
        return raw, 0, 0

    packet_size = payload_bytes + 4
    if len(raw) == 0 or len(raw) % packet_size != 0:
        return raw, 0, 0

    packet_count = len(raw) // packet_size
    payload = bytearray()
    for i in range(packet_count):
        base = i * packet_size
        # Real layout in current files: [4-byte counter][payload]
        payload.extend(raw[base + 4: base + 4 + payload_bytes])

    return bytes(payload), payload_bytes, packet_count


def _is_expected_timestamp(buf: bytes, off: int, expected: float, tol: float) -> bool:
    if off < 0 or off + 8 > len(buf):
        return False
    val = struct.unpack_from("<d", buf, off)[0]
    return math.isfinite(val) and abs(val - expected) <= tol


def _detect_phase_and_ts_count(payload: bytes, samples_per_ts: int, ioffset: float, odr_hz: float) -> Tuple[int, int]:
    frame_data_bytes = samples_per_ts * 2
    frame_bytes = frame_data_bytes + 8
    dt = samples_per_ts / odr_hz
    tol = max(1e-6, 2.0 / odr_hz)

    best_phase = 0
    best_count = 0
    for phase in range(0, min(frame_bytes, 256), 2):
        count = 0
        off = phase + frame_data_bytes
        while off + 8 <= len(payload):
            expected = ioffset + (count + 1) * dt
            if _is_expected_timestamp(payload, off, expected, tol):
                count += 1
                off += frame_bytes
            else:
                break
        if count > best_count:
            best_phase, best_count = phase, count
    return best_phase, best_count


def _extract_samples_from_payload(payload: bytes, samples_per_ts: int, phase: int, ts_count: int) -> bytes:
    frame_data_bytes = samples_per_ts * 2
    frame_bytes = frame_data_bytes + 8

    pure = bytearray()
    pos = phase
    for _ in range(ts_count):
        if pos + frame_data_bytes + 8 > len(payload):
            break
        pure.extend(payload[pos: pos + frame_data_bytes])
        pos += frame_bytes

    pure.extend(payload[pos:])
    if len(pure) % 2 != 0:
        pure = pure[:-1]
    return bytes(pure)


def parse_custom_dat(acq_dir: Path, sensor_name: str, odr_hz: float) -> Tuple[List[int], ParseInfo]:
    raw_path = acq_dir / f"{sensor_name}.dat"
    acq_info = _load_json(acq_dir / "acquisition_info.json")
    dev_cfg = _load_json(acq_dir / "device_config.json")
    comp_cfg = _find_component_cfg(dev_cfg, sensor_name)

    raw = raw_path.read_bytes()
    samples_per_ts = int(comp_cfg.get("samples_per_ts", 0) or 0)
    ioffset = float(comp_cfg.get("ioffset", 0.0) or 0.0)
    if samples_per_ts <= 0:
        raise ValueError("samples_per_ts must be > 0")
    if odr_hz <= 0:
        raise ValueError("odr_hz must be > 0")

    payload, packet_payload_bytes, packet_count = _strip_transport_counters(raw, acq_info, comp_cfg)
    phase, ts_count = _detect_phase_and_ts_count(payload, samples_per_ts, ioffset, odr_hz)
    pure_bytes = _extract_samples_from_payload(payload, samples_per_ts, phase, ts_count)

    samples = list(struct.unpack("<{}h".format(len(pure_bytes) // 2), pure_bytes)) if pure_bytes else []

    frame_data_bytes = samples_per_ts * 2
    frame_bytes = frame_data_bytes + 8
    consumed = phase + ts_count * frame_bytes
    tail_bytes = max(0, len(payload) - consumed)
    info = ParseInfo(
        packet_payload_bytes=packet_payload_bytes,
        packet_count=packet_count,
        phase_bytes=phase,
        samples_per_ts=samples_per_ts,
        timestamps_found=ts_count,
        tail_samples=tail_bytes // 2,
        total_samples=len(samples),
    )
    return samples, info


def make_windows(samples: List[int], signal_length: int, signal_increment: int) -> Iterable[List[int]]:
    if signal_length <= 0 or signal_increment <= 0:
        raise ValueError("signal_length and signal_increment must be > 0")
    idx = 0
    while idx + signal_length <= len(samples):
        yield samples[idx: idx + signal_length]
        idx += signal_increment


def _maybe_scale(values: List[int], sensitivity: Optional[float]) -> List[float] | List[int]:
    if sensitivity is None:
        return values
    return [v * sensitivity for v in values]


def write_windows_csv(path: Path, windows: List[List[int]], mode: str, sensitivity: Optional[float]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        if mode == "row":
            for window in windows:
                writer.writerow(_maybe_scale(window, sensitivity))
        else:
            for window in windows:
                scaled = _maybe_scale(window, sensitivity)
                for v in scaled:
                    writer.writerow([v])


def find_acquisition_dirs(root: Path, sensor_name: str) -> List[Path]:
    if (root / f"{sensor_name}.dat").exists() and (root / "acquisition_info.json").exists() and (root / "device_config.json").exists():
        return [root]
    dirs: List[Path] = []
    for child in sorted(root.iterdir()):
        if child.is_dir() and (child / f"{sensor_name}.dat").exists() and (child / "acquisition_info.json").exists() and (child / "device_config.json").exists():
            dirs.append(child)
    return dirs


def main() -> int:
    ap = argparse.ArgumentParser(description="Convert custom [samples][timestamp] STM32/ST-style .dat folders to NanoEdge CSV.")
    ap.add_argument("input", help="Acquisition folder, or a root folder containing many acquisition subfolders.")
    ap.add_argument("-o", "--output", required=True, help="Output folder")
    ap.add_argument("-s", "--sensor", default="imp23absu_mic", help="Sensor base filename without .dat")
    ap.add_argument("-sl", "--signal-length", type=int, required=True, help="Window length in samples")
    ap.add_argument("-si", "--signal-increment", type=int, required=True, help="Stride in samples")
    ap.add_argument("--odr", type=float, default=16000.0, help="Sensor ODR in Hz. Default: 16000")
    ap.add_argument("--csv-shape", choices=["column", "row"], default="row", help="Write each window as one row, or stack values in one column")
    ap.add_argument("--scaled", action="store_true", help="Convert int16 raw samples to physical units using sensitivity")
    ap.add_argument("--summary", default="conversion_summary.csv", help="Summary CSV filename inside output folder")
    args = ap.parse_args()

    root = Path(args.input)
    out_root = Path(args.output)
    out_root.mkdir(parents=True, exist_ok=True)

    acq_dirs = find_acquisition_dirs(root, args.sensor)
    if not acq_dirs:
        raise SystemExit(f"No acquisition folders found under: {root}")

    summary_rows = []
    total_windows = 0

    for acq_dir in acq_dirs:
        dev_cfg = _load_json(acq_dir / "device_config.json")
        comp_cfg = _find_component_cfg(dev_cfg, args.sensor)
        sensitivity = float(comp_cfg.get("sensitivity", 0.0) or 0.0) if args.scaled else None
        samples, info = parse_custom_dat(acq_dir, args.sensor, args.odr)
        windows = list(make_windows(samples, args.signal_length, args.signal_increment))

        stamp = acq_dir.name
        out_name = f"{stamp}__{args.sensor}.csv"
        write_windows_csv(out_root / out_name, windows, args.csv_shape, sensitivity)

        win_count = len(windows)
        total_windows += win_count
        summary_rows.append({
            "folder": stamp,
            "total_samples": info.total_samples,
            "timestamps_found": info.timestamps_found,
            "phase_bytes": info.phase_bytes,
            "packet_count": info.packet_count,
            "packet_payload_bytes": info.packet_payload_bytes,
            "tail_samples": info.tail_samples,
            "signal_length": args.signal_length,
            "signal_increment": args.signal_increment,
            "windows_written": win_count,
            "output_csv": out_name,
        })
        print(
            f"[OK] {stamp}: packets={info.packet_count}, payload_bytes={info.packet_payload_bytes}, "
            f"samples={info.total_samples}, timestamps={info.timestamps_found}, windows={win_count}, csv={out_name}"
        )

    summary_path = out_root / args.summary
    with summary_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(summary_rows[0].keys()))
        writer.writeheader()
        writer.writerows(summary_rows)

    print(f"[DONE] folders={len(acq_dirs)}, total_windows={total_windows}")
    print(f"[DONE] summary={summary_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
