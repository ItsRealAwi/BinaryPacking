#define PY_SSIZE_T_CLEAN
#include <Python.h>

#ifdef Py_LIMITED_API
#define PyBytes_AS_STRING(Value) PyBytes_AsString(Value)
#define PyBytes_GET_SIZE(Value) PyBytes_Size(Value)
#define PyTuple_GET_ITEM(Value, Index) PyTuple_GetItem(Value, Index)
#define PyList_GET_ITEM(Value, Index) PyList_GetItem(Value, Index)
#define PyList_GET_SIZE(Value) PyList_Size(Value)
#define PyFloat_AS_DOUBLE(Value) PyFloat_AsDouble(Value)
#define Py_SETREF(Value, Replacement) \
    do { \
        PyObject *ObexOldValue = (PyObject *)(Value); \
        (Value) = (Replacement); \
        Py_DECREF(ObexOldValue); \
    } while (0)
#endif

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int LittleEndian;
    int Dense;
} EndianInfo;

static PyObject *
RequireExactInteger(PyObject *Value, const char *Name)
{
    if (!PyLong_CheckExact(Value)) {
        PyErr_Format(PyExc_TypeError, "%s must be an integer", Name);
        return NULL;
    }
    return Value;
}

static int
GetEndianInfo(PyObject *Endian, EndianInfo *Info)
{
    if (!PyUnicode_CheckExact(Endian)) {
        PyErr_SetString(PyExc_TypeError, "Endian must be a string");
        return -1;
    }

    if (PyUnicode_CompareWithASCIIString(Endian, "little") == 0 ||
        PyUnicode_CompareWithASCIIString(Endian, "baremetal-little") == 0) {
        Info->LittleEndian = 1;
        Info->Dense = 0;
        return 0;
    }
    if (PyUnicode_CompareWithASCIIString(Endian, "big") == 0 ||
        PyUnicode_CompareWithASCIIString(Endian, "baremetal-big") == 0) {
        Info->LittleEndian = 0;
        Info->Dense = 0;
        return 0;
    }
    if (PyUnicode_CompareWithASCIIString(Endian, "dense-little") == 0) {
        Info->LittleEndian = 1;
        Info->Dense = 1;
        return 0;
    }
    if (PyUnicode_CompareWithASCIIString(Endian, "dense-big") == 0) {
        Info->LittleEndian = 0;
        Info->Dense = 1;
        return 0;
    }
    if (PyErr_Occurred()) {
        return -1;
    }
    PyErr_SetString(
        PyExc_ValueError,
        "Endian must be little, big, baremetal-little, baremetal-big, "
        "dense-little, or dense-big");
    return -1;
}

static int
GetPositiveSize(PyObject *Value, const char *Name, Py_ssize_t *Result)
{
    Py_ssize_t Converted;

    if (RequireExactInteger(Value, Name) == NULL) {
        return -1;
    }
    Converted = PyLong_AsSsize_t(Value);
    if (Converted == -1 && PyErr_Occurred()) {
        return -1;
    }
    *Result = Converted;
    return 0;
}

static int
ValidateFormat(PyObject *ExponentBits, PyObject *FractionBits,
               Py_ssize_t *ExponentWidth, Py_ssize_t *FractionWidth,
               Py_ssize_t *TotalWidth)
{
    Py_ssize_t ExponentValue;
    Py_ssize_t FractionValue;
    Py_ssize_t Total;

    if (GetPositiveSize(ExponentBits, "ExponentBits", &ExponentValue) < 0 ||
        GetPositiveSize(FractionBits, "FractionBits", &FractionValue) < 0) {
        return -1;
    }
    if (ExponentValue < 2 || FractionValue < 1) {
        PyErr_SetString(
            PyExc_ValueError,
            "ExponentBits must be at least two and FractionBits must be positive");
        return -1;
    }
    if (ExponentValue > PY_SSIZE_T_MAX - FractionValue - 1) {
        PyErr_SetString(PyExc_OverflowError, "Float format width is too large");
        return -1;
    }
    Total = ExponentValue + FractionValue + 1;
    if (Total % 8 != 0) {
        PyErr_SetString(
            PyExc_ValueError,
            "The selected float format must have a byte-aligned width");
        return -1;
    }
    *ExponentWidth = ExponentValue;
    *FractionWidth = FractionValue;
    *TotalWidth = Total;
    return 0;
}

static PyObject *
PowerOfTwo(Py_ssize_t Shift)
{
    PyObject *One = PyLong_FromLong(1);
    PyObject *ShiftObject;
    PyObject *Result;

    if (One == NULL) {
        return NULL;
    }
    ShiftObject = PyLong_FromSsize_t(Shift);
    if (ShiftObject == NULL) {
        Py_DECREF(One);
        return NULL;
    }
    Result = PyNumber_Lshift(One, ShiftObject);
    Py_DECREF(ShiftObject);
    Py_DECREF(One);
    return Result;
}

static PyObject *
IntegerFromBytes(const unsigned char *Buffer, Py_ssize_t ByteCount,
                 int LittleEndian, int IsSigned)
{
    PyObject *Result = PyLong_FromLong(0);
    PyObject *ByteMask = PyLong_FromLong(255);
    PyObject *Eight = PyLong_FromLong(8);
    Py_ssize_t Index;
    if (Result == NULL || ByteMask == NULL || Eight == NULL) {
        Py_XDECREF(Result);
        Py_XDECREF(ByteMask);
        Py_XDECREF(Eight);
        return NULL;
    }
    for (Index = 0; Index < ByteCount; Index++) {
        Py_ssize_t ByteIndex = LittleEndian ? ByteCount - 1 - Index : Index;
        PyObject *Shifted = PyNumber_Lshift(Result, Eight);
        PyObject *Byte = PyLong_FromLong(Buffer[ByteIndex]);
        PyObject *Next;
        if (Shifted == NULL || Byte == NULL) {
            Py_XDECREF(Shifted);
            Py_XDECREF(Byte);
            Py_DECREF(Result);
            Py_DECREF(ByteMask);
            Py_DECREF(Eight);
            return NULL;
        }
        Next = PyNumber_Or(Shifted, Byte);
        Py_DECREF(Shifted);
        Py_DECREF(Byte);
        if (Next == NULL) {
            Py_DECREF(Result);
            Py_DECREF(ByteMask);
            Py_DECREF(Eight);
            return NULL;
        }
        Py_SETREF(Result, Next);
    }
    Py_DECREF(ByteMask);
    Py_DECREF(Eight);
    if (IsSigned && ByteCount > 0) {
        Py_ssize_t MostSignificantIndex = LittleEndian ? ByteCount - 1 : 0;
        if (Buffer[MostSignificantIndex] & 0x80) {
            PyObject *BitCount = PyLong_FromSsize_t(ByteCount * 8);
            PyObject *Modulus;
            PyObject *NegativeModulus;
            if (BitCount == NULL) {
                Py_DECREF(Result);
                return NULL;
            }
            Modulus = PyNumber_Lshift(PyLong_FromLong(1), BitCount);
            Py_DECREF(BitCount);
            if (Modulus == NULL) {
                Py_DECREF(Result);
                return NULL;
            }
            NegativeModulus = PyNumber_Negative(Modulus);
            if (NegativeModulus == NULL) {
                Py_DECREF(Modulus);
                Py_DECREF(Result);
                return NULL;
            }
            Py_SETREF(Result, PyNumber_Add(Result, NegativeModulus));
            Py_DECREF(Modulus);
            Py_DECREF(NegativeModulus);
            if (Result == NULL) {
                return NULL;
            }
        }
    }
    return Result;
}

static PyObject *
ShiftObject(PyObject *Value, Py_ssize_t Shift, int Left)
{
    PyObject *ShiftObject = PyLong_FromSsize_t(Shift);
    PyObject *Result;
    if (ShiftObject == NULL) {
        return NULL;
    }
    Result = Left ? PyNumber_Lshift(Value, ShiftObject)
                  : PyNumber_Rshift(Value, ShiftObject);
    Py_DECREF(ShiftObject);
    return Result;
}

