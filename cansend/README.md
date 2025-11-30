# CAN frame send utility

QNX CAN frame send utility for CAN bus interface cards.

## Features

- CAN frame send utility
- Compatible with:
  - QNX 7.0
  - QNX 7.1
- Supports multiple architectures:
  - x86_64
  - ARM
  - ARM_64

## Build Targets

| QNX Version | Architectures Supported |
|-------------|-------------------------|
| QNX 7.0     | x86_64, ARM, ARM_64     |
| QNX 7.1     | x86_64, ARM, ARM_64     |


## Usage

```sh
 ./cansend <device> <can_frame>.
```

```sh
<can_frame>:
 <can_id>#{data}          for CAN CC (Classical CAN 2.0B) data frames
 <can_id>#R{len}          for CAN CC (Classical CAN 2.0B) data frames
<can_id>:
 3 (SFF) or 8 (EFF) hex chars
{data}:
 0..8 ASCII hex-values (optionally separated by '.')
{len}:
 an optional 0..8 value as RTR frames can contain a valid dlc field
```

### Examples

```
  5A1#11.2233.44556677.88
  242#1.1.2.3.4.5.6.7.8
  123#DEADBEEF  
  5AA# 
  1F334455#1122334455667788  
  123#R 
  00000123#R3  
  333#R8 
```

## Notes

- Tested on QNX 7.0 and 7.1 with x86 and ARM platforms

