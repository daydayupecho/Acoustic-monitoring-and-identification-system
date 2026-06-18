# 环境配置说明：蓝牙接收端 UART 上位机

本目录包含用于显示蓝牙接收端解析结果的 MATLAB 上位机程序、通信说明文档和界面素材。MATLAB App 不直接接收蓝牙空口数据，而是通过 PC 串口 / UART 接收蓝牙接收端输出的解析结果或状态字符串。

主要文件：

```text
xiaobiao.mlapp
白色背景.mlapp
蓝牙通信说明新.docx
data.txt
app1.prj
```

## 1. 推荐环境

- 操作系统：Windows 10/11
- MATLAB：建议使用支持 App Designer 的版本


## 2. MATLAB 工具箱

`.mlapp` 文件由 MATLAB App Designer 创建。

建议确认 MATLAB 是否包含：

- App Designer
- Instrument Control Toolbox 或串口通信相关支持包

具体需要哪些工具箱，以 `.mlapp` 内部通信实现为准。

## 3. 打开方式

在 MATLAB 中打开：

```matlab
appdesigner('xiaobiao.mlapp')
```

或：

```matlab
appdesigner('白色背景.mlapp')
```

也可以在 MATLAB 文件浏览器中双击 `.mlapp` 文件。

## 4. 通信配置

本模块的实际通信链路为：

```text
STM32 识别端 -> 蓝牙发送模块 -> 蓝牙接收端 -> UART/USB 转串口 -> MATLAB App
```

其中 MATLAB App 只负责打开 PC 侧串口，读取蓝牙接收端已经解析并输出的字符串，然后更新界面状态。

通信协议请参考：

```text
蓝牙通信说明新.docx
```

测试数据示例：

```text
data.txt
```

联调时需要确认：

- 蓝牙发送端和蓝牙接收端是否上电。
- 蓝牙接收端是否与发送端完成配对或透传连接。
- 蓝牙接收端到 PC 的 UART/USB 转串口是否正常，设备管理器中是否出现对应 COM 口。
- MATLAB App 中的串口号、波特率和终止符是否与接收端 UART 输出一致。
- 蓝牙接收端输出的解析结果或状态字符串是否与 MATLAB App 中的解析逻辑一致。

## 5. 界面素材

目录中的 `.png`、`.jpg`、`.gif` 文件为 MATLAB App 界面显示素材。移动 `.mlapp` 文件或重新组织目录时，需要保持素材路径可用，否则界面可能无法正常显示图片。

## 6. 与固件端的对应关系

固件端命令词识别成功后，会通过蓝牙相关串口向蓝牙发送模块发送控制指令。蓝牙接收端接收并解析后，再通过 UART 输出给运行 MATLAB App 的 PC。

固件中命令类别到 AT 指令的映射位置：

```text
../stm32-firmware/Tx_LowPower_echo/Src/app_threadx.c
```

如果修改命令词类别顺序、设备种类或 AT 指令，需要同步修改：

- 命令词模型训练标签顺序。
- `app_threadx.c` 中的类别名称和 AT 指令映射。
- 蓝牙接收端输出到 UART 的字符串格式。
- MATLAB App 中的串口接收解析和界面状态更新逻辑。

## 7. 常见问题

- 如果 MATLAB App 无法读取数据，先用串口助手打开 PC 侧 COM 口，确认蓝牙接收端是否已经通过 UART 输出了解析结果。
- 如果界面状态不更新，检查接收到的字符串格式是否与解析逻辑一致。
- 如果图片缺失，检查素材文件是否仍在 `.mlapp` 预期路径下。
