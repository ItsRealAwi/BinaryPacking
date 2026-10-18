"""Pack and unpack numeric values as binary data.

Fixed-width helpers use little-endian encoding by default and validate that
values fit the requested width. ``big`` reverses byte order; ``baremetal-*``
selects the same fixed width with the stated byte order; ``dense-*`` removes
redundant leading zero/sign-extension bytes and therefore produces variable
length output. Dense encodings are useful for compact storage but are not
interchangeable with fixed-width formats.

Examples:
    >>> from Obex.Encoding import BinaryPacking
    >>> BinaryPacking.PackInt32(258)
    b'\\x02\\x01\\x00\\x00'
    >>> BinaryPacking.UnPackInt32(b"\\x02\\x01\\x00\\x00")
    258
    >>> BinaryPacking.PackUInt32(0x1234, "big")
    b'\\x00\\x00\\x12\\x34'
    >>> BinaryPacking.UnPackFloat32(BinaryPacking.PackFloat32(1.5))
    1.5
    >>> BinaryPacking.UnPackBools(BinaryPacking.PackBools([True, False, True]))[:3]
    [True, False, True]

Unpack functions require the exact fixed-width input size, except dense integer
formats, which accept non-empty variable-length encodings. Float helpers use
IEEE-754 binary formats. Boolean arrays store the first value in the least
significant bit for little-endian modes and in the most significant bit for
big-endian modes. Pointer helpers encode an unsigned 64-bit integer.
"""

from typing import Literal

Endian = Literal[
    "little",
    "big",
    "baremetal-little",
    "baremetal-big",
    "dense-little",
    "dense-big",
]

def ValidateEndian(Endian: str) -> tuple[str, bool]:
    """Validate an endian option and return its byte order and dense-mode flag."""
    ...

def RoundRatio(Numerator: int, Denominator: int) -> int:
    """Round an integer ratio to nearest, resolving ties to an even result."""
    ...

def ValidateFloatFormat(ExponentBits: int, FractionBits: int) -> int:
    """Validate IEEE-like field widths and return their total bit count."""
    ...

def PackInteger(Value: int, BitCount: int, Endian: Endian, Signed: bool) -> bytes:
    """Encode an integer with an explicit width, signedness, and byte order."""
    ...

def UnPackInteger(Value: bytes, BitCount: int, Endian: Endian, Signed: bool) -> int:
    """Decode an integer encoded by PackInteger."""
    ...

def FloatToBits(Value: float, ExponentBits: int, FractionBits: int) -> int:
    """Encode a floating-point value as an integer IEEE-style bit pattern."""
    ...

def BitsToFloat(Value: int, ExponentBits: int, FractionBits: int) -> float:
    """Decode an integer IEEE-style bit pattern as a Python float."""
    ...

def PackFloat(Value: float, ExponentBits: int, FractionBits: int, Endian: Endian) -> bytes:
    """Pack a float using the selected exponent/fraction bit widths."""
    ...

def UnPackFloat(Value: bytes, ExponentBits: int, FractionBits: int, Endian: Endian) -> float:
    """Unpack a float using the selected exponent/fraction bit widths."""
    ...

def PackInt8(Value: int, Endian: Endian = "little") -> bytes:
    """Pack a signed 8-bit integer (range -128 through 127)."""
    ...

def PackInt16(Value: int, Endian: Endian = "little") -> bytes:
    """Pack a signed 16-bit integer (range -32768 through 32767)."""
    ...

def PackInt32(Value: int, Endian: Endian = "little") -> bytes:
    """Pack a signed 32-bit integer."""
    ...

def PackInt64(Value: int, Endian: Endian = "little") -> bytes:
    """Pack a signed 64-bit integer."""
    ...

def PackUInt8(Value: int, Endian: Endian = "little") -> bytes:
    """Pack an unsigned 8-bit integer (range 0 through 255)."""
    ...

def PackUInt16(Value: int, Endian: Endian = "little") -> bytes:
    """Pack an unsigned 16-bit integer."""
    ...

def PackUInt32(Value: int, Endian: Endian = "little") -> bytes:
    """Pack an unsigned 32-bit integer."""
    ...

def PackUInt64(Value: int, Endian: Endian = "little") -> bytes:
    """Pack an unsigned 64-bit integer."""
    ...

def PackFloat16(Value: float, Endian: Endian = "little") -> bytes:
    """Pack an IEEE-754 binary16 value into two bytes."""
    ...

def PackFloat32(Value: float, Endian: Endian = "little") -> bytes:
    """Pack an IEEE-754 binary32 value into four bytes."""
    ...

def PackFloat64(Value: float, Endian: Endian = "little") -> bytes:
    """Pack an IEEE-754 binary64 value into eight bytes."""
    ...

def PackBool(Value: bool, Endian: Endian = "little") -> bytes:
    """Pack one Boolean as a single byte containing zero or one."""
    ...

def PackBools(Value: list[bool], Endian: Endian = "little") -> bytes:
    """Pack Boolean values densely, eight values per output byte."""
    ...

def PackPointer(Value: int, Endian: Endian = "little") -> bytes:
    """Pack an unsigned 64-bit integer used to represent a pointer value."""
    ...

def UnPackInt8(Value: bytes, Endian: Endian = "little") -> int:
    """Unpack a signed 8-bit integer from exactly one byte."""
    ...

def UnPackInt16(Value: bytes, Endian: Endian = "little") -> int:
    """Unpack a signed 16-bit integer from exactly two bytes."""
    ...

def UnPackInt32(Value: bytes, Endian: Endian = "little") -> int:
    """Unpack a signed 32-bit integer from exactly four bytes."""
    ...

def UnPackInt64(Value: bytes, Endian: Endian = "little") -> int:
    """Unpack a signed 64-bit integer from exactly eight bytes."""
    ...

def UnPackUInt8(Value: bytes, Endian: Endian = "little") -> int:
    """Unpack an unsigned 8-bit integer from exactly one byte."""
    ...

def UnPackUInt16(Value: bytes, Endian: Endian = "little") -> int:
    """Unpack an unsigned 16-bit integer from exactly two bytes."""
    ...

def UnPackUInt32(Value: bytes, Endian: Endian = "little") -> int:
    """Unpack an unsigned 32-bit integer from exactly four bytes."""
    ...

def UnPackUInt64(Value: bytes, Endian: Endian = "little") -> int:
    """Unpack an unsigned 64-bit integer from exactly eight bytes."""
    ...

def UnPackFloat16(Value: bytes, Endian: Endian = "little") -> float:
    """Unpack an IEEE-754 binary16 value from exactly two bytes."""
    ...

def UnPackFloat32(Value: bytes, Endian: Endian = "little") -> float:
    """Unpack an IEEE-754 binary32 value from exactly four bytes."""
    ...

def UnPackFloat64(Value: bytes, Endian: Endian = "little") -> float:
    """Unpack an IEEE-754 binary64 value from exactly eight bytes."""
    ...

def UnPackBool(Value: bytes, Endian: Endian = "little") -> bool:
    """Unpack a Boolean byte; only zero and one are valid."""
    ...

def UnPackBools(Value: bytes, Endian: Endian = "little") -> list[bool]:
    """Unpack each input byte into eight Booleans in the selected bit order."""
    ...

def UnPackPointer(Value: bytes, Endian: Endian = "little") -> int:
    """Unpack an unsigned 64-bit pointer representation."""
    ...
