# Compiler error codes

Every diagnostic the compiler prints in the rich layout carries a code.
Run `komodo explain K0001` for the long form. See [diagnostics](../diagnostics.md).

| Code | Message |
|---|---|
| [K0001](K0001.md) | `undefined name '…'` |
| [K0002](K0002.md) | `undefined function '…::…'` |
| [K0003](K0003.md) | `assignment to undeclared variable '…'` |
| [K0004](K0004.md) | ``new` of unknown type '…'` |
| [K0005](K0005.md) | `type '…' has no field '…'` |
| [K0006](K0006.md) | `type '…' has no method '…'` |
| [K0007](K0007.md) | `struct pattern names unknown type '…'` |
| [K0008](K0008.md) | `type '…' inherits unknown type '…'` |
| [K0009](K0009.md) | ``is` expects a known type name on the right` |
| [K0010](K0010.md) | `'…' is private to class '…'` |
| [K0011](K0011.md) | `'…' is private to module '…'` |
| [K0012](K0012.md) | `cannot find module '…'` |
| [K0013](K0013.md) | `file not found: …` |
| [K0020](K0020.md) | `argument … to '…': type '…' is not assignable to '…'` |
| [K0021](K0021.md) | `argument … to extern '…': type '…' is not assignable to handle type '…'` |
| [K0022](K0022.md) | `argument … to extern '…': '…' is not compatible with '…'` |
| [K0023](K0023.md) | `field '…' of '…': type '…' is not assignable to '…'` |
| [K0024](K0024.md) | `…: type '…' is not assignable to '…'` |
| [K0025](K0025.md) | `cannot add '…' and '…' (use string(x) or an interpolated string)…` |
| [K0026](K0026.md) | `operator '…' is not defined for the opaque handle type '…'` |
| [K0027](K0027.md) | ``in` on a string wants a string / char on the left, got '…'` |
| [K0028](K0028.md) | ``in`: '…' can't be an element of '…'` |
| [K0029](K0029.md) | ``in` wants a string or an array on the right, got '…'` |
| [K0030](K0030.md) | `field access on a non-object (type '…')` |
| [K0031](K0031.md) | `field assignment on a non-object (type '…')` |
| [K0032](K0032.md) | `'int' expects a number, got '…'` |
| [K0040](K0040.md) | `'…' takes at least … argument(s), got …` |
| [K0041](K0041.md) | `'…' takes … argument(s), got …` |
| [K0042](K0042.md) | `method '….…' takes … argument(s), got …` |
| [K0043](K0043.md) | `constructor '…' takes … argument(s), got …` |
| [K0044](K0044.md) | ``new …` given more values than it has fields` |
| [K0045](K0045.md) | `'assert' takes a condition and an optional message, got … argument(s)` |
| [K0046](K0046.md) | `'…' takes exactly one argument` |
| [K0047](K0047.md) | `cannot `copy` the opaque handle type '…'` |
| [K0050](K0050.md) | `… operand of '…' is possibly null` |
| [K0051](K0051.md) | `…possibly null — narrow it before calling '…'` |
| [K0052](K0052.md) | `in '…': value of type '…' is possibly null — narrow it before reading '.…'` |
| [K0053](K0053.md) | `value of type '…' is possibly null — narrow it before indexing` |
| [K0060](K0060.md) | ``break` outside a loop` |
| [K0061](K0061.md) | ``continue` outside a loop` |
| [K0062](K0062.md) | `a void function cannot return a value` |
| [K0063](K0063.md) | `a value function must return a value` |
| [K0064](K0064.md) | `function '…' must return a value on every path` |
| [K0070](K0070.md) | `cannot assign to macro '…' (declared with be)` |
| [K0071](K0071.md) | `cannot reassign immutable binding '…' (declared with `be`)` |
| [K0072](K0072.md) | `pure `fn …` mutates state — declare it `mut`` |
| [K0073](K0073.md) | `a pure `fn` cannot call the void procedure '…'` |
| [K0074](K0074.md) | ``let … be` needs a value` |
| [K0080](K0080.md) | `duplicate function '…'` |
| [K0081](K0081.md) | `duplicate type '…'` |
| [K0082](K0082.md) | `duplicate macro '…'` |
| [K0083](K0083.md) | `duplicate top-level name '…'` |
| [K0084](K0084.md) | `template '…' cannot share its name with another '…' declaration (a template does not overload)` |
| [K0085](K0085.md) | `top-level '…' needs a type annotation or an initialiser` |
| [K0086](K0086.md) | `cyclic or unresolved inheritance near '…'` |
| [K0087](K0087.md) | `cannot instantiate abstract type '…'` |
| [K0088](K0088.md) | `cannot construct `extern type …` — it is an opaque C handle; obtain one from a C function` |
| [K0090](K0090.md) | `default parameter values are only allowed on free functions with a body` |
| [K0091](K0091.md) | `overloaded function '…' cannot have default parameter values` |
| [K0092](K0092.md) | `parameter '…' needs a default value, because it follows a parameter that has one` |
| [K0093](K0093.md) | `default value of parameter '…' must be a literal` |
| [K0100](K0100.md) | ``export fn main` is not allowed…` |
| [K0101](K0101.md) | `export fn '…' cannot be overloaded…` |
| [K0102](K0102.md) | `export fn '…': parameter '…' needs a type annotation` |
| [K0103](K0103.md) | `export fn '…': parameter '…' has type '…', which can't cross the C ABI…` |
| [K0104](K0104.md) | `export fn '…' cannot have default parameter values…` |
| [K0105](K0105.md) | `export fn '…': return type '…' can't cross the C ABI…` |
| [K0106](K0106.md) | `extern…: character '…' is not allowed in the C symbol "…"` |
| [K0107](K0107.md) | `extern '…': the C symbol "…" cannot start with a digit` |
| [K0108](K0108.md) | `extern variable '…': type '…' is not supported…` |
| [K0109](K0109.md) | `'…' cannot be a C callback: parameter type '…' is not a plain machine word` |
| [K0110](K0110.md) | `'…' cannot be a C callback: return type '…' is not a plain machine word` |
| [K0111](K0111.md) | ``export` needs a free function with a body: '…'` |
| [K0112](K0112.md) | `extern variable '…' needs a type…` |
| [K0113](K0113.md) | `expected a string literal after `=` (the C symbol name) in extern…` |
| [K0114](K0114.md) | `export fn is not supported on target '…' yet…` |
| [K0120](K0120.md) | `no overload of '…' takes … argument(s)` |
| [K0121](K0121.md) | `ambiguous overloaded call to '…'` |
| [K0122](K0122.md) | `no overload of '…' matches the argument types` |
| [K0123](K0123.md) | `duplicate function '…' — two overloads have identical parameter types` |
| [K0130](K0130.md) | `use of moved value '…' — …` |
| [K0131](K0131.md) | `cannot move `unique` value out of '…' — it is not a locally owned binding` |
| [K0132](K0132.md) | `cannot move `unique` value of type '…' out of field '…' — …` |
| [K0133](K0133.md) | `cannot move `unique` value of type '…' out of an array element` |
| [K0140](K0140.md) | `struct pattern '…(…)' used on non-struct subject of type '…'` |
| [K0141](K0141.md) | `struct pattern type '…' is unrelated to subject type '…'` |
| [K0142](K0142.md) | `field '…' named twice in pattern '…'` |
| [K0143](K0143.md) | `struct pattern cannot match on `float` field '…' yet` |
| [K0144](K0144.md) | `pattern for field '…': literal of type '…' is not comparable to field type '…'` |
| [K0145](K0145.md) | `cannot destructure the opaque handle type '…' in a pattern` |
| [K0146](K0146.md) | `match on enum '…' is not exhaustive — missing: …` |
| [K0160](K0160.md) | `unreachable code` |
| [K0161](K0161.md) | `self-assignment of '…' has no effect` |
| [K0162](K0162.md) | ``if` condition is always …` |
| [K0163](K0163.md) | `division by zero` |
| [K0164](K0164.md) | `'…' compares a value with itself` |
| [K0165](K0165.md) | `'…' is declared but never used` |
| [K0166](K0166.md) | `parameter '…' has no type annotation (implicit any)` |
| [K0167](K0167.md) | ``mut …` has no mutating code — could be `fn` or `void`` |
| [K0168](K0168.md) | `'…' has no type annotation and no initialiser` |
| [K0180](K0180.md) | `unexpected 'else': a one-line `if c then X else Y` is not supported…` |
| [K0181](K0181.md) | `a range `a to b` is only valid in a `for` loop or a slice, not as a value` |
| [K0182](K0182.md) | `'?' applies to calls, not indexing` |
| [K0183](K0183.md) | `'?' applies to calls and `new` expressions only` |
| [K0184](K0184.md) | `struct bodies hold fields only; use a class for methods` |
| [K0185](K0185.md) | ``link` may only be used inside a module…` |
| [K0186](K0186.md) | `expected `case` or `else` in match, got …` |
| [K0187](K0187.md) | `unexpected '…'…` |
| [K0188](K0188.md) | `Unexpected top-level token…` |
| [K0189](K0189.md) | `expected `struct` or `class` after type modifier` |
| [K0190](K0190.md) | `method defined for unknown class '…'` |
| [K0191](K0191.md) | `unknown target '…'…` |
| [K0200](K0200.md) | `…not supported yet…` |
| [K0201](K0201.md) | `internal: …` |
