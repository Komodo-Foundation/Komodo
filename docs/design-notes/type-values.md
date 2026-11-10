# Design note: first-class type values (`type`)

Status: proposal, no source changes. Branch `feat/type-values`.
Cites are `file:line` in this tree. Behaviour marked **[tested]** was run
with `./komodo -q` here. Builds on `docs/design-notes/typed-ir.md` (branch
`feat/typed-ir`, not yet merged here; its 13 steps are called **T1..T13**).

Maintainer constraints applied throughout: **additive only** (new node
kinds, fields, functions beside the existing ones; no rewrites; every
current program keeps compiling and the old id/vtable scheme stays);
typed-IR decisions (generic returns must agree; `u8`/`bool`/`char` are
distinct from `int` for overloads, instances and hashes; uncalled generics
are not checked until used; `Type` moves to `modules/support/`).

## 0. Recommendation (short)

1. `type` is a built-in type whose values are 63-bit name hashes. Reuse the
   existing `km_str_hash` (FNV-1a 64, stdlib/runtime.c:672-679) over the
   canonical name; mask keeps ids non-negative, 0 is "no type".
2. Keep the object header word and `__km_vtab[dense]` exactly as they are
   (backend_x86.kmm:661-667, 958-967, 1724-1740). The hash is a *second*
   id: constants folded at compile time, plus one opt-in table
   `__km_tydesc` (hash, name, dense id, parent hash) for runtime use.
3. Generics are monomorphised in sema (needs T7 clone lib, T9/T10).
   Type parameters are ordinary parameters of type `type`:
   `fn f(T: type, x: T)`, `class Box<T>`.
4. Application syntax `Box<int>` in *type positions*; `type(...)` wraps a
   type expression in value position. `[int]` stays an array literal.

## 1. Surface syntax and semantics

### 1.1 What exists today (checked)

- `type` is not a keyword: `let type = 3` works **[tested]**; the lexer's
  keyword list (lexer.kmm:379) has no `type`, and `extern type` is parsed
  as a contextual identifier (parser.kmm:333). Keep it contextual.
- Declared types are *strings* (`parseType`, parser.kmm:249-264): only
  `[T]`, `T?`, one identifier. So `x: type` already parses (an identifier
  named `type`), and so does `T: type`.
- `expr is Name` takes the next token's lexeme as an `Identifier`
  (parser_expr.kmm:47-55); sema demands `isObjType(rn)` (sema_expr.kmm:124-131);
  codegen expands it to `isTypeCheck(descendantIds(rn))` (codegen_expr.kmm:122-131).
  So `x is t` with `t` a variable parses unchanged; only sema/codegen reject it.
- `new Name(args)` takes one lexeme (+ `ns::` segments) as the type name
  (parser_expr.kmm:221-241); `new t(args)` likewise parses unchanged.
- A bare `int` in expression position is `undefined name 'int'` **[tested]**,
  so making builtin type names into values collides with nothing.
- `[int]` as an expression is today an error for the same reason; after this
  change it is an array literal holding the type value `int`, i.e. a `[type]`.
  That is the consistent reading, so *array type values need a wrapper*:
  `type([int])`.
- `<` / `>` are comparison tokens (lexer.kmm:630-647) and `>>` is one SHR
  token. `new Box<int>(1)` today **hangs the parser** **[tested]**
  (pre-existing bug; step S2 fixes it).
- Generic functions today: un-annotated parameters (types.md:541-575).

### 1.2 Grammar additions

```
type-param     :=  IDENT ':' 'type'                      (a Param whose declaredType is "type")
type-app       :=  IDENT '<' type ( ',' type )* '>'      (type positions only)
type-expr      :=  'type' '(' type ')'                   (value position)
class-header   :=  'class' IDENT [ '<' IDENT (',' IDENT)* '>' ] [ ':' parent ]
struct-header  :=  same
```

*Type positions* are exactly the places `parseType` is already called:
after `:` in params/fields/lets, after `->`/return `:`, after `new`, after
`is`, after `extern`, and inside `type( )`. In them `<` cannot be a
comparison, so `Box<int>` is unambiguous. In value position there is no
generic application: write `type(Box<int>)`. The closing `>>` of
`Map<string, Box<int>>` is split by the type parser (consume SHR, push back
a GT): parser-local, no lexer change. `Box(int)` and `Box[int]` are rejected
(call syntax / indexing / array confusion; see Q1).

