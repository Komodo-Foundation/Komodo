# Komodo — Language Reference

A catalogue of every syntax and feature the language currently has. Reference
only, for keeping the implementation and sample code coherent.

## Files & modules

- `.km` — program (has a `main`). `.kmm` — module (library).
- `module <name>` — first line of a module file. All files declaring the same
  module name are auto-merged into one namespace.
- A module body may carry **`public` / `private` section labels**, like a
  class:

  ```
  module greet
  public
      fn hello(who: string): string
          return banner() + who
  private
      fn banner(): string
          return "Hello, "
  ```

  A `private` member is module-local: reaching it through the module name
  from *another* module (`greet::banner()`) is a compile error. Members
  in the same module call it freely, and a member with no section is
  `public`. Enforcement covers functions reached as `mod::name`; it does
  not (yet) cover private globals or types.
- `import <name>` — pull in a module. `<name>.kmm` is resolved from the
  directory tree of the file being compiled (and, as a fallback, the
  compiler's own directory, which is where the standard library lives);
  then every `.km` / `.kmm` **anywhere under that file's directory**
  declaring the same `module` is merged in too — so a module's files may
  be grouped into sub-folders (the compiler's own sources are, under
  `modules/frontend/`, `modules/analyze/`, `modules/codegen/`, …).
  Applied transitively, everything reached is compiled as one unit, so
  `kmc main.km` builds the compiler with no concatenation step.
- Members are reached as `name::member` (scope) or `name.member`. After the
  merge namespaces are flat, so `name::member` resolves to `member`.
- Entry point: `void main()` or `void main(args)`; `args` is a string array,
  `args[2]` is the first user argument, `#args` its count.

## Lexical

- Blocks are indentation-based: an indent opens a block, a dedent closes it.
  Newlines are significant.
- Comments: `//` to end of line, `/* ... */` block, `/** ... */` doc.
- Identifiers: letters, digits, `_`; may not start with a digit.

## Literals

| Kind    | Examples |
|---------|----------|
| int     | `0`, `42`, `256`, `0xFF` (hex), `0b1010` (binary), `1_000_000` (`_` digit separators, any base) |
| float   | `2.0`, `3.14`, `2.0f`, `2f` (trailing `f` forces float) |
| string  | `"hi"`, `"a\nb"` (backslash escapes) |
| interp. string | `` `x = {a + 1}` `` — backtick-delimited; each `{expr}` is spliced in as `string(expr)` |
| char    | `'a'`, `'\n'` |
| bool    | `true`, `false` |
| null    | `null` |
| array   | `[]`, `[4, 15, 8]` |
| range   | `a to b` (usable as a value) |

An **interpolated string** is written with backticks: `` `Score {n}` ``. The
lexer rewrites it in place to `("Score " + string(n) + "")` — an ordinary
concatenation the rest of the pipeline never sees as special, so `{…}` may
hold any expression (`` `{a > b and "hi" or "lo"}` ``) and the result is a
plain `string`. Inside the text the usual `\n` / `\t` / `\\` escapes work,
a literal backtick is `\` + `` ` ``, and `\{` / `\}` write literal braces.
There is no format-spec syntax; a `float` prints in `string(x)`'s `%g`
form, so round it first if you want fixed decimals.

## Types

- Primitives: `int`, `uint`, `float`, `bool`, `string`, `char`.
- Fixed-width integer containers: `u8`/`u16`/`u32`/`u64`, `i8`/`i16`/`i32`/`i64`.
- Fixed-width floats: `f64` (a spelling of `float`) and **`f32`**. An `f32`
  is arithmetically identical to `float` everywhere inside Komodo — it is
  held and computed as a 64-bit double, shares every code path, and
  `f32`↔`float`↔`int` mix freely. Its 32-bit width matters only at a
  boundary: a `struct` field declared `f32` occupies 4 bytes in the C
  layout used for a by-value FFI argument (see *Foreign functions*), and
  an `extern` parameter / return typed `f32` binds a C `float` (not
  `double`). Use it to mirror a C struct like raylib's `Vector3`.
- Array: `[T]` (e.g. `[int]`, `[[Token]]`).
- Nullable: `T?` (e.g. `char?`, `string?`, `Expression?`).
- Annotations are written `name: Type`, after params, vars, members, and as
  function return types.
- **`float`** is IEEE-754 double throughout — locals, parameters, returns,
  struct / class fields, and array elements. `+ - * / %`, all six
  comparisons, and unary `-` work on it; an `int` operand in a mixed
  expression is widened. Where a numeric type is *inferred* from a mix,
  `float` wins over `int` (`[1, 2.5]` is `[float]`; `cond and 1 or 2.5`
  is `float`). A `float` prints in `%g` form but keeps a trailing `.0`
  when it is integral (`4.0`, not `4`), so it always reads back as a float.

## Variables

```
let x = expr            // reassignable
let x be expr           // immutable binding
let x: Type = expr      // explicit type
let x be expr           // (also: `let x: T be expr`)
```

`let NAME be expr` declares a compile-time **macro**: every use of `NAME` is
replaced by a fresh lowering of `expr`. No storage is allocated for the name,
and `NAME` cannot be reassigned. The right-hand side may refer to macros
already in scope.

Macros are **block-scoped** and may be declared anywhere a statement is
allowed — at module top level, in a function body, or inside an `if` / `while`
/ `for` block. A macro is visible from its declaration to the end of its
enclosing block; an inner declaration shadows an outer macro (or a local) of
the same name for the rest of that block. Because each use re-lowers `expr`,
a macro whose body reads a variable that later changes sees the new value at
each site — keep macro bodies to constants and pure expressions.

```
let WIDTH be 4
let AREA be WIDTH * 3        // each use of AREA re-evaluates `WIDTH * 3`

void main()
    let N be 10
    if WIDTH > 0
        let N be 99         // shadows the outer N within this block only
        print(N)            // 99
    print(N)                // 10
```

## Functions

```
fn name(a: int, b: int) : int       // pure value function — no mutation
void name(x: string)                // procedure — returns nothing
mut name(): [Token]                 // mutates receiver / instance state
```

- Parameters: `name: Type`, may have default values — `fn greet(name: string, greeting: string = "hello")`.
  A default must be a literal (optionally negated), assignable to the declared
  type, and only trailing parameters may have one; a call may omit them. Defaults
  apply to free functions (including generic ones); an overloaded name cannot
  take defaults, so overloads never compete with defaulted arities. A trailing `...`
  makes the function **variadic** — the call may pass any number of extra
  arguments after the fixed ones (`void console::print(...)`).
- Return type follows `:`. `return expr` / bare `return`.
- A **body-less** `fn` / `void` / `mut` (a header with no indented block)
  is a **prototype**: the signature is declared here and the definition
  is supplied at link time, using the ordinary Komodo call ABI. This is
  how the standard library declares itself (see *Built-ins*); it differs
  from `extern fn`, which uses the C ABI.
- Mutation contract (compile-time checked): a pure `fn` may not contain
  mutating code, and a `mut` function/method must. "Mutating code" means a
  write the caller can observe — a field write (`obj.f = v`, or bare
  `field = v` against the receiver) or an array-element write
  (`arr[i] = v`). Reassigning the function's own locals does not count,
  and the check does not follow calls. `void` procedures and constructors
  are unconstrained.
- **Overloading** — a *free* function may be declared more than once under
  one name with different parameter lists (by count or by type). An exact
  repeat of the normalized parameter types is an error. A call is resolved
  from the static argument types; when a base-typed object argument leaves
  several candidates viable, the compiler emits a runtime header type-id
  check that dispatches to the matching overload (most-specific first, and
  `abort`s if none matches at run time). Two equally-good static matches
  are an "ambiguous call" error. Methods, constructors, `extern` and
  prototype functions do not overload.
- **Generic (implicit-type) functions** — a *free* function with one or
  more un-annotated parameters (`fn add(a, b)  return a + b`) is a
  *template*: it is never emitted on its own. Each call site classifies
  its argument types and the compiler monomorphizes a specialized copy, so
  `add(1, 2)` is an integer add and `add("a", "b")` a string concat.
  Typed and untyped parameters may be mixed (`fn scale(v, k: int)`). A
  template does not also overload (a name clash with any other declaration
  of that name is an error).

## Structs

```
struct Token
    type: TokenType
    lexeme: string
    line: int = 0                    // default values allowed

abstract struct Expression : ASTNode        // abstract base, inheritance
abstract shared struct ASTNode             // `shared` modifier (the default)
unique struct FileHandle                    // `unique` modifier — single owner
struct Flags : u32                          // fixed-width int container
```

- Inheritance: `struct Child : Parent` — child fields follow parent fields.
- Instantiating an `abstract` type is an error.
- `new Name(a, b, …)` builds a heap value, binding args to fields in
  declaration order (unset fields are zero); `obj.field` reads/writes a
  field; a struct is passed and returned by handle.
- Ownership modifiers (before `struct`, combinable with `abstract`):
  - `shared` — the default: values alias freely, `let b = a` gives two
    handles to one value.
  - `unique` — the value has exactly one owner. Binding it (`let b = a`),
    assigning it (`x = a`), storing it into a field or array element, or
    returning it *moves* it; the source name is then invalid and any read
    of it is an error until a fresh value revives it (`a = new …`). The
    runtime representation is identical to `shared` — this is a
    compile-time check only. `unique` is inherited by subtypes. The
    analyzer tracks ownership per local binding, so moving a `unique`
    value *out of* a field, array element, or global is rejected; move the
    owning value instead. Passing a `unique` value to a function is a
    borrow, not a move (the callee is not tracked across the call).

## Classes

```
class Lexer
public
    constructor(src: string)
        m_src = src

    mut tokenize(): [Token]
        ...
private
    m_src: string
    m_pos: int = 0                   // fields require a type annotation
```

- `public` / `private` section labels.
- `constructor(params)`; instance state held in fields.
- `mut` methods may mutate fields; non-`mut` may not.
- Inside a method / constructor an unqualified name that matches a field
  resolves against the receiver.
- `new C(args)` allocates the instance and runs `constructor`;
  `obj.method(args)` passes the receiver as an implicit first argument.
- `abstract class`, inheritance `class C : Base`, virtual dispatch (a
  base-typed field may call a subclass override).

## Enums

```
enum TokenType
    EOF
    NEWLINE
    IDENT
```

- Variant access: `TokenType::IDENT`.

## Expressions & operators

Precedence low → high: `or`, `and`, `is` / `in`, comparison, bitwise,
additive, multiplicative, unary, postfix.

- Arithmetic: `+ - * / %`
- Comparison: `== != < <= > >=`
- Logical: `and`, `or` (short-circuit), `not`, `!`
- Bitwise: `& | ^ << >>`; keyword `xor` is an alias for `^`
- Conditional value: Lua-style `cond and whenTrue or whenFalse` (built from
  short-circuit `and`/`or`); there is no C-style `?:` operator
- Unary: `-x`, `not x` / `!x`, `#x` (length of array or string)
- Assignment: `x = v` and the compound forms `x += v`, `x -= v`, `x *= v`,
  `x /= v`, `x %= v` (`lhs <op>= v` ≡ `lhs = lhs <op> v`) — on a plain
  name, `obj.field`, or `arr[i]`. `+=` also appends to an array / global
  array (`arr += [x]`, `arr += other`) and concatenates strings.
- Type test: `expr is TypeName`
- Membership: `x in coll` — a bool. `coll` a string ⇒ substring test
  (`"at" in "cat"`, a `char` left operand is a 1-char string); `coll` an
  array ⇒ element test by value (`==`, `strcmp` for `[string]`).
- Grouping: `( expr )`
- Construction: `new Type(args)`

### Postfix

- Call: `name(args)`
- Method / member: `recv.method(args)`, `recv.member`
- Scope: `namespace::name` (e.g. `file::read_all`, `TokenType::EOF`)
- Index: `arr[i]` — out-of-range throws; guard with `#arr`
- Slice: `arr[start to end]`
- Protected call: `name(args)?` — yields `null` if the call throws

### Strings

- `+` concat, `*` repeat (`"ab" * 3`), `#s` length, `==` / `!=` compare,
  `s[i]` char access, `.length` property.

### Arrays

- Literal `[a, b, c]`, `.length` property / `#arr`.
- `arr += [x]` append element, `arr += other` concatenate.

## Statements

```
if cond then                 // block form
    ...
else
    ...

if cond then doThing()       // one-liner

while cond do
    ...

for i = 1 to 5               // inclusive ascending
for i = 3 down to 1          // descending
for i = 0 until n            // exclusive upper bound
for x in xs                  // for-each

break
continue
return expr
```

- `then` and `do` are interchangeable as the block-opening keyword.

### match

The `case` arms are indented under `match`; each arm has an indented body.
An arm's pattern is a value expression (matched with `==`, or string
equality when it is a string literal) or a **struct pattern**.

