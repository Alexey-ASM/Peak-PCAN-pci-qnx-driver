# CAN dump utility

QNX CAN dump utility for CAN bus interface cards.

## Overview

## Features

- CAN damp utility
- Data filtering
- Log to file
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
./candump_g [options] <CAN interface>+
```
  (use CTRL-C to terminate ./candump)

### Options

```sh
         -t <type>   (timestamp: (a)bsolute/(d)elta/(z)ero/(A)bsolute w date)
         -N          (log nanosecond timestamps instead of microseconds)
         -a          (enable additional ASCII output)
         -s          (silent mode)
         -l          (log CAN-frames into file. Sets '-s (silent mode) by default)
         -f <fname>  (log CAN-frames into file <fname>. Sets '-s 1' by default)
         -n <count>  (terminate after reception of <count> CAN frames)
         -e          (dump CAN error frames in human-readable format)
         -T <msecs>  (terminate after <msecs> if no frames were received)
```
One CAN interfaces with optional filter sets can be specified
on the commandline in the form: **\<ifname\>\[,filter\]**

### Filters

Comma separated filters can be specified for each given CAN interface:

```
    <can_id>:<can_mask>
         (matches when <received_can_id> & mask == can_id & mask)
    <can_id>~<can_mask>
         (matches when <received_can_id> & mask != can_id & mask)
```

CAN IDs, masks and data content are given and expected in hexadecimal values.
When the can_id is 8 digits long the CAN_EFF_FLAG is set for 29 bit EFF format.
Without any given filter all data frames are received ('0:0' default filter).

### Examples

```sh
./candump_g -ta can0,123:7FF,400:700

./candump_g vcan2,12345678:DFFFFFFF
         (match only for extended CAN ID 12345678)
./candump_g vcan2,123:7FF
         (matches CAN ID 123 - including EFF and RTR frames)
./candump_g vcan2,123:C00007FF
         (matches CAN ID 123 - only SFF and non-RTR frames)
```

## Notes

- Tested on QNX 7.0 and 7.1 with x86 and ARM platforms

## Known issues

- doesn't dump self messages

