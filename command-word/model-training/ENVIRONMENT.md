# Environment Setup: Command-Word Model Training

This directory is used for data preparation, model training, evaluation, and TensorFlow Lite export for command-word recognition.

Main files:

```text
prepare_command_dataset.py
train_command_classifier.ipynb
```

## 1. Recommended Environment

- Operating system: Windows 10/11
- Python: the training environment used in this study was Python 3.10.1; Python 3.10.x is recommended for this workflow
- Visual Studio Code (VS Code): used to open and run `train_command_classifier.ipynb`
- VS Code extensions: Python extension, with VS Code support for running `.ipynb` files
- TensorFlow: use a stable TensorFlow version compatible with the selected Python version
- STM32CubeMX / STM32Cube.AI: used later to import `model.tflite` and generate MCU C code

The command-word training scripts in this directory were modified from the `UltrasoundClassification` example in STMicroelectronics FP-AI-MONITOR2. To run the training workflow, copy the two files in this directory into the corresponding official FP-AI-MONITOR2 example directory.

Recommended location:

```text
<FP-AI-MONITOR2_ROOT>\Utilities\AI_Resources\TrainingScripts\UltrasoundClassification\
```

Files to copy:

```text
prepare_command_dataset.py
train_command_classifier.ipynb
```

ST official FP-AI-MONITOR2 download page:

```text
https://www.st.com/en/embedded-software/fp-ai-monitor2.html
```

The official package contains example resources for sensor monitoring and AI workflows. The command-word training script in this work was adapted from:

```text
Utilities\AI_Resources\TrainingScripts\UltrasoundClassification
```

## 2. Create the `usc_train` Virtual Environment in PowerShell

The command-word model training is based on the official FP-AI-MONITOR2 `UltrasoundClassification` example. Download and extract FP-AI-MONITOR2 first, and then create an independent virtual environment named `usc_train` under the root directory of the official package.

The experimental path used in this study was:

```text
D:\workspace\project\keil\thesis_speech_recognition
```

This directory corresponds to the FP-AI-MONITOR2 root directory and should contain the following official example directory:

```text
Utilities\AI_Resources\TrainingScripts\UltrasoundClassification
```

When running the workflow, replace the path in the commands below with the local FP-AI-MONITOR2 extraction directory.

### 2.1 Enter the FP-AI-MONITOR2 Root Directory

Open Windows PowerShell and run:

```powershell
D:
cd D:\workspace\project\keil\thesis_speech_recognition
```

If FP-AI-MONITOR2 is extracted elsewhere, use:

```powershell
cd <FP-AI-MONITOR2_ROOT>
```

### 2.2 Create and Activate the `usc_train` Environment

Python 3.10 is recommended. The interpreter used during training in this study was shown as `usc_train (Python 3.10.1)`.

```powershell
py -3.10 --version
py -3.10 -m venv usc_train
.\usc_train\Scripts\Activate.ps1
```

After successful activation, the PowerShell prompt should start with:

```text
(usc_train)
```

If PowerShell reports that script execution is disabled, temporarily allow script execution for the current PowerShell session and activate the environment again:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\usc_train\Scripts\Activate.ps1
```

If `py -3.10` is not available, but `python --version` already shows Python 3.10.x, the following commands can also be used:

```powershell
python -m venv usc_train
.\usc_train\Scripts\Activate.ps1
```

### 2.3 Install Training Dependencies

After activating `usc_train`, update the basic packaging tools:

```powershell
python -m pip install --upgrade pip setuptools wheel
```

Install the main dependencies required by the training scripts.

```powershell
python -m pip install `
    numpy==1.26.4 `
    pandas==2.2.3 `
    scipy==1.11.4 `
    librosa==0.11.0 `
    scikit-learn==1.3.2 `
    tensorflow-intel==2.15.0 `
    keras==2.15.0 `
    matplotlib==3.10.7 `
    soundfile==0.13.1 `
    ipykernel==7.1.0 `
    stdatalog-core==1.2.1
```

After installation, check whether there are dependency conflicts:

```powershell
python -m pip check
```

Notes:

- `tensorflow-intel==2.15.0` provides the `tensorflow` module used by the code for model training and `model.tflite` export.
- `librosa`, `scipy`, and `soundfile` are used for WAV audio reading and feature processing.
- `numpy`, `pandas`, and `scikit-learn` are used for data processing, label handling, data splitting, and evaluation.
- `matplotlib` is used for plotting training curves, confusion matrices, and related results.
- `stdatalog-core` is used for compatibility with ST HSDatalog-related data reading.
- `ipykernel` allows VS Code to detect and select `usc_train` as the `.ipynb` execution environment.

After installation, run the dependency check once:

```powershell
python -c "import numpy, pandas, librosa, scipy, sklearn, soundfile; print('basic command model training packages OK')"
python -c "import importlib.metadata as md; print('tensorflow-intel', md.version('tensorflow-intel'))"
```

This step is only used to confirm that the dependencies have been installed correctly. It does not need to be repeated before every training run. `tensorflow-intel` is the Windows TensorFlow distribution package, while the code still uses `import tensorflow as tf`. The first TensorFlow import may be slow; therefore, after opening `train_command_classifier.ipynb` in VS Code, run the first few Notebook cells to confirm that TensorFlow can be loaded correctly.

### 2.4 Register the Python Kernel for VS Code

To select `usc_train` directly when running `train_command_classifier.ipynb` in VS Code, register a kernel name:

```powershell
python -m ipykernel install --user --name usc_train --display-name "usc_train (Python 3.10.1)"
```

After registration, when opening a `.ipynb` file in VS Code, the following kernel can be selected from the upper-right kernel selection menu:

```text
usc_train (Python 3.10.1)
```

### 2.5 Place the Training Scripts

Copy the following two files from this directory into the official ST example directory:

```text
<FP-AI-MONITOR2_ROOT>\Utilities\AI_Resources\TrainingScripts\UltrasoundClassification\
```

Files to copy:

```text
prepare_command_dataset.py
train_command_classifier.ipynb
```

The recommended directory structure is:

```text
<FP-AI-MONITOR2_ROOT>\
|-- usc_train\
`-- Utilities\
    `-- AI_Resources\
        `-- TrainingScripts\
            `-- UltrasoundClassification\
                |-- prepare_command_dataset.py
                `-- train_command_classifier.ipynb