static PyObject *
RoundRatioObjects(PyObject *Numerator, PyObject *Denominator)
{
    PyObject *DivMod = NULL;
    PyObject *Quotient = NULL;
    PyObject *Remainder = NULL;
    PyObject *TwiceRemainder = NULL;
    PyObject *One = NULL;
    PyObject *Parity = NULL;
    PyObject *Two = NULL;
    int Comparison;

    DivMod = PyNumber_Divmod(Numerator, Denominator);
    if (DivMod == NULL) {
        goto error;
    }
    Quotient = Py_NewRef(PyTuple_GET_ITEM(DivMod, 0));
    Remainder = Py_NewRef(PyTuple_GET_ITEM(DivMod, 1));
    Two = PyLong_FromLong(2);
    if (Two == NULL) {
        goto error;
    }
    TwiceRemainder = PyNumber_Multiply(Remainder, Two);
    if (TwiceRemainder == NULL) {
        goto error;
    }
    Comparison = PyObject_RichCompareBool(TwiceRemainder, Denominator, Py_GT);
    if (Comparison < 0) {
        goto error;
    }
    if (Comparison == 0) {
        Comparison = PyObject_RichCompareBool(TwiceRemainder, Denominator, Py_EQ);
        if (Comparison < 0) {
            goto error;
        }
        if (Comparison) {
            One = PyLong_FromLong(1);
            if (One == NULL) {
                goto error;
            }
            Parity = PyNumber_And(Quotient, One);
            if (Parity == NULL) {
                goto error;
            }
            Comparison = PyObject_IsTrue(Parity);
            if (Comparison < 0) {
                goto error;
            }
        }
    }
    if (Comparison) {
        PyObject *Rounded;
        if (One == NULL) {
            One = PyLong_FromLong(1);
            if (One == NULL) {
                goto error;
            }
        }
        Rounded = PyNumber_Add(Quotient, One);
        Py_DECREF(Quotient);
        Quotient = Rounded;
        if (Quotient == NULL) {
            goto error;
        }
    }

    Py_DECREF(DivMod);
    Py_DECREF(Remainder);
    Py_XDECREF(TwiceRemainder);
    Py_XDECREF(One);
    Py_XDECREF(Parity);
    Py_XDECREF(Two);
    return Quotient;

error:
    Py_XDECREF(DivMod);
    Py_XDECREF(Quotient);
    Py_XDECREF(Remainder);
    Py_XDECREF(TwiceRemainder);
    Py_XDECREF(One);
    Py_XDECREF(Parity);
    Py_XDECREF(Two);
    return NULL;
}

static PyObject *
PackIntegerImpl(PyObject *Value, PyObject *BitCountObject,
                PyObject *EndianObject, PyObject *SignedObject)
{
    Py_ssize_t BitCount;
    Py_ssize_t ByteCount;
    EndianInfo Endian;
    int IsSigned;
    PyObject *Minimum = NULL;
    PyObject *Maximum = NULL;
    PyObject *One = NULL;
    PyObject *Shift = NULL;
    PyObject *Comparison = NULL;
    PyObject *Bytes = NULL;
    unsigned char *Buffer = NULL;
    int Needed;
    Py_ssize_t Start;
    Py_ssize_t End;
    unsigned char Fill;
    PyObject *CurrentValue = NULL;
    PyObject *ByteMask = NULL;
    PyObject *Eight = NULL;
    Py_ssize_t Index;

    if (RequireExactInteger(Value, "Value") == NULL ||
        GetPositiveSize(BitCountObject, "BitCount", &BitCount) < 0) {
        return NULL;
    }
    if (BitCount <= 0 || BitCount % 8 != 0) {
        PyErr_SetString(PyExc_ValueError, "BitCount must be a positive multiple of eight");
        return NULL;
    }
    if (!PyBool_Check(SignedObject)) {
        PyErr_SetString(PyExc_TypeError, "Signed must be a bool");
        return NULL;
    }
    if (GetEndianInfo(EndianObject, &Endian) < 0) {
        return NULL;
    }
    IsSigned = (SignedObject == Py_True);
    ByteCount = BitCount / 8;
    if ((size_t)ByteCount > PY_SSIZE_T_MAX) {
        PyErr_SetString(PyExc_OverflowError, "BitCount is too large");
        return NULL;
    }

    One = PyLong_FromLong(1);
    Shift = PyLong_FromSsize_t(BitCount - (IsSigned ? 1 : 0));
    if (One == NULL || Shift == NULL) {
        goto error;
    }
    Maximum = PyNumber_Lshift(One, Shift);
    if (Maximum == NULL) {
        goto error;
    }
    if (IsSigned) {
        PyObject *MinusOne = PyLong_FromLong(1);
        PyObject *MaxValue;
        if (MinusOne == NULL) {
            goto error;
        }
        Minimum = PyNumber_Negative(Maximum);
        if (Minimum == NULL) {
            Py_DECREF(MinusOne);
            goto error;
        }
        MaxValue = PyNumber_Subtract(Maximum, MinusOne);
        Py_DECREF(MinusOne);
        if (MaxValue == NULL) {
            goto error;
        }
        Py_SETREF(Maximum, MaxValue);
    }
    else {
        Minimum = PyLong_FromLong(0);
        PyObject *MaxValue;
        PyObject *MinusOne = PyLong_FromLong(1);
        if (MinusOne == NULL) {
            goto error;
        }
        MaxValue = PyNumber_Subtract(Maximum, MinusOne);
        Py_DECREF(MinusOne);
        if (MaxValue == NULL) {
            goto error;
        }
        Py_SETREF(Maximum, MaxValue);
    }
    if (Minimum == NULL) {
        goto error;
    }
    Comparison = PyObject_RichCompare(Value, Minimum, Py_LT);
    if (Comparison == NULL) {
        goto error;
    }
    Needed = PyObject_IsTrue(Comparison);
    if (Needed < 0) {
        goto error;
    }
    if (Needed) {
        PyErr_SetString(PyExc_OverflowError, "Value is outside the selected integer range");
        goto error;
    }
    Py_CLEAR(Comparison);
    Comparison = PyObject_RichCompare(Value, Maximum, Py_GT);
    if (Comparison == NULL) {
        goto error;
    }
    Needed = PyObject_IsTrue(Comparison);
    if (Needed < 0) {
        goto error;
    }
    if (Needed) {
        PyErr_SetString(PyExc_OverflowError, "Value is outside the selected integer range");
        goto error;
    }
    Py_CLEAR(Comparison);

    Buffer = PyMem_Malloc((size_t)ByteCount);
    if (Buffer == NULL) {
        PyErr_NoMemory();
        goto error;
    }
    CurrentValue = Py_NewRef(Value);
    ByteMask = PyLong_FromLong(255);
    Eight = PyLong_FromLong(8);
    if (CurrentValue == NULL || ByteMask == NULL || Eight == NULL) {
        goto error;
    }
    for (Index = 0; Index < ByteCount; Index++) {
        PyObject *Byte = PyNumber_And(CurrentValue, ByteMask);
        long ByteValue;
        PyObject *Shifted;
        if (Byte == NULL) {
            goto error;
        }
        ByteValue = PyLong_AsLong(Byte);
        Py_DECREF(Byte);
        if (ByteValue == -1 && PyErr_Occurred()) {
            goto error;
        }
        Buffer[Endian.LittleEndian ? Index : ByteCount - 1 - Index] =
            (unsigned char)ByteValue;
        Shifted = PyNumber_Rshift(CurrentValue, Eight);
        if (Shifted == NULL) {
            goto error;
        }
        Py_SETREF(CurrentValue, Shifted);
    }
    Py_CLEAR(CurrentValue);
    Py_CLEAR(ByteMask);
    Py_CLEAR(Eight);

    Start = 0;
    End = ByteCount;
    if (Endian.Dense) {
        if (!IsSigned) {
            if (Endian.LittleEndian) {
                while (End - Start > 1 && Buffer[End - 1] == 0) {
                    End--;
                }
            }
            else {
                while (End - Start > 1 && Buffer[Start] == 0) {
                    Start++;
                }
            }
        }
        else {
            PyObject *Zero = PyLong_FromLong(0);
            int IsNegative;
            if (Zero == NULL) {
                goto error;
            }
            IsNegative = PyObject_RichCompareBool(Value, Zero, Py_LT);
            Py_DECREF(Zero);
            if (IsNegative < 0) {
                goto error;
            }
            Fill = IsNegative ? 0xff : 0;
            if (Endian.LittleEndian) {
                while (End - Start > 1 && Buffer[End - 1] == Fill &&
                       (Buffer[End - 2] & 0x80) == (Fill & 0x80)) {
                    End--;
                }
            }
            else {
                while (End - Start > 1 && Buffer[Start] == Fill &&
                       (Buffer[Start + 1] & 0x80) == (Fill & 0x80)) {
                    Start++;
                }
            }
        }
    }
    Bytes = PyBytes_FromStringAndSize((const char *)Buffer + Start, End - Start);

error:
    PyMem_Free(Buffer);
    Py_XDECREF(Minimum);
    Py_XDECREF(Maximum);
    Py_XDECREF(One);
    Py_XDECREF(Shift);
    Py_XDECREF(Comparison);
    Py_XDECREF(CurrentValue);
    Py_XDECREF(ByteMask);
    Py_XDECREF(Eight);
    return Bytes;
}

