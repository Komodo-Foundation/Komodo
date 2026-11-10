# Design note: a typed IR for the self-hosted compiler

Status: proposal, no source changes. Branch `feat/typed-ir`.
Every code claim below was checked against the tree at the commit this note
sits on (cites are `file:line`). Behaviour claims marked **[tested]** were
reproduced with `./komodo -q` on snippets; the counts in section 1.4 come from
an instrumented *scratch copy* of the compiler (the instrumentation is not in
the tree; step 1 of the plan turns it into a tool).

Read first: `docs/language/basics.md`, `docs/language/types.md`
("Known gaps" lists exactly one item: template bodies, types.md:1004-1011).

## 0. Summary

Sema already computes a rich `Type` (analyze/types.kmm:30-37) for every
expression it visits, then throws most of it away: it stamps a coarse string
(`typeToLegacy`, types.kmm:129-161) and codegen re-derives the rest. The
largest holes are generics (never checked by sema, monomorphised in codegen)
and overloads (sema returns `any`, codegen resolves from coarse strings).

Recommendation: **option A**, annotate the existing AST and make it the IR;
move overload resolution and monomorphisation into sema; leave codegen with
nothing to infer. Details in sections 2-4.

## 1. Current state

### 1.1 Pipeline

`compileMapped` (km_compiler.kmm:63-98): `Lexer -> Parser -> Analyzer.analyze
-> (header exit) -> Optimizer.run -> Codegen.compileProgram`. One `Program`
(ast.kmm:362-) object flows through, mutated in place by each stage. There
is no stage boundary type: "analysed" is a bool (`Program.analyzed`).

### 1.2 How a call flows today

**Plain call `f(x)` / `ns::f(x)`**

1. Parser: `Identifier` followed by `(` becomes `CallExpr(name, args)`, with
   `qualifier` copied from the identifier (parser_expr.kmm:162-173; the
   qualifier is only kept for a privacy diagnostic, ast.kmm:62-64).
2. Sema `inferCall` (sema_expr.kmm:275-451). Branch order: non-name receiver;
   builtins (`assert`, `copy`, `join`, `string`, `int`...); **template or
   overload name -> infer args, return `tAny`** (356-367); dotted name; plain
   name -> `resolveFnCall` (sema_expr.kmm:496) which type-checks args
   against *strings* from `m_fnParamTypes` (re-parsed with `parseTypeStr` per
   call, sema_expr.kmm:~545) and sets `resolvedSym`. `inferExpr` then stamps
   `e.resolvedType = typeToLegacy(t)` (sema_expr.kmm:23-35).
3. Optimizer (optimize.kmm): rewrites the tree in place; only fresh literals
   and one `<<` node are re-stamped (optimize.kmm:441, 653-684). Reads
   `resolvedType == "int"/"float"` only (optimize.kmm:447,452). Runs over
   *template bodies* too (they are still in `prog.functions`; codegen removes
   them in "Pass 1-pre", codegen.kmm:152-163).
4. Codegen `genCall` (codegen_expr.kmm:407-~640) re-dispatches on the *name*:
   builtins; receiver; `recv.m` via its own `isMethodReceiver`
   (codegen_expr.kmm:877); sibling method (510); dotted module call through
   `m_backend.mangle` (518-530); template -> `fillDefaults` + `ensureInstance`
   (532-543); overload -> `selectOverload` (545-556); defaults (558-561);
   extern (`genExternCall`); else `callFrameBegin/ArgTag/ArgStore/End` with
   `exprType(a)` per argument.
5. Backend: accumulator/stack machine (backend.kmm); `callFrameEnd(name, n)`
   gets a *name* and mangles it itself (backend.kmm:150 `mangle`).

**Method call `a.m(x)`**

1. Parser: a plain-identifier receiver yields `CallExpr("a.m", args)`; any
   other receiver yields `CallExpr(".m", args)` with `.receiver` set
   (parser_expr.kmm:115-134). `MethodCallExpr` and `ScopeExpr` exist in
   ast.kmm:89-100 but are never constructed (only mentioned at
   sema_expr.kmm:59).
2. Sema: `inferMethodCall` (sema_expr.kmm:575-594) or the receiver branch
   (277-297) resolves the owner (`methodOwner`), checks args, writes
   `isMethod = true` and `resolvedSym = "Owner__m"`.
