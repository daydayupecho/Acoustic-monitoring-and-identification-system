# Environment Setup: STM32 Firmware Project

[Chinese version](ENVIRONMENT_CH.md)

This directory contains the STM32U575 firmware project, including data acquisition, wake-word screening, command-word recognition, Bluetooth command output, and low-power control logic.

## 1. Recommended Software Environment

- STM32CubeIDE: version 1.18.1 is recommended
- STM32CubeMX: used to open `Tx_LowPower.ioc`
- STM32Cube.AI / X-CUBE-AI: used to generate C inference code from `model.tflite`
- NanoEdge AI Studio: used to generate the wake-word classification static library
- ARM GCC toolchain: installed together with STM32CubeIDE
- ST-LINK driver: used for flashing and debugging

ST official NanoEdge AI Studio wiki:

```text
https://wiki.st.com/stm32mcu/wiki/AI%3ANanoEdge_AI_Studio
```

ST official STM32CubeU5 repository:

```text
https://github.com/STMicroelectronics/STM32CubeU5
```

Official cloning command:

```powershell
git clone --recursive https://github.com/STMicroelectronics/STM32CubeU5.git
```

The STM32CubeU5 firmware package can also be downloaded from the ST website, but the official directory structure must be preserved.

## 2. Hardware Target

The current project targets:

```text
STM32U575VGT6
```

The project includes the following toolchain directories:

- `STM32CubeIDE/`
- `MDK-ARM/`
- `EWARM/`

STM32CubeIDE is currently recommended as the preferred toolchain.

## 3. Required Official Directory Placement

Important: `Tx_LowPower_echo` is not a fully standalone project separated from STM32CubeU5. To allow the STM32CubeIDE project, ThreadX middleware, HAL drivers, and relative paths to be resolved correctly, readers should first download the official ST STM32CubeU5 package and then place this directory at the following location:

```text
<STM32CubeU5_ROOT>\Projects\NUCLEO-U575ZI-Q\Applications\ThreadX\Tx_LowPower_echo
```

Here, `<STM32CubeU5_ROOT>` is the root directory of the official ST STM32CubeU5 package, for example:

```text
D:\ST\STM32CubeU5
```

The final directory should be similar to:

```text
D:\ST\STM32CubeU5\Projects\NUCLEO-U575ZI-Q\Applications\ThreadX\Tx_LowPower_echo
```

Recommended steps:

1. Clone or download the official ST STM32CubeU5 package.
2. Go to:

```text
<STM32CubeU5_ROOT>\Projects\NUCLEO-U575ZI-Q\Applications\ThreadX
```

3. Copy the entire `stm32-firmware\Tx_LowPower_echo` folder from this repository into that directory.
4. Import the following directory with STM32CubeIDE:

```text
<STM32CubeU5_ROOT>\Projects\NUCLEO-U575ZI-Q\Applications\ThreadX\Tx_LowPower_echo\STM32CubeIDE
```

5. Run Clean Project and Build Project.

If the project is not placed under the official directory, compilation may fail because relative paths cannot locate HAL, CMSIS, ThreadX, startup files, or linker scripts.

## 4. Run Mode Configuration

Main configuration file:

```text
Inc/power_test_cfg.h
```

Data acquisition mode:

```c
#define APP_RUN_MODE APP_RUN_MODE_DATA_CAPTURE
```

Recognition deployment mode:

```c
#define APP_RUN_MODE APP_RUN_MODE_RECOGNITION
```

Power/function configurations:

- `APP_PWR_MODE_A_SAMPLING_ONLY`: sampling only
- `APP_PWR_MODE_B_NEAI_ONLY`: NanoEdge AI wake-word screening only
- `APP_PWR_MODE_C_NEAI_USC_SILENT`: NanoEdge AI + command-word recognition with recognition prints disabled
- `APP_PWR_MODE_D_FULL`: complete logic with debug printing

## 5. NanoEdge AI Static Library Environment

According to the official ST wiki, the NanoEdge AI Studio deployment package usually contains a static library, header file, metadata, and emulator. This project uses:

```text
Inc/NanoEdgeAI.h
Middlewares/ST/STM32_AI_Library/Lib/libneai.a
```

When replacing the wake-word model:

1. Export the static library from NanoEdge AI Studio.
2. Replace `Inc/NanoEdgeAI.h`.
3. Replace `Middlewares/ST/STM32_AI_Library/Lib/libneai.a`.
4. Check `NEAI_INPUT_SIGNAL_LENGTH`, `NEAI_INPUT_AXIS_NUMBER`, and `NEAI_NUMBER_OF_CLASSES`.
5. Confirm that `AUDIO_NEAI_FRAME_SAMPLES` in `Inc/audio_config.h` is consistent with `NEAI_INPUT_SIGNAL_LENGTH`.
6. Confirm that the project links `-lneai`, and that the library filename remains `libneai.a`.

The firmware calls NanoEdge AI in:

```text
Src/app_threadx.c
```

Main APIs:

```c
neai_classification_init()
neai_classification(...)
neai_get_class_name(...)
```

## 6. STM32Cube.AI Command-Word Model Environment

The command-word model is trained with TensorFlow/Keras and exported as:

```text
model.tflite
```

Import `model.tflite` into STM32CubeMX / STM32Cube.AI. The network name is recommended to remain:

```text
usc_network
```

Keeping this name allows the generated code to match the firmware calling interface directly:

```c
ai_usc_network_create_and_init(...)
ai_usc_network_inputs_get(...)
ai_usc_network_outputs_get(...)
ai_usc_network_run(...)
```

Files generated or replaced:

```text
Inc/usc_network.h
Inc/usc_network_config.h
Inc/usc_network_data.h
Inc/usc_network_data_params.h
Src/usc_network.c
Src/usc_network_data.c
Src/usc_network_data_params.c
```

STM32 AI runtime library:

```text
Middlewares/ST/STM32_AI_Library/Lib/NetworkRuntime800_CM33_GCC.a
```

Official STM32CubeMX and X-CUBE-AI download pages:

```text
https://www.st.com/en/development-tools/stm32cubemx.html
https://www.st.com/en/embedded-software/x-cube-ai.html
```

## 7. Feature-Extraction Consistency Check

The command-word model front-end parameters must be consistent with the training scripts.

Files to check:

```text
Inc/audio_config.h
Src/usc_preproc.c
```

Key parameters:

- `AUDIO_SAMPLE_RATE_HZ`
- `AUDIO_USC_FFT_LEN`
- `AUDIO_USC_FRAME_LEN`
- `AUDIO_USC_NUM_TIME_STEPS`
- `AUDIO_USC_NUM_MFCC`
- `AUDIO_USC_STRIDE_COLS`
- `AUDIO_USC_FEATURE_COUNT`

The current firmware automatically derives part of the input size through `usc_network.h`. Therefore, after replacing the model, recheck the `AI_USC_NETWORK_IN_1_*` macros.

## 8. STM32CubeIDE Build Check

After importing a model, it is recommended to:

1. Clean Project.
2. Build Project.
3. Check whether any header files are missing.
4. Check whether there are unresolved linked symbols.
5. Check whether Flash/RAM usage exceeds the limits of STM32U575VGT6.

If link errors occur, focus on:

- Whether `libneai.a` is in the library search path.
- Whether `NetworkRuntime800_CM33_GCC.a` exists.
- Whether `usc_network*.c` is included in STM32CubeIDE.
- Whether the network name is still `usc_network`.

## 9. Checklist

1. Download or clone the official ST STM32CubeU5 package.
2. Place `Tx_LowPower_echo` into the official directory:

```text
<STM32CubeU5_ROOT>\Projects\NUCLEO-U575ZI-Q\Applications\ThreadX\
```

3. Confirm that the final path is:

```text
<STM32CubeU5_ROOT>\Projects\NUCLEO-U575ZI-Q\Applications\ThreadX\Tx_LowPower_echo
```

4. Import `Tx_LowPower_echo\STM32CubeIDE` with STM32CubeIDE.
5. Check `APP_RUN_MODE` in `Inc/power_test_cfg.h`.
6. For data acquisition, set it to `APP_RUN_MODE_DATA_CAPTURE`.
7. For recognition deployment, set it to `APP_RUN_MODE_RECOGNITION`.
8. If replacing the wake-word model, update `NanoEdgeAI.h` and `libneai.a`.
9. If replacing the command-word model, update the `usc_network*` files.
10. Clean, build, and flash the firmware to the STM32U575 device.

## 10. References

- ST NanoEdge AI Studio wiki: `https://wiki.st.com/stm32mcu/wiki/AI%3ANanoEdge_AI_Studio`
- STM32CubeU5 official repository: `https://github.com/STMicroelectronics/STM32CubeU5`
- For STM32Cube.AI / X-CUBE-AI documentation, refer to the version included with the currently installed STM32CubeMX.