In value position these name type values with no wrapper: any identifier
that resolves to a struct/class/enum/builtin type and is not shadowed by a
local/global (`int`, `string`, `bool`, `char`, `u8`..`i64`, `float`, `any`,
user types), plus `T` for a type parameter. Wrapper needed for anything
with punctuation: `type([int])`, `type(string?)`, `type(Box<int>)`,
`type(fn(int): int)`.

### 1.3 Type parameters on functions

<!-- doc-skip -->
```km
// T is a compile-time type parameter; x and the result use it.
fn first(T: type, xs: [T]): T
    return xs[0]

fn swap(A: type, B: type, p: Pair<A, B>): Pair<B, A>
    return new Pair<B, A>(p.b, p.a)

fn main()
    let xs = [3, 4]
    console.print(first(int, xs))     // explicit
    console.print(first(xs))          // inferred: T = int from [T]
```

Rules:
- A parameter `P: type` is a type parameter. It may be used as a type name
  in later parameter types, the return type, and the body (`let y: T`,
  `new T(...)` if T is an object type, `T == int`).
- **Inference**: a leading run of `type` parameters may be omitted at a call
  site if each occurs in some later parameter's type as `T`, `T?`, `[T]`,
  `Name<T>`; they are unified with the argument types (reusing sema's
  `unify`, sema_expr.kmm:244). Explicit and inferred are told apart by
  arity: `#args == #params` means explicit, `#args == #params - #typeparams`
  means inferred. Ambiguity (both counts equal) is impossible because
  `type` parameters cannot have defaults. A type parameter that cannot be
  inferred is an error telling the user to pass it.
- Default parameters, `module.fn(...)` rewriting and overloading follow
  typed-IR 3.5/3.6 (instances are ordinary functions after sema).
- **Return type**: all `return` expressions must agree (typed-IR decision
  1); otherwise error. Recursive generics must annotate the return type.
- **Distinct instances**: instance key is the tuple of canonical type names,
  so `first(u8, ...)`, `first(int, ...)`, `first(bool, ...)`, `first(char, ...)`
  are four instances (typed-IR decision 2). Bool results print `true/false`.
- **Static `if`**: inside an instance, `if T == int then ... else ...` where
  both sides are compile-time type constants is decided in sema *before*
  the dead branch is checked, so a body may legitimately specialise:

<!-- doc-skip -->
```km
fn show(T: type, x: T): string
    if T == string then
        return x
    else if T == int then
        return string(x)
    else
        return name(T)
```

- Uncalled generics are not checked (typed-IR decision 3); their headers
  are checked for syntax and for unknown names outside type parameters.

### 1.4 Generic classes and structs

<!-- doc-skip -->
```km
class Box<T>
    value: T
    constructor(v: T)
        value = v
    fn get(): T
        return value
    mut set(v: T)
        value = v

struct Pair<A, B>
    a: A
    b: B

fn main()
    let b = new Box<int>(5)           // explicit
    let b2 = new Box(7)               // inferred from the ctor arg: Box<int>
    let p = new Pair<string, int>("n", 1)
    let q: Pair<string, int> = p
    console.print(b.get() + b2.get())
```

