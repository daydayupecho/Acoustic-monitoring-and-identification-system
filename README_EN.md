# Acoustic Monitoring and Identification System

[中文版](README.md)

This repository contains the software components for an acoustic monitoring and identification system. It includes a PC host application for dataset acquisition, wake-word data conversion tools, command-word preprocessing scripts, command-word model training scripts, and an STM32 firmware project.

## Repository Structure

```text
Acoustic-monitoring-and-identification-system/
├── host-acquisition/              # PC host program for dataset acquisition
├── wake-word/                     # Wake-word data processing tools
│   └── nanoedge-converter/
├── command-word/                  # Command-word preprocessing and model training
│   ├── preprocessing/
│   └── model-training/
├── stm32-firmware/                # STM32 embedded firmware project
└── bluetooth-receiver-host/       # Reserved directory for Bluetooth receiver host software
```

## Program Description

### 1. Dataset Acquisition Host Program

Path: `host-acquisition/host_acquisition_gui.py`

This is a Python-based PC host program for dataset acquisition. It receives audio or ultrasonic data from the embedded device through a serial port, displays the waveform in real time, and saves the collected dataset.

Main features:

- Automatically lists available serial ports and connects to the acquisition device.
- Sends control commands such as `START`, `STOP`, and `PING` to the embedded device.
- Parses the incoming serial data stream and displays the waveform in real time.
- Saves acquisition data in an ST HSDatalog-style format, including `.dat` files, `acquisition_info.json`, and `device_config.json`.
- Provides raw data for later wake-word and command-word processing workflows.

Typical use cases:

- Collect wake-word, command-word, or environmental acoustic datasets.
- Debug embedded audio acquisition and serial transmission.
- Quickly check whether the acquired waveform is valid.

### 2. Wake-Word Dataset Format Converter

Path: `wake-word/nanoedge-converter/`

This directory contains tools for converting custom STM32/ST-HSD-style acquisition data into CSV files that can be imported into NanoEdge AI Studio. It is mainly used for wake-word recognition or anomaly-detection dataset preparation.

#### `dat_to_nanoedge_csv.py`

Core conversion script.

Main features:

- Reads `.dat` files and JSON configuration files from each acquisition folder.
- Removes the 4-byte transport counter added by the communication protocol.
- Removes periodic timestamp fields from the data stream.
- Splits audio samples into fixed-length windows with a configurable step size.
- Exports each window as a CSV row or column for NanoEdge AI.
- Supports conversion parameters such as sensor name, sampling rate, signal length, signal increment, and CSV layout.

Typical usage:

```powershell
python dat_to_nanoedge_csv.py <acquisition_root> -o <output_dir> -s imp23absu_mic -sl 8192 -si 8192 --odr 16000 --csv-shape row
```

#### `run_nanoedge_conversion.bat`

A Windows batch script that calls `dat_to_nanoedge_csv.py` for batch conversion.

Before running it, update the following variables according to your local dataset:

- `ROOT`: input acquisition root directory.
- `OUT_ALL`: output directory for generated CSV files.
- `SL`: signal/window length.
- `SI`: signal/window increment.
- `ODR`: sampling rate.

### 3. Command-Word Dataset Preprocessing Tools

Path: `command-word/preprocessing/`

This directory contains scripts for converting raw command-word acquisition data into a dataset format suitable for model training.

#### `convert_stdatalog_to_wav.ps1`

A PowerShell batch script for converting ST DATALOG/STDATALOG acquisition folders into WAV files. It can also generate failure reports and metadata files for later training.

Main features:

- Scans command-word acquisition folders in batch.
- Calls ST's `stdatalog_to_wav.py` tool to convert `.dat` acquisition data into `.wav`.
- Supports parallel conversion to speed up large dataset processing.
- Uses a pinned device JSON configuration to avoid repeated catalog writes during parallel execution.
- Organizes converted WAV files by class and acquisition name.
- Records failed conversion items and exports CSV/XLSX failure reports.
- Optionally calls a post-processing script to generate `metadata.csv` and restructure the training dataset.

Notes:

- This script contains local absolute paths. Before publishing or reproducing the workflow, update variables such as `$Root`, `$Out`, `$Py`, `$ToWav`, and `$PinnedJsonPath`.
- PowerShell 7 or later is required.
- The ST DATALOG/STDATALOG Python SDK environment must be configured in advance.

#### `audit_wav_and_metadata.py`

A dataset auditing script for WAV files and `metadata.csv`.

Main features:

- Checks whether every item in `metadata.csv` has a corresponding WAV file.
- Checks whether each WAV file contains enough samples for the model input length.
- Reports missing files, read errors, and samples that are too short.
- Generates `audit_report.csv` to help locate abnormal dataset entries.

