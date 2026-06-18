import json
import math
import struct
import threading
import time
from collections import deque
from copy import deepcopy
from datetime import datetime, timezone, timedelta
from pathlib import Path

import serial
import serial.tools.list_ports
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure

SAMPLE_RATE = 16000
SAMPLES_PER_TS = 1000
MAX_POINTS = 12000
POLL_MS = 40
USB_DPS = 1600
DATA_PROTOCOL_SIZE = 4
IOFFSET = 0.3000054657459259
FRAME_PERIOD = SAMPLES_PER_TS / SAMPLE_RATE
REL_TS_START = IOFFSET + FRAME_PERIOD
CMD_READY = b"READY\r\n"
CMD_PING = b"PING\r\n"
CMD_START = b"START\r\n"
CMD_STOP = b"STOP\r\n"
ACK_OK_START = b"OK_START\r\n"
ACK_OK_STOP = b"OK_STOP\r\n"
ACK_ERR_START = b"ERR_START\r\n"
CTRL_TIMEOUT_S = 2.0
MAX_CHUNKS_PER_POLL = 32
STOP_OK_TIMEOUT_S = 1.5
STOP_READY_TIMEOUT_S = 0.4
STOP_RECOVERY_TIMEOUT_S = 1.5
STOP_QUIET_S = 0.10
STOP_PING_PERIOD_S = 0.25
STOP_ACCEPT_EXTRA_S = 0.30

ACQ_TEMPLATE = json.loads(r"""{
    "tags": [],
    "name": "STWIN.Box_acquisition",
    "description": "",
    "uuid": "00f04c54-3c25-4e97-82ec-9c344bcffb5c",
    "start_time": "2026-02-26T17:04:12.000Z",
    "end_time": "2026-02-26T17:04:14.003Z",
    "data_ext": ".dat",
    "data_fmt": "HSD_2.0.0",
    "interface": 1,
    "schema_version": "2.0.0",
    "c_type": 2
}""")

DEVICE_CONFIG_TEMPLATE = json.loads(r"""{
    "schema_version": "2.2.0",
    "uuid": "00f04c54-3c25-4e97-82ec-9c344bcffb5c",
    "devices": [
        {
            "board_id": 14,
            "fw_id": 46,
            "protocol_id": 2,
            "sn": "001A00394330501620333352",
            "pnpl_responses": true,
            "pnpl_ble_responses": true,
            "components": [
                {
                    "DeviceInformation": {
                        "manufacturer": "STMicroelectronics",
                        "model": "STEVAL-STWINBX1",
                        "swVersion": "3.1.0",
                        "osName": "AzureRTOS",
                        "processorArchitecture": "ARM Cortex-M33",
                        "processorManufacturer": "STMicroelectronics",
                        "totalStorage": 0,
                        "totalMemory": 784
                    }
                },
                {
                    "firmware_info": {
                        "alias": "STWIN_BOX_001",
                        "fw_name": "FP-SNS-DATALOG2_Datalog2",
                        "fw_version": "3.1.0",
                        "part_number": "FP-SNS-DATALOG2",
                        "device_url": "https://www.st.com/stwinbox",
                        "fw_url": "https://github.com/STMicroelectronics/fp-sns-datalog2",
                        "mac_address": "00:00:00:00:00:00",
                        "c_type": 2
                    }
                },
                {
                    "tags_info": {
                        "max_tags_num": 100,
                        "sw_tag0": {"label": "SW_TAG_0", "enabled": true, "status": false},
                        "sw_tag1": {"label": "SW_TAG_1", "enabled": true, "status": false},
                        "sw_tag2": {"label": "SW_TAG_2", "enabled": true, "status": false},
                        "sw_tag3": {"label": "SW_TAG_3", "enabled": true, "status": false},
                        "sw_tag4": {"label": "SW_TAG_4", "enabled": true, "status": false},
                        "sw_tag5": {"label": "SW_TAG_5", "enabled": true, "status": false},
                        "sw_tag6": {"label": "SW_TAG_6", "enabled": true, "status": false},
                        "sw_tag7": {"label": "SW_TAG_7", "enabled": true, "status": false},
                        "sw_tag8": {"label": "SW_TAG_8", "enabled": true, "status": false},
                        "sw_tag9": {"label": "SW_TAG_9", "enabled": true, "status": false},
                        "sw_tag10": {"label": "SW_TAG_10", "enabled": true, "status": false},
                        "sw_tag11": {"label": "SW_TAG_11", "enabled": true, "status": false},
                        "sw_tag12": {"label": "SW_TAG_12", "enabled": true, "status": false},
                        "sw_tag13": {"label": "SW_TAG_13", "enabled": true, "status": false},
                        "sw_tag14": {"label": "SW_TAG_14", "enabled": true, "status": false},
                        "sw_tag15": {"label": "SW_TAG_15", "enabled": true, "status": false},
                        "hw_tag0": {"label": "HW_TAG_0", "enabled": false, "status": false},
                        "hw_tag1": {"label": "HW_TAG_1", "enabled": false, "status": false},
                        "c_type": 2
                    }
                },
                {
                    "log_controller": {
                        "log_status": false,
                        "sd_mounted": false,
                        "sd_failed": false,
                        "controller_type": 0,
                        "c_type": 2
                    }
                },
                {
                    "automode": {
                        "enabled": false,
                        "nof_acquisitions": 0,
                        "start_delay_s": 0,
                        "logging_period_s": 1,
                        "idle_period_s": 1,
                        "c_type": 2
                    }
                },
                {
                    "imp23absu_mic": {
                        "odr": 0,
                        "aop": 0,
                        "enable": true,
                        "volume": 100,
                        "resolution": 0,
                        "samples_per_ts": 1000,
                        "dim": 1,
                        "ioffset": 0.3000054657459259,
                        "usb_dps": 1600,
                        "sd_dps": 10752,
                        "sensitivity": 3.0517578125e-05,
                        "data_type": "int16",
                        "sensor_annotation": "",
                        "sensor_category": 1,
                        "c_type": 0,
                        "stream_id": 0,
                        "ep_id": 0
                    }
                }
            ]
        }
    ]
}""")