static PyObject *
UnPackIntegerImpl(PyObject *Value, PyObject *BitCountObject,
                  PyObject *EndianObject, PyObject *SignedObject)
{
    Py_ssize_t BitCount;
    Py_ssize_t ByteCount;
    Py_ssize_t InputLength;
    EndianInfo Endian;
    int IsSigned;
    const unsigned char *Input;
    unsigned char *Buffer = NULL;
    Py_ssize_t PadCount;
    unsigned char Fill = 0;
    PyObject *Result = NULL;

    if (!PyBytes_CheckExact(Value)) {
        PyErr_SetString(PyExc_TypeError, "Value must be bytes");
        return NULL;
    }
    if (GetPositiveSize(BitCountObject, "BitCount", &BitCount) < 0) {
        return NULL;
    }
    if (BitCount <= 0 || BitCount % 8 != 0) {
        PyErr_SetString(PyExc_ValueError, "BitCount must be a positive multiple of eight");
        return NULL;
    }
    if (!PyBool_Check(SignedObject)) {
        PyErr_SetString(PyExc_TypeError, "Signed must be a bool");
        return NULL;
    }
    if (GetEndianInfo(EndianObject, &Endian) < 0) {
        return NULL;
    }
    IsSigned = (SignedObject == Py_True);
    ByteCount = BitCount / 8;
    Input = (const unsigned char *)PyBytes_AS_STRING(Value);
    InputLength = PyBytes_GET_SIZE(Value);
    if (InputLength == 0 || InputLength > ByteCount ||
        (!Endian.Dense && InputLength != ByteCount)) {
        PyErr_SetString(PyExc_ValueError, "Value has an invalid byte count for the selected format");
        return NULL;
    }

    Buffer = PyMem_Malloc((size_t)ByteCount);
    if (Buffer == NULL) {
        return PyErr_NoMemory();
    }
    PadCount = ByteCount - InputLength;
    if (IsSigned) {
        unsigned char MostSignificant =
            Endian.LittleEndian ? Input[InputLength - 1] : Input[0];
        Fill = (MostSignificant & 0x80) ? 0xff : 0;
    }
    if (Endian.LittleEndian) {
        memcpy(Buffer, Input, (size_t)InputLength);
        memset(Buffer + InputLength, Fill, (size_t)PadCount);
    }
    else {
        memset(Buffer, Fill, (size_t)PadCount);
        memcpy(Buffer + PadCount, Input, (size_t)InputLength);
    }
    Result = IntegerFromBytes(Buffer, ByteCount, Endian.LittleEndian, IsSigned);
    PyMem_Free(Buffer);
    return Result;
}

static PyObject *
ValidateEndian(PyObject *Self, PyObject *Args, PyObject *Keywords)
{
    static char *Names[] = {"Endian", NULL};
    PyObject *EndianObject;
    EndianInfo Endian;
    PyObject *Tuple;
    PyObject *Order;

    if (!PyArg_ParseTupleAndKeywords(Args, Keywords, "O:ValidateEndian", Names, &EndianObject)) {
        return NULL;
    }
    if (GetEndianInfo(EndianObject, &Endian) < 0) {
        return NULL;
    }
    Order = PyUnicode_FromString(Endian.LittleEndian ? "little" : "big");
    if (Order == NULL) {
        return NULL;
    }
    PyObject *Dense = PyBool_FromLong(Endian.Dense);
    if (Dense == NULL) {
        Py_DECREF(Order);
        return NULL;
    }
    Tuple = PyTuple_Pack(2, Order, Dense);
    Py_DECREF(Order);
    Py_DECREF(Dense);
    return Tuple;
}

static PyObject *
RoundRatio(PyObject *Self, PyObject *Args, PyObject *Keywords)
{
    static char *Names[] = {"Numerator", "Denominator", NULL};
    PyObject *Numerator;
    PyObject *Denominator;
    PyObject *Zero = NULL;
    int Comparison;

    if (!PyArg_ParseTupleAndKeywords(Args, Keywords, "OO:RoundRatio", Names,
                                     &Numerator, &Denominator)) {
        return NULL;
    }
    if (RequireExactInteger(Numerator, "Numerator") == NULL ||
        RequireExactInteger(Denominator, "Denominator") == NULL) {
        return NULL;
    }
    Zero = PyLong_FromLong(0);
    if (Zero == NULL) {
        return NULL;
    }
    Comparison = PyObject_RichCompareBool(Numerator, Zero, Py_LT);
    if (Comparison < 0) {
        Py_DECREF(Zero);
        return NULL;
    }
    if (Comparison == 1) {
        Py_DECREF(Zero);
        PyErr_SetString(PyExc_ValueError,
                        "RoundRatio requires a nonnegative numerator and positive denominator");
        return NULL;
    }
    Comparison = PyObject_RichCompareBool(Denominator, Zero, Py_LE);
    Py_DECREF(Zero);
    if (Comparison < 0) {
        return NULL;
    }
    if (Comparison == 1) {
        PyErr_SetString(PyExc_ValueError,
                        "RoundRatio requires a nonnegative numerator and positive denominator");
        return NULL;
    }
    return RoundRatioObjects(Numerator, Denominator);
}

static PyObject *
ValidateFloatFormat(PyObject *Self, PyObject *Args, PyObject *Keywords)
{
    static char *Names[] = {"ExponentBits", "FractionBits", NULL};
    PyObject *ExponentBits;
    PyObject *FractionBits;
    Py_ssize_t ExponentWidth;
    Py_ssize_t FractionWidth;
    Py_ssize_t TotalWidth;

    if (!PyArg_ParseTupleAndKeywords(Args, Keywords, "OO:ValidateFloatFormat",
                                     Names, &ExponentBits, &FractionBits)) {
        return NULL;
    }
    if (ValidateFormat(ExponentBits, FractionBits, &ExponentWidth,
                       &FractionWidth, &TotalWidth) < 0) {
        return NULL;
    }
    return PyLong_FromSsize_t(TotalWidth);
}

static PyObject *
PackInteger(PyObject *Self, PyObject *Args, PyObject *Keywords)
{
    static char *Names[] = {"Value", "BitCount", "Endian", "Signed", NULL};
    PyObject *Value;
    PyObject *BitCount;
    PyObject *Endian;
    PyObject *Signed;
    if (!PyArg_ParseTupleAndKeywords(Args, Keywords, "OOOO:PackInteger", Names,
                                     &Value, &BitCount, &Endian, &Signed)) {
        return NULL;
    }
    return PackIntegerImpl(Value, BitCount, Endian, Signed);
}

