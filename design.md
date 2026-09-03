# Komodo — Language Reference

A catalogue of every syntax and feature the language currently has. Reference
only, for keeping the implementation and sample code coherent.

## Files & modules

- `.km` — program (has a `main`). `.kmm` — module (library).
- `module <name>` — first line of a module file. All files declaring the same
  module name are auto-merged into one namespace.
- `import <name>` — pull in a module. The compiler resolves `<name>.kmm`
  from the directory tree of the file being compiled, transitively, and
  compiles every resolved source together as one unit. Modules with no
  `.kmm` source (`console`, `math`, `file`, …) are left to the linker.
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

- Parameters: `name: Type`, may have default values.
- Return type follows `:`. `return expr` / bare `return`.
- Mutation contract (compile-time checked): a pure `fn` may not contain
  mutating code, and a `mut` function/method must. "Mutating code" means a
  write the caller can observe — a field write (`obj.f = v`, or bare
  `field = v` against the receiver) or an array-element write
  (`arr[i] = v`). Reassigning the function's own locals does not count,
  and the check does not follow calls. `void` procedures and constructors
  are unconstrained.

## Structs

```
struct Token
    type: TokenType
    lexeme: string
    line: int = 0                    // default values allowed

abstract struct Expression : ASTNode        // abstract base, inheritance
abstract shared struct ASTNode             // `shared` modifier
struct Flags : u32                          // fixed-width int container
```

- Inheritance: `struct Child : Parent` — child fields follow parent fields.
- Instantiating an `abstract` type is an error.
- `new Name(a, b, …)` builds a heap value, binding args to fields in
  declaration order (unset fields are zero); `obj.field` reads/writes a
  field; a struct is passed and returned by handle.

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

Precedence low → high: `or`, `and`, comparison, bitwise, additive,
multiplicative, unary, postfix.

- Arithmetic: `+ - * / %`
- Comparison: `== != < <= > >=`
- Logical: `and`, `or` (short-circuit), `not`, `!`
- Bitwise: `& | ^ << >>`; keyword `xor` is an alias for `^`
- Conditional value: Lua-style `cond and whenTrue or whenFalse` (built from
  short-circuit `and`/`or`); there is no C-style `?:` operator
- Unary: `-x`, `not x` / `!x`, `#x` (length of array or string)
- Assignment: `x = v`, `x += v`, `x -= v`; also `obj.field = v`, `arr[i] = v`
- Type test: `expr is TypeName`
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

```
match text
    case "true"
        type = TokenType::TRUE
    case "if"
        type = TokenType::IF
    else
        type = TokenType::IDENT
```

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
extern fn SDL_CreateWindow(t: string, x: int, y: int, w: int, h: int, f: int): int
extern void SDL_Delay(ms: int)
extern void SDL_Quit()
```

- `extern fn` / `extern void` — a prototype only, no body. The symbol is
  resolved by the linker under its raw C name.
- Calls use the full native System V AMD64 C ABI (not Komodo's
  `(tag,value)[]` ABI):
  - integer / pointer / `string` args fill `rdi, rsi, rdx, rcx, r8, r9`,
    then spill to the stack — any number of arguments;
  - `float` args fill `xmm0..7`, then spill;
  - `al` is set to the number of xmm registers used, so C varargs
    (`printf`, …) work;
  - `: float` return type comes back in `xmm0`; any other return type
    (pointer included) comes back in `rax` as a plain 64-bit `int`.
- `data(arr)` — the element pointer of a Komodo array, for passing a
  writable buffer / out-param to C (e.g. `SDL_PollEvent(data(ev))`). Read
  results back with ordinary `arr[i]`.
- Not supported: passing/returning structs by value, and Komodo function
  pointers as C callbacks.
- Link the library by passing linker flags to `build.sh` after the output
  name, e.g. `./build.sh app.km x86 ./app -lSDL2`.

## Built-ins

- `print(...)` — write values.
- `string(x)` — convert to string.
- Standard library modules (imported): `console` (`print`, `write`, `error`),
  `algorithm` (`sort`, `reverse`, `sum`, `max`, `min`, `contains`,
  `index_of`, `fill`), `math` (`abs`, `min`, `max`, `pow`, `gcd`, `sqrt`),
  `file` (`exists`, `read_all`, `write_all`).
