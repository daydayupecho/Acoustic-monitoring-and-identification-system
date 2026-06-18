# 环境配置说明：唤醒词 NanoEdge AI 数据转换

本目录用于将上位机保存的 `.dat` 采集数据转换为 NanoEdge AI Studio 可导入的 CSV 文件。

主要程序：

```text
dat_to_nanoedge_csv.py
run_nanoedge_conversion.bat
```

## 1. 推荐环境

- 操作系统：Windows 10/11
- Python：3.10 及以上
- NanoEdge AI Studio：建议使用 v5.1.1 
- 固件侧采样率：16 kHz

ST 官方 NanoEdge AI Studio wiki：

```text
https://wiki.st.com/stm32mcu/wiki/AI%3ANanoEdge_AI_Studio
```

NanoEdge AI Studio 下载入口可从 ST Edge AI / NanoEdge AI Studio 页面进入：

```text
https://stm32ai.st.com/nanoedge-ai-studio/
```

## 2. Python 依赖

`dat_to_nanoedge_csv.py` 只使用 Python 标准库：

- `argparse`
- `csv`
- `json`
- `struct`
- `pathlib`

一般不需要额外安装第三方 Python 包。

检查命令：

```powershell
python dat_to_nanoedge_csv.py --help
```

该脚本可以直接在本目录中运行，不需要放入 ST 官方工程目录。

## 3. NanoEdge AI Studio 准备

根据 ST 官方说明，NanoEdge AI Studio 用于从传感器数据中搜索预处理、特征提取和机器学习库组合，并导出可在 MCU 上调用的静态库。

使用前需要准备：

- 安装 NanoEdge AI Studio。
- 登录或配置可用许可证。
- 明确目标 MCU 和资源限制。
- 准备每个类别对应的 CSV 数据。

本项目唤醒词任务为二分类：

- 正样本：`echo`
- 负样本：`other` / `unknown` / `background` 等非唤醒词声音

## 4. 数据转换参数

典型命令：

```powershell
python dat_to_nanoedge_csv.py <采集数据根目录> -o <输出目录> -s imp23absu_mic -sl 8192 -si 8192 --odr 16000 --csv-shape row
```

参数说明：

- `<采集数据根目录>`：上位机保存的采集数据目录。
- `-o`：CSV 输出目录。
- `-s`：传感器名称，当前常用 `imp23absu_mic`, 这个是STEVAL-STWINBX1开发板使用的麦克风，本工作使用的是S-TAU
- `-sl`：窗口长度。
- `-si`：窗口步长。
- `--odr`：采样率，当前为 `16000`。
- `--csv-shape`：CSV 排列方式，当前常用 `row`。

如果使用批处理脚本：

```text
run_nanoedge_conversion.bat
```

运行前需要设置以下变量：

```bat
set "ROOT=<唤醒词原始采集数据根目录>"
set "OUT_ALL=<NanoEdge CSV 输出目录>"
set "PYTHON_EXE=python"
set "CLI=%~dp0dat_to_nanoedge_csv.py"
set "SENSOR=imp23absu_mic"
set "SL=8192"
set "SI=8192"
set "ODR=16000"
set "CSV_SHAPE=row"
```

参数含义：

- `ROOT`：上位机保存的采集数据根目录，目录下应包含多个采集文件夹。
- `OUT_ALL`：转换后的 CSV 输出目录。
- `PYTHON_EXE`：Python 解释器命令或绝对路径。
- `CLI`：转换脚本路径。若批处理文件中仍是旧文件名，需要改为 `dat_to_nanoedge_csv.py`。
- `SENSOR`：JSON 配置中的传感器名称，当前为 `imp23absu_mic`。
- `SL`：每个 NanoEdge 样本窗口长度。
- `SI`：窗口步长。若希望不重叠，设置为与 `SL` 相同。
- `ODR`：采样率，当前实验为 16 kHz。
- `CSV_SHAPE`：NanoEdge AI Studio 导入方式，当前建议使用 `row`。

使用时，最常需要设置的是 `ROOT`、`OUT_ALL`、`SL`、`SI`。其中 `SL` 必须与最终 NanoEdge AI Studio 导出库中的 `NEAI_INPUT_SIGNAL_LENGTH` 保持一致。

## 5. 与 NanoEdge AI Studio 的对应关系

CSV 窗口长度必须与 NanoEdge AI Studio 训练和最终导出的库保持一致。

导出库后，需要检查：

```text
NanoEdgeAI.h
```

其中关键宏包括：

```c
#define NEAI_INPUT_SIGNAL_LENGTH ...
#define NEAI_INPUT_AXIS_NUMBER ...
#define NEAI_NUMBER_OF_CLASSES ...
```

当前固件中 `Inc/audio_config.h` 使用 `NEAI_INPUT_SIGNAL_LENGTH` 作为唤醒词输入窗口长度，因此重新训练模型时，应让以下三处保持一致：

- CSV 转换时的 `-sl`
- NanoEdge AI Studio 项目的输入信号长度
- 固件中导出的 `NanoEdgeAI.h`

## 6. NanoEdge AI Studio 输出文件

根据 ST 官方 wiki，编译/部署后得到的库包通常包含：

- `libneai.a`：NanoEdge AI 静态库
- `NanoEdgeAI.h`：API 和模型配置头文件
- `knowledge.h`：知识文件，具体是否出现取决于项目类型
- `metadata.json`：库元数据
- emulator：用于在 PC 上验证库表现

本项目导入固件时主要使用：

```text
NanoEdgeAI.h
libneai.a
```

复制目标：

```text
../../stm32-firmware/Tx_LowPower_echo/Inc/NanoEdgeAI.h
../../stm32-firmware/Tx_LowPower_echo/Middlewares/ST/STM32_AI_Library/Lib/libneai.a
```

## 7. 推荐验证

1. 使用未参与训练的数据在 NanoEdge AI Studio 或 emulator 中验证分类效果。
2. 检查 `NEAI_INPUT_SIGNAL_LENGTH` 和固件窗口长度。
3. 将库导入固件后，确认 `neai_classification_init()` 返回 `NEAI_OK`。
4. 在实际设备上测试唤醒词和负样本，观察是否能稳定输出分类结果。

## 8. 常见问题

- 如果 NanoEdge AI Studio Benchmark 结果不好，优先检查数据是否按类别分开、窗口长度是否合理、采样率是否一致。
- 可以通过滑窗技术来提高识别准确率。
- 对 MCU 端误触发，可以在固件层增加连续多帧确认逻辑；当前固件已通过 `APP_NEAI_WAKE_HITS` 等参数实现类似确认。

## 9. 检查清单

1. 安装 NanoEdge AI Studio，并确认许可证可用。
2. 使用本脚本将唤醒词正样本和负样本分别转换为 CSV。
3. 在 NanoEdge AI Studio 中创建 Classification 项目。
4. 导入每个类别对应的 CSV。
5. 设置目标 MCU 和资源限制。
6. 运行 Benchmark。
7. 使用未参与 Benchmark 的数据做 Validation 或 emulator 测试。
8. 导出静态库。
9. 将 `NanoEdgeAI.h` 和 `libneai.a` 导入 STM32 固件工程。
10. 重新编译并在 MCU 上验证实时唤醒词筛选。
