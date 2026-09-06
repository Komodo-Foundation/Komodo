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
| int     | `0`, `42`, `256` |
| float   | `2.0`, `3.14`, `2.0f`, `2f` (trailing `f` forces float) |
| string  | `"hi"`, `"a\nb"` (backslash escapes) |
| char    | `'a'`, `'\n'` |
| bool    | `true`, `false` |
| null    | `null` |
| array   | `[]`, `[4, 15, 8]` |
| range   | `a to b` (usable as a value) |

## Types

- Primitives: `int`, `uint`, `float`, `bool`, `string`, `char`.
- Fixed-width integer containers: `u8`/`u16`/`u32`/`u64`, `i8`/`i16`/`i32`/`i64`.
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

- Parameters: `name: Type`, may have default values. A trailing `...`
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
  - `float` args fill `xmm0..7`, then spill;
  - `al` is set to the number of xmm registers used, so C varargs
    (`printf`, …) work;
  - `: float` return type comes back in `xmm0`; any other return type
    (pointer / handle included) comes back in `rax` as a plain 64-bit int.
- **C callbacks.** A bare Komodo function name passed as an `extern` call
  argument is handed to C as a function pointer: the compiler emits a
  System V ABI *trampoline* that repackages the integer-register arguments
  into Komodo's call ABI and invokes the function. The target may take up
  to 6 plain-machine-word parameters (int / bool / char / string / handle)
  and return one or `void` — no `float`, no arrays. See
  `examples/ffi_callback.km`.
- `data(arr)` — the element pointer of a Komodo array, for passing a
  writable buffer / out-param to C (e.g. `SDL_PollEvent(data(ev))`). Read
  results back with ordinary `arr[i]`.
- Not supported: passing/returning structs by value; `float` in a callback
  signature; a callback with more than 6 parameters.
- With `build.sh` (the interpreter path) pass link flags after the output
  name — `./build.sh app.km x86 ./app -lSDL2`; the `komodo` CLI reads
  `<file>.links` and needs none.

## Built-ins

A handful of names the compiler lowers directly (no `import`, no library):

- `print(...)` — write values. A `float` argument is shown in `%g` form
  with a trailing `.0` kept when integral.
- `string(x)` — convert any value to its text form (same `float` rule).
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
- `link "path/to/file.c"` — a quoted string — is passed to the linker
  as-is (relative to wherever the compiler is invoked from), for a
  companion source file rather than an installed system library.

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

A struct / class instance carries an 8-byte **reference count** in the
word *before* the handle, so the type-id word and every field / vtable
offset are unchanged; `new` sets it to 1. The runtime exposes
`km_retain` / `km_release` (`stdlib/runtime.c`) — `km_release` frees the
whole block, header included, at count zero, without recursing into
fields.

Codegen then runs a **conservative escape analysis** (`Codegen::computeFreeable`
in `modules/codegen_stmt.kmm`) per function. A `let`-bound local of heap
type (struct / class handle, or array) is *freeable* when:

- every initialiser and `=` value for it is a **fresh** producer — `new`,
  an array literal, or `copy(...)`; and
- it never **escapes**: not returned, not stored into a field / element /
  global, not passed to a call, not aliased to another binding, not used
  as a method receiver, not handed to `data()`. Only pass-through reads
  (`v.f`, `v[i]`, `#v`, `v == null`) keep it freeable.

Such a value has refcount 1 for its whole life, so the compiler emits an
unconditional free — `km_release` for objects, `km_drop_array` for arrays
(arrays carry no count word) — at every `return`, at fall-through
function exit, and before each fresh reassignment (the slot is
zero-initialised so the first release is a harmless no-op). Everything
the analysis is unsure about, and the fields / elements *inside* a freed
object, are left to leak; cyclic structures always leak. The `stackvm`
backend treats the hooks as no-ops.