def iso_utc_now_ms():
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z"


class StreamParser:
    def __init__(self, samples_per_ts=SAMPLES_PER_TS, sample_rate=SAMPLE_RATE):
        self.samples_per_ts = int(samples_per_ts)
        self.sample_rate = int(sample_rate)
        self.sample_bytes = self.samples_per_ts * 2
        self.frame_bytes = self.sample_bytes + 8
        self.buf = bytearray()
        self.phase_found = False
        self.order = "ts_first"
        self.phase_off = 0
        self.last_ts = None

    def reset(self):
        self.buf.clear()
        self.phase_found = False
        self.order = "ts_first"
        self.phase_off = 0
        self.last_ts = None

    def _ts_ok(self, vals):
        if len(vals) < 3:
            return False
        try:
            diffs = [vals[i + 1] - vals[i] for i in range(len(vals) - 1)]
        except Exception:
            return False
        target = self.samples_per_ts / self.sample_rate
        if any((v != v) or abs(v) > 1e12 for v in vals):
            return False
        if not all(0.01 < d < 0.2 for d in diffs):
            return False
        avg = sum(diffs) / len(diffs)
        return abs(avg - target) < 0.02 and (max(diffs) - min(diffs) < 0.01)

    def _samples_ok(self, raw):
        if len(raw) < self.sample_bytes:
            return False
        try:
            vals = struct.unpack("<" + "h" * self.samples_per_ts, raw[:self.sample_bytes])
        except Exception:
            return False
        good = sum(1 for v in vals if -2048 <= v <= 2048)
        return good >= int(self.samples_per_ts * 0.95)

    def try_detect_phase(self):
        need = self.frame_bytes * 5
        if len(self.buf) < need:
            return False
        for order in ("ts_first", "ts_last"):
            for off in range(self.frame_bytes):
                vals = []
                ok = True
                for k in range(4):
                    if order == "ts_first":
                        ts_pos = off + k * self.frame_bytes
                        smp_pos = ts_pos + 8
                    else:
                        smp_pos = off + k * self.frame_bytes
                        ts_pos = smp_pos + self.sample_bytes
                    if ts_pos + 8 > len(self.buf) or smp_pos + self.sample_bytes > len(self.buf):
                        ok = False
                        break
                    try:
                        vals.append(struct.unpack("<d", self.buf[ts_pos:ts_pos + 8])[0])
                    except Exception:
                        ok = False
                        break
                    if k == 0 and not self._samples_ok(self.buf[smp_pos:smp_pos + self.sample_bytes]):
                        ok = False
                        break
                if ok and self._ts_ok(vals):
                    self.phase_found = True
                    self.order = order
                    self.phase_off = off
                    return True
        return False

    def feed(self, data):
        self.buf.extend(data)
        out_samples = []
        out_ts = []

        if not self.phase_found:
            self.try_detect_phase()
            if not self.phase_found:
                return out_samples, out_ts

        if self.phase_off > 0 and len(self.buf) >= self.phase_off:
            del self.buf[:self.phase_off]
            self.phase_off = 0

        while len(self.buf) >= self.frame_bytes:
            if self.order == "ts_first":
                ts_bytes = self.buf[:8]
                smp_bytes = self.buf[8:8 + self.sample_bytes]
            else:
                smp_bytes = self.buf[:self.sample_bytes]
                ts_bytes = self.buf[self.sample_bytes:self.sample_bytes + 8]

            if len(ts_bytes) < 8 or len(smp_bytes) < self.sample_bytes:
                break

            try:
                ts = struct.unpack("<d", ts_bytes)[0]
            except Exception:
                del self.buf[:1]
                self.phase_found = False
                self.try_detect_phase()
                if not self.phase_found:
                    break
                continue

            if self.last_ts is not None:
                dt = ts - self.last_ts
                target = self.samples_per_ts / self.sample_rate
                if not (target - 0.02 <= dt <= target + 0.02):
                    del self.buf[:1]
                    self.phase_found = False
                    self.try_detect_phase()
                    if not self.phase_found:
                        break
                    continue

            try:
                samples = struct.unpack("<" + "h" * self.samples_per_ts, smp_bytes)
            except Exception:
                del self.buf[:1]
                self.phase_found = False
                self.try_detect_phase()
                if not self.phase_found:
                    break
                continue

            out_ts.append(ts)
            out_samples.extend(samples)
            self.last_ts = ts
            del self.buf[:self.frame_bytes]

        return out_samples, out_ts



