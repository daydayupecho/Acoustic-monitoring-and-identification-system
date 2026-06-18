# 环境配置说明：命令词模型训练

本目录用于命令词识别模型的数据准备、训练、评估和 TensorFlow Lite 导出。

主要文件：

```text
prepare_command_dataset.py
train_command_classifier.ipynb
```

## 1. 推荐环境

- 操作系统：Windows 10/11
- Python：本文训练环境为 Python 3.10.1；建议复现时使用 Python 3.10.x
- Visual Studio Code（VS Code）：用于打开和运行 `train_command_classifier.ipynb`
- VS Code 扩展：Python 扩展，以及 VS Code 对 `.ipynb` 文件的运行支持
- TensorFlow：建议使用与本机 Python 版本兼容的稳定版本
- STM32CubeMX / STM32Cube.AI：用于后续导入 `model.tflite` 并生成 MCU C 代码

本目录中的命令词训练脚本是基于 ST 官方 FP-AI-MONITOR2 中的 UltrasoundClassification 示例修改而来。复现训练流程时，建议将本目录两个文件复制到官方 FP-AI-MONITOR2 的对应示例目录下运行。

推荐放置位置：

```text
<FP-AI-MONITOR2_ROOT>\Utilities\AI_Resources\TrainingScripts\UltrasoundClassification\
```

需要复制的文件：

```text
prepare_command_dataset.py
train_command_classifier.ipynb
```

ST 官方 FP-AI-MONITOR2 下载页：

```text
https://www.st.com/en/embedded-software/fp-ai-monitor2.html
```

该官方包包含用于传感器监测和 AI 工作流的示例资源。本文命令词训练脚本参考其中：

```text
Utilities\AI_Resources\TrainingScripts\UltrasoundClassification
```

## 2. PowerShell 创建训练虚拟环境 `usc_train`

命令词模型训练在 ST 官方 FP-AI-MONITOR2 的 `UltrasoundClassification` 示例基础上完成。复现时，建议先下载并解压 FP-AI-MONITOR2，然后在该官方包根目录下创建独立虚拟环境 `usc_train`。

以下以实验使用的目录为例：

```text
D:\workspace\project\keil\thesis_speech_recognition
```

该目录应对应 FP-AI-MONITOR2 根目录，并包含以下官方示例目录：

```text
Utilities\AI_Resources\TrainingScripts\UltrasoundClassification
```

读者复现时，请将下面命令中的路径替换为本机 FP-AI-MONITOR2 解压目录。

### 2.1 进入 FP-AI-MONITOR2 根目录

打开 Windows PowerShell，执行：

```powershell
D:
cd D:\workspace\project\keil\thesis_speech_recognition
```

若 FP-AI-MONITOR2 解压在其他位置，则使用：

```powershell
cd <FP-AI-MONITOR2_ROOT>
```

### 2.2 创建并激活 `usc_train` 环境

建议使用 Python 3.10 创建环境。本文训练时使用的解释器显示为 `usc_train (Python 3.10.1)`。

```powershell
py -3.10 --version
py -3.10 -m venv usc_train
.\usc_train\Scripts\Activate.ps1
```

激活成功后，PowerShell 提示符前会出现：

```text
(usc_train)
```