A use `Box<int>` creates (once per canonical name) an *instance*: a clone of
the `ClassDecl` with `T` substituted in field types, method signatures and
bodies (needs T7 extended with `cloneClass/cloneStruct`; cheap, the member
node classes are already in typed-IR's clone list), registered as an
ordinary class named `Box<int>` with its own dense id, vtable and slot
list. Codegen's registries (`registerTypeFull`, codegen.kmm:400-430) see a
normal class; nothing in codegen learns about generics. Link names use a
label-safe form (`Box_int_` style via `symSafeType`, typed-IR 3.1).
Inheritance: `class Sub<T> : Base<T>` is allowed (parent is a type-app);
`extends` a type parameter is not. Variance: none, `Box<Dog>` is not a
`Box<Animal>` (invariant; Q4).

Inference for `new Box(7)`: only from constructor parameter types (same
rule as 1.3); for a declaration with no way to infer, write `Box<int>`.

### 1.5 Library `Map` and `Set` written with these features

<!-- doc-skip -->
```km
// stdlib/map.kmm
module map

struct Entry<K, V>
    key: K
    val: V

class Map<K, V>
    buckets: [[Entry<K, V>]] = []
    count: int = 0
    constructor()
        for i = 0 to 16 do
            buckets += [[]]

    fn size(): int
        return count

    fn bucketOf(k: K): int
        return (hash(k) & 0x7fffffffffffffff) % #buckets

    fn has(k: K): bool
        for e in buckets[bucketOf(k)] do
            if e.key == k then
                return true
        return false

    fn get(k: K, dflt: V): V
        for e in buckets[bucketOf(k)] do
            if e.key == k then
                return e.val
        return dflt

    mut put(k: K, v: V)
        let b = bucketOf(k)
        let i = 0
        while i < #buckets[b] do
            if buckets[b][i].key == k then
                buckets[b][i].val = v
                return
            i += 1
        buckets[b] += [new Entry<K, V>(k, v)]
        count += 1
        if count > #buckets * 2 then
            grow()

    mut grow()
        let old = buckets
        buckets = []
        for i = 0 to #old * 2 do
            buckets += [[]]
        count = 0
        for bucket in old do
            for e in bucket do
                put(e.key, e.val)

class Set<T>
    items: Map<T, bool> = new Map<T, bool>()
    mut add(x: T)
        items.put(x, true)
    fn has(x: T): bool
        return items.has(x)
    fn size(): int
        return items.size()

void main()
    let m = new Map<string, int>()
    m.put("a", 1)
    m.put("b", 2)
    console.print(m.get("a", 0))                     // 1
    let seen = new Set<type>()                      // types as keys
    seen.add(int)
    seen.add(type([int]))
    console.print(seen.has(int))                    // true
```

New primitives this needs (compiler-side, small): builtin `hash(x)`
accepting `int`, widths, `bool`, `char`, `string` (calls `km_str_hash`),
`type` (the id itself), enums; for classes it requires a user `hash()`
method (sema error otherwise); and `==` on `K` is by-value for those same
kinds and identity for objects unless the class defines `equals` (Q5).
Whether `hash(x)` and `==` on a type parameter are legal is checked per
instance, so `Map<Foo, int>` errors at `new Map<Foo, int>` with
"in Map<Foo,int>: `hash` needs a `hash()` method on Foo" (typed-IR 3.3
style context line).

### 1.6 Runtime use of type values

<!-- doc-skip -->
```km
fn describe(x: Shape): string
    let t = typeof(x)                 // dynamic: the object's real class
    if t == type(Circle) then         // or: t == Circle
        return "circle"
    return name(t)                    // "Square"

fn make(t: type): Shape?              // runtime factory
    if not (t is Shape) then          // `t is Base`: is class t a subtype of Base?
        return null
    return new t(1)                   // ctor arity checked at run time

fn main()
    let kinds: [type] = [Circle, Square, int]       // arrays of type
    for k in kinds do
        console.print(name(k) + " " + hex(k))       // e.g. "Circle 0x...."
    let reg: Registry = new Registry(Circle)        // a field of type `type`
```

- `typeof(e)`: static type is `type`. If `e`'s static type is a primitive,
  array or non-polymorphic struct it folds to a constant; for a class it is
  a header load (`__km_tyhash[dense]`, section 2.4). On `any` it is an error
  in the first version (values are untagged in locals; Q6).
- `x is t` (t a `type` variable): true iff `x`'s dynamic class equals `t`
  or descends from it. `t is Shape` (left operand of type `type`): subtype
  test between types. Existing `x is Shape` with a literal name stays and
  stays static-expanded (codegen_expr.kmm:122-131).
- `t == u`, `t != u`: integer compare, type-checked (both `type`).
- `name(t)`: string. Folded when `t` is constant, else descriptor lookup.
  Printing a `type` (`console.print(t)`, `string(t)`) yields `name(t)`.
- `hex(t)`: string `0x...`, 16 digits. There is no `hex` builtin today
  (grep of stdlib/ and sema found none), so it is added as a builtin that
  also accepts `int`.
- `new t(args)` with a non-constant `t`: result static type `any`; arity is
  checked against the descriptor at run time ("`Circle` takes 2 arguments,
  1 given"). With a constant `t` it is the ordinary `new` (folded).
- `type` values: comparable, hashable (`hash(t)`, usable as `Map` keys),
  storable in fields/arrays/globals, passable to `extern fn` as `int` (C
  sees `int64_t`), no arithmetic, `< >` rejected, no implicit conversion
  to/from `int`: `let n: int = int` is an error, `int(t)` is the explicit
  escape (`typeId`-style conversion) and `type(n)` is *not* provided; the
  inverse is `typeFromId(n: int): type?` which checks the descriptor table
  (unknown ids give null). Rationale: forging a type from any int would let
  `new t()` jump anywhere.

## 2. Representation

### 2.1 Today (checked)

- Object handle `[type_id | field0 ...]`; `objNew(typeId, n)` stores the
  *dense registry index* in word 0 (backend_x86.kmm:661-667; id =
  `typeIndex(name)`, a `StrIndex` position, codegen.kmm:838-843; structs and
  classes share the space).
- `vcall`: `mov rcx,[recv]; lea rdx,[__km_vtab]; mov rdx,[rdx+rcx*8]; mov
  rax,[rdx+slot*8]` (backend_x86.kmm:958-967). `__km_vtab` is a dense array of
  `__vt_<i>` or 0 (backend_x86.kmm:1724-1740), filled by `emitVtables`
  (codegen.kmm:656-672).
- `is`: `isTypeCheck(descendantIds(T))`, a linear `cmp` chain (x86 679-697;
  C `km_c_is` backend_c.kmm:439-440; stackvm `ISTYPE`, backend_stackvm.kmm:458-463).
- C and stackvm emit the same `objNew/objTypeId/isTypeCheck/vcall/setVtable`
  interface (backend.kmm:44-49), so all backends share the dense scheme.

### 2.2 Decision: keep dense ids in headers; add the hash as a public id

Putting 64-bit hashes in headers would force `__km_vtab` to be hash-keyed or
double-indirect on every virtual call (the hottest path), change every
backend's `objNew`/`vcall`/`isTypeCheck`, and break the stackvm bytecode
contract. The compatibility path instead keeps all of that byte-identical
and adds, only when a program uses a runtime type op:

```
__km_tydesc:   ; sorted by hash, 4 words per entry
  dq hash, name_ptr, dense_id_or_-1, parent_hash_or_0
__km_tyhash:   ; indexed by dense id -> hash
  dq h0, h1, ...
__km_tycount: dq N
```

Lookups: `dense -> hash` is one load. `hash -> descriptor` is a binary
search in `__km_tydesc` (runtime helper `km_type_find(hash)` in stdlib/
runtime.c, ~20 lines; N is small). Subtype test `is`: walk `parent_hash`
links (depth is tiny). A program that never mentions `typeof`, a `type`
variable, `name(t)` of a non-constant, etc. emits none of this
(sema flags `usesRuntimeTypes`, like `m_usesAlloc`, backend_x86.kmm:42/662),
so existing output is **byte-identical** and the fixpoint is untouched
until a later step opts the compiler itself in.

Descriptors cover every type the program can name at run time: all classes
and structs (dense id present), plus each builtin/array/nullable/fn type
that appears as a constant `type` value somewhere (dense id -1, no parent).

### 2.3 Which hash, and why it is enough

`km_str_hash`: FNV-1a 64 (offset basis `1469598103934665603`, prime
`1099511628211`), `& 0x7fff...ffff` (stdlib/runtime.c:672-679). It is
already linked into every compiled program and used by the compiler itself
(strindex.kmm:20), so the compiler can compute ids with zero new code.
Justification: ids are generated for at most a few thousand names per
program; for n names the 63-bit collision probability is about n^2/2^64
(for n = 10,000, ~5e-12); and, unlike a runtime hash table, a collision is
*detected* at compile time, not suffered. FNV is not adversarial-safe, but
names come from source the user already controls. A cryptographic hash would
add code for no gain. The constants are frozen forever (changing them
changes every published id) by a golden test. The golden test `tests/typeid/ids.expected` is generated by the tool in
step S1, not by this note, so the table is never hand-maintained. Example
values from an independent Python implementation: `int` 0x268b425134d641ac,
`u8` 0x1bf01d00c695238e, `bool` 0x26f69aafbc840517, `char`
0x297671a89c0da5ab, `string` 0x3850125edc81f186, `[int]` 0x0aa1b2c5ee3622bc,
`string?` 0x3200f030b0cdbd5b, `Box<int>` 0x57324c9f8ad95c15, `Box<u8>`
0x485ccc6c9b2fb529. `int`, `u8`, `bool`, `char` all differ (typed-IR
decision 2).

**Collision detection** (compile time, in the one place that mints ids):
`TypeIds.idOf(canon)` keeps a `StrIndex`-style map `hash -> canon`; if the
hash is already bound to a *different* canonical name, or equals 0, it
errors: `type id collision: 'A' and 'B' both hash to 0x...; rename one`.
Ids are minted for every declared type, every instance, and every builtin
type used as a value, so the check is complete for the program being
compiled. Separate compilation (modules/FFI) hashes the same names to the
same ids; a collision across independently built objects is the one
undetected case, documented as a limitation of serialising ids.

### 2.4 Canonical names

Canonical form = what `typeToString` prints plus qualification, with no
spaces: `int`, `u8`, `bool`, `char`, `float`, `string`, `string?`,
`[int]`, `[[u8]]?`, `Point`, `geom::Point` (module-owned declarations use
`moduleOwner::Name`; namespaces are flat after the merge,
parser_expr.kmm:226-227, so today only the bare name is unique, see Q2),
`Box<int>`, `Map<string,[Box<u8>]>`, `fn(int,int):int`, `Point?`.
Arguments are canonicalised recursively, so `Box<  int >` and `Box<int>`
hash identically. Nullable is part of the name (`string?` differs from
`string`; `T?` is also a type value).

`Type` (types.kmm:30-37) gains `typeCanon(t): string` and a cached
`id: int` field (additive), and a new kind `"type"` (the type of type
values; `typeToLegacy("type") = "int"` so backends need no change, the same
trick extern handles use, sema_expr.kmm:29-31).

## 3. Compile time vs runtime

| Situation | What happens |
|---|---|
| `int`, `Circle`, `type(Box<int>)`, a type parameter inside an instance | compile-time constant; `loadIntLit(id)`; operators fold (`T == int` is a bool constant, which static `if` consumes) |
| `typeof(e)` where `e` is a class instance | header load + `__km_tyhash` lookup; non-constant `type` value (the 64-bit id) |
| `typeof(e)` where `e` is non-polymorphic | constant |
| `t == u`, `hash(t)`, store/load, array of `type` | plain `int64` operations on the id |
| `name(t)`, `x is t`, `new t(...)`, `typeFromId` | descriptor-table helpers |
| generic function called with a non-constant type value | **compile error** |

The last row is the central decision. `first(t, xs)` with `t` a runtime
variable cannot be monomorphised and would need a dispatch table of *every*
instance in the program (unbounded, and typed-IR's per-instance checking
has no answer for it). Error text: ``cannot instantiate `first` with a
runtime type value `t`: type parameters must be known at compile time (use
`if t == int then` to select among known instances)``. A constant-folded
`if t == int then first(int, xs)` is still fine, because inside the taken
branch the value is a constant. A future opt-in `@dispatch(int, string)`
annotation (explicit instance list) is out of scope (Q7).

Interaction with typed-IR monomorphisation (T9/T10): the instance cache key
changes from "arg types" to "type-parameter bindings + arg types"; type
parameters are removed from the instance's parameter list (they are erased:
the instance `first<int>` takes only `xs`) and substituted as constants in
the cloned body. `callee.kind = "inst"` with `instArgs = [Type]` (already a
planned field, typed-IR 2.2). A `type` *value parameter* that is not a type
parameter is any other parameter named `t: type` that is **not** used in
later types; it is passed at run time as an `int64` (the id), is not
specialised on, and is ordinary. The compiler decides by usage: a `type`
parameter referenced in a later parameter type / return type / `new T` /
`T == ...` is a type parameter (must be constant at the call), otherwise a
value parameter. This keeps `fn name2(t: type): string` runtime-callable
while `fn first(T: type, xs: [T]): T` stays compile-time.

Nested instantiation depth cap and the instance diagnostics (`in call
first(int, [int]) ...`) are those of typed-IR 3.3; a generic class
instantiating itself with a growing type (`Box<Box<...>>`) hits the same
cap (64).

## 4. Type checking rules and messages

New `Type.kind == "type"` (printed `type`). Rules, with the new messages:

1. `type` is assignable only from `type`: `cannot assign 'int' to a type
   variable (use a type name or typeof(x))`; and `type` to `int` is rejected:
   `type 'type' is not an integer; use hex(t) or int(t)`.
2. Arithmetic/bitwise/ordering on `type`: `operator '+' is not defined for
   'type'` (reuse the existing binary-op error path, sema_expr.kmm:124-).
   `==`/`!=` require both operands `type`.
3. A name in value position resolving to a type has type `type`; a name
   that is both a type and a local resolves to the local (existing
   shadowing rules), so old programs keep their meaning.
4. `x is t`: `t` must be an in-scope `type` variable or a type name:
   `` `is` expects a type name or a `type` value on the right `` (replaces
   the message at sema_expr.kmm:127-129 only when the rhs is not a type).
5. `new t(args)`: `t` is a type name, a type parameter or a `type` value.
   Constant: normal checks. Variable: result `any`; `new t` on `type(int)`
   is a run-time error "`int` is not constructible".
6. Type-parameter errors:
   - `type parameter 'T' of 'first' cannot be inferred from the arguments; pass it explicitly`
   - `'T' of 'first' must be a compile-time type; got a runtime type value`
   - `generic type 'Box' takes 1 type argument, 2 given` /
     `... 'Box' needs type arguments: write Box<int>`
   - `'Box<int>' is not 'Box<Animal>': generic types are invariant`
   - `type parameter 'T' is declared twice` / `unknown type parameter`.
   - `return types of 'f' disagree: int and string` (typed-IR decision 1);
     `recursive generic 'f' must declare its return type`.
7. `type` as a parameter name keeps working (`let type = 3`); only the
   *annotation* `: type` is special, resolved after normal scope lookup, so
   a user class named `type` still shadows it with a lint warning.
8. `typeof(e)` on `any`: `typeof needs a statically typed operand`.

## 5. Backend impact

| Target | Needed |
|---|---|
| x86 | a `loadTypeId`-free design: constants use `loadIntLit` (`mov rax, imm64`, hashes need the full 64-bit form; `loadIntLit` already takes a string). New: `emitTypeTable(descs)` writing `__km_tydesc`/`__km_tyhash` (cf. `setVtable` text path, backend_x86.kmm:975-987) and `typeOfObj` = `mov rax,[rax]; lea rdx,[__km_tyhash]; mov rax,[rdx+rax*8]`. |
| c | same shape: `static const int64_t km_tyhash[]`, `km_tydesc[]`, `km_type_find` (shared with runtime.c; C backend already includes the runtime). `km_c_is` (backend_c.kmm:439) unchanged for static `is`. |
| wasm | the C output through emcc: same as c, no extra work (C `int64_t` ids are fine; JS never sees them). |
| stackvm | two new ops `TYHASH` (dense -> hash) and `TYFIND`, plus a `TYTAB` data directive; existing `OTID/ISTYPE/VCALL` untouched (backend_stackvm.kmm:443-470). Programs without runtime type ops emit none. |
| `Backend` interface | add `emitTypeTable`, `typeHashOf`, `typeFind`, `typeIs`, `typeName` with default no-op bodies (backend.kmm is a struct of methods; adding methods beside existing ones). |

FFI: a `type` is an `int64_t`; `extern fn` signatures may declare `type`
parameters/results, which `typeToLegacy` maps to `int`, so
`genExternCall` classification (`int` class) is unchanged and C sees plain
`int64_t`. Exported `export fn` likewise (c_header.kmm emits `int64_t`).
Ids are stable across builds and modules, so a C library may store them or
compare them against a constant; C cannot get a name from an id without
the program calling `name(t)` (a stdlib helper `km_type_name(int64_t)`
exported from the runtime when the table exists). `tests/bind` is
unaffected (`km-bind` maps C types to Komodo ones; no `type` is generated).

## 6. Cost (measured baseline vs `tools/perf-budget.sh`)

`tools/perf-budget.sh` lives in the sibling main checkout (not this
branch); its budgets and what this tree measures now: kmc 1,068,056 B (budget
1,400,000); compiler lines 13,164 (budget 15,500, so 2,336 lines of shared
headroom); seed 144,832 B (200,000); self-compile 0.50 s (0.80 s);
hello binary 73 KB (100 KB, unchanged: runtime tables are opt-in).

Estimated compiler growth for this whole plan, in lines: S1 +90, S2 +70,
S3 +120, S4 +110 (+60 in stdlib/runtime.c, not counted by the budget),
S5 +70, S6 +150, S7 +80, S8 +220, S9 +40, S10 +40, S11 +60 = **~+1,050**,
of which typed-IR (clone lib, nested checking) is *not* included, since it
is already budgeted there (+1,100 / -1,100 net). That leaves about 1,300
lines of the shared headroom. Mitigations to keep the budget:
`Map`/`Set` live in stdlib (not counted); `TypeIds` is one small file;
descriptor emission shares the vtable string-building helper.
Compile time: ids are hashed once per canonical name and cached in
`Type.id`; one `StrIndex` probe per type reference; no per-node work added
(the `ty` field is typed-IR T4). Expected self-compile impact below 3 %
(the compiler does not use generics, so no instance work happens). Each
step is gated by `tools/perf-budget.sh` and the fixpoint; S4 adds the first
steps that change emitted code and is measured with a before/after table in
its commit message.

## 7. Migration plan (12 steps)

Gate **G** after every step: `./build-self.sh` (`fixpoint OK`),
`./run-tests.sh` (49 examples x x86/stackvm/c, + wasm if available, +
`tests/interop`), `./run-sema-tests.sh`, `tests/bind`, `tests/debug`,
`python3 tools/check-docs.py`, `tools/perf-budget.sh`. Every step adds
examples with `.expected` (runnable) and/or `tests/sema/*.experr`.
Steps are additive; rollback is `git revert` of the one step.

| # | Step | Waits for | New tests |
|---|---|---|---|
| S1 | **Kill the id risk first.** Move `Type` to `modules/support/` (typed-IR decision 4, mechanical: just the file move + `module` line). Add `typeCanon`, `TypeIds` (hash via `km_str_hash`, collision map), a `kmc --dump-type-ids` debug mode listing every declared type's id. No language change. | none | `tests/typeid/ids.expected` golden; a forced-collision unit (feed two names through a test hook that overrides the hash) |
| S2 | **Parse only.** `parseType` accepts `Name<A,B>` (and splits `>>`); `ClassDecl`/`StructDecl`/`FunctionDecl` gain `typeParams: [string] = []`; new `TypeExpr` node for `type(...)`; fix the `new Box<int>(` parser hang. Sema reports "generic types are not supported yet" for any use. | none | `tests/sema/generic_parse*.experr`; example proving old `a < b > c` and `x is T` still parse |
| S3 | **`type` kind and folded constants.** `Type.kind "type"`, type names in value position, `==`/`!=`, `name(t)`, `string(t)`, `hex(...)`, `typeof(e)` for statics, `x is t` when `t` is constant; arrays/fields of `type` carry ids as ints. Folded only; no runtime tables yet (non-constant `type` values use `typeToLegacy "int"`). | T4 (`ty` stored on nodes) preferably; works without | `examples/type_values.km`; sema errors from section 4 rules 1-4 |
| S4 | **Runtime descriptor table**, opt-in: `usesRuntimeTypes`, `emitTypeTable` on the 4 backends, `km_type_find`, dynamic `typeof(obj)`, `name(t)`, `t == u` after round trips, `[type]` arrays printing names. Output unchanged for programs not using it (verified byte-identical on the 49 examples). | S1, S3 | `examples/type_runtime.km`; asm-identity check in G |
| S5 | **`x is t` (dynamic), `t is Base`, `new t(args)`.** Parent-hash walk; ctor arity check in a helper; result `any`. | S4 | `examples/type_factory.km` incl. a runtime error case with `.experr`-style expected output |
| S6 | **Type parameters on functions**, explicit call sites only: `T: type` params, erasure, constant substitution into the cloned body, instance keys from canonical names, returns must agree. Static `if` folding (`T == int`). Per-instance diagnostics. | T7 (clone), T8-T10 (sema mono; use the flag-guarded T9 path, not codegen's) | `tests/sema/tparam_*.experr`, `examples/generic_tparam.km` (u8/int/bool/char distinct) |
| S7 | **Inference of omitted type parameters**; the "runtime type value" error. | S6 | `tests/sema/tparam_infer*.experr`; `examples/generic_infer.km` |
| S8 | **Generic classes/structs**: header params, `Box<int>` in type positions, `new Box<int>(..)`/`new Box(..)`, `cloneClass`/`cloneStruct`, instance registration under the canonical name (registries see a normal class), generic parent. Uncalled generic classes unchecked. | S6, T7 extended | `examples/generic_box.km`, `generic_pair.km`; `tests/sema/gclass_*.experr` |
| S9 | **`hash(x)` builtin, `==` rules for type parameters, `stdlib/map.kmm` `Map`/`Set`**, docs (`docs/language/types.md`, ROADMAP collections item), doc snippets verified by `check-docs.py`. | S8 | `examples/map_set.km`, `map_type_keys.km` |
| S10 | **FFI**: `type` in `extern fn`/`export fn`, `typeFromId`, `km_type_name` export, `tests/interop` case round-tripping an id through C. | S4 | `tests/interop` addition |
| S11 | **Lint warnings** for features made redundant (section 8): implicit-type generics, type-based overloads, static-only `is` get a `lint` (not `err`, not default-visible beyond the existing lint level) with a one-line migration hint. | S6-S9 | `tests/sema/*.warn` cases; no existing example changes behaviour |
| S12 | **Optional later step, separate approval:** self-host the compiler's own registries on `Map` (the manual `StrIndex` code is replaced by `Map<string,int>`), proving the feature on 13k lines; and, if wanted, removal of redundant features (section 8) one by one. | S9 + maintainer sign-off | the fixpoint is the test |

Ordering rationale: S1 kills the two risks that cannot be fixed later
(hash choice, canonical names, collision handling) with no behaviour change;
S2-S5 deliver all runtime features *without* typed-IR, so they can merge
while typed-IR is in flight; S6-S9 (generics) wait for T7 and the flag-guarded
sema monomorphisation (T9) and are the highest-risk part, so they come
after the representation is proven.

## 8. Features made redundant

None is removed by this note. They keep working unchanged; S11 adds a
warning, and removal would be a separate approved step.

| Feature | Superseded by | What removal would break |
|---|---|---|
| Implicit-type generic functions (`fn id(x)`, types.md:541-575, sema.kmm:61-70, overload.kmm template path, `ensureInstance`) | `fn id(T: type, x: T)` with inference (call sites look the same: `id(3)`) | stdlib/algorithm.kmm (`sum`, `min`, `max`, `index_of`, `contains`), `examples/generic_demo`, `default_params`, `stdlib_demo`, `float_demo`, `random_demo` and every user template. Mechanical migration: annotate params with the inferred `T`. |
| Type-based overloading (`__ov` link names, runtime dispatch on type ids, overload.kmm:101-318) | generic functions with static `if T == ...`, or distinct names | `overload_demo`, `stdlib_demo`; user overload sets. Runtime dispatch on a *dynamic* type has no replacement except `if typeof(x) == ...` chains, so this one should be kept (arity-based and subclass dispatch). |
| Static-only `x is Name` (genIsCheck, codegen_expr.kmm:122-131) | `x is t` handles it (a literal name is just a constant `t`) | nothing, the syntax is a subset; only the dedicated codegen path (and the `isObjType` sema check) can be deleted. |
| `copy()`'s per-type synthesised helpers keyed by type name (codegen.kmm:440-520) | not superseded | keep. |
| Codegen's string type registries (`m_typeNames`, ...) | not touched; typed-IR T13 | keep. |

## 9. Open questions for the maintainer

1. **Application syntax.** `Box<int>` (recommended: type positions only,
   `type(Box<int>)` in value position, `>>` split in the type parser) vs
   `Box(int)` (reuses call/param syntax but `Box(int)` in an expression is a
   call, and `b: Box(int)` is a new type-position form) vs `Box[int]`
   (clashes with indexing and `[T]`). Confirm.
2. **Qualified names.** Registries are flat (parser_expr.kmm:226-227), so two
   modules cannot both declare `Point` today. Should ids hash `module::Point`
   from the start (future-proof, but then renaming a module changes ids), or
   the bare name until modules are namespaced?
3. **Generic-class constructor syntax**: inference `new Box(7)` from
   constructor args (proposed), or always require `new Box<int>(7)`?
4. **Variance**: invariant (proposed). Allow `Box<Dog>` to `Box<Animal>` for
   read-only classes later?
5. **Equality/hashing for object keys in `Map<K,V>`**: user `hash()` and
   `equals()` methods (proposed), or identity only?
6. **`typeof` on `any`**: error now (proposed); or give `any` values a tag
   at call boundaries (the `(tag,value)[]` frames already carry one,
   backend_x86.kmm:951-967)?
7. **Bounded type values**: `fn make(t: type of Shape)` so `new t(...)`
   returns `Shape` rather than `any`, and an explicit `@dispatch` instance
   list for runtime-type calls to generics.
8. **Header ids later?** If a future step wants the 64-bit hash in the
   header (one word, FFI-visible objects), it needs a hash-to-dense table
   on the `vcall` path; the dense plan here does not prevent it.
9. **`type` as a reserved word?** Proposed contextual (keeps `let type = 3`
   and `extern type` working); reserving it would break any program using
   `type` as an identifier.
10. **FNV offset basis**: `km_str_hash` uses `1469598103934665603`, not the
    standard `14695981039346656037` (it looks like a transposed digit).
    Harmless for hashing but ids are forever; confirm freezing the existing
    function rather than "fixing" it.

## Maintainer decisions (answers to the open questions)

1. **Generic type application:** `Box<int>` in type positions; value position uses `type(Box<int>)`. `[int]` stays an array.
2. **Id input:** hash the qualified `module::Name`, not the bare name.
3. **Inference:** `new Box(7)` infers `Box<int>` from the arguments; `new Box<int>(7)` stays available.
4. **Object header:** keep the existing dense-index header and vtable; the 64-bit hash is a second public id served from a table (zero cost when unused).
5. Standing constraints: additive only, within the perf budgets. Still open: questions 4 to 7 and 9 to 10 of the note (variance, Map key hashing, `typeof` on untyped values, bounded type values, contextual `type`, freezing the hash constants).
