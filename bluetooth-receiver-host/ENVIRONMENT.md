# Environment Setup: Bluetooth Receiver UART Host

[Chinese version](ENVIRONMENT_CH.md)

This directory contains the MATLAB host application, communication documentation, and interface assets used to display the parsed results from the Bluetooth receiver. The MATLAB App does not directly receive data from the Bluetooth transmitter. Instead, it receives the parsed results output by the Bluetooth receiver through the PC serial port.

Main files:

```text
xiaobiao.mlapp
白色背景.mlapp
蓝牙通信说明新.docx
data.txt
app1.prj
```

## 1. Recommended Environment

- Operating system: Windows 10/11
- MATLAB: a version that supports App Designer is recommended

## 2. MATLAB Toolboxes

The `.mlapp` files were created with MATLAB App Designer.

It is recommended to confirm that the MATLAB installation includes:

- App Designer
- Instrument Control Toolbox or serial-communication-related support packages

The exact toolbox requirements depend on the serial communication implementation inside the `.mlapp` files.

## 3. How to Open

Open the application in MATLAB:

```matlab
appdesigner('xiaobiao.mlapp')
```

or:

```matlab
appdesigner('白色背景.mlapp')
```

The `.mlapp` file can also be opened by double-clicking it in the MATLAB file browser.

## 4. Communication Configuration

The actual communication chain of this module is:

```text
STM32 recognition node -> Bluetooth transmitter module -> Bluetooth receiver -> UART/USB-to-serial adapter -> MATLAB App
```

The MATLAB App only opens the PC-side serial port, reads the strings that have already been parsed and output by the Bluetooth receiver, and updates the interface status.

Refer to the communication protocol document:

```text
蓝牙通信说明新.docx
```

Example test data:

```text
data.txt
```

During joint debugging, confirm the following items:

- The Bluetooth transmitter and Bluetooth receiver are powered on.
- The Bluetooth receiver has been paired or connected in transparent-transmission mode with the transmitter.
- The UART/USB-to-serial connection from the Bluetooth receiver to the PC is working, and the corresponding COM port appears in Windows Device Manager.
- The serial port number, baud rate, and terminator configured in the MATLAB App are consistent with the UART output of the receiver.
- The parsed result or status string output by the Bluetooth receiver is consistent with the parsing logic in the MATLAB App.

## 5. Interface Assets

The `.png`, `.jpg`, and `.gif` files in this directory are interface display assets used by the MATLAB App. When moving the `.mlapp` files or reorganizing the directory, keep the asset paths available; otherwise, the interface may fail to display the images correctly.

## 6. Relationship with the Firmware

After a command word is successfully recognized by the firmware, the firmware sends a control command through the Bluetooth transmitter module. The Bluetooth receiver receives and parses the command, and then outputs it through UART to the PC running the MATLAB App.

The mapping from command classes to AT commands in the firmware is located at:

```text
../stm32-firmware/Tx_LowPower_echo/Src/app_threadx.c
```

If the command-word class order, appliance types, or AT commands are modified, update the following items consistently:

- The label order used for command-word model training.
- The class names and AT-command mapping in `app_threadx.c`.
- The string format output by the Bluetooth receiver through UART.
- The serial receiving, parsing, and interface-status update logic in the MATLAB App.

## 7. Troubleshooting

- If the MATLAB App cannot read data, first use a serial-port assistant to open the PC-side COM port and confirm that the Bluetooth receiver has output parsed results through UART.
- If the interface status does not update, check whether the received string format matches the parsing logic.
- If images are missing, check whether the asset files are still located in the paths expected by the `.mlapp` files.
