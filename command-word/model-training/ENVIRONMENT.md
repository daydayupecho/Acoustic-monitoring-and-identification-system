# 环境配置说明：命令词模型训练

本目录用于命令词识别模型的数据准备、训练、评估和 TensorFlow Lite 导出。

主要文件：

```text
prepare_command_dataset.py
train_command_classifier.ipynb
```

## 1. 推荐环境

- 操作系统：Windows 10/11
- Python：建议 3.10 或 3.11
- Jupyter Notebook 或 JupyterLab
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

## 2. Python 虚拟环境

建议在官方 `UltrasoundClassification` 示例目录或其上级目录创建独立虚拟环境：

```powershell
cd <FP-AI-MONITOR2_ROOT>\Utilities\AI_Resources\TrainingScripts\UltrasoundClassification
python -m venv .venv-command
.\.venv-command\Scripts\Activate.ps1
python -m pip install --upgrade pip
```

安装训练依赖：

```powershell
python -m pip install numpy pandas scipy librosa scikit-learn tensorflow jupyter matplotlib seaborn
```

如果需要读取 ST HSDatalog 数据，可按实际 SDK 版本安装：

```powershell
python -m pip install stdatalog-core
```

或使用已有的 `st_hsdatalog` 环境。

实验训练时也可在项目根目录创建名为 `usc_train` 的虚拟环境，并在 VS Code 中选择该解释器。截图中的实验路径示例如下：

```powershell
D:
cd D:\workspace\project\keil\thesis_speech_recognition
.\usc_train\Scripts\Activate.ps1
```

如果需要新建该环境，可使用：

```powershell
py -3.10 -m venv usc_train
.\usc_train\Scripts\Activate.ps1
python -m pip install --upgrade pip setuptools wheel
python -m pip install numpy pandas scipy librosa scikit-learn tensorflow jupyter matplotlib seaborn soundfile stdatalog-core ipykernel
python -m ipykernel install --user --name usc_train --display-name "usc_train (Python 3.10.1)"
```

在 VS Code 中打开项目文件夹后，需要在右上角选择 `usc_train (Python 3.10.1)` 作为 Notebook 解释器，然后运行训练 Notebook。

```text
D:\workspace\project\keil\thesis_speech_recognition
```

训练过程截图见：

```text
../../docs/figures/Fig_Command_model_training_process.png
```

如果使用 ST 官方 FP-AI-MONITOR2 原始示例，Notebook 文件名通常为：

```text
UltrasoundClassification.ipynb
```

本仓库中对应整理后的文件为：

```text
train_command_classifier.ipynb
```

## 3. 训练 Notebook 需要设置的关键参数

运行 Notebook 前，需要根据当前复现环境设置数据路径、输入帧长和类别列表。截图中的训练过程主要修改以下参数。

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

`frame_length` 表示一次推理使用的原始音频采样点数。截图中的训练示例为：

```python
frame_length = 4096 * 63 + 1  # each inference will use this many samples
```

实际复现时，`frame_length` 需要与前处理阶段的 WAV 长度检查、训练特征提取参数以及固件端命令词前端保持一致。例如，若使用 `audit_wav_and_metadata.py` 检查的目标采样点数为 `19456`，则训练 Notebook 中的输入长度也应按当前实验设置同步确认。

### 3.3 类别列表

`classes` 的顺序决定模型输出类别顺序，必须与 `metadata.csv` 中的标签、训练数据目录名和固件端 `Src/app_threadx.c` 中的类别映射保持一致。

截图中的四分类调试示例为：

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

## 5. 依赖检查

运行：

```powershell
python -c "import numpy, pandas, librosa, scipy, sklearn, tensorflow; print('command model training environment OK')"
```

启动 Notebook：

```powershell
jupyter notebook
```

然后打开：

```text
train_command_classifier.ipynb
```

或在 VS Code 中打开 ST 示例原名：

```text
UltrasoundClassification.ipynb
```

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

导出后建议同时保存：

- 训练和验证曲线。
- 混淆矩阵。
- `train_valid_data.npz` 或等价训练/测试数据缓存。
- `post_validation_data.npz`，用于 STM32Cube.AI 或后续验证。

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
3. 创建 Python 虚拟环境并安装依赖。
4. 在 VS Code 或 Jupyter 中选择 `usc_train (Python 3.10.1)` 或等价训练环境。
5. 将 Notebook 中的数据集路径、元数据路径和结果输出路径设置为当前本机路径。
6. 确认 `dataset_path` 末尾斜杠、`frame_length` 和 `classes` 均已按当前数据集设置。
7. 确认 `metadata.csv`、WAV 目录和类别名一致。
8. 运行 Notebook 完成训练。
9. 导出 `model.tflite`。
10. 在 STM32CubeMX / STM32Cube.AI 中导入 `model.tflite`，网络名称设置为 `usc_network`。
11. 生成代码并替换固件中的 `usc_network*` 文件。
12. 重新编译 STM32 固件并进行板端测试。

## 10. 常见问题

- 如果 TensorFlow 安装失败，检查 Python 版本是否受当前 TensorFlow 支持。
- 如果 Notebook 找不到数据，检查数据集根目录和 `metadata.csv` 路径。
- 如果 VS Code 运行时使用了错误解释器，检查右上角 Notebook kernel 是否为 `usc_train (Python 3.10.1)` 或当前训练虚拟环境。
- 如果提示找不到 `prepare_command_dataset`，检查 Notebook 是否位于 `prepare_command_dataset.py` 同一目录，或检查 Python 工作目录是否正确。
- 如果 MCU 端类别错位，检查训练标签顺序、模型输出顺序和 `app_threadx.c` 中类别映射。
- 如果 STM32Cube.AI Analyze 显示 RAM/Flash 超限，需要简化模型、量化模型或调整网络结构。
