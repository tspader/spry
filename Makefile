COMMON_INC := -Ivendor -Isrc/abi
WASM_INC   := $(COMMON_INC) -Isrc/runtime
NATIVE_INC := $(COMMON_INC) -Isrc/backend -Isrc/host/native
GTK_INC    := $(NATIVE_INC) -Isrc/backend/gtk
STUB_INC   := $(NATIVE_INC) -Isrc/backend/stub

GTK_CFLAGS := $(shell pkg-config --cflags gtk4)
GTK_LIBS   := $(shell pkg-config --libs gtk4)

WASM_CFLAGS  := --target=wasm32-freestanding -nostdlib -fno-sanitize=undefined
WASM_LDFLAGS := -Wl,--no-entry -Wl,--export-dynamic

HEADERS    := $(shell find src vendor -name '*.h')
NATIVE_SRC := src/host/native/wasm_host.c src/backend/gtk/gtk_backend.c example/native/main.c
SMOKE_SRC  := src/host/native/wasm_host.c src/backend/stub/stub_backend.c example/native/smoke.c

WASM_DBFLAGS := --target=wasm32-freestanding -ffreestanding $(WASM_INC)
GTK_DBFLAGS  := $(GTK_INC) $(GTK_CFLAGS)
STUB_DBFLAGS := $(STUB_INC)

.PHONY: all abi wasm web native smoke compdb clean
all: web native compdb
abi: src/abi/abi.gen.ts
wasm: .cache/runtime.wasm
web: build/web/host.bundle.js
native: build/gtk/app .cache/runtime.wasm
smoke: .cache/native-smoke .cache/runtime.wasm
compdb: compile_commands.json

src/abi/abi.gen.ts: src/abi/abi.h tools/gen-abi.ts
	bun run tools/gen-abi.ts

.cache/runtime.wasm: src/runtime/runtime.c $(HEADERS) | .cache
	zig cc $(WASM_CFLAGS) $(WASM_INC) $(WASM_LDFLAGS) -o $@ $<

build/web/host.bundle.js: example/web/main.ts example/web/index.html \
                          src/abi/abi.gen.ts .cache/runtime.wasm tools/bundle-web.ts | build/web
	bun run tools/bundle-web.ts

build/gtk/app: $(NATIVE_SRC) $(HEADERS) | build/gtk
	zig cc $(GTK_CFLAGS) $(GTK_INC) $(NATIVE_SRC) -lwasmer $(GTK_LIBS) -o $@

.cache/native-smoke: $(SMOKE_SRC) $(HEADERS) | .cache
	zig cc $(STUB_INC) $(SMOKE_SRC) -lwasmer -o $@

compile_commands.json: tools/gen-compile-commands.ts Makefile
	bun run tools/gen-compile-commands.ts \
	  "src/runtime/runtime.c::$(WASM_DBFLAGS)" \
	  "src/host/native/wasm_host.c::$(GTK_DBFLAGS)" \
	  "src/backend/gtk/gtk_backend.c::$(GTK_DBFLAGS)" \
	  "example/native/main.c::$(GTK_DBFLAGS)" \
	  "src/backend/stub/stub_backend.c::$(STUB_DBFLAGS)" \
	  "example/native/smoke.c::$(STUB_DBFLAGS)"

.cache build/web build/gtk:
	mkdir -p $@

clean:
	rm -rf build .cache compile_commands.json src/abi/abi.gen.ts
