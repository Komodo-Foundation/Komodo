# Debugging Komodo programs

The x86 target can describe your program to a debugger (gdb, lldb) in terms
of Komodo source: breakpoints on `file.km:LINE`, backtraces with Komodo
function names and lines, and a call chain on uncaught errors and crashes.
Debug info is off by default; the normal output is byte-for-byte unchanged.

## Turning it on

Set `KM_DEBUG=1` for the compiler, assemble with `-g -F dwarf`, link with `-g`:

```bash
KM_DEBUG=1 kmc prog.km                       # writes prog.km.asm with line info
nasm -g -F dwarf -f elf64 prog.km.asm -o prog.o
gcc -g -no-pie -Istdlib -o prog prog.o stdlib/*.c -lm -l:libgc.so.1
```

(`kmc` is the compiler binary built by `build-self.sh`; the link line is the
one `komodo` uses, plus `-g`.) Then:

```bash
gdb -batch -ex 'break oop_demo.km:14' -ex run -ex bt ./prog
```

```text
Breakpoint 1, Counter__bump () at oop_demo.km:14
14	        m_n = m_n + 1
#0  Counter__bump () at oop_demo.km:14
#1  0x00000000004029f7 in km_program_main () at oop_demo.km:37
#2  0x0000000000409152 in main (argc=1, argv=0x7fffffffda18) at .../stdlib/runtime.c:640
```

`tests/debug/run.sh` runs this recipe end to end and asserts the stops.

## What `KM_DEBUG=1` changes

* **Line table.** Before each statement (and each function) the backend emits
  NASM's `%line N+0 file`. `+0` keeps every following assembly line on source
  line N until the next directive, so `nasm -g -F dwarf` records the Komodo
  file and line instead of the `.asm`'s. Modules are merged into one
  translation unit; the backend gets the same file/start-line map the
  diagnostics use (`locate`), so each directive names the *original* file and
  line (the path as the compiler found it: relative to where you ran it for
  your own files, under `stdlib/` for the library). Granularity is one entry
  per statement: a multi-line expression maps to its first line.
* **Function symbols.** Every function is emitted as a global `FUNC` symbol
  with a size, and jump targets become `..@Ln` labels, so any address inside a
  function resolves to that function (not to a jump label). Default builds keep
  functions file-local and untyped.
* **Runtime tables.** `km_text_begin`/`km_text_end` (extent of the generated
  code) and `km_dbg_lines` (offset, line, file rows, sorted) are added for
  the runtime's own reports, below.

Because function symbols are global in a debug build, a Komodo function that
shares its name with a C library function the runtime calls can rebind that
call (or fail to link with "multiple definition"). Don't ship debug builds.

## Symbol names

| Komodo | symbol / gdb name |
|---|---|
| `fn f` | `f` |
| `Class` method `m` | `Class__m` (constructor: `Class__ctor`) |
| function in a `module` file | plain `f` (module members are not prefixed); a C-backed `module.f` links as `module_f` |
| `void main()` | `km_program_main` (the C runtime provides the real `main`) |
| overload / generic instance | `name__ov<N>_<types>` / `name__g<N>_<types>` |

`break Class__method` works in gdb. The runtime's reports below show the
source spelling (`Class::method`, `main`).

## Call chains on errors and crashes

With a `KM_DEBUG=1` build an uncaught error prints the Komodo call chain,
innermost first, with the line of each call:

```text
Uncaught error: boom
  at inner (trace.km:12)
  at Job::run (trace.km:7)
  at outer (trace.km:17)
  at main (trace.km:21)
```

A crash (null access, division by zero, stack overflow) adds the line to its
headline and to each frame of the existing "Komodo call stack":

```text
Komodo: crash - integer division by zero in divide (crash.km:4)
Komodo call stack (innermost first):
    divide (crash.km:4)
    main (crash.km:11)
```

Frames are found by walking the `rbp` chain (every generated function begins
`push rbp; mov rbp, rsp`) and named from the executable's own symbol table,
so there is no address table to embed; lines come from `km_dbg_lines`.
Recursive runs are folded (`(x N, recursive)`) and very deep stacks elided.

A plain build prints only `Uncaught error: boom` (unchanged). Set `KM_TRACE=1`
when *running* any build to get the chain with function names only:

```text
Uncaught error: boom
  at inner
  at Job::run
  at outer
  at main
```

Output goes to stderr and contains no addresses, so it is stable enough to
test (`tests/debug/trace.expected`).