static PyObject *
UnPackInteger(PyObject *Self, PyObject *Args, PyObject *Keywords)
{
    static char *Names[] = {"Value", "BitCount", "Endian", "Signed", NULL};
    PyObject *Value;
    PyObject *BitCount;
    PyObject *Endian;
    PyObject *Signed;
    if (!PyArg_ParseTupleAndKeywords(Args, Keywords, "OOOO:UnPackInteger", Names,
                                     &Value, &BitCount, &Endian, &Signed)) {
        return NULL;
    }
    return UnPackIntegerImpl(Value, BitCount, Endian, Signed);
}

static PyObject *
FloatToBitsImpl(PyObject *Value, PyObject *ExponentBits, PyObject *FractionBits)
{
    Py_ssize_t ExponentWidth;
    Py_ssize_t FractionWidth;
    Py_ssize_t TotalWidth;
    Py_ssize_t BiasShift;
    int BinaryExponent;
    int SignBit;
    double FloatValue;
    double Fraction;
    uint64_t Mantissa;
    PyObject *Numerator = NULL;
    PyObject *Denominator = NULL;
    PyObject *Rounded = NULL;
    PyObject *One = NULL;
    PyObject *Bias = NULL;
    PyObject *MaximumExponent = NULL;
    PyObject *MinExponentObject = NULL;
    PyObject *EncodedExponent = NULL;
    PyObject *EncodedFraction = NULL;
    PyObject *SignValue = NULL;
    PyObject *Temporary = NULL;
    PyObject *Result = NULL;
    Py_ssize_t Shift;
    int Comparison;

    if (!PyFloat_CheckExact(Value)) {
        PyErr_SetString(PyExc_TypeError, "Value must be a float");
        return NULL;
    }
    if (ValidateFormat(ExponentBits, FractionBits, &ExponentWidth,
                       &FractionWidth, &TotalWidth) < 0) {
        return NULL;
    }
    FloatValue = PyFloat_AS_DOUBLE(Value);
    SignBit = signbit(FloatValue) ? 1 : 0;
    SignValue = PyLong_FromLong(SignBit);
    if (SignValue == NULL) {
        goto done;
    }
    Temporary = ShiftObject(SignValue, TotalWidth - 1, 1);
    Py_CLEAR(SignValue);
    SignValue = Temporary;
    Temporary = NULL;
    if (SignValue == NULL) {
        goto done;
    }

    One = PyLong_FromLong(1);
    if (One == NULL) {
        goto done;
    }
    BiasShift = ExponentWidth - 1;
    Bias = PowerOfTwo(BiasShift);
    if (Bias == NULL) {
        goto done;
    }
    Temporary = PyNumber_Subtract(Bias, One);
    Py_SETREF(Bias, Temporary);
    Temporary = NULL;
    if (Bias == NULL) {
        goto done;
    }
    MaximumExponent = Py_NewRef(Bias);
    MinExponentObject = PyNumber_Negative(Bias);
    if (MinExponentObject == NULL) {
        goto done;
    }
    Temporary = PyNumber_Add(MinExponentObject, One);
    Py_SETREF(MinExponentObject, Temporary);
    Temporary = NULL;
    if (MinExponentObject == NULL) {
        goto done;
    }

    if (isnan(FloatValue)) {
        Temporary = PowerOfTwo(ExponentWidth);
        if (Temporary == NULL) {
            goto done;
        }
        {
            PyObject *MinusOne = PyLong_FromLong(1);
            PyObject *ExponentMask;
            if (MinusOne == NULL) {
                goto done;
            }
            ExponentMask = PyNumber_Subtract(Temporary, MinusOne);
            Py_DECREF(MinusOne);
            Py_CLEAR(Temporary);
            if (ExponentMask == NULL) {
                goto done;
            }
            Temporary = ExponentMask;
        }
        EncodedExponent = ShiftObject(Temporary, FractionWidth, 1);
        Py_CLEAR(Temporary);
        EncodedFraction = PowerOfTwo(FractionWidth - 1);
        if (EncodedExponent == NULL || EncodedFraction == NULL) {
            goto done;
        }
        Temporary = PyNumber_Or(SignValue, EncodedExponent);
        if (Temporary == NULL) {
            goto done;
        }
        Result = PyNumber_Or(Temporary, EncodedFraction);
        goto done;
    }
    if (isinf(FloatValue)) {
        Temporary = PowerOfTwo(ExponentWidth);
        if (Temporary == NULL) {
            goto done;
        }
        {
            PyObject *MinusOne = PyLong_FromLong(1);
            PyObject *ExponentMask;
            if (MinusOne == NULL) {
                goto done;
            }
            ExponentMask = PyNumber_Subtract(Temporary, MinusOne);
            Py_DECREF(MinusOne);
            Py_CLEAR(Temporary);
            if (ExponentMask == NULL) {
                goto done;
            }
            Temporary = ExponentMask;
        }
        EncodedExponent = ShiftObject(Temporary, FractionWidth, 1);
        Py_CLEAR(Temporary);
        if (EncodedExponent == NULL) {
            goto done;
        }
        Result = PyNumber_Or(SignValue, EncodedExponent);
        goto done;
    }
    if (FloatValue == 0.0) {
        Result = Py_NewRef(SignValue);
        goto done;
    }

    Fraction = frexp(fabs(FloatValue), &BinaryExponent);
    Mantissa = (uint64_t)ldexp(Fraction, 53);
    Numerator = PyLong_FromUnsignedLongLong(Mantissa);
    Denominator = PyLong_FromLong(1);
    if (Numerator == NULL || Denominator == NULL) {
        goto done;
    }
    Temporary = PyLong_FromLong((long)(BinaryExponent - 1));
    if (Temporary == NULL) {
        goto done;
    }
    Comparison = PyObject_RichCompareBool(Temporary, MinExponentObject, Py_GE);
    Py_CLEAR(Temporary);
    if (Comparison < 0) {
        goto done;
    }

    if (Comparison) {
        Shift = FractionWidth - 52;
        if (Shift >= 0) {
            Temporary = PowerOfTwo(Shift);
            if (Temporary == NULL) {
                goto done;
            }
            Py_SETREF(Numerator, PyNumber_Multiply(Numerator, Temporary));
            Py_CLEAR(Temporary);
            if (Numerator == NULL) {
                goto done;
            }
        }
        else {
            Py_SETREF(Denominator, PowerOfTwo(-Shift));
            if (Denominator == NULL) {
                goto done;
            }
        }
        Rounded = RoundRatioObjects(Numerator, Denominator);
        if (Rounded == NULL) {
            goto done;
        }
        Temporary = PowerOfTwo(FractionWidth + 1);
        if (Temporary == NULL) {
            goto done;
        }
        Comparison = PyObject_RichCompareBool(Rounded, Temporary, Py_EQ);
        Py_CLEAR(Temporary);
        if (Comparison < 0) {
            goto done;
        }
        if (Comparison) {
            Py_SETREF(Rounded, PyNumber_Rshift(Rounded, One));
            if (Rounded == NULL) {
                goto done;
            }
            BinaryExponent++;
        }
        Temporary = PyLong_FromLong((long)(BinaryExponent - 1));
        if (Temporary == NULL) {
            goto done;
        }
        Comparison = PyObject_RichCompareBool(Temporary, MaximumExponent, Py_GT);
        if (Comparison < 0) {
            goto done;
        }
        if (Comparison) {
            PyErr_SetString(PyExc_OverflowError,
                            "Value exceeds the largest finite value of the target float format");
            goto done;
        }
        EncodedExponent = PyNumber_Add(Temporary, Bias);
        Py_CLEAR(Temporary);
        if (EncodedExponent == NULL) {
            goto done;
        }
        Temporary = PowerOfTwo(FractionWidth);
        if (Temporary == NULL) {
            goto done;
        }
        EncodedFraction = PyNumber_Subtract(Rounded, Temporary);
        Py_CLEAR(Temporary);
        if (EncodedFraction == NULL) {
            goto done;
        }
    }
    else {
        PyObject *ExponentForShift;
        PyObject *FloatExponent;
        Py_ssize_t SubnormalShift;

        FloatExponent = PyLong_FromLong((long)BinaryExponent);
        Temporary = PyLong_FromSsize_t(FractionWidth);
        if (Temporary == NULL) {
            Py_XDECREF(FloatExponent);
            goto done;
        }
        ExponentForShift = PyNumber_Add(Temporary, FloatExponent);
        Py_CLEAR(Temporary);
        Py_DECREF(FloatExponent);
        if (ExponentForShift == NULL) {
            goto done;
        }
        Temporary = PyNumber_Subtract(ExponentForShift, MinExponentObject);
        Py_DECREF(ExponentForShift);
        ExponentForShift = Temporary;
        Temporary = NULL;
        if (ExponentForShift == NULL) {
            goto done;
        }
        {
            PyObject *FiftyThree = PyLong_FromLong(53);
            if (FiftyThree == NULL) {
                Py_DECREF(ExponentForShift);
                goto done;
            }
            Temporary = PyNumber_Subtract(ExponentForShift, FiftyThree);
            Py_DECREF(FiftyThree);
        }
        Py_DECREF(ExponentForShift);
        ExponentForShift = Temporary;
        Temporary = NULL;
        if (ExponentForShift == NULL) {
            goto done;
        }
        SubnormalShift = PyLong_AsSsize_t(ExponentForShift);
        Py_DECREF(ExponentForShift);
        if (SubnormalShift == -1 && PyErr_Occurred()) {
            goto done;
        }
        if (SubnormalShift >= 0) {
            Temporary = PowerOfTwo(SubnormalShift);
            if (Temporary == NULL) {
                goto done;
            }
            Py_SETREF(Numerator, PyNumber_Multiply(Numerator, Temporary));
            Py_CLEAR(Temporary);
            if (Numerator == NULL) {
                goto done;
            }
        }
        else {
            Py_SETREF(Denominator, PowerOfTwo(-SubnormalShift));
            if (Denominator == NULL) {
                goto done;
            }
        }
        Rounded = RoundRatioObjects(Numerator, Denominator);
        if (Rounded == NULL) {
            goto done;
        }
        Comparison = PyObject_IsTrue(Rounded);
        if (Comparison < 0) {
            goto done;
        }
        if (!Comparison) {
            Result = Py_NewRef(SignValue);
            goto done;
        }
        Temporary = PowerOfTwo(FractionWidth);
        if (Temporary == NULL) {
            goto done;
        }
        Comparison = PyObject_RichCompareBool(Rounded, Temporary, Py_GE);
        Py_CLEAR(Temporary);
        if (Comparison < 0) {
            goto done;
        }
        if (Comparison) {
            EncodedExponent = PyLong_FromLong(1);
            EncodedFraction = PyLong_FromLong(0);
        }
        else {
            EncodedExponent = PyLong_FromLong(0);
            EncodedFraction = Py_NewRef(Rounded);
        }
        if (EncodedExponent == NULL || EncodedFraction == NULL) {
            goto done;
        }
    }

    Temporary = ShiftObject(EncodedExponent, FractionWidth, 1);
    if (Temporary == NULL) {
        goto done;
    }
    EncodedExponent = Temporary;
    Temporary = PyNumber_Or(EncodedExponent, EncodedFraction);
    if (Temporary == NULL) {
        goto done;
    }
    Result = PyNumber_Or(SignValue, Temporary);

done:
    Py_XDECREF(Numerator);
    Py_XDECREF(Denominator);
    Py_XDECREF(Rounded);
    Py_XDECREF(One);
    Py_XDECREF(Bias);
    Py_XDECREF(MaximumExponent);
    Py_XDECREF(MinExponentObject);
    Py_XDECREF(EncodedExponent);
    Py_XDECREF(EncodedFraction);
    Py_XDECREF(SignValue);
    Py_XDECREF(Temporary);
    return Result;
}

