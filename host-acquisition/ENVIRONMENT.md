# Environment Setup: Dataset Acquisition Host

[Chinese version](ENVIRONMENT_CH.md)

This directory contains the Python host program for dataset acquisition:

```text
host_acquisition_gui.py
```

The program communicates with the STM32 embedded device through a serial port. It is used to display the acquired waveform in real time and save ST HSDatalog-style data files.

## 1. Recommended Environment

- Operating system: Windows 10/11
- Python: 3.13 was used in this work; Python 3.10 or later is recommended
- STM32 firmware: firmware supporting UART data-acquisition handshaking must be flashed
- Serial driver: install the corresponding driver according to the USB-to-serial chip

This program does not depend on official ST Python packages. It can be run directly in this repository and does not need to be placed under STM32CubeU5 or STDATALOG-PYSDK.

## 2. Python Dependencies

The script mainly depends on:

- `pyserial`: serial communication
- `matplotlib`: waveform display
- `tkinter`: Python standard-library component, usually installed together with Python

Install the dependencies:

```powershell
python -m pip install pyserial matplotlib
```

Check whether the dependencies are available:

```powershell
python -c "import serial, matplotlib, tkinter; print('host-acquisition environment OK')"
```

## 3. Firmware Configuration

Before acquiring data, switch the STM32 firmware to data acquisition mode.

Configure the following file:

```text
../stm32-firmware/Tx_LowPower_echo/Inc/power_test_cfg.h
```

Set the run mode to:

```c
#define APP_RUN_MODE APP_RUN_MODE_DATA_CAPTURE
```

Then rebuild and flash the firmware.

The firmware project itself must be placed under the specified directory in the official ST STM32CubeU5 package. See:

```text
../stm32-firmware/Tx_LowPower_echo/ENVIRONMENT.md
```

## 4. How to Run

Use PyCharm 2025.2.2 to run the following script:

```text
<repo>\host-acquisition\host_acquisition_gui.py
```

Open the `host-acquisition` directory or the entire repository in PyCharm, confirm that the project interpreter is a Python environment with the required dependencies installed, and then click the run button for `host_acquisition_gui.py` to start the host program.

After the program starts, set or confirm the following items in the interface:

- `Serial Port`: select the virtual serial port corresponding to the STM32 device.
- `Baud Rate`: keep it consistent with the firmware UART configuration.
- `Save root`: select the root directory for saving data.
- `Capture time (s)`: set the acquisition duration.

Before acquisition starts, the program performs a `PING` / `READY` / `START` handshake with the firmware.

## 5. Serial Communication Convention

The host program and the embedded device use simple text handshake commands:

- `PING`
- `START`
- `STOP`
- `READY`
- `OK_START`
- `OK_STOP`
- `ERR_START`

If acquisition cannot start, check the following items first:

- Whether the firmware is in `APP_RUN_MODE_DATA_CAPTURE`.
- Whether the selected serial port is correct.
- Whether the baud rate is consistent with the firmware side.
- Whether the serial port is occupied by STM32CubeIDE, a serial-port assistant, or another program.

## 6. Output Data Format

The data saved by the host program is used as the input for the subsequent wake-word and command-word processing workflows. It usually includes:

- `.dat` raw sample data
- `acquisition_info.json`
- `device_config.json`

Subsequent processing entry points:

- Wake word: `../wake-word/nanoedge-converter/dat_to_nanoedge_csv.py`
- Command word: `../command-word/preprocessing/convert_stdatalog_to_wav.ps1`

## 7. Troubleshooting

- If no serial port is visible, first check the USB cable, driver, and Windows Device Manager.
- If the program can connect but no data is received, check whether the embedded device has entered acquisition mode.
- If the saved `.dat` file cannot be parsed in later steps, confirm that the sampling rate, sensor name, and firmware transmission format have not changed.
