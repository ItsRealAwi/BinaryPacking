# BinaryPacking

C extension for packing and unpacking numeric values (ints, floats, bools, pointers) as binary data.
Supports fixed-width little/big-endian formats and dense variable-length integers.

## Install

```
pip install BinaryPacking
```

## Usage

```python
from BinaryPacking import Packing

Packing.PackInt32(258)            # b'\x02\x01\x00\x00'
Packing.UnPackInt32(b"\x02\x01\x00\x00")  # 258
Packing.PackUInt32(0x1234, "big") # b'\x00\x00\x12\x34'
```

## Build from source

- Windows: `build.bat` (also `build.bat wheel`, `build.bat clean`)
- Make: `make build`, `make wheel`, `make dist`, `make upload`, `make clean`

## License

MIT
