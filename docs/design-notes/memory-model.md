# Memory model: plain, `shared`, `unique`

Status: proposal. Nothing here is implemented. Decisions are marked **Default** and
need the maintainer's pick.

## The model proposed

| Form | Meaning |
|------|---------|
| `shared struct` / `shared class` | Reference counted. Freed when the last shared reference is released. |
| `unique struct` / `unique class` | Single owner. Moves on bind, assign, store and return (already implemented for structs). |
| `struct` / `class` (plain) | Unmanaged. No automatic freeing. |

## What the compiler does today (verified)

- **Plain and `shared` are the same thing.** The parser reads `shared` and ignores it
  (`modules/frontend/parser.kmm`, around line 71). Both forms alias and both live on
  the Boehm GC heap (`stdlib/runtime.c`).
- **Nothing is refcounted.** `km_retain` and `km_release` exist in `stdlib/runtime.c`
  as no-ops, and no compiler module calls them. Memory is reclaimed by the collector
  only (`docs/language/reference.md` §15.1).
- **`unique` is the only ownership form with a checker.** Verified: `unique struct H`,
  `let b = a`, then `a.id` gives `error: use of moved value 'a'`. The runtime
  representation is unchanged.
- **Compiler sources:** 64 plain `struct`/`class` declarations and 1 `shared` declaration
  (`modules/frontend/ast.kmm`, `abstract shared struct ASTNode`). The compiler also
  relies on the GC, so the GC cannot be removed while plain types depend on it.

## Gap: Lukas's model vs today

| Behavior | Today | Lukas's model |
|----------|-------|---------------|
| Plain struct freed | by GC | never (unmanaged: the program must own the lifetime) |
| `shared` freed | by GC | at last release (refcount) |
| `shared` copy/assign | alias | alias, but increments the count |
| Scope exit | nothing | decrement shared refs |
| Cycles | GC collects | refcount leaks; needs `weak` or a documented rule |

Plain structs becoming unmanaged is the large change. Under the current runtime, a
program that drops a plain struct would leak it. The compiler's own code would leak
everything it allocates, so the self-compiler needs a migration first.

## Where `shared` must count references

1. **Binding and assignment**: `let b = a`, `b = a`. Increment the new value, decrement the old.
2. **Scope exit**: decrement every `shared` local when its scope ends, including early `return`, `break`, and error paths.
3. **Fields and elements**: storing into a field or array element increments; overwriting decrements; freeing a container decrements its `shared` children.
4. **Function arguments and returns**: decide borrow (no change) or own (increment) per parameter.
5. **`copy(x)`**: deep copy creates new counts for nested `shared` fields.
6. **Cycles**: refcounting leaks cycles. Options: a `weak` reference type, or a rule that `shared` graphs must be acyclic.
7. **Backends**: x86, C, wasm and stackvm each emit the retain/release calls. The runtime has to stop being a no-op on all four.

## Phased plan (recommended)

**Phase 1 (default): make `shared` refcounted, leave plain alone.**
- Add a refcount header for `shared` objects, implement `km_retain`/`km_release` for real, and emit them on the cases above.
- Risk: the cost is high in four backends and the checker. Start with heap objects, C backend only, behind a flag, and keep the other backends on the GC until it is proven.
- Gate: `./run-tests.sh`, `./build-self.sh` fixpoint, and `tools/perf-budget.sh` pass unchanged. The compiler's own code stays plain, so self-hosting is unaffected.

**Phase 2: explicit unmanaged plain types.**
- Require the compiler sources to say `shared` wherever they need freeing, or use an opt-in such as a `--gc` flag so the old GC behavior stays available.
- Plain types become unmanaged. Programs that rely on GC must opt in.

**Phase 3: remove the GC-for-plain behavior.** Only after a deprecation period and Lukas's approval.

## Forks for Lukas

1. **Cycles in `shared`**: a `weak` keyword, or ban cycles?
   **Default: add `weak`** (smallest model that handles trees with back-pointers).
2. **Keep the GC as an opt-in fallback after phase 2?**
   **Default: yes**, so existing programs keep building during migration.
3. **Phase 1 backends**: C only first, or all four?
   **Default: C only**, the other backends keep the GC until phase 1 is proven.
4. **Do not implement phase 1 yet.** It touches the checker, four backends and the runtime.
   **Default: wait for the decision above**; this doc is the only change in this PR.

## Related

- Item 8 of the one-way audit: `shared struct` vs plain `struct` spelling.
- `docs/language/reference.md` §10.2 and §15 describe the current behavior and must change with phase 1.
