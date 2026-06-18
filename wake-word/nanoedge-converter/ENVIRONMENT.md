# Environment Setup: Wake-Word NanoEdge AI Data Conversion

[Chinese version](ENVIRONMENT_CH.md)

This directory is used to convert `.dat` acquisition data saved by the host program into CSV files that can be imported into NanoEdge AI Studio.

Main programs:

```text
dat_to_nanoedge_csv.py
run_nanoedge_conversion.bat
```

## 1. Recommended Environment

- Operating system: Windows 10/11
- Python: 3.10 or later
- NanoEdge AI Studio: version v5.1.1 is recommended
- Firmware-side sampling rate: 16 kHz

ST official NanoEdge AI Studio wiki:

```text
https://wiki.st.com/stm32mcu/wiki/AI%3ANanoEdge_AI_Studio
```

NanoEdge AI Studio can be downloaded from the ST Edge AI / NanoEdge AI Studio page:

```text
https://stm32ai.st.com/nanoedge-ai-studio/
```

## 2. Python Dependencies

`dat_to_nanoedge_csv.py` only uses the Python standard library:

- `argparse`
- `csv`
- `json`
- `struct`
- `pathlib`

Generally, no additional third-party Python packages are required.

Check command:

```powershell
python dat_to_nanoedge_csv.py --help
```

This script can be run directly in this directory and does not need to be placed inside an official ST project directory.

## 3. NanoEdge AI Studio Preparation

According to the official ST documentation, NanoEdge AI Studio searches combinations of preprocessing, feature extraction, and machine-learning libraries from sensor data, and exports a static library that can be called on an MCU.

Before use, prepare the following:

- Install NanoEdge AI Studio.
- Log in or configure an available license.
- Confirm the target MCU and resource constraints.
- Prepare CSV data for each class.

The wake-word task in this project is a binary classification task:

- Positive samples: `echo`
- Negative samples: `other` / `unknown` / `background` and other non-wake-word sounds

## 4. Data Conversion Parameters

Typical command:

```powershell
python dat_to_nanoedge_csv.py <acquisition_data_root> -o <output_dir> -s imp23absu_mic -sl 8192 -si 8192 --odr 16000 --csv-shape row
```

Parameter description:

- `<acquisition_data_root>`: acquisition data directory saved by the host program.
- `-o`: CSV output directory.
- `-s`: sensor name. The commonly used value is `imp23absu_mic`; this is the microphone name used by the STEVAL-STWINBX1 development board, while this work uses S-TAU.
- `-sl`: window length.
- `-si`: window step.
- `--odr`: sampling rate, currently `16000`.
- `--csv-shape`: CSV layout, currently commonly set to `row`.

If the batch script is used:

```text
run_nanoedge_conversion.bat
```

Set the following variables before running it:

```bat
set "ROOT=<wake-word_raw_acquisition_data_root>"
set "OUT_ALL=<NanoEdge_CSV_output_directory>"
set "PYTHON_EXE=python"
set "CLI=%~dp0dat_to_nanoedge_csv.py"
set "SENSOR=imp23absu_mic"
set "SL=8192"
set "SI=8192"
set "ODR=16000"
set "CSV_SHAPE=row"
```

Parameter meanings:

- `ROOT`: root directory of acquisition data saved by the host program. It should contain multiple acquisition folders.
- `OUT_ALL`: output directory for converted CSV files.
- `PYTHON_EXE`: Python interpreter command or absolute path.
- `CLI`: conversion script path. If the batch file still contains an old script name, change it to `dat_to_nanoedge_csv.py`.
- `SENSOR`: sensor name in the JSON configuration, currently `imp23absu_mic`.
- `SL`: length of each NanoEdge sample window.
- `SI`: window step. To avoid overlap, set it to the same value as `SL`.
- `ODR`: sampling rate, 16 kHz in the current experiments.
- `CSV_SHAPE`: NanoEdge AI Studio import layout. The current recommendation is `row`.

For local use, the most commonly modified variables are `ROOT`, `OUT_ALL`, `SL`, and `SI`. `SL` must be consistent with `NEAI_INPUT_SIGNAL_LENGTH` in the final library exported by NanoEdge AI Studio.

## 5. Relationship with NanoEdge AI Studio

The CSV window length must be consistent with the NanoEdge AI Studio training setting and the final exported library.

After exporting the library, check:

```text
NanoEdgeAI.h
```

Key macros include:

```c
#define NEAI_INPUT_SIGNAL_LENGTH ...
#define NEAI_INPUT_AXIS_NUMBER ...
#define NEAI_NUMBER_OF_CLASSES ...
```

In the current firmware, `Inc/audio_config.h` uses `NEAI_INPUT_SIGNAL_LENGTH` as the wake-word input window length. Therefore, when retraining the model, keep the following three items consistent:

- `-sl` used during CSV conversion
- Input signal length of the NanoEdge AI Studio project
- Exported `NanoEdgeAI.h` used in the firmware

## 6. NanoEdge AI Studio Output Files

According to the official ST wiki, the library package generated after compilation/deployment usually contains:

- `libneai.a`: NanoEdge AI static library
- `NanoEdgeAI.h`: API and model-configuration header file
- `knowledge.h`: knowledge file, whether it appears depends on the project type
- `metadata.json`: library metadata
- emulator: used to validate the library performance on a PC

This project mainly uses the following files for firmware integration:

```text
NanoEdgeAI.h
libneai.a
```

Copy targets:

```text
../../stm32-firmware/Tx_LowPower_echo/Inc/NanoEdgeAI.h
../../stm32-firmware/Tx_LowPower_echo/Middlewares/ST/STM32_AI_Library/Lib/libneai.a
```

## 7. Recommended Validation

1. Use data not involved in training to validate classification performance in NanoEdge AI Studio or the emulator.
2. Check `NEAI_INPUT_SIGNAL_LENGTH` and the firmware window length.
3. After importing the library into the firmware, confirm that `neai_classification_init()` returns `NEAI_OK`.
4. Test wake-word and negative samples on the actual device and observe whether stable classification results are produced.

## 8. Troubleshooting

- If the NanoEdge AI Studio Benchmark result is poor, first check whether the data is separated by class, whether the window length is reasonable, and whether the sampling rate is consistent.
- Sliding-window processing can be used to improve recognition accuracy.
- For false triggers on the MCU side, continuous multi-frame confirmation can be added in the firmware. The current firmware already implements similar confirmation through parameters such as `APP_NEAI_WAKE_HITS`.

## 9. Checklist

1. Install NanoEdge AI Studio and confirm that the license is available.
2. Use this script to convert wake-word positive samples and negative samples into CSV files separately.
3. Create a Classification project in NanoEdge AI Studio.
4. Import the CSV file corresponding to each class.
5. Set the target MCU and resource constraints.
6. Run Benchmark.
7. Use data not involved in Benchmark for Validation or emulator testing.
8. Export the static library.
9. Import `NanoEdgeAI.h` and `libneai.a` into the STM32 firmware project.
10. Rebuild the firmware and validate real-time wake-word screening on the MCU.
