# Komodo language reference

This page states every syntax form and rule of the Komodo language as the
current compiler behaves. It is the single place to check a rule. The guides
([basics](basics.md), [types](types.md)) teach the language; [design.md](../../design.md)
is the older catalogue, and where it disagrees with this build, this page
follows the compiler and lists the difference in
[Open questions](#open-questions-for-the-maintainer).

## How to read the examples

Every ```` ```km ```` block is compiled and run by `tools/check-docs.py`:

- A block followed by a ```` ```text ```` block must print exactly that text (line
  order is ignored). If the text starts with `error:` or `warning:`, the
  compiler output must contain that message.
- `<!-- doc-error -->` before a block marks code that the compiler must reject.
  The text block gives the expected message.
- `<!-- doc-skip -->` before a block skips it. Each skipped block says why: it
  needs several files, C companion code, command-line flags, or it crashes or
  hangs.
- Output lines that the compiler prints for a file are shown with the file
  name `main.km`. Paths and file names in messages are normalised by the checker.

Every rule is backed by a block. Behaviour that is observed but not clearly
intended is not stated as a rule; it is listed under
[Open questions](#open-questions-for-the-maintainer) with the observed output.

## Contents

1. [Programs and files](#1-programs-and-files)
2. [Lexical structure](#2-lexical-structure)
3. [Literals](#3-literals)
4. [Types](#4-types)
5. [Variables and constants](#5-variables-and-constants)
6. [Operators](#6-operators)
7. [Postfix forms and expressions](#7-postfix-forms-and-expressions)
8. [Statements](#8-statements)
9. [Functions](#9-functions)
10. [Structs](#10-structs)
11. [Classes](#11-classes)
12. [Enums](#12-enums)
13. [Patterns and match](#13-patterns-and-match)
14. [Modules and imports](#14-modules-and-imports)
15. [Memory and ownership](#15-memory-and-ownership)
16. [Errors and runtime failures](#16-errors-and-runtime-failures)
17. [Built-in functions](#17-built-in-functions)
18. [Standard library](#18-standard-library)
19. [Foreign functions and exports](#19-foreign-functions-and-exports)
20. [Warnings and strict mode](#20-warnings-and-strict-mode)
21. [Compile error catalogue](#21-compile-error-catalogue)
22. [Open questions for the maintainer](#open-questions-for-the-maintainer)

---

## 1. Programs and files

- A program is a `.km` file with a `void main()` function. `main` is the entry point.
- `.kmm` files are modules. A module file starts with `module <name>` (section 14).
- `main` takes either no parameters or one parameter for the command line.

```km
void main()
    print("Hello, Komodo!")
```

```text
Hello, Komodo!
```

With `void main(args)`, `args` is a `[string]`. `args[0]` is the program path
and `args[1]` is the first user argument. Its length is `#args`. The
compiler was given no arguments in this check, so `#args` is 1.

```km
void main(args)
    print(#args)
```

```text
1
```

`design.md` says the first user argument is `args[2]`. The compiler uses
`args[1]`.

Command line (not checked by the tool):

```bash
komodo hello.km          # compile, link and run
komodo -c hello.km       # compile and link only
komodo -q hello.km       # run without the compiler's progress lines
komodo --strict hello.km # warnings become errors (section 20)
komodo --lib prog.km     # build libprog.a for C/C++ (section 19)
```

---

## 2. Lexical structure

### 2.1 Comments

`//` runs to the end of the line. `/* ... */` spans lines. `/** ... */` is a doc
comment and is ignored. Block comments do not nest: the first `*/` ends the
comment.

```km
// A line comment runs to the end of the line.
/* A block comment
   can span several lines. */

/** A doc comment. */
void main()
    print("comments are ignored") // trailing comment
    /* outer /* inner */ print("the first */ ends the comment")
```

```text
comments are ignored
the first */ ends the comment
```

### 2.2 Blocks and newlines

Blocks are indentation-based. An indented line opens a block; a dedent closes
it. Newlines end statements, and there are no semicolons. A dedent must return
to a level that is already open.

```km
void main()
    let n = 7
    if n > 5 then
        print("big")
    else
        print("small")
```

```text
big
```

<!-- doc-error -->
```km
void main()
    print(1)
  print(2)
```

```text
main.km:3:3: error: Unexpected top-level token: print
```

### 2.3 Identifiers

An identifier is a letter or `_`, followed by letters, digits or `_`. Case
matters.

```km
void main()
    let _x = 3
    let x_1 = _x + 1
    print(x_1)
```

```text
4
```

Case matters: `A` and `a` are different names.

```km
void main()
    let A = 1
    let a = 2
    print(A, a)
```

```text
1 2
```

An identifier that starts with a digit is not valid. The compiler's message for
`let 9lives = 1` is misleading; see [Open questions](#open-questions-for-the-maintainer).

### 2.4 Keywords

These words are reserved (the lexer's keyword table):

```
true false null if else while for to until do then fn void return throw
break continue struct class enum match case new let and or not import link
mut extern module constructor be down in is xor
```

Some words are contextual: they have a meaning only in one position and are
ordinary identifiers elsewhere. They are `public`, `private`, `abstract`,
`shared`, `unique`, `export`, `type` (after `extern`), and `self` (the receiver
inside a method). `then` is the same token as `do`. `xor` is the bitwise
operator `^`. `elif`, `step`, `super` are not keywords.

```km
void main()
    let type = 1
    let end = 2
    let public = 3
    let self = 4
    let elif = 5
    print(type + end + public + self + elif)
```

```text
15
```

A keyword used as a name, such as `let while = 1`, is not rejected: the
program compiles and the statement is misparsed. See
[Open questions](#open-questions-for-the-maintainer).

---

## 3. Literals

| Kind | Forms |
|---|---|
| int | `42`, `0xFF`, `0b1010`; `_` may separate digits in any base |
| float | `2.0`, `3.14`, `2.0f`, `2f`, `1_000.5` |
| string | `"text"` with escapes; `` `text {expr}` `` interpolated |
| char | `'a'`, `'\n'` |
| bool | `true`, `false` |
| null | `null` |
| array | `[]`, `[4, 15, 8]` |

There is no exponent notation and no `0o` octal literal:

<!-- doc-error -->
```km
void main()
    let x = 0o17
    print(x)
```

```text
main.km:2:14: error: undefined name 'o17'
```

<!-- doc-error -->
```km
void main()
    let x = 1e3
    print(x)
```

```text
main.km:2:14: error: undefined name 'e3'
```

### 3.1 Integers

```km
void main()
    print(0xFF, 0b1010, 1_000_000, 0xFF_FF)
    print(0xff + 1, 0b1111_0000)
```

```text
255 10 1000000 65535
256 240
```

### 3.2 Floats

A float literal has a `.`. The suffix `f` forces a float: `2f` is `2.0`.

```km
void main()
    print(1_000.5, 2.0_5)
    print(2f, 2.5f, 2.0f * 2, 1_0f)
```

```text
1000.5 2.05
2.0 2.5 4.0 10.0
```

### 3.3 Strings

Escapes: `\n` newline, `\t` tab, `\\` backslash, `\"` quote, `\'` apostrophe,
`\xHH` a byte in hex, `\NNN` an octal byte, `\uHHHH` a Unicode code point
(written as UTF-8).

```km
void main()
    print("quote:\" back:\\ apostrophe:\'")
    print("line1\nline2")
    print("\x41\101A café")
```

```text
quote:" back:\ apostrophe:'
line1
line2
AAA café
```

A string literal is a sequence of bytes. A NUL byte (`\0`) inside a string
ends the printed text at that point; see
[Open questions](#open-questions-for-the-maintainer).

### 3.4 Interpolated strings

A string in backticks is interpolated. Each `{expr}` is converted with
`string(...)`. Inside a backtick string, `\{` and `\}` write braces, and
`` \` `` writes a backtick. `{...}` may hold any expression, including another
backtick string.

```km
void main()
    let n = 3
    let who = "world"
    print(`hello {who}, next is {n + 1}`)
    print(`big? {n > 2 and "yes" or "no"}`)
    print(`literal \{braces\} and a \`tick\``)
    print(`nested {`in {n}`}`)
```

```text
hello world, next is 4
big? yes
literal {braces} and a `tick`
nested in 3
```

There is no format-spec syntax: `{n:5}` is an error. A float prints in `%g`
form with a trailing `.0` when whole (section 4.2), so round it first if you
need fixed decimals.

<!-- doc-error -->
```km
void main()
    let n = 7
    print(`[{n:5}]`)
```

```text
error: string(x) takes exactly one argument
```

### 3.5 Chars and bools

```km
void main()
    let c = 'a'
    print(c, c == 'a', "abc"[1], "abc"[0] == 'a')
    print(c == "a", '\'', '\\', '\"')
```

```text
a 1 b 1
1 ' \ "
```

A `char` compares equal to a one-character string. A `bool` prints as `1` or
`0`.

### 3.6 Ranges

`a to b` is a range. It is valid only in a `for` header and in a slice.

<!-- doc-error -->
```km
void main()
    for i in 1 to 3
        print(i)
```

```text
main.km:2:16: error: a range `a to b` is only valid in a `for` loop or a slice, not as a value
```

---

## 4. Types

### 4.1 Primitive and built-in types

| Type | Meaning |
|---|---|
| `int` | 64-bit signed integer |
| `uint` | unsigned integer |
| `u8`, `u16`, `u32`, `u64` | unsigned integers |
| `i8`, `i16`, `i32`, `i64` | signed integers |
| `float`, `f64` | IEEE-754 double (`f64` is the same type) |
| `f32` | single-precision: a value is held as a double; 4 bytes only at a C boundary |
| `bool` | `true` / `false` |
| `char` | one character |
| `string` | text |
| `[T]` | array of `T` |
| `T?` | nullable `T` |

Integer types print as their value. The widths matter at a C boundary
(section 19).

```km
void main()
    let a: u16 = 1
    let b: i16 = -1
    let c: u32 = 4
    let d: i32 = -4
    let e: i64 = 9
    let f: uint = 2
    print(a, b, c, d, e, f)
```

```text
1 -1 4 -4 9 2
```

Literals are range-checked: `let x: u8 = 300` is an error
(`literal 300 does not fit in u8`). Arithmetic on the narrow types wraps; see
[Integers and `f32`](integers.md) for every width.

### 4.2 Floats

`float` and `f32` mix freely, and `int` widens to `float` in any mixed
expression or assignment. A float prints in `%g` form, with `.0` kept when the
value is whole.

```km
void main()
    let a: int = 3
    let b: float = a
    let g: f32 = 2.5
    let h: f64 = 0.25
    print(b, g + h, string(7.0))
```

```text
3.0 2.75 7.0
```

Assigning a `float` to an `int` is an error. Use `int(x)` (section 17).

<!-- doc-error -->
```km
void main()
    let x: int = 1.5
    print(x)
```

```text
main.km:2:5: error: initialiser for 'x': type 'float' is not assignable to 'int'
```

### 4.3 Arrays

`[T]` is an array of `T`. Arrays nest: `[[int]]`. An empty array needs a
type annotation.

```km
void main()
    let e: [int] = []
    e += [1]
    e += [2, 3]
    let grid: [[int]] = []
    grid += [[1, 2]]
    print(#e, e, #grid, grid[0][1])
```

```text
3 [1, 2, 3] 1 2
```

Arrays and strings have `#x` and `x.length`:

```km
void main()
    let xs = [1, 2]
    print(xs.length, #xs, "ab".length)
```

```text
2 2 2
```

### 4.4 Nullable types

`T` is never null. `T?` may hold `null`. Only a nullable type accepts `null`.
A value of type `T?` must be narrowed before it is read as a `T` (section 4.5).

<!-- doc-error -->
```km
struct Node
    value: int

void main()
    let n: Node = null
    print(n.value)
```

```text
main.km:5:5: error: initialiser for 'n': type 'null' is not assignable to 'Node'
```

```km
void main()
    let s: string? = null
    let xs: [int]? = null
    print(s, xs == null)
```

```text
null 1
```

### 4.5 Narrowing

Narrowing comes from `x == null`, `x != null`, `x is T`, and from a branch
that returns, throws, breaks or continues. After `if x == null then return`,
`x` is not null for the rest of the block.

```km
fn find(xs: [int], want: int): int?
    for x in xs
        if x == want then
            return x
    return null

void main()
    let hit = find([1, 2, 3], 2)
    if hit == null then
        print("none")
        return
    print("found", hit + 1)
    let miss = find([1], 9)
    if miss != null then
        print("unexpected")
    else
        print("miss is null")
```

```text
found 3
miss is null
```

Using a `T?` without narrowing it is an error:

<!-- doc-error -->
```km
fn find(xs: [int], want: int): int?
    for x in xs
        if x == want then
            return x
    return null

void main()
    let hit = find([1, 2, 3], 2)
    print(hit + 1)
```

```text
main.km:9:11: error: left operand of '+' is possibly null
```

### 4.6 Conversions

- `int(x)` converts a number to `int`. A `float` is truncated toward zero.
  `int` of a string is an error.
- `string(x)` converts any value to text.
- `float` and `int` convert implicitly only in the widening direction (`int` to
  `float`).

```km
void main()
    print(int(3.99), int(-2.5), int(7))
    print(string(2.0), string(true), string(42))
    print(string(1.5) + "x", "pi~" + string(3))
```

```text
3 -2 7
2.0 true 42
1.5x pi~3
```

<!-- doc-error -->
```km
void main()
    print(int("3"))
```

```text
main.km:2:11: error: 'int' expects a number, got 'string'
```

### 4.7 Type names

A struct, class, enum or `extern type` is named by its declaration. `is`
accepts only struct and class names (section 6.5), not primitive types.

<!-- doc-error -->
```km
void main()
    let x = 1
    print(x is int)
```

```text
main.km:3:11: error: `is` expects a known type name on the right
```

---

## 5. Variables and constants

### 5.1 `let`

`let name = expr` declares a reassignable local. The type comes from the
initialiser unless it is written after the name.

```km
void main()
    let count = 0
    count = count + 1
    let name: string = "Ada"
    let limit be 10
    print(count, name, limit)
```

```text
1 Ada 10
```

A `let` with a type and no initialiser is accepted. Its value is not
initialised (open question 7).

### 5.2 `be`: constants (macros)

`let NAME be expr` declares a compile-time macro. Each use of `NAME` is
replaced by `expr`. No storage is allocated, and `NAME` cannot be assigned.
`let NAME: T be expr` also works.

```km
let WIDTH be 4
let HEIGHT be 3
let AREA be WIDTH * HEIGHT
let LIM: int be 10
let NAMES be ["a", "b"]

void main()
    print("area:", AREA, "limit:", LIM + 1, #NAMES, NAMES[1])
    let N be 10
    if WIDTH > 0
        let N be 99
        print("inner:", N)
    print("outer:", N)
```

```text
area: 12 limit: 11 2 b
inner: 99
outer: 10
```

Assigning to a macro is an error:

<!-- doc-error -->
```km
void main()
    let limit be 10
    limit = 11
```

```text
main.km:3:5: error: cannot assign to macro 'limit' (declared with be)
```

A `be` needs a value:

<!-- doc-error -->
```km
void main()
    let x be
    print(x)
```

```text
main.km:2:5: error: `let x be` needs a value
```

A macro body is re-evaluated at each use. Keep macro bodies to constants and
pure expressions.

### 5.3 Top-level globals

`let NAME = expr` at the top level declares a global variable. It is shared
by all functions and is initialised in source order before `main` runs. A
global can refer only to globals declared before it. A type or an initialiser
is required.

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

<!-- doc-error -->
```km
let a = b + 1
let b = 2

void main()
    print(a, b)
```

```text
error: undefined name 'b'
```

<!-- doc-error -->
```km
let g

void main()
    print(1)
```

```text
error: top-level 'g' needs a type annotation or an initialiser
```

### 5.4 Assignment

`=` assigns to a name, a field (`obj.f`), or an array element (`arr[i]`).
The compound forms are `+=`, `-=`, `*=`, `/=`, `%=` and mean `x = x op v`.
`+=` also appends to an array (`arr += [x]`) and concatenates strings.

```km
void main()
    let n = 20
    n += 5
    n -= 3
    n *= 2
    n /= 4
    n %= 7
    print("n:", n)
    let xs = [2, 4, 6]
    xs[1] *= 10
    xs[2] -= 1
    print(xs[0], xs[1], xs[2])
    let s = "x"
    s += "y"
    print(s)
```

```text
n: 4
2 40 5
xy
```

Assigning a value of the wrong type is an error:

<!-- doc-error -->
```km
void main()
    let n = 1
    n = "text"
    print(n)
```

```text
main.km:3:5: error: 'n': type 'string' is not assignable to 'int'
```

Assigning to an undeclared name is an error:

<!-- doc-error -->
```km
void main()
    y = 3
    print(y)
```

```text
main.km:2:5: error: assignment to undeclared variable 'y'
```

Loop variables and `be` names are immutable:

<!-- doc-error -->
```km
void main()
    for i = 1 to 2
        i = 5
    print(1)
```

```text
main.km:3:9: error: cannot reassign immutable binding 'i' (declared with `be`)
```

---

## 6. Operators

### 6.1 Precedence

From lowest to highest:

| Level | Operators |
|---|---|
| 1 | `or` |
| 2 | `and` |
| 3 | `is`, `in` (the design's intended place; see open question 22) |
| 4 | `== != < <= > >=` |
| 5 | bitwise `& \| ^ << >>` (and `xor`) |
| 6 | `+ -` |
| 7 | `* / %` |
| 8 | unary `- not #` |
| 9 | postfix: call, `.`, `[ ]`, `?` |

The bitwise operators all share one level and group left to right. A shift
binds looser than `+`, and a comparison binds looser than a shift. The places
of `is` and `in` relative to comparisons do not match the table; see open
question 23 before relying on them.

```km
void main()
    print(2 + 3 * 4, (2 + 3) * 4)
    print(3 * 4 + 2 << 1, 1 << 2 < 5)
    print(12 & 10 | 1 ^ 3)
    print(not 0 == 5, 0 or 1 and 0, 1 or 0 and 0)
    print(-2 * 3, - 2 * - 3)
```

```text
14 20
28 1
10
0 0 1
-6 6
```

### 6.2 Arithmetic

`+ - * / %`. Integer `/` truncates toward zero, and `%` takes the sign of the
dividend. A `float` operand makes the result a `float`.

```km
void main()
    print(7 / 2, 7 % 3, -7 / 2)
    print(7 / -2, -7 % 3, 7 % -3)
    print(1.0 / 4.0, -3.0 % 2.0)
```

```text
3 1 -3
-3 -1 1
0.25 -1.0
```

Division by zero is a warning at compile time and a crash at run time
(section 16).

### 6.3 Comparison

`== != < <= > >=` yield `1` or `0`. Comparisons do not chain: write
`(a == b) == c`. Written as a bare argument list, `a == b == c` is not a
chain; see open question 1.

```km
void main()
    print(1.0 < 2.0, 5 < 6, 5 == 6, 5 != 6)
    print((1 == 1) == 1)
```

```text
1 1 0 1
1
```

Struct and class values compare by identity: two `new` objects are not equal
even when their fields match.

```km
struct A
    n: int

void main()
    let a = new A(1)
    let b = new A(1)
    print(a == b, a != b)
```

```text
0 1
```

### 6.4 Logical operators

`and`, `or` and `not` work on `bool` and `int` values. `and` and `or` are
short-circuit. The conditional value `c and x or y` gives `x` when `c` is true
and `y` otherwise. There is no `?:` operator.

```km
void main()
    print(not true, true and false, false or true)
    let n = 7
    print(n > 3 and "yes" or "no")
    print(n > 0 and 100 / n > 5)
```

```text
0 0 1
yes
1
```

`!` is accepted but its result is unreliable; use `not`. See open question 2.

### 6.5 Membership and type tests

`x in coll` tests a string for a substring (`coll` a string; `x` a string or
`char`), or an array for an element (`coll` an array of the same element type).

```km
void main()
    print(3 in [1, 2, 3], "ell" in "hello", 'h' in "hello")
    print("beta" in ["alpha", "beta"])
```

```text
1 1 1
1
```

<!-- doc-error -->
```km
void main()
    print("a" in [1, 2])
```

```text
main.km:2:11: error: `in`: 'string' can't be an element of '[int]'
```

`x is T` is true when the runtime type of struct or class `x` is `T` or a
subtype of `T`. `null is T` is false.

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

### 6.6 Strings and arrays

`+` concatenates strings and `*` repeats them. `#s` is the length.
`string.length(s)` is deprecated and warns: write `#s`.

```km
void main()
    print("ab" * 3, "x" == "x", "x" != "y")
    print("Komodo" + " rocks", #"Komodo")
```

```text
ababab 1 1
Komodo rocks 6
```

<!-- doc-error -->
```km
void main()
    let v = 3
    print("v: " + v)
```

```text
main.km:3:11: error: cannot add 'string' and 'int' (use string(x) or an interpolated string)
```

---

## 7. Postfix forms and expressions

| Form | Meaning |
|---|---|
| `name(args)` | call |
| `recv.method(args)` | method call |
| `recv.field` | field access |
| `mod::name` / `mod.name` | module member (section 14) |
| `arr[i]` | element; out of range throws |
| `arr[a to b]` | slice |
| `f(args)?` | protected call (section 16) |
| `Type::Variant` | enum value (section 12) |

### 7.1 Slices

`a[i to j]` gives the elements from `i` up to, not including, `j`. Either bound
may be left out: `a[2:]`, `a[:5]`. A slice is a new value: changing it does
not change the original.

```km
void main()
    let s = "hello, world"
    print(s[0 to 5])
    print(s[7 to #s])
    print(s[7:])
    print(s[:5])
    let xs = [10, 20, 30, 40, 50]
    print(xs[1 to 4], xs[2:])
    let ys = xs[0 to 2]
    xs[0] = 9
    print(ys[0], xs[0])
```

```text
hello
world
world
hello
[20, 30, 40] [30, 40, 50]
10 9
```

### 7.2 Index errors

Indexing past the end throws; see section 16.

```km
void main()
    let xs = [1, 2, 3]
    print(xs[5])
```

```text
Uncaught error: array index 5 out of range (length 3) in main
```

<!-- doc-error -->
```km
void main()
    let xs = [1, 2, 3]
    print(xs[0]?)
```

```text
main.km:3:16: error: '?' applies to calls, not indexing
```

---

## 8. Statements

### 8.1 `if`

```km
void main()
    let n = 2
    if n == 1 then
        print("one")
    else if n == 2 then
        print("two")
    else
        print("other")
```

```text
two
```

`then` is optional on an `if` header, and so is `do` on `while` and `for`.
A one-line `if` has no `else`.

```km
void main()
    let n = 7
    if n % 2 == 0 then print("even")
    if n % 2 != 0 then print("odd")
    let i = 0
    while i < 2
        i += 1
    if i == 2
        print("i is", i)
```

```text
odd
i is 2
```

`elif` is not a keyword:

<!-- doc-error -->
```km
void main()
    let n = 2
    if n == 1 then
        print("one")
    elif n == 2 then
        print("two")
```

```text
main.km:5:17: error: unexpected 'then'
```

An `else` after a one-line `if` is an error:

<!-- doc-error -->
```km
void main()
    let n = 3
    if n > 1 then print("big") else print("small")
```

```text
main.km:3:32: error: unexpected 'else': a one-line `if c then X else Y` is not supported; write the branches on separate indented lines
```

### 8.2 `while`

```km
void main()
    let i = 0
    while i < 3 do
        print("while", i)
        i += 1
    while i > 0
        i -= 1
    print("done", i)
```

```text
while 0
while 1
while 2
done 0
```

### 8.3 `for`

| Form | Meaning |
|---|---|
| `for i = a to b` | `i` runs from `a` up to and including `b` |
| `for i = a down to b` | counts down, including `b` |
| `for i = a until b` | counts up, excluding `b` |
| `for x in xs` | each element of array `xs` |

```km
void main()
    for k = 1 to 3
        print("to", k)
    for k = 3 down to 1 do
        print("down", k)
    for k = 0 until 2
        print("until", k)
    for x in [5, 6] then
        print("each", x)
    for i = 5 to 3
        print("never")
```

```text
to 1
to 2
to 3
down 3
down 2
down 1
until 0
until 1
each 5
each 6
```

A range whose bounds are in the wrong order runs zero times. `step` is not
supported:

<!-- doc-error -->
```km
void main()
    for i = 1 to 3 step 2
        print(i)
```

```text
main.km:3:9: error: unexpected ''
```

`for x in` takes an array. `for c in "text"` is accepted by the compiler and
crashes at run time; see open question 13.

### 8.4 `break` and `continue`

They work in `while` and `for`. Outside a loop they are errors.

```km
void main()
    for x in [4, 15, 8, 23, 16]
        if x % 2 != 0 then
            continue
        if x > 20 then
            break
        print("kept", x)
```

```text
kept 4
kept 8
kept 16
```

<!-- doc-error -->
```km
void main()
    break
```

```text
main.km:2:5: error: `break` outside a loop
```

### 8.5 `match`

`match` compares a subject with each `case`. Each arm body is indented under its
`case`. `else` is the fallback. A `case` value may be an integer, a string, a
`char`, a `float`, a `bool`, an enum value, or a struct pattern (section 13).

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
    let ch = 'x'
    match ch
        case 'x'
            print("ex")
    let b = true
    match b
        case true
            print("bool case")
        case false
            print("no")
```

```text
second
ex
bool case
```

A `match` without `else` is allowed; a subject matching no `case` does nothing.
These forms are rejected:

<!-- doc-error -->
```km
void main()
    let n = 4
    match n
        case 1, 2
            print("low")
        else
            print("else")
```

```text
main.km:4:15: error: expected `case` or `else` in match, got ,
```

<!-- doc-error -->
```km
void main()
    let n = 4
    match n
        case 3 to 5
            print("range")
        else
            print("else")
```

```text
main.km:4:16: error: expected `case` or `else` in match, got to
```

<!-- doc-error -->
```km
void main()
    let n = 4
    match n
        case 4 if n > 3
            print("guard")
        else
            print("else")
```

```text
main.km:4:16: error: expected `case` or `else` in match, got if
```

### 8.6 `return` and `throw`

`return expr` returns a value. A bare `return` leaves a `void` function. A
function declared with a return type must return a value on every path.

```km
void helper(x: int)
    if x > 0 then
        return
    print("neg")

void main()
    helper(1)
    helper(-1)
```

```text
neg
```

<!-- doc-error -->
```km
fn f(): int
    if 1 > 0 then
        return 1

void main()
    print(f())
```

```text
main.km:1:1: error: function 'f' must return a value on every path
```

<!-- doc-error -->
```km
void f()
    return 5

void main()
    f()
```

```text
main.km:2:5: error: a void function cannot return a value
```

<!-- doc-error -->
```km
fn f(): int
    return

void main()
    print(f())
```

```text
main.km:2:5: error: a value function must return a value
```

<!-- doc-error -->
```km
fn f(): int
    return "x"

void main()
    print(f())
```

```text
main.km:2:5: error: return value of 'f': type 'string' is not assignable to 'int'
```

`throw expr` raises an error; see section 16. Control does not return from a
`throw`.

---

## 9. Functions

### 9.1 Declaration forms

```
fn name(params): T       value function: must not change anything the caller can see
mut name(params): T      may change its receiver's fields or array elements
void name(params)        procedure: returns nothing and is not constrained
```

A header with no return type is `void`. A `fn` or `mut` body must return a
value on every path.

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

A `fn` may not change anything the caller can see:

<!-- doc-error -->
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
main.km:4:1: error: pure `fn poke` mutates state — declare it `mut`
```

A `fn` may not call a `void` procedure:

<!-- doc-error -->
```km
void g()
    print(1)

fn f(): int
    g()
    return 1

void main()
    print(f())
```

```text
main.km:5:5: error: a pure `fn` cannot call the void procedure 'g'
```

The mutation check looks at the function's own body only. A `fn` that calls a
`mut` method is accepted (section 11.4).

Parameters are local variables and may be assigned:

```km
fn inc(x: int): int
    x = x + 1
    return x

void main()
    print(inc(1))
```

```text
2
```

Calling a function with the wrong number of arguments is an error:

<!-- doc-error -->
```km
void main()
    print(add(1))

fn add(a: int, b: int): int
    return a + b
```

```text
main.km:2:11: error: 'add' takes 2 argument(s), got 1
```

### 9.2 Default parameter values

A trailing parameter may have a default. The default is a literal, optionally
negated, and must have the parameter's type. A call may omit defaulted
parameters.

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

Defaults apply to free functions with a body only. Overloaded names may not have
them.

<!-- doc-error -->
```km
class C
    m_v: int = 0
    mut bump(by: int = 1)
        m_v += by

void main()
    let c = new C()
    c.bump()
```

```text
main.km:3:5: error: default parameter values are only allowed on free functions with a body
```

<!-- doc-error -->
```km
fn add(a: int, b: int = 1 + 2): int
    return a + b

void main()
    print(add(1))
```

```text
main.km:1:1: error: default value of parameter 'b' must be a literal
```

<!-- doc-error -->
```km
fn add(a: int = 1, b: int): int
    return a + b

void main()
    print(add(1, 2))
```

```text
main.km:1:1: error: parameter 'b' needs a default value, because it follows a parameter that has one
```

<!-- doc-error -->
```km
fn add(a: int, b: int = "two"): int
    return a + b

void main()
    print(add(1))
```

```text
main.km:1:1: error: default value of parameter 'b': type 'string' is not assignable to 'int'
```

### 9.3 Variadic functions

A trailing `...` makes a prototype accept any number of extra arguments. The
standard library's `console.print` is declared this way.

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

### 9.4 Prototypes

A header with no body is a prototype. Its body is supplied at link time, using
Komodo's call convention. The standard library is built this way (section 18).
A prototype with no definition fails at link time.

The example needs a second file (a module with `link` or a C companion). See
`stdlib/math.kmm` and `stdlib/math.c` for the pattern.

### 9.5 Generic (implicit-type) functions

A free function with an un-annotated parameter is a template. The compiler
makes a copy for each set of argument types at each call site. Typed and
untyped parameters may be mixed.

```km
import console

fn add(a, b)
    return a + b

fn scale(v, k: int)
    return v * k

void main()
    console.print(add(2, 3))
    console.print(add("Komo", "do"))
    console.print(add(1.5, 2.0))
    console.print(scale(6, 7))
    console.print(scale("ab", 3))
```

```text
5
Komodo
3.5
42
ababab
```

A call whose types do not fit the body is an error. The message names the
instantiation:

<!-- doc-error -->
```km
fn add(a, b)
    return a + b

void main()
    print(add(1, "x"))
```

```text
main.km:2:12: error: cannot add 'int' and 'string' (use string(x) or an interpolated string) in call add(int, string)
```

There are no generic angle brackets. `id<int>(1)` is read as a comparison:

<!-- doc-error -->
```km
fn id(x)
    return x

void main()
    print(id<int>(1))
```

```text
error: undefined name 'id'
```

### 9.6 Overloading

A free function may be declared several times with different parameter lists.
A call is resolved from the static argument types. When the argument's static
type is a base type and several overloads fit, the compiler dispatches on the
object's runtime type.

```km
import console

fn kind(x: int): string
    return "int"

fn kind(x: string): string
    return "string"

fn kind(x: bool): string
    return "bool"

fn area(w: int): int
    return w * w

fn area(w: int, h: int): int
    return w * h

abstract struct Shape

struct Circle : Shape
    r: int

struct Square : Shape
    s: int

fn name(x: Circle): string
    return "circle"

fn name(x: Square): string
    return "square"

fn describe(sh: Shape): string
    return name(sh)

void main()
    console.print(kind(1), kind("hi"), kind(true))
    console.print(area(5), area(3, 4))
    console.print(describe(new Circle(2)))
    console.print(describe(new Square(3)))
```

```text
int string bool
25 12
circle
square
```

<!-- doc-error -->
```km
fn f(a: int): int
    return a

fn f(b: int): int
    return b

void main()
    print(f(1))
```

```text
error: duplicate function 'f' — two overloads have identical parameter types
```

<!-- doc-error -->
```km
fn f(a: int): int
    return a

fn f(a: string): string
    return a

void main()
    print(f(1.5))
```

```text
error: no overload of 'f' matches the argument types
```

<!-- doc-error -->
```km
fn f(a: int): int
    return a

fn f(a: string): string
    return a

void main()
    print(f(1, 2))
```

```text
error: no overload of 'f' takes 2 argument(s)
```

<!-- doc-error -->
```km
fn greet(name: string, greeting: string = "hello"): string
    return greeting + ", " + name

fn greet(n: int): string
    return "num"

void main()
    print(greet("x"))
```

```text
main.km:1:1: error: overloaded function 'greet' cannot have default parameter values
```

Methods, constructors, `extern` functions and prototypes do not overload.

<!-- doc-error -->
```km
fn twice(x: int): int
    return x * 2

fn twice(x)
    return x + x

void main()
    print(twice(3))
```

```text
main.km:4:1: error: template 'twice' cannot share its name with another 'twice' declaration (a template does not overload)
```

### 9.7 Protected calls

`f(args)?` yields `null` instead of propagating an error thrown out of `f`.
It applies to calls, method calls and `new` expressions (section 16).

---

## 10. Structs

A struct is a heap object with named fields. It has no methods.

```km
import console

struct Token
    kind: string
    lexeme: string
    line: int = 1

void main()
    let t = new Token("ident", "count")
    let u = new Token("number", "42", 7)
    console.print(t.kind, t.lexeme, t.line)
    u.line = u.line + 1
    console.print(u.kind, u.lexeme, u.line)
    let z = new Token()
    console.print("zero:", z.line, "[" + z.kind + "]")
```

```text
ident count 1
number 42 8
zero: 1 []
```

- `new Name(args)` binds arguments to fields in order: fields without a default
  first, then fields with one. A field that gets no argument takes its default,
  or else a zero value: `0`, an empty string, `false`, `null` or the zero char.
- Named arguments are not supported (`new Pt(y: 4)` is an undefined name).
- Fields are read and written with `.`.

```km
struct Pt
    x: int
    y: int = 9

void main()
    let p = new Pt(1, 2)
    let q = new Pt(1)
    let z = new Pt()
    print(p.y, q.y, q.x, z.x, z.y)
```

```text
2 9 1 0 9
```

<!-- doc-error -->
```km
struct Pt
    x: int
    y: int

void main()
    let e = new Pt(1, 2, 3)
    print(e.x)
```

```text
main.km:6:13: error: `new Pt` given more values than it has fields
```

A struct value that a field did not initialise holds zeros:

```km
struct C
    x: int
    s: string
    b: bool
    f: float
    c: char

void main()
    let v = new C()
    print(v.x, "[" + v.s + "]", v.b, v.f, v.c == 'a')
```

```text
0 [] 0 0.0 0
```

<!-- doc-error -->
```km
struct Box
    v: int
    fn get(): int
        return v

void main()
    print(1)
```

```text
main.km:3:5: error: struct bodies hold fields only; use a class for methods
```

<!-- doc-error -->
```km
struct Pt
    x: int

void main()
    let p = new Pt(1)
    print(p.ex)
```

```text
main.km:6:11: error: type 'Pt' has no field 'ex' (did you mean 'x'?)
```

### 10.1 Inheritance and abstract structs

`struct C : P` gives `C` one parent. The parent's fields come first. An
`abstract` struct can be a parent but cannot be instantiated.

```km
import console

abstract struct Shape
    name: string = "shape"

struct Circle : Shape
    r: int

void main()
    let c = new Circle(5)
    console.print(c.name, c.r)
```

```text
shape 5
```

<!-- doc-error -->
```km
abstract struct Shape
    name: string = "shape"

void main()
    let s = new Shape()
    print(s.name)
```

```text
main.km:5:13: error: cannot instantiate abstract type 'Shape'
```

A base-typed value has only the base's fields. Narrow it with `is` to reach the
subtype's fields:

<!-- doc-error -->
```km
abstract struct Shape
    tag: int

struct Circle : Shape
    r: int

void main()
    let s: Shape = new Circle(0, 2)
    print(s.r)
```

```text
main.km:9:11: error: type 'Shape' has no field 'r'
```

### 10.2 Ownership modifiers

`shared` (the default), `unique`, and `abstract` may prefix `struct`. Their
semantics are in section 15.

---

## 11. Classes

A class has a constructor, fields and methods.

```km
import console

class Counter
    constructor(start: int)
        m_n = start

    mut bump()
        m_n = m_n + 1

    fn value(): int
        return m_n

    m_n: int

void main()
    let c = new Counter(5)
    c.bump()
    c.bump()
    console.print("counter:", c.value())
```

```text
counter: 7
```

- `constructor(params)` is run by `new C(args)`. The argument count must match.
- A class with no constructor takes no arguments; its fields take their defaults.
- A field is declared with a type and may have a default (`m_s: string = "x"`).
- An unqualified name that matches a field refers to the receiver's field.
  `self.m_n` also works.
- `fn` methods may not write a field. `mut` methods may.

```km
class Box
    m_v: int = 4
    fn get(): int
        return self.m_v
    mut put(v: int)
        self.m_v = v

void main()
    let b = new Box()
    b.put(9)
    print(b.get(), b.m_v)
```

```text
9 9
```

<!-- doc-error -->
```km
class Counter
    constructor(start: int)
        m_n = start

    fn value(): int
        m_n = m_n + 1
        return m_n

    m_n: int

void main()
    let c = new Counter(5)
    print(c.value())
```

```text
main.km:5:5: error: pure `fn Counter.value` mutates state — declare it `mut`
```

<!-- doc-error -->
```km
class Counter
    constructor(n: int)
        m_n = n
    m_n: int

void main()
    let c = new Counter(1, 2)
    print(c.m_n)
```

```text
main.km:7:13: error: constructor 'Counter' takes 1 argument(s), got 2
```

<!-- doc-error -->
```km
class Acc
    constructor(start: int)
        m_v = start
    m_v: int

void main()
    let a = new Acc()
    print(a.m_v)
```

```text
main.km:7:13: error: constructor 'Acc' takes 1 argument(s), got 0
```

<!-- doc-error -->
```km
class Acc
    m_v: int = 0
    fn peek(): int
        return m_v

void main()
    let a = new Acc()
    print(a.peek(1))
```

```text
main.km:8:11: error: method 'Acc.peek' takes 0 argument(s), got 1
```

<!-- doc-error -->
```km
class Acc
    m_v: int = 0

void main()
    let a = new Acc()
    a.bump()
```

```text
main.km:6:5: error: type 'Acc' has no method 'bump'
```

### 11.1 Access control

`public` and `private` labels in a class body. A `private` field or method is
usable only inside the class's own members.

```km
import console

class Account
public
    constructor(owner: string, balance: int)
        m_owner = owner
        m_balance = balance

    mut deposit(amount: int)
        m_balance = m_balance + fee(amount)

    fn describe(): string
        return m_owner + ": " + string(m_balance)

    fn sibling(other: Account): int
        return other.m_balance
private
    m_owner: string = ""
    m_balance: int = 0
    fn fee(amount: int): int
        return amount - 1

void main()
    let a = new Account("ann", 10)
    a.deposit(5)
    console.print(a.describe())
    console.print(a.sibling(new Account("bob", 7)))
```

```text
ann: 14
7
```

<!-- doc-error -->
```km
class Counter
public
    constructor(n: int)
        m_n = n

    fn value(): int
        return m_n
private
    fn helper(): int
        return m_n
    m_n: int

void main()
    let c = new Counter(5)
    print(c.helper())
```

```text
main.km:15:11: error: 'helper' is private to class 'Counter'
```

A `private` member of a parent class is not reachable from a subclass, with the
same message:

<!-- doc-error -->
```km
class Base
public
    constructor(n: int)
        m_n = n
private
    m_n: int = 0

class Sub : Base
public
    fn steal(): int
        return m_n

void main()
    print(new Sub(1).steal())
```

```text
main.km:11:16: error: 'm_n' is private to class 'Base'
```

### 11.2 Inheritance and virtual dispatch

`class C : Base` has one parent. Methods are inherited. A call through a
base-typed variable runs the subclass override.

```km
import console

abstract class Shape
    constructor(name: string)
        m_name = name
    fn area(): int
        return 0
    m_name: string

class Square : Shape
    constructor(s: int)
        m_name = "sq"
        m_s = s
    fn area(): int
        return m_s * m_s
    m_s: int

void main()
    let s: Shape = new Square(3)
    console.print(s.area(), s.m_name)
    console.print(s is Square, s is Shape)
```

```text
9 sq
1 1
```

A subclass with no constructor inherits the parent's constructor and it runs:

```km
import console

class Base
    constructor(n: int)
        console.print("base ctor", n)
        m_n = n
    m_n: int

class Kid : Base
    fn show(): int
        return m_n

void main()
    let k = new Kid(7)
    console.print(k.show())
```

```text
base ctor 7
7
```

A subclass that declares its own constructor does not run the parent's
constructor, and there is no `super` call; see open question 19.

<!-- doc-error -->
```km
class Base
    constructor(n: int)
        m_n = n
    m_n: int

class Kid : Base
    constructor(n: int)
        super(n * 2)

void main()
    print(new Kid(3).m_n)
```

```text
undefined reference to `super'
```

<!-- doc-error -->
```km
abstract class Shape
    fn area(): int
        return 0

void main()
    let s = new Shape()
    print(s.area())
```

```text
main.km:6:13: error: cannot instantiate abstract type 'Shape'
```

### 11.3 Methods calling methods

Inside a method, `self.m()` and an unqualified `m()` both call a method of the
receiver.

```km
class Acc
    m_v: int = 1
    fn get(): int
        return self.twice()
    fn twice(): int
        return m_v * 2

void main()
    print(new Acc().get())
```

```text
2
```

### 11.4 Calls from `fn` methods

The mutation check does not follow calls. A `fn` method that calls a `mut`
method is accepted, and the callee's writes happen anyway.

```km
class Acc
    m_v: int = 1
    mut bump()
        m_v += 1
    fn peek(): int
        return m_v
    fn check(): int
        bump()
        return 0

void main()
    let a = new Acc()
    print(a.check(), a.peek())
```

```text
0 2
```

### 11.5 Copying objects

`copy(x)` on a class object duplicates it without running its constructor.

```km
class Acc
    m_v: int = 0
    mut inc()
        m_v += 1

void main()
    let a = new Acc()
    a.inc()
    let dup = copy(a)
    dup.inc()
    print(a.m_v, dup.m_v)
```

```text
1 2
```

---

## 12. Enums

```km
import console

enum Color
    Red
    Green
    Blue

fn name(c: Color): string
    match c
        case Color::Red
            return "red"
        case Color::Green
            return "green"
        else
            return "blue"

void main()
    let c = Color::Green
    console.print(name(c), name(Color::Blue))
    console.print(c == Color::Green, c == Color::Red)
    console.print(c)
```

```text
green blue
1 0
1
```

- Variants are written `Enum::Variant` and are numbered from 0 in declaration
  order. An enum prints as its number.
- A variant of an enum is not an `int`: assigning it to an `int` is an error.

<!-- doc-error -->
```km
enum Dir
    North
    South

void main()
    let n: int = Dir::South
    print(n)
```

```text
main.km:6:5: error: initialiser for 'n': type 'Dir' is not assignable to 'int'
```

<!-- doc-error -->
```km
enum Dir
    North
    South

void main()
    print(Dir::Up)
```

```text
main.km:6:11: error: undefined name 'Up'
```

- A `match` on an enum with no `else` that misses a variant is accepted. With
  `--strict` it is an error (section 20).
- An enum field that `new` does not set holds the first variant (`0`), or the
  field's declared default:

```km
enum Dir
    North
    South

struct Car
    d: Dir = Dir::North

void main()
    let c = new Car()
    print(c.d, c.d == Dir::North)
```

```text
0 1
```

---

## 13. Patterns and match

A struct pattern `case Type(field: literal, name, ...)` matches a struct or
class value. The subject's runtime type must be `Type` or a subtype, and every
literal sub-pattern must equal the field. A bare name binds the field for the
arm body. `field: name` binds the field to `name`.

```km
import console

struct User
    name: string
    role: string
    age: int

void greet(u: User)
    match u
        case User(name: "admin", role: "superuser")
            console.print("!! all-powerful admin")
        case User(role: "admin", name)
            console.print("admin:", name)
        case User(name, role, age)
            console.print(name, "is a", role, "aged", age)

void main()
    greet(new User("admin", "superuser", 40))
    greet(new User("bob", "admin", 33))
    greet(new User("carol", "guest", 21))
```

```text
!! all-powerful admin
admin: bob
carol is a guest aged 21
```

Struct patterns cannot nest, and they cannot match `float` fields. Use an `else`
for a fallback.

<!-- doc-error -->
```km
struct P
    x: int

struct Q
    p: P

void main()
    let q = new Q(new P(1))
    match q
        case Q(p: P(x: 1))
            print(1)
        else
            print(2)
```

```text
main.km:10:21: error: undefined name 'x' (did you mean 'q'?)
```

Errors:

<!-- doc-error -->
```km
struct P
    x: int

void main()
    let p = new P(1)
    match p
        case Nope(x)
            print(1)
```

```text
error: struct pattern names unknown type 'Nope'
```

<!-- doc-error -->
```km
struct P
    x: int
    y: int

void main()
    let p = new P(1, 2)
    match p
        case P(x: 1, x: 2)
            print(1)
```

```text
error: field 'x' named twice in pattern 'P'
```

<!-- doc-error -->
```km
struct P
    x: int

void main()
    let p = new P(1)
    match p
        case P(x: "a")
            print(1)
        else
            print(2)
```

```text
error: pattern for field 'x': literal of type 'string' is not comparable to field type 'int'
```

<!-- doc-error -->
```km
struct P
    x: int

void main()
    match 3
        case P(x: 1)
            print(1)
        else
            print(2)
```

```text
error: struct pattern 'P(…)' used on non-struct subject of type 'int'
```

<!-- doc-error -->
```km
struct P
    x: float

void main()
    let p = new P(1.0)
    match p
        case P(x: 1.0)
            print(1)
```

```text
error: struct pattern cannot match on `float` field 'x' yet
```

<!-- doc-error -->
```km
struct P
    x: int

struct Z
    y: int

void main()
    let p = new P(1)
    match p
        case Z(y: 1)
            print(1)
```

```text
error: struct pattern type 'Z' is unrelated to subject type 'P'
```

<!-- doc-error -->
```km
extern type H

void main()
    let p = 1
    match p
        case H(x)
            print(1)
```

```text
error: cannot destructure the opaque handle type 'H' in a pattern
```

---

## 14. Modules and imports

A module is a `.kmm` file whose first line is `module <name>`. Its members are
reached with `name::member` or `name.member` (for functions).

This example needs two files: `greet.kmm` and `main.km`. The repository has the
same files as `tests/examples/greet.kmm` and `tests/examples/module_demo.km`.

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

<!-- doc-skip -->
```km
import console
import greet

void main()
    console.print(greet::hello("world"))
    console.print(greet.hello("world"))
    console.print(greet::banner())
```

Expected output of the first two lines is `Hello, world!`. The third line is
an error: `'banner' is private to module 'greet'`.

Rules:

- `import name` loads `name.kmm`. The compiler looks in the importing file's
  directory tree, then in its own directory, where the standard library lives.
- Every file that declares the same `module` name is merged into one namespace.
- A `public` label opens a section of members visible to other modules. A
  `private` section's members are visible only in their own module. A member with
  no label is public.
- Calling a private function as `module::name` from another module is an error.
- `mod::Type` builds a struct from another module: `new shapes::Sq(3)`. The dotted
  form `new shapes.Sq(3)` makes the compiler hang; see open question 18.
- `link` (section 19) may appear only inside a module.

A missing module is an error:

<!-- doc-error -->
```km
import nosuchmod

void main()
    print(1)
```

```text
main.km:1:8: error: cannot find module 'nosuchmod'
```

---

## 15. Memory and ownership

### 15.1 Garbage collection

Structs, classes, arrays and strings live on a collected heap. Nothing is freed
by hand, and an object is reclaimed once nothing refers to it.

```km
shared struct Point
    x: int = 0
    y: int = 0

void main()
    let acc = 0
    let i = 0
    while i < 1000000 do
        let p = new Point(i, i)
        acc = acc + (p.x - p.y)
        i = i + 1
    print("acc =", acc)
```

```text
acc = 0
```

By design.md, a program may use up to 70 % of available memory.
`KOMODO_MAX_HEAP` sets the limit (bytes, or with a `K`, `M`, `G` or `T` suffix). When live data does not
fit, the program prints `Komodo: out of memory ...` and exits with status 3.

The limit is set in the environment, which the checker does not pass. Command
line: `KOMODO_MAX_HEAP=64M komodo -q heap.km`.

### 15.2 `shared` (default)

Values alias. `let b = a` gives two names for one object.

```km
import console

shared struct Box
    v: int

void main()
    let a = new Box(1)
    let b = a
    b.v = 7
    console.print(a.v)
```

```text
7
```

`copy(x)` makes a deep copy: nested struct and array fields are copied too.

```km
import console

shared struct Box
    v: int

void main()
    let a = new Box(1)
    let b = copy(a)
    b.v = 7
    console.print(a.v, b.v)
```

```text
1 7
```

### 15.3 `unique`

A `unique` value has one owner. Binding it to a name, assigning it, storing it
in a field or element, or returning it moves it. The source name is then
invalid until it is assigned a fresh value. Passing it to a function is a borrow.

```km
import console

unique struct Handle
    id: int

void take(h: Handle)
    console.print("took", h.id)

void main()
    let a = new Handle(1)
    take(a)
    console.print("still mine:", a.id)
    let b = a
    a = new Handle(2)
    console.print(a.id, b.id)
```

```text
took 1
still mine: 1
2 1
```

Reading a moved-from name is an error:

<!-- doc-error -->
```km
unique struct Handle
    id: int

void main()
    let a = new Handle(1)
    let b = a
    print(b.id)
    print(a.id)
```

```text
error: use of moved value 'a' — a `unique` value moved away at 6:13 leaves nothing behind
```

Returning a value moves it to the caller:

```km
unique struct H
    id: int

fn make(): H
    let h = new H(4)
    return h

void main()
    let x = make()
    print(x.id)
```

```text
4
```

A `unique` value stored in a struct field moves into the struct, and a `unique`
value cannot be moved out of a field, an array element, or a global:

<!-- doc-error -->
```km
unique struct H
    id: int

struct Holder
    h: H

void main()
    let a = new H(1)
    let hold = new Holder(a)
    let x = hold.h
    print(x.id)
```

```text
main.km:10:13: error: cannot move `unique` value of type 'H' out of field 'h' — move the value that owns it instead
```

<!-- doc-error -->
```km
unique struct H
    id: int

void main()
    let arr = [new H(1)]
    let a = arr[0]
    print(a.id)
```

```text
main.km:6:13: error: cannot move `unique` value of type 'H' out of an array element
```

<!-- doc-error -->
```km
unique struct H
    id: int

let g = new H(1)

void main()
    let x = g
    print(1)
```

```text
main.km:7:13: error: cannot move `unique` value out of 'g' — it is not a locally owned binding
```

`copy(a)` on a `unique` value duplicates it and leaves `a` owned:

```km
unique struct H
    id: int

void main()
    let a = new H(3)
    let b = a
    a = new H(4)
    print(a.id, b.id)
    print(copy(b).id, b.id)
```

```text
4 3
3 3
```

`unique` may also prefix `class`. `abstract` may combine with `shared` or `unique`.

---

## 16. Errors and runtime failures

### 16.1 `throw` and protected calls

`throw expr` raises an error with a message. `f(args)?` catches any error thrown
out of the call and evaluates to `null`. `value or throw "msg"` throws when the
value is null or false.

```km
import console

fn check_port(n: int): int
    if n < 1 or n > 65535 then
        throw "port out of range"
    return n

void main()
    let port = check_port(70000)?
    if port == null then
        console.print("bad port, using 8080")
    else
        console.print("port", port)
    console.print(check_port(80)?)
```

```text
bad port, using 8080
80
```

`?` applies only to calls, method calls and `new` expressions:

<!-- doc-error -->
```km
void main()
    let xs = [1, 2]
    print(xs?)
```

```text
main.km:3:13: error: '?' applies to calls and `new` expressions only
```

A protected constructor that throws yields `null`:

```km
class Acc
    m_v: int
    constructor(v: int)
        m_v = v
        if v < 0 then
            throw "negative"
    mut add(n: int): int
        m_v += n
        return m_v

void main()
    let a = new Acc(-1)?
    if a == null then
        print("ctor failed")
    let b = new Acc(1)
    print(b.add(2)?)
```

```text
ctor failed
3
```

`or throw`:

```km
void main()
    let n = 0
    let v = n or throw "n must not be zero"
    print("unreachable", v)
```

```text
Uncaught error: n must not be zero
```

### 16.2 Uncaught errors

An error that reaches `main` without a `?` prints `Uncaught error: <message>`
to stderr and exits with status 1.

```km
fn check(n: int): int
    if n < 0 then
        throw "negative"
    return n

void main()
    print(check(4))
    let ok = check(-1)
    print("never")
```

```text
4
Uncaught error: negative
```

### 16.3 `assert`

`assert(cond)` and `assert(cond, "message")` throw when `cond` is false. The
message includes the file and line.

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
Uncaught error: main.km:2: assertion failed: half needs an even number
```

<!-- doc-error -->
```km
void main()
    assert()
```

```text
main.km:2:5: error: 'assert' takes a condition and an optional message, got 0 argument(s)
```

<!-- doc-error -->
```km
void main()
    let n: int = 1
    assert(n > 0, n)
```

```text
main.km:3:5: error: assert message: type 'int' is not assignable to 'string'
```

### 16.4 Runtime failures

- Index out of range throws (section 7.2).
- Integer division by zero crashes the program. The runtime prints a report
  with the Komodo call stack and exits with status 128 plus the signal number
  (136 for division by zero, 139 for a segmentation fault).

The report lists C library frames whose names differ between systems, so it is
not a stable text block. Example:

<!-- doc-skip -->
```km
void main()
    let z = 0
    print(10 / z)
```

The compiler prints `warning: division by zero` for a literal zero divisor and
still builds the program.

---

## 17. Built-in functions

These names need no `import`.

| Name | Meaning |
|---|---|
| `print(...)` | writes its arguments separated by spaces, then a newline |
| `string(x)` | converts any value to text |
| `int(x)` | converts a number to `int`, truncating |
| `join(parts, sep)` | joins a `[string]` with `sep` between the parts |
| `assert(cond[, msg])` | throws when `cond` is false |
| `copy(x)` | deep copy of a struct, class or array |
| `data(arr)` | element pointer of an array, for C (section 19) |

`print()` with no arguments writes an empty line.

```km
void main()
    print()
    print("after blank")
```

```text

after blank
```

```km
void main()
    print(string([1, 2]), string(true))
    print(join(["a", "b", "c"], ", "))
    print(join(["ab"], ""))
```

```text
[1, 2] true
a, b, c
ab
```

<!-- doc-error -->
```km
void main()
    print(copy(1, 2))
```

```text
main.km:2:11: error: 'copy' takes exactly one argument
```

`data(arr)` passes an array's storage to C. The example writes 8 bytes into the
first element of an `[int]`:

```km
extern fn memset(p: int, c: int, n: int): int

void main()
    let xs = [1, 2, 3]
    memset(data(xs), 0, 8)
    print(xs[0], xs[1])
```

```text
0 2
```

---

## 18. Standard library

Standard library modules are ordinary modules: `import name`. The bodies are in
`stdlib/<name>.c`.

| Module | Members |
|---|---|
| `console` | `print`, `write`, `error` (variadic; `print` adds a newline, `write` does not, `error` writes to stderr) |
| `math` | `abs min max pow gcd clamp sign` (int); `sqrt floor ceil round trunc fabs sin cos tan atan atan2 exp log log2 log10 powf hypot` (float); `pi()`, `e()` |
| `string` | `length split join trim trim_start trim_end replace index_of contains starts_with ends_with to_upper to_lower repeat reverse substring pad_start pad_end` |
| `algorithm` | `sort reverse sum max min index_of contains fill`; each works on `[int]` and `[float]` |
| `file` | `exists is_dir read_all read_lines write_all append_all find list_dir` |
| `os` | `args getenv setenv exit cwd pid platform` |
| `time` | `now millis sleep_ms format` |
| `random` | `seed int float shuffle` (splitmix64) |
| `path` | `join dirname basename ext abs exists` |
| `process` | `run output` (through `sh -c`; no-ops on wasm) |
| `sdl2`, `raylib` | C library bindings (section 19) |

Members of `math`, `string`, `algorithm`, `os`, `time`, `random`, `path` and
`process` are reached with a dot (`math.abs(x)`). `file` members use `::`
(`file::exists(p)`). `console` works with either.

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
    console.print(string.pad_start("7", 3, "0"), #"four")
```

```text
4 9 1024 6
4.0 2.0 3.14159
[1, 2, 3] 6 3
0.5 1
["a", "b", "c"] ABC
007 4
```

---

## 19. Foreign functions and exports

### 19.1 `extern fn` and `extern void`

An `extern` prototype names a C symbol and uses the C calling convention. A
call must pass exactly as many arguments as the prototype declares, except for
a variadic prototype (`...`).

```km
extern fn strlen(s: string): int
extern "m"
    fn cbrt(x: float): float
    fn hypot(a: float, b: float): float

void main()
    print(strlen("hello, world"))
    print(cbrt(27.0))
    print(hypot(3.0, 4.0))
```

```text
12
3.0
5.0
```

`extern void` returns nothing:

```km
extern void srand(seed: int)
extern fn rand(): int

void main()
    srand(1)
    print(rand() >= 0)
```

```text
1
```

<!-- doc-error -->
```km
extern fn puts(s: string): int

void main()
    puts("a", "b")
```

```text
main.km:4:5: error: 'puts' takes 1 argument(s), got 2
```

A variadic prototype takes its fixed parameters first:

```km
extern fn puts(s: string): int
extern fn printf(fmt: string, ...): int

void main()
    puts("one")
    printf("two\n")
```

```text
one
two
```

### 19.2 Symbol names

`extern fn name(params): T = "symbol"` binds the Komodo name `name` to the C
symbol `symbol`. This lets a Komodo name differ from the C name, for example
when the C name is a Komodo keyword.

```km
extern "m"
    fn cbrt_c(x: float): float = "cbrt"
    fn sqrt_c(x: float): float = "sqrt"

void main()
    print(cbrt_c(8.0), sqrt_c(16.0))
```

```text
2.0 4.0
```

<!-- doc-error -->
```km
extern fn f(x: int): int = "bad-name"

void main()
    print(f(1))
```

```text
error: extern 'f': character '-' is not allowed in the C symbol "bad-name"
```

<!-- doc-error -->
```km
extern fn f(x: int): int = ""

void main()
    print(1)
```

```text
main.km:1:30: error: extern 'f': the C symbol name after `=` is empty
```

<!-- doc-error -->
```km
extern fn f(x: int): int = 42

void main()
    print(1)
```

```text
main.km:1:28: error: expected a string literal after `=` (the C symbol name) in extern 'f'
```

<!-- doc-error -->
```km
extern fn f(x: int): int = "1abc"

void main()
    print(f(1))
```

```text
error: extern 'f': the C symbol "1abc" cannot start with a digit
```

### 19.3 `extern "lib"` blocks

A block groups declarations under one library. The header string is also the
link directive: `"m"` links `-lm`, and a flag such as `"-lm"` also works (checked).

### 19.4 `extern type`

`extern type Name` declares an opaque, pointer-sized handle. C functions
produce it. Komodo stores it and passes it back.

- A handle cannot be built with `new` or from an integer:

<!-- doc-error -->
```km
extern type H

void main()
    let h = new H()
    print(1)
```

```text
main.km:4:13: error: cannot construct `extern type H` — it is an opaque C handle; obtain one from a C function
```

- Comparisons `==` and `!=` work (`h == 0` tests for NULL, as in the example
  above). Arithmetic such as `+` is an error (checked below).

```km
extern type FileH
extern fn fopen(path: string, mode: string): FileH
extern fn fclose(f: FileH): int

void main()
    let f = fopen("/nonexistent/komodo-ref", "r")
    if f == 0 then
        print("open failed")
```

```text
open failed
```

<!-- doc-error -->
```km
extern type H

void main()
    let h: H = 0
    print(h)
```

```text
main.km:4:5: error: initialiser for 'h': type 'int' is not assignable to 'H'
```

<!-- doc-error -->
```km
extern type H
extern fn mk(): H

void main()
    let h = mk()
    print(h + 1)
```

```text
main.km:6:11: error: operator '+' is not defined for the opaque handle type 'H'
```

<!-- doc-error -->
```km
extern type H
extern fn mk(): H

void main()
    let h = copy(mk())
    print(1)
```

```text
main.km:5:13: error: cannot `copy` the opaque handle type 'H'
```

### 19.5 `extern let`

`extern let name: T` reads and writes a C global variable. The declared type
gives its width. The compiler needs a type.

```km
extern let timezone: int

void main()
    let z = timezone
    print(z >= -86400)
```

```text
1
```

<!-- doc-error -->
```km
extern let counter

void main()
    print(1)
```

```text
main.km:1:19: error: extern variable 'counter' needs a type: `extern let counter: int`
```

<!-- doc-error -->
```km
extern let xs: [int]

void main()
    print(1)
```

```text
error: extern variable 'xs': type '[int]' is not supported (use int / sized ints, bool, float, f32, string or an `extern type` handle)
```

Writing an `extern let` is shown in `tests/examples/ffi_extern_var.km` with
`tests/examples/cglobals.kmm` (needs a C companion).

### 19.6 Passing values to C

The full mapping of Komodo types to C types is in
[docs/ffi.md](../ffi.md#type-mapping). The checked rules are: a `string`
argument is a NUL-terminated `const char *`; `data(arr)` passes the array's
elements (section 17); `copy(s)` passes a struct by value (section 19.8). An
`int` passed to a `string` parameter is not rejected; see open question 15.

### 19.7 Callbacks

A bare Komodo function name passed to an `extern` is handed to C as a function
pointer. The compiler emits a trampoline.

```km
import console

extern fn atexit(callback: int): int

void farewell()
    console.print("farewell, from a Komodo callback")

void main()
    atexit(farewell)
    console.print("main running")
```

```text
main running
farewell, from a Komodo callback
```

A callback's parameters must be machine words or `float`/`f32`. An array is not
allowed:

<!-- doc-error -->
```km
fn cb(xs: [int]): int
    return 1

extern fn run_cb(cb: int): int

void main()
    print(run_cb(cb))
```

```text
main.km:7:11: error: 'cb' cannot be a C callback: parameter type '[int]' is not a plain machine word
```

Callbacks with more than six parameters and with `float` parameters compile and
run (`tests/examples/ffi_callback_ext.km`). `docs/ffi.md` says otherwise; see open
questions.

### 19.8 Structs by value

`copy(struct)` as an `extern` argument passes a struct by value, and an
`extern` may return a struct. This needs a C companion. See
`tests/examples/ffi_struct.km`, `tests/examples/ffi_sret.km` and `stdlib/raylib.kmm`.

### 19.9 Exporting to C: `export fn`

`export fn` prefixes a free function with a body. The function still works in
Komodo, and it gets an unmangled C entry point.

```km
export fn add(a: int, b: int): int
    return a + b

export fn greet(name: string): string
    return "hello, " + name

export void tick()
    print("tick")

void main()
    print(add(2, 3), greet("x"))
    tick()
```

```text
5 hello, x
tick
```

Exports have these restrictions:

<!-- doc-error -->
```km
export fn main(): int
    return 0
```

```text
main.km:1:8: error: `export fn main` is not allowed — `main` is the program entry point; export a differently named function
```

<!-- doc-error -->
```km
export fn total(xs: [int]): int
    return 0

void main()
    print(1)
```

```text
main.km:1:8: error: export fn 'total': parameter 'xs' has type '[int]', which can't cross the C ABI (use int / sized ints, bool, float, f32, string or an `extern type` handle)
```

<!-- doc-error -->
```km
export fn f(a: int = 3): int
    return a

void main()
    print(1)
```

```text
main.km:1:8: error: export fn 'f' cannot have default parameter values — C callers always pass every argument
```

<!-- doc-error -->
```km
export fn twice(x): int
    return 2

void main()
    print(1)
```

```text
main.km:1:8: error: export fn 'twice': parameter 'x' needs a type annotation
```

<!-- doc-error -->
```km
export fn add(a: int, b: int): int
    return a + b

export fn add(a: int): int
    return a

void main()
    print(1)
```

```text
main.km:1:8: error: export fn 'add' cannot be overloaded — a C symbol names exactly one function
```

<!-- doc-error -->
```km
export fn f(a: int): int

void main()
    print(1)
```

```text
main.km:3:1: error: `export` needs a free function with a body: 'f'
```

Building a library and header (not checked by the tool):

```bash
komodo --lib --emit-header prog.h prog.km      # prog.o, libprog.a, libprog.so, prog.h
komodo --emit-header out.h prog.km             # header only
gcc -no-pie main.c -L. -lprog -l:libgc.so.1 -lm
```

(Checked: `komodo --lib` on a one-function file wrote `libx.o`, `liblibx.a` and
`libx.h`.)

`-no-pie` is required. A program with no `main` takes the C `main` as an extra
input: `komodo prog.km -- driver.c`.

### 19.10 `link` (modules only)

`link` names a library or a companion C file for a module; see
`stdlib/sdl2.kmm` and `tests/examples/cbhelper.kmm`. `link` outside a module is an
error:

<!-- doc-error -->
```km
link "x.c"

void main()
    print(1)
```

```text
main.km:1:6: error: `link` may only be used inside a module (a file starting with `module <name>`)
```

The `link` forms need a module file; see `stdlib/sdl2.kmm` for a library name.

### 19.11 Target support

The checks in this section were run on the default x86-64 target. The other
targets are described in [docs/ffi.md](../ffi.md#target-support); this reference
does not re-verify them.

---

## 20. Warnings and strict mode

Warnings are printed and the program still builds.

These warnings print for `x == x`, a constant `if` condition, a self-assignment,
a division by a literal zero, and unreachable code. Each is shown with its
message.

```km
void main()
    let y = 3
    if y == y then
        print("same")
```

```text
main.km:3:8: warning: '==' compares a value with itself
same
```

```km
void main()
    if true then
        print(3)
```

```text
main.km:2:5: warning: `if` condition is always true
3
```

```km
void main()
    print(1)
    return
    print(2)
```

```text
main.km:4:5: warning: unreachable code
1
```

`--strict` turns every warning into an error and adds checks (run with
`komodo --strict file.km`; the checker runs without flags):

- an unused local: `error: 'unused' is declared but never used`
- a `match` on an enum with no `else` that misses a variant:
  `error: match on enum 'Color' is not exhaustive — missing: Blue`
- `==` comparing a value with itself is an error.

`komodo lint file.km` reports the same extra checks as warnings without
failing the build, plus a `let` that reuses a name already declared in the
function and a comparison with `true` or `false` (see `docs/cli.md`). A
local or parameter whose name starts with `_` is never reported as unused.

The `--strict` example below is verified by hand with `komodo --strict`. The
checker does not pass flags.

<!-- doc-skip -->
```km
void main()
    let unused = 5
    print("hi")
```

---

## 21. Compile error catalogue

Each row names a class of compile error and gives one example. Messages are
printed as `main.km:line:col: error: message`, or without a location where the
compiler has none. A message class with a single reason is listed once.

### 21.1 Lexing and parsing

<!-- doc-error -->
```km
void main()
    print(1)
  print(2)
```

```text
main.km:3:3: error: Unexpected top-level token: print
```

<!-- doc-error -->
```km
shared print(1)
void main()
    print(2)
```

```text
main.km:1:8: error: expected `struct` or `class` after type modifier
```

<!-- doc-error -->
```km
void main()
    let f = fn(x: int): int
        return x
    print(f(1))
```

```text
main.km:2:13: error: unexpected 'fn'
```

<!-- doc-error -->
```km
void main()
    let g = (x) => x
    print(g(1))
```

```text
main.km:2:17: error: unexpected '='
```

<!-- doc-error -->
```km
link "x.c"

void main()
    print(1)
```

```text
main.km:1:6: error: `link` may only be used inside a module (a file starting with `module <name>`)
```

### 21.2 Declarations

<!-- doc-error -->
```km
let a = 1
let a = 2

void main()
    print(a)
```

```text
error: duplicate top-level name 'a'
```

<!-- doc-error -->
```km
struct Shape
    n: int

struct Shape
    m: int

void main()
    print(1)
```

```text
error: duplicate type 'Shape'
```

<!-- doc-error -->
```km
struct A : Nope
    x: int

void main()
    print(1)
```

```text
error: cyclic or unresolved inheritance near 'A'
```

<!-- doc-error -->
```km
fn f(a: int): int
    return a

fn f(b: int): int
    return b

void main()
    print(f(1))
```

```text
error: duplicate function 'f' — two overloads have identical parameter types
```

<!-- doc-error -->
```km
struct Flags : u32
    bits: int

void main()
    print(1)
```

```text
error: cyclic or unresolved inheritance near 'Flags'
```

<!-- doc-error -->
```km
mut f(a: int = 1)
    print(a)

class Q
    m_v: int = 0
    mut bump(by: int = 1)
        m_v += by

void main()
    f()
```

```text
main.km:6:5: error: default parameter values are only allowed on free functions with a body
```

<!-- doc-error -->
```km
fn f(a: int = x): int
    return a

void main()
    print(1)
```

```text
main.km:1:1: error: default value of parameter 'a' must be a literal
```

<!-- doc-error -->
```km
extern let xs: [int]

void main()
    print(1)
```

```text
error: extern variable 'xs': type '[int]' is not supported (use int / sized ints, bool, float, f32, string or an `extern type` handle)
```

<!-- doc-error -->
```km
extern fn f(x: int): int = "bad-name"

void main()
    print(f(1))
```

```text
error: extern 'f': character '-' is not allowed in the C symbol "bad-name"
```

<!-- doc-error -->
```km
export fn f(xs: [int]): int
    return 0

void main()
    print(1)
```

```text
main.km:1:8: error: export fn 'f': parameter 'xs' has type '[int]', which can't cross the C ABI (use int / sized ints, bool, float, f32, string or an `extern type` handle)
```

<!-- doc-error -->
```km
fn twice(x: int): int
    return x * 2

fn twice(x)
    return x + x

void main()
    print(twice(3))
```

```text
main.km:4:1: error: template 'twice' cannot share its name with another 'twice' declaration (a template does not overload)
```

<!-- doc-error -->
```km
struct P
    x: int
    fn f(): int
        return x

void main()
    print(1)
```

```text
main.km:3:5: error: struct bodies hold fields only; use a class for methods
```

### 21.3 Types and assignments

<!-- doc-error -->
```km
void main()
    let x: Nope = 1
    print(x)
```

```text
main.km:2:5: error: initialiser for 'x': type 'int' is not assignable to 'Nope'
```

<!-- doc-error -->
```km
fn pick(n: int): int
    return n

void main()
    print(pick("x"))
```

```text
main.km:5:11: error: argument 1 to 'pick': type 'string' is not assignable to 'int'
```

<!-- doc-error -->
```km
struct Pt
    x: int

void main()
    let p = new Pt("a")
    print(p.x)
```

```text
main.km:5:13: error: field 'x' of 'Pt': type 'string' is not assignable to 'int'
```

<!-- doc-error -->
```km
fn f(a: int): int
    return a

void main()
    print(f(1, 2))
```

```text
main.km:5:11: error: 'f' takes 1 argument(s), got 2
```

<!-- doc-error -->
```km
fn f(): int
    return 1

fn f(a: int): int
    return a

void main()
    print(f(1, 2, 3))
```

```text
error: no overload of 'f' takes 3 argument(s)
```

<!-- doc-error -->
```km
void main()
    let n = 3
    print(n.x)
```

```text
main.km:3:11: error: field access on a non-object (type 'int')
```

<!-- doc-error -->
```km
struct Pt
    x: int

void main()
    let p = new Pt(1)
    p.nope()
```

```text
main.km:6:5: error: type 'Pt' has no method 'nope'
```

<!-- doc-error -->
```km
void main()
    let xs: [int]? = null
    print(xs[0])
```

```text
main.km:3:11: error: value of type '[int]?' is possibly null — narrow it before indexing
```

<!-- doc-error -->
```km
struct Pt
    x: int
    fn m(): int
        return 1

void main()
    let p: Pt? = null
    print(p.m())
```

```text
main.km:3:5: error: struct bodies hold fields only; use a class for methods
```

<!-- doc-error -->
```km
struct Pt
    x: int

void main()
    let p: Pt? = null
    print(p.m())
```

```text
main.km:6:11: error: 'p' is possibly null — narrow it before calling 'm'
```

<!-- doc-error -->
```km
void main()
    let n: int? = null
    print(1 + n)
```

```text
main.km:3:11: error: right operand of '+' is possibly null
```

<!-- doc-error -->
```km
void main()
    let n: int = 1
    print(n is int)
```

```text
main.km:3:11: error: `is` expects a known type name on the right
```

<!-- doc-error -->
```km
void main()
    print(int("3"))
```

```text
main.km:2:11: error: 'int' expects a number, got 'string'
```

<!-- doc-error -->
```km
void main()
    let xs = [1, 2]
    print(xs[0] + "x")
```

```text
main.km:3:11: error: cannot add 'int' and 'string' (use string(x) or an interpolated string)
```

<!-- doc-error -->
```km
void main()
    let s = new Nope()
    print(s)
```

```text
main.km:2:13: error: `new` of unknown type 'Nope'
```

<!-- doc-error -->
```km
void main()
    print(5 in "abc")
```

```text
main.km:2:11: error: `in` on a string wants a string / char on the left, got 'int'
```

<!-- doc-error -->
```km
void main()
    print(1 in 2)
```

```text
main.km:2:11: error: `in` wants a string or an array on the right, got 'int'
```

<!-- doc-error -->
```km
fn f(): int
    return 1

void main()
    f(1)
```

```text
main.km:5:5: error: 'f' takes 0 argument(s), got 1
```

<!-- doc-error -->
```km
void g()
    print(1)

fn f(): int
    g()
    return 1

void main()
    print(f())
```

```text
main.km:5:5: error: a pure `fn` cannot call the void procedure 'g'
```

<!-- doc-error -->
```km
extern fn strlen(s: string): int

void main()
    print(strlen(1.5))
```

```text
main.km:4:11: error: argument 1 to extern 'strlen': 'float' is not compatible with 'string'
```

<!-- doc-error -->
```km
fn cb(xs: [int]): int
    return 1

extern fn run_cb(cb: int): int

void main()
    print(run_cb(cb))
```

```text
main.km:7:11: error: 'cb' cannot be a C callback: parameter type '[int]' is not a plain machine word
```

<!-- doc-error -->
```km
struct P
    x: int

void main()
    let p = new P(1)
    p.z = 2
```

```text
main.km:6:5: error: type 'P' has no field 'z' (did you mean 'x'?)
```

<!-- doc-error -->
```km
void main()
    let n = 1
    n = "text"
    print(n)
```

```text
main.km:3:5: error: 'n': type 'string' is not assignable to 'int'
```

<!-- doc-error -->
```km
fn f(): int
    return "x"

void main()
    print(f())
```

```text
main.km:2:5: error: return value of 'f': type 'string' is not assignable to 'int'
```

<!-- doc-error -->
```km
enum Dir
    North

void main()
    print(Dir::North.x)
```

```text
main.km:5:11: error: field access on a non-object (type 'Dir')
```

<!-- doc-error -->
```km
fn f(a: int = "s"): int
    return 1

void main()
    print(f())
```

```text
main.km:1:1: error: default value of parameter 'a': type 'string' is not assignable to 'int'
```

### 21.4 Names, scope and visibility

<!-- doc-error -->
```km
void main()
    let count = 10
    print(cont)
```

```text
main.km:3:11: error: undefined name 'cont' (did you mean 'count'?)
```

<!-- doc-error -->
```km
void main()
    print(mystery + 1)
```

```text
error: undefined name 'mystery'
```

<!-- doc-error -->
```km
void main()
    let x = 1
    print(x)
    unknown_call(2)
```

```text
undefined reference to `unknown_call'
```

<!-- doc-error -->
```km
void main()
    break
```

```text
main.km:2:5: error: `break` outside a loop
```

<!-- doc-error -->
```km
void main()
    for i = 1 to 2
        i = 5
    print(1)
```

```text
main.km:3:9: error: cannot reassign immutable binding 'i' (declared with `be`)
```

<!-- doc-error -->
```km
void main()
    let limit be 10
    limit = 11
```

```text
main.km:3:5: error: cannot assign to macro 'limit' (declared with be)
```

<!-- doc-error -->
```km
class C
    m_v: int = 0
    mut bump(by: int = 1)
        m_v += by

void main()
    let c = new C()
    c.bump()
```

```text
main.km:3:5: error: default parameter values are only allowed on free functions with a body
```

<!-- doc-error -->
```km
class Counter
public
    constructor(n: int)
        m_n = n
private
    m_n: int = 0

void main()
    let c = new Counter(1)
    print(c.m_n)
```

```text
main.km:10:11: error: 'm_n' is private to class 'Counter'
```

<!-- doc-error -->
```km
void main()
    print(nosuch::f())
```

```text
main.km:2:11: error: undefined function 'nosuch::f'
```

### 21.5 Ownership

<!-- doc-error -->
```km
unique struct Handle
    id: int

void main()
    let a = new Handle(1)
    let b = a
    print(b.id)
    print(a.id)
```

```text
error: use of moved value 'a' — a `unique` value moved away at 6:13 leaves nothing behind
```

### 21.6 Patterns

<!-- doc-error -->
```km
struct P
    x: int

void main()
    let p = new P(1)
    match p
        case P(z: 1)
            print(1)
```

```text
error: type 'P' has no field 'z' (did you mean 'x'?)
```

<!-- doc-error -->
```km
struct P
    x: float

void main()
    let p = new P(1.0)
    match p
        case P(x: 1.0)
            print(1)
```

```text
error: struct pattern cannot match on `float` field 'x' yet
```

<!-- doc-error -->
```km
struct P
    x: int

void main()
    let p = new P(1)
    match p
        case Q(x)
            print(1)
```

```text
error: struct pattern names unknown type 'Q' (did you mean 'P'?)
```

### 21.7 Parameter defaults and overloads

<!-- doc-error -->
```km
fn f(a: int, b: int = 1, c: int): int
    return a

void main()
    print(f(1, 2))
```

```text
main.km:1:1: error: parameter 'c' needs a default value, because it follows a parameter that has one
```

<!-- doc-error -->
```km
fn f(a: int): int
    return a

fn f(a: string): string
    return a

void main()
    print(f(1.5))
```

```text
error: no overload of 'f' matches the argument types
```

### 21.8 Declarations that need values

<!-- doc-error -->
```km
let g
void main()
    print(1)
```

```text
error: top-level 'g' needs a type annotation or an initialiser
```

<!-- doc-error -->
```km
void main()
    let x be
    print(x)
```

```text
main.km:2:5: error: `let x be` needs a value
```

### 21.9 Returns and control flow

<!-- doc-error -->
```km
fn pick(n: int): int
    if n > 0 then
        return 1

void main()
    print(pick(3))
```

```text
main.km:1:1: error: function 'pick' must return a value on every path
```

<!-- doc-error -->
```km
void f()
    return 5

void main()
    f()
```

```text
main.km:2:5: error: a void function cannot return a value
```

<!-- doc-error -->
```km
fn f(): int
    return

void main()
    print(f())
```

```text
main.km:2:5: error: a value function must return a value
```

<!-- doc-error -->
```km
fn f(): int
    return "x"

void main()
    print(f())
```

```text
main.km:2:5: error: return value of 'f': type 'string' is not assignable to 'int'
```

<!-- doc-error -->
```km
void main()
    let n = 4
    match n
        case 4 if n > 3
            print("guard")
```

```text
main.km:4:16: error: expected `case` or `else` in match, got if
```

### 21.10 Functions and calls

<!-- doc-error -->
```km
fn add(a: int, b: int): int
    return a + b

void main()
    print(add(1))
```

```text
main.km:5:11: error: 'add' takes 2 argument(s), got 1
```

<!-- doc-error -->
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
main.km:4:1: error: pure `fn poke` mutates state — declare it `mut`
```

<!-- doc-error -->
```km
void main()
    let xs = [1, 2, 3]
    print(xs[0]?)
```

```text
main.km:3:16: error: '?' applies to calls, not indexing
```

<!-- doc-error -->
```km
void main()
    print(assert())
```

```text
main.km:2:11: error: 'assert' takes a condition and an optional message, got 0 argument(s)
```

---

## Open questions for the maintainer

Each item gives the snippet and what the compiler does now. Items marked
"doc-skip" hang, crash or depend on the environment, so the checker does not
run them. Each other item is a checked block that records the current output.
When a fix lands, the block fails and needs updating.

**1. A comparison inside a call's argument list splits into several arguments.**
`print(1 == 1 == 1)` prints two values. Outside an argument list, `let b = 1 == 1 == 1` gives `1`.

```km
void main()
    print(1 == 1 == 1)
```

```text
1 1
```

**2. `!` on a boolean gives an unreliable value.** `let t = !true` followed by
`print(t)` prints a stale value (for example `10`). The value changes between
runs.

<!-- doc-skip -->
```km
void main()
    let t = !true
    print(t)
```

**3. Exponent literals are misread.** `let x = 1e3` is `undefined name 'e3'`,
but `print(1e3)` prints `1` and no error.

```km
void main()
    print(1e3)
```

```text
1
```

**4. `**` is accepted as two multiplications.** `2 ** 3` prints `12`.

```km
void main()
    print(2 ** 3)
```

```text
12
```

**5. A NUL byte ends a printed string.** `"a\0b"` prints `a`, and the rest of the
string is lost.

```km
void main()
    print("a\0b")
    print("end")
```

```text
a
end
```

**6. An unknown escape drops its backslash.** `"a\qb"` prints `aqb`.

```km
void main()
    print("a\qb")
```

```text
aqb
```

**7. An uninitialised `let x: int` holds garbage.** It prints a large value
such as `140720869082744`.

<!-- doc-skip -->
```km
void main()
    let u: int
    print(u)
```

**8. (Resolved.)** Integer literals are now range-checked; see [Integers and `f32`](integers.md).

**9. Explicit enum values are ignored.** `Low = 1` and `High = 10` print as `0`
and `3`; `High` is not the value written.

```km
enum Level
    Low = 1
    High = 10

void main()
    print(Level::High, Level::Low)
```

```text
3 0
```

**10. Duplicate enum variants are accepted.**

```km
enum Dir
    North
    North

void main()
    print(1)
```

```text
1
```

**11. Duplicate `case` labels are accepted, and the first one wins.**

```km
void main()
    let n = 1
    match n
        case 1
            print("one")
        case 1
            print("dup one")
        else
            print("else")
```

```text
one
```

**12. `struct Flags : u32` is rejected.** `design.md` gives this as a fixed-width
container. The compiler reads `u32` as an unresolved parent.

```km
struct Flags : u32
    bits: int

void main()
    print(1)
```

```text
error: cyclic or unresolved inheritance near 'Flags'
```

**13. `for x in` over a string crashes.** The compiler accepts it.

<!-- doc-skip -->
```km
void main()
    for c in "ab"
        print(c)
```

**14. `join` on an `[int]` crashes.** The compiler accepts it.

<!-- doc-skip -->
```km
void main()
    let xs = [1, 2]
    print(join(xs, ","))
```

**15. An `int` passed to a `string` extern parameter is accepted.** The call
crashes at run time.

<!-- doc-skip -->
```km
extern fn strlen(s: string): int

void main()
    print(strlen(5))
```

**16. A call to an undeclared plain function is reported by the linker.** The
analyser reports `undefined name` for variables but not for calls to names
without a module prefix.

<!-- doc-error -->
```km
void main()
    print(1)
    unknown_call(2)
```

```text
undefined reference to `unknown_call'
```

**17. A program with no `main` is reported by the linker.**

<!-- doc-error -->
```km
let x = 1
```

```text
undefined reference to `main'
```

**18. Three inputs hang the compiler.** A dotted type in `new` never returns:
`new shapes.Sq(3)` with `shapes` a module (the `::` form works). An unclosed
parenthesis at the end of a file also never returns:

<!-- doc-skip -->
```km
import shapes

void main()
    let q = new shapes.Sq(3)
    print(q.s)
```

<!-- doc-skip -->
```km
void main()
    print(add(1, 2)
```

**19. A subclass constructor does not run the parent constructor, and
`super` is not a keyword.** `super(n * 2)` in a subclass constructor is a link
error (section 11.2). A subclass with its own constructor skips the parent's
constructor:

```km
class Base
    constructor(n: int)
        print("base ctor", n)
        m_n = n
    m_n: int

class Kid : Base
    constructor(n: int)
        m_n = n + 1

void main()
    print(new Kid(1).m_n)
```

```text
2
```

**20. `step` and a keyword as a name give poor messages.** `for i = 1 to 3 step 2`
reports `unexpected ''` (the token text is empty, section 8.3). `let while = 1`
followed by `print(while)` compiles and prints an empty line:

```km
void main()
    let while = 1
    print(while)
```

```text
```

**21. An identifier starting with a digit gets a misleading message.** `let 9lives = 1`
reports `assignment to undeclared variable 'lives'`.

```km
void main()
    let 9lives = 1
    print(9lives)
```

```text
main.km:2:10: error: assignment to undeclared variable 'lives'
```

**22. `in` mixed with `==` without parentheses does not follow the table.**
`let a = 2 in [2] == 0` prints `1`. The parenthesised `(2 in [2]) == 0` prints
`0`. Section 6.1 places `in` below comparisons, which would make the first form
a different expression. Write the parentheses until this is settled.

```km
void main()
    let a = 2 in [2] == 0
    print(a)
```

```text
1
```

**23. A class body accepts a bare assignment.** `m_v = 1` in a class body (with
no field declared) compiles, and the value is not visible.

```km
class A
    m_v = 1

void main()
    print(1)
```

```text
1
```

**24. A method without a body in an abstract class is accepted.** Whether that
is meant as an abstract method is not known.

```km
abstract class Shape
    fn area(): int

void main()
    print(1)
```

```text
1
```

**25. Macros are visible before their declaration.** `design.md` says a macro is
visible from its declaration to the end of its block.

```km
void main()
    print(LATER)

let LATER be 5
```

```text
5
```

**26. An unknown type in a `let` annotation reports a type mismatch.** The
message names `Nope` as the target, but the cause is the unknown type.

```km
void main()
    let x: Nope = 1
    print(x)
```

```text
main.km:2:5: error: initialiser for 'x': type 'int' is not assignable to 'Nope'
```

**27. Callback limits in `docs/ffi.md` are stale.** It says callbacks take up to
six parameters and reject `float`. `tests/examples/ffi_callback_ext.km` (ten
parameters, `double`s and `f32`) builds and prints the expected output with this
compiler, and `tests/examples/ffi_callback_ext.expected` agrees.

**28. `design.md` says the first user argument is `args[2]`.** The compiler
uses `args[1]`, and `docs/language/basics.md` already notes this difference.

**29. Struct equality is identity.** `a == b` on two `new A(1)` values is `0`.
This is stated in section 6.3 as the rule. Whether `==` should compare fields is
the maintainer's decision.

**30. `design.md` says a `private` section has `private` globals and types.** The
compiler checks only private functions reached as `module::name`.

**31. `--strict` mode is not covered by the checker.** The examples in section 20
were verified by hand. A future flag argument for `tools/check-docs.py` would
allow them to run.
