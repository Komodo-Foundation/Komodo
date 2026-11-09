# Komodo types

This page explains the types in Komodo: primitives and floats, structs and
classes, enums, nullable types, type tests, generic and overloaded functions,
errors, ownership, and the diagnostics you will meet while writing code.

Every snippet was compiled and run with the current `komodo` build, and the
`text` blocks show its real output. Where a snippet is meant to fail, the exact
compiler message is shown. Compile errors are printed as
`file:line:col: error: message`; the location is left out of the text blocks.
For the complete syntax reference see [design.md](../../design.md).

## Contents

- [Overview](#overview)
- [Structs](#structs)
- [Classes](#classes)
- [Type tests and narrowing](#type-tests-and-narrowing)
- [Nullable types](#nullable-types)
- [Enums and match](#enums-and-match)
- [Generic functions and overloading](#generic-functions-and-overloading)
- [Errors](#errors)
- [Ownership and memory](#ownership-and-memory)
- [Floats](#floats)
- [Common type errors](#common-type-errors)
- [Known gaps](#known-gaps)

## Overview

| Type | Meaning |
|---|---|
| `int` | 64-bit signed integer |
| `uint`, `u8`..`u64`, `i8`..`i64` | other integer widths |
| `float` | IEEE-754 double. `f64` is the same type; `f32` is held as a double too and is 4 bytes only at a C boundary |
| `bool`, `char`, `string` | booleans, single characters, text |
| `[T]` | array of `T` |
| `T?` | nullable `T`; may hold `null` |
| `struct`, `class` | heap object, held by reference |
| `enum` | named variants, stored as integers |

Annotations are written `name: Type`. Structs and classes are reference types:
`let b = a` gives a second name for the same object (see
[Ownership and memory](#ownership-and-memory)).

## Structs

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
    console.print(u.kind, u.lexeme, u.line)
    u.line = u.line + 1
    console.print("u.line now", u.line)
    let z = new Token()
    console.print("zero:", z.line, "[" + z.kind + "]")
```

```text
ident count 1
number 42 7
u.line now 8
zero: 1 []
```

- A field may have a default value (`line: int = 1`).
- `new Name(args)` allocates the object. Arguments bind positionally to the
  fields in constructor order: fields without a default come first, then the
  fields with a default. A field that gets no argument takes its default, or
  zero (`0`, `""`, `false`, `null`) if it has none. `design.md` states the
  same order.
- Fields are read and written with `.`.
- A struct has no methods. Write free functions that take the struct as a
  parameter.

### Inheritance and abstract types

`:` gives a struct one parent. The child's fields come after the parent's. An
`abstract` type can be a parent but cannot be instantiated.

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

Here `5` binds to the required field `r`, and `name` keeps its default.

```km
import console

abstract struct Shape
    name: string = "shape"

void main()
    let s = new Shape()
    console.print(s.name)
```

```text
error: cannot instantiate abstract type 'Shape'
```

## Classes

A class has a constructor, fields, and methods. Fields are declared with a
type annotation, as in the examples, and conventionally named with an `m_`
prefix. The prefix is a convention, not a rule.

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

- `fn` is a read-only method. It may not write a field.
- `mut` is a method that may write fields.
- Inside a method, an unqualified name that matches a field refers to the
  receiver's field. `self.m_n` also works.
- `new C(args)` allocates the object, sets field defaults, and runs the
  constructor. The argument count must match the constructor.

A `fn` that writes a field is rejected:

```km
import console

class Counter
    constructor(start: int)
        m_n = start

    fn value(): int
        m_n = m_n + 1
        return m_n

    m_n: int

void main()
    let c = new Counter(5)
    console.print(c.value())
```

```text
error: pure `fn Counter.value` mutates state — declare it `mut`
```

A constructor argument count mismatch is also an error:

```km
import console

class Counter
    constructor(n: int)
        m_n = n
    m_n: int

void main()
    let c = new Counter(1, 2)
    console.print(c.m_n)
```

```text
error: constructor 'Counter' takes 1 argument(s), got 2
```

### Public and private

`public` and `private` section labels are accepted in a class body. A
`private` field or method is reachable only from the members of its own
class (methods and the constructor). Reaching it from outside the class is a
compile error, and so is reaching a parent's private member from a subclass.

```km
import console

class Counter
public
    constructor(n: int)
        m_n = n

    fn value(): int
        return helper()

private
    fn helper(): int
        return m_n

    m_n: int

void main()
    let c = new Counter(5)
    console.print(c.value())
```

```text
5
```

`c.helper()` or `c.m_n` written in `main` is rejected with
`'helper' is private to class 'Counter'`.

Module-level `private` is enforced: a `private` function reached as
`module::name` from another module is an error (see
[design.md](../../design.md#files--modules)).

### Inheritance and virtual dispatch

A class has one parent (`class C : Base`). Methods are inherited. A call
through a base-typed variable runs the subclass's override.

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
    let q = new Square(4)
    console.print(q.area(), q is Shape)
```

```text
9 sq
1 1
16 1
```

`s.area()` dispatches to `Square`'s override, even though `s` is declared as
`Shape`.

## Type tests and narrowing

`x is T` is true when `x`'s runtime type is `T` or a subtype of it. A `null`
value is never an instance of anything, so `null is T` is false.

Inside an `if x is T` branch, `x` has type `T`, so you can read `T`'s fields.
The narrowing is only inside the branch.

```km
import console

abstract struct Shape
    tag: int

struct Circle : Shape
    r: int

struct Square : Shape
    side: int

fn area(s: Shape): int
    if s is Circle then
        return 3 * s.r * s.r
    if s is Square then
        return s.side * s.side
    return 0

void main()
    console.print(area(new Circle(0, 2)), area(new Square(0, 3)))
```

```text
12 9
```

Without a test, a base-typed value has only the base's fields:

```km
import console

abstract struct Shape
    tag: int

struct Circle : Shape
    r: int

void main()
    let s: Shape = new Circle(0, 2)
    console.print(s.r)
```

```text
error: type 'Shape' has no field 'r'
```

## Nullable types

`T` is never null. `T?` may be null. A `null` value is assignable only to a
nullable type:

```km
import console

struct Node
    value: int

void main()
    let n: Node = null
    console.print(n.value)
```

```text
error: initialiser for 'n': type 'null' is not assignable to 'Node'
```

Reading a field, calling a method, indexing, or doing arithmetic on a `T?` is
an error until the value is narrowed. Narrowing comes from `x == null`,
`x != null`, `x is T`, or a branch that returns, throws, breaks or continues.
After `if x == null then return`, `x` is non-null for the rest of the block:

```km
import console

fn find(xs: [int], want: int): int?
    for x in xs
        if x == want then
            return x
    return null

void main()
    let hit = find([1, 2, 3], 2)
    if hit == null then
        console.print("none")
        return
    console.print("found", hit + 1)
    let miss = find([1], 9)
    if miss != null then
        console.print("unexpected")
    else
        console.print("miss is null")
```

```text
found 3
miss is null
```

Without the check:

```km
import console

fn find(xs: [int], want: int): int?
    for x in xs
        if x == want then
            return x
    return null

void main()
    let hit = find([1, 2, 3], 2)
    console.print(hit + 1)
```

```text
error: left operand of '+' is possibly null
```

An object-typed field with no non-null default is treated as `T?`, because a
fresh `new` zero-fills it with `null`:

```km
import console

struct Node
    value: int
    next: Node?

void main()
    let n = new Node(1, null)
    console.print(n.next.value)
```

```text
error: in 'main': value of type 'Node?' is possibly null — narrow it before reading '.value'
```

## Enums and match

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

- Variants are referenced as `Enum::Variant`. A variant prints as its integer
  position (`Green` prints `1`).
- A `match` on an enum that misses a variant and has no `else` is allowed by
  default. With `--strict` it is an error:

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
    return "?"

void main()
    console.print(name(Color::Red))
```

```bash
komodo --strict enum_strict.km
```

```text
error: match on enum 'Color' is not exhaustive — missing: Blue
```

### Struct patterns

`case Type(field: literal, binding, ...)` matches when the subject's runtime
type is `Type` or a subtype, and every literal sub-pattern is equal. A bare
name binds that field for the arm body.

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

Struct patterns do not nest, do not count toward enum exhaustiveness, and
cannot match `float` fields yet. Use a final `case Type(...)` or `else` as a
fallback.

## Generic functions and overloading

### Generic (implicit-type) functions

A free function with an un-annotated parameter is a template. The compiler
creates a specialized copy for each set of argument types at a call site.

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

Typed and untyped parameters can be mixed (`scale`). A template is never
emitted on its own, so the body is only checked for the types it is called
with. A call whose types do not fit the body is an error naming the
instantiation, for example `cannot add 'int' and 'string' ... in call add(int, string)`.

### Overloading

A free function can be declared more than once with different parameter lists,
by count or by type. A call is resolved from the static argument types.

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

void main()
    console.print(kind(1), kind("hi"), kind(true))
    console.print(area(5), area(3, 4))
```

```text
int string bool
25 12
```

When the argument's static type is a base type and several overloads match,
the compiler emits a runtime check on the object's type id and calls the
matching overload (most specific first):

```km
import console

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
    console.print(describe(new Circle(2)))
    console.print(describe(new Square(3)))
```

```text
circle
square
```

The rules are:

- An exact repeat of the parameter types is an error:

  <!-- doc-error -->
  ```km
  import console

  fn f(a: int): int
      return a

  fn f(b: int): int
      return b

  void main()
      console.print(f(1))
  ```

  ```text
  error: duplicate function 'f' — two overloads have identical parameter types
  ```

- A call that matches no overload is an error:

  <!-- doc-error -->
  ```km
  import console

  fn f(a: int): int
      return a

  fn f(a: string): string
      return a

  void main()
      console.print(f(1.5))
  ```

  ```text
  error: no overload of 'f' matches the argument types
  ```

- **An overloaded name cannot take default parameter values.** Overloads and
  defaulted arities would compete, so the compiler rejects the combination:

  <!-- doc-error -->
  ```km
  import console

  fn greet(name: string, greeting: string = "hello"): string
      return greeting + ", " + name

  fn greet(n: int): string
      return "num"

  void main()
      console.print(greet("x"))
  ```

  ```text
  error: overloaded function 'greet' cannot have default parameter values
  ```

Methods, constructors, `extern` functions and prototypes do not overload.

## Errors

### throw and protected calls

`throw "message"` raises an error. A call written with a trailing `?` catches
any error thrown out of it and evaluates to `null`. `value or throw "msg"`
throws when `value` is null or false.

```km
import console

fn parse_port(n: int): int
    if n < 1 or n > 65535 then
        throw "port out of range"
    return n

void main()
    let p = parse_port(8080)?
    console.print("port:", p)
    let q = parse_port(70000)?
    if q == null then
        console.print("rejected, using default")
    let r = parse_port(0)
    console.print("never", r)
```

```text
port: 8080
rejected, using default
Uncaught error: port out of range
```

An error that reaches the top of `main` with no `?` prints
`Uncaught error: <message>` to stderr and exits with status 1.

`?` applies to calls (`f(args)?`, `obj.m(args)?`, `new T(args)?`). A
protected constructor that throws yields `null`, which must be narrowed before
you use the object.

### assert

`assert(cond)` and `assert(cond, "message")` throw when `cond` is false. The
message is built from `file:line`:

```km
import console

fn check(n: int): int
    assert(n > 0, "n must be positive")
    return n * 2

void main()
    console.print(check(4))
    console.print(check(-1))
```

```text
8
Uncaught error: 25.km:4: assertion failed: n must be positive
```

The `file:line` part is the path the compiler was given and the line of the
`assert`, so it depends on how the file was passed.

### Runtime index errors

Reading or writing an array index out of range throws an ordinary error:

```km
import console

void main()
    let xs = [1, 2, 3]
    console.print(xs[5])
```

```text
Uncaught error: array index 5 out of range (length 3) in main
```

## Ownership and memory

Every struct, class, array and string lives on a garbage-collected heap, and
nothing is freed by hand. The ownership modifiers below are compile-time
checks; they do not change the runtime layout.

### shared (default)

Values alias. `let b = a` gives two names for one object:

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

`copy(x)` makes a deep copy. Nested struct fields and array fields are
duplicated too. A class object is copied the same way, without running its
constructor:

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

### unique

A `unique` value has one owner. Binding, assigning, returning or storing it
moves it, and the source name is then invalid. Passing it to a function is a
borrow, not a move.

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

```km
import console

unique struct Handle
    id: int

void main()
    let a = new Handle(1)
    let b = a
    console.print(b.id)
    console.print(a.id)
```

```text
error: use of moved value 'a' — a `unique` value moved away at 8:13 leaves nothing behind
```

`copy(a)` on a `unique` value duplicates it and leaves `a` live.

### Heap limit

A program may use up to 70 % of the memory available to it. Set
`KOMODO_MAX_HEAP` to change the limit (a number of bytes, or with a `K`, `M`,
`G` or `T` suffix). When the live data does not fit, the program stops with
exit status 3:

```km
import console

struct Node
    value: int
    next: Node?

void main()
    let head: Node? = null
    let i = 0
    while i < 3000000 do
        head = new Node(i, head)
        i = i + 1
    console.print("built")
```

```bash
KOMODO_MAX_HEAP=64M ./komodo -q heap_demo.km
```

```text
Komodo: out of memory - heap limit 64 MB reached in main (raise it with KOMODO_MAX_HEAP)
```

## Floats

`float` is an IEEE-754 double. Arithmetic is `+ - * / %`, and an `int` in a
mixed expression is widened. Integer division stays integer. A float prints
with `%g` form, and a whole-number float keeps a trailing `.0`, so it reads
back as a float.

```km
import console

void main()
    let a = 3.0
    console.print(a / 2.0, 6.0 / 3.0, 7 / 2, 7.0 / 2)
    console.print(int(3.99), int(-2.5))
    let n = 5
    console.print(n + 0.5)
```

```text
1.5 2.0 3 3.5
3 -2
5.5
```

- `int(x)` is the explicit float-to-int conversion. It truncates toward zero.
  Plain assignment does not convert floats to ints:

  <!-- doc-error -->
  ```km
  import console

  void main()
      let x: int = 1.5
      console.print(x)
  ```

  ```text
  error: initialiser for 'x': type 'float' is not assignable to 'int'
  ```

- `int(x)` accepts only numbers: `int("3")` is an error,
  `'int' expects a number, got 'string'`.
- `f32` behaves like `float` inside Komodo. Its 4-byte width matters only for
  struct fields and parameters at a C FFI boundary.

## Common type errors

The messages below are the compiler's. The `file:line:col:` prefix is left
out of every text block, and the message is shown as it is printed.

| Message | Cause |
|---|---|
| `argument 1 to 'add': type 'string' is not assignable to 'int'` | an argument has the wrong type |
| `'f' takes 1 argument(s), got 2` | wrong argument count for a function call |
| `return value of 'f': type 'string' is not assignable to 'int'` | a return value does not match the declared return type |
| `function 'pick' must return a value on every path` | a branch falls through without a `return` |
| `left operand of '+' is possibly null` | arithmetic on a `T?` that is not narrowed |
| `value of type 'Node?' is possibly null — narrow it before reading '.value'` | a field read on a nullable value |
| `type 'Point' has no field 'ex' (did you mean 'x'?)` | unknown field |
| `undefined name 'cont' (did you mean 'count'?)` | unknown name |
| ``` `new` of unknown type 'Nope' ``` | `new` on an undeclared type |
| `cannot instantiate abstract type 'Shape'` | `new` on an `abstract` type |
| `constructor 'Counter' takes 1 argument(s), got 2` | constructor argument count |
| ``` pure `fn poke` mutates state — declare it `mut` ``` | a `fn` writes a field |
| ``` cannot reassign immutable binding 'i' (declared with `be`) ``` | assignment to a loop variable or a `be` binding |
| `use of moved value 'a' — a `unique` value moved away at 8:13 leaves nothing behind` | use of a `unique` value after a move |
| `'key' is private to module 'secret'` | `module::name` of a module-private function |
| `'m_n' is private to class 'Counter'` | a `private` field or method used outside its class |
| `template 'twice' cannot share its name with another 'twice' declaration` | a template and another declaration of one name |
| `no overload of 'f' matches the argument types` | no overload fits the call |
| `duplicate function 'f' — two overloads have identical parameter types` | exact repeat of an overload |
| `overloaded function 'greet' cannot have default parameter values` | defaults on an overloaded name |
| `` `break` outside a loop `` | `break` or `continue` outside a loop |
| `` `in` wants a string or an array on the right, got 'int' `` | `in` on an unsupported right-hand type |
| `struct pattern cannot match on `float` field 'x' yet` | `float` field in a struct pattern |
| `cannot add 'string' and 'int' (use string(x) or an interpolated string)` | `+` between a string and a number |
| `undefined function 'secret::nope'` | `module::name` that the module does not declare |
| `struct bodies hold fields only; use a class for methods` | a `fn` inside a `struct` |
| `'?' applies to calls, not indexing` | `xs[i]?` |
| `struct pattern type 'Z' is unrelated to subject type 'P'` | struct pattern of an unrelated type |

## Known gaps

These were found while writing this page. The compiler's behaviour is shown
as observed; the design reference describes a stricter rule for some of them.

- **Template bodies are only checked per instance by the code generator.**
  A template's body is not analysed on its own. Each instantiated copy is
  generated with the argument types, so `let q: string = a + b` inside
  `fn bad(a, b)` is accepted for `bad(1, 2)`, and a wrong call inside the body
  surfaces only when that copy is generated. A `module.fn(...)` call inside a
  template body is not rewritten to its linked symbol by the analyzer.
  Checking each instantiation needs a clone of the template body, because the
  instances share one body today.
