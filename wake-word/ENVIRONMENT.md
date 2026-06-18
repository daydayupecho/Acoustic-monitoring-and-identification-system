# Environment Setup: Wake-Word Module

[Chinese version](ENVIRONMENT_CH.md)

This directory is used for wake-word data processing and NanoEdge AI Studio model development.

Current subdirectory:

```text
nanoedge-converter/
```

For detailed environment configuration, see:

```text
nanoedge-converter/ENVIRONMENT.md
```

## Recommended Tools

- Python 3.10 or later
- NanoEdge AI Studio, version v5.1.1 is recommended
- STM32CubeIDE, used for final firmware integration and flashing

## Development Workflow Overview

1. Use the host program in `host-acquisition/` to acquire raw wake-word and negative-sample data.
2. Use `nanoedge-converter/dat_to_nanoedge_csv.py` to convert `.dat` files into CSV files.
3. Create a classification project in NanoEdge AI Studio and import the CSV data.
4. Run Benchmark and select a library with a balance between performance and resource usage.
5. Export `NanoEdgeAI.h` and `libneai.a`.
6. Copy the exported files into the STM32 firmware project:

```text
../stm32-firmware/Tx_LowPower_echo/Inc/NanoEdgeAI.h
../stm32-firmware/Tx_LowPower_echo/Middlewares/ST/STM32_AI_Library/Lib/libneai.a
```

## Reference

ST NanoEdge AI Studio wiki:

```text
https://wiki.st.com/stm32mcu/wiki/AI%3ANanoEdge_AI_Studio
```
