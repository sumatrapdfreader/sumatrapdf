default:
	@npm install -s
	bash tools/build.sh

memento:
	BUILD=memento \
	bash tools/build.sh

large:
	SUFFIX=-large \
	DEFINES="-DTOFU -DTOFU_CJK_EXT" \
	FEATURES="brotli=no mujs=no extract=no" \
	bash tools/build.sh

small:
	SUFFIX=-small \
	DEFINES="-DTOFU -DTOFU_CJK_EXT" \
	FEATURES="brotli=no mujs=no extract=no xps=no svg=no html=no" \
	bash tools/build.sh

tiny:
	SUFFIX=-tiny \
	DEFINES="-DTOFU -DTOFU_CJK" \
	FEATURES="brotli=no mujs=no extract=no xps=no svg=no html=no" \
	bash tools/build.sh

clean:
	rm -f LICENSE
	rm -rf dist
	rm -f lib/mupdf-wasm.d.ts
	rm -rf ../../build/wasm

nuke: clean
	rm -rf node_modules
	rm -f package-lock.json
