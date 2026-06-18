#!/usr/bin/env python
# coding: utf-8

#   This software component is licensed by ST under BSD 3-Clause license,
#   the "License"; You may not use this file except in compliance with the
#   License. You may obtain a copy of the License at:
#                        https://opensource.org/licenses/BSD-3-Clause

import os
from typing import List, Optional, Sequence, Tuple

import librosa as lb
import numpy as np
import pandas as pd
from scipy.io import wavfile

# importing required dependencies
from sklearn.model_selection import train_test_split
from sklearn.utils import shuffle
from tensorflow.keras.utils import to_categorical  # type: ignore[reportMissingImports]

# from st_hsdatalog.HSD.HSDatalog import HSDatalog

# Compatible with both newer stdatalog-core and older st_hsdatalog packages.
HAVE_HSD = False
_HSDatalog = None
_HSDatalogV2 = None
_HSD_BACKEND = None

try:
    # Newer SDK (stdatalog-core)
    from stdatalog_core.HSD.HSDatalog import HSDatalog as _HSDatalog

    _HSD_BACKEND = "stdatalog_core"
    HAVE_HSD = True
    try:
        # Some FP-SNS-DATALOG2 datasets are more reliably parsed with the explicit V2 class
        from stdatalog_core.HSD.HSDatalog_v2 import HSDatalog_v2 as _HSDatalogV2
    except Exception:
        _HSDatalogV2 = None
except Exception:
    try:
        # Older SDK (st_hsdatalog / HSDPython_SDK)
        from st_hsdatalog.HSD.HSDatalog import HSDatalog as _HSDatalog

        _HSD_BACKEND = "st_hsdatalog"
        HAVE_HSD = True
    except Exception:
        _HSDatalog = None  # Common in training-only environments without ST SDK

# setting random seeds for reproducing
np.random.seed(611)


