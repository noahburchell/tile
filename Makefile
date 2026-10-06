CFLAGS ?= -O2 -pipe
PREFIX ?= /usr/local

WARN := \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Wno-unused-parameter \
	-Wshadow \
	-Wundef \
	-Wcast-qual \
	-Wcast-align \
	-Wwrite-strings \
	-Wpointer-arith \
	-Wmissing-prototypes \
	-Wmissing-declarations \
	-Wstrict-prototypes \
	-Wold-style-definition \
	-Wredundant-decls \
	-Wnested-externs \
	-Wswitch-enum \
	-Wformat=2 \
	-Wvla \
	-Wdouble-promotion \
	-Wfloat-equal \
	-Winit-self \
	-Wmissing-include-dirs \
	-Wimplicit-fallthrough \
	-Wnull-dereference \
	-Wdate-time \
	-Wmissing-variable-declarations

ifneq ($(shell $(CC) -dM -E -x c /dev/null 2>/dev/null | grep -c __clang__),0)
WARN += \
	-Wshadow-all \
	-Wassign-enum \
	-Wcomma \
	-Wconditional-uninitialized \
	-Wloop-analysis \
	-Wunreachable-code-aggressive \
	-Wextra-semi \
	-Wover-aligned \
	-Wformat-type-confusion \
	-Wtautological-constant-in-range-compare \
	-Wbitfield-enum-conversion \
	-Wcast-function-type-strict \
	-Wthread-safety \
	-Wheader-guard \
	-Wnewline-eof
else
WARN += \
	-Wstrict-aliasing=3 \
	-Wlogical-op \
	-Wduplicated-cond \
	-Wduplicated-branches \
	-Wjump-misses-init \
	-Wtrampolines \
	-Walloc-zero \
	-Walloca \
	-Warray-bounds=2 \
	-Wshift-overflow=2 \
	-Wstringop-overflow=4 \
	-Wstringop-truncation \
	-Wformat-overflow=2 \
	-Wformat-truncation=2 \
	-Wuse-after-free=3 \
	-Wflex-array-member-not-at-end \
	-Wbidi-chars=any \
	-Wdisabled-optimization \
	-Wcalloc-transposed-args \
	-Wmultistatement-macros \
	-Wsuggest-attribute=format \
	-Wsuggest-attribute=noreturn \
	-Wattribute-alias=2 \
	-Wtrivial-auto-var-init
endif

override CFLAGS += -std=gnu23 $(WARN)

SRC := src/sway.c src/tile.c

build/tile: $(SRC) src/sway.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(SRC)

install: build/tile
	install -Dm755 build/tile $(DESTDIR)$(PREFIX)/bin/tile

clean:
	rm -f build/tile

.PHONY: install clean