```
match text
    case "true"
        type = TokenType::TRUE
    case "if"
        type = TokenType::IF
    else
        type = TokenType::IDENT
```

**Struct patterns** — `case Type(field: sub, …)` — match when the
subject's runtime type is `Type` (or a subtype) and every field
sub-pattern matches. A sub-pattern is either a **literal** (compared to
that field with `==`) or a **binding**: a bare identifier that introduces
a local bound to the field's value for the arm body. `field: name` binds
`subject.field` to `name`; `name` on its own is shorthand for
`name: name`.

```
match user
    case User(name: "admin", role: "superuser")
        …                                   // both fields equal
    case User(role: "admin", name)
        print("admin: " + name)             // matches role, binds name
    case User(name, role, age)
        …                                   // binds all three
```

The subject's type must be in `Type`'s hierarchy; unknown fields, an
unrelated pattern type, and (for now) `float` fields are compile errors.
Struct patterns do not yet nest, and do not contribute to enum
exhaustiveness — add an `else` (or a bare `case Type(…)`) for a fallback.

### Errors

- `throw <expr>` — raise an error carrying a message (a string). Valid in
  statement or expression position. Control does not return from a `throw`.
- `expr or throw "message"` — yield `expr`, or throw if it is null/false.
- `f(args)?` — a **protected call**. If an error is thrown out of the callee
  (transitively), the call expression evaluates to `null` instead of
  propagating. The `?` binds to a call's closing `)` and also applies to
  method calls (`obj.m(args)?`) and construction (`new T(args)?`).
