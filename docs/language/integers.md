# Integers and `f32`

Komodo has the fixed-width integers `u8 u16 u32 u64 i8 i16 i32 i64`, plus
`int` (the same as `i64`) and `uint` (the same as `u64`). Arithmetic wraps
modulo 2^N, so a value always stays inside its type.

| Type | Range |
|---|---|
| `u8` | 0 .. 255 |
| `u16` | 0 .. 65535 |
| `u32` | 0 .. 4294967295 |
| `u64`, `uint` | 0 .. 18446744073709551615 |
| `i8` | -128 .. 127 |
| `i16` | -32768 .. 32767 |
| `i32` | -2147483648 .. 2147483647 |
| `i64`, `int` | -9223372036854775808 .. 9223372036854775807 |

## Wrap-around

`+ - * << /` and unary `-` wrap to the width of the type. `u64` compares,
divides, takes `%`, and shifts right as an unsigned number; the narrower
unsigned types and the signed types behave like their C `stdint` namesakes.

```km
void main()
    let a: u8 = 250
    print(a + 10)
    let b: i8 = 127
    b += 1
    print(b)
    let c: u32 = 0
    print(c - 1)
    let d: u64 = 0
    print(d - 1)
    print(d - 1 > 5)
    let e: i32 = 2147483647
    print(e + 1)
```

```text
4
-128
4294967295
18446744073709551615
1
-2147483648
```

## Conversions

Widening is implicit when every value fits: `u8` to any wider type, `i8` to
any wider signed type, `u32` to `i64`/`int`, and so on. Narrowing and changing
sign need an explicit conversion, written like `int(x)`: `u8(x)`, `i32(x)`,
`u64(x)`, ... A conversion truncates (wraps). A float is truncated toward zero
first.

```km
void main()
    let big: int = 300
    print(u8(big))
    print(i8(200))
    print(u32(-1))
    print(u8(3.9))
    let w: u8 = 200
    let wide: u32 = w
    print(wide + 4000000000)
```

```text
44
-56
4294967295
3
4000000200
```

A binary operation joins its operands: equal types stay, a lossless widening
picks the wider type (`u8 + u32` is `u32`), and an integer literal takes the
other operand's type. Mixing signed and unsigned of the same width, or two
types where neither fits the other, is an error that asks for a conversion.

<!-- doc-error -->
```km
void main()
    let a: u32 = 5
    let b: i32 = 7
    print(a + b)
```

```text
error: cannot mix 'u32' and 'i32'
```

## Literals

A literal must fit the type it is given:

<!-- doc-error -->
```km
void main()
    let a: u8 = 300
    print(a)
```

```text
error: literal 300 does not fit in u8
```

Assigning a wider integer needs a conversion:

<!-- doc-error -->
```km
void main()
    let n: int = 70000
    let s: u16 = n
    print(s)
```

```text
error: initialiser for 's': type 'int' is not assignable to 'u16' — convert explicitly with u16(x)
```

## Printing

`print` and `string(x)` show a `u64` with its full unsigned value, and every
other integer as a plain decimal number.

```km
void main()
    let m: u64 = 18446744073709551615
    print(m)
    print(string(m / 3))
    let h: u16 = 65535
    print(h + 1)
```

```text
18446744073709551615
6148914691236517205
0
```

## `f32`

`f32` is IEEE single precision. Each `f32` operation rounds to 32 bits, and a
float literal combined with an `f32` is itself an `f32`. A plain `float`
operand makes the result `float`. `f32` prints as the shortest text that reads
back as the same single-precision value, keeping the `.0` on whole numbers.

```km
void main()
    let s: f32 = 0.0
    let i = 0
    while i < 1000 do
        s += 0.1
        i += 1
    print(s)
    let x: f32 = 16777216.0
    print(x + 1.0)
    print(f32(1) / f32(3))
```

```text
99.99905
16777216.0
0.33333334
```

## Storage

Every value still lives in an 8-byte slot (locals, fields, array elements);
narrow values are always kept normalised in it (zero-extended for unsigned,
sign-extended for signed). Packed storage for arrays and fields is a later
phase. Across the C FFI, narrow arguments and returns marshal with their
declared width.

## Notes

- Shifting by a count at or beyond the width of the type is not defined.
- The stackvm target does not implement the narrow-integer operations and
  reports a clear error for them.
