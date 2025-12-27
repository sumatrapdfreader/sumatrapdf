default:
	@npm install -s
	bash tools/build.sh

memento:
	BUILD=memento \
	bash tools/build.sh

clean:
	rm -f LICENSE
	rm -rf dist
	rm -f lib/mupdf-wasm.d.ts
	rm -rf ../../build/wasm

nuke: clean
	rm -rf node_modules
	rm -f package-lock.json
