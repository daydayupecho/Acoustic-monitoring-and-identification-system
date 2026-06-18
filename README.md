# Acoustic Monitoring and Identification System

[English version](README.md)

本仓库用于声学监测与识别系统的程序归档，包含数据采集上位机、唤醒词数据格式转换、命令词数据预处理、命令词识别模型训练以及 STM32 端固件工程。

## 目录结构

```text
Acoustic-monitoring-and-identification-system/
├── host-acquisition/              # 数据集采集上位机程序
├── wake-word/                     # 唤醒词相关数据处理程序
│   └── nanoedge-converter/
├── command-word/                  # 命令词数据处理与模型训练程序
│   ├── preprocessing/
│   └── model-training/
├── stm32-firmware/                # STM32 嵌入式端工程
└── bluetooth-receiver-host/       # 蓝牙接收端上位机程序预留目录
```

## 程序说明

### 1. 数据集采集上位机

路径：`host-acquisition/host_acquisition_gui.py`

该程序是用 Python 3.13 编写的数据采集上位机，用于通过串口接收下位机采集到的音频数据，并进行实时波形显示和数据保存。

主要功能：

- 自动枚举串口并连接采集设备。
- 向下位机发送 `START`、`STOP`、`PING` 等控制指令。
- 实时解析串口数据流，显示采集波形。
- 按 ST HSDatalog 风格保存采集数据，包括 `.dat` 数据文件以及 `acquisition_info.json`、`device_config.json` 等配置文件。
- 为后续唤醒词和命令词数据处理流程提供原始数据。

适用场景：

- 采集唤醒词、命令词或环境声学数据集。
- 调试下位机音频采集和串口传输状态。
- 快速查看采集波形是否正常。

### 2. 唤醒词数据集格式处理程序

路径：`wake-word/nanoedge-converter/`

该目录用于将自定义 STM32/ST-HSD 风格采集数据转换为 NanoEdge AI Studio 可用的 CSV 格式，主要服务于唤醒词识别或异常检测类模型的数据准备。

#### `dat_to_nanoedge_csv.py`

核心转换脚本。

主要功能：

- 读取每个采集目录中的 `.dat` 数据文件和 JSON 配置文件。
- 去除传输协议中附加的 4 字节计数器。
- 去除数据流中周期性插入的时间戳字段。
- 按指定窗口长度和步长切分音频采样点。
- 将每个窗口导出为 NanoEdge AI Studio 可导入的 CSV 行或列。
- 支持按传感器名称、采样率、窗口长度、窗口步长等参数进行转换。

典型用途：

```powershell
python dat_to_nanoedge_csv.py <采集数据根目录> -o <输出目录> -s imp23absu_mic -sl 8192 -si 8192 --odr 16000 --csv-shape row
```

#### `run_nanoedge_conversion.bat`

Windows 批处理示例脚本，用于调用 `dat_to_nanoedge_csv.py` 批量转换数据。

使用前需要根据本机数据位置修改脚本中的：

- `ROOT`：输入数据根目录。
- `OUT_ALL`：CSV 输出目录。
- `SL`：窗口长度。
- `SI`：窗口步长。
- `ODR`：采样率。

### 3. 命令词数据集格式处理程序

路径：`command-word/preprocessing/`

该目录用于将命令词原始采集数据整理为模型训练所需的数据集格式。

#### `convert_stdatalog_to_wav.ps1`

PowerShell 批处理脚本，用于把 ST DATALOG/STDATALOG 采集目录批量转换为 WAV 文件，并可生成失败报告和后续训练用的元数据文件。

主要功能：

- 批量扫描命令词采集目录。
- 调用 ST 的 `stdatalog_to_wav.py` 工具将 `.dat` 采集数据转换为 `.wav`。
- 支持多进程并行转换，提高大批量数据处理速度。
- 使用固定的设备 JSON 配置，避免并行转换时反复写入 catalog。
- 将转换结果按类别和采集名称整理输出。
- 记录转换失败的采集项，并导出 CSV/XLSX 失败报告。
- 可选调用后处理脚本生成 `metadata.csv` 并整理训练目录。

注意事项：

- 该脚本中包含本机绝对路径，上传或复现前需要修改 `$Root`、`$Out`、`$Py`、`$ToWav`、`$PinnedJsonPath` 等变量。
- 脚本要求 PowerShell 7 或更高版本。
- 需要提前配置好 ST DATALOG/STDATALOG Python SDK 环境。

#### `audit_wav_and_metadata.py`

WAV 数据和 `metadata.csv` 审核脚本。

主要功能：

