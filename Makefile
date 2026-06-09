ROOT        := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
HOST_TRIPLE := $(shell uname -m)-linux-gnu
TRIPLE      ?= $(HOST_TRIPLE)

BUILD := $(ROOT)/.build
WORK  := $(BUILD)/work/$(TRIPLE)
STORE := $(BUILD)/store/$(TRIPLE)

.PHONY: all build configure fetch clean
all: build
	@ln -sf $(WORK)/compile_commands.json $(ROOT)/compile_commands.json
	@echo "store: $(STORE)"

build: configure
	@cmake --build $(WORK)

configure: fetch
	@cmake -S $(ROOT) -B $(WORK) -DTRIPLE=$(TRIPLE)

fetch:
	@cmake -P $(ROOT)/cmake/fetch.cmake

clean:
	rm -rf $(BUILD) $(ROOT)/compile_commands.json $(ROOT)/src/abi/abi.gen.ts $(ROOT)/src/abi/ui.gen.h $(ROOT)/src/abi/ui.enums.gen.h $(ROOT)/src/abi/ui.enums.gen.ts $(ROOT)/src/abi/schema.ts