static PyObject *
FloatToBits(PyObject *Self, PyObject *Args, PyObject *Keywords)
{
    static char *Names[] = {"Value", "ExponentBits", "FractionBits", NULL};
    PyObject *Value;
    PyObject *ExponentBits;
    PyObject *FractionBits;
    if (!PyArg_ParseTupleAndKeywords(Args, Keywords, "OOO:FloatToBits", Names,
                                     &Value, &ExponentBits, &FractionBits)) {
        return NULL;
    }
    return FloatToBitsImpl(Value, ExponentBits, FractionBits);
}

static PyObject *
BitsToFloatImpl(PyObject *Value, PyObject *ExponentBits, PyObject *FractionBits)
{
    Py_ssize_t ExponentWidth;
    Py_ssize_t FractionWidth;
    Py_ssize_t TotalWidth;
    PyObject *Zero = NULL;
    PyObject *Limit = NULL;
    PyObject *Comparison = NULL;
    PyObject *One = NULL;
    PyObject *FractionMask = NULL;
    PyObject *ExponentMask = NULL;
    PyObject *FractionValue = NULL;
    PyObject *ExponentValue = NULL;
    PyObject *Bias = NULL;
    PyObject *SignShift = NULL;
    PyObject *SignBits = NULL;
    PyObject *Significand = NULL;
    PyObject *BinaryExponent = NULL;
    PyObject *Temporary = NULL;
    PyObject *Power = NULL;
    PyObject *ResultObject = NULL;
    PyObject *Two = NULL;
    double Sign;
    double SignificandDouble;
    double PowerDouble;
    double ResultDouble;
    int IsSpecial;
    int Truth;
    int CompareResult;

    if (ValidateFormat(ExponentBits, FractionBits, &ExponentWidth,
                       &FractionWidth, &TotalWidth) < 0) {
        return NULL;
    }
    if (RequireExactInteger(Value, "Value") == NULL) {
        return NULL;
    }
    Zero = PyLong_FromLong(0);
    One = PyLong_FromLong(1);
    if (Zero == NULL || One == NULL) {
        goto done;
    }
    Comparison = PyObject_RichCompare(Value, Zero, Py_LT);
    if (Comparison == NULL) {
        goto done;
    }
    Truth = PyObject_IsTrue(Comparison);
    if (Truth < 0) {
        goto done;
    }
    if (Truth) {
        PyErr_SetString(PyExc_ValueError, "Value must be a nonnegative integer");
        goto done;
    }
    Py_CLEAR(Comparison);
    Limit = PowerOfTwo(TotalWidth);
    if (Limit == NULL) {
        goto done;
    }
    Comparison = PyObject_RichCompare(Value, Limit, Py_GE);
    if (Comparison == NULL) {
        goto done;
    }
    Truth = PyObject_IsTrue(Comparison);
    if (Truth < 0) {
        goto done;
    }
    if (Truth) {
        PyErr_SetString(PyExc_ValueError, "Value contains bits outside the selected float format");
        goto done;
    }

    Temporary = PowerOfTwo(FractionWidth);
    if (Temporary == NULL) {
        goto done;
    }
    FractionMask = PyNumber_Subtract(Temporary, One);
    Py_CLEAR(Temporary);
    Temporary = PowerOfTwo(ExponentWidth);
    if (Temporary == NULL) {
        goto done;
    }
    ExponentMask = PyNumber_Subtract(Temporary, One);
    Py_CLEAR(Temporary);
    if (FractionMask == NULL || ExponentMask == NULL) {
        goto done;
    }
    FractionValue = PyNumber_And(Value, FractionMask);
    Temporary = ShiftObject(Value, FractionWidth, 0);
    if (Temporary == NULL) {
        goto done;
    }
    ExponentValue = PyNumber_And(Temporary, ExponentMask);
    Py_CLEAR(Temporary);
    if (FractionValue == NULL || ExponentValue == NULL) {
        goto done;
    }
    Temporary = PowerOfTwo(ExponentWidth - 1);
    if (Temporary == NULL) {
        goto done;
    }
    Bias = PyNumber_Subtract(Temporary, One);
    Py_CLEAR(Temporary);
    if (Bias == NULL) {
        goto done;
    }
    SignShift = PyLong_FromSsize_t(TotalWidth - 1);
    if (SignShift == NULL) {
        goto done;
    }
    SignBits = PyNumber_Rshift(Value, SignShift);
    if (SignBits == NULL) {
        goto done;
    }
    Truth = PyObject_IsTrue(SignBits);
    if (Truth < 0) {
        goto done;
    }
    Sign = Truth ? -1.0 : 1.0;
    CompareResult = PyObject_RichCompareBool(ExponentValue, ExponentMask, Py_EQ);
    if (CompareResult < 0) {
        goto done;
    }
    IsSpecial = CompareResult;
    if (IsSpecial) {
        Truth = PyObject_IsTrue(FractionValue);
        if (Truth < 0) {
            goto done;
        }
        if (Truth) {
            ResultObject = PyFloat_FromDouble(NAN);
            goto done;
        }
        ResultObject = PyFloat_FromDouble(Sign < 0.0 ? -INFINITY : INFINITY);
        goto done;
    }

    Truth = PyObject_IsTrue(ExponentValue);
    if (Truth < 0) {
        goto done;
    }
    if (!Truth) {
        Truth = PyObject_IsTrue(FractionValue);
        if (Truth < 0) {
            goto done;
        }
        if (!Truth) {
            ResultObject = PyFloat_FromDouble(Sign < 0.0 ? -0.0 : 0.0);
            goto done;
        }
        Significand = Py_NewRef(FractionValue);
        Temporary = PyNumber_Subtract(One, Bias);
        if (Temporary == NULL) {
            goto done;
        }
        BinaryExponent = PyNumber_Subtract(Temporary, FractionBits);
        Py_CLEAR(Temporary);
    }
    else {
        Temporary = ShiftObject(One, FractionWidth, 1);
        if (Temporary == NULL) {
            goto done;
        }
        Significand = PyNumber_Add(Temporary, FractionValue);
        Py_DECREF(Temporary);
        Temporary = PyNumber_Subtract(ExponentValue, Bias);
        if (Temporary == NULL) {
            goto done;
        }
        BinaryExponent = PyNumber_Subtract(Temporary, FractionBits);
        Py_CLEAR(Temporary);
    }
    if (Significand == NULL || BinaryExponent == NULL) {
        goto done;
    }
    SignificandDouble = PyLong_AsDouble(Significand);
    if (SignificandDouble == -1.0 && PyErr_Occurred()) {
        goto done;
    }
    Two = PyFloat_FromDouble(2.0);
    if (Two == NULL) {
        goto done;
    }
    Power = PyNumber_Power(Two, BinaryExponent, Py_None);
    if (Power == NULL) {
        goto done;
    }
    PowerDouble = PyFloat_AsDouble(Power);
    if (PowerDouble == -1.0 && PyErr_Occurred()) {
        goto done;
    }
    ResultDouble = Sign * SignificandDouble * PowerDouble;
    ResultObject = PyFloat_FromDouble(ResultDouble);

done:
    Py_XDECREF(Zero);
    Py_XDECREF(Limit);
    Py_XDECREF(Comparison);
    Py_XDECREF(One);
    Py_XDECREF(FractionMask);
    Py_XDECREF(ExponentMask);
    Py_XDECREF(FractionValue);
    Py_XDECREF(ExponentValue);
    Py_XDECREF(Bias);
    Py_XDECREF(SignShift);
    Py_XDECREF(SignBits);
    Py_XDECREF(Significand);
    Py_XDECREF(BinaryExponent);
    Py_XDECREF(Temporary);
    Py_XDECREF(Power);
    Py_XDECREF(Two);
    return ResultObject;
}