Typical usage:

```powershell
python audit_wav_and_metadata.py <dataset_root> <frame_len> [offset]
```

Example:

```powershell
python audit_wav_and_metadata.py E:\SPEECH_DATA\20251029-command 188416 0
```

### 4. Command-Word Recognition Model Training Scripts

Path: `command-word/model-training/`

This directory is used for command-word model data preparation, feature extraction, training, evaluation, and visualization.

#### `prepare_command_dataset.py`

A command-word dataset preparation script that provides the `UltrasoundDataHelper` data processing class.

Main features:

- Loads command-word WAV datasets and metadata.
- Loads samples by class, such as fan on/off, light on/off, TV on/off, and other command categories.
- Applies offset trimming, framing, and sample segmentation to raw audio.
- Extracts features such as Mel spectrograms and MFCCs.
- Automatically splits the dataset into training, validation, and test sets.
- Prepares input data and labels for TensorFlow/Keras model training.

Typical use cases:

- Prepare data before training a command-word classification model.
- Experiment with different sampling rates, frame lengths, and MFCC parameters.
- Check whether dataset splits and feature shapes match the model input requirements.

#### `train_command_classifier.ipynb`

A Jupyter Notebook for command-word classification model training.

Main features:

- Calls the dataset preparation script to generate training data.
- Builds and trains a command-word classification model.
- Outputs the training process and evaluation metrics.
- Exports classification reports, performance results, and visualization figures.
- Supports ROC, t-SNE, and MFCC clustering analysis to inspect class separability.

Typical use cases:

- Interactive model training and hyperparameter tuning.
- Presenting model training results.
- Generating evaluation figures for papers, reports, or presentations.

### 5. STM32 Embedded Firmware Project

Path: `stm32-firmware/Tx_LowPower_echo/`

This directory contains the embedded firmware project for the STM32U575 platform. It includes audio acquisition, feature extraction, AI inference, UART debugging, and low-power related code. Project files for STM32CubeIDE, Keil MDK-ARM, and IAR EWARM are kept in the repository.

Main contents:

- `Src/`: application source code, including audio acquisition, feature extraction, AI inference, UART debugging, and ThreadX tasks.
- `Inc/`: header files and model-related configuration files.
- `STM32CubeIDE/`: STM32CubeIDE project files.
- `MDK-ARM/`: Keil MDK project files.
- `EWARM/`: IAR EWARM project files.
- `Middlewares/`: ST AI library, NanoEdge AI library, and other middleware.
- `docs/`: flowcharts, BOM files, and system documentation figures.
- `Tx_LowPower.ioc`: STM32CubeMX project configuration file.

Main features:

- Acquires microphone audio or ultrasonic data on STM32U575.
- Performs front-end feature extraction, such as windowing, FFT, Mel filtering, and MFCC extraction.
- Runs embedded AI inference for command-word or acoustic state recognition.
- Outputs debug information or recognition results through UART.
- Provides a ThreadX-based low-power framework.

### 6. Bluetooth Receiver Host Program

Path: `bluetooth-receiver-host/`

This directory is currently empty. It is reserved for the Bluetooth receiver host synchronization and display program. If this function has already been merged into `host-acquisition/`, this empty directory can be removed to avoid confusion for GitHub readers.

## Recommended Workflow

1. Use `host-acquisition/host_acquisition_gui.py` to connect to the embedded device and collect raw data.
2. For wake-word data, use `wake-word/nanoedge-converter/dat_to_nanoedge_csv.py` to convert raw acquisition data into NanoEdge AI CSV files.
3. For command-word data, use `command-word/preprocessing/convert_stdatalog_to_wav.ps1` to convert raw acquisition data into WAV files.
4. Use `command-word/preprocessing/audit_wav_and_metadata.py` to check WAV files and metadata integrity.
5. Use `command-word/model-training/prepare_command_dataset.py` and `train_command_classifier.ipynb` to train and evaluate the command-word recognition model.
6. Deploy the trained model and parameters into the corresponding firmware project under `stm32-firmware/Tx_LowPower_echo/`.

## Suggestions Before Publishing to GitHub

- Replace local absolute paths in scripts with command-line arguments, configuration files, or example paths in documentation.
- Check whether intermediate files, debug logs, temporary caches, and IDE build outputs should be excluded.
- If the firmware contains third-party libraries or ST/NanoEdge AI generated files, confirm that their licenses allow public sharing.
- Add more specific running instructions inside each subdirectory to make the workflow easier to reproduce.
