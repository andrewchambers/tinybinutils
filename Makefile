CC ?= cc
CFLAGS ?= -O2 -g -Wall -Wextra
LDLIBS ?=
AS_CFLAGS = $(CFLAGS) -Wno-sign-compare -Wno-implicit-fallthrough \
	-Wno-unused-parameter -Wno-unused-variable -Wno-clobbered \
	-Wno-missing-field-initializers

PROGS = tinyld tinyld-x86_64 tinyld-aarch64 tinyld-riscv64 \
	tinyas tinyas-x86_64 tinyas-aarch64 tinyas-riscv64 tinyar
COMMON_SRCS = tinyld.c tinyld_support.c tccelf.c
COMMON_DEPS = tcc.h tinyld.h elf.h x86_64-link.c arm64-link.c riscv64-link.c
AS_COMMON_SRCS = tinyas.c tinyld_support.c tinyas_support.c tccelf.c tccpp.c tccasm.c
AS_DEPS = $(COMMON_DEPS) tcctok.h i386-tok.h arm64-tok.h riscv64-tok.h \
	i386-asm.c x86_64-asm.h arm64-asm.c arm64-tok.h riscv64-asm.c riscv64-tok.h

X86_64_OBJS = $(COMMON_SRCS:%.c=build/x86_64/%.o) build/x86_64/x86_64-link.o
AARCH64_OBJS = $(COMMON_SRCS:%.c=build/aarch64/%.o) build/aarch64/arm64-link.o
RISCV64_OBJS = $(COMMON_SRCS:%.c=build/riscv64/%.o) build/riscv64/riscv64-link.o
X86_64_AS_OBJS = $(AS_COMMON_SRCS:%.c=build/as-x86_64/%.o) \
	build/as-x86_64/i386-asm.o build/as-x86_64/tinyas_x86_64.o
AARCH64_AS_OBJS = $(AS_COMMON_SRCS:%.c=build/as-aarch64/%.o) \
	build/as-aarch64/arm64-asm.o
RISCV64_AS_OBJS = $(AS_COMMON_SRCS:%.c=build/as-riscv64/%.o) \
	build/as-riscv64/riscv64-asm.o

.PHONY: all clean

all: $(PROGS)

tinyld: tinyld-x86_64
	cp tinyld-x86_64 tinyld

tinyas: tinyas-x86_64
	cp tinyas-x86_64 tinyas

tinyld-x86_64: $(X86_64_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(X86_64_OBJS) $(LDLIBS)

tinyld-aarch64: $(AARCH64_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(AARCH64_OBJS) $(LDLIBS)

tinyld-riscv64: $(RISCV64_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(RISCV64_OBJS) $(LDLIBS)

tinyas-x86_64: $(X86_64_AS_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(X86_64_AS_OBJS) $(LDLIBS)

tinyas-aarch64: $(AARCH64_AS_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(AARCH64_AS_OBJS) $(LDLIBS)

tinyas-riscv64: $(RISCV64_AS_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(RISCV64_AS_OBJS) $(LDLIBS)

tinyar: tinyar.o
	$(CC) $(LDFLAGS) -o $@ tinyar.o

build/x86_64/%.o: %.c $(COMMON_DEPS)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -DTCC_TARGET_X86_64 $(CFLAGS) -c -o $@ $<

build/aarch64/%.o: %.c $(COMMON_DEPS)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -DTCC_TARGET_ARM64 $(CFLAGS) -c -o $@ $<

build/riscv64/%.o: %.c $(COMMON_DEPS)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -DTCC_TARGET_RISCV64 $(CFLAGS) -c -o $@ $<

build/as-x86_64/%.o: %.c $(AS_DEPS)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -DTCC_TARGET_X86_64 $(AS_CFLAGS) -c -o $@ $<

build/as-aarch64/%.o: %.c $(AS_DEPS)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -DTCC_TARGET_ARM64 $(AS_CFLAGS) -c -o $@ $<

build/as-riscv64/%.o: %.c $(AS_DEPS)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -DTCC_TARGET_RISCV64 $(AS_CFLAGS) -c -o $@ $<

tinyar.o: elf.h

clean:
	rm -rf build
	rm -f $(PROGS) tinyar.o