- An error that reaches the top of the program with no enclosing `?` prints
  `Uncaught error: <message>` to stderr and exits with status 1.

```
fn parse_port(s: string) : int
    let n = int(s)
    if n < 1 or n > 65535 then
        throw "port out of range"
    return n

let port = parse_port(userInput)?      // null if it threw
if port == null then
    port = 8080
```

x86: a runtime `jmp_buf` stack (`km_throw` / `km_try_push` / `km_try_pop`
in `stdlib/km_error.c`, armed with `_setjmp` at each `?` site). The
stackvm backend emits `THROW` / `TRY` / `ENDTRY` opcodes.

## Foreign functions (C FFI)

```
extern fn SDL_Init(flags: int): int
extern void SDL_Quit()

extern type SDL_Window              // an opaque, pointer-sized C handle

extern "SDL2"                       // a block: one library, many decls
    type SDL_Renderer
    fn  SDL_CreateWindow(t: string, x: int, y: int, w: int, h: int, f: int): SDL_Window
    fn  SDL_CreateRenderer(win: SDL_Window, index: int, flags: int): SDL_Renderer
    void SDL_DestroyWindow(win: SDL_Window)
```

- `extern fn` / `extern void` — a prototype only, no body. The symbol is
  resolved by the linker under its raw C name. A trailing `...` makes it
  variadic (`printf`).