class UltrasoundDataHelper:
    def __init__(
        self,
        dataset="../../../../../../FP-AI-MONITOR1/Ultrasound_DataCampaign/",
        sensor_name="imp23absu",
        sensor_type="mic",
        offset=192000,
        sample_rate=192000,
        frame_len=46 * 4096,
        n_fft=4096,
        hop_length=4096,
        n_mels=32,
        n_mfccs=32,
        classes=["off", "normal", "clogging", "friction"],
        debug=True,
        auto_split_train_ratio=0.8,  # 80% train / 20% test
        auto_split_random_state=611,
        print_file_segment_report=True,
    ):
        """
        Creating the UltrasoundDataHelper object.
        dataset : path to the dataset directory containing the 'meta.txt' file.
        sensor_name : name of the sensor to be used from the acquisitioin folder, [default is 'IMP23ABSU']
        sensor_type : type of the sensor data, [default is 'MIC'].
        offset : number of samples to skip at the start of the acquisition. [default is 192000].
        sample_rate : sampling rate of the sensor acquisition. [default is 192000 samples/sec].
        frame_len = number of samples to use for one inference. [default is 46*4096 (almost 1 sec)].
        n_fft = number of points of fft for Fast-Fourier Transform computation. [default is 4096]. Note as we are not specifying win_len same number will be use for this.
        hop_length = stride for the starting point of the next window. [default is 4096]. Note this value is chosen to have no overlap.
        n_mels = number of mel filters in the mel-filterbank. [default is 32].
        n_mfccs = number of MFCCs to calculate. [default is 32].
        classes =  class labels in the dataset. [default is [ 'off', 'normal', 'clogging', 'friction' ] ]
        """
        self.dataset = dataset
        self.sensor_name = sensor_name
        self.sensor_type = sensor_type
        self.offset = offset
        self.sample_rate = sample_rate
        self.frame_len = frame_len
        self.nFFT = n_fft
        self.hop_length = hop_length
        self.n_mels = n_mels
        self.n_mfccs = n_mfccs
        self.classes = classes
        self.debug = debug
        self.auto_split_train_ratio = float(auto_split_train_ratio)
        self.auto_split_random_state = int(auto_split_random_state)
        self.print_file_segment_report = bool(print_file_segment_report)
        self.last_file_segment_report = pd.DataFrame()

    def _log(self, message: str) -> None:
        if getattr(self, "debug", False):
            print(message)

    def _normalize_train_flag(self, value) -> bool:
        if pd.isna(value):
            return True
        if isinstance(value, str):
            v = value.strip().lower()
            if v in ("1", "true", "t", "yes", "y"):
                return True
            if v in ("0", "false", "f", "no", "n"):
                return False
        return bool(value)

    def _resolve_display_name(self, acq_path: str, acq_name) -> str:
        acq_name_str = "" if pd.isna(acq_name) else str(acq_name).strip()
        if acq_name_str:
            return os.path.basename(os.path.normpath(acq_name_str))
        return os.path.basename(os.path.normpath(acq_path))

    def _build_split_assignments(self, meta: pd.DataFrame) -> pd.Series:
        if "train" in meta.columns:
            train_flags = meta["train"].apply(self._normalize_train_flag)
            if train_flags.all():
                self._log(
                    "[SPLIT] metadata.csv 'train' column is present and all rows are TRAIN; keeping all acquisitions in TRAIN first. If TEST stays empty after feature extraction, a segment-level split will be applied later."
                )
            else:
                self._log(
                    "[SPLIT] Using explicit train/test assignments from metadata.csv 'train' column."
                )
            return train_flags.reset_index(drop=True)

        train_ratio = float(self.auto_split_train_ratio)
        if not (0.0 < train_ratio < 1.0):
            raise ValueError("auto_split_train_ratio must be between 0 and 1, e.g. 0.8 or 0.6")

        test_ratio = 1.0 - train_ratio
        if len(meta) < 2:
            self._log("[SPLIT] Only one acquisition found; assigning it to TRAIN.")
            return pd.Series([True] * len(meta))

        indices = np.arange(len(meta))
        stratify = None
        if "label" in meta.columns:
            label_counts = meta["label"].value_counts(dropna=False)
            if len(label_counts) > 1 and label_counts.min() >= 2:
                stratify = meta["label"]

        try:
            train_idx, test_idx = train_test_split(
                indices,
                train_size=train_ratio,
                test_size=test_ratio,
                random_state=self.auto_split_random_state,
                stratify=stratify,
            )
            self._log(
                f"[SPLIT] Auto split by acquisition: TRAIN={train_ratio:.0%}, TEST={test_ratio:.0%}, "
                f"random_state={self.auto_split_random_state}, stratify={'ON' if stratify is not None else 'OFF'}"
            )
        except Exception as e:
            self._log(
                f"[SPLIT] Stratified acquisition split failed ({e}); falling back to unstratified split."
            )
            train_idx, test_idx = train_test_split(
                indices,
                train_size=train_ratio,
                test_size=test_ratio,
                random_state=self.auto_split_random_state,
                stratify=None,
            )
            self._log(
                f"[SPLIT] Auto split by acquisition: TRAIN={train_ratio:.0%}, TEST={test_ratio:.0%}, "
                f"random_state={self.auto_split_random_state}, stratify=OFF"
            )

        assigned = pd.Series(False, index=np.arange(len(meta)))
        assigned.iloc[train_idx] = True
        return assigned.reset_index(drop=True)

    def _print_file_segment_report(self, report_rows: List[dict]) -> None:
        report_df = pd.DataFrame(report_rows)
        self.last_file_segment_report = report_df

        if report_df.empty:
            self._log("[REPORT] No acquisitions processed.")
            return

        if self.print_file_segment_report:
            self._log("\n[REPORT] Segments per acquisition")
            for _, row in report_df.iterrows():
                split_name = str(row.get("split", "NA")).upper()
                label = str(row.get("label", "NA"))
                display_name = str(row.get("display_name", "NA"))
                n_segments = int(row.get("segments", 0))
                status = str(row.get("status", "ok"))
                source_kind = str(row.get("source_kind", "unknown")).upper()
                self._log(
                    f"  [{split_name}] [{label}] [{source_kind}] {display_name} -> {n_segments} segments ({status})"
                )

        ok_df = report_df[report_df["status"] == "ok"].copy()
        if not ok_df.empty:
            summary = (
                ok_df.groupby(["split", "label"], as_index=False)["segments"]
                .sum()
                .sort_values(["split", "label"])
            )
            self._log("\n[REPORT] Segment totals by split and label")
            for _, row in summary.iterrows():
                self._log(
                    f"  [{str(row['split']).upper()}] [{row['label']}] -> {int(row['segments'])} segments"
                )
        else:
            self._log("\n[REPORT] No valid segments were generated.")

    def _unique_existing_paths(self, candidates: Sequence[Optional[str]]) -> List[str]:
        paths: List[str] = []
        seen = set()
        for path in candidates:
            if not path:
                continue
            norm = os.path.normpath(path)
            if norm in seen:
                continue
            if os.path.exists(norm):
                paths.append(norm)
                seen.add(norm)
        return paths

    def _find_wav_candidates(self, acq_path: str, sensor_name: str, sensor_type: str) -> List[str]:
        sensor_name = str(sensor_name).lower()
        sensor_type = str(sensor_type).lower()

        candidates: List[Optional[str]] = []
        if os.path.isfile(acq_path):
            if acq_path.lower().endswith(".wav"):
                candidates.append(acq_path)
            return self._unique_existing_paths(candidates)

        candidates.extend(
            [
                os.path.join(acq_path, f"{sensor_name}_{sensor_type}.wav"),
                os.path.join(acq_path, f"{sensor_name}.wav"),
                os.path.join(acq_path, f"{sensor_name.upper()}_{sensor_type.upper()}.wav"),
                os.path.join(acq_path, f"{sensor_name.upper()}.wav"),
            ]
        )

        if os.path.isdir(acq_path):
            try:
                wavs = sorted(
                    os.path.join(acq_path, fn)
                    for fn in os.listdir(acq_path)
                    if fn.lower().endswith(".wav")
                )
                candidates.extend(wavs)
            except FileNotFoundError:
                pass

        return self._unique_existing_paths(candidates)

    def _find_dat_folder(self, acq_path: str, sensor_name: str) -> Optional[str]:
        sensor_name = str(sensor_name).lower()

        if os.path.isfile(acq_path):
            if acq_path.lower().endswith(".dat"):
                parent = os.path.dirname(acq_path)
                if parent:
                    return parent
            return None

        candidates = [
            os.path.join(acq_path, f"{sensor_name}.dat"),
            os.path.join(acq_path, f"{sensor_name.upper()}.dat"),
        ]
        for candidate in candidates:
            if os.path.isfile(candidate):
                return acq_path

        if os.path.isdir(acq_path):
            try:
                for fn in os.listdir(acq_path):
                    if fn.lower().endswith(".dat"):
                        return acq_path
            except FileNotFoundError:
                return None

        return None

    def _to_int16_mono(self, data: np.ndarray) -> np.ndarray:
        arr = np.asarray(data)

        if arr.ndim > 1:
            # scipy.io.wavfile.read returns (n_samples, n_channels) for multichannel WAV.
            if arr.shape[1] > 1:
                arr = arr.mean(axis=1)
            else:
                arr = arr[:, 0]

        if np.issubdtype(arr.dtype, np.floating):
            max_abs = float(np.max(np.abs(arr))) if arr.size else 0.0
            if max_abs <= 1.0001:
                arr = arr * 32767.0
            arr = np.clip(np.round(arr), -32768, 32767).astype(np.int16)
        elif arr.dtype != np.int16:
            arr = np.clip(np.round(arr.astype(np.float64)), -32768, 32767).astype(np.int16)

        return arr

    def _resample_int16_if_needed(self, data: np.ndarray, sr: int) -> np.ndarray:
        if int(sr) == int(self.sample_rate):
            return data.astype(np.int16, copy=False)

        y = data.astype(np.float32) / 32768.0
        y = lb.resample(y, orig_sr=int(sr), target_sr=int(self.sample_rate), res_type="kaiser_fast")
        resampled = np.clip(np.round(y * 32768), -32768, 32767).astype(np.int16)
        self._log(f"[WAV] resampled {sr} -> {self.sample_rate}")
        return resampled

    def _read_wav(self, wav_path: str, sample_start: int = 0) -> np.ndarray:
        sr, data = wavfile.read(wav_path)
        data = self._to_int16_mono(data)
        data = self._resample_int16_if_needed(data, sr)
        if sample_start > 0:
            data = data[sample_start:]
        self._log(f"[WAV] {wav_path} | n={len(data)} | offset={sample_start}")
        return data

    def _has_datalog2_markers(self, dat_folder: str) -> bool:
        if not os.path.isdir(dat_folder):
            return False
        names_lower = {fn.lower() for fn in os.listdir(dat_folder)}
        return "device_config.json" in names_lower and "acquisition_info.json" in names_lower

    def _create_hsd_reader(self, dat_folder: str):
        """
        Return a tuple (api_owner, hsd_instance, version_hint).

        api_owner: object on which helper methods may live
        hsd_instance: dataset-specific reader / instance (or the same object)
        version_hint: string such as 'V1' / 'V2' / unknown
        """
        if not HAVE_HSD or _HSDatalog is None:
            return None, None, None

        manager = _HSDatalog()
        version_hint = None

        # New stdatalog_core exposes validate_hsd_folder/create_hsd.
        if hasattr(manager, "validate_hsd_folder"):
            try:
                version_hint = manager.validate_hsd_folder(dat_folder)
            except Exception:
                version_hint = None

        if hasattr(manager, "create_hsd"):
            for kwargs in (
                {"acquisition_folder": dat_folder},
                {"folder_path": dat_folder},
                {"path": dat_folder},
                {},
            ):
                try:
                    if kwargs:
                        hsd_instance = manager.create_hsd(**kwargs)
                    else:
                        hsd_instance = manager.create_hsd(dat_folder)
                    if hsd_instance is not None:
                        return manager, hsd_instance, version_hint
                except TypeError:
                    continue
                except Exception:
                    break

        # Some FP-SNS-DATALOG2 acquisitions are easier to load explicitly as V2.
        if _HSDatalogV2 is not None and self._has_datalog2_markers(dat_folder):
            try:
                hsd_v2 = _HSDatalogV2(dat_folder)
                return hsd_v2, hsd_v2, version_hint or "V2"
            except Exception:
                try:
                    hsd_v2 = _HSDatalogV2()
                    if hasattr(hsd_v2, "create_hsd"):
                        inst = hsd_v2.create_hsd(acquisition_folder=dat_folder)
                        return hsd_v2, inst or hsd_v2, version_hint or "V2"
                except Exception:
                    pass

        # Older backends may operate directly on the manager object.
        return manager, manager, version_hint

    def _call_hsd_method(self, api_owner, hsd_instance, method_names, *args, **kwargs):
        for obj in (api_owner, hsd_instance):
            if obj is None:
                continue
            for method_name in method_names:
                if hasattr(obj, method_name):
                    method = getattr(obj, method_name)
                    try:
                        return method(*args, **kwargs)
                    except TypeError:
                        try:
                            return method(hsd_instance, *args, **kwargs)
                        except Exception:
                            pass
                    except Exception:
                        pass
        return None

    def _extract_component_names(self, obj) -> List[str]:
        names: List[str] = []

        def walk(x):
            if x is None:
                return
            if isinstance(x, str):
                # Keep only simple component-like strings; avoid stringified dicts
                if "{" not in x and "}" not in x and x.strip():
                    names.append(x.strip())
                return
            if isinstance(x, dict):
                # Common explicit name fields
                for k in ("name", "sensor_name", "component_name"):
                    v = x.get(k)
                    if isinstance(v, str) and v.strip():
                        names.append(v.strip())
                # FP-SNS-DATALOG2 / V2 often uses {"imp23absu_mic": {...}}
                if len(x) == 1:
                    only_key = next(iter(x.keys()))
                    if (
                        isinstance(only_key, str)
                        and only_key.strip()
                        and only_key
                        not in (
                            "device",
                            "components",
                            "sensors",
                            "sensor",
                            "schema",
                            "board",
                            "firmware",
                        )
                    ):
                        names.append(only_key.strip())
                # Walk common nested containers first, then remaining values
                for k in ("components", "sensors", "sensor", "device"):
                    if k in x:
                        walk(x.get(k))
                for v in x.values():
                    if isinstance(v, (list, tuple, dict)):
                        walk(v)
                return
            if isinstance(x, (list, tuple, set)):
                for item in x:
                    walk(item)
                return
            n = getattr(x, "name", None)
            if isinstance(n, str) and n.strip():
                names.append(n.strip())

        walk(obj)

        out: List[str] = []
        seen = set()
        for n in names:
            if n and n not in seen:
                seen.add(n)
                out.append(n)
        return out

    def _load_component_names_from_json(self, dat_folder: str) -> List[str]:
        names: List[str] = []
        json_candidates = [
            os.path.join(dat_folder, "device_config.json"),
            os.path.join(dat_folder, "DeviceConfig.json"),
        ]
        for json_path in json_candidates:
            if not os.path.isfile(json_path):
                continue
            try:
                import json

                with open(json_path, "r", encoding="utf-8") as f:
                    cfg = json.load(f)
            except Exception:
                continue
            names = self._extract_component_names(cfg)
            if names:
                break
        return [str(n) for n in names if str(n)]

    def _get_hsd_component_names(
        self, api_owner, hsd_instance, dat_folder: Optional[str] = None
    ) -> List[str]:
        names: List[str] = []

        # stdatalog_core manager-style API
        result = self._call_hsd_method(
            api_owner, hsd_instance, ["get_components_name_list", "get_component_names"]
        )
        if result is not None:
            names = self._extract_component_names(result)

        # Older / V2 instance-style APIs
        if not names:
            result = self._call_hsd_method(
                api_owner, hsd_instance, ["get_sensor_list", "get_sensors", "get_components"]
            )
            if result is not None:
                names = self._extract_component_names(result)

        # Device model dict fallback
        if not names:
            for obj in (hsd_instance, api_owner):
                dm = getattr(obj, "device_model", None)
                names = self._extract_component_names(dm)
                if names:
                    break

        # JSON fallback (important for FP-SNS-DATALOG2 folders with device_config.json)
        if not names and dat_folder:
            names = self._load_component_names_from_json(dat_folder)

        return [str(n) for n in names if str(n)]

    def _pick_component_name(
        self, names: Sequence[str], sensor_name: str, sensor_type: str
    ) -> Optional[str]:
        sensor_name = str(sensor_name).lower()
        sensor_type = str(sensor_type).lower()

        # Prefer exact sensor + type match
        for name in names:
            lower_name = name.lower()
            if sensor_name in lower_name and sensor_type in lower_name:
                return name

        # Then prefer sensor-name-only match
        for name in names:
            if sensor_name in name.lower():
                return name

        # Then any mic/audio-like component
        for name in names:
            lower_name = name.lower()
            if "mic" in lower_name or "audio" in lower_name:
                return name

        return names[0] if names else None

    def _get_dataframe_from_hsd(self, api_owner, hsd_instance, comp_name: str):
        df = None

        # 1) Prefer bound instance methods with the plain component name.
        for obj in (hsd_instance, api_owner):
            if obj is None:
                continue
            for method_name in ("get_dataframe", "get_component_dataframe"):
                method = getattr(obj, method_name, None)
                if not callable(method):
                    continue
                for args in ((comp_name,), (hsd_instance, comp_name)):
                    try:
                        result = method(*args)
                        if result is not None:
                            df = result
                            break
                    except Exception:
                        pass
                if df is not None:
                    break
            if df is not None:
                break

        # 2) Some APIs require a component object instead of its name.
        if df is None:
            comp = None
            for obj in (hsd_instance, api_owner):
                if obj is None:
                    continue
                method = getattr(obj, "get_component", None)
                if not callable(method):
                    continue
                try:
                    comp = method(comp_name)
                except Exception:
                    try:
                        comp = method(hsd_instance, comp_name)
                    except Exception:
                        comp = None
                if comp is not None:
                    break

            if comp is not None:
                for obj in (hsd_instance, api_owner):
                    if obj is None:
                        continue
                    for method_name in ("get_dataframe", "get_component_dataframe"):
                        method = getattr(obj, method_name, None)
                        if not callable(method):
                            continue
                        for args in ((comp,), (hsd_instance, comp)):
                            try:
                                result = method(*args)
                                if result is not None:
                                    df = result
                                    break
                            except Exception:
                                pass
                        if df is not None:
                            break
                    if df is not None:
                        break

        if isinstance(df, (list, tuple)) and len(df) > 0:
            df = df[0]

        return df

    def _pick_signal_column(self, df: pd.DataFrame) -> Optional[str]:
        preferred = ("MIC", "mic", "audio", "data", "Signal", "signal")
        for col in preferred:
            if col in df.columns:
                return col

        numeric_cols = [
            col
            for col in df.columns
            if str(col).lower() not in ("time", "timestamp")
            and np.issubdtype(df[col].dtype, np.number)
        ]
        if numeric_cols:
            return str(numeric_cols[0])

        return None

    def _read_dat(self, dat_folder: str, sensor_name: str, sensor_type: str, sample_start: int = 0):
        if not HAVE_HSD or _HSDatalog is None:
            self._log(
                f"[SKIP] DAT exists but HSD SDK is unavailable: {dat_folder}. "
                f"Install STDATALOG-PYSDK/stdatalog_core or HSDPython_SDK."
            )
            return None

        try:
            api_owner, hsd_instance, version_hint = self._create_hsd_reader(dat_folder)
            if hsd_instance is None:
                self._log(f"[SKIP] Unable to create HSD reader for: {dat_folder}")
                return None

            names = self._get_hsd_component_names(api_owner, hsd_instance, dat_folder=dat_folder)
            comp_name = self._pick_component_name(names, sensor_name, sensor_type)

            if not comp_name:
                self._log(
                    f"[SKIP] Cannot find matching sensor in DAT folder: {dat_folder} | "
                    f"available={names}"
                )
                return None

            df = self._get_dataframe_from_hsd(api_owner, hsd_instance, comp_name)
            if df is None:
                self._log(
                    f"[SKIP] Failed to load dataframe from DAT folder: {dat_folder} | "
                    f"comp={comp_name} | version={version_hint}"
                )
                return None
            if isinstance(df, pd.DataFrame) and df.empty:
                self._log(
                    f"[SKIP] Empty dataframe from DAT folder: {dat_folder} | comp={comp_name}"
                )
                return None

            col = self._pick_signal_column(df)
            if col is None:
                self._log(
                    f"[SKIP] No signal column found in DAT folder: {dat_folder} | "
                    f"comp={comp_name} | cols={list(df.columns) if hasattr(df, 'columns') else 'NA'}"
                )
                return None

            data = df[col].to_numpy()
            data = self._to_int16_mono(data)

            if sample_start > 0:
                data = data[sample_start:]

            self._log(
                f"[DAT] {dat_folder} | backend={_HSD_BACKEND} | version={version_hint} | "
                f"comp={comp_name} | col={col} | n={len(data)} | offset={sample_start}"
            )
            return data

        except Exception as e:
            self._log(f"[SKIP] Failed to read DAT: {dat_folder} | reason: {e}")
            return None

        try:
            hsd = _HSDatalog().create_hsd(dat_folder)
            names = self._get_hsd_component_names(hsd)
            comp_name = self._pick_component_name(names, sensor_name, sensor_type)

            if not comp_name:
                self._log(f"[SKIP] Cannot find matching sensor in DAT folder: {dat_folder}")
                return None

            df = self._get_dataframe_from_hsd(hsd, comp_name)
            if df is None:
                self._log(f"[SKIP] Failed to load dataframe from DAT folder: {dat_folder}")
                return None
            if isinstance(df, pd.DataFrame) and df.empty:
                self._log(f"[SKIP] Empty dataframe from DAT folder: {dat_folder}")
                return None

            col = self._pick_signal_column(df)
            if col is None:
                self._log(f"[SKIP] No signal column found in DAT folder: {dat_folder}")
                return None

            data = df[col].to_numpy()
            data = self._to_int16_mono(data)

            if sample_start > 0:
                data = data[sample_start:]

            self._log(
                f"[DAT] {dat_folder} | comp={comp_name} | col={col} | n={len(data)} | offset={sample_start}"
            )
            return data

        except Exception as e:
            self._log(f"[SKIP] Failed to read DAT: {dat_folder} | reason: {e}")
            return None

    def _resolve_acquisition_path(self, location, acq_name) -> str:
        """
        Build an acquisition path robustly from metadata.
        Supports:
        - dataset/location/acqName
        - dataset/acqName
        - acqName already being an absolute or relative file path
        """
        acq_name_str = "" if pd.isna(acq_name) else str(acq_name).strip()
        location_str = "" if pd.isna(location) else str(location).strip()

        candidates: List[str] = []

        if acq_name_str:
            if os.path.isabs(acq_name_str):
                candidates.append(acq_name_str)
            else:
                if location_str and location_str not in (".", "./", "nan", "None"):
                    candidates.append(os.path.join(self.dataset, location_str, acq_name_str))
                candidates.append(os.path.join(self.dataset, acq_name_str))
                candidates.append(acq_name_str)

        if location_str and not acq_name_str and location_str not in (".", "./", "nan", "None"):
            candidates.append(os.path.join(self.dataset, location_str))

        for candidate in candidates:
            if os.path.exists(candidate):
                return candidate

        # Fallback to the conventional original behavior
        if location_str and location_str not in (".", "./", "nan", "None"):
            return os.path.join(self.dataset, location_str, acq_name_str)
        return os.path.join(self.dataset, acq_name_str)

    def read_hsd_ultrasound(
        self, acq_folder, sensor_name="imp23absu", sensor_type="mic", sample_start=0
    ):
        """
        Reading a high-speed datalog acquisition for ultrasound (analog microphone)
        acq_folder : path to the acquisiton directory containing IMP23ABSU.dat and DeviceConfig.json files
                     or a direct path to a .wav file.
        sensor_name : Name of the analog microphone sensor ['IMP23ABSU']
        sensor_type : name of the sensor type ['MIC']
        sample_start : the first sample to consider when parsing the acquisition. This parameter is used to ignore starting samples.
        returns
        data : 16 bit integer values of the audio data array.
        """
        acq_folder = str(acq_folder)

        wav_candidates = self._find_wav_candidates(acq_folder, sensor_name, sensor_type)
        if wav_candidates:
            return self._read_wav(wav_candidates[0], sample_start=sample_start)

        dat_folder = self._find_dat_folder(acq_folder, sensor_name)
        if dat_folder:
            return self._read_dat(dat_folder, sensor_name, sensor_type, sample_start=sample_start)

        self._log(f"[SKIP] Neither WAV nor DAT found: {acq_folder}")
        return None

    def get_segment_indices(self, dataLen, seqLen, seqStep):
        """
        creates segment indices to window the data into (non) overlapping frames
        dataLen : length of the array (data)
        seqLen : segment length to be used for framing
        seqStep : stride from start of n frame to the n + 1, should be < seqLen,  (this is used to control the overlap, i.e. if seqLen and seqStep are equal no overlap)
        returns
        a sequence of start and end indices of the segments in the database (data array)
        """
        init = 0
        while init < dataLen:
            yield int(init), int(init + seqLen)
            init = init + seqStep

    def get_data_segments(self, dataset, seqLen, seqStep):
        """
        Creates the segments/frames of the data

        dataset : the dataset to be segmented/framed
        seqLen : segment length to be used for framing
        seqStep : stride from start of n frame to the n + 1, should be < seqLen,  (this is used to control the overlap, i.e. if seqLen and seqStep are equal no overlap)
        returns
        frames/segments of the data of length seqLen
        """

        # segmentng the data into overlaping frames
        segments = []
        # creating segments until the get_segment_indices keep on yielding the start and end of the segments
        for init, end in self.get_segment_indices(len(dataset), seqLen, seqStep):
            # check if the nr of remaing samples are enough to create a frame
            if end <= len(dataset):
                segments.append(dataset[init:end])

        # converting the segments from list to numpy array
        segments = np.asarray(segments, dtype=np.float32)
        return segments

    def _segment_level_split(self, MFCCsAll, labelsAll):
        train_ratio = float(self.auto_split_train_ratio)
        if not (0.0 < train_ratio < 1.0):
            raise ValueError("auto_split_train_ratio must be between 0 and 1, e.g. 0.8 or 0.6")

        test_ratio = 1.0 - train_ratio
        if MFCCsAll.shape[0] < 2:
            raise ValueError("Need at least 2 segments to split train/test.")

        y_all = np.argmax(labelsAll, axis=1)
        stratify = None
        unique, counts = np.unique(y_all, return_counts=True)
        if len(unique) > 1 and counts.min() >= 2:
            stratify = y_all

        try:
            return train_test_split(
                MFCCsAll,
                labelsAll,
                train_size=train_ratio,
                test_size=test_ratio,
                random_state=self.auto_split_random_state,
                stratify=stratify,
            )
        except Exception as e:
            self._log(
                f"[SPLIT] Stratified segment split failed ({e}); falling back to unstratified split."
            )
            return train_test_split(
                MFCCsAll,
                labelsAll,
                train_size=train_ratio,
                test_size=test_ratio,
                random_state=self.auto_split_random_state,
                stratify=None,
            )

    def get_data_mfccs(self):
        """
        This function prepares the data for training and testing the model using the metadata.csv
        file in the dataset root. One row in metadata.csv represents one acquisition.

        Split behavior:
        - If metadata.csv contains a 'train' column and at least one row has train=0, those
          explicit assignments are respected.
        - Otherwise, acquisitions are automatically split BEFORE feature extraction, using
          auto_split_train_ratio (default 0.8 -> 80% train / 20% test).

        This function also prints a per-acquisition segment report such as:
            20260228_17_21_45 -> 915 segments
        and stores the full report in self.last_file_segment_report.

        Returns
        MFCCsTrain : Train data tensor
        labelsTrain : one-hot coded Train data labels
        MFCCsTest : Test data tensor
        labelsTest : one-hot coded Test data labels
        """
        meta = pd.read_csv(os.path.join(self.dataset, "metadata.csv")).reset_index(drop=True)

        # nFrames = ceil ( ( self.frame_len - self.nFFT ) / self.hop_length  )
        nFrames = int((self.frame_len - self.nFFT) / self.hop_length) + 1
        MFCCsTrain = np.empty((0, self.n_mfccs, nFrames, 1), dtype=np.float32)
        labelsTrain = np.empty((0, len(self.classes)), dtype=np.float32)

        MFCCsTest = np.empty((0, self.n_mfccs, nFrames, 1), dtype=np.float32)
        labelsTest = np.empty((0, len(self.classes)), dtype=np.float32)

        effective_train = self._build_split_assignments(meta)
        report_rows: List[dict] = []

        for i in range(len(meta)):
            location = meta.location[i] if "location" in meta.columns else ""
            acq_name = meta.acqName[i] if "acqName" in meta.columns else ""
            label_name = meta.label[i] if "label" in meta.columns else ""
            acq_path = self._resolve_acquisition_path(location, acq_name)
            display_name = self._resolve_display_name(acq_path, acq_name)
            split_name = "train" if bool(effective_train.iloc[i]) else "test"

            data = self.read_hsd_ultrasound(
                acq_path, self.sensor_name, self.sensor_type, self.offset
            )

            source_kind = "wav" if str(acq_path).lower().endswith(".wav") else "dat_or_folder"
            if data is None:
                print(f"[SKIP] unreadable acquisition: {acq_path}")
                report_rows.append(
                    {
                        "index": i,
                        "label": label_name,
                        "split": split_name,
                        "display_name": display_name,
                        "acq_path": acq_path,
                        "source_kind": source_kind,
                        "segments": 0,
                        "status": "unreadable",
                    }
                )
                continue

            # Re-check actual source after read_hsd_ultrasound resolves folder contents.
            wav_candidates = self._find_wav_candidates(acq_path, self.sensor_name, self.sensor_type)
            dat_folder = self._find_dat_folder(acq_path, self.sensor_name)
            if wav_candidates:
                source_kind = "wav"
            elif dat_folder:
                source_kind = "dat"

            dataSegments = self.get_data_segments(
                data, seqLen=self.frame_len, seqStep=self.frame_len
            )

            if dataSegments is None or len(dataSegments) == 0:
                print(f"[SKIP] no valid segments: {acq_path}")
                report_rows.append(
                    {
                        "index": i,
                        "label": label_name,
                        "split": split_name,
                        "display_name": display_name,
                        "acq_path": acq_path,
                        "source_kind": source_kind,
                        "segments": 0,
                        "status": "no_segments",
                    }
                )
                continue

            _MFCCs = []
            for ii in range(dataSegments.shape[0]):
                mfcc = lb.feature.mfcc(
                    y=dataSegments[ii, :],
                    sr=self.sample_rate,
                    n_mfcc=self.n_mfccs,
                    n_mels=self.n_mels,
                    n_fft=self.nFFT,
                    hop_length=self.hop_length,
                    center=False,
                )
                _MFCCs.append(mfcc)
            _MFCCs = np.asarray(_MFCCs, dtype=np.float32)
            _MFCCs = _MFCCs.reshape((_MFCCs.shape[0], _MFCCs.shape[1], _MFCCs.shape[2], 1))
            _labels = np.repeat(
                to_categorical(
                    self.classes.index(label_name), num_classes=len(self.classes)
                ).reshape(1, -1),
                len(_MFCCs),
                axis=0,
            ).astype(np.float32)

            if bool(effective_train.iloc[i]):
                MFCCsTrain = np.concatenate((MFCCsTrain, _MFCCs), axis=0)
                labelsTrain = np.concatenate((labelsTrain, _labels), axis=0)
            else:
                MFCCsTest = np.concatenate((MFCCsTest, _MFCCs), axis=0)
                labelsTest = np.concatenate((labelsTest, _labels), axis=0)

            report_rows.append(
                {
                    "index": i,
                    "label": label_name,
                    "split": split_name,
                    "display_name": display_name,
                    "acq_path": acq_path,
                    "source_kind": source_kind,
                    "segments": int(len(_MFCCs)),
                    "status": "ok",
                }
            )

        if MFCCsTest.shape[0] == 0 and MFCCsTrain.shape[0] > 1:
            self._log(
                f"[SPLIT] No TEST segments were produced from metadata/file-level assignment; applying segment-level split with train_ratio={self.auto_split_train_ratio:.0%}, test_ratio={1.0 - self.auto_split_train_ratio:.0%}, random_state={self.auto_split_random_state}."
            )
            MFCCsTrain, MFCCsTest, labelsTrain, labelsTest = self._segment_level_split(
                MFCCsTrain, labelsTrain
            )
        elif MFCCsTest.shape[0] == 0 and MFCCsTrain.shape[0] <= 1:
            self._log("[SPLIT] TEST is empty, but there are not enough segments to split train/test.")

        self._print_file_segment_report(report_rows)

        MFCCsTrain, labelsTrain = shuffle(
            MFCCsTrain, labelsTrain, random_state=self.auto_split_random_state
        )
        MFCCsTest, labelsTest = shuffle(
            MFCCsTest, labelsTest, random_state=self.auto_split_random_state
        )

        self._log(
            f"\n[FINAL] Train samples={MFCCsTrain.shape[0]}, Test samples={MFCCsTest.shape[0]}"
        )
        if labelsTrain.shape[0] > 0:
            train_counts = np.bincount(np.argmax(labelsTrain, axis=1), minlength=len(self.classes))
            self._log(f"[FINAL] Train per class={train_counts.tolist()}")
        if labelsTest.shape[0] > 0:
            test_counts = np.bincount(np.argmax(labelsTest, axis=1), minlength=len(self.classes))
            self._log(f"[FINAL] Test per class={test_counts.tolist()}")

        return MFCCsTrain, labelsTrain, MFCCsTest, labelsTest

    def get_data_mfcc_single_acquisition(self, acq_path, label):
        """
        This fuction prepares the data tensor for the model and the one-hot coded labels for the ground truth.
        This function uses all the variables from the class object when created and doesnot requires any additional input but only the acquisition folder path and label.

        acq_path : path to the acquisition folder with the IMP23ABSU.dat and DeviceConfig.json files.
                   A direct .wav path is also supported.
        label : the string label to be used for the data (one of the labels in the classes list)
        Returns
        mfccs : input data tensor
        labels : one-hot coded labels
        """
        mfccs = []
        labels = np.empty((0, len(self.classes)))

        data = self.read_hsd_ultrasound(acq_path, self.sensor_name, self.sensor_type, self.offset)

        if data is None:
            raise RuntimeError(f"Cannot read acquisition: {acq_path}")

        if len(data) < self.frame_len:
            raise RuntimeError(f"Acquisition too short: {acq_path}")

        dataSegments = self.get_data_segments(data, seqLen=self.frame_len, seqStep=self.frame_len)
        for ii in range(dataSegments.shape[0]):
            mfcc = lb.feature.mfcc(
                y=dataSegments[ii, :],
                sr=self.sample_rate,
                n_mfcc=self.n_mfccs,
                n_mels=self.n_mels,
                n_fft=self.nFFT,
                hop_length=self.hop_length,
                center=False,
            )
            mfccs.append(mfcc)
        mfccs = np.asarray(mfccs, dtype=np.float32)
        mfccs = mfccs.reshape((mfccs.shape[0], mfccs.shape[1], mfccs.shape[2], 1))
        labels = np.repeat(
            to_categorical(self.classes.index(label), num_classes=len(self.classes)).reshape(1, -1),
            len(mfccs),
            axis=0,
        )
        mfccs, labels = shuffle(mfccs, labels, random_state=611)
        return mfccs, labels

    def dump_data_for_post_validation(
        self,
        testX,
        testY,
        nrSamples,
        output_path: str = "post_validation_data.npz",
    ):
        """
        Create a reduced validation dump for STM32-CUBE-AI post validation.

        This randomizes the provided test data and saves up to nrSamples samples
        for each class into an .npz file with keys x_test and y_test.

        Example
        -------
        # keep only 2 samples per class
        x_small, y_small = myUDH.dump_data_for_post_validation(
            mfccsTest, labelsTest, nrSamples=2,
            output_path="post_validation_data_small.npz"
        )
        """
        nrSamples = int(nrSamples)
        if nrSamples <= 0:
            raise ValueError("nrSamples must be > 0")

        # randomize the order of the sequences
        testX, testY = shuffle(testX, testY, random_state=611)

        if len(testY.shape) > 1:
            x_test = np.empty((0, testX.shape[1], testX.shape[2], testX.shape[3]), dtype=testX.dtype)
            y_test = np.empty((0, len(self.classes)), dtype=testY.dtype)

            for i in range(testY.shape[1]):
                inds = np.where(np.argmax(testY, axis=1) == i)[0][:nrSamples]
                x_test = np.concatenate((x_test, testX[inds, :]), axis=0)
                y_test = np.concatenate((y_test, testY[inds, :]), axis=0)
        else:
            x_test = np.empty((0, testX.shape[1]), dtype=testX.dtype)
            y_test = np.empty((0,), dtype=testY.dtype)

            for i in list(np.unique(testY)):
                inds = np.where(testY == i)[0][:nrSamples]
                x_test = np.concatenate((x_test, testX[inds]), axis=0)
                y_test = np.concatenate((y_test, testY[inds]), axis=0)

        np.savez(output_path, x_test=x_test, y_test=y_test)
        self._log(
            f"[POST-VAL] saved {output_path} | x_test={x_test.shape} | y_test={y_test.shape} | "
            f"nrSamples/class={nrSamples}"
        )
        return x_test, y_test

    def shrink_existing_post_validation_npz(
        self,
        npz_path: str,
        nrSamples: int,
        output_path: str = "post_validation_data_small.npz",
    ) -> Tuple[np.ndarray, np.ndarray]:
        """
        Re-shrink an already exported post_validation_data.npz file.

        Useful when validate-on-target times out with a large validation set.
        This loads x_test/y_test from an existing npz, then keeps only up to
        nrSamples samples per class and writes a smaller npz.

        Example
        -------
        x_small, y_small = myUDH.shrink_existing_post_validation_npz(
            npz_path=r"...\post_validation_data.npz",
            nrSamples=2,
            output_path="post_validation_data_small.npz"
        )
        """
        obj = np.load(npz_path)
        if "x_test" not in obj or "y_test" not in obj:
            raise KeyError(f"{npz_path} must contain x_test and y_test")

        testX = obj["x_test"]
        testY = obj["y_test"]
        return self.dump_data_for_post_validation(
            testX=testX,
            testY=testY,
            nrSamples=nrSamples,
            output_path=output_path,
        )


# 2026
