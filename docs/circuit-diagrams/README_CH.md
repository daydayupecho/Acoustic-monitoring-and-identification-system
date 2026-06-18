# 电路图说明

[English version](README.md)

本目录保存声学监测与识别系统中硬件电路板的原理图 PDF 文件，用于帮助读者查看系统的板级实现和信号连接关系。

## 目录结构

```text
circuit-diagrams/
├── core-board/
│   └── core-board-schematic.pdf
├── extension-board/
│   └── extension-board-schematic.pdf
└── bluetooth-receiver-relay-board/
    └── bluetooth-receiver-relay-board-schematic.pdf
```

## 文件说明

| 目录 | 文件 | 说明 |
|---|---|---|
| `core-board/` | `core-board-schematic.pdf` | 核心板原理图。核心板主要包括 MCU、电源转换电路和晶振电路，其中超低功耗 MCU（STM32U575VGT6）用于系统逻辑控制。 |
| `extension-board/` | `extension-board-schematic.pdf` | 扩展板原理图。扩展板主要包括自唤醒模块、传感信号调理模块和蓝牙控制模块，用于完成声学信号接入、前端调理以及识别结果的蓝牙发送。 |
| `bluetooth-receiver-relay-board/` | `bluetooth-receiver-relay-board-schematic.pdf` | 蓝牙接收端与继电器控制板原理图。该电路板通过蓝牙接收发送端的控制命令，解析命令后驱动继电器相应通道，用于家电控制，同时通过 UART 将解析后的命令或继电器状态发送给上位机，用于同步显示识别结果和控制状态。 |

## 注意事项

- 蓝牙接收端与继电器控制板对应支撑信息中蓝牙接收端和继电器控制模块的家电控制实验设置。
- PC 端 MATLAB App 只是可选的 UART 状态监视界面，用于显示继电器或家电状态，不执行语音识别，也不参与继电器控制决策。


## 相关说明文档

- 根目录系统流程说明：`README.md`
- 根目录中文说明：`README_CH.md`
- 蓝牙接收端上位机说明：`bluetooth-receiver-host/ENVIRONMENT.md`
- STM32 固件说明：`stm32-firmware/ENVIRONMENT.md`