- **`extern type Name`** — an *opaque handle*: a distinct, pointer-sized
  nominal type with no fields. It cannot be built with `new` (a C function
  hands you one), supports only `==` / `!=` (`h == 0` for a NULL check —
  never arithmetic), and is not interchangeable with `int` or with another
  handle type. At run time it is just a machine word, so it costs nothing.
  An explicit base (`extern type Fd : int`) is accepted and ignored.
- **`extern "lib"` block** — one indented group of `type` / `fn` / `void`
  declarations that all belong to library `"lib"`. The header string is
  also the link directive: a bare name links `-lname`, a path or flag
  (`"vendor/foo.c"`, `"-lm"`) is passed to the linker as-is. Unlike the
  `link` keyword it needs no enclosing `module`, so a one-off program can
  bind a C library inline; the driver records it into `<file>.links` and
  `komodo` links it automatically.
- Calls use the full native System V AMD64 C ABI (not Komodo's
  `(tag,value)[]` ABI):
  - integer / pointer / `string` / handle args fill `rdi, rsi, rdx, rcx,
    r8, r9`, then spill to the stack — any number of arguments;
  - `float` args fill `xmm0..7`, then spill; a Komodo `float` is a
    64-bit IEEE double, so the C prototype it binds to must say `double`
    (not `float`) for every such parameter and for a `: float` return;
  - `al` is set to the number of xmm registers used, so C varargs
    (`printf`, …) work;
  - `: float` return type comes back in `xmm0`; any other return type
    (pointer / handle included) comes back in `rax` as a plain 64-bit int.
- **C callbacks.** A bare Komodo function name passed as an `extern` call
  argument is handed to C as a function pointer: the compiler emits a
  System V ABI *trampoline* that repackages the integer-register arguments
  into Komodo's call ABI and invokes the function. The target may take up
  any number of parameters, each an int / bool / string / handle word or
  a `float` / `f32` (a C `double` / `float`), and return one of those or
  `void` — no arrays. Parameters beyond the six integer / eight SSE
  registers are read from the C stack, so a callback may take, say, ten
  arguments. See `examples/ffi_callback.km` and
  `examples/ffi_callback_ext.km`.
- `data(arr)` — the element pointer of a Komodo array, for passing a
  writable buffer / out-param to C (e.g. `SDL_PollEvent(data(ev))`). Read
  results back with ordinary `arr[i]`.
