# Constant arrays

`name := [..]` declares a **constant array**: fixed length, read-only, stored as
packed data in the program image, like a C `static const T xs[]`. There is no
header and no allocation. It works at the top level and inside functions.

```km
import console

primes := [2, 3, 5, 7, 11]
weights: [float] := [1.5, 2, -0.25]
bytes: [u8] := [0, 1, 255]

void main()
    console.print(primes[4])
    console.print(#primes)
    for w in weights do
        console.print(w)
    console.print(bytes[2])
```

```text
11
5
1.5
2.0
-0.25
255
```

## Element types

The element type is the `T` of an explicit `name: [T] := ..`, otherwise it is taken
from the first element (`int`, `float`, `bool`, `string` or `char`). Each element
is packed at its natural width:

| Type | Bytes | Type | Bytes |
|---|---|---|---|
| `u8` `i8` `bool` `char` | 1 | `int` `i64` `u64` `float` `f64` `string` | 8 |
| `u16` `i16` | 2 | `u32` `i32` `f32` | 4 |

Reading a sized integer widens it to a 64-bit `int` (sign-extended for `i8`..`i32`,
zero-extended for `u8`..`u32`); `f32` widens to `float`. A `char` element is read
back as the one-character string.

Every element must be a literal (a leading `-` is allowed). Nested constant arrays
and arrays of structs are not supported yet.

## What you can do with one

Only three things, because the layout differs from an ordinary `[T]`:

- index it: `xs[i]` (a bounds check against the constant length runs at every read)
- measure it: `#xs`
- iterate it: `for x in xs do`

Passing it to a function, assigning it, or slicing it is a compile error; copy the
elements into an ordinary array first if you need one:

<!-- doc-skip -->
```km
let copy: [int] = []
for p in primes do
    copy += [p]
```

## Limits

- x86 target only for now (`-t c`, `-t wasm` and `-t stackvm` report an error).
- Names must be unique among constant arrays in a program.
- An out-of-range literal for a narrow element type (for example `256` in a `[u8]`)
  is not diagnosed yet.
