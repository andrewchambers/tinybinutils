# tinybinutils

tinybinutils is a small ELF-only linker and archive toolset forked from
TinyCC. It is intended for bootstrap environments and small compilers that
need compact static-linking tools.

## Tools

- `tinyld`: x86_64 linker alias
- `tinyld-x86_64`, `tinyld-aarch64`, `tinyld-riscv64`
- `tinyar`: static archive creator, lister, and extractor

## Build

```sh
make
```

## Examples

```sh
cc -c -o start.o start.s
./tinyld -o prog start.o libc.a
./tinyar crs libfoo.a foo.o bar.o
```

## Scope

- ELF64 only
- static linking only
- relocatable `.o` and static `.a` input
- relocatable output with `tinyld -r`
- x86_64, AArch64, and RISC-V 64-bit Linux targets

## Unsupported By Design

- dynamic libraries
- shared object output
- PIE output
- linker scripts

If you need these features then consider installing a more full featured toolchain.

Some compatibility flags such as `-static`, `-nostdlib`, `-s`, and common `-z` linker options are accepted
and ignored where that keeps existing build systems working.