如果 PowerShell 提示脚本执行被禁止，可仅对当前 PowerShell 窗口临时放开执行策略，然后再次激活环境：

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\usc_train\Scripts\Activate.ps1
```

如果本机无法使用 `py -3.10`，但 `python --version` 已显示 Python 3.10.x，也可以使用：

```powershell
python -m venv usc_train
.\usc_train\Scripts\Activate.ps1
```

### 2.3 安装训练依赖

在已激活的 `usc_train` 环境中更新基础工具：

```powershell
python -m pip install --upgrade pip setuptools wheel
```

安装训练脚本需要的主要依赖。
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

安装完成后，建议检查包依赖是否存在冲突：

```powershell
python -m pip check
```

其中：

- `tensorflow-intel==2.15.0` 提供代码中使用的 `tensorflow` 模块，用于模型训练和 `model.tflite` 导出。
- `librosa`、`scipy`、`soundfile` 用于 WAV 音频读取和特征处理。
- `numpy`、`pandas`、`scikit-learn` 用于数据整理、标签处理、划分和评价。
- `matplotlib` 用于训练曲线、混淆矩阵等结果可视化。
- `stdatalog-core` 用于兼容 ST HSDatalog 相关数据读取。
- `ipykernel` 用于让 VS Code 识别并选择 `usc_train` 作为 `.ipynb` 运行环境。

完成安装后，建议执行一次依赖检查：

```powershell
python -c "import numpy, pandas, librosa, scipy, sklearn, soundfile; print('basic command model training packages OK')"
python -c "import importlib.metadata as md; print('tensorflow-intel', md.version('tensorflow-intel'))"
```

说明：这一步用于确认依赖已经正确安装，不需要每次训练前重复执行。`tensorflow-intel` 是 Windows 环境下的 TensorFlow 发行包，代码中仍然使用 `import tensorflow as tf`。TensorFlow 首次导入可能较慢，因此建议在 VS Code 打开 `train_command_classifier.ipynb` 后，通过运行 Notebook 前几个单元确认 TensorFlow 是否可以正常加载。

### 2.4 注册 VS Code 可选择的 Python 内核

为了在 VS Code 中直接选择 `usc_train` 环境运行 `train_command_classifier.ipynb`，建议注册内核名称：

```powershell
python -m ipykernel install --user --name usc_train --display-name "usc_train (Python 3.10.1)"
```

注册后，VS Code 打开 `.ipynb` 文件时，可在右上角内核选择菜单中选择：

```text
usc_train (Python 3.10.1)
```

### 2.5 放置训练脚本

将本目录中的以下两个文件复制到 ST 官方示例目录：

```text
<FP-AI-MONITOR2_ROOT>\Utilities\AI_Resources\TrainingScripts\UltrasoundClassification\
```

需要复制的文件：

```text
prepare_command_dataset.py
train_command_classifier.ipynb
```

复制后，推荐目录结构为：

```text
<FP-AI-MONITOR2_ROOT>\
├── usc_train\
└── Utilities\
    └── AI_Resources\
        └── TrainingScripts\
            └── UltrasoundClassification\
                ├── prepare_command_dataset.py
                └── train_command_classifier.ipynb