- **Structs by value — `copy(x)`.** An `extern` argument written
  `copy(structVal)` is passed *by value*: the compiler repacks the
  Komodo object (one 8-byte slot per field) into the tight C layout its
  declared field types imply — `f32`→4 bytes, `i32`/`u32`→4, `u8`→1,
  `float`/`int`→8 — in a static scratch buffer, then places it per the
  System V AMD64 aggregate rules:
  - ≤ 8 bytes → one eightbyte; ≤ 16 → two. Each eightbyte is SSE (an
    `xmm` register) when every field in it is floating-point, otherwise
    INTEGER (a GP register); it spills to the stack when the register
    file is full. So `Vector2`→one `xmm`, `Vector3`→two `xmm`,
    `Color{u8×4}`→one GP reg, `Rectangle`→two `xmm`.
  - 17–192 bytes → MEMORY class: the whole aggregate goes in the stack
    argument area, consuming no registers (this is how a 44-byte
    `Camera3D` or a 64-byte `Matrix` is passed).
  An `extern fn` whose declared **return** type is such a struct (≤ 16
  bytes) is unpacked from `rax:rdx` / `xmm0:xmm1` into a fresh Komodo
  object. A struct argument *without* `copy(...)` still passes the bare
  handle as a pointer (for a `T*` parameter), as before. Nested struct
  fields and arrays are not supported; see
  `stdlib/raylib.kmm` for a shim-free binding built on this.
- **Struct returns over 16 bytes** (System V MEMORY class, e.g. a 32-byte
  `{double,double,double,long}`): the compiler passes a hidden pointer to
  a static buffer as the first integer argument (so the visible
  arguments shift by one register) and unpacks the filled buffer into a
  fresh Komodo object, exactly like the register case. Same field
  restrictions (scalars only, up to 192 bytes) — see
  `examples/ffi_sret.km`. (The buffer is shared, so the result must be
  consumed before the next struct-returning call, which it always is.)
- **Global variables — `extern let name: T`.** Reads and writes go
  straight to the C symbol (`extern let counter: i32`,
  `extern let scale: float`); the declared type is the C width — `i8`
  ... `u64`, `bool`, `f32` (C `float`), `float` (`double`), `string`
  (`const char *`), or a handle — widened on read and narrowed on write,
  so `counter += 1` on a C `int` is correct. Add `= "sym"` to bind a
  differently spelled symbol. Arrays and structs are rejected. Works in
  `extern "lib"` blocks too. See `examples/ffi_extern_var.km`.
- With `build.sh` (the interpreter path) pass link flags after the output
  name — `./build.sh app.km x86 ./app -lSDL2`; the `komodo` CLI reads
  `<file>.links` and needs none.

### Exporting to C and C++ (`export fn`)

The reverse direction: a Komodo function callable *from* C / C++.

```
export fn add(a: int, b: int): int
    return a + b

export fn greet(name: string): string
    return "hello, " + name

export void tick()
    ...
```

- `export` prefixes an ordinary free function **with a body**. It is a
  contextual keyword (still a plain identifier elsewhere). The function
  keeps working as a normal Komodo function; additionally the compiler
  emits a native System V AMD64 entry point under the **unmangled** symbol
  `add`, which repackages the C arguments into Komodo's `(argc, KmArg*)`
  call ABI (the mirror image of the callback trampoline) and calls the
  real body, which lives under an internal symbol (`add__kmx`).
- Parameters and the return type must be machine words or doubles:
  `int` / `uint` / `i8..i64` / `u8..u64` (narrow values are re-extended
  from the low bits, as C leaves the rest of the register undefined),
  `bool`, `float` (`double` in C), `f32` (`float`), `string`
  (`const char *`, NUL-terminated), or an `extern type` handle (an opaque
  pointer). Arrays, structs and classes are rejected by the analyser.
  Any number of arguments is fine (stack-passed beyond the registers).
  Overloading, generics (untyped parameters), default values and the
  name `main` are rejected: a C symbol names exactly one function.
- Ownership: a returned `string` points into the Komodo heap. Copy it
  before calling back into Komodo; do not `free` it. An unhandled
  `throw` inside an export aborts the process (there is no C++ exception
  mapping).
- **Runtime start-up.** If the program has a `main`, the C runtime's own
  `main` has already set everything up. If it has **none**, the first
  export call lazily initialises the collector and runs the top-level
  `let` initialisers (a once-flag in the wrapper; not thread-safe, so
  make the first call from one thread). The C/C++ side then supplies
  `main`.
- Only the **x86** target implements it; `-t c` / `-t stackvm` report
  `export fn is not supported on target 'X' yet`.

**Building a library** — `komodo --lib prog.km` writes `prog.o` and
`libprog.a` (the program plus the Komodo runtime, with the runtime's own
`main` renamed so it cannot clash with yours) and links nothing:

```bash
komodo --lib --emit-header prog.h prog.km
gcc -no-pie main.c -L. -lprog -l:libgc.so.1 -lm        # C
g++ -no-pie main.cpp -L. -lprog -l:libgc.so.1 -lm      # C++
```