- 检查 `metadata.csv` 中记录的每条采集数据是否存在对应 WAV 文件。
- 检查 WAV 文件采样点数是否满足模型训练所需的最小长度。
- 输出缺失、读取失败或长度不足的数据项。
- 生成 `audit_report.csv`，用于定位数据集中的异常样本。

典型用途：

```powershell
python audit_wav_and_metadata.py <数据集根目录> <frame_len> [offset]
```

示例：

```powershell
python audit_wav_and_metadata.py E:\SPEECH_DATA\20251029-command 188416 0
```

### 4. 命令词识别模型训练脚本

路径：`command-word/model-training/`

该目录用于命令词识别模型的数据准备、特征提取、训练、评估和可视化分析。

#### `prepare_command_dataset.py`

命令词数据准备脚本，封装了 `UltrasoundDataHelper` 数据处理类。

主要功能：

- 读取命令词 WAV 数据集和元数据。
- 按类别加载样本，例如风扇开关、灯光开关、电视开关等命令词类别。
- 对原始音频执行偏移裁剪、分帧和样本切分。
- 提取 Mel 频谱、MFCC 等特征。
- 自动划分训练集、验证集和测试集。
- 为后续 TensorFlow/Keras 模型训练准备输入数据和标签。

适用场景：

- 训练命令词分类模型前的数据准备。
- 对不同采样率、帧长、MFCC 参数进行实验。
- 检查数据集切分和特征形状是否符合模型输入要求。

#### `train_command_classifier.ipynb`

命令词分类模型训练 Notebook。

主要功能：

- 调用数据准备脚本生成训练数据。
- 构建和训练命令词分类模型。
- 输出模型训练过程和评估指标。
- 导出分类报告、性能结果和可视化图表。
- 支持 ROC、t-SNE、MFCC 聚类等分析图的生成，用于观察不同命令词类别的可分性。

适用场景：

- 交互式调参和模型训练。
- 展示模型训练结果。
- 生成论文或报告中需要的评估图表。

### 5. STM32 嵌入式端固件工程

路径：`stm32-firmware/Tx_LowPower_echo/`

该目录为 STM32U575 平台上的嵌入式端工程，包含音频采集、特征提取、AI 推理、串口调试和低功耗相关代码。工程保留了 STM32CubeIDE、Keil MDK-ARM 和 IAR EWARM 等工具链相关文件。

主要内容：

- `Src/`：应用源代码，包括音频采集、特征提取、AI 推理、串口调试、ThreadX 任务等。
- `Inc/`：头文件和模型相关配置文件。
- `STM32CubeIDE/`：STM32CubeIDE 工程文件。
- `MDK-ARM/`：Keil MDK 工程文件。
- `EWARM/`：IAR EWARM 工程文件。
- `Middlewares/`：ST AI 库、NanoEdge AI 库等中间件。
- `docs/`：文档资料。
- `Tx_LowPower.ioc`：STM32CubeMX 工程配置文件。

主要功能：

- 基于 STM32U575 采集麦克风音频数据。
- 执行前端特征提取，例如窗函数、FFT、Mel 滤波器、MFCC 等。
- 运行嵌入式 AI 模型进行命令词或声学状态识别。
- 通过 UART 输出调试信息或识别结果。
- 支持 ThreadX 低功耗相关框架。

### 6. 蓝牙接收端上位机程序

路径：`bluetooth-receiver-host/`

该目录目前为空，建议后续用于存放蓝牙接收端上位机同步显示程序。若该功能已经合并到 `host-acquisition/`，也可以删除该空目录，避免 GitHub 读者误以为缺少文件。

## 推荐使用流程

1. 将训练完成的模型和参数部署到 `stm32-firmware/Tx_LowPower_echo/` 对应固件工程中。并修改为APP_RUN_MODE_DATA_CAPTURE模式，用于数据集采集。
2. 使用 `host-acquisition/host_acquisition_gui.py` 连接下位机并采集原始数据,可以用于采集唤醒词数据集和命令词数据集。
3. 如果处理唤醒词数据，使用 `wake-word/nanoedge-converter/dat_to_nanoedge_csv.py` 转换为 NanoEdge AI 所需 CSV,然后使用NanoEdge AI Studio进行数据预处理和模型训练。
4. 如果处理命令词数据，使用 `command-word/preprocessing/convert_stdatalog_to_wav.ps1` 将原始采集数据转换为 WAV。使用 `command-word/preprocessing/audit_wav_and_metadata.py` 检查 WAV 文件和元数据完整性。使用 `command-word/model-training/prepare_command_dataset.py` 和 `train_command_classifier.ipynb` 训练并评估命令词识别模型。
