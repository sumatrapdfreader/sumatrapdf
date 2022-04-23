-include ../../user.make

ifndef build
  build := release
endif

ifeq ($(build),debug)
  BUILD_FLAGS := \
	-Wall -O0 \
	-s VERBOSE=0 \
	-s ABORTING_MALLOC=0 \
	-s ALLOW_MEMORY_GROWTH=1
else
  BUILD_FLAGS := \
	-Wall -Os \
	-s VERBOSE=0 \
	-s ABORTING_MALLOC=0 \
	-s ALLOW_MEMORY_GROWTH=1
endif

ifndef BUILD_DIR
  BUILD_DIR := ../../build/wasm/$(build)
endif

ifndef EMSDK_DIR
  EMSDK_DIR := /opt/emsdk
endif

ifndef SAMPLE_PDF
  SAMPLE_PDF := pdfref13.pdf
endif

MUPDF_JS := libmupdf.js
MUPDF_WASM := libmupdf.wasm


all: $(MUPDF_JS)

MUPDF_CORE := $(BUILD_DIR)/libmupdf.a $(BUILD_DIR)/libmupdf-third.a
$(MUPDF_CORE): .FORCE
	$(MAKE) -j4 -C ../.. generate
	BASH_SOURCE=$(EMSDK_DIR)/emsdk_env.sh; \
	. $(EMSDK_DIR)/emsdk_env.sh; \
	$(MAKE) -j4 -C ../.. \
		OS=wasm build=$(build) \
		XCFLAGS='-DTOFU -DTOFU_CJK -DFZ_ENABLE_SVG=0 -DFZ_ENABLE_HTML=0 -DFZ_ENABLE_EPUB=0 -DFZ_ENABLE_JS=0' \
		libs

wasm: $(MUPDF_JS) $(MUPDF_WASM)
$(MUPDF_JS) $(MUPDF_WASM): $(MUPDF_CORE) wrap.c wrap.js
	BASH_SOURCE=$(EMSDK_DIR)/emsdk_env.sh; . $(EMSDK_DIR)/emsdk_env.sh; \
	emcc -o $@ $(BUILD_FLAGS) \
		-s WASM=1 \
		-s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
		-s EXPORTED_FUNCTIONS='["_malloc","_free"]' \
		-I ../../include \
		--pre-js wrap.js \
		wrap.c \
		$(BUILD_DIR)/libmupdf.a \
		$(BUILD_DIR)/libmupdf-third.a

run: $(SAMPLE_PDF) $(MUPDF_JS) $(MUDPF_WASM)
	python3 -m http.server 8000
	sleep 3
	xdg-open http://127.0.0.1:8000/view.html?file=$(SAMPLE_PDF) &

clean:
	rm -f $(MUPDF_JS) $(MUPDF_WASM)
	$(MAKE) -C ../../ OS=wasm build=$(build) clean

.PHONY: .FORCE clean
