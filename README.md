# Peak PCAN PCI QNX Resource Manager

QNX resource manager for PEAK-System PCAN-PCI CAN bus interface cards.

## Overview

This project provides a native QNX resource manager implementation for **PEAK PCAN-PCI** devices, enabling real-time CAN communication in embedded systems running **QNX Neutrino RTOS**.

The CAN frame format is defined in [can.h](common/include/can.h), based on the Linux SocketCAN specification (https://github.com/torvalds/linux/blob/master/include/uapi/linux/can.h):

```c
/*
 * Controller Area Network Identifier structure
 *
 * bit 0-28 : CAN identifier (11/29 bit)
 * bit 29   : error message frame flag (0 = data frame, 1 = error message)
 * bit 30   : remote transmission request flag (1 = RTR frame)
 * bit 31   : frame format flag (0 = standard 11 bit, 1 = extended 29 bit)
 */

struct can_frame {
    canid_t can_id;   /* 32-bit CAN_ID + EFF/RTR/ERR flags */
    __u8    len;      /* Data Length Code (0..8) */
    __u8    __pad;    /* padding */
    __u8    __res0;   /* reserved / padding */
    __u8    len8_dlc; /* optional DLC for 8-byte payload length (9..15) */
    __u8    data[CAN_MAX_DLEN] __attribute__((aligned(8)));
};
```

includes **cansend** and **candump** utilities similar to Linux can utils.

## Features

- Resource manager for PEAK PCAN-PCI cards
- Compatible with:
  - QNX 7.0
  - QNX 7.1
- Supports multiple architectures:
  - x86_64
  - ARM
  - ARM_64
- Modular project structure for easy integration

## Build Targets

| QNX Version | Architectures Supported |
|-------------|-------------------------|
| QNX 7.0     | x86_64, ARM, ARM_64     |
| QNX 7.1     | x86_64, ARM, ARM_64     |

## Project Structure

```
├── candump/   # Utility to dump CAN messages
├── cansend/   # Utility to send CAN messages
├── common/    # Shared files
├── resmgr/    # Peak CAN resource manager (driver)
├── README.md  # Documentation
├── LICENSE    # License file
├── CHANGELOG.md
├── jamroot.jam

```

## Build

ready to build as QNX projects with project files or Boost build

## Usage

```sh
# Start resource manager
canrmd -d can1 -s 800

# Send a CAN message
cansend can1 123#DEADBEEF

# Dump CAN messages
candump can1
```

### Options

- `-d device` : Specify PCAN device node (e.g., `can0`)
- `-s bus_speed` : Bus speed in kbit per second (e.g., 125 for 125kbit/s)

Possible speeds: 10, 20, 50, 100, 125, 250, 500, 800, 1000

## Notes

- Requires PEAK PCAN-PCI hardware
- Tested on QNX 7.0 and 7.1 with x86 and ARM platforms

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

## Contributing

Pull requests are welcome. For major changes, please open an issue first to discuss what you’d like to change.