static PyObject *
BitsToFloat(PyObject *Self, PyObject *Args, PyObject *Keywords)
{
    static char *Names[] = {"Value", "ExponentBits", "FractionBits", NULL};
    PyObject *Value;
    PyObject *ExponentBits;
    PyObject *FractionBits;
    if (!PyArg_ParseTupleAndKeywords(Args, Keywords, "OOO:BitsToFloat", Names,
                                     &Value, &ExponentBits, &FractionBits)) {
        return NULL;
    }
    return BitsToFloatImpl(Value, ExponentBits, FractionBits);
}

static PyObject *
PackFloatImpl(PyObject *Value, PyObject *ExponentBits, PyObject *FractionBits,
              PyObject *Endian)
{
    Py_ssize_t ExponentWidth;
    Py_ssize_t FractionWidth;
    Py_ssize_t TotalWidth;
    PyObject *Bits;
    PyObject *Signed = Py_False;
    PyObject *BitCount;
    PyObject *Result;
    if (ValidateFormat(ExponentBits, FractionBits, &ExponentWidth,
                       &FractionWidth, &TotalWidth) < 0) {
        return NULL;
    }
    Bits = FloatToBitsImpl(Value, ExponentBits, FractionBits);
    if (Bits == NULL) {
        return NULL;
    }
    BitCount = PyLong_FromSsize_t(TotalWidth);
    if (BitCount == NULL) {
        Py_DECREF(Bits);
        return NULL;
    }
    Result = PackIntegerImpl(Bits, BitCount, Endian, Signed);
    Py_DECREF(Bits);
    Py_DECREF(BitCount);
    return Result;
}

static PyObject *
UnPackFloatImpl(PyObject *Value, PyObject *ExponentBits, PyObject *FractionBits,
                PyObject *Endian)
{
    Py_ssize_t ExponentWidth;
    Py_ssize_t FractionWidth;
    Py_ssize_t TotalWidth;
    PyObject *Bits;
    PyObject *BitCount;
    PyObject *Unsigned = Py_False;
    PyObject *Result;
    if (ValidateFormat(ExponentBits, FractionBits, &ExponentWidth,
                       &FractionWidth, &TotalWidth) < 0) {
        return NULL;
    }
    BitCount = PyLong_FromSsize_t(TotalWidth);
    if (BitCount == NULL) {
        return NULL;
    }
    Bits = UnPackIntegerImpl(Value, BitCount, Endian, Unsigned);
    Py_DECREF(BitCount);
    if (Bits == NULL) {
        return NULL;
    }
    Result = BitsToFloatImpl(Bits, ExponentBits, FractionBits);
    Py_DECREF(Bits);
    return Result;
}

static PyObject *
PackFloat(PyObject *Self, PyObject *Args, PyObject *Keywords)
{
    static char *Names[] = {"Value", "ExponentBits", "FractionBits", "Endian", NULL};
    PyObject *Value, *ExponentBits, *FractionBits, *Endian;
    if (!PyArg_ParseTupleAndKeywords(Args, Keywords, "OOOO:PackFloat", Names,
                                     &Value, &ExponentBits, &FractionBits, &Endian)) {
        return NULL;
    }
    return PackFloatImpl(Value, ExponentBits, FractionBits, Endian);
}

static PyObject *
UnPackFloat(PyObject *Self, PyObject *Args, PyObject *Keywords)
{
    static char *Names[] = {"Value", "ExponentBits", "FractionBits", "Endian", NULL};
    PyObject *Value, *ExponentBits, *FractionBits, *Endian;
    if (!PyArg_ParseTupleAndKeywords(Args, Keywords, "OOOO:UnPackFloat", Names,
                                     &Value, &ExponentBits, &FractionBits, &Endian)) {
        return NULL;
    }
    return UnPackFloatImpl(Value, ExponentBits, FractionBits, Endian);
}

static PyObject *
PackBoolImpl(PyObject *Value, PyObject *EndianObject)
{
    EndianInfo Endian;
    if (!PyBool_Check(Value)) {
        PyErr_SetString(PyExc_TypeError, "Value must be a bool");
        return NULL;
    }
    if (GetEndianInfo(EndianObject, &Endian) < 0) {
        return NULL;
    }
    return PyBytes_FromStringAndSize(Value == Py_True ? "\x01" : "\x00", 1);
}

