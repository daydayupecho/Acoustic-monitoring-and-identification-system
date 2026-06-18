# 环境配置说明：STM32 固件工程

本目录为 STM32U575 平台固件工程，包含数据采集、唤醒词筛选、命令词识别、蓝牙指令输出和低功耗控制逻辑。

## 1. 推荐软件环境

- STM32CubeIDE：建议使用较新的 1.x 版本
- STM32CubeMX：用于打开 `Tx_LowPower.ioc`
- STM32Cube.AI / X-CUBE-AI：用于从 `model.tflite` 生成 C 推理代码
- NanoEdge AI Studio：用于生成唤醒词分类静态库
- ARM GCC 工具链：随 STM32CubeIDE 安装
- ST-LINK 驱动：用于下载和调试

ST 官方 NanoEdge AI Studio wiki：

```text
https://wiki.st.com/stm32mcu/wiki/AI%3ANanoEdge_AI_Studio
```

ST 官方 STM32CubeU5 仓库：

```text
https://github.com/STMicroelectronics/STM32CubeU5
```

官方克隆命令：

```powershell
git clone --recursive https://github.com/STMicroelectronics/STM32CubeU5.git
```

也可以从 ST 官网下载 STM32CubeU5 固件包，但需要保持官方目录结构。

## 2. 硬件目标

当前工程面向：

```text
STM32U575VGT6
```

工程中包含以下工具链目录：

- `STM32CubeIDE/`
- `MDK-ARM/`
- `EWARM/`

当前建议优先使用 STM32CubeIDE。

## 3. 必须放置的官方目录

重要：`Tx_LowPower_echo` 不是一个完全脱离 STM32CubeU5 的独立工程。为了让 STM32CubeIDE 工程、ThreadX 中间件、HAL 驱动和相对路径都能正确解析，复现者需要先下载 ST 官方 STM32CubeU5，然后将本目录放到官方包的以下位置：

```text
<STM32CubeU5_ROOT>\Projects\NUCLEO-U575ZI-Q\Applications\ThreadX\Tx_LowPower_echo
```

其中 `<STM32CubeU5_ROOT>` 是 ST 官方 STM32CubeU5 根目录，例如：

```text
D:\ST\STM32CubeU5
```

最终目录应类似：

```text
D:\ST\STM32CubeU5\Projects\NUCLEO-U575ZI-Q\Applications\ThreadX\Tx_LowPower_echo
```

推荐复现步骤：

1. 克隆或下载 ST 官方 STM32CubeU5。
2. 进入：

```text
<STM32CubeU5_ROOT>\Projects\NUCLEO-U575ZI-Q\Applications\ThreadX
```

3. 将本仓库中的 `stm32-firmware\Tx_LowPower_echo` 整个文件夹复制到该目录下。
4. 使用 STM32CubeIDE 导入：

```text
<STM32CubeU5_ROOT>\Projects\NUCLEO-U575ZI-Q\Applications\ThreadX\Tx_LowPower_echo\STM32CubeIDE
```

5. 执行 Clean Project 和 Build Project。

如果不放在该官方目录下，工程可能因为相对路径找不到 HAL、CMSIS、ThreadX、启动文件或链接脚本，导致无法编译。

## 4. 运行模式配置

主要配置文件：

```text
Inc/power_test_cfg.h
```

数据采集模式：

```c
#define APP_RUN_MODE APP_RUN_MODE_DATA_CAPTURE
```

识别部署模式：

```c
#define APP_RUN_MODE APP_RUN_MODE_RECOGNITION
```

功耗/功能配置：

- `APP_PWR_MODE_A_SAMPLING_ONLY`：只采样
- `APP_PWR_MODE_B_NEAI_ONLY`：只运行 NanoEdge AI 唤醒词筛选
- `APP_PWR_MODE_C_NEAI_USC_SILENT`：运行 NanoEdge AI + 命令词识别，关闭识别打印
- `APP_PWR_MODE_D_FULL`：完整逻辑与调试打印

## 5. NanoEdge AI 静态库环境

根据 ST 官方 wiki，NanoEdge AI Studio 部署包通常包含静态库、头文件、元数据和 emulator。本工程使用：

```text
Inc/NanoEdgeAI.h
Middlewares/ST/STM32_AI_Library/Lib/libneai.a
```

替换唤醒词模型时：

1. 从 NanoEdge AI Studio 导出静态库。
2. 替换 `Inc/NanoEdgeAI.h`。
3. 替换 `Middlewares/ST/STM32_AI_Library/Lib/libneai.a`。
4. 检查 `NEAI_INPUT_SIGNAL_LENGTH`、`NEAI_INPUT_AXIS_NUMBER`、`NEAI_NUMBER_OF_CLASSES`。
5. 确认 `Inc/audio_config.h` 中 `AUDIO_NEAI_FRAME_SAMPLES` 与 `NEAI_INPUT_SIGNAL_LENGTH` 一致。
6. 确认工程链接 `-lneai`，且库文件名保持为 `libneai.a`。