```

In the original ST FP-AI-MONITOR2 example, the original Notebook file is usually named:

```text
UltrasoundClassification.ipynb
```

The reorganized file in this repository is:

```text
train_command_classifier.ipynb
```

The training instructions in this document refer to `train_command_classifier.ipynb`.

## 3. Key Parameters to Set in the Training Notebook

Before running `train_command_classifier.ipynb` in VS Code, set the dataset path, input frame length, and class list according to the current local environment. The main parameters modified in this training workflow are listed below.

### 3.1 Dataset Path

`dataset_path` points to the root directory of the command-word dataset. This directory should contain `metadata.csv` and the `wav/` directory. A trailing slash is recommended to avoid path-concatenation errors.

```python
dataset_path = 'E:/SPEECH_DATA/20251029-command/'
```

Generic form:

```python
dataset_path = '<COMMAND_DATASET_ROOT>/'
```

For example, if the command-word preprocessing step generates:

```text
E:\SPEECH_DATA\TENG\touming-dataset\command-20260315-merge\metadata.csv
E:\SPEECH_DATA\TENG\touming-dataset\command-20260315-merge\wav\
```

then the training path can be set as:

```python
dataset_path = 'E:/SPEECH_DATA/TENG/touming-dataset/command-20260315-merge/'
```

### 3.2 Input Frame Length

`frame_len` is the number of raw audio samples used for one inference. The training example in this study used:

```python
frame_len = 512 * 60 + 1
```

When running the workflow, `frame_len` should be consistent with the WAV length check in preprocessing, the training feature-extraction parameters, and the command-word frontend parameters in the firmware.

### 3.3 Class List

The order of `classes` determines the model output order. It must be consistent with the labels in `metadata.csv`, the training data directory names, and the class mapping in the firmware file `Src/app_threadx.c`.

A four-class debugging example is:

```python
classes = ['lightoff', 'lighton', 'tvoff', 'tvon']
```

The final command-word recognition model in this study is an 11-class task. The recommended class order is:

```python
classes = [
    'microwaveoff', 'microwaveon',
    'fanoff', 'fanon',
    'humidifieroff', 'humidifieron',
    'lightoff', 'lighton',
    'Televisionoff', 'Televisionon',
    'background'
]
```

The `background` class is used as the last class by default. In the firmware, the background class is identified by `AI_USC_NETWORK_OUT_1_SIZE - 1U`.

## 4. Other Training Parameters and Paths to Set Locally

Before running the Notebook, set the following items according to the local environment:

- Dataset root directory, for example `dataset` or `dataset_path`.
- Directory containing `metadata.csv`.
- WAV data directory.
- Result output directory `resultDir`.
- Class list `classes`.
- Sampling rate `sample_rate`; the current experiment used `16000`.
- Input length `frame_len`; the current command-word model uses approximately 2 s samples, corresponding to the slice length in the training script.
- MFCC parameters, including `n_fft`, `hop_length`, `n_mels`, and `n_mfccs`.
- Training/validation/test split ratios and random seed.

The command-word frontend parameters in the current firmware are located at:

```text
../../../../stm32-firmware/Tx_LowPower_echo/Inc/audio_config.h
../../../../stm32-firmware/Tx_LowPower_echo/Src/usc_preproc.c
```

The training parameters must be consistent with the firmware frontend. Otherwise, `model.tflite` may perform normally on the PC but show degraded recognition accuracy or class-order mismatch after being deployed to the MCU.

## 5. Run Training in VS Code

### 5.1 Dependency Check (Run After First Installation or When the Environment Changes)

This check is mainly used to confirm that the training dependencies have been installed successfully. It is not required before every training run. Run it in the following cases:

- After creating and installing the `usc_train` environment for the first time.
- After switching to another computer or recreating the virtual environment.
- When VS Code reports missing modules, incorrect interpreter selection, or dependency conflicts while running the Notebook.

In PowerShell, confirm that `usc_train` is activated:

```powershell
cd <FP-AI-MONITOR2_ROOT>
.\usc_train\Scripts\Activate.ps1
```

Then run:

```powershell
python -c "import numpy, pandas, librosa, scipy, sklearn, soundfile; print('basic command model training packages OK')"
python -c "import importlib.metadata as md; print('tensorflow-intel', md.version('tensorflow-intel'))"
```

Expected output:

```text
basic command model training packages OK
tensorflow-intel 2.15.0
```

This indicates that the main training dependencies have been installed. The first TensorFlow import may be slow, so run the import cells in the Notebook in VS Code for the final check.

For repeated training runs, if the `usc_train` environment has not changed, skip this section and continue with the VS Code training steps below.

### 5.2 Open the Official Training Directory in VS Code

Open VS Code, select `File` -> `Open Folder...`, and open:

```text
<FP-AI-MONITOR2_ROOT>\Utilities\AI_Resources\TrainingScripts\UltrasoundClassification
```

Alternatively, start VS Code from PowerShell under the FP-AI-MONITOR2 root directory:

```powershell
cd <FP-AI-MONITOR2_ROOT>\Utilities\AI_Resources\TrainingScripts\UltrasoundClassification
code .
```

In the VS Code file explorer, open:

```text
train_command_classifier.ipynb
```

### 5.3 Select the `usc_train` Interpreter

After opening `train_command_classifier.ipynb`, select the Notebook kernel in the upper-right corner of VS Code.

Recommended kernel:

```text
usc_train (Python 3.10.1)
```

If this name does not appear in the list, select the Python interpreter path:

```text
<FP-AI-MONITOR2_ROOT>\usc_train\Scripts\python.exe
```

If VS Code still cannot detect the environment, run the kernel-registration command again in PowerShell:

```powershell
cd <FP-AI-MONITOR2_ROOT>
.\usc_train\Scripts\Activate.ps1
python -m ipykernel install --user --name usc_train --display-name "usc_train (Python 3.10.1)"
```

Then restart VS Code or reopen `train_command_classifier.ipynb`.

### 5.4 Train the Model in VS Code

Before running the Notebook, check that the following items have been updated for the local path and dataset:

- `dataset_path`: root directory of the command-word dataset, containing `metadata.csv` and `wav/`.
- `frame_len`: input audio sample count, consistent with preprocessing and firmware-side parameters.
- `classes`: class order, consistent with `metadata.csv`, data directories, and firmware-side class mapping.
- Output directory: used to save training results, confusion matrices, `.npz` files, and `model.tflite`.

After confirming these settings, run the Notebook cells sequentially in VS Code. It is recommended to run the dependency-import and data-check cells first, confirm that the dataset can be loaded, and then run the training cells.

After training, save or check the following outputs:

- `model.tflite`
- Training/validation accuracy and loss curves
- Confusion matrix
- `train_valid_data.npz` or an equivalent training cache
- `post_validation_data.npz`

## 6. Dataset Requirements

The command-word model is an 11-class task:

- `microwaveoff`
- `microwaveon`
- `fanoff`
- `fanon`
- `humidifieroff`
- `humidifieron`
- `lightoff`
- `lighton`
- `Televisionoff`
- `Televisionon`
- `background`

Notes:

- The class order must be consistent with the class-name array and Bluetooth AT-command mapping in the firmware file `Src/app_threadx.c`.
- `background` is used as the last class by default. In the firmware, the background class is identified by `AI_USC_NETWORK_OUT_1_SIZE - 1U`.

## 7. Model Export

After training, export:

```text
model.tflite
```

This file is imported into STM32CubeMX / STM32Cube.AI to generate MCU C code.

## 8. STM32Cube.AI Pre-Check

Before importing `model.tflite`, check the following:

- The model input shape is consistent with the firmware frontend features.
- The command-word input feature in the current firmware is `60 x 32 x 1`.
- The output class count is 11.
- The MFCC parameters used during training are consistent with `Inc/audio_config.h` and `Src/usc_preproc.c` in the firmware.

Firmware-related parameters:

```text
../../stm32-firmware/Tx_LowPower_echo/Inc/audio_config.h
../../stm32-firmware/Tx_LowPower_echo/Src/usc_preproc.c
```

Official STM32CubeMX and X-CUBE-AI download pages:

```text
https://www.st.com/en/development-tools/stm32cubemx.html
https://www.st.com/en/embedded-software/x-cube-ai.html
```

## 9. Configuration Checklist

1. Download and extract the official ST FP-AI-MONITOR2 package.
2. Copy `prepare_command_dataset.py` and `train_command_classifier.ipynb` into the official `Utilities\AI_Resources\TrainingScripts\UltrasoundClassification` directory.
3. In PowerShell, enter the FP-AI-MONITOR2 root directory, create and activate the `usc_train` virtual environment.
4. Install the training dependencies in `usc_train` and register the VS Code-selectable kernel.
5. Open the official `UltrasoundClassification` directory in VS Code.
6. Open `train_command_classifier.ipynb` and select `usc_train (Python 3.10.1)` or `<FP-AI-MONITOR2_ROOT>\usc_train\Scripts\python.exe` in the upper-right corner.
7. Set the dataset path, metadata path, and result output path in the Notebook to the local paths.
8. Confirm that the trailing slash of `dataset_path`, `frame_len`, and `classes` have been set according to the current dataset.
9. Confirm that `metadata.csv`, the WAV directory, and class names are consistent.
10. Run the Notebook cells sequentially in VS Code to complete training.
11. Export `model.tflite`.
12. Import `model.tflite` in STM32CubeMX / STM32Cube.AI and set the network name to `usc_network`.
13. Generate the code and replace the `usc_network*` files in the firmware.
14. Rebuild the STM32 firmware and test it on the board.

## 10. Troubleshooting

- If TensorFlow installation fails, check whether the Python version is supported by the selected TensorFlow version.
- If the Notebook cannot find the data, check the dataset root directory and the `metadata.csv` path.
- If VS Code uses the wrong interpreter, check whether the Notebook kernel in the upper-right corner is `usc_train (Python 3.10.1)` or the current training virtual environment.
- If `prepare_command_dataset` cannot be found, check whether the Notebook is located in the same directory as `prepare_command_dataset.py`, or check whether the Python working directory is correct.
- If the MCU-side classes are mismatched, check the training label order, model output order, and class mapping in `app_threadx.c`.
- If STM32Cube.AI Analyze reports that RAM or Flash usage is too high, simplify the model, quantize the model, or adjust the network structure.
