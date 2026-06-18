# 环境配置说明：数据采集上位机

本目录包含 Python 数据采集上位机：

```text
host_acquisition_gui.py
```

该程序通过串口与 STM32 下位机通信，用于实时显示采集波形并保存 ST HSDatalog 风格的数据文件。

## 1. 推荐环境

- 操作系统：Windows 10/11
- Python：3.13（本文实验使用版本）；建议使用 Python 3.10 及以上
- STM32 固件：需要烧录支持 UART 数据采集握手的固件
- 串口驱动：根据 USB 转串口芯片安装对应驱动

本程序不依赖 ST 官方 Python 包，可以直接在本仓库目录中运行，不需要放到 STM32CubeU5 或 STDATALOG-PYSDK 目录下。

## 2. Python 依赖

脚本主要依赖：

- `pyserial`：串口通信
- `matplotlib`：波形显示
- `tkinter`：Python 标准库组件，通常随 Python 一起安装

安装命令：

```powershell
python -m pip install pyserial matplotlib
```

检查依赖是否可用：

```powershell
python -c "import serial, matplotlib, tkinter; print('host-acquisition environment OK')"
```

## 3. 固件端配置

采集数据前，需要将 STM32 固件切换到数据采集模式。

在以下文件中配置：

```text
../stm32-firmware/Tx_LowPower_echo/Inc/power_test_cfg.h
```

将运行模式设置为：

```c
#define APP_RUN_MODE APP_RUN_MODE_DATA_CAPTURE
```

然后重新编译并烧录固件。

固件工程本身需要放在 ST 官方 STM32CubeU5 包的指定目录下，具体见：

```text
../stm32-firmware/Tx_LowPower_echo/ENVIRONMENT.md
```

## 4. 运行方式

使用 PyCharm 2025.2.2 运行以下代码：

```text
<repo>\host-acquisition\host_acquisition_gui.py
```

在 PyCharm 中打开 `host-acquisition` 目录或整个仓库目录，确认项目解释器为已安装依赖的 Python 环境，然后点击 `host_acquisition_gui.py` 的运行按钮即可启动上位机程序。

运行后需要在界面中设置或确认：

- `Serial Port`：选择 STM32 设备对应的虚拟串口。
- `Baud Rate`：与固件 UART 配置保持一致。
- `Save root`：选择数据保存根目录。
- `Capture time (s)`：设置采集时长。

采集开始前，程序会与固件进行 `PING` / `READY` / `START` 握手。

## 5. 串口通信约定

上位机与下位机使用简单的文本握手命令：

- `PING`
- `START`
- `STOP`
- `READY`
- `OK_START`
- `OK_STOP`
- `ERR_START`

如果无法开始采集，优先检查：

- 固件是否处于 `APP_RUN_MODE_DATA_CAPTURE`。
- 串口号是否正确。
- 波特率是否与固件端一致。
- 串口是否被 STM32CubeIDE、串口助手或其他程序占用。

## 6. 输出数据格式

上位机保存的数据会作为后续唤醒词和命令词处理流程的输入，通常包含：

- `.dat` 原始采样数据
- `acquisition_info.json`
- `device_config.json`

后续处理入口：

- 唤醒词：`../wake-word/nanoedge-converter/dat_to_nanoedge_csv.py`
- 命令词：`../command-word/preprocessing/convert_stdatalog_to_wav.ps1`

## 7. 常见问题

- 如果看不到串口，先检查 USB 线、驱动和设备管理器。
- 如果能连接但没有数据，检查下位机是否已经进入采集模式。
- 如果保存后的 `.dat` 后续无法解析，确认采样率、传感器名称和固件传输格式没有变化。
