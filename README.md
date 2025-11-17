# Peak PCAN PCI QNX Resource Manager

QNX resource manager for PEAK-System PCAN-PCI CAN bus interface cards.

## Overview

This project provides a native QNX resource manager implementation for PEAK PCAN-PCI devices, enabling real-time CAN communication in embedded systems running QNX Neutrino RTOS.

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
├── src/        # Resource manager source code
├── include/    # Header files and CAN interface definitions
├── build/      # QNX build files and configurations
```

## Build

ready to build as QNX projects with project files or Boost build

## Usage

```sh
canrmd [-d device] [-s bus_speed]
```

### Options

- `-d device` : Specify PCAN device node (e.g., `/dev/can0`)
- `-s bus_speed` : Bus speed in kbit per second (e.g., 125 for 125kbit/s)

### Example

```sh
canrmd -d /dev/can1 -s 800
```

## Notes

- Requires PEAK PCAN-PCI hardware
- Tested on QNX 7.0 and 7.1 with x86 and ARM platforms

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

## Contributing

Pull requests are welcome. For major changes, please open an issue first to discuss what you’d like to change.

