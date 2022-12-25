-include user.make

build ?= release

EMSDK_DIR ?= /opt/emsdk
BUILD_DIR ?= ../../build/wasm/$(build)
WASM_SKIP_TRY_CATCH ?= 0

SAMPLES := mupdf-sample-file.pdf pdfref13.pdf ECMA-388.xps

ifeq ($(build),debug)
  LIB_BUILD_FLAGS := -Os -g
  BUILD_FLAGS := -Wall -O0 -g
else ifeq ($(build),sanitize)
  LIB_BUILD_FLAGS := -O2 -g -fsanitize=null
  BUILD_FLAGS := -Wall -O0 -g -fsanitize=null -fsanitize=address -fsanitize=leak
else
  LIB_BUILD_FLAGS := -O2
  BUILD_FLAGS := -Wall -Os -flto
endif

all: mupdf-wasm.js mupdf-wasm.wasm mupdf-wasm-singlethread.js mupdf-wasm-singlethread.wasm samples

MUPDF_CORE := $(BUILD_DIR)/libmupdf.a $(BUILD_DIR)/libmupdf-third.a
$(MUPDF_CORE): .FORCE
	$(MAKE) -j4 -C ../.. generate
	BASH_SOURCE=$(EMSDK_DIR)/emsdk_env.sh; \
	. $(EMSDK_DIR)/emsdk_env.sh; \
	$(MAKE) -j4 -C ../.. \
		OS=wasm build=$(build) \
		XCFLAGS='$(LIB_BUILD_FLAGS) -pthread -DTOFU -DTOFU_CJK -DFZ_ENABLE_SVG=0 -DFZ_ENABLE_HTML=0 -DFZ_ENABLE_EPUB=0 -DFZ_ENABLE_JS=0 -DWASM_SKIP_TRY_CATCH=$(WASM_SKIP_TRY_CATCH)' \
		libs

mupdf-wasm-singlethread.js mupdf-wasm-singlethread.wasm: $(MUPDF_CORE) lib/wrap.c
	BASH_SOURCE=$(EMSDK_DIR)/emsdk_env.sh \
	. $(EMSDK_DIR)/emsdk_env.sh; \
	emcc -o $@ $(BUILD_FLAGS) \
		--no-entry \
		-D WASM_SKIP_TRY_CATCH=$(WASM_SKIP_TRY_CATCH) \
		-s VERBOSE=0 \
		-s ABORTING_MALLOC=0 \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s WASM=1 \
		-s MODULARIZE=1 \
		-s EXPORT_NAME='"libmupdf"' \
		-s EXPORTED_RUNTIME_METHODS='["ccall","cwrap", "UTF8ToString","lengthBytesUTF8","stringToUTF8", "wasmMemory"]' \
		-s EXPORTED_FUNCTIONS='["_malloc","_free"]' \
		-I ../../include \
		lib/wrap.c \
		$(BUILD_DIR)/libmupdf.a \
		$(BUILD_DIR)/libmupdf-third.a

mupdf-wasm.js mupdf-wasm.wasm: $(MUPDF_CORE) lib/wrap.c
	BASH_SOURCE=$(EMSDK_DIR)/emsdk_env.sh \
	. $(EMSDK_DIR)/emsdk_env.sh; \
	emcc -o $@ $(BUILD_FLAGS) \
		-pthread \
		--no-entry \
		-D WASM_SKIP_TRY_CATCH=$(WASM_SKIP_TRY_CATCH) \
		-s VERBOSE=0 \
		-s ABORTING_MALLOC=0 \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s WASM=1 \
		-s MODULARIZE=1 \
		-s EXPORT_NAME='"libmupdf"' \
		-s EXPORTED_RUNTIME_METHODS='["ccall","cwrap", "UTF8ToString","lengthBytesUTF8","stringToUTF8"]' \
		-s EXPORTED_FUNCTIONS='["_malloc","_free"]' \
		-I ../../include \
		lib/wrap.c \
		$(BUILD_DIR)/libmupdf.a \
		$(BUILD_DIR)/libmupdf-third.a

samples: $(SAMPLES:%=samples/%)

$(SAMPLES:%=samples/%):
	mkdir -p samples/
	curl 'https://ghostscript.com/~olivier/$@' -o $@

clean:
	rm -f mupdf-wasm.js mupdf-wasm.wasm mupdf-wasm.worker.js mupdf-wasm-with-threads.js mupdf-wasm-with-threads.wasm
	$(MAKE) -C ../../ OS=wasm build=$(build) clean

.PHONY: .FORCE clean
