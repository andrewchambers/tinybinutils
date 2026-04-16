# TinyCC (Tiny C Compiler)

## Build & Test

```sh
./configure && make && make test
```

- `make` builds `tcc` + `libtcc.a` + `libtcc1.a`. `libtcc1.a` is built by a recursive `make -C lib`.
- `make test` runs the full test suite (`tests/`), ~2 minutes.
- Run a single tests2 test: `make tests2.37` (run) or `make tests2.37+` (update .expect).
- Run a single tests/pp test: `make testspp.17`.
- Sanitizer tests: `make sani-test`, `make sani-tests2.37`, `make sani-testspp.17`.
- Coverage tests: `make tcov-test`, `make tcov-tests2.37`, `make tcov-testspp.17`.
- Test installed tcc: `make test-install`.

## Architecture

- **Single-file-ish compiler**: tcc compiles itself (or can be bootstrapped). `tcc.c` is the main entrypoint; `tccpp.c` handles preprocessing, `tccgen.c` handles code generation, `tccelf.c` handles ELF linking, `tccasm.c` handles inline asm, `tccrun.c` handles runtime execution, `tccdbg.c` handles debug info.
- **Backend files per architecture**: `i386-gen.c`, `x86_64-gen.c`, `arm-gen.c`, `arm64-gen.c`, `riscv64-gen.c`, `c67-gen.c`, plus corresponding asm/link files.
- **Generated file**: `include/tccdefs.h` is converted to `tccdefs_.h` by `c2str.exe` (built from `conftest.c`). Always exists after first `make`.
- **Cross-compilers**: `make cross` builds all cross-compilers; `make cross-i386`, `make cross-x86_64`, etc. for one target. `make CROSS_TARGET=i386 ONE_SOURCE=yes` for single-object cross build.
- **`ONE_SOURCE=yes`**: builds tcc as a single object file (default for cross-compilers, native uses separate objects).

## Coding Style

- **C90** with these pre-existing non-C90 features: `long long`, `inline`, `//` comments, unnamed struct/union fields, empty macro arguments, assignment between function pointer and `void *`.
- **Indentation**: 4 spaces for most files; some files (e.g. `i386-gen.c`) use 2 spaces for deep nesting.
- **Do not reformat** existing code unless the change is the primary purpose.
- Remove tabs and trailing whitespace from modified lines.
- Check with `./configure --extra-cflags="-std=c90 -Wpedantic"`.

## Test Quirks

- `test.ref` (reference output) is generated from `tcctest.c` using gcc: `(cd tests && gcc -I.. tcctest.c && valgrind -q ./a.out > test.ref)` then `make test TCC="valgrind -q --leak-check=full $(pwd)/tcc -B$(pwd) -I$(pwd)"`.
- VLA tests (`79_vla_continue`) produce expected invalid reads under Valgrind.
- tests2 skip list is in `tests/tests2/Makefile` (architecture- and config-specific skips).
- Some tests need special flags (see `FLAGS =` assignments in `tests/tests2/Makefile`).
- Bound-check tests (`-b` flag) are skipped when `CONFIG_bcheck=no`.
- `.expect` files are generated with gcc by default; some use TCC (flagged with `GEN-TCC`).

## Language Extension: Optional Semicolons

Semicolons are optional after most statements and declarations:
- `return`, `break`, `continue`, `goto`, expression statements
- `do-while` trailing semicolon
- Variable declarations

Semicolons are still **required** for:
- `for()` loop init/condition/increment sections (e.g., `for(i=0; i<n; i++)`)
- Old-style function parameter declarations
- Struct/enum member declarations

The `skip(';')` function in `tccpp.c:100` errors if token doesn't match. Use `if (tok == ';') next();` instead to make semicolons optional.
