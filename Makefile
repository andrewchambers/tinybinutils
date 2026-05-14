CC ?= cc
CFLAGS ?= -O2 -g -Wall -Wextra
LDLIBS ?=

PROGS = tinyld tinyld-x86_64 tinyld-aarch64 tinyld-riscv64 tinyar
COMMON_SRCS = tinyld.c tinyld_support.c tccelf.c
COMMON_DEPS = tcc.h tinyld.h x86_64-link.c arm64-link.c riscv64-link.c

X86_64_OBJS = $(COMMON_SRCS:%.c=build/x86_64/%.o) build/x86_64/x86_64-link.o
AARCH64_OBJS = $(COMMON_SRCS:%.c=build/aarch64/%.o) build/aarch64/arm64-link.o
RISCV64_OBJS = $(COMMON_SRCS:%.c=build/riscv64/%.o) build/riscv64/riscv64-link.o

.PHONY: all clean

all: $(PROGS)

tinyld: tinyld-x86_64
	cp tinyld-x86_64 tinyld

tinyld-x86_64: $(X86_64_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(X86_64_OBJS) $(LDLIBS)

tinyld-aarch64: $(AARCH64_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(AARCH64_OBJS) $(LDLIBS)

tinyld-riscv64: $(RISCV64_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(RISCV64_OBJS) $(LDLIBS)

tinyar: build/tinyar.o
	$(CC) $(LDFLAGS) -o $@ build/tinyar.o

build/x86_64/%.o: %.c $(COMMON_DEPS)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -DTINY_TARGET_X86_64 $(CFLAGS) -c -o $@ $<

build/aarch64/%.o: %.c $(COMMON_DEPS)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -DTINY_TARGET_ARM64 $(CFLAGS) -c -o $@ $<

build/riscv64/%.o: %.c $(COMMON_DEPS)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -DTINY_TARGET_RISCV64 $(CFLAGS) -c -o $@ $<

build/tinyar.o: tinyar.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf build
	rm -f $(PROGS)
