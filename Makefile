# WCPL -- standalone self-hosted C compiler/linker/libc targeting
# WebAssembly and WASI.
#
#   make           build the native compiler
#   make test      compile the tests and examples, then check self-hosting
#   make install   install the binary
#   make clean     remove build products
#
# The compiler is self-sufficient: it carries its own headers and
# libraries, so `make install' installs the one binary and nothing else.
#
# A wasm host is needed only by `make test', to run what the compiler
# produces.  wasmtime is tried first, as the README's self-hosting
# session uses it; node is accepted instead.  Without either, the
# compile-only checks still run and the run checks are skipped.

CC      ?= cc
CFLAGS  ?= -O2
PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
INSTALL ?= install

SRCS = c.c l.c p.c w.c
HDRS = c.h l.h p.h w.h

all: wcpl

wcpl: $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

install: wcpl
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 wcpl $(DESTDIR)$(BINDIR)/wcpl

clean:
	rm -f wcpl wcpl.wasm wcpl1.wasm tests/*.wasm examples/*.wasm

# `test' does three things, cheapest first:
#
#  1. compiles every tests/*.c and examples/*.c, which exercises the
#     compiler without needing a wasm host at all;
#  2. runs the two test programs, which report their own failure counts;
#  3. checks self-hosting the way the README does -- the compiler builds
#     itself, and the result builds itself again, and the two wasm files
#     must be byte-identical.  That is the strongest check here: a fault
#     that merely perturbs data the program never reads would pass (1)
#     and (2) and still show up as a difference in (3).
test: wcpl
	@set -e; \
	if command -v wasmtime >/dev/null 2>&1; then \
	  RUN="wasmtime --dir=. --"; HOST=wasmtime; \
	elif command -v node >/dev/null 2>&1; then \
	  printf '%s\n' \
	    "import { readFile } from 'node:fs/promises';" \
	    "import { WASI } from 'node:wasi';" \
	    "const wasi = new WASI({ version: 'preview1'," \
	    "  args: ['wcpl', ...process.argv.slice(3)], env: process.env," \
	    "  preopens: { '.': process.cwd() }, returnOnExit: true });" \
	    "const m = await WebAssembly.compile(await readFile(process.argv[2]));" \
	    "const i = await WebAssembly.instantiate(m, wasi.getImportObject());" \
	    "process.exitCode = wasi.start(i);" > .wasirun.mjs; \
	  RUN="node .wasirun.mjs"; HOST=node; \
	else \
	  RUN=""; HOST=""; \
	fi; \
	echo "== compiling tests and examples =="; \
	for f in tests/*.c examples/*.c; do \
	  ./wcpl -q -o "$${f%.c}.wasm" "$$f" || { echo "FAIL to compile $$f"; exit 1; }; \
	  echo "  ok $$f"; \
	done; \
	if [ -z "$$HOST" ]; then \
	  echo "== no wasm host (wasmtime or node): skipping run checks =="; \
	  exit 0; \
	fi; \
	echo "== running tests ($$HOST) =="; \
	for t in tests/*.wasm; do \
	  echo "  $$t: `$$RUN $$t 2>&1 | grep -oE 'FAILURES: [0-9]+' | tail -1`"; \
	done; \
	echo "== self-hosting fixed point ($$HOST) =="; \
	./wcpl -q -o wcpl.wasm $(SRCS); \
	$$RUN wcpl.wasm -q -o wcpl1.wasm $(SRCS); \
	if cmp -s wcpl.wasm wcpl1.wasm; then \
	  echo "  wcpl.wasm and wcpl1.wasm are identical"; \
	else \
	  echo "  FAIL: wcpl.wasm and wcpl1.wasm differ"; exit 1; \
	fi; \
	rm -f .wasirun.mjs

.PHONY: all install clean test
