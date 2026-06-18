# Circuit Diagrams

[Chinese version](README_CH.md)

This directory contains schematic PDFs for the hardware circuit boards used in the acoustic monitoring and identification system. The files help readers check the board-level implementation and signal connections of the system.

## Directory Structure

```text
circuit-diagrams/
├── core-board/
│   └── core-board-schematic.pdf
├── extension-board/
│   └── extension-board-schematic.pdf
└── bluetooth-receiver-relay-board/
    └── bluetooth-receiver-relay-board-schematic.pdf
```

## File Description

| Directory | File | Description |
|---|---|---|
| `core-board/` | `core-board-schematic.pdf` | Schematic of the core board. The core board mainly includes the MCU, power-conversion circuit, and crystal oscillator circuit. An ultra-low-power MCU (STM32U575VGT6) is used for system logic control. |
| `extension-board/` | `extension-board-schematic.pdf` | Schematic of the expansion board. The expansion board mainly includes a self-wake-up module, a sensing signal-conditioning module, and a Bluetooth control module for acoustic-signal input, front-end conditioning, and Bluetooth transmission of recognition results. |
| `bluetooth-receiver-relay-board/` | `bluetooth-receiver-relay-board-schematic.pdf` | Schematic of the Bluetooth receiver and relay-control board. The board receives control commands from the transmitter through Bluetooth, parses the commands, and drives the corresponding relay channels for appliance control. It also sends the parsed command or relay state to the host computer through UART for synchronized display of recognition results and control status. |

## Notes

- The Bluetooth receiver and relay-control board corresponds to the appliance-control setup described in the Supporting Information figure 36 for the Bluetooth receiver and relay control module.
- The PC-side MATLAB App is only an optional UART monitoring interface for displaying relay or appliance states. It does not perform speech recognition or relay-control decision making.

## Related Documentation

- Root workflow description: `README.md`
- Chinese root description: `README_CH.md`
- Bluetooth receiver host notes: `bluetooth-receiver-host/ENVIRONMENT.md`
- STM32 firmware notes: `stm32-firmware/ENVIRONMENT.md`
