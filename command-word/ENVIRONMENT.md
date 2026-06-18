# Environment Setup: Command-Word Module

This directory contains two parts of the command-word workflow: data preprocessing and model training.

Subdirectories:

```text
preprocessing/
model-training/
```

Detailed setup instructions:

- Data preprocessing: `preprocessing/ENVIRONMENT.md`
- Model training: `model-training/ENVIRONMENT.md`

## Recommended Tools

- PowerShell 7
- Python 3.10 or 3.11
- Visual Studio Code (VS Code), used to run the training Notebook
- TensorFlow / Keras
- STM32CubeMX + STM32Cube.AI
- STM32CubeIDE

Official sources:

```text
STDATALOG-PYSDK:
https://github.com/STMicroelectronics/stdatalog-pysdk

FP-AI-MONITOR2:
https://www.st.com/en/embedded-software/fp-ai-monitor2.html

STM32CubeMX:
https://www.st.com/en/development-tools/stm32cubemx.html

X-CUBE-AI:
https://www.st.com/en/embedded-software/x-cube-ai.html
```

## Programs That Must Be Placed in Official ST Directories

The programs in the two subdirectories should be run inside the corresponding official ST packages.

### 1. `preprocessing`

Copy:

```text
preprocessing/convert_stdatalog_to_wav.ps1
```

to:

```text
<STDATALOG-PYSDK_ROOT>\stdatalog-pysdk\
```

The final path should be:

```text
<STDATALOG-PYSDK_ROOT>\stdatalog-pysdk\convert_stdatalog_to_wav.ps1
```

### 2. `model-training`

Copy:

```text
model-training/prepare_command_dataset.py
model-training/train_command_classifier.ipynb
```

to the official FP-AI-MONITOR2 example directory:

```text
<FP-AI-MONITOR2_ROOT>\Utilities\AI_Resources\TrainingScripts\UltrasoundClassification\
```

## Development Workflow Overview

1. Use `host-acquisition/` to collect raw command-word data.
2. Use `preprocessing/convert_stdatalog_to_wav.ps1` to convert acquisition data into WAV files.
3. Use `preprocessing/audit_wav_and_metadata.py` to check the WAV files and `metadata.csv`.
4. Use `model-training/prepare_command_dataset.py` to prepare the training data.
5. Use `model-training/train_command_classifier.ipynb` in VS Code to train the command-word model.
6. Export `model.tflite`.
7. Import `model.tflite` into STM32CubeMX / STM32Cube.AI and generate the `usc_network*` C files.
8. Integrate the generated code into `../stm32-firmware/Tx_LowPower_echo/`.

## Key Consistency Requirements With the Firmware

The following items must remain consistent between the training workflow and the firmware:

- Command-word class order
- Whether the background class is the last class
- MFCC / Mel feature parameters
- Model input shape
- AT-command mapping in the firmware

Firmware files that should be checked carefully:

```text
../stm32-firmware/Tx_LowPower_echo/Inc/audio_config.h
../stm32-firmware/Tx_LowPower_echo/Src/usc_preproc.c
../stm32-firmware/Tx_LowPower_echo/Src/app_threadx.c
```
