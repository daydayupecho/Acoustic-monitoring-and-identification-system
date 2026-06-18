# 环境配置说明：命令词模块

本目录包含命令词数据预处理和模型训练两部分。

子目录：

```text
preprocessing/
model-training/
```

详细配置：

- 数据预处理：`preprocessing/ENVIRONMENT.md`
- 模型训练：`model-training/ENVIRONMENT.md`

## 推荐工具

- PowerShell 7 或更高版本
- Python 3.10 或 3.11
- Jupyter Notebook / JupyterLab
- TensorFlow / Keras
- STM32CubeMX + STM32Cube.AI
- STM32CubeIDE

官方来源：

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

## 必须放置到官方目录的程序

为了复现本文流程，两个子目录的程序需要放到对应 ST 官方包中运行。

### 1. `preprocessing`

将：

```text
preprocessing/convert_stdatalog_to_wav.ps1
```

复制到：

```text
<STDATALOG-PYSDK_ROOT>\stdatalog-pysdk\
```

即最终路径应为：

```text
<STDATALOG-PYSDK_ROOT>\stdatalog-pysdk\convert_stdatalog_to_wav.ps1
```

### 2. `model-training`

将：

```text
model-training/prepare_command_dataset.py
model-training/train_command_classifier.ipynb
```

复制到 FP-AI-MONITOR2 官方示例目录：

```text
<FP-AI-MONITOR2_ROOT>\Utilities\AI_Resources\TrainingScripts\UltrasoundClassification\
```

## 开发流程概览

1. 使用 `host-acquisition/` 采集命令词原始数据。
2. 使用 `preprocessing/convert_stdatalog_to_wav.ps1` 将采集数据转换为 WAV。
3. 使用 `preprocessing/audit_wav_and_metadata.py` 检查 WAV 和 `metadata.csv`。
4. 使用 `model-training/prepare_command_dataset.py` 准备训练数据。
5. 使用 `model-training/train_command_classifier.ipynb` 训练命令词模型。
6. 导出 `model.tflite`。
7. 在 STM32CubeMX / STM32Cube.AI 中导入 `model.tflite`，生成 `usc_network*` C 代码。
8. 将生成代码集成到 `../stm32-firmware/Tx_LowPower_echo/`。

## 与固件的关键一致性

需要保持一致的内容：

- 命令词类别顺序
- 背景类是否为最后一类
- MFCC / Mel 特征参数
- 模型输入形状
- 固件中的 AT 指令映射

固件中需要重点检查：

```text
../stm32-firmware/Tx_LowPower_echo/Inc/audio_config.h
../stm32-firmware/Tx_LowPower_echo/Src/usc_preproc.c
../stm32-firmware/Tx_LowPower_echo/Src/app_threadx.c
```
