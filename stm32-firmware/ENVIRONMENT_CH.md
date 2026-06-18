# 环境配置说明：STM32 固件模块

本目录用于存放 STM32 嵌入式端工程。

当前工程目录：

```text
Tx_LowPower_echo/
```

详细环境配置见：

```text
Tx_LowPower_echo/ENVIRONMENT.md
```

## 推荐工具

- STM32CubeIDE
- STM32CubeMX
- STM32Cube.AI / X-CUBE-AI
- NanoEdge AI Studio
- ST-LINK 驱动

官方来源：

```text
STM32CubeU5:
https://github.com/STMicroelectronics/STM32CubeU5

STM32CubeMX:
https://www.st.com/en/development-tools/stm32cubemx.html

X-CUBE-AI:
https://www.st.com/en/embedded-software/x-cube-ai.html

NanoEdge AI Studio wiki:
https://wiki.st.com/stm32mcu/wiki/AI%3ANanoEdge_AI_Studio
```

## 必须放置到官方目录

`Tx_LowPower_echo` 需要放到 ST 官方 STM32CubeU5 包的 NUCLEO-U575ZI-Q ThreadX 应用目录下：

```text
<STM32CubeU5_ROOT>\Projects\NUCLEO-U575ZI-Q\Applications\ThreadX\Tx_LowPower_echo
```

官方 STM32CubeU5 克隆命令：

```powershell
git clone --recursive https://github.com/STMicroelectronics/STM32CubeU5.git
```

如果不放在该目录下，STM32CubeIDE 工程可能找不到 HAL、CMSIS、ThreadX 和板级支持文件。

## 固件集成内容

当前固件集成两个模型阶段：

- NanoEdge AI 唤醒词筛选静态库
- STM32Cube.AI 生成的命令词识别 C 代码

模型文件对应关系：

```text
Tx_LowPower_echo/Inc/NanoEdgeAI.h
Tx_LowPower_echo/Middlewares/ST/STM32_AI_Library/Lib/libneai.a
Tx_LowPower_echo/Inc/usc_network*.h
Tx_LowPower_echo/Src/usc_network*.c
```

## 模式切换

主要配置文件：

```text
Tx_LowPower_echo/Inc/power_test_cfg.h
```

数据采集：

```c
#define APP_RUN_MODE APP_RUN_MODE_DATA_CAPTURE
```

模型识别：

```c
#define APP_RUN_MODE APP_RUN_MODE_RECOGNITION
```