class HSDTimestampRewriter:
    """
    Rebuild the payload in the same logical frame layout used by the official
    STWIN.Box acquisition for imp23absu_mic:
        [1000 int16 samples][8-byte double timestamp]
    i.e. timestamp at the END of each frame (ts_last).

    The incoming MCU stream may start at an arbitrary byte phase, so we search
    for the first plausible timestamp position at sample_bytes + off and then
    discard any leading partial bytes before the first full frame.
    """
    def __init__(self, samples_per_ts=SAMPLES_PER_TS, sample_rate=SAMPLE_RATE, relative_start=REL_TS_START):
        self.samples_per_ts = int(samples_per_ts)
        self.sample_rate = int(sample_rate)
        self.sample_bytes = self.samples_per_ts * 2
        self.frame_bytes = self.sample_bytes + 8
        self.relative_start = float(relative_start)
        self.frame_period = self.samples_per_ts / self.sample_rate
        self.buf = bytearray()
        self.phase_found = False
        self.phase_off = 0
        self.first_raw_ts = None
        self.last_raw_ts = None
        self.saved_frames = 0

    def reset(self):
        self.buf.clear()
        self.phase_found = False
        self.phase_off = 0
        self.first_raw_ts = None
        self.last_raw_ts = None
        self.saved_frames = 0

    def _ts_ok(self, vals):
        if len(vals) < 3:
            return False
        try:
            diffs = [vals[i + 1] - vals[i] for i in range(len(vals) - 1)]
        except Exception:
            return False
        if any((v != v) or abs(v) > 1e12 for v in vals):
            return False
        if not all(0.01 < d < 0.2 for d in diffs):
            return False
        avg = sum(diffs) / len(diffs)
        return abs(avg - self.frame_period) < 0.02 and (max(diffs) - min(diffs) < 0.01)

    def _samples_ok(self, raw):
        if len(raw) < self.sample_bytes:
            return False
        try:
            vals = struct.unpack("<" + "h" * self.samples_per_ts, raw[:self.sample_bytes])
        except Exception:
            return False
        good_signed = sum(1 for v in vals if -2048 <= v <= 2048)
        good_unsigned = sum(1 for v in vals if 0 <= v <= 4095)
        good = max(good_signed, good_unsigned)
        return good >= int(self.samples_per_ts * 0.95)

    def try_detect_phase(self):
        need = self.frame_bytes * 5
        if len(self.buf) < need:
            return False

        # Official / HSD expected ordering is ts_last only:
        #   [samples(2000 bytes)][timestamp(8 bytes)]
        for off in range(self.frame_bytes):
            vals = []
            ok = True
            smp_pos0 = off
            ts_pos0 = off + self.sample_bytes
            if ts_pos0 + 8 > len(self.buf):
                continue
            if not self._samples_ok(self.buf[smp_pos0:smp_pos0 + self.sample_bytes]):
                continue
            for k in range(4):
                ts_pos = off + self.sample_bytes + k * self.frame_bytes
                if ts_pos + 8 > len(self.buf):
                    ok = False
                    break
                try:
                    vals.append(struct.unpack("<d", self.buf[ts_pos:ts_pos + 8])[0])
                except Exception:
                    ok = False
                    break
            if ok and self._ts_ok(vals):
                self.phase_found = True
                self.phase_off = off
                return True
        return False

    def _make_saved_ts(self, raw_ts):
        if self.first_raw_ts is None:
            self.first_raw_ts = raw_ts
            return self.relative_start
        delta = raw_ts - self.first_raw_ts
        if delta < 0:
            delta = 0.0
        return self.relative_start + delta

    def feed(self, data):
        self.buf.extend(data)
        out = bytearray()

        if not self.phase_found:
            self.try_detect_phase()
            if not self.phase_found:
                return bytes(out)

        if self.phase_off > 0 and len(self.buf) >= self.phase_off:
            del self.buf[:self.phase_off]
            self.phase_off = 0

        while len(self.buf) >= self.frame_bytes:
            smp_bytes = self.buf[:self.sample_bytes]
            ts_bytes = self.buf[self.sample_bytes:self.sample_bytes + 8]

            if len(ts_bytes) < 8 or len(smp_bytes) < self.sample_bytes:
                break

            try:
                raw_ts = struct.unpack("<d", ts_bytes)[0]
            except Exception:
                del self.buf[:1]
                self.phase_found = False
                self.try_detect_phase()
                if not self.phase_found:
                    break
                continue

            if not self._samples_ok(smp_bytes):
                del self.buf[:1]
                self.phase_found = False
                self.try_detect_phase()
                if not self.phase_found:
                    break
                continue

            if self.last_raw_ts is not None:
                dt = raw_ts - self.last_raw_ts
                if not (self.frame_period - 0.02 <= dt <= self.frame_period + 0.02):
                    del self.buf[:1]
                    self.phase_found = False
                    self.try_detect_phase()
                    if not self.phase_found:
                        break
                    continue

            saved_ts = self._make_saved_ts(raw_ts)
            out.extend(smp_bytes)
            out.extend(struct.pack("<d", saved_ts))

            self.last_raw_ts = raw_ts
            self.saved_frames += 1
            del self.buf[:self.frame_bytes]

        return bytes(out)



