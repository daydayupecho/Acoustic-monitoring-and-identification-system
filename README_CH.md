# Acoustic Monitoring and Identification System 中文说明

[English version](README_EN.md) | [当前 README](README.md)

本仓库用于声学监测与识别系统的程序归档，覆盖从数据采集、数据格式转换、命令词模型训练到 STM32 端部署的完整流程。仓库主要包含以下部分：

- 数据采集上位机程序
- 唤醒词数据集格式转换程序
- 命令词数据预处理程序
- 命令词识别模型训练脚本
- STM32 嵌入式端固件工程
- 蓝牙接收端 UART 上位机程序及界面资源

## 目录结构

```text
Acoustic-monitoring-and-identification-system/
├── host-acquisition/              # Python 数据采集上位机
├── wake-word/                     # 唤醒词数据处理程序
│   └── nanoedge-converter/
├── command-word/                  # 命令词数据预处理与模型训练
│   ├── preprocessing/
│   └── model-training/
├── stm32-firmware/                # STM32 嵌入式端工程
│   └── Tx_LowPower_echo/
└── bluetooth-receiver-host/       # 蓝牙接收端 UART 上位机程序与界面素材
```

## 1. 数据采集上位机

路径：`host-acquisition/host_acquisition_gui.py`

该程序是用 Python 编写的数据采集上位机，用于通过串口与 STM32 下位机通信，接收下位机采集到的音频数据，并进行实时波形显示和数据保存。

主要功能：

- 枚举并连接串口设备。
- 向下位机发送 `START`、`STOP`、`PING` 等控制指令。
- 接收并解析下位机上传的音频采样数据。
- 实时显示采集波形，便于检查采集质量。
- 保存 ST HSDatalog 风格的数据文件，包括 `.dat`、`acquisition_info.json`、`device_config.json` 等。
- 为唤醒词和命令词数据处理流程提供原始采集数据。

运行环境：

- Python 3.13（本文实验使用版本）；建议使用 Python 3.10 及以上
- `pyserial`
- `matplotlib`

使用前需要确认：

- STM32 固件已切换到数据采集模式。
- 串口号和波特率设置正确。
- 保存目录具有写入权限。

## 2. 唤醒词数据集格式处理程序

路径：`wake-word/nanoedge-converter/`

该目录用于将上位机保存的 STM32/ST-HSD 风格采集数据转换为 NanoEdge AI Studio 可导入的 CSV 格式，主要服务于唤醒词识别或异常检测数据集准备。

### `dat_to_nanoedge_csv.py`

核心转换脚本。

主要功能：

- 读取采集目录中的 `.dat` 数据文件和 JSON 配置文件。
- 去除数据传输过程中附加的 4 字节协议计数器。
- 去除数据流中周期性插入的时间戳字段。
- 按指定窗口长度和窗口步长切分音频采样点。
- 导出 NanoEdge AI Studio 可导入的 CSV 文件。
- 支持设置传感器名称、采样率、窗口长度、窗口步长和 CSV 排列方式。

典型命令：

```powershell
python dat_to_nanoedge_csv.py <采集数据根目录> -o <输出目录> -s imp23absu_mic -sl 8192 -si 8192 --odr 16000 --csv-shape row
```

常用参数说明：

- `<采集数据根目录>`：包含多个采集文件夹的根目录。
- `-o`：CSV 输出目录。
- `-s`：传感器名称，例如 `imp23absu_mic`。
- `-sl`：每个样本窗口的长度。
- `-si`：相邻窗口之间的步长。
- `--odr`：采样率。
- `--csv-shape`：CSV 输出形状，可按脚本支持的方式选择行或列。

### `run_nanoedge_conversion.bat`

Windows 批处理示例脚本，用于批量调用 `dat_to_nanoedge_csv.py`。

使用前需要根据本机数据路径修改：

- `ROOT`：输入数据根目录。
- `OUT_ALL`：CSV 输出目录。
- `SL`：窗口长度。
- `SI`：窗口步长。
- `ODR`：采样率。

## 3. 命令词数据集格式处理程序

路径：`command-word/preprocessing/`

该目录用于将命令词原始采集数据转换和整理为模型训练所需的数据集格式。

### `convert_stdatalog_to_wav.ps1`

PowerShell 批处理脚本，用于将上位机保存的 STM32/ST-HSD 风格采集数据批量转换为 WAV 文件，并生成失败报告和训练用的元数据。

主要功能：

- 批量扫描命令词采集目录。
- 调用 ST 的 `stdatalog_to_wav.py` 工具将 `.dat` 数据转换为 `.wav`。
- 支持 PowerShell 7 并行处理，提高大批量数据转换速度。
- 使用固定设备 JSON 配置，避免并行转换时重复写入 catalog。
- 按命令词类别和采集名称整理 WAV 输出目录。
- 导出转换失败记录，支持 CSV/XLSX 报告。
- 可选执行后处理步骤，生成 `metadata.csv` 并整理训练目录。