固件调用 NanoEdge AI 的位置：

```text
Src/app_threadx.c
```

主要 API：

```c
neai_classification_init()
neai_classification(...)
neai_get_class_name(...)
```

## 6. STM32Cube.AI 命令词模型环境

命令词模型由 TensorFlow/Keras 训练后导出为：

```text
model.tflite
```

在 STM32CubeMX / STM32Cube.AI 中导入 `model.tflite`，网络名称建议保持：

```text
usc_network
```

保持该名称可以直接匹配当前固件中的调用接口：

```c
ai_usc_network_create_and_init(...)
ai_usc_network_inputs_get(...)
ai_usc_network_outputs_get(...)
ai_usc_network_run(...)
```

生成或替换的文件：

```text
Inc/usc_network.h
Inc/usc_network_config.h
Inc/usc_network_data.h
Inc/usc_network_data_params.h
Src/usc_network.c
Src/usc_network_data.c
Src/usc_network_data_params.c
```

STM32 AI 运行时库：

```text
Middlewares/ST/STM32_AI_Library/Lib/NetworkRuntime800_CM33_GCC.a
```

官方 STM32CubeMX 和 X-CUBE-AI 下载页：

```text
https://www.st.com/en/development-tools/stm32cubemx.html
https://www.st.com/en/embedded-software/x-cube-ai.html
```

## 7. 特征提取一致性检查

命令词模型前端参数需要与训练脚本一致。

检查文件：

```text
Inc/audio_config.h
Src/usc_preproc.c
```

重点参数：

- `AUDIO_SAMPLE_RATE_HZ`
- `AUDIO_USC_FFT_LEN`
- `AUDIO_USC_FRAME_LEN`
- `AUDIO_USC_NUM_TIME_STEPS`
- `AUDIO_USC_NUM_MFCC`
- `AUDIO_USC_STRIDE_COLS`
- `AUDIO_USC_FEATURE_COUNT`

当前固件通过 `usc_network.h` 自动推导部分输入尺寸，因此替换模型后要重新检查 `AI_USC_NETWORK_IN_1_*` 宏。

## 8. STM32CubeIDE 编译检查

导入模型后，建议执行：

1. Clean Project。
2. Build Project。
3. 检查是否有缺失头文件。
4. 检查是否有未链接符号。
5. 检查 Flash/RAM 占用是否超出 STM32U575VGT6 限制。

如果出现链接错误，重点检查：

- `libneai.a` 是否在库搜索路径中。
- `NetworkRuntime800_CM33_GCC.a` 是否存在。
- STM32CubeIDE 中是否包含 `usc_network*.c`。
- 网络名称是否仍为 `usc_network`。

## 9. 复现检查清单

1. 下载或克隆 ST 官方 STM32CubeU5。
2. 将 `Tx_LowPower_echo` 放入官方目录：

```text
<STM32CubeU5_ROOT>\Projects\NUCLEO-U575ZI-Q\Applications\ThreadX\
```

3. 确认最终路径为：

```text
<STM32CubeU5_ROOT>\Projects\NUCLEO-U575ZI-Q\Applications\ThreadX\Tx_LowPower_echo
```

4. 用 STM32CubeIDE 导入 `Tx_LowPower_echo\STM32CubeIDE`。
5. 检查 `Inc/power_test_cfg.h` 中的 `APP_RUN_MODE`。
6. 如果复现数据采集，设置为 `APP_RUN_MODE_DATA_CAPTURE`。
7. 如果复现识别部署，设置为 `APP_RUN_MODE_RECOGNITION`。
8. 如果替换唤醒词模型，更新 `NanoEdgeAI.h` 和 `libneai.a`。
9. 如果替换命令词模型，更新 `usc_network*` 文件。
10. Clean、Build、烧录到 STM32U575 设备。

## 10. 下载和验证

下载固件后建议按顺序验证：

1. 数据采集模式下，上位机能否收到 UART 数据。
2. 识别模式下，NanoEdge AI 初始化是否成功。
3. 唤醒词是否能触发命令词识别阶段。
4. 10 个有效命令词和 `background` 类是否分类正确。
5. 蓝牙 AT 指令是否与识别类别一致。

## 11. 参考

- ST NanoEdge AI Studio wiki: `https://wiki.st.com/stm32mcu/wiki/AI%3ANanoEdge_AI_Studio`
- STM32CubeU5 official repository: `https://github.com/STMicroelectronics/STM32CubeU5`
- STM32Cube.AI / X-CUBE-AI 文档请以当前安装的 STM32CubeMX 版本为准。
