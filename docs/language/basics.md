# Komodo language basics

A tour of everyday Komodo: program structure, variables, types, operators,
control flow, functions, modules, built-ins, the standard library and the
analyser's diagnostics. Every example below is a complete program that was
compiled and run with the `komodo` CLI in this repository; the output shown
is the real output.

For the full reference, see [`reference.md`](reference.md).

## Contents

- [Program structure](#program-structure)
- [Comments](#comments)
- [Blocks and indentation](#blocks-and-indentation)
- [Variables](#variables)
- [Primitive types](#primitive-types)
- [Literals](#literals)
- [Operators](#operators)
- [String interpolation](#string-interpolation)
- [Slices](#slices)
- [Arrays](#arrays)
- [Control flow](#control-flow)
- [Functions](#functions)
- [Top-level globals](#top-level-globals)
- [Macros](#macros)
- [Modules and imports](#modules-and-imports)
- [Built-ins](#built-ins)
- [Standard library](#standard-library)
- [Diagnostics](#diagnostics)
- [Differences from the reference](#differences-from-the-reference)

## Program structure

A program is a `.km` file with a `void main()` function. That is the entry
point. `print` is a built-in and needs no `import`.

```km
void main()
    print("Hello, Komodo!")
```

```text
Hello, Komodo!
```

Compile, link and run a file with the `komodo` CLI:

```bash
komodo hello.km          # compile, link and run
komodo -c hello.km       # compile and link only
komodo -q hello.km       # run without the compiler's progress lines
```

`main` may take the command-line arguments as `void main(args)`. `args` is a
string array: `args[0]` is the program path and `args[1]` is the first user
argument. `#args` is the count.

<!-- doc-skip -->
```km
void main(args)
    print("program: " + args[0])
    print("first user argument: " + args[1])
```

```bash
./args_demo hello
```

```text
program: /path/to/args_demo
first user argument: hello
```

## Comments

```km
// A line comment runs to the end of the line.
/* A block comment
   can span several lines. */

/** A doc comment. */
void main()
    print("comments are ignored") // trailing comment
```

```text
comments are ignored
```

## Blocks and indentation

Blocks are indentation-based. An indented line opens a block and a dedent
closes it. Newlines are significant.

`if`, `while` and `for` take their block on the following indented lines.
`then` and `do` are optional keywords that may follow the condition. A
one-line form, `if cond then statement`, is also allowed, but it cannot have
an `else`.

```km
void main()
    let n = 7
    if n > 5 then
        print("big")
    else
        print("small")
    if n % 2 == 0 then print("even")
    if n % 2 != 0 then print("odd")
```

```text
big
odd
```

An `else` on the same line as a one-line `if` is a parse error:

```km
void main()
    let n = 7
    if n % 2 == 0 then print("even") else print("odd")
```

```text
example.km:3:38: error: unexpected 'else': a one-line `if c then X else Y` is not supported; write the branches on separate indented lines
```

## Variables

`let` declares a variable. Its type is inferred from the initialiser unless
you write one.

```km
void main()
    let count = 0              // reassignable, inferred as int
    count = count + 1
    let name: string = "Ada"   // explicit type
    let limit be 10            // immutable: `be` declares a macro (see Macros)
    print(count, name, limit)
```

```text
1 Ada 10
```

Assigning to a `be` name is rejected:

```km
void main()
    let limit be 10
    limit = 11
```

```text
example.km:3:5: error: cannot assign to macro 'limit' (declared with be)
```

A `let` with `be` may also carry a type: `let limit: int be 10`.

## Primitive types

| Type     | What it holds | Example |
|----------|---------------|---------|
| `int`    | 64-bit signed integer | `-42` |
| `uint`   | unsigned 64-bit integer (same as `u64`); `u8`..`u64` and `i8`..`i64` are the fixed widths, see [Integers](integers.md) | `7` |
| `float`  | IEEE-754 double | `3.14` |
| `f32`    | single-precision float; every `f32` result is rounded to single precision | `2.5` |
| `bool`   | truth value; prints as `1` or `0` | `true` |
| `char`   | one character | `'K'` |
| `string` | text | `"Komodo"` |

`f32` and `float` mix freely (a plain `float` operand makes the result `float`). Nullable types are written `T?`, for example
`int?` or `string?`, and the value `null` fills them.

```km
void main()
    let i: int = -42
    let u: uint = 7
    let f: float = 3.14
    let g: f32 = 2.5
    let b: bool = true
    let c: char = 'K'
    let s: string = "Komodo"
    print(i, u, f, g, b, c, s)
    print(string(i) + "!", 2.0, 2.0f)
```

```text
-42 7 3.14 2.5 1 K Komodo
-42! 2.0 2.0
```

A `float` prints in `%g` form and keeps a trailing `.0` when it is whole, so
`2.0` prints as `2.0`. Write a float literal as `2.0` or `2.0f`.

## Literals

| Kind | Forms |
|------|-------|
| int | `42`, `0xFF` (hex), `0b1010` (binary), `1_000_000` (`_` separators in any base) |
| float | `2.0`, `3.14`, `2.0f` |
| string | `"a\nb"`, escapes `\n \t \\ \"` |
| char | `'a'`, `'\n'` |
| bool | `true`, `false` |
| null | `null` |
| array | `[]`, `[4, 15, 8]` |

```km
void main()
    print(0xFF, 0b1010, 1_000_000, 0xFF_FF)
    print(0xff + 1, 0b1111_0000)
    print("tab:\tend", "quote:\" backslash:\\ newline-next")
    print("line1\nline2")
    print('\n' == '\n', 'a')
```

```text
255 10 1000000 65535
256 240
tab:	end quote:" backslash:\ newline-next
line1
line2
1 a
```

## Operators

Precedence, lowest to highest: `or`, `and`, `is` / `in`, comparison, bitwise,
additive, multiplicative, unary, postfix.

| Group | Operators |
|-------|-----------|
| Arithmetic | `+ - * / %` |
| Comparison | `== != < <= > >=` |
| Logical | `and`, `or` (short-circuit), `not` or `!` |
| Bitwise | `& \| ^ << >>`; `xor` is an alias for `^` |
| Membership | `x in coll` |
| Type test | `x is Type` |
| Compound assignment | `+= -= *= /= %=` |

Booleans print as `1` and `0`. Integer `/` truncates toward zero.

```km
void main()
    print(7 / 2, 7 % 3, -7 / 2, 2 * 3 + 4)
    print(1.0 / 4.0, 5 < 6, 5 == 6, 5 != 6)
    print(6 & 3, 6 | 3, 6 ^ 3, 6 xor 3, 1 << 4, 256 >> 2)
    print(not true, true and false, false or true, not false)
    print(3 in [1, 2, 3], "ell" in "hello", 'h' in "hello")
    let n = 0
    n += 5
    n *= 3
    n -= 1
    n /= 2
    n %= 5
    print(n)
```

```text
3 1 -3 10
0.25 1 0 1
2 7 5 5 16 64
0 0 1 1
1 1 1
2
```

`x in coll` tests a string for a substring when `coll` is a string, and an
array for an element when `coll` is an array.

`cond and a or b` is the conditional value. It is built from the
short-circuit `and` and `or`. There is no `?:` operator.

Compound assignment works on a name, a field (`obj.f += 1`) and an array
element (`arr[i] *= 2`). `+=` also appends to arrays and concatenates strings.

Type tests with `is` check a value's runtime type. This works on struct
values and their base types:

```km
abstract struct Shape
    tag: int

struct Circle : Shape
    r: int

struct Square : Shape
    side: int

fn describe(s: Shape): string
    if s is Circle then
        return "circle"
    return "other"

void main()
    let c = new Circle(0, 5)
    print(c is Circle, c is Square, describe(c))
```

```text
1 0 circle
```

## String interpolation

Put a string in backticks and write expressions inside `{ }`. Each
expression is converted with `string(...)`.

```km
void main()
    let n = 3
    let who = "world"
    print(`hello {who}, next is {n + 1}`)
    print(`big? {n > 2 and "yes" or "no"}`)
    print(`literal \{braces\} and a \`tick\``)
```

```text
hello world, next is 4
big? yes
literal {braces} and a `tick`
```

There is no format-spec syntax. A float prints in `%g` form, so round it
first if you need fixed decimals.

## Slices

`a[i to j]` gives the elements from `i` up to but not including `j`. Either
bound may be left out, as in `a[2:]` and `a[:5]`. The same forms work on
strings and arrays.

```km
void main()
    let s = "hello, world"
    print(s[0 to 5])
    print(s[7 to #s])
    print(s[7:])
    print(s[:5])
    let xs = [10, 20, 30, 40, 50]
    print(xs[1 to 4], xs[2:])
```

```text
hello
world
world
hello
[20, 30, 40] [30, 40, 50]
```

## Arrays

An array literal is written `[a, b, c]`. `#a` is the length. Append with
`+=`.

```km
void main()
    let xs = [4, 15, 8]
    print(#xs, xs[0], xs[#xs - 1])
    let empty: [int] = []
    empty += [1]
    empty += [2, 3]
    print(#empty, empty)
    let names = ["ada", "bo"]
    names += ["cy"]
    print(names, "bo" in names)
```

```text
3 4 8
3 [1, 2, 3]
["ada", "bo", "cy"] 1
```

Indexing out of range throws an error. Check `#a` first, or use a protected
call (see [Built-ins](#built-ins)).

## Control flow

### if, while, for

```km
void main()
    let i = 0
    while i < 3 do
        print("while", i)
        i += 1
    for k = 1 to 3
        print("to", k)
    for k = 3 down to 1
        print("down", k)
    for k = 0 until 3
        print("until", k)
    for x in [5, 6]
        print("each", x)
    for x in [5, 6, 7, 8] do
        if x == 6 then continue
        if x == 8 then break
        print("kept", x)
```

```text
while 0
while 1
while 2
to 1
to 2
to 3
down 3
down 2
down 1
until 0
until 1
until 2
each 5
each 6
kept 5
kept 7
```

- `for i = a to b` counts up and includes `b`.
- `for i = a down to b` counts down and includes `b`.
- `for i = a until b` counts up and excludes `b`.
- `for x in xs` loops over the elements of an array.
- `break` and `continue` work in all loops.

A range such as `1 to 4` is only valid in a `for` header or a slice. Writing
`let r = 1 to 4` is a parse error.

### match

`match` compares a value against each `case`. An arm's body is indented
under its `case`. `else` is the fallback.

```km
void main()
    let word = "beta"
    match word
        case "alpha"
            print("first")
        case "beta"
            print("second")
        else
            print("other")
```

```text
second
```

`case Type(field: value, name)` also matches struct values. See
`tests/examples/pattern_demo.km`.

## Functions

```km
fn square(x: int): int
    return x * x

fn add(a: int, b: int): int
    return a + b

mut counter(): int
    return 1

void show(label: string, v: int)
    print(label, v)

void main()
    show("square", square(4))
    show("add", add(2, 3))
    print(counter())
```

```text
square 16
add 5
1
```

The three kinds of function differ in what they promise:

- `fn name(...): T` returns a value and must not change anything the caller
  can see. Writing a field or an array element inside it is a compile error.
- `mut name(...): T` may change its receiver's fields or array elements.
- `void name(...)` returns nothing. It is a procedure and is not constrained.

`return expr` returns a value. A bare `return` leaves a `void` function.
A value-returning function must return on every path.

### Default parameter values

A trailing parameter may have a default. The default must be a literal, and
the caller may leave the parameter out.

```km
fn greet(name: string, greeting: string = "hello"): string
    return greeting + ", " + name

fn scale(v: int, k: int = 2, offset: int = -1): int
    return v * k + offset

void main()
    print(greet("ann"))
    print(greet("bob", "hi"))
    print(scale(5), scale(5, 3))
```

```text
hello, ann
hi, bob
9 14
```

Defaults apply to free functions only. An overloaded name cannot take
defaults.

### Variadic parameters

A trailing `...` makes a function accept any number of extra arguments.
`console.print` is declared this way in the standard library, so you use it
like this:

```km
import console

void main()
    console.print("sum:", 1, 2, 3)
    console.write("no newline")
    console.write("\n")
```

```text
sum: 1 2 3
no newline
```

A function with a body cannot read its extra arguments. The `...` form is
meant for declared prototypes such as the standard library's.

Free functions may also be overloaded on their parameter types. See
`tests/examples/overload_demo.km`.

## Top-level globals

`let NAME = EXPR` at module top level declares a global variable. It is
shared by every function, and it is assigned once, in source order, before
`main` runs.

```km
import console

let counter = 0
let label: string = "hits"
let log: [int] = []

fn hit(): int
    counter = counter + 1
    log += [counter]
    return counter

void main()
    hit()
    hit()
    counter += 10
    console.print(label, counter, log)
```

```text
hits 12 [1, 2]
```

`let NAME be EXPR` with `be` is a macro instead. See the next section.

## Macros

`let NAME be EXPR` declares a compile-time macro. Each use of `NAME` is
replaced by `EXPR`, so no storage is allocated, and `NAME` cannot be
reassigned. A macro is block-scoped. An inner declaration shadows an outer
one until the end of its block.

```km
let WIDTH be 4
let HEIGHT be 3
let AREA be WIDTH * HEIGHT

void main()
    print("area:", AREA)
    let N be 10
    if WIDTH > 0
        let N be 99
        print("inner:", N)
    print("outer:", N)
```

```text
area: 12
inner: 99
outer: 10
```

Keep macro bodies to constants and pure expressions. A body that reads a
variable sees that variable's current value at each use.

## Modules and imports

A module file (`.kmm`) begins with `module <name>`. Members are reached with
`name::member`. A module may split its members into `public` and `private`
sections. A `private` member can be called from inside its own module but
not from another one.

<!-- doc-skip -->
```km
module greet

public
    fn hello(who: string): string
        return banner() + who + "!"

private
    fn banner(): string
        return "Hello, "
```

```km
import console
import greet

void main()
    console.print(greet::hello("world"))
```

```text
Hello, world!
```

Calling the private member from outside is an error:

```km
import console
import greet

void main()
    console.print(greet::banner())
```

```text
example.km:5:19: error: 'banner' is private to module 'greet'
```

`import name` loads `name.kmm`. The compiler looks in the importing file's
directory tree first, then in its own directory, where the standard library
lives. Every file that declares the same `module` name is merged into one
namespace.

## Built-ins

These names need no `import`:

| Name | What it does |
|------|--------------|
| `print(...)` | writes its arguments separated by spaces, then a newline |
| `string(x)` | converts any value to text |
| `int(x)` | converts a float to an integer, truncating toward zero |
| `join(parts, sep)` | joins a `[string]` with `sep` between the parts |
| `assert(cond)` / `assert(cond, "message")` | throws if `cond` is false |
| `copy(x)` | deep copy of a struct, class or array |
| `data(arr)` | element pointer of an array, for passing a buffer to C; see `tests/examples/ffi_struct.km` |

```km
import console

void main()
    print(int(3.99), int(-2.5), int(7))
    print(string(2.0), string(true), string(42))
    print(string(1.5) + "x", "pi~" + string(3))
    let s = join(["a", "b", "c"], ", ")
    print(s)
    assert(1 + 1 == 2)
    assert(true, "never shown")
    print("done")
```

```text
3 -2 7
2.0 true 42
1.5x pi~3
a, b, c
done
```

A failing `assert` throws. Its message gives the file and line:

```km
fn half(n: int): int
    assert(n % 2 == 0, "half needs an even number")
    return n / 2

void main()
    print(half(10))
    print(half(7))
```

```text
5
Uncaught error: example.km:2: assertion failed: half needs an even number
```

`copy(x)` makes an independent copy. Changing the copy leaves the original
alone:

```km
struct Pixel
    x: int = 0
    y: int = 0

void main()
    let a = new Pixel(1, 2)
    let b = copy(a)
    b.x = 99
    print(a.x, b.x)
    let xs = [1, 2]
    let ys = copy(xs)
    ys[0] = 50
    print(xs[0], ys[0])
```

```text
1 99
1 50
```

### Errors: throw and protected calls

`throw "message"` raises an error. A call written with a trailing `?` catches
the error and evaluates to `null` instead. Test the result for `null`.

```km
fn check_port(n: int): int
    if n < 1 or n > 65535 then
        throw "port out of range"
    return n

void main()
    let port = check_port(70000)?
    if port == null then
        print("bad port, using 8080")
    else
        print("port", port)
    print(check_port(80)?)
```

```text
bad port, using 8080
80
```

An error that is not caught stops the program:

```km
void main()
    let n = 0
    if n == 0 then
        throw "n must not be zero"
    print("unreachable")
```

```text
Uncaught error: n must not be zero
```

The program exits with status 1.

## Standard library

The standard library is made of ordinary modules. Import them by name. Each
module's functions are declared in `stdlib/<name>.kmm` and implemented in
`stdlib/<name>.c`.

| Module | Main members |
|--------|--------------|
| `console` | `print`, `write`, `error` (all variadic). `print` goes to stdout with a newline; `write` goes to stdout without one; `error` goes to stderr |
| `math` | ints: `abs`, `min`, `max`, `pow`, `gcd`, `clamp`, `sign`. floats: `sqrt`, `floor`, `ceil`, `round`, `trunc`, `fabs`, `sin`, `cos`, `tan`, `atan`, `atan2`, `exp`, `log`, `log2`, `log10`, `powf`, `hypot`. constants: `pi()`, `e()` |
| `string` | `length`, `split`, `join`, `trim`, `trim_start`, `trim_end`, `replace`, `index_of`, `contains`, `starts_with`, `ends_with`, `to_upper`, `to_lower`, `repeat`, `reverse`, `substring`, `pad_start`, `pad_end` |
| `algorithm` | `sort`, `reverse`, `sum`, `max`, `min`, `index_of`, `contains`, `fill`. Each works on `[int]` and on `[float]` |
| `file` | `exists`, `is_dir`, `read_all`, `read_lines`, `write_all`, `append_all`, `find`, `list_dir` |
| `os` | `args(): [string]` (argv, argv[0] included), `getenv(name): string?` (null when unset), `setenv(name, value)`, `exit(code)`, `cwd(): string`, `pid(): int`, `platform(): string` (`linux`, `darwin`, `windows`, `wasm`) |
| `time` | `now(): int` (unix seconds), `millis(): int` (monotonic milliseconds), `sleep_ms(ms)`, `format(secs: int, fmt: string): string` (strftime, UTC) |
| `random` | `seed(n)`, `int(lo, hi): int` (inclusive), `float(): float` in [0, 1), `shuffle(xs: [int])` in place. splitmix64: a seed gives the same sequence on every platform |
| `path` | `join(a, b)`, `dirname(p)`, `basename(p)`, `ext(p)` (with the dot), `abs(p)` (lexical, against the cwd), `exists(p): int` (any file or directory) |
| `process` | `run(cmd): int` (exit status, stdio inherited; 128+n for a signal, -1 if it cannot start), `output(cmd): string` (captured stdout). Run through `sh -c`; no-ops on wasm (`-1` and `""`) |
| `sdl2`, `raylib` | C library bindings; see `stdlib/sdl2.kmm` and `stdlib/raylib.kmm` |

Calls to `math`, `string` and `algorithm` use a dot, as in `math.abs(x)`,
`string.split(s, ",")` and `algorithm.sort(xs)`. Calls to `file` use the
scope operator, as in `file::read_all(path)`. Calls to `console` work either
way.

```km
import math
import algorithm
import string
import console

void main()
    console.print(math.abs(-4), math.max(3, 9), math.pow(2, 10), math.gcd(12, 18))
    console.print(math.sqrt(16.0), math.floor(2.7), math.pi())
    let xs = [3, 1, 2]
    algorithm.sort(xs)
    console.print(xs, algorithm.sum(xs), algorithm.max(xs))
    let fs = [2.5, 0.5]
    console.print(algorithm.min(fs), algorithm.contains(xs, 2))
    console.print(string.split("a,b,c", ","), string.to_upper("abc"))
    console.print(string.pad_start("7", 3, "0"), string.length("four"))
```

```text
4 9 1024 6
4.0 2.0 3.14159
[1, 2, 3] 6 3
0.5 1
["a", "b", "c"] ABC
007 4
```

File access uses `::`:

```km
import file
import console

void main()
    file::write_all("/tmp/komodo-basics-demo.txt", "one\n")
    file::append_all("/tmp/komodo-basics-demo.txt", "two\n")
    console.print(file::read_lines("/tmp/komodo-basics-demo.txt"))
    console.print(file::exists("/tmp/komodo-basics-demo.txt"), file::is_dir("/tmp"))
    let f = file::find("greet.kmm", "/tmp/komodo-basics-demo.txt")
    if f == null then
        console.print("not found")
    else
        console.print("found", f)
```

```text
["one", "two"]
1 1
not found
```

`file::find` returns `string?`. It searches the directory of its second
argument, recursively, and returns `null` when nothing matches. Check for
`null` before you use the result.

## Diagnostics

The analyser checks a program before it is built. It reports three kinds of
message. The examples below show the one-line form, which is what tools read;
on a terminal the compiler prints a richer layout with a source snippet and an
explanation, see [Diagnostics](../diagnostics.md).

**Errors** stop compilation with a non-zero exit status. Most carry a
location in the form `file:line:col: error: message`:

```km
import console

void main()
    let count = 10
    console.print(cont)
```

```text
example.km:5:19: error: undefined name 'cont' (did you mean 'count'?)
```

```km
import console

fn add(a: int, b: int): int
    return a + b

void main()
    console.print(add("x", 2))
```

```text
example.km:7:19: error: argument 1 to 'add': type 'string' is not assignable to 'int'
```

```km
fn maybe(n: int): int?
    if n > 0 then
        return n
    return null

void main()
    let x = maybe(0)
    print(x + 1)
```

```text
example.km:8:11: error: left operand of '+' is possibly null
```

```km
void main()
    break
```

```text
example.km:2:5: error: `break` outside a loop
```

A `fn` that mutates state and a missing return are reported at the
function's declaration:

```km
struct Box
    v: int = 0

fn poke(b: Box): int
    b.v = 9
    return b.v

void main()
    print(poke(new Box()))
```

```text
example.km:4:1: error: pure `fn poke` mutates state — declare it `mut`
```

```km
fn pick(n: int): int
    if n > 0 then
        return 1

void main()
    print(pick(3))
```

```text
example.km:1:1: error: function 'pick' must return a value on every path
```

**Warnings** (lint) flag code that is legal but probably wrong. They are
printed and the program still builds and runs:

```km
void main()
    let y = 3
    if y == y then
        print("same")
```

```text
example.km:3:8: warning: '==' compares a value with itself
same
```

Lints include `x / 0`, `x == x`, `x = x`, a constant `if` condition and
unreachable code.

**Strict mode** turns every warning into an error. Pass `--strict`:

```bash
komodo --strict example.km
```

With `--strict`, the program above fails:

```text
example.km:3:8: error: '==' compares a value with itself
```

`--strict` also reports a local that is never used:

```km
void main()
    let unused = 5
    print("hi")
```

```bash
komodo --strict example.km
```

```text
error: 'unused' is declared but never used
```

Strict mode also reports a `match` on an enum that has no `else` and does not
cover every variant:

```km
enum Color
    Red
    Green
    Blue

void main()
    let c = Color::Red
    match c
        case Color::Red
            print("red")
        case Color::Green
            print("green")
```

```bash
komodo --strict example.km
```

```text
example.km:8:5: error: match on enum 'Color' is not exhaustive — missing: Blue
```

**Runtime errors** are reported when the program runs. An uncaught `throw`
or failed `assert` ends the program with status 1 (see
[Built-ins](#built-ins)).

## Differences from the reference

The language reference (`reference.md`) is the source of truth. Where the current compiler behaves
differently, the compiler is what the examples above follow. The differences
found while writing this page are:

- `main(args)`: `args[1]` is the first user argument. an older draft said
  `args[2]`.