运行环境：

- PowerShell 7 或更高版本。
- ST DATALOG/STDATALOG Python SDK 环境。
- 将脚本复制到 `<STDATALOG-PYSDK_ROOT>\stdatalog-pysdk\` 后，按 `command-word/preprocessing/ENVIRONMENT.md` 第 3 至 6 节配置 STDATALOG-PYSDK 环境、脚本放置位置和路径参数。
- `$Py` 应设置为 STDATALOG-PYSDK 虚拟环境中的 Python，例如 `<STDATALOG-PYSDK_ROOT>\stdatalog-pysdk\.venv_dlog\Scripts\python.exe`。
- `$ToWav` 应设置为 ST 官方转换脚本，例如 `<STDATALOG-PYSDK_ROOT>\stdatalog-pysdk\stdatalog_examples\cli_applications\stdatalog_to_wav.py`。

注意：

- 复现者需要在脚本开头根据本机环境设置 `$Root`、`$Out`、`$StageRoot`、`$Py`、`$ToWav`、`$PinnedJsonPath`、`$JsonSources` 和 `$DtdlDir`。
- `$Root` 指向上位机保存的命令词 `.dat` 采集数据根目录，`$Out` 指向转换后的 WAV 输出目录。
- 若开启 XLSX 报告导出，需要对应 Python 环境安装 `openpyxl`。

### `audit_wav_and_metadata.py`

WAV 文件和 `metadata.csv` 审核脚本。

主要功能：

- 检查 `metadata.csv` 中记录的采集样本是否存在对应 WAV 文件。
- 检查 WAV 文件采样点数是否满足模型输入长度要求。
- 输出缺失、读取失败或长度不足的样本记录。
- 生成 `audit_report.csv`，便于定位数据集异常。

运行环境：

- Python 3.x
- `soundfile`

典型命令：

```powershell
python audit_wav_and_metadata.py <数据集根目录> <frame_len> [offset]
```

示例：

```powershell
cd D:\workspace\project\keil\thesis_speech_recognition
python .\audit_wav_and_metadata.py "E:\SPEECH_DATA\TENG\touming-dataset\command-20260315-merge" 19456 0
```

其中，`19456` 为本次检查要求的 WAV 采样点数，最后的 `0` 为读取偏移量。

## 4. 命令词识别模型训练脚本

路径：`command-word/model-training/`

该目录用于命令词识别模型的数据准备、特征提取、训练、评估和可视化。

### `prepare_command_dataset.py`

命令词数据准备脚本，主要封装 `UltrasoundDataHelper` 数据处理类。

主要功能：

- 读取命令词 WAV 数据集和元数据。
- 按类别加载命令词样本。
- 对音频执行偏移裁剪、分帧和样本切分。
- 提取 Mel 频谱、MFCC 等特征。
- 自动划分训练集、验证集和测试集。
- 为 TensorFlow/Keras 模型训练准备输入数据和标签。
- 兼容部分 ST HSDatalog 数据读取环境。

运行环境：

- Python 3.x
- `numpy`
- `pandas`
- `librosa`
- `scipy`
- `scikit-learn`
- `tensorflow`
- 可选：`stdatalog_core` 或 `st_hsdatalog`

### `train_command_classifier.ipynb`

命令词分类模型训练 Notebook。

主要功能：

- 调用数据准备脚本生成训练数据。
- 构建并训练命令词分类模型。
- 输出训练过程、测试结果和评估指标。
- 导出分类报告和模型性能结果。
- 生成 ROC、t-SNE、MFCC 聚类等可视化结果，用于分析不同命令词类别的可分性。

训练前需要在 VS Code 或 Jupyter 中选择命令词训练虚拟环境，例如 `usc_train (Python 3.10.1)`，并在 Notebook 中设置：

- `dataset_path`：命令词数据集根目录，通常应包含 `metadata.csv` 和 `wav/`。
- `frame_length`：一次推理使用的原始音频采样点数。
- `classes`：类别列表和顺序，必须与固件端类别映射一致。

具体操作步骤见 `command-word/model-training/ENVIRONMENT.md`。

适用场景：

- 命令词识别模型训练。
- 模型参数调试。
- 论文或报告中的实验结果可视化。

## 5. STM32 嵌入式端固件工程

路径：`stm32-firmware/Tx_LowPower_echo/`

该目录为 STM32U575 平台上的嵌入式端工程，包含音频采集、特征提取、AI 推理、串口通信和低功耗相关代码。

主要内容：

- `Src/`：应用源代码，包括音频采集、特征提取、AI 推理、串口调试、ThreadX 任务等。
- `Inc/`：头文件和模型相关配置。
- `Inc/power_test_cfg.h`：运行模式和功耗测试相关配置。
- `STM32CubeIDE/`：STM32CubeIDE 工程文件。
- `MDK-ARM/`：Keil MDK 工程文件。
- `EWARM/`：IAR EWARM 工程文件。
- `Middlewares/`：ST AI 库、NanoEdge AI 库等中间件。
- `docs/`：流程图、BOM 和系统说明图等文档资料。
- `Tx_LowPower.ioc`：STM32CubeMX 工程配置文件。

主要功能：

- 采集麦克风音频数据。
- 执行前端特征提取，例如窗函数、FFT、Mel 滤波器、MFCC 等。
- 运行嵌入式 AI 模型，实现命令词或声学状态识别。
- 通过 UART 输出调试信息、采集数据或识别结果。
- 支持 ThreadX 低功耗相关框架。

运行模式说明：

- 数据采集时，可在 `Inc/power_test_cfg.h` 中将 `APP_RUN_MODE` 配置为 `APP_RUN_MODE_DATA_CAPTURE`。
- 模型识别时，可将 `APP_RUN_MODE` 配置为 `APP_RUN_MODE_RECOGNITION`。

## 6. 蓝牙接收端 UART 上位机程序

路径：`bluetooth-receiver-host/`

该目录用于存放蓝牙接收端 UART 上位机程序、通信说明文档和界面素材。与 Supporting Information Fig. S36 所示链路一致，MATLAB App 不直接接收蓝牙空口数据，也不参与识别或控制决策；它只通过 PC 串口 / UART 接收蓝牙接收端输出的继电器状态、解析结果或状态字符串，并用于上位机界面显示。

主要内容：

- `xiaobiao.mlapp`、`白色背景.mlapp`：MATLAB App Designer 工程文件。
- `蓝牙通信说明新.docx`：蓝牙接收端与上位机串口通信相关说明文档。
- `data.txt`：蓝牙接收端通过 UART 输出到上位机的数据示例。
- `.png`、`.jpg`、`.gif`：上位机界面中使用的图片和动画素材。
- `app1.prj`：MATLAB App/工程相关文件。

功能定位：

- 蓝牙接收端包含 BLE 接收和 MCU 命令解析功能，并控制 5 路继电器 / 插座通道。
- MATLAB App 是可选的 PC 监视界面，只显示继电器或家电状态。
- 语音识别、命令词判断和继电器控制决策由嵌入式端和蓝牙接收控制模块完成，不由 MATLAB App 完成。

运行环境：

- MATLAB。
- 如程序中使用 MATLAB 串口通信功能，需要根据 `.mlapp` 工程实际内容确认对应 MATLAB 工具箱。

## 7. 系统开发流程

本仓库对应的完整开发流程如下，根目录只说明系统级步骤；各软件的环境配置、官方包下载、必须放置的目录和参数修改见下一节的模块说明。

### 7.1 固件准备

1. 下载 ST 官方 STM32CubeU5 固件包。
2. 将 `stm32-firmware/Tx_LowPower_echo/` 放入官方目录：

```text
<STM32CubeU5_ROOT>/Projects/NUCLEO-U575ZI-Q/Applications/ThreadX/Tx_LowPower_echo
```

3. 在 `Inc/power_test_cfg.h` 中配置运行模式。
   - 采集数据集时使用 `APP_RUN_MODE_DATA_CAPTURE`。
   - 部署识别系统时使用 `APP_RUN_MODE_RECOGNITION`。
4. 使用 STM32CubeIDE 编译并烧录到 STM32U575 设备。

### 7.2 数据采集

1. 运行 `host-acquisition/host_acquisition_gui.py`。
2. 通过 UART 与下位机建立连接。
3. 采集 S-TAU 输出的音频数据。
4. 保存 `.dat`、`acquisition_info.json` 和 `device_config.json` 等原始数据文件。

### 7.3 唤醒词模型开发

1. 使用 `wake-word/nanoedge-converter/dat_to_nanoedge_csv.py` 将唤醒词原始 `.dat` 数据转换为 NanoEdge AI Studio 可导入的 CSV。
2. 在 NanoEdge AI Studio 中建立二分类项目。
3. 导入 `echo` 正样本和非唤醒词负样本。
4. 运行 Benchmark 和 Validation。
5. 导出 NanoEdge AI 静态库。
6. 将导出的 `NanoEdgeAI.h` 和 `libneai.a` 集成到 `stm32-firmware/Tx_LowPower_echo/`。

### 7.4 命令词模型开发

1. 将命令词原始 `.dat` 数据放入 ST 官方 STDATALOG-PYSDK 环境。
2. 使用 `command-word/preprocessing/convert_stdatalog_to_wav.ps1` 转换为 WAV。
3. 使用 `audit_wav_and_metadata.py` 检查 WAV 文件和 `metadata.csv`。
4. 将训练脚本放入 ST 官方 FP-AI-MONITOR2 的 `UltrasoundClassification` 示例目录。
5. 使用 `prepare_command_dataset.py` 和 `train_command_classifier.ipynb` 训练 11 分类命令词模型。
6. 导出 `model.tflite`。
7. 使用 STM32CubeMX / STM32Cube.AI 将 `model.tflite` 转换为 MCU C 代码。
8. 将生成的 `usc_network*` 文件集成到 `stm32-firmware/Tx_LowPower_echo/`。

### 7.5 系统部署和验证

1. 将固件切换到 `APP_RUN_MODE_RECOGNITION`。
2. 编译并烧录完整固件。
3. 测试唤醒词筛选是否能正确进入命令词识别阶段。
4. 测试 10 类有效命令词和 `background` 背景类。
5. 检查固件端蓝牙 AT 指令发送、蓝牙接收端解析结果、继电器状态和 MATLAB 上位机 UART 显示是否一致。

## 8. 各模块详细环境配置入口

每个功能模块的环境配置、官方程序下载地址、文件放置位置和需要设置的参数分别写在对应目录下：

| 模块 | 说明文件 | 主要内容 |
|---|---|---|
| 数据采集上位机 | `host-acquisition/ENVIRONMENT.md` | Python 依赖、串口连接、采集参数 |
| 唤醒词模块总览 | `wake-word/ENVIRONMENT.md` | NanoEdge AI Studio 工作流总览 |
| 唤醒词 CSV 转换 | `wake-word/nanoedge-converter/ENVIRONMENT.md` | CSV 转换参数、NanoEdge AI Studio 导入、静态库导出 |
| 命令词模块总览 | `command-word/ENVIRONMENT.md` | STDATALOG-PYSDK 和 FP-AI-MONITOR2 放置要求 |
| 命令词预处理 | `command-word/preprocessing/ENVIRONMENT.md` | 必须放入 STDATALOG-PYSDK、`.dat` 到 WAV、路径参数 |
| 命令词训练 | `command-word/model-training/ENVIRONMENT.md` | 必须放入 FP-AI-MONITOR2 UltrasoundClassification、训练依赖、`model.tflite` 导出 |
| STM32 固件模块总览 | `stm32-firmware/ENVIRONMENT.md` | STM32CubeU5 官方目录和模型集成总览 |
| STM32 固件工程 | `stm32-firmware/Tx_LowPower_echo/ENVIRONMENT.md` | 必须放入 STM32CubeU5 ThreadX 目录、NanoEdge AI 和 STM32Cube.AI 导入 |
| 蓝牙接收端 UART 上位机 | `bluetooth-receiver-host/ENVIRONMENT.md` | MATLAB App、UART 串口接收、界面素材 |

## 9. 官方程序下载入口

复现实验需要用到以下官方工具或软件包：

- STM32CubeU5: `https://github.com/STMicroelectronics/STM32CubeU5`
- STDATALOG-PYSDK: `https://github.com/STMicroelectronics/stdatalog-pysdk`
- FP-AI-MONITOR2: `https://www.st.com/en/embedded-software/fp-ai-monitor2.html`
- STM32CubeMX: `https://www.st.com/en/development-tools/stm32cubemx.html`
- X-CUBE-AI / STM32Cube.AI: `https://www.st.com/en/embedded-software/x-cube-ai.html`
- NanoEdge AI Studio wiki: `https://wiki.st.com/stm32mcu/wiki/AI%3ANanoEdge_AI_Studio`
- NanoEdge AI Studio download入口: `https://stm32ai.st.com/nanoedge-ai-studio/`

## 复现和使用注意事项

- 复现者应优先阅读根目录说明，再进入各功能模块的 `ENVIRONMENT.md` 查看具体环境配置。
- 各脚本中的数据输入目录、输出目录、Python 解释器路径和 ST 官方工具路径需要根据本机安装位置设置。
- IDE 编译输出、缓存文件、日志和临时文件不是复现实验的必要输入，可由本地环境重新生成。
- ST AI 库、NanoEdge AI 库和 STM32Cube.AI 生成文件涉及 ST 官方工具链和授权约束时，请以 ST 官方软件包、工具导出结果和本仓库说明为准。
- 较大的图片、动画、PPT、DOCX 或数据文件若未随仓库提供，应根据文档中的目录结构和数据采集流程重新生成。
- 数据集目录格式、依赖安装命令和关键参数以各模块 `ENVIRONMENT.md` 中的说明为准。