static PyObject *
PackBoolsImpl(PyObject *Value, PyObject *EndianObject)
{
    EndianInfo Endian;
    Py_ssize_t Count;
    Py_ssize_t ByteCount;
    Py_ssize_t Index;
    unsigned char *Buffer = NULL;
    PyObject *Result = NULL;

    if (!PyList_CheckExact(Value)) {
        PyErr_SetString(PyExc_TypeError, "Value must be a list of bool values");
        return NULL;
    }
    Count = PyList_GET_SIZE(Value);
    for (Index = 0; Index < Count; Index++) {
        if (!PyBool_Check(PyList_GET_ITEM(Value, Index))) {
            PyErr_SetString(PyExc_TypeError, "Every item in Value must be a bool");
            return NULL;
        }
    }
    if (GetEndianInfo(EndianObject, &Endian) < 0) {
        return NULL;
    }
    ByteCount = Count / 8 + (Count % 8 != 0);
    if (ByteCount != 0) {
        Buffer = PyMem_Calloc((size_t)ByteCount, 1);
        if (Buffer == NULL) {
            return PyErr_NoMemory();
        }
    }
    for (Index = 0; Index < Count; Index++) {
        Py_ssize_t Group = Index / 8;
        int BitIndex = (int)(Index % 8);
        int Shift = Endian.LittleEndian ? BitIndex : 7 - BitIndex;
        Py_ssize_t ByteIndex = Endian.LittleEndian ? Group : ByteCount - 1 - Group;
        if (PyList_GET_ITEM(Value, Index) == Py_True) {
            Buffer[ByteIndex] |= (unsigned char)(1u << Shift);
        }
    }
    if (Endian.Dense && ByteCount > 1) {
        if (Endian.LittleEndian) {
            while (ByteCount > 1 && Buffer[ByteCount - 1] == 0) {
                ByteCount--;
            }
        }
        else {
            Py_ssize_t First = 0;
            while (ByteCount - First > 1 && Buffer[First] == 0) {
                First++;
            }
            if (First > 0) {
                memmove(Buffer, Buffer + First, (size_t)(ByteCount - First));
                ByteCount -= First;
            }
        }
    }
    Result = PyBytes_FromStringAndSize((const char *)Buffer, ByteCount);
    PyMem_Free(Buffer);
    return Result;
}

static PyObject *
UnPackBoolImpl(PyObject *Value, PyObject *EndianObject)
{
    EndianInfo Endian;
    const unsigned char *Bytes;
    if (!PyBytes_CheckExact(Value)) {
        PyErr_SetString(PyExc_TypeError, "Value must be bytes");
        return NULL;
    }
    if (GetEndianInfo(EndianObject, &Endian) < 0) {
        return NULL;
    }
    if (PyBytes_GET_SIZE(Value) != 1 ||
        (unsigned char)PyBytes_AS_STRING(Value)[0] > 1) {
        PyErr_SetString(PyExc_ValueError,
                        "Value must contain exactly one canonical bool byte");
        return NULL;
    }
    Bytes = (const unsigned char *)PyBytes_AS_STRING(Value);
    return PyBool_FromLong(Bytes[0] == 1);
}

static PyObject *
UnPackBoolsImpl(PyObject *Value, PyObject *EndianObject)
{
    EndianInfo Endian;
    const unsigned char *Bytes;
    Py_ssize_t ByteCount;
    Py_ssize_t Index;
    Py_ssize_t ResultCount;
    PyObject *Result;

    if (!PyBytes_CheckExact(Value)) {
        PyErr_SetString(PyExc_TypeError, "Value must be bytes");
        return NULL;
    }
    if (GetEndianInfo(EndianObject, &Endian) < 0) {
        return NULL;
    }
    ByteCount = PyBytes_GET_SIZE(Value);
    if (ByteCount > PY_SSIZE_T_MAX / 8) {
        PyErr_SetString(PyExc_OverflowError, "Value is too large");
        return NULL;
    }
    ResultCount = ByteCount * 8;
    Result = PyList_New(ResultCount);
    if (Result == NULL) {
        return NULL;
    }
    Bytes = (const unsigned char *)PyBytes_AS_STRING(Value);
    for (Index = 0; Index < ResultCount; Index++) {
        Py_ssize_t Group = Index / 8;
        int BitIndex = (int)(Index % 8);
        Py_ssize_t ByteIndex = Endian.LittleEndian ? Group : ByteCount - 1 - Group;
        int Shift = Endian.LittleEndian ? BitIndex : 7 - BitIndex;
        PyObject *Bit = PyBool_FromLong((Bytes[ByteIndex] >> Shift) & 1);
        if (Bit == NULL) {
            Py_DECREF(Result);
            return NULL;
        }
        if (PyList_SetItem(Result, Index, Bit) < 0) {
            Py_DECREF(Result);
            return NULL;
        }
    }
    return Result;
}

static PyObject *
PackPointerImpl(PyObject *Value, PyObject *Endian)
{
    PyObject *BitCount = PyLong_FromLong(64);
    PyObject *Unsigned = Py_False;
    PyObject *Result;
    if (BitCount == NULL) {
        return NULL;
    }
    Result = PackIntegerImpl(Value, BitCount, Endian, Unsigned);
    Py_DECREF(BitCount);
    return Result;
}

static PyObject *
UnPackPointerImpl(PyObject *Value, PyObject *Endian)
{
    PyObject *BitCount = PyLong_FromLong(64);
    PyObject *Unsigned = Py_False;
    PyObject *Result;
    if (BitCount == NULL) {
        return NULL;
    }
    Result = UnPackIntegerImpl(Value, BitCount, Endian, Unsigned);
    Py_DECREF(BitCount);
    return Result;
}

static int
ParseValueEndian(PyObject *Args, PyObject *Keywords, const char *FunctionName,
                 PyObject **Value, PyObject **Endian)
{
    static char *Names[] = {"Value", "Endian", NULL};
    if (!PyArg_ParseTupleAndKeywords(Args, Keywords, "O|O", Names, Value, Endian)) {
        return -1;
    }
    if (*Endian == NULL) {
        *Endian = PyUnicode_FromString("little");
        if (*Endian == NULL) {
            return -1;
        }
    } else {
        *Endian = Py_NewRef(*Endian);
    }
    return 0;
}