`-no-pie` is required (the generated code is not position-independent).
Alternatively let `komodo` link it for you: a program with no `main`
takes the C/C++ `main` as an extra, `komodo prog.km -- driver.cpp`.

**Headers** — `komodo --emit-header out.h prog.km` writes one prototype
per `export fn` inside `#ifdef __cplusplus extern "C" { #endif`, with
`int64_t` / `double` / `const char *` / `bool` / `float` mappings, plus a
`typedef void *Name;` for every `extern type` an exported signature
mentions. Used alone it only writes the header; with `--lib` / `-c` it
also builds.

### Binding C++ and oddly named C symbols

`extern fn nice(x: int): int = "_Z3fooi"` binds the Komodo name `nice`
to the symbol `_Z3fooi` — a C++ mangled name, or a C name that is not a
valid Komodo identifier (a Komodo keyword such as `match`, say). The
`= "symbol"` suffix works on `extern fn` / `extern void` and on the
members of an `extern "lib"` block; the symbol may contain letters,
digits, `_ $ @ ?`. C++ *classes* are reached the usual way: write a
small `extern "C"` wrapper (`counter_new`, `counter_add`, …) and bind
those, with the class held as an opaque `extern type` handle — see
`examples/ffi_cxx.km`.

C++ sources are built by `komodo` itself: `.cpp` / `.cc` / `.cxx` files
after `--` (or named by a module's `link "x.cpp"`) are compiled with
`g++` (cached by content, the headers beside them and the toolchain) and
linked with `-lstdc++`. `-I…` / `-D…` / `-std=…` extras are forwarded to
the compile.

## Built-ins

A handful of names the compiler lowers directly (no `import`, no library):

- `print(...)` — write values. A `float` argument is shown in `%g` form
  with a trailing `.0` kept when integral.
- `string(x)` — convert any value to its text form (same `float` rule).
- `int(x)` — narrow a number to a 64-bit signed integer: a `float` /
  `f32` is truncated toward zero (`cvttsd2si`), an integer-class value
  passes through unchanged. This is the sanctioned float→int conversion;
  a plain assignment / return stays width-checked and rejects it.
- `assert(cond)` / `assert(cond, "message")` — when `cond` is false, throws
  `file:line: assertion failed: message` (an ordinary error: catchable with `f(...)?`,
  otherwise `Uncaught error: …`). The message is evaluated only on failure. No value.
- `join(parts, sep)` — concatenate a `[string]` with `sep` between, in
  one pass (avoids the O(n²) churn of repeated `s = s + line`).
- `data(arr)` — an array's element pointer, for handing a buffer to C.
- `copy(x)` — a deep copy of `x`. Struct / class fields are cloned
  recursively (through a synthesised `km_deepcopy_<T>` per value type),
  array fields get a fresh buffer; scalars, strings, enum values and
  opaque handles are copied by value. The source is *read*, never moved —
  `copy(a)` is the explicit way to duplicate a `unique` value while
  keeping `a` a live owner. Constructor-based classes and `extern type`
  handles have no deep copy; cyclic data recurses forever.

## Standard library

The standard library has **no special treatment** — each part is an
ordinary Komodo module, resolved and type-checked like any other. Each
ships as a `.kmm` file of prototype declarations (bodies linked from
`stdlib/*.c`), found automatically under the compiler's own directory, so
`import console` / `import math` / `import string` / `import algorithm` /
`import file` work out of the box from anywhere.

| Module | Members |
|---|---|
| `console` | `print`, `write`, `error` (all variadic) |
| `math` | ints: `abs`, `min`, `max`, `pow`, `gcd`, `clamp`, `sign`; floats: `sqrt`, `floor`, `ceil`, `round`, `trunc`, `fabs`, `sin`, `cos`, `tan`, `atan`, `atan2`, `exp`, `log`, `log2`, `log10`, `powf`, `hypot`; constants `pi()`, `e()` |
| `string` | `length`, `split`, `join`, `trim`, `trim_start`, `trim_end`, `replace`, `index_of`, `contains`, `starts_with`, `ends_with`, `to_upper`, `to_lower`, `repeat`, `reverse`, `substring`, `pad_start`, `pad_end` |
| `algorithm` | `sort`, `reverse`, `sum`, `max`, `min`, `contains`, `index_of`, `fill` — each **overloaded** for `[int]` and `[float]` (same call, resolved on the array's element type) |
| `file` | `exists`, `is_dir`, `read_all`, `read_lines`, `write_all`, `append_all`, `find`, `list_dir` |
| `sdl2` | a System-V-ABI subset of SDL2 (window / renderer / events) — see `stdlib/sdl2.kmm` |

`math` / `string` / `algorithm` members carry a module-name prefix on
their symbol (`math_abs`, `string_split`, `algorithm_sort`), so they are
reached with a **dot** — `math.abs(x)`, `string.split(s, ",")`,
`algorithm.sort(xs)`. A dotted call whose `"_"`-joined name is an
overload set (as every `algorithm` op is) resolves like any other
overload. `file` and `console` are bare symbols reached with the `::`
scope operator that drops the namespace — `file::read_all(path)`. (The
bare `string(x)` value-to-text built-in is unrelated to `import string`.)

To add another library, drop a `name.kmm` (prototypes) plus its
implementation object next to the others — nothing in the compiler needs
to change. For a *system* library there's no object to drop in: write the
prototypes as `extern fn` (native C ABI — see "Foreign functions" above)
and declare what the linker needs with **`link`** — a keyword, valid only
inside a module (a file starting with `module <name>`; the parser rejects
it anywhere else):

```
module sdl2

link SDL2

extern fn SDL_Init(flags: int): int
…
```

- `link Name` — a bare identifier — becomes the linker flag `-lName`.
  When a `Name.pc` exists, the `komodo` CLI expands it through
  `pkg-config` — both `--static --libs` (the `-L` prefix and every
  private transitive dependency) and `--cflags` (the `-I` for a
  headers-in-a-prefix install), so a module's companion C shim can
  `#include <lib.h>`.
- `link "path/to/file.c"` — a quoted string — names a companion source
  file rather than an installed system library. It is resolved against
  the directory of the module file that declared it (unless already
  absolute), so a library can ship its C shim next to its `.kmm` and
  still be `import`ed from any directory.

`kmc`'s driver resolves every `link` reachable from a program (the same
traversal it uses for `import`) into ready linker tokens and writes them
to `<file>.links`; the `komodo` CLI just reads that file and adds them to
the `gcc` step — no scanning, no special-casing in the compiler or the
CLI beyond parsing the keyword itself. `import sdl2` is enough on its
own; no `-lSDL2` on the command line. This is how `stdlib/sdl2.kmm` links
itself — copy the pattern for any other C library.

## Semantic analysis

Between the parser and the code generator the compiler runs a dedicated
**static analysis pass** (`modules/sema*.kmm`, driven from
`km_compiler::compileWith`). It is the single authority on the language's
rules; the code generator only lowers a tree the analyser has already
accepted and annotated.

What it does:

- **Type inference.** Every expression is resolved to one static `Type`
  (`modules/types.kmm`). An unannotated `let` binding takes its
  initialiser's inferred type. The coarse form is written back onto the
  AST (`Expression.resolvedType`) so codegen never re-derives it.
- **Flow-sensitive narrowing.** `if x == null` / `x != null` /
  `x is T` / `not (…)` refine a binding's type within the branch it
  guards; a branch that provably diverges (`return` / `throw` / `break` /
  `continue`) narrows the *rest* of the enclosing block. So after
  `if p == null then return`, `p` is non-null for the code that follows.
- **Null-safety.** `T` is non-nullable; `T?` is nullable. Reading a
  field / calling a method / indexing / doing arithmetic on a possibly-
  null value is an error until it is narrowed. An object-typed field with
  no non-null default is treated as `T?` (a fresh `new` zero-fills it).
- **Assignability** on initialisers, returns, assignments, call
  arguments, `new` field bindings — with numeric widening (`int`→`float`,
  among integer widths) and subclass→base. `extern` calls use the looser
  native-ABI rule (any integer-class value fills an integer-class
  parameter; `float` stays distinct).
- **Signatures.** Arity and result type for every free function, method,
  constructor, `extern` prototype, and the modelled standard library
  (`console` / `algorithm` / `math` / `file`, `string`, `data`).
- **Structural rules.** "Returns a value on every path", the `fn` / `mut`
  mutation contract, `break` / `continue` only inside a loop, no
  instantiation of an `abstract` type, no duplicate function / type /
  macro, single-inheritance cycle detection.
- **Ownership.** A `unique struct` value has one owner: binding, assigning,
  storing or returning it moves it, and reading the moved-from name is an
  error until a fresh value revives it. Move state is merged conservatively
  across `if` / `match` arms and loop bodies (moved on any path ⇒ moved
  after).

A semantic error is reported as `line:col: message` and stops
compilation.

### Strict mode

Passing `strict` as a driver argument (`main.km <file> <target> strict`)
additionally reports, as hard errors: a binding with neither annotation
nor initialiser, a local / parameter that is never used, unreachable
code after a diverging statement, and a `match` on an enum that lacks an
`else` and does not cover every variant.  The default checks above always
run.

## Optimization

After analysis, an AST-rewriting pass (`modules/optimize.kmm`) runs at
one of four levels, chosen by a driver argument
(`main.km <file> <target> none|low|medium|max`, default `none`; `KM_OPT`
env for `build.sh` / `krun.sh`):

| Level | Adds |
|---|---|
| `none` | nothing — literals and branches pass through verbatim |
| `low` | constant folding (`+ - * / %`, comparisons, string `+` / `*`, `not`, `#[…]`, `and` / `or` with a literal guard); the x86 backend also interns identical `.rodata` constants |
| `medium` | + algebraic identities (`x+0`, `x*1`, `x*0`, `x-0`, `x/1`), dead-branch elimination (`if`/`while`/`match` on a constant), and dead code after a diverging statement |
| `max` | + constant propagation of never-reassigned `let x = <literal>` bindings, then another fold |

Every rewrite preserves observable behaviour.  Folds are skipped where
the interpreter (which runs the pass at bootstrap) and the native runtime
would disagree — division by zero, results outside a safe i64 range,
string literals carrying backslash escapes, and the bitwise operators
(`^` is exponentiation in interpreter expression context but XOR
natively).  A folded boolean is emitted as the int `1` / `0`, so
`print` still shows it the way the comparison it replaced was shown.

## Memory

Komodo is **garbage-collected**. Every heap value — struct / class
instance, array, string — lives on a conservative collected heap (the
Boehm–Demers–Weiser collector, `libgc.so.1`, linked into every program)
and is reclaimed once nothing reaches it. There is no `free`, and nothing
can be freed too early: a value stays valid for as long as anything
refers to it. Cycles are collected like everything else.

- **Roots** are the stack, the registers and the program's data / bss
  (top-level `let`s). Objects and arrays are scanned for pointers;
  string bytes are allocated *atomic* (never scanned).
- **Layout.** An object handle points at `[ type_id | field0 | … ]`; an
  array handle at `[ len | cap | elements… ]`. `new` and array literals
  call `km_alloc` (zeroed), C stdlib code builds strings with
  `km_alloc_atomic` / `km_strndup` (`stdlib/km_runtime.h`).
- **Growth.** `arr += …` grows in place with doubling; when the buffer
  moves, the old one is left to the collector, so another alias of the
  array stays valid (it no longer sees the appends).
- **C interop.** `data(arr)` is an interior pointer, which the collector
  honours, so the array stays alive while that pointer is on the stack or
  in the program's / a library's static data. Memory owned by C (libc
  `malloc`, a library's buffers) is *not* scanned: if C keeps a pointer
  into Komodo data beyond the call, keep a Komodo reference to it too.

**Heap ceiling.** A program may use at most 70 % of the memory available
to it — physical RAM, or the cgroup / container limit when that is lower
(e.g. `systemd-run -p MemoryMax=4G`). Set `KOMODO_MAX_HEAP` to change it
(bytes, or with a `K`/`M`/`G`/`T` suffix, powers of 1024: `2G`, `512M`;
`0` = no ceiling). When the live data no longer fits — or the system
refuses memory — the program stops with one line on stderr and exit
status **3**; generated code is never handed a null allocation:

```
Komodo: out of memory - heap limit 10.4 GB reached in Mesh::rebuild (raise it with KOMODO_MAX_HEAP)
```

**Runtime errors.** `arr[i]` / `arr[i] = v` check `0 <= i < #arr` inline
(one compare); out of range throws an ordinary Komodo error, so `f(...)?`
can catch it, and uncaught it reads
`Uncaught error: array index 5 out of range (length 3) in parse`
(status 1). String indexing `s[i]` is not checked (`s[#s]` is `""`).

**Crashes.** A hardware fault never ends in a silent core dump. The
runtime handles SIGSEGV, SIGBUS, SIGFPE, SIGILL and SIGABRT on its own
signal stack (so a stack overflow is caught too), prints what happened,
where, and the Komodo call stack — direct recursion folded into one line
— then exits with 128 + the signal number (139 for SIGSEGV), the status a
shell shows for that signal:

```
Komodo: crash - stack overflow (recursion too deep) in depth
Komodo call stack (innermost first):
    depth
    depth   (x 130979, recursive)
    main
```

Other headlines: `invalid memory access (a null value was used)`,
`invalid memory access at address 0x…`, `integer division by zero`,
`bus error`, `illegal instruction`, `aborted`. Function names come from
the executable's own symbol table (so don't strip it); C frames are
unwound through their unwind tables, generated code through its rbp
frame chain.

Code built by a compiler from before the collector (it allocates with raw
`malloc` and lacks the `km_abi_gc` marker) still links and runs, with
collection switched off — that keeps bootstrapping from an older `kmc`
working (`build-self.sh` twice: the first run reports a fixpoint mismatch
because the code generator changed, the second reaches the fixpoint).
