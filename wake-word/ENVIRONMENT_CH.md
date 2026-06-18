# 环境配置说明：唤醒词模块

本目录用于唤醒词数据处理和 NanoEdge AI Studio 模型开发。

当前子目录：

```text
nanoedge-converter/
```

详细环境配置见：

```text
nanoedge-converter/ENVIRONMENT_CH.md
```

## 推荐工具

- Python 3.10 及以上
- NanoEdge AI Studio，建议使用 v5.1.1
- STM32CubeIDE，用于最终固件集成和烧录

## 开发流程概览

1. 使用 `host-acquisition/` 中的上位机采集唤醒词和负样本原始数据。
2. 使用 `nanoedge-converter/dat_to_nanoedge_csv.py` 将 `.dat` 转换为 CSV。
3. 在 NanoEdge AI Studio 中创建分类项目，导入 CSV 数据。
4. 运行 Benchmark 并选择性能和资源占用均衡的库。
5. 导出 `NanoEdgeAI.h` 和 `libneai.a`。
6. 将导出的文件复制到 STM32 固件工程：

```text
../stm32-firmware/Tx_LowPower_echo/Inc/NanoEdgeAI.h
../stm32-firmware/Tx_LowPower_echo/Middlewares/ST/STM32_AI_Library/Lib/libneai.a
```

## 参考

ST NanoEdge AI Studio wiki:

```text
https://wiki.st.com/stm32mcu/wiki/AI%3ANanoEdge_AI_Studio
```