#define PACK_INT_WRAPPER(Name, Width, IsSigned)                              \
    static PyObject *Name(PyObject *Self, PyObject *Args, PyObject *Keywords) \
    {                                                                        \
        PyObject *Value;                                                     \
        PyObject *Endian = NULL;                                             \
        PyObject *BitCount;                                                  \
        PyObject *Signed;                                                    \
        PyObject *Result;                                                    \
        if (ParseValueEndian(Args, Keywords, #Name, &Value, &Endian) < 0) {   \
            return NULL;                                                     \
        }                                                                    \
        BitCount = PyLong_FromLong(Width);                                   \
        Signed = IsSigned ? Py_True : Py_False;                              \
        if (BitCount == NULL) {                                              \
            Py_DECREF(Endian);                                               \
            return NULL;                                                     \
        }                                                                    \
        Result = PackIntegerImpl(Value, BitCount, Endian, Signed);            \
        Py_DECREF(BitCount);                                                 \
        Py_DECREF(Endian);                                                   \
        return Result;                                                       \
    }

#define UNPACK_INT_WRAPPER(Name, Width, IsSigned)                            \
    static PyObject *Name(PyObject *Self, PyObject *Args, PyObject *Keywords) \
    {                                                                        \
        PyObject *Value;                                                     \
        PyObject *Endian = NULL;                                             \
        PyObject *BitCount;                                                  \
        PyObject *Signed;                                                    \
        PyObject *Result;                                                    \
        if (ParseValueEndian(Args, Keywords, #Name, &Value, &Endian) < 0) {   \
            return NULL;                                                     \
        }                                                                    \
        BitCount = PyLong_FromLong(Width);                                   \
        Signed = IsSigned ? Py_True : Py_False;                              \
        if (BitCount == NULL) {                                              \
            Py_DECREF(Endian);                                               \
            return NULL;                                                     \
        }                                                                    \
        Result = UnPackIntegerImpl(Value, BitCount, Endian, Signed);          \
        Py_DECREF(BitCount);                                                 \
        Py_DECREF(Endian);                                                   \
        return Result;                                                       \
    }

#define FLOAT_WRAPPER(Name, ExpWidth, FracWidth, IsPacking)                  \
    static PyObject *Name(PyObject *Self, PyObject *Args, PyObject *Keywords) \
    {                                                                        \
        PyObject *Value;                                                     \
        PyObject *Endian = NULL;                                             \
        PyObject *ExponentBits;                                              \
        PyObject *FractionBits;                                              \
        PyObject *Result;                                                    \
        if (ParseValueEndian(Args, Keywords, #Name, &Value, &Endian) < 0) {   \
            return NULL;                                                     \
        }                                                                    \
        ExponentBits = PyLong_FromLong(ExpWidth);                            \
        FractionBits = PyLong_FromLong(FracWidth);                           \
        if (ExponentBits == NULL || FractionBits == NULL) {                  \
            Py_XDECREF(ExponentBits);                                        \
            Py_XDECREF(FractionBits);                                        \
            Py_DECREF(Endian);                                               \
            return NULL;                                                     \
        }                                                                    \
        Result = IsPacking ?                                                 \
            PackFloatImpl(Value, ExponentBits, FractionBits, Endian) :       \
            UnPackFloatImpl(Value, ExponentBits, FractionBits, Endian);      \
        Py_DECREF(ExponentBits);                                             \
        Py_DECREF(FractionBits);                                             \
        Py_DECREF(Endian);                                                   \
        return Result;                                                       \
    }

PACK_INT_WRAPPER(PackInt8, 8, 1)
PACK_INT_WRAPPER(PackInt16, 16, 1)
PACK_INT_WRAPPER(PackInt32, 32, 1)
PACK_INT_WRAPPER(PackInt64, 64, 1)
PACK_INT_WRAPPER(PackUInt8, 8, 0)
PACK_INT_WRAPPER(PackUInt16, 16, 0)
PACK_INT_WRAPPER(PackUInt32, 32, 0)
PACK_INT_WRAPPER(PackUInt64, 64, 0)
UNPACK_INT_WRAPPER(UnPackInt8, 8, 1)
UNPACK_INT_WRAPPER(UnPackInt16, 16, 1)
UNPACK_INT_WRAPPER(UnPackInt32, 32, 1)
UNPACK_INT_WRAPPER(UnPackInt64, 64, 1)
UNPACK_INT_WRAPPER(UnPackUInt8, 8, 0)
UNPACK_INT_WRAPPER(UnPackUInt16, 16, 0)
UNPACK_INT_WRAPPER(UnPackUInt32, 32, 0)
UNPACK_INT_WRAPPER(UnPackUInt64, 64, 0)
FLOAT_WRAPPER(PackFloat16, 5, 10, 1)
FLOAT_WRAPPER(PackFloat32, 8, 23, 1)
FLOAT_WRAPPER(PackFloat64, 11, 52, 1)
FLOAT_WRAPPER(UnPackFloat16, 5, 10, 0)
FLOAT_WRAPPER(UnPackFloat32, 8, 23, 0)
FLOAT_WRAPPER(UnPackFloat64, 11, 52, 0)

static PyObject *
PackBool(PyObject *Self, PyObject *Args, PyObject *Keywords)
{
    PyObject *Value, *Endian = NULL, *Result;
    if (ParseValueEndian(Args, Keywords, "PackBool", &Value, &Endian) < 0) {
        return NULL;
    }
    Result = PackBoolImpl(Value, Endian);
    Py_DECREF(Endian);
    return Result;
}

static PyObject *
PackBools(PyObject *Self, PyObject *Args, PyObject *Keywords)
{
    PyObject *Value, *Endian = NULL, *Result;
    if (ParseValueEndian(Args, Keywords, "PackBools", &Value, &Endian) < 0) {
        return NULL;
    }
    Result = PackBoolsImpl(Value, Endian);
    Py_DECREF(Endian);
    return Result;
}

static PyObject *
PackPointer(PyObject *Self, PyObject *Args, PyObject *Keywords)
{
    PyObject *Value, *Endian = NULL, *Result;
    if (ParseValueEndian(Args, Keywords, "PackPointer", &Value, &Endian) < 0) {
        return NULL;
    }
    Result = PackPointerImpl(Value, Endian);
    Py_DECREF(Endian);
    return Result;
}

static PyObject *
UnPackBool(PyObject *Self, PyObject *Args, PyObject *Keywords)
{
    PyObject *Value, *Endian = NULL, *Result;
    if (ParseValueEndian(Args, Keywords, "UnPackBool", &Value, &Endian) < 0) {
        return NULL;
    }
    Result = UnPackBoolImpl(Value, Endian);
    Py_DECREF(Endian);
    return Result;
}

static PyObject *
UnPackBools(PyObject *Self, PyObject *Args, PyObject *Keywords)
{
    PyObject *Value, *Endian = NULL, *Result;
    if (ParseValueEndian(Args, Keywords, "UnPackBools", &Value, &Endian) < 0) {
        return NULL;
    }
    Result = UnPackBoolsImpl(Value, Endian);
    Py_DECREF(Endian);
    return Result;
}

static PyObject *
UnPackPointer(PyObject *Self, PyObject *Args, PyObject *Keywords)
{
    PyObject *Value, *Endian = NULL, *Result;
    if (ParseValueEndian(Args, Keywords, "UnPackPointer", &Value, &Endian) < 0) {
        return NULL;
    }
    Result = UnPackPointerImpl(Value, Endian);
    Py_DECREF(Endian);
    return Result;
}

#define KEYWORD_METHOD(Name) {#Name, (PyCFunction)(void(*)(void))Name, METH_VARARGS | METH_KEYWORDS, NULL}

static PyMethodDef PackingMethods[] = {
    KEYWORD_METHOD(ValidateEndian),
    KEYWORD_METHOD(RoundRatio),
    KEYWORD_METHOD(ValidateFloatFormat),
    KEYWORD_METHOD(PackInteger),
    KEYWORD_METHOD(UnPackInteger),
    KEYWORD_METHOD(FloatToBits),
    KEYWORD_METHOD(BitsToFloat),
    KEYWORD_METHOD(PackFloat),
    KEYWORD_METHOD(UnPackFloat),
    KEYWORD_METHOD(PackInt8),
    KEYWORD_METHOD(PackInt16),
    KEYWORD_METHOD(PackInt32),
    KEYWORD_METHOD(PackInt64),
    KEYWORD_METHOD(PackUInt8),
    KEYWORD_METHOD(PackUInt16),
    KEYWORD_METHOD(PackUInt32),
    KEYWORD_METHOD(PackUInt64),
    KEYWORD_METHOD(PackFloat16),
    KEYWORD_METHOD(PackFloat32),
    KEYWORD_METHOD(PackFloat64),
    KEYWORD_METHOD(PackBool),
    KEYWORD_METHOD(PackBools),
    KEYWORD_METHOD(PackPointer),
    KEYWORD_METHOD(UnPackInt8),
    KEYWORD_METHOD(UnPackInt16),
    KEYWORD_METHOD(UnPackInt32),
    KEYWORD_METHOD(UnPackInt64),
    KEYWORD_METHOD(UnPackUInt8),
    KEYWORD_METHOD(UnPackUInt16),
    KEYWORD_METHOD(UnPackUInt32),
    KEYWORD_METHOD(UnPackUInt64),
    KEYWORD_METHOD(UnPackFloat16),
    KEYWORD_METHOD(UnPackFloat32),
    KEYWORD_METHOD(UnPackFloat64),
    KEYWORD_METHOD(UnPackBool),
    KEYWORD_METHOD(UnPackBools),
    KEYWORD_METHOD(UnPackPointer),
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef PackingModule = {
    PyModuleDef_HEAD_INIT,
    "Packing",
    "Native binary packing and IEEE-754 conversion helpers.",
    -1,
    PackingMethods
};

PyMODINIT_FUNC
PyInit_Packing(void)
{
    return PyModule_Create(&PackingModule);
}