3. Codegen **ignores both fields**: `grep resolvedSym|isMethod` finds no read
   outside sema. It re-resolves with its own `resolveMethodOwner`
   (codegen.kmm:676; calls at codegen_frame.kmm:114,125, codegen_expr.kmm:513,
   908) and additionally needs the receiver's *static type* for the vtable slot
   (`methodSlotOf(objType, meth)`, codegen_expr.kmm:~915). The comment on
   `resolvedSym` (ast.kmm:78-81, "so codegen does not re-run method-owner
   resolution") is stale. **[measured]** over 49 examples + the compiler
   itself, 3530 method-call sites: codegen's re-resolution equals sema's
   `resolvedSym` in 3530/3530 cases where sema visited the node.

### 1.3 Where type information is lost or recomputed

| # | Where | What happens |
|---|---|---|
| L1 | types.kmm:129-161 `typeToLegacy` | nullable->inner, `bool`->"int", every int width->"int", `char`->"string", `any/void/null/never/fn`->"int", union->agreed-or-"int". Extern handles forced to "int" (sema_expr.kmm:31-34). |
| L2 | sema_expr.kmm:24-28 | an `any` result is *not stamped* (left null) so codegen re-infers. |
| L3 | sema_expr.kmm:356-367 | every template and overload call is typed `any`. Any binding of its result is `any` downstream: **[tested]** `let r = id(3); let s: string = r` compiles and crashes at run time. |
| L4 | sema.kmm:117-122 | template bodies are never checked or stamped. **[tested]** `fn bad(a,b): let q: string = a + b` accepted, `bad(1,2)` prints `3`. A call to an undefined function inside a body fails at *link time* (`undefined reference to nosuch`). A field access error inside a body is reported as `error: 0: field access '.zzz' ...` (no position, no instance). |
| L5 | codegen_frame.kmm:150-160 | local slot types recomputed from `declaredType` / `exprType(init)`. `VarDecl.resolvedType` (ast.kmm:152, set at sema_stmt.kmm:348) is read only for *globals* (codegen.kmm:75, 828-829). Its value for an `any` init is `"int"` (L1), so it could not simply be trusted. |
| L6 | codegen.kmm:340 `typeOrInt`, `m_fnRets`, `m_typeFieldTypes` | codegen builds its own string registries from raw `declaredType` strings (Pass 0/1, codegen.kmm:94-215). |
| L7 | overload.kmm:291-298 `tplArgTypes` | instance key is one of `float/string/bool/<object>/int`. **[tested]** `first(xs)` instantiated with `[int]` and `[float]` shares `first__g1_int`; `first([1.5,2.5])` prints garbage (`4.60943e+18`). `char` shares `string`, `bool` shares `int` (`show(true)` prints `1`). Real defect, not just inelegance. |
| L8 | codegen_frame.kmm:130 | `exprType` **mutates the AST**: `fillDefaults` appends the shared default-value node to `e.args`. |
| L9 | codegen_frame.kmm:132 | `exprType` **instantiates and queues code**: it calls `ensureInstance`. A "type query" has emission side effects. |
| L10 | overload.kmm:301-318 `instDisplay` | finds the first `"__g"` anywhere in a function name, so a method symbol such as `Codegen__genExpr` is treated as an instance (`m_curInst` non-empty). Latent: the one per-instance diagnostic (codegen_expr.kmm:176-183) is only reachable with unchecked bodies, so it has not misfired. |
| L11 | sema.kmm:1064-1093 vs codegen.kmm:975-1000 | two independent registries (types, fields, parents, fn signatures, macros, globals). **19** names are defined in both `Analyzer` and `Codegen`: `ctorFieldOrder declName dotIndex hasFn isAbstractType isCompareOp isCurField isDescendantOf isGlobalVar isMacro isMethodReceiver isObjType macroExprOf methodSym parentOrEmpty pushMacro receiverType typeIndex typeParentOf` (the maintainer's "18" excludes `pushMacro`, defined at sema.kmm:636 and codegen.kmm:802). Some differ in meaning (`receiverType`: sema.kmm:603 reads the declared `Type`, codegen_expr.kmm:887 reads the frame's coarse string). |
| L12 | sema vs codegen, macros | a `let x be e` use is re-lowered at each use from one shared `e`; sema re-stamps that shared tree on every use (`inferIdentifier`, sema_expr.kmm:~98-104), so the last use's stamps win. Not observed to misfire; flagged as latent, **not tested**. |

### 1.4 Every use of `exprType` and which can fire after sema

Size: `exprType` is codegen_frame.kmm:63-137. Call sites (non-comment):
codegen_expr.kmm 40, codegen_stmt.kmm 15, overload.kmm 4,
codegen_frame.kmm 12 (10 are the function's own recursion, 2 are
`collectLocals` at 158/172) = **71 lexical sites, 61 external callers**.
Related coarse-string readers (`typeOf`, `fieldTypeOf`, `elemTypeOf`,
`isObjType`, `globalTypeOf`) add 44 more outside codegen_frame.kmm.

First statement of `exprType` is `if e.resolvedType != null return it`
(66-68). Everything after that is the *fall-through*, which runs only for a
node whose `resolvedType` is null. Method: scratch copy of the tree with a
print in the fall-through (kind, whether we are in a template instance, which
arm), built with `./build-self.sh --fast`, then `kmc` over all 49 examples
and over `main.km` itself (x86 target; the walk is target-independent).

Fall-through hits by *source of the unstamped node* (49 examples: 342 hits
in 11 programs; compiler self-compile: 256 hits):

| Source of the null stamp | Evidence | Arms hit |
|---|---|---|
| (a) template-instance bodies (never sema'd) | `generic_demo`, `default_params` | Identifier-local (`typeOf`), Identifier-env, BinaryOp |
| (b) result of a template/overload call (`any`, L2/L3) | `overload_demo`, `stdlib_demo`, `float_demo`, `random_demo` (`algorithm.sum/min/max/index_of/contains`) | CallExpr template/overload arms; then Identifier-local of the binding (146 hits: the type rides in the frame, not sema) |
| (c) **struct / class field default values** (sema never infers them) | **[tested]** `xs: [int] = []`, `f: float = 1.5` in a struct hit the fall-through; 241 of the 256 self-compile hits are `[]` defaults | `ArrayExpr`/literals -> fall to `return "int"` |
| (d) nodes synthesised by codegen | `deep_copy*`, `ffi_struct`, `ffi_sret` (copy helpers, codegen.kmm:477-514), `assert_demo` (codegen_expr.kmm:414-421) | Identifier, MemberExpr, NullLiteral (kind "other"), StringLiteral |
| (e) optimizer-made nodes | all re-stamped (optimize.kmm:441,653-684) | none |

Arms of the fall-through, by whether the corpus ever fires them:

| Arm (codegen_frame.kmm) | Fires in corpus | After sema-only trees | Notes |
|---|---|---|---|
| FloatLiteral / StringLiteral literal (69-72) | yes (c, d) | no, once (c)(d) are stamped | |
| Identifier: env (74-77) | yes (a, template return inference) | only for templates | goes away with sema monomorphisation |
| Identifier: current-class field (78-79) | **never** | **dead**: templates have `ownerClass == null` (overload.kmm:257), so no `self` field in an instance | field defaults (c) do not read fields |
| Identifier: macro (80-81), global (82-83) | **never** | live only inside template bodies | |
| Identifier: local `typeOf` (83) | yes, 207 hits | live: (a) and (b) | the main consumer |
| NewExpr (84-85), SliceExpr (88-90), UnaryOp (91-94) | **never** | live only inside template bodies | |
| MemberExpr (86-87) | yes (d: copy helpers) | live only for (d) | stamp (d) and it is dead |
| BinaryOp (95-111) | yes (a) | live only inside template bodies | |
| CallExpr receiver (113-117), dotted-method (118-129), plain `fnRet(e.name)` (136) | **never** | live only inside template bodies | |
| CallExpr template (130-134) / overload (135) | yes (b) | live until sema resolves templates and overloads | |
| `return "int"` default (137) | yes (c: `[]`) | dead after (c) | |

Reading: **after sema, the only live fall-through is (a) template bodies and
(b) template/overload results** (plus (c)/(d), which are two small stamping
omissions). "Remove the dead fallbacks first" therefore deletes very little
today: the Identifier-field arm and the default arm, plus the literal arms
once (c)/(d) are stamped. Every other arm is reachable from a template body
and cannot be deleted until template bodies are sema-checked (steps 9-10).
The corpus does not cover method calls, macros, globals, slices, unary or
`new` inside a template body, which is why those arms show zero; step 1 adds
an example that does, so deletion claims rest on evidence.

## 2. Proposal

### 2.1 Choice: (A) annotate the AST, not (B) a separate lowered IR

| | (A) annotated AST | (B) separate lowered IR |
|---|---|---|
| New node classes | 0 (about 12 new fields on existing nodes) | about 30 (every Expression/Statement kind, ast.kmm has 21 + 14) plus the lowering |
| Code touched | sema ~+450, codegen ~-900, optimizer ~30 | lowering pass ~+1000-1300, IR classes ~+300, codegen walks rewritten (codegen_expr 936 + stmt 554 + frame 190 + overload 431) and optimizer rewritten (740) ~ 3,000 lines churn |
| Concurrent edits | codegen emission points keep their shape: `genCall` etc. change an input, not their structure | every emission point is rewritten while other agents edit them |
| Self-hosting risk | each step keeps codegen walking the same classes; fixpoint diffs are small | a second tree shape must be fixpoint-stable from day one |
| What it fixes | L1-L9, L11 (all fall from "sema decides, codegen reads") | same, plus a place to desugar |
| What it does not give | no desugared form | desugaring is the only extra benefit; most of it is available in (A) by sema rewriting nodes in place (`e.name`, default args, macro expansion by clone) |

Recommend **(A)**. The only thing (B) buys that (A) cannot is a normal form
with fewer node kinds (explicit calls/loads/stores), which pays off when
there is a CFG-based optimiser or a second backend family. Neither exists
(the optimiser is a tree rewriter, optimize.kmm:1-30; backends are an
accumulator machine). Revisit (B) when that need is real; (A) does not
preclude it, since the typed AST is exactly the input a lowering pass wants.

### 2.2 What gets added (all on existing classes, ast.kmm)

`Type` stays as defined in types.kmm; the AST references it directly (forward
references between structs are fine, **[tested]**: a `Node` with a `Ty?` field
declared before `Ty`, 200k nodes sharing `Type` objects compile and run).

| Node | New field | Meaning | Replaces |
|---|---|---|---|
| `Expression` (ast.kmm:27-28) | `ty: Type? = null` | full static type; never `any` after sema except for a genuinely unresolved extern symbol | `resolvedType: string?` (kept during migration, derived from `ty`) |
| `VarDecl` | `ty: Type? = null` | bound type (`widenForBinding`) | `resolvedType` (ast.kmm:152) |
| `Identifier` | `ref: string = ""` | `"local" "param" "field" "global" "macro" "enum" "fn"` as sema resolved it (order in sema: scope, field, macro, global; sema_expr.kmm:~98-125) | codegen's `hasLocal`/`isCurField`/`isMacro`/`isGlobalVar` ladder (codegen_frame.kmm:74-83) |
| `CallExpr` | `callee: Callee? = null` | resolved target, see below | `resolvedSym`, `isMethod`, name re-dispatch in `genCall` |
| `Callee` (new struct, ast.kmm) | `kind: string`, `sym: string`, `decl: FunctionDecl?`, `ownerType: string`, `recvTy: Type?`, `cands: [FunctionDecl]` | `kind` in `"fn" "method" "extern" "builtin" "inst" "overload" "dispatch"`; `sym` is the final *link symbol* (post `module.fn` rewrite, post mangle, instance or overload link name); `ownerType`+`recvTy` give codegen the static receiver type for vtable slots; `cands` is the specificity-ordered runtime-dispatch list | |
| `FunctionDecl` (ast.kmm:266-300) | `paramTys: [Type]`, `retTy: Type?`, `instOf: FunctionDecl?`, `instArgs: [Type]` | resolved signature; instance provenance | `typeOrInt`, `m_fnRets`, `instDisplay` string-sniffing |
| `Program` | `instances: [FunctionDecl]` (appended into `functions` too) | monomorphised bodies, already checked and typed | codegen's `m_genQueue`/`m_genDone` |

Not added: per-node symbol tables, a CFG, or typed copies of declarations.
Struct/class/enum declarations keep string field types; the shared registry
(step 13) exposes them as `Type` through `parseTypeStr`.

### 2.3 Who builds it, who consumes it

- **Builder: `Analyzer` only.** `inferExpr` already computes `Type` per node
  (sema_expr.kmm:14-36); it just stores `e.ty = t` instead of collapsing. The
  two new responsibilities are overload resolution (section 3.5) and
  monomorphisation (section 3).
- **Optimizer**: a consumer and a producer. It reads `ty.kind` instead of
  `resolvedType == "int"` (this separates bool/char/widths, which
  `typeToLegacy` conflates), and every node it creates stamps `ty` through the
  existing `mkInt`/`mkStr`/`cloneLit` helpers (optimize.kmm:653-690). Because
  instances now exist before it runs (section 3.4), it sees typed instance
  bodies instead of one shared untyped generic body.
- **Codegen**: a pure consumer. `exprType(e)` becomes `legacyOf(e.ty)` and
  then disappears at the leaves; backends keep taking coarse strings at the
  `Backend` boundary (`loadLocal(off, ty)`, `callArgTag`), since that is a
  deliberate ABI-class vocabulary (backend.kmm:6-60). `typeToLegacy` moves to
  that boundary and stays there.
- **Invariant** (checked by a debug verifier, step 12): after sema, every
  reachable `Expression` has non-null `ty`; every `CallExpr` has non-null
  `callee`; no `FunctionDecl` with an untyped parameter is reachable from
  `prog.functions` other than as a template marker.

## 3. Monomorphisation

### 3.1 Where instantiation happens

In `Analyzer.inferCall`, at the template branch that today returns `tAny`
(sema_expr.kmm:356-367). Sequence for a call `f(a1..an)`:

1. `checkTemplateArity` (existing, sema_expr.kmm:~520) then infer the
   arguments; append defaults (3.6).
2. Key: `f` + the `typeToString` of each argument type after
   `widenForBinding` (so `[int]` and `[float]`, `bool`, `char`, `int?` are
   all distinct, fixing L7). An `any` argument keeps the parameter `any`
   (today's loose behaviour).
3. If an instance exists, return its `retTy`. Otherwise create it (3.2),
   check it (3.3), register it, append it to `prog.functions` and
   `prog.instances`, return `retTy`.
4. Stamp the call: `callee = {kind:"inst", sym:<instance link name>, decl}`.

The instance symbol keeps the shape `name__g<argc>_<t1>_<t2>` (overload.kmm:320)
with each `ti` label-safe-ified like `symSafeType` (`[float]` -> `arr_float`);
`isTemplateDecl` (overload.kmm:256) is replicated in sema as `fnIsTemplate`
(already exists, sema.kmm:1031, used at sema.kmm:69).

### 3.2 Cloning the generic body

Instances currently share one body (`inst.body = tpl.body`, overload.kmm:~410)
and stamps/rewrites are per node, so each instance needs its own tree. `copy()`
cannot do it: it is a *static-type* deep copy that synthesises
`km_deepcopy_<T>` per concrete non-abstract type and shares anything of
abstract type (codegen.kmm:440-520, `deepCopierExists`), so it does not clone
a `[Statement]` of mixed concrete subclasses. A hand-written clone with one
`is` arm per node class is needed. The pattern works with the AST's
conventions **[tested]** on a mini AST (abstract base, subclasses with
defaulted fields, `Expr?` children, list children):

```km
// must be `mut`: a pure `fn` may not mutate even a fresh local object
// ("pure `fn` ... mutates state" — tested)
mut cloneExpr(e: Expression?): Expression?
    if e == null then
        return null
    let out: Expression? = null
    if e is IntLiteral then
        out = new IntLiteral(e.value)
    else if e is BinaryOp then
        let b = new BinaryOp(e.op)            // required fields first (ctor order)
        b.left = cloneExpr(e.left)
        b.right = cloneExpr(e.right)
        out = b
    else if e is CallExpr then
        let c = new CallExpr(e.name)
        for a in e.args do
            c.args += [cloneExpr(a) or throw "clone"]
        c.qualifier = e.qualifier
        c.receiver = cloneExpr(e.receiver)
        c.protected = e.protected
        c.site = e.site
        out = c
    // ... one arm per class ...
    if out != null then
        out.line = e.line
        out.column = e.column
    return out                                 // note: ty / resolvedType stay null
```

Scope: 21 expression classes (incl. the 6 literals and `StructPattern`), 14
statement classes, plus the plain structs `MatchCase`, `FieldPattern`, `Param`
(ast.kmm:213, 227, 260 are not `ASTNode`s). About 220 lines in a new
`frontend/astclone.kmm`. `cloneFunction(tpl, paramTypes)` copies the decl
header, replaces each untyped `Param.declaredType` with
`typeToString(argTy)`, clones default values (literals or a negated literal, sema_stmt.kmm:117
`isLiteralDefault`) and the body, copies `moduleOwner`/`visibility`/
`line`/`column` (so diagnostics point at the generic source line), and sets
`instOf = tpl`.

Fresh nodes carry no stamps, which is what makes sema's in-place rewrites
(`e.name = meth`, defaults, macro expansion) safe: they hit the clone, never
the template. This is the precise reason types.md:1010 says "needs a clone".

Verification of the clone *before* anything depends on it: a build mode that
replaces every function body by its clone before sema must produce
byte-identical assembly on all examples (step 7).

### 3.3 Checking an instance

`checkFunction(fd, owner)` (sema_stmt.kmm:12-62) is the existing entry. It
already treats a parameter without annotation as `any` and warns
(sema_stmt.kmm:36-42), so the machinery is there; the instance simply has
annotations. Two things need adding:

- **Nested function context.** `checkFunction` calls `scopeReset()` and
  overwrites ~25 per-function fields (`m_cur*`, scope arrays, path
  narrowings, macro stack, move state). Instantiation happens in the middle of
  checking the caller, so these must be saved and restored around the nested
  check: `pushFnContext()` / `popFnContext()`, about 60 lines. (An alternative
  is a second `Analyzer` sharing the registries; arrays are values in
  Komodo, so sharing them cheaply is not obvious. Open question Q5.)
- **Return type inference.** A template has `returnType == null` and returns a
  value; `checkFunction` would call it void (`m_curVoid`, sema_stmt.kmm:23).
  In instance mode `checkReturn` (sema_stmt.kmm:483-494) records each returned
  type into `m_retSeen` instead of asserting assignability, and the instance's
  `retTy` is the `unify` (sema_expr.kmm:244) of all of them (void if none).
  Today codegen uses the *first* `return` only (`firstReturnType`,
  overload.kmm:336); unify is stricter. Open question Q2.
  A declared return annotation on the template wins and is checked normally.
- **Recursion.** While an instance is `inProgress` a recursive call returns the
  annotation if there is one, else the unify of the returns seen so far, else
  an error asking for an annotation. A depth cap (`m_instDepth`, say 64) turns
  `f(x)` calling `f([x])` into an error instead of an infinite instantiation.
  Note the old coarse key hid this by accident (arrays collapsed to `int`).

Diagnostics: `err()` (sema.kmm) appends `" (in call f(int, string), called at
FILE:LINE:COL)"` while `m_instStack` is non-empty. The reported position is
the generic body's own line/column (preserved by the clone). This
generalises the single check at codegen_expr.kmm:176-183, which only handles
`string + int`, prints no call site and misfires on `__g` method names (L10).

### 3.4 Emission and the optimizer

Instances are ordinary `FunctionDecl`s in `prog.functions` with
`linkName = sym` (the field already exists, ast.kmm:300, used for overloads
and instances). Codegen Pass 1 registers them like any function; Pass 1-pre
(codegen.kmm:152-163) still removes the *template* decls. Deleted from codegen:
`ensureInstance`, `tplArgTypes`, `instSym`, `instDisplay`, `inferReturnType`,
`firstReturnType`, `retAnnot`, `envTypeOf`/`m_envNames`/`m_envTypes`,
`m_genQueue`/`m_genDone` and Pass 2b (codegen.kmm:226-234). The optimizer
runs after sema and therefore now optimises instances, typed (it skips the
templates themselves).

### 3.5 Overload sets and templates coexist

They never share a name: sema already rejects a template that shares a name
with any other declaration (sema.kmm:66-70), and instance (`__g`) and
overload (`__ov`) link names are disjoint (overload.kmm:56-60, 320).
Branch order in `inferCall` stays: builtins -> template -> overload set ->
plain.

Overload set members are *ordinary typed functions* that sema already
checks (sema.kmm:73-80, "Overload members are ordinary functions"). Today
only the call site is loose. Move `argFit`/`sigFit`/`sigSpecificity`/
`orderBySpecificity`/`selectOverload`/`overloadRet` (overload.kmm:101-250,
~150 lines) into sema, re-expressed over `Type`:

- fit 2 = `isAssignable(param, arg)` and exact (`typeToString` equal, or
  subtype for objects); fit 1 = possible at run time (`any`, union, a base
  class argument for a derived parameter); fit 0 = incompatible. Same ranking
  as today: unique exact -> static; several exact -> "ambiguous"; one
  scored -> static; several scored -> runtime dispatch with the
  specificity order.
- result: `callee.kind = "fn"` with `sym = linkName`, or `"dispatch"` with
  `cands` ordered; `e.ty` = common return type of the survivors, else `any`
  (today: `"int"`, overloadRet at overload.kmm:225-247).
- link names move to sema (Pass 1b, codegen.kmm:196-215, becomes
  `Analyzer`), so `m_fnLink` is read-only in codegen.
- Behaviour change to flag (Q3): `normParamType` (overload.kmm:34-49)
  collapses all int widths and `char` to `"int"`, so `f(x: u8)` and
  `f(x: int)` are the same overload today (Pass 1a rejects the duplicate,
  codegen.kmm:182). With `Type` they become distinct.
- `algorithm.sum(xs)` and every stdlib overload (stdlib/algorithm.kmm) get a
  real result type instead of `any`, so the `any`-poisoning in L3 disappears
  for them.

### 3.6 `module.fn` rewriting and default parameters

- `module.fn(...)` for a user module: sema already rewrites in place
  (`e.name = meth; e.qualifier = recv`, sema_expr.kmm:388-393) and re-enters
  `inferCall`. For stdlib/prototype calls the dotted name survives and the
  *backend* mangles it (`console.print` -> `console_print`, backend.kmm:150;
  codegen_frame.kmm:127, codegen_expr.kmm:526). In the new design sema sets
  `callee.sym = mangleDotted(...)` (sema.kmm:~1017) once, so codegen and
  backend stop re-deriving it, and the dotted-overload/template branch in
  `genCall` (codegen_expr.kmm:518-530, which builds a synthetic `CallExpr`)
  disappears: sema resolves the flat symbol through the same
  template/overload code as a bare name. Because instance bodies are now
  cloned *then* checked, the rewrite happens on the clone, which also closes
  the known gap "a `module.fn(...)` call inside a template body is not
  rewritten" (types.md:1008-1009) and makes L4's link-time `undefined
  reference` a compile error.
- Defaults: sema's `appendDefaults(e, decl)` appends a *clone* of each missing
  default literal (already stamped by `checkParamDefaults`,
  sema_stmt.kmm:~146), for plain functions and templates alike (overloads
  cannot have defaults, sema_stmt.kmm:135). For a template the appended
  arguments take part in the instance key, matching today's order (defaults
  filled before `tplArgTypes`, codegen_frame.kmm:130-132). `fillDefaults`
  (codegen_expr.kmm:398) and its two call sites go away, ending the
  AST mutation inside `exprType` (L8) and the aliasing of one default node
  across call sites.
- Macros: a use of `let K be e` is expanded in sema by `cloneExpr(e)` at the
  use site and stamped there, which removes codegen's macro stack
  (`isMacro`, `macroExprOf`, `pushMacro`, `macroScopeMark/Restore`) and L12.

## 4. Migration plan (13 steps)

Gate **G** after every step: `./build-self.sh` (must print `fixpoint OK`),
`./run-tests.sh` (49 examples x x86/stackvm/c, wasm when emcc+node present;
plus tests/interop), `./run-sema-tests.sh`, `tests/bind/run.sh`. "Rollback"
is `git revert` of the single step; steps are additive until step 10 unless
noted. Line deltas are estimates from the sizes cited above.

| # | Step | Files | Delta | Verify (beyond G) | Rollback |
|---|---|---|---|---|---|
| 1 | **Audit harness.** `tools/audit-exprtype.sh` builds a scratch copy with the fall-through/arm/parity prints used for this note and prints the histograms (section 1.4). Add `examples/typed_ir_audit.km` (+`.expected`) exercising method calls, macros, globals, slices, unary, `new`, `[float]`-free generic bodies and overloaded calls *inside template bodies*. Commit the baseline table in this note. | tools/, examples/ | +80 script, +60 example, 0 compiler | script output matches section 1.4 on the baseline tree; new example passes on all targets | delete files; no compiler effect |
| 2 | **Stamp what sema skips.** Infer struct/class field defaults in `registerStruct`/`registerClass` (infer only, no new checks); give codegen-synthesised nodes types at construction (codegen.kmm:477-514, codegen_expr.kmm:414-421,461,528). | sema.kmm, codegen.kmm, codegen_expr.kmm | +40 | audit: zero fall-through for programs without templates/overload results; G | revert; stamps are additive. Risk: a default expr whose inference throws; infer is already total, so low |
| 3 | **Make `resolvedSym` live.** Codegen uses `e.resolvedSym` when non-null (method owner + `fnRet`), falls back to re-resolution otherwise. Parity assertion in the audit build. | codegen_expr.kmm, codegen_frame.kmm | +15 | audit parity stays 100% (3530/3530 baseline) | revert |
| 4 | **Add `ty: Type?`** to `Expression` and `VarDecl`; `inferExpr`/`checkVarDecl` store it next to the legacy string; add `Codegen::tyOf`. No consumer. | ast.kmm, sema_expr.kmm, sema_stmt.kmm | +12 | G; compile time unchanged (one pointer store per node; `Type` sharing across 200k nodes tested) | revert |
| 5 | **Dedupe the pure helpers** (`dotIndex`, `isCompareOp`, `methodSym`, `parentOrEmpty`, `declName`, `ctorFieldOrder`) into one free-function file. Stateful ones wait for step 13. | new support/names.kmm, sema.kmm, codegen*.kmm | +30 / -60 | G; fixpoint asm may change textually, must stay a fixpoint | revert |
| 6 | **Frame layout reads `ty`.** `collectLocals` (codegen_frame.kmm:150-175), parameters (codegen_stmt.kmm:24-30), globals (codegen.kmm:75,828) use `typeToLegacy(s.ty)` when set, else today's path. Also stamps `Identifier.ref`. | codegen_frame.kmm, codegen_stmt.kmm, codegen.kmm, sema_expr.kmm | +30 / -20 | audit: legacy-vs-`ty` parity on every local; G | revert |
| 7 | **Clone library.** `frontend/astclone.kmm` (`cloneExpr/cloneStmt/cloneFunction`). Unused in production; verified by an audit-build mode that clones every function body before sema and must produce byte-identical `.asm` on all examples and on `main.km`. | new frontend/astclone.kmm, tools/ | +230 | byte-identical asm (the strong check); G | delete file |
| 8 | **Overload resolution in sema.** Port fit/select/specificity onto `Type`; `Callee`; link names assigned in sema; codegen consumes `callee` for overloaded calls and deletes `selectOverload`/`overloadRet`/Pass 1b. Keep `genOverloadDispatch` (emission). | sema_expr.kmm, sema.kmm, ast.kmm, codegen_expr.kmm, overload.kmm, codegen.kmm | +200 / -240 | `overload_demo`, `stdlib_demo`, `float_demo`, `random_demo`, `tests/sema/*overload*`; new tests: `let s: string = algorithm.sum(xs)` now an error; `f(u8)`/`f(int)` decision (Q3) | revert (self-contained); this is the first step with an intended behaviour change |
| 9 | **Sema monomorphisation behind a flag** (`kmc ... mono-sema`, default off): nested fn context, return inference, instance cache, depth cap, `m_instStack` diagnostics, instances appended to `prog.functions`, `callee.kind="inst"`. Codegen still has its own path when the flag is off. CI runs the whole suite in both modes. | sema.kmm, sema_expr.kmm, sema_stmt.kmm, km_compiler.kmm, main.km, tools/ | +320 | G in both modes; asm of flag-on vs flag-off compared on the 4 generic examples (differs only in symbols/where unavoidable) | revert; flag is off so no behaviour at risk |
| 10 | **Flip default to sema monomorphisation; delete the codegen machinery** (list in 3.4). New tests: `tests/sema/template_*.km` with `.experr` per-instance errors (`in call f(int, string)`); `examples/generic_arrays.km` for `[float]`/`[int]`/`bool`/`char`; undefined function in body is a compile error. | overload.kmm, codegen.kmm, codegen_frame.kmm, codegen_expr.kmm, examples/, tests/sema/ | -200 | G; self-compile of `main.km` (the compiler contains templates, see audit) must stay a fixpoint | revert restores codegen path (kept in git history); flip is one-line if only the flag default is reverted |
| 11 | **Defaults, `module.fn`, macros in sema.** `appendDefaults`, `callee.sym` final link symbol, macro expansion by clone. Delete `fillDefaults`, dotted/sibling branches of `genCall`, macro stack in codegen. | sema_expr.kmm, codegen_expr.kmm, codegen_stmt.kmm, codegen.kmm | +60 / -120 | `default_params`, `default_*` sema tests, a macro-in-two-contexts test; G | revert |
| 12 | **Typed `exprType`.** Replace the 61 external `exprType(x)` by `legacyOf(x.ty)` (mechanical, no logic change), then delete the fall-through (codegen_frame.kmm:69-137) and throw an internal error `unstamped node` instead of guessing. Optimizer reads `ty.kind`. Add the post-sema verifier (invariant in 2.3). | codegen_*.kmm, overload.kmm, optimize.kmm, sema.kmm | -110 / +40 | audit must show 0 fall-through on corpus (the precondition); G. **Highest risk step**: a missed node now fails loudly | revert; the `legacyOf` rename can land first as its own commit |
| 13 | **Share the registry.** `SymTab` (types, parents, fields, methods, fn signatures, globals) owned by `Analyzer`, passed to `Codegen`; delete the remaining duplicated helpers (`hasFn`, `isObjType`, `isAbstractType`, `isDescendantOf`, `typeIndex`, `typeParentOf`, `isCurField`, `isGlobalVar`, `isMethodReceiver`, `receiverType`, `macroExprOf`, `pushMacro`). Most are already unused after steps 6, 8, 11. Keep codegen-only data (type ids, vtable slots, C layouts). | new analyze/symtab.kmm, sema.kmm, codegen.kmm | +80 / -250 | G; type-id numbering must not change | revert |

Net: roughly +1100 added / -1100 removed across the plan, with the code that
decides types (sema) growing and the code that guesses them (codegen)
shrinking. Step 13 is last on purpose: it is the biggest mechanical change
to files other agents are editing, and by then it is mostly deletion.

Ordering rationale. Steps 1-3 reduce *uncertainty* without changing
behaviour (they prove what fires, fix two cheap omissions, make a dead field
live). Step 4 adds the data. Steps 5-7 are safe prerequisites. Steps 8-10
are the substantive behaviour changes, each behind its own tests, and 9 is
flag-guarded so the risky code is exercised before it is the default. 11-13
delete.

## 5. What this unlocks, and what it does not

Unlocks:

- **Template body checking.** Every instance is type-checked with concrete
  types; a bad body is a compile error, not a miscompile (L4), a link error
  or `error: 0:` (L4).
- **Per-instance diagnostics**, e.g. `error: 'string' is not assignable to
  'int' (in call add(int, string), called at f.km:12:5)`, with the generic
  body's own line/column.
- **Correct instances**: `[float]` vs `[int]`, `bool`, `char` no longer share
  code (L7, a real wrong-result bug).
- **No `any` poisoning** from template and overload calls (L2/L3): the
  result of `algorithm.sum(xs)` or `id(3)` is typed, so downstream
  assignments are checked.
- **Optimizer data-flow foundations.** Types instead of "int"-or-not, and
  instance bodies exist before the optimizer runs, so folding, strength
  reduction (width-aware) and a later inliner can see them. Constant
  propagation today is only never-reassigned `let x = <literal>` (optimize.kmm
  header, "max"); real data-flow still needs a CFG, which this does not add.
- **FFI on c/wasm, partially.** `backend_c.kmm:586-611` rejects every `extern`
  call and by-value struct because the x86 ABI classification is done in
  codegen (`genExternCall`, codegen_expr.kmm:609-) and handed to the
  backend as register classes. With `callee.kind == "extern"` plus
  `decl.paramTys/retTy` the C backend could instead emit a plain C call with a
  prototype built from `Type` and let the C compiler do the ABI (struct by
  value, floats, callbacks as function pointers); wasm goes through the same C
  backend (docs/ffi.md:684). That needs a new `Backend` hook at call
  granularity (e.g. `callExtern(sym, [Type], Type)`) and C prototype
  emission: the IR supplies the data, it does not implement the feature.
- **Fewer places to disagree**: one registry, one decision per call.

Does **not** solve:

- The accumulator/stack-machine backend interface (an IR choice (B) would
  not fix either); backends still receive coarse class strings.
- Generics beyond implicit-type free functions: no explicit `<T>`, no
  constraints (errors are C++-style, after instantiation), no generic types
  or methods (`isTemplateDecl` excludes `ownerClass != null`, overload.kmm:257),
  no instantiation of overload members.
- Uninstantiated templates are still unchecked unless the optional
  "check once with `any` parameters" pass is added (Q4).
- `any` itself (un-annotated bindings that are not template parameters) and
  runtime dispatch remain; so does coarse ABI tagging (`argTag`).
- A CFG / SSA optimiser, inlining policy, or any speed-up of generated code.
- Compile-time cost is expected to be small (a few instances, one extra tree
  per instance, one pointer per node) but is **not measured**; kmc is
  currently about 0.25 s per compile (memory note `komodo-compile-perf`), which
  should be re-checked after steps 4 and 9.

## 6. Risks and open questions for the maintainer

Risks:

1. **Step 12 changes failure mode** from "guess int" to "internal error". The
   audit tool is the safeguard, but it is only as good as the corpus (it
   already misses template-body method calls/macros/globals; step 1 adds
   them). A fuzz-ish pass compiling every `.km`/`.kmm` under `stdlib/`,
   `demos/` and `tests/` in the audit would widen it.
2. **Concurrent edits.** Steps 6, 8, 11, 12, 13 touch `codegen_expr.kmm`,
   `codegen.kmm` and `overload.kmm`, which other agents are editing. The
   additive steps (1-4, 7, 9) do not conflict structurally; merge order
   should be fixed with those agents.
3. **Behaviour changes are intended in 8 and 10** (distinct overloads per int
   width, distinct `bool`/`char`/array instances, `bool` results printing as
   `true`/`false` instead of `1`). Existing `.expected` files may need review,
   not just regeneration.
4. **Memory/ownership**: stamping one `Type` object onto many nodes relies on
   the conservative auto-free analysis (memory note `komodo-ffi-and-ownership`)
   treating a field store as an escape. A 200k-node sharing test passed on the
   current compiler, but that is not a proof; step 4 should keep that test.
5. **Nested `checkFunction`** (3.3) is the most invasive sema change: ~25
   fields of implicit per-function state. A missed field would leak narrowing
   or move-state from the caller into the instance or back.
6. `m_curInst`/`instDisplay` (L10) should be fixed independently (exact
   `__g<digits>_` match, or a flag on `FunctionDecl.instOf`); it is a
   one-line risk today.

Open questions I cannot decide from the code:

- **Q1** Is `Type` allowed to leak into `ast.kmm`? It makes the frontend
  directory depend on analyze/types.kmm (same module, so it compiles, but it
  inverts the documented stage layout in km_compiler.kmm:20-37). Alternative:
  move `Type` to `modules/support/`.
- **Q2** Return type of a template with several `return`s of different
  types: keep today's "first return wins" (`firstReturnType`,
  overload.kmm:336), `unify`, or require agreement? Also, may recursive
  templates without a return annotation be required to annotate?
- **Q3** Should `f(x: u8)` and `f(x: int)` be distinct overloads, and should
  `bool`/`char` overloads be distinct from `int`/`string`? (Today they
  collapse, overload.kmm:34-49.)
- **Q4** Check an uninstantiated template once with `any` parameters? It
  catches undefined names and arity errors in unused code but may reject
  programs that compile today; implicit-any warnings (sema_stmt.kmm:41) would
  need suppressing.
- **Q5** Nested instantiation: save/restore the per-function fields in one
  `Analyzer` (my default), or run a second `Analyzer` sharing registries? The
  second needs reference-sharing of the registry arrays, which I could not
  confirm from the docs.
- **Q6** Should instance link names stay `name__g<n>_<types>` (continuity with
  existing `.asm` / `komodo` caches), or include a hash? Collisions are
  impossible with `typeToString` keys except through label-safe-ification of
  odd names.
- **Q7** `Program` currently has no "typed" state beyond `analyzed`
  (ast.kmm:364). Do you want a distinct `TypedProgram` wrapper to make the
  stage boundary explicit even under option (A)? I did not propose it because
  it adds a class without removing a problem.
- **Q8** Latent L12 (macro stamps overwritten per use) is untested; confirm
  whether scoped macros can legitimately reference names whose types differ
  between uses.

## Maintainer decisions (answers to the open questions)

1. **Several `return`s of different types in a generic function:** all returns must
   agree, otherwise a compile error; recursive generics must annotate their return type.
2. **Overloads by numeric width / bool / char:** distinct. `f(u8)` and `f(int)` are
   separate overloads and instances. Consequence to handle: a `bool` result of a
   generic instance may print as `true`/`false` instead of `1`/`0`; audit the
   examples' `.expected` files when this lands.
3. **Uncalled generic functions:** skipped until used (today's behavior). No
   opaque-type check of uninstantiated bodies.
4. **Layering:** move `Type` (and its helpers) from `modules/analyze/types.kmm` to
   `modules/support/` so `ast.kmm` and the analyzer both depend on it.
5. **Constraint:** additive only — extend the existing AST/sema/codegen; no rewrites.
