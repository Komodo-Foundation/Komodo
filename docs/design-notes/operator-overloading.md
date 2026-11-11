# Operator overloading — design (proposal)

Status: **proposal, awaiting maintainer decision.** Nothing is implemented yet.

## Today

- Operators work on built-in types only. `a == b` on two struct values compares
  identity (prints `0` for two separate `new V(1)` objects).
- `a + b` on structs compiles and silently adds the object addresses
  (verified: prints a pointer-sized number). Another thread adds diagnostics for this.
- Free-function overloading exists (`docs/language/reference.md` §9.6).

## Recommended default: operators are overloaded functions named by the operator

Declare an operator as an ordinary free function whose name is the operator:

```km
struct Vec
    x: int
    y: int

fn +(a: Vec, b: Vec): Vec
    return new Vec(a.x + b.x, a.y + b.y)

fn -(v: Vec): Vec            // one parameter = unary minus
    return new Vec(0 - v.x, 0 - v.y)

fn ==(a: Vec, b: Vec): bool
    return a.x == b.x and a.y == b.y
```

Why this one:

- **One mechanism.** It reuses §9.6 overload resolution, so there is no new
  dispatch rule. Arity picks unary vs binary.
- **Resolved at compile time.** The call is type-checked like any call, using the
  existing "no overload matches" error. The lowering happens in sema, so all
  backends (x86, C, wasm, stackvm) get it without changes.
- **No implicit conversions.** Each operator takes exactly the declared types.
- **Fixed arity and precedence.** Operators keep their usual meaning and position.

Rules:

1. Overloadable: `+ - * / %` (binary and unary `-`), `< <= > >=`, `==`.
2. Not overloadable: `and or not in is`, `&&`/`||`-style forms, `,`, `=`,
   user-defined literals.
3. `!=` is derived as `not (a == b)`. There is no separate `!=` function.
4. Ordering is not derived. Each of `< <= > >=` must be declared, or the
   compiler reports a missing overload. (Alternative: derive all from `<`; see below.)
5. `+=`-style compound forms, if written, desugar to `a = a + b` and need no
   separate definition.
6. `==` on objects stays identity **unless** an `fn ==(T, T)` exists. `x == null`
   always stays a null check, so nullable narrowing is unchanged.
7. `#x` and indexing hooks are deferred. They can reuse the same mechanism later
   (`fn #(v: Vec): int`, `fn [](v: Vec, i: int): int`) if wanted.

Implementation sketch: the parser accepts an operator token after `fn`. Sema
rewrites a `BinaryOp`/unary node whose operand is a struct/class into a call when
an operator overload exists, otherwise it keeps today's behavior plus a diagnostic
for objects with no overload. Docs, `komodo fmt`, the TextMate grammar, `km-lsp`,
tests, and the bootstrap seed are updated in the same PR.

## Alternatives

- **A. Methods inside the struct:** `fn +(other: Vec): Vec` in the body of
  `struct Vec`, with `self` implicit. Reads nicely, but it is a second place
  (methods) where operators live, and unary/binary become two forms.
- **B. Named methods, no operator syntax:** `fn add(a: Vec, b: Vec)`, with no
  overloading of `+`. Simplest and fully explicit, but it is less intuitive, and
  it is the opposite of what was asked for.

## Decisions needed from Lukas

1. Free functions named by the operator (recommended) or methods inside structs?
2. Ordering: declare each of `< <= > >=`, or derive all four from `<`?
3. Should `+=` and friends be included in this change or left for later?
