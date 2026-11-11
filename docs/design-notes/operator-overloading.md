# Operator overloading — design (proposal)

Status: **syntax confirmed by the maintainer; implementation in progress.**

## Today

- Operators work on built-in types only. `a == b` on two struct values compares
  identity (prints `0` for two separate `new V(1)` objects).
- `a + b` on structs compiles and silently adds the object addresses
  (verified: prints a pointer-sized number). Another thread adds diagnostics for this.
- Free-function overloading exists (`docs/language/reference.md` §9.6).

## Proposed: operators are methods declared inside the struct

```text
struct Vec
    x: int
    y: int

    fn(operator +)(other)
        return new Vec(self.x + other.x, self.y + other.y)

    fn(operator -)()         // no argument = unary minus
        return new Vec(0 - self.x, 0 - self.y)

    fn(operator ==)(other)
        return self.x == other.x and self.y == other.y
```

Rules:

1. Overloadable: `+ - * / %`, unary `-`, `< <= > >=`, `==`. The left operand
   is `self`, so the form is `fn(operator <op>)(other)`, with no argument for unary `-`.
2. Not overloadable: `and or not in is`, `&&`/`||`, `,`, `=`, user-defined literals.
3. `!=` is derived as `not (a == b)`.
4. Ordering is not derived: each of `< <= > >=` is declared when needed.
5. `==` on objects stays identity unless the struct declares `fn ==`. `x == null`
   always stays a null check.
6. Operators keep their usual arity and precedence. The result type is checked
   at compile time against the declared return type.
7. `#x` and indexing hooks are deferred; they can use the same form later.

Implementation sketch: the parser accepts an operator token as a method name.
Sema records operator methods per struct, and when a binary or unary operator has
a struct operand with a matching method, it type-checks the call against that
method. Codegen emits an ordinary method call, so all backends (x86, C, wasm,
stackvm) work without changes. Docs, `komodo fmt`, the TextMate grammar,
`km-lsp`, tests and the bootstrap seed are updated in the same PR.

## Alternative: free functions named by the operator

`fn +(a: Vec, b: Vec): Vec` at top level, reusing §9.6 overload resolution. Less
per-type grouping, and both operands are written out, but there is no `self`.

## Alternative: named methods, no operator syntax

`fn add(other: Vec): Vec` and no overloading of `+`. Simplest, but not what was asked for.

## Decisions needed from Lukas

1. Confirm the syntax `fn(operator +)(other)` inside the struct, and what `ex:` means.
2. Ordering: declare each of `< <= > >=`, or derive all four from `<`?
3. Include `+=`-style compound forms now (desugared to `a = a + b`) or later?