class App:
    def __init__(self, root):
        self.root = root
        self.root.title("BINN DataLog Viewer")
        self.script_dir = Path(__file__).resolve().parent
        self.save_root = self.script_dir

        self.ser = None
        self.stop_evt = threading.Event()
        self.reader_thread = None
        self.mcu_ready = False
        self.capture_active = False
        self.capture_start_monotonic = 0.0
        self.capture_duration_sec = 0.0
        self.capture_stopping = False
        self.stop_finalize_pending = False
        self.stop_state = None
        self.stop_deadline_monotonic = 0.0
        self.stop_tail_quiet_deadline = 0.0
        self.stop_next_ping_monotonic = 0.0
        self.stop_accept_payload_deadline = 0.0
        self.plot_freeze_on_stop = False

        self.raw_fp = None
        self.current_capture_dir = None
        self.capture_start_iso = None
        self.capture_end_iso = None
        self.saved_samples = 0
        self.saved_packets = 0
        self.save_payload_buf = bytearray()
        self.save_counter = 0
        self.save_rewriter = HSDTimestampRewriter()
        self.rewritten_frame_bytes = 0
        self.target_frames = 0
        self.plot_kept_frames = 0
        self.saved_kept_frames = 0

        self.parser = StreamParser()
        self.raw_queue = deque()
        self.live_samples = []

        self.ctrl_line_buf = bytearray()
        self.evt_ready = threading.Event()
        self.evt_ok_start = threading.Event()
        self.evt_ok_stop = threading.Event()
        self.evt_err_start = threading.Event()

        self.debug_enabled = tk.BooleanVar(value=False)
        self.port = tk.StringVar()
        self.baud = tk.StringVar(value="921600")
        self.dur = tk.StringVar(value="2.0")
        self.save_dir_var = tk.StringVar(value=str(self.script_dir))

        self._build()
        self.refresh_ports()
        self.poll()

    def _build(self):
        outer = ttk.Frame(self.root, padding=4)
        outer.pack(fill=tk.BOTH, expand=True)

        pw = ttk.Panedwindow(outer, orient=tk.HORIZONTAL)
        pw.pack(fill=tk.BOTH, expand=True)

        left = ttk.Frame(pw, padding=(4, 2, 4, 2))
        right = ttk.Frame(pw, padding=(4, 2, 2, 2))
        pw.add(left, weight=0)
        pw.add(right, weight=1)

        left.columnconfigure(0, weight=1)
        left.columnconfigure(1, weight=1)

        r = 0
        ttk.Label(left, text="Serial Port").grid(row=r, column=0, sticky="w", pady=(0, 2))
        self.portbox = ttk.Combobox(left, textvariable=self.port, state="readonly", width=14)
        self.portbox.grid(row=r, column=1, sticky="ew", pady=(0, 2))
        r += 1

        row = ttk.Frame(left)
        row.grid(row=r, column=0, columnspan=2, sticky="ew", pady=(0, 6))
        row.columnconfigure(0, weight=1)
        row.columnconfigure(1, weight=1)
        ttk.Button(row, text="Refresh", command=self.refresh_ports, width=8).grid(row=0, column=0, sticky="ew", padx=(0, 3))
        self.conn_btn = ttk.Button(row, text="Connect", command=self.toggle_conn, width=8)
        self.conn_btn.grid(row=0, column=1, sticky="ew", padx=(3, 0))
        r += 1

        ttk.Label(left, text="Baud Rate").grid(row=r, column=0, sticky="w", pady=(0, 2))
        ttk.Entry(left, textvariable=self.baud).grid(row=r, column=1, sticky="ew", pady=(0, 2))
        r += 1

        save_row = ttk.Frame(left)
        save_row.grid(row=r, column=0, columnspan=2, sticky="ew", pady=(0, 6))
        save_row.columnconfigure(1, weight=1)
        ttk.Button(save_row, text="Browse", command=self.browse_dir, width=8).grid(row=0, column=0, sticky="ew", padx=(0, 3))
        ttk.Entry(save_row, textvariable=self.save_dir_var).grid(row=0, column=1, sticky="ew")
        r += 1

        ttk.Label(left, text="Capture time (s)").grid(row=r, column=0, sticky="w", pady=(0, 2))
        ttk.Entry(left, textvariable=self.dur).grid(row=r, column=1, sticky="ew", pady=(0, 2))
        r += 1

        row = ttk.Frame(left)
        row.grid(row=r, column=0, columnspan=2, sticky="ew", pady=(2, 8))
        row.columnconfigure(0, weight=1)
        row.columnconfigure(1, weight=1)
        self.start_btn = ttk.Button(row, text="Start", command=self.start_capture, state=tk.DISABLED, width=8)
        self.start_btn.grid(row=0, column=0, sticky="ew", padx=(0, 3))
        self.stop_btn = ttk.Button(row, text="Stop", command=self.stop_capture, state=tk.DISABLED, width=8)
        self.stop_btn.grid(row=0, column=1, sticky="ew", padx=(3, 0))
        r += 1

        dbg_bar = ttk.Frame(left)
        dbg_bar.grid(row=r, column=0, columnspan=2, sticky="ew", pady=(0, 3))
        dbg_bar.columnconfigure(0, weight=1)
        ttk.Checkbutton(dbg_bar, text="Debug", variable=self.debug_enabled, command=self.on_debug_toggle).grid(row=0, column=0, sticky="w")
        ttk.Button(dbg_bar, text="Clear", command=self.clear_debug, width=7).grid(row=0, column=1, sticky="e")
        r += 1

        dbg_wrap = ttk.Frame(left)
        dbg_wrap.grid(row=r, column=0, columnspan=2, sticky="nsew")
        dbg_wrap.rowconfigure(0, weight=1)
        dbg_wrap.columnconfigure(0, weight=1)
        self.debug_text = tk.Text(dbg_wrap, height=12, wrap="word")
        self.debug_text.grid(row=0, column=0, sticky="nsew")
        dbg_scroll = ttk.Scrollbar(dbg_wrap, orient="vertical", command=self.debug_text.yview)
        dbg_scroll.grid(row=0, column=1, sticky="ns")
        self.debug_text.configure(yscrollcommand=dbg_scroll.set)
        left.rowconfigure(r, weight=1)

        right.rowconfigure(0, weight=1)
        right.columnconfigure(0, weight=1)

        self.fig = Figure(figsize=(9.0, 6.0), dpi=100)
        self.ax = self.fig.add_subplot(111)
        self.ax.set_xlabel("Time (s)")
        self.ax.set_ylabel("Amplitude")
        self.ax.margins(x=0)
        self.fig.subplots_adjust(left=0.08, right=0.985, top=0.96, bottom=0.11)
        self.line, = self.ax.plot([], [], linewidth=0.8)
        self.canvas = FigureCanvasTkAgg(self.fig, master=right)
        self.canvas.get_tk_widget().grid(row=0, column=0, sticky="nsew")

        try:
            pw.sashpos(0, 285)
        except Exception:
            pass

    def log(self, msg):
        if not self.debug_enabled.get():
            return
        ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        self.debug_text.insert("end", f"[{ts}] {msg}\n")
        self.debug_text.see("end")

    def clear_debug(self):
        self.debug_text.delete("1.0", "end")

    def on_debug_toggle(self):
        if not self.debug_enabled.get():
            self.clear_debug()

    def refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.portbox["values"] = ports
        if ports and (not self.port.get() or self.port.get() not in ports):
            self.port.set(ports[0])

    def browse_dir(self):
        path = filedialog.askdirectory(initialdir=str(self.save_root))
        if path:
            self.save_root = Path(path)
            self.save_dir_var.set(str(self.save_root))
            self.log(f"Save root set: {self.save_root}")

    def toggle_conn(self):
        if self.ser is None:
            self.connect()
        else:
            self.disconnect()

    def connect(self):
        if not self.port.get():
            messagebox.showerror("Connect failed", "No serial port selected.")
            return
        try:
            self.ser = serial.Serial(self.port.get(), int(self.baud.get()), timeout=0)
        except Exception as e:
            messagebox.showerror("Connect failed", str(e))
            return

        self.stop_evt.clear()
        self.ctrl_line_buf.clear()
        self.evt_ready.clear()
        self.evt_ok_start.clear()
        self.evt_ok_stop.clear()
        self.evt_err_start.clear()
        self.mcu_ready = False

        if self.reader_thread is None or (not self.reader_thread.is_alive()):
            self.reader_thread = threading.Thread(target=self.reader, daemon=True)
            self.reader_thread.start()

        self.conn_btn.config(text="Disconnect")
        self.start_btn.config(state=tk.NORMAL)
        self.log(f"Connected to {self.port.get()} @ {self.baud.get()}")

        time.sleep(0.10)
        self.mcu_ready = self._probe_mcu_ready(timeout=2.0)
        if self.mcu_ready:
            self.log("MCU ready")
        else:
            self.log("MCU ready probe timeout; START will still retry")

    def disconnect(self):
        self.capture_active = False
        self.capture_stopping = False
        self.plot_freeze_on_stop = False
        self.stop_finalize_pending = False
        self.stop_state = None
        self.stop_accept_payload_deadline = 0.0
        self.plot_freeze_on_stop = False
        self.stop_evt.set()

        ser = self.ser
        self.ser = None
        if ser is not None:
            try:
                ser.close()
            except Exception:
                pass

        if self.reader_thread is not None and self.reader_thread.is_alive():
            try:
                self.reader_thread.join(timeout=0.3)
            except Exception:
                pass
        self.reader_thread = None

        self.conn_btn.config(text="Connect")
        self.start_btn.config(state=tk.DISABLED)
        self.stop_btn.config(state=tk.DISABLED)
        self.log("Disconnected")

    def _write_cmd(self, cmd: bytes) -> bool:
        if self.ser is None:
            return False
        try:
            self.ser.write(cmd)
            self.ser.flush()
            return True
        except Exception as e:
            self.log(f"Serial write error: {e}")
            return False

    def _signal_control_line(self, line: bytes) -> bool:
        # Accept CR, LF, or CRLF terminated control lines.
        norm = line.strip()
        if norm == b"READY":
            self.mcu_ready = True
            self.evt_ready.set()
            return True
        if norm == b"OK_START":
            self.mcu_ready = False
            self.evt_ok_start.set()
            return True
        if norm == b"OK_STOP":
            self.evt_ok_stop.set()
            return True
        if norm == b"ERR_START":
            self.evt_err_start.set()
            return True
        return False

    def _split_control_and_payload(self, chunk: bytes) -> bytes:
        """
        Robustly peel fixed ASCII control tokens out of a mixed binary stream.

        Instead of guessing control bytes by character class, search for the
        exact wire tokens anywhere in the incoming byte stream:
            READY\r\n
            OK_START\r\n
            OK_STOP\r\n
            ERR_START\r\n
        and return everything else as payload bytes.
        """
        tokens = (CMD_READY, ACK_OK_START, ACK_OK_STOP, ACK_ERR_START)
        self.ctrl_line_buf.extend(chunk)
        payload = bytearray()

        while True:
            best_idx = None
            best_tok = None

            for tok in tokens:
                idx = self.ctrl_line_buf.find(tok)
                if idx >= 0 and (best_idx is None or idx < best_idx):
                    best_idx = idx
                    best_tok = tok

            if best_tok is not None:
                if best_idx > 0:
                    payload.extend(self.ctrl_line_buf[:best_idx])
                del self.ctrl_line_buf[:best_idx + len(best_tok)]
                self._signal_control_line(best_tok)
                continue

            # No complete token found. Keep only the longest suffix that could
            # be the prefix of a token, and flush the rest as payload.
            keep_len = 0
            max_check = min(len(self.ctrl_line_buf), max(len(t) - 1 for t in tokens))
            for k in range(1, max_check + 1):
                suffix = bytes(self.ctrl_line_buf[-k:])
                if any(tok.startswith(suffix) for tok in tokens):
                    keep_len = k

            if keep_len > 0:
                flush_len = len(self.ctrl_line_buf) - keep_len
                if flush_len > 0:
                    payload.extend(self.ctrl_line_buf[:flush_len])
                    del self.ctrl_line_buf[:flush_len]
            else:
                payload.extend(self.ctrl_line_buf)
                self.ctrl_line_buf.clear()

            break

        return bytes(payload)

    def _probe_mcu_ready(self, timeout=2.0):
        if self.ser is None:
            return False
        if self.mcu_ready or self.evt_ready.is_set():
            self.mcu_ready = True
            return True

        deadline = time.monotonic() + float(timeout)
        next_ping = 0.0
        while time.monotonic() < deadline:
            now = time.monotonic()
            if now >= next_ping:
                if not self._write_cmd(CMD_PING):
                    return False
                next_ping = now + 0.25

            wait_s = min(0.08, max(0.0, deadline - time.monotonic()))
            if self.evt_ready.wait(wait_s):
                self.mcu_ready = True
                return True

        return False

    def _ensure_mcu_ready(self, timeout=1.5):
        if self.ser is None:
            return False
        if self.mcu_ready:
            return True
        ok = self._probe_mcu_ready(timeout=timeout)
        if ok:
            self.log("MCU ready before START")
        else:
            self.log("MCU still not ready before START")
        return ok

    def _begin_stop_sequence(self, finalize=True):
        if not self.capture_active or self.ser is None:
            self.capture_active = False
            self.capture_stopping = False
            self.stop_finalize_pending = False
            self.stop_state = None
            self.start_btn.config(state=tk.NORMAL if self.ser is not None else tk.DISABLED)
            self.stop_btn.config(state=tk.DISABLED)
            return

        self.capture_stopping = True
        self.stop_finalize_pending = bool(finalize)
        self.stop_state = "wait_ok_stop"
        self.stop_deadline_monotonic = time.monotonic() + STOP_OK_TIMEOUT_S
        self.stop_tail_quiet_deadline = 0.0
        self.stop_next_ping_monotonic = time.monotonic()
        self.stop_accept_payload_deadline = time.monotonic() + STOP_ACCEPT_EXTRA_S
        self.plot_freeze_on_stop = True
        self.capture_active = False
        self.evt_ok_stop.clear()
        self.evt_ready.clear()
        self.start_btn.config(state=tk.DISABLED)
        self.stop_btn.config(state=tk.DISABLED)

        if self._write_cmd(CMD_STOP):
            self.log("STOP sent")
        else:
            self.log("STOP send failed")
            self.stop_state = "tail_drain"

    def _finish_stop_sequence(self):
        was_active = self.capture_active or self.capture_stopping
        self.capture_active = False
        self.capture_stopping = False
        self.plot_freeze_on_stop = False
        self.mcu_ready = bool(self.evt_ready.is_set()) or self.mcu_ready
        self.stop_state = None
        self.stop_deadline_monotonic = 0.0
        self.stop_tail_quiet_deadline = 0.0
        self.stop_next_ping_monotonic = 0.0
        self.stop_accept_payload_deadline = 0.0
        self.plot_freeze_on_stop = False

        # IMPORTANT:
        # During the non-blocking STOP sequence we intentionally freeze live plotting
        # so the UI does not keep scrolling past the requested capture time.
        # However, tail payload may still be accepted and appended to live_samples.
        # Do one final redraw here so the final displayed waveform duration matches
        # the kept/saved frames after STOP completes.
        self.update_plot()

        self.start_btn.config(state=tk.NORMAL if self.ser is not None else tk.DISABLED)
        self.stop_btn.config(state=tk.DISABLED)

        if self.stop_finalize_pending and self.current_capture_dir is not None:
            self._finalize_capture_files()
            self.current_capture_dir = None

        self.stop_finalize_pending = False

        if was_active:
            self.log("Capture stopped")

    def _service_stop_sequence(self, processed_any=False):
        if not self.capture_stopping:
            return

        now = time.monotonic()

        if processed_any or self.raw_queue:
            self.stop_tail_quiet_deadline = now + STOP_QUIET_S

        if self.stop_state == "wait_ok_stop":
            if self.evt_ok_stop.is_set():
                self.log("STOP ack received")
                self.stop_state = "wait_ready"
                self.stop_deadline_monotonic = now + STOP_READY_TIMEOUT_S
                return
            if now >= self.stop_deadline_monotonic:
                self.log("STOP ack timeout: evt_ok_stop not set")
                self.stop_state = "probe_ready"
                self.stop_deadline_monotonic = now + STOP_RECOVERY_TIMEOUT_S
                self.stop_next_ping_monotonic = now
                return
            return

        if self.stop_state == "wait_ready":
            if self.evt_ready.is_set():
                self.mcu_ready = True
                self.stop_state = "tail_drain"
                if self.stop_tail_quiet_deadline <= 0.0:
                    self.stop_tail_quiet_deadline = now + STOP_QUIET_S
                return
            if now >= self.stop_deadline_monotonic:
                self.stop_state = "tail_drain"
                if self.stop_tail_quiet_deadline <= 0.0:
                    self.stop_tail_quiet_deadline = now + STOP_QUIET_S
                return
            return

        if self.stop_state == "probe_ready":
            if self.evt_ready.is_set():
                self.log("Recovered READY after STOP timeout")
                self.mcu_ready = True
                self.stop_state = "tail_drain"
                if self.stop_tail_quiet_deadline <= 0.0:
                    self.stop_tail_quiet_deadline = now + STOP_QUIET_S
                return
            if now >= self.stop_deadline_monotonic:
                self.stop_state = "tail_drain"
                if self.stop_tail_quiet_deadline <= 0.0:
                    self.stop_tail_quiet_deadline = now + STOP_QUIET_S
                return
            if now >= self.stop_next_ping_monotonic:
                self._write_cmd(CMD_PING)
                self.stop_next_ping_monotonic = now + STOP_PING_PERIOD_S
            return

        if self.stop_state == "tail_drain":
            if self.stop_tail_quiet_deadline <= 0.0:
                self.stop_tail_quiet_deadline = now + STOP_QUIET_S
                return
            if now >= self.stop_tail_quiet_deadline and not self.raw_queue:
                self._finish_stop_sequence()
            return

    def _drain_tail_until_quiet(self, max_wait_s=1.5, quiet_s=0.10):
        """
        After STOP, keep draining parser backlog until the raw queue stays empty
        for a short quiet window, or until max_wait_s expires.
        This avoids finalizing too early and losing the tail of a valid capture.
        """
        deadline = time.monotonic() + float(max_wait_s)
        quiet_deadline = None

        while time.monotonic() < deadline:
            drained_any = False
            while self.raw_queue:
                self._handle_raw_chunk(self.raw_queue.popleft())
                drained_any = True

            if drained_any:
                quiet_deadline = time.monotonic() + float(quiet_s)
            else:
                if quiet_deadline is None:
                    quiet_deadline = time.monotonic() + float(quiet_s)
                if time.monotonic() >= quiet_deadline:
                    break
                time.sleep(0.01)

    def _abort_capture_files(self):
        if self.raw_fp is not None:
            try:
                self.raw_fp.close()
            except Exception:
                pass
            self.raw_fp = None
        if self.current_capture_dir is not None:
            try:
                dat = self.current_capture_dir / "imp23absu_mic.dat"
                if dat.exists():
                    dat.unlink()
                ai = self.current_capture_dir / "acquisition_info.json"
                if ai.exists():
                    ai.unlink()
                dc = self.current_capture_dir / "device_config.json"
                if dc.exists():
                    dc.unlink()
                self.current_capture_dir.rmdir()
            except Exception:
                pass
            self.current_capture_dir = None

    def _reset_capture_state(self):
        self.raw_queue.clear()
        self.live_samples.clear()
        self.parser.reset()
        self.saved_samples = 0
        self.saved_packets = 0
        self.save_payload_buf = bytearray()
        self.save_counter = 0
        self.save_rewriter.reset()
        self.rewritten_frame_bytes = 0
        self.target_frames = 0
        self.plot_kept_frames = 0
        self.saved_kept_frames = 0
        self.capture_stopping = False
        self.stop_finalize_pending = False
        self.stop_state = None
        self.stop_deadline_monotonic = 0.0
        self.stop_tail_quiet_deadline = 0.0
        self.stop_next_ping_monotonic = 0.0
        self.stop_accept_payload_deadline = 0.0
        self.plot_freeze_on_stop = False
        self.ctrl_line_buf.clear()
        self.stop_accept_payload_deadline = 0.0
        self.plot_freeze_on_stop = False
        self.line.set_data([], [])
        self.ax.set_xlim(0, 1)
        self.ax.set_ylim(-1, 1)
        self.canvas.draw_idle()

    def _open_capture_files(self):
        text_dir = self.save_dir_var.get().strip()
        self.save_root = Path(text_dir) if text_dir else self.script_dir
        self.current_capture_dir = self.save_root / datetime.now().strftime("%Y%m%d_%H_%M_%S")
        self.current_capture_dir.mkdir(parents=True, exist_ok=True)
        self.raw_fp = open(self.current_capture_dir / "imp23absu_mic.dat", "wb")
        self.capture_start_iso = iso_utc_now_ms()
        self.capture_end_iso = None

    def _compute_capture_end_iso(self):
        if not self.capture_start_iso:
            return iso_utc_now_ms()
        try:
            start_dt = datetime.strptime(self.capture_start_iso, "%Y-%m-%dT%H:%M:%S.%fZ").replace(tzinfo=timezone.utc)
        except Exception:
            return iso_utc_now_ms()

        kept_frames = self.saved_kept_frames or self.plot_kept_frames
        duration_s = (kept_frames * SAMPLES_PER_TS) / SAMPLE_RATE if kept_frames > 0 else 0.0
        end_dt = start_dt + timedelta(seconds=duration_s)
        return end_dt.strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z"

    def _finalize_capture_files(self):
        dropped_tail = len(self.save_payload_buf)
        self.save_payload_buf.clear()

        if self.raw_fp is not None:
            try:
                self.raw_fp.flush()
                self.raw_fp.close()
            except Exception:
                pass
            self.raw_fp = None
        if self.current_capture_dir is None:
            return
        self.capture_end_iso = self._compute_capture_end_iso()
        acq = deepcopy(ACQ_TEMPLATE)
        acq["start_time"] = self.capture_start_iso
        acq["end_time"] = self.capture_end_iso
        with open(self.current_capture_dir / "acquisition_info.json", "w", encoding="utf-8", newline="\n") as f:
            json.dump(acq, f, indent=4, ensure_ascii=False)
        with open(self.current_capture_dir / "device_config.json", "w", encoding="utf-8", newline="\n") as f:
            json.dump(DEVICE_CONFIG_TEMPLATE, f, indent=4, ensure_ascii=False)
        self.log(f"Saved folder: {self.current_capture_dir}")
        self.log(f"Saved parsed samples for plot: {self.saved_samples}")
        self.log(f"Saved HSD USB packets: {self.saved_packets}")
        self.log(f"Rewritten timestamp frames: {self.save_rewriter.saved_frames}")
        self.log(f"Kept plot frames: {self.plot_kept_frames}")
        self.log(f"Kept saved frames: {self.saved_kept_frames}")
        if dropped_tail:
            self.log(f"Dropped trailing partial USB payload bytes: {dropped_tail}")

    def start_capture(self):
        if self.ser is None:
            return
        try:
            sec = float(self.dur.get())
            if sec <= 0:
                raise ValueError
        except Exception:
            messagebox.showerror("Error", "Invalid capture time.")
            return

        self.capture_duration_sec = sec
        self._reset_capture_state()
        self.target_frames = max(1, int(round(sec * SAMPLE_RATE / SAMPLES_PER_TS)))
        self._open_capture_files()

        # Do not start the auto-stop timer until MCU has really acknowledged START.
        self.capture_start_monotonic = 0.0
        self.capture_active = False

        self.evt_ok_start.clear()
        self.evt_err_start.clear()
        self.evt_ready.clear()

        ok = False
        for attempt in range(3):
            if attempt == 0:
                self._ensure_mcu_ready(timeout=1.0)
            else:
                self._probe_mcu_ready(timeout=1.0)
                time.sleep(0.05)

            self.evt_ok_start.clear()
            self.evt_err_start.clear()
            if not self._write_cmd(CMD_START):
                continue

            t0 = time.monotonic()
            while time.monotonic() - t0 < 0.8:
                if self.evt_ok_start.is_set():
                    ok = True
                    break
                if self.evt_err_start.is_set():
                    break
                time.sleep(0.01)

            if ok:
                break

        if not ok:
            self.capture_active = False
            self.raw_queue.clear()
            self._abort_capture_files()
            messagebox.showerror("Error", "MCU did not acknowledge START.")
            return

        self.capture_start_monotonic = time.monotonic()
        self.capture_active = True
        self.mcu_ready = False
        self.start_btn.config(state=tk.DISABLED)
        self.stop_btn.config(state=tk.NORMAL)
        self.log(f"Capture started for {sec:.3f} s")

    def stop_capture(self, finalize=True):
        if self.capture_stopping:
            self.stop_finalize_pending = self.stop_finalize_pending or bool(finalize)
            return

        if self.capture_active:
            self._begin_stop_sequence(finalize=finalize)
            return

        self.capture_active = False
        self.capture_stopping = False
        self.plot_freeze_on_stop = False
        self.start_btn.config(state=tk.NORMAL if self.ser is not None else tk.DISABLED)
        self.stop_btn.config(state=tk.DISABLED)

        if finalize and self.current_capture_dir is not None:
            self._finalize_capture_files()
            self.current_capture_dir = None

    def reader(self):
        while not self.stop_evt.is_set():
            try:
                if self.ser is None:
                    time.sleep(0.01)
                    continue
                n = self.ser.in_waiting
                if n <= 0:
                    time.sleep(0.005)
                    continue
                chunk = self.ser.read(n)
            except Exception as e:
                self.log(f"Serial read error: {e}")
                break

            if not chunk:
                continue

            payload = self._split_control_and_payload(chunk)
            if payload:
                if self.capture_active:
                    self.raw_queue.append(payload)
                elif self.capture_stopping:
                    if time.monotonic() <= self.stop_accept_payload_deadline:
                        self.raw_queue.append(payload)

    def _append_hsd_usb_payload(self, payload_bytes):
        if self.raw_fp is None or not payload_bytes:
            return
        self.save_payload_buf.extend(payload_bytes)
        while len(self.save_payload_buf) >= USB_DPS:
            payload = bytes(self.save_payload_buf[:USB_DPS])
            del self.save_payload_buf[:USB_DPS]
            self.raw_fp.write(struct.pack("<I", self.save_counter))
            self.raw_fp.write(payload)
            self.save_counter += USB_DPS
            self.saved_packets += 1

    def _handle_raw_chunk(self, chunk):
        rewritten = self.save_rewriter.feed(chunk)
        if not rewritten:
            return

        frame_bytes = self.save_rewriter.frame_bytes
        produced_frames = len(rewritten) // frame_bytes
        if self.target_frames > 0:
            remaining_frames = max(0, self.target_frames - self.saved_kept_frames)
            keep_frames = min(produced_frames, remaining_frames)
        else:
            keep_frames = produced_frames

        if keep_frames <= 0:
            return

        keep_bytes = keep_frames * frame_bytes
        kept_rewritten = rewritten[:keep_bytes]
        self.rewritten_frame_bytes += len(kept_rewritten)
        self._append_hsd_usb_payload(kept_rewritten)
        self.saved_kept_frames += keep_frames

        # IMPORTANT: build the live plot from the exact same kept frames that are saved,
        # so the displayed waveform duration and the saved acquisition duration always match.
        sample_bytes = self.save_rewriter.sample_bytes
        for k in range(keep_frames):
            pos = k * frame_bytes
            smp_bytes = kept_rewritten[pos:pos + sample_bytes]
            try:
                samples = struct.unpack("<" + "h" * self.save_rewriter.samples_per_ts, smp_bytes)
            except Exception:
                break
            self.live_samples.extend(samples)

        self.saved_samples = len(self.live_samples)
        self.plot_kept_frames = self.saved_kept_frames

        self.log(
            f"Rewriter produced={produced_frames}, kept={keep_frames}, total_saved={self.saved_kept_frames}, total_plot={self.plot_kept_frames}"
        )

    def update_plot(self):
        if not self.live_samples:
            return
        n = len(self.live_samples)
        step = max(1, math.ceil(n / MAX_POINTS))
        yv = self.live_samples[::step]
        xv = [i / SAMPLE_RATE for i in range(0, n, step)]
        if not yv or not xv:
            return
        self.line.set_data(xv, yv)
        total_t = max(n / SAMPLE_RATE, 1e-6)
        self.ax.set_xlim(0, total_t)
        ymin = min(yv)
        ymax = max(yv)
        if ymin == ymax:
            ymin -= 1
            ymax += 1
        margin = max(1, (ymax - ymin) * 0.05)
        self.ax.set_ylim(ymin - margin, ymax + margin)
        self.canvas.draw_idle()

    def poll(self):
        processed_any = False
        n_done = 0
        max_chunks = MAX_CHUNKS_PER_POLL * (4 if self.capture_stopping else 1)
        while self.raw_queue and n_done < max_chunks:
            chunk = self.raw_queue.popleft()
            self._handle_raw_chunk(chunk)
            processed_any = True
            n_done += 1

        if self.capture_active and (not self.capture_stopping) and self.capture_start_monotonic > 0.0:
            elapsed = time.monotonic() - self.capture_start_monotonic
            if elapsed >= self.capture_duration_sec:
                self.stop_capture(finalize=True)

        self._service_stop_sequence(processed_any=processed_any)

        if processed_any and (not self.plot_freeze_on_stop):
            self.update_plot()
        self.root.after(POLL_MS, self.poll)


if __name__ == "__main__":
    root = tk.Tk()
    app = App(root)

    def _on_close():
        app.disconnect()
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", _on_close)
    root.mainloop()