```

如果使用 ST 官方 FP-AI-MONITOR2 原始示例，原始 Notebook 文件名通常为：

```text
UltrasoundClassification.ipynb
```

本仓库中对应整理后的文件为：

```text
train_command_classifier.ipynb
```

本文训练和复现说明均以 `train_command_classifier.ipynb` 为准。

## 3. 训练 Notebook 需要设置的关键参数

在 VS Code 中运行 `train_command_classifier.ipynb` 前，需要根据当前复现环境设置数据路径、输入帧长和类别列表。本文训练流程主要修改以下参数。

### 3.1 数据集路径

`dataset_path` 指向命令词数据集根目录，该目录下应包含 `metadata.csv` 和 `wav/` 目录。路径末尾建议保留斜杠，避免后续路径拼接出错。

```python
dataset_path = 'E:/SPEECH_DATA/20251029-command/'
```

通用写法：

```python
dataset_path = '<COMMAND_DATASET_ROOT>/'
```

例如，若命令词预处理步骤生成：

```text
E:\SPEECH_DATA\TENG\touming-dataset\command-20260315-merge\metadata.csv
E:\SPEECH_DATA\TENG\touming-dataset\command-20260315-merge\wav\
```

则训练时可设置：

```python
dataset_path = 'E:/SPEECH_DATA/TENG/touming-dataset/command-20260315-merge/'
```

### 3.2 输入帧长

`frame_len` 表示一次推理使用的原始音频采样点数。本文训练示例为：

```python
frame_len = 512 * 60 + 1
```

实际复现时，`frame_len` 需要与前处理阶段的 WAV 长度检查、训练特征提取参数以及固件端命令词前端保持一致。

### 3.3 类别列表

`classes` 的顺序决定模型输出类别顺序，必须与 `metadata.csv` 中的标签、训练数据目录名和固件端 `Src/app_threadx.c` 中的类别映射保持一致。

四分类调试示例为：

```python
classes = ['lightoff', 'lighton', 'tvoff', 'tvon']
```

本文最终命令词识别模型为 11 分类任务，当前推荐类别顺序为：

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

其中 `background` 默认作为最后一类，固件中通过 `AI_USC_NETWORK_OUT_1_SIZE - 1U` 判断背景类。

## 4. 其他需要根据本机环境设置的训练参数和路径

运行 Notebook 前，需要根据当前复现环境设置以下内容：

- 数据集根目录，例如 `dataset` 或 `dataset_path`。
- `metadata.csv` 所在目录。
- WAV 数据目录。
- 结果输出目录 `resultDir`。
- 类别列表 `classes`。
- 采样率 `sample_rate`，当前实验为 `16000`。
- 输入长度 `frame_len`，当前命令词模型约为 2 s 样本，对应训练脚本中的切片长度。
- MFCC 参数，包括 `n_fft`、`hop_length`、`n_mels`、`n_mfccs`。
- 训练/验证/测试划分比例和随机种子。

当前固件中的命令词前端参数位于：

```text
../../../../stm32-firmware/Tx_LowPower_echo/Inc/audio_config.h
../../../../stm32-firmware/Tx_LowPower_echo/Src/usc_preproc.c
```

训练参数必须与固件前端保持一致，否则 `model.tflite` 在 PC 上表现正常，但导入 MCU 后会出现识别率下降或类别错位。

## 5. 使用 VS Code 运行训练

### 5.1 依赖检查（首次安装后或环境异常时执行）

该检查主要用于确认训练依赖是否安装成功，不是每次训练前的必需步骤。通常在以下情况执行一次即可：

- 首次创建并安装 `usc_train` 环境后。
- 更换电脑或重新创建虚拟环境后。
- VS Code 运行 Notebook 时出现缺少模块、解释器选择错误或依赖冲突时。

在 PowerShell 中确认已激活 `usc_train` 环境：

```powershell
cd <FP-AI-MONITOR2_ROOT>
.\usc_train\Scripts\Activate.ps1
```

然后运行：

```powershell
python -c "import numpy, pandas, librosa, scipy, sklearn, soundfile; print('basic command model training packages OK')"
python -c "import importlib.metadata as md; print('tensorflow-intel', md.version('tensorflow-intel'))"
```

如果输出类似：

```text
basic command model training packages OK
tensorflow-intel 2.15.0
```

说明主要训练依赖已安装完成。TensorFlow 首次导入可能较慢，建议在 VS Code 中运行 Notebook 的导入单元进行最终确认。

日常重复训练时，如果 `usc_train` 环境没有变动，可直接执行后续 VS Code 训练步骤，不需要重复运行本节检查命令。

### 5.2 用 VS Code 打开官方训练目录

打开 VS Code，选择 `File` -> `Open Folder...`，打开以下目录：

```text
<FP-AI-MONITOR2_ROOT>\Utilities\AI_Resources\TrainingScripts\UltrasoundClassification
```

也可以在 PowerShell 中从 FP-AI-MONITOR2 根目录启动 VS Code：

```powershell
cd <FP-AI-MONITOR2_ROOT>\Utilities\AI_Resources\TrainingScripts\UltrasoundClassification
code .
```

在 VS Code 文件列表中打开：

```text
train_command_classifier.ipynb
```

### 5.3 选择 `usc_train` 解释器

打开 `train_command_classifier.ipynb` 后，在 VS Code 右上角选择 Notebook 内核。

推荐选择：

```text
usc_train (Python 3.10.1)
```

如果列表中没有显示该名称，可选择 Python 解释器路径：

```text
<FP-AI-MONITOR2_ROOT>\usc_train\Scripts\python.exe
```

如果 VS Code 仍不能识别该环境，可回到 PowerShell 中重新执行内核注册命令：

```powershell
cd <FP-AI-MONITOR2_ROOT>
.\usc_train\Scripts\Activate.ps1
python -m ipykernel install --user --name usc_train --display-name "usc_train (Python 3.10.1)"
```

然后重启 VS Code 或重新打开 `train_command_classifier.ipynb`。

### 5.4 在 VS Code 中训练模型

在运行训练前，先检查 Notebook 中以下内容是否已经根据本机路径和当前数据集修改：

- `dataset_path`：命令词数据集根目录，目录下应包含 `metadata.csv` 和 `wav/`。
- `frame_len`：输入音频采样点数，应与数据预处理和固件端参数一致。
- `classes`：类别顺序，应与 `metadata.csv`、数据目录和固件端类别映射一致。
- 输出目录：用于保存训练结果、混淆矩阵、`.npz` 文件和 `model.tflite`。

确认后，在 VS Code 中按照 Notebook 单元格顺序运行。建议先运行导入依赖和数据检查单元，确认数据集能正常读取后，再运行训练单元。

训练完成后，应重点保存或检查以下输出：

- `model.tflite`
- 训练/验证准确率和损失曲线
- 混淆矩阵
- `train_valid_data.npz` 或等价训练缓存
- `post_validation_data.npz`


## 6. 数据集要求

命令词模型为 11 分类任务：

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

注意：

- 类别顺序必须与固件中 `Src/app_threadx.c` 的类别名称数组和蓝牙 AT 指令映射一致。
- `background` 默认作为最后一类，固件中通过 `AI_USC_NETWORK_OUT_1_SIZE - 1U` 判断背景类。

## 7. 模型导出

训练完成后，需要导出：

```text
model.tflite
```

该文件用于导入 STM32CubeMX / STM32Cube.AI 并生成 MCU C 代码。


## 8. STM32Cube.AI 前置检查

导入 `model.tflite` 前建议确认：

- 模型输入形状与固件前端特征一致。
- 当前固件中的命令词输入特征为 `60 x 32 x 1`。
- 输出类别数为 11。
- 训练时的 MFCC 参数与固件中 `Inc/audio_config.h`、`Src/usc_preproc.c` 保持一致。

固件相关参数：

```text
../../stm32-firmware/Tx_LowPower_echo/Inc/audio_config.h
../../stm32-firmware/Tx_LowPower_echo/Src/usc_preproc.c
```

官方 STM32CubeMX 和 X-CUBE-AI 下载页：

```text
https://www.st.com/en/development-tools/stm32cubemx.html
https://www.st.com/en/embedded-software/x-cube-ai.html
```

## 9. 复现检查清单

1. 下载并解压 ST 官方 FP-AI-MONITOR2。
2. 将 `prepare_command_dataset.py` 和 `train_command_classifier.ipynb` 放入官方 `Utilities\AI_Resources\TrainingScripts\UltrasoundClassification` 目录。
3. 在 PowerShell 中进入 FP-AI-MONITOR2 根目录，创建并激活 `usc_train` 虚拟环境。
4. 在 `usc_train` 中安装训练依赖，并注册 VS Code 可选择的内核。
5. 在 VS Code 中打开官方 `UltrasoundClassification` 目录。
6. 打开 `train_command_classifier.ipynb`，在右上角选择 `usc_train (Python 3.10.1)` 或 `<FP-AI-MONITOR2_ROOT>\usc_train\Scripts\python.exe`。
7. 将 Notebook 中的数据集路径、元数据路径和结果输出路径设置为当前本机路径。
8. 确认 `dataset_path` 末尾斜杠、`frame_len` 和 `classes` 均已按当前数据集设置。
9. 确认 `metadata.csv`、WAV 目录和类别名一致。
10. 在 VS Code 中按单元格顺序运行 Notebook 完成训练。
11. 导出 `model.tflite`。
12. 在 STM32CubeMX / STM32Cube.AI 中导入 `model.tflite`，网络名称设置为 `usc_network`。
13. 生成代码并替换固件中的 `usc_network*` 文件。
14. 重新编译 STM32 固件并进行板端测试。

## 10. 常见问题

- 如果 TensorFlow 安装失败，检查 Python 版本是否受当前 TensorFlow 支持。
- 如果 Notebook 找不到数据，检查数据集根目录和 `metadata.csv` 路径。
- 如果 VS Code 运行时使用了错误解释器，检查右上角 Notebook kernel 是否为 `usc_train (Python 3.10.1)` 或当前训练虚拟环境。
- 如果提示找不到 `prepare_command_dataset`，检查 Notebook 是否位于 `prepare_command_dataset.py` 同一目录，或检查 Python 工作目录是否正确。
- 如果 MCU 端类别错位，检查训练标签顺序、模型输出顺序和 `app_threadx.c` 中类别映射。
- 如果 STM32Cube.AI Analyze 显示 RAM/Flash 超限，需要简化模型、量化模型或调整网络结构。
