# Acoustic Monitoring and Identification System

[Chinese version](README_CH.md)

This repository archives the software components used in an acoustic monitoring and identification system. It covers the complete workflow from dataset acquisition, data format conversion, command-word model training, and STM32-side deployment to the optional UART-based PC monitoring interface for the Bluetooth receiver.

The repository contains:

- A PC host program for dataset acquisition
- Wake-word dataset conversion tools
- Command-word preprocessing scripts
- Command-word recognition model training scripts
- An STM32 embedded firmware project
- UART-based PC monitoring software for synchronized display of appliance states
- Circuit diagrams of the core board, expansion board, and Bluetooth receiver-relay control board

## Repository Structure

```text
Acoustic-monitoring-and-identification-system/
├── host-acquisition/              # Python host program for dataset acquisition
├── wake-word/                     # Wake-word data processing tools
│   └── nanoedge-converter/
├── command-word/                  # Command-word preprocessing and model training
│   ├── preprocessing/
│   └── model-training/
├── stm32-firmware/                # STM32 embedded firmware project
│   └── Tx_LowPower_echo/
├── bluetooth-receiver-host/       # UART-based PC monitoring software for appliance-state display
└── docs/                          # Documentation and hardware circuit diagrams
    └── circuit-diagrams/
```

## 1. Dataset Acquisition Host Program

Path: `host-acquisition/host_acquisition_gui.py`

This Python program communicates with the STM32 embedded device through a serial port, receives the audio data acquired by the device, displays the waveform in real time, and saves the dataset.

Main functions:

- Enumerates and connects to serial devices.
- Sends control commands such as `START`, `STOP`, and `PING` to the embedded device.
- Receives and parses audio sample data from the embedded device.
- Displays the acquired waveform in real time for acquisition-quality inspection.
- Saves ST HSDatalog-style data files, including `.dat`, `acquisition_info.json`, and `device_config.json`.
- Provides raw acquisition data for later wake-word and command-word processing.

Runtime environment:

- Python 3.13 was used in this work; Python 3.10 or later is recommended.
- `pyserial`
- `matplotlib`

Before running the program, confirm that:

- The STM32 firmware is configured in data acquisition mode.
- The serial port and baud rate are correctly configured.
- The output directory has write permission.

## 2. Wake-Word Dataset Format Converter

Path: `wake-word/nanoedge-converter/`

This directory converts STM32/ST-HSD-style acquisition data saved by the host program into CSV files that can be imported into NanoEdge AI Studio. It is mainly used for wake-word recognition or anomaly-detection dataset preparation.

### `dat_to_nanoedge_csv.py`

Core conversion script.

Main functions:

- Reads `.dat` data files and JSON configuration files from acquisition folders.
- Removes the 4-byte protocol counter added during data transmission.
- Removes periodic timestamp fields inserted in the data stream.
- Splits audio samples according to the specified window length and window step.
- Exports CSV files that can be imported into NanoEdge AI Studio.
- Supports configurable sensor name, sampling rate, window length, window step, and CSV layout.

Typical command:

```powershell
python dat_to_nanoedge_csv.py <acquisition_data_root> -o <output_dir> -s imp23absu_mic -sl 8192 -si 8192 --odr 16000 --csv-shape row
```

Common parameters:

- `<acquisition_data_root>`: root directory containing multiple acquisition folders.
- `-o`: output directory for generated CSV files.
- `-s`: sensor name, for example `imp23absu_mic`.
- `-sl`: sample length of each window.
- `-si`: step between adjacent windows.
- `--odr`: sampling rate.
- `--csv-shape`: CSV output layout supported by the script.

### `run_nanoedge_conversion.bat`

A Windows batch example for calling `dat_to_nanoedge_csv.py` in batch mode.

Before running it, update the following variables according to the local dataset:

- `ROOT`: input data root directory.
- `OUT_ALL`: CSV output directory.
- `SL`: window length.
- `SI`: window step.
- `ODR`: sampling rate.

## 3. Command-Word Dataset Preprocessing Tools

Path: `command-word/preprocessing/`

This directory converts and organizes raw command-word acquisition data into the dataset format required for model training.

### `convert_stdatalog_to_wav.ps1`

A PowerShell batch-processing script that converts STM32/ST-HSD-style acquisition data saved by the host program into WAV files, and generates failure reports and metadata for training.

Main functions:

- Scans command-word acquisition directories in batch.
- Calls ST's `stdatalog_to_wav.py` tool to convert `.dat` data into `.wav`.
- Supports parallel processing with PowerShell 7 to speed up large-batch conversion.
- Uses a pinned device JSON configuration to avoid repeated catalog writes during parallel conversion.
- Organizes WAV output directories by command class and acquisition name.
- Exports conversion failure records as CSV/XLSX reports.
- Optionally runs post-processing to generate `metadata.csv` and organize the training directory.

Runtime environment:

- PowerShell 7.
- ST DATALOG/STDATALOG Python SDK environment.
- Copy the script to `<STDATALOG-PYSDK_ROOT>\stdatalog-pysdk\`, then configure the STDATALOG-PYSDK environment, script location, and path parameters according to Sections 3-6 of `command-word/preprocessing/ENVIRONMENT.md`.
- `$Py` should point to the Python interpreter in the STDATALOG-PYSDK virtual environment, for example `<STDATALOG-PYSDK_ROOT>\stdatalog-pysdk\.venv_dlog\Scripts\python.exe`.
- `$ToWav` should point to the official ST conversion script, for example `<STDATALOG-PYSDK_ROOT>\stdatalog-pysdk\stdatalog_examples\cli_applications\stdatalog_to_wav.py`.

Notes:

- Set `$Root`, `$Out`, `$StageRoot`, `$Py`, `$ToWav`, `$PinnedJsonPath`, `$JsonSources`, and `$DtdlDir` at the beginning of the script according to the local environment.
- `$Root` points to the command-word `.dat` acquisition data root directory saved by the host program.
- `$Out` points to the converted WAV output directory.
- If XLSX report export is enabled, install `openpyxl` in the corresponding Python environment.

### `audit_wav_and_metadata.py`

A checking script for WAV files and `metadata.csv`.

Main functions:

- Checks whether each sample recorded in `metadata.csv` has a corresponding WAV file.
- Checks whether each WAV file satisfies the required model input length.
- Reports missing files, read failures, or samples with insufficient length.
- Generates `audit_report.csv` for locating abnormal dataset samples.

Runtime environment:

- Python 3.x
- `soundfile`

Typical command:

```powershell
python audit_wav_and_metadata.py <dataset_root> <frame_len> [offset]
```

Example:

```powershell
cd D:\workspace\project\keil\thesis_speech_recognition
python .\audit_wav_and_metadata.py "E:\SPEECH_DATA\TENG\touming-dataset\command-20260315-merge" 19456 0
```

Here, `19456` is the required number of WAV samples for this check, and `0` is the reading offset.

## 4. Command-Word Recognition Model Training Scripts

Path: `command-word/model-training/`

This directory is used for command-word dataset preparation, feature extraction, model training, evaluation, and visualization.

### `prepare_command_dataset.py`

A command-word dataset preparation script that mainly provides the `UltrasoundDataHelper` data-processing class.

Main functions:

- Reads command-word WAV datasets and metadata.
- Loads command-word samples by class.
- Applies offset cropping, framing, and sample segmentation to audio data.
- Extracts features such as Mel spectrograms and MFCCs.
- Automatically splits the dataset into training, validation, and test sets.
- Prepares input data and labels for TensorFlow/Keras model training.
- Maintains compatibility with part of the ST HSDatalog data-reading environment.

Runtime environment:

- Python 3.x
- `numpy`
- `pandas`
- `librosa`
- `scipy`
- `scikit-learn`
- `tensorflow`
- Optional: `stdatalog_core` or `st_hsdatalog`

### `train_command_classifier.ipynb`

A Jupyter Notebook for command-word classification model training.

Main functions:

- Calls the dataset preparation script to generate training data.
- Builds and trains the command-word classification model.
- Outputs training progress, test results, and evaluation metrics.
- Exports classification reports and model performance results.
- Generates ROC, t-SNE, and MFCC clustering visualizations to analyze class separability.

Before training, select the command-word training virtual environment in VS Code or Jupyter, for example `usc_train (Python 3.10.1)`, and configure the following notebook parameters:

- `dataset_path`: command-word dataset root directory, usually containing `metadata.csv` and `wav/`.
- `frame_length`: number of raw audio samples used for one inference window.
- `classes`: class list and order, which must match the class mapping used in the firmware.

See `command-word/model-training/ENVIRONMENT.md` for detailed steps.

Typical use cases:

- Command-word recognition model training.
- Model parameter tuning.
- Generation of experimental figures for papers or reports.

## 5. STM32 Embedded Firmware Project

Path: `stm32-firmware/Tx_LowPower_echo/`

This directory contains the embedded firmware project for the STM32U575 platform, including audio acquisition, feature extraction, AI inference, serial communication, and low-power related code.

Main contents:

- `Src/`: application source code, including audio acquisition, feature extraction, AI inference, UART debugging, and ThreadX tasks.
- `Inc/`: header files and model-related configuration.
- `Inc/power_test_cfg.h`: run-mode and power-test configuration.
- `STM32CubeIDE/`: STM32CubeIDE project files.
- `MDK-ARM/`: Keil MDK project files.
- `EWARM/`: IAR EWARM project files.
- `Middlewares/`: ST AI library, NanoEdge AI library, and other middleware.
- `docs/`: flowcharts, BOM files, and system documentation figures.
- `Tx_LowPower.ioc`: STM32CubeMX project configuration file.

Main functions:

- Acquires microphone audio data.
- Performs front-end feature extraction, such as windowing, FFT, Mel filtering, and MFCC extraction.
- Runs embedded AI models for command-word or acoustic-state recognition.
- Outputs debug information, acquisition data, or recognition results through UART.
- Supports the ThreadX low-power framework.

Run modes:

- For dataset acquisition, set `APP_RUN_MODE` in `Inc/power_test_cfg.h` to `APP_RUN_MODE_DATA_CAPTURE`.
- For model recognition, set `APP_RUN_MODE` to `APP_RUN_MODE_RECOGNITION`.

## 6. Bluetooth Receiver UART Host Program

Path: `bluetooth-receiver-host/`

This directory stores the UART-based PC monitoring software for synchronized appliance-state display and the related communication documentation. Consistent with Supporting Information Fig. S36, the MATLAB App does not directly receive Bluetooth wireless-link data and does not participate in speech recognition or control decisions. It only receives relay states, parsed results, or status strings from the Bluetooth receiver through the PC serial port/UART, and displays them on the host interface.

Main contents:

- `xiaobiao.mlapp`, `white-background.mlapp` or equivalent `.mlapp` files: MATLAB App Designer projects.
- Bluetooth communication documentation: serial communication notes between the Bluetooth receiver and the host.
- `data.txt`: example data output by the Bluetooth receiver through UART.
- `.png`, `.jpg`, `.gif`: images and animations used by the host interface.
- `app1.prj`: MATLAB App/project file.

Functional role:

- The Bluetooth receiver includes BLE receiving and MCU command parsing, and controls five relay/outlet channels.
- The MATLAB App is an optional PC monitoring interface that displays relay or appliance states.
- Speech recognition, command classification, and relay control decisions are completed by the embedded device and the Bluetooth receiver control module, not by the MATLAB App.

Runtime environment:

- MATLAB.
- If MATLAB serial communication functions are used, confirm the required MATLAB toolbox according to the actual `.mlapp` project.

## 7. System Development Workflow

This repository corresponds to the following complete development workflow. The root README only describes the system-level workflow. Detailed environment setup, official package downloads, required directory placement, and parameter configuration are provided in the module-level `ENVIRONMENT.md` files.

### 7.1 Firmware Preparation

1. Download the official ST STM32CubeU5 firmware package.
2. Place `stm32-firmware/Tx_LowPower_echo/` in the following official directory:

```text
<STM32CubeU5_ROOT>/Projects/NUCLEO-U575ZI-Q/Applications/ThreadX/Tx_LowPower_echo
```

3. Configure the run mode in `Inc/power_test_cfg.h`.
   - Use `APP_RUN_MODE_DATA_CAPTURE` for dataset acquisition.
   - Use `APP_RUN_MODE_RECOGNITION` for system deployment.
4. Compile and flash the firmware with STM32CubeIDE.

### 7.2 Data Acquisition

1. Run `host-acquisition/host_acquisition_gui.py`.
2. Establish a UART connection with the embedded device.
3. Acquire audio data from the S-TAU output.
4. Save raw data files such as `.dat`, `acquisition_info.json`, and `device_config.json`.

### 7.3 Wake-Word Model Development

1. Use `wake-word/nanoedge-converter/dat_to_nanoedge_csv.py` to convert wake-word raw `.dat` data into CSV files for NanoEdge AI Studio.
2. Create a binary-classification project in NanoEdge AI Studio.
3. Import `echo` positive samples and non-wake-word negative samples.
4. Run Benchmark and Validation.
5. Export the NanoEdge AI static library.
6. Integrate the exported `NanoEdgeAI.h` and `libneai.a` into `stm32-firmware/Tx_LowPower_echo/`.

### 7.4 Command-Word Model Development

1. Place the command-word raw `.dat` data in the official ST STDATALOG-PYSDK environment.
2. Use `command-word/preprocessing/convert_stdatalog_to_wav.ps1` to convert the data into WAV files.
3. Use `audit_wav_and_metadata.py` to check the WAV files and `metadata.csv`.
4. Place the training scripts in the official FP-AI-MONITOR2 `UltrasoundClassification` example directory.
5. Use `prepare_command_dataset.py` and `train_command_classifier.ipynb` to train the 11-class command-word model.
6. Export `model.tflite`.
7. Use STM32CubeMX / STM32Cube.AI to convert `model.tflite` into MCU C code.
8. Integrate the generated `usc_network*` files into `stm32-firmware/Tx_LowPower_echo/`.

### 7.5 System Deployment and Verification

1. Switch the firmware to `APP_RUN_MODE_RECOGNITION`.
2. Compile and flash the complete firmware.
3. Test whether the wake-word stage correctly triggers the command-word recognition stage.
4. Test the 10 valid command classes and the `background` class.
5. Check whether the firmware-side Bluetooth AT commands, Bluetooth receiver parsing result, relay state, and MATLAB host UART display are consistent.

## 8. Detailed Environment Configuration

Detailed environment setup, official download links, required file locations, and parameters are documented in each module:

| Module | Documentation | Main contents |
|---|---|---|
| Dataset acquisition host | `host-acquisition/ENVIRONMENT.md` | Python dependencies, serial connection, acquisition parameters |
| Wake-word module overview | `wake-word/ENVIRONMENT.md` | NanoEdge AI Studio workflow overview |
| Wake-word CSV conversion | `wake-word/nanoedge-converter/ENVIRONMENT.md` | CSV conversion parameters, NanoEdge AI Studio import, static library export |
| Command-word module overview | `command-word/ENVIRONMENT.md` | STDATALOG-PYSDK and FP-AI-MONITOR2 placement requirements |
| Command-word preprocessing | `command-word/preprocessing/ENVIRONMENT.md` | Required placement under STDATALOG-PYSDK, `.dat` to WAV conversion, path parameters |
| Command-word training | `command-word/model-training/ENVIRONMENT.md` | Required placement under FP-AI-MONITOR2 UltrasoundClassification, training dependencies, `model.tflite` export |
| STM32 firmware overview | `stm32-firmware/ENVIRONMENT.md` | STM32CubeU5 official directory and model integration overview |
| STM32 firmware project | `stm32-firmware/Tx_LowPower_echo/ENVIRONMENT.md` | Required placement under the STM32CubeU5 ThreadX directory, NanoEdge AI and STM32Cube.AI integration |
| Appliance-state monitoring host | `bluetooth-receiver-host/ENVIRONMENT.md` | MATLAB App, UART serial receiving, synchronized appliance-state display |

## 9. Official Software Download Links

The following official tools or packages are required to run the workflow:

- STM32CubeU5: `https://github.com/STMicroelectronics/STM32CubeU5`
- STDATALOG-PYSDK: `https://github.com/STMicroelectronics/stdatalog-pysdk`
- FP-AI-MONITOR2: `https://www.st.com/en/embedded-software/fp-ai-monitor2.html`
- STM32CubeMX: `https://www.st.com/en/development-tools/stm32cubemx.html`
- X-CUBE-AI / STM32Cube.AI: `https://www.st.com/en/embedded-software/x-cube-ai.html`
- NanoEdge AI Studio wiki: `https://wiki.st.com/stm32mcu/wiki/AI%3ANanoEdge_AI_Studio`
- NanoEdge AI Studio download entry: `https://stm32ai.st.com/nanoedge-ai-studio/`

## Workflow Notes

- Readers should start from this root README and then refer to each module's `ENVIRONMENT.md` for detailed environment configuration.
- Data input directories, output directories, Python interpreter paths, and official ST tool paths in scripts should be set according to the local installation.
- IDE build outputs, cache files, logs, and temporary files are not required inputs for running the workflow and can be regenerated locally.
- If ST AI libraries, NanoEdge AI libraries, or STM32Cube.AI-generated files are subject to ST software-package or license constraints, follow the official ST packages, tool-exported files, and the instructions in this repository.
- Large images, animations, presentations, DOCX files, or data files that are not included in the repository should be regenerated according to the documented directory structure and data acquisition workflow.
- Dataset directory format, dependency installation commands, and key parameters are specified in the corresponding module-level `ENVIRONMENT.md` files.
