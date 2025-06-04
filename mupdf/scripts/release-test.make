run-release-test:
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make test-release-build
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make test-no-icc-build
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make test-no-js-build
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make test-sanitize-build
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make test-valgrind-build
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make test-memento-build
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-all-disabled
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make test-examples
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-python-build
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-python-with-tesseract-build
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-cplusplus-build
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-csharp-build
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make test-java-examples
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make test-docs
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make test-java-build

make-release-build:
	$(MAKE) -j2 build=release build/release/mutool

test-release-build: make-release-build pdfref17.pdf
	/usr/bin/test 38b6fd1d44108881f06fe8a260b0c7b3 == $$(./build/release/mutool draw -s5 pdfref17.pdf 1140 2>&1 | grep '^page pdfref17.pdf 1140 ' | cut -d' ' -f4)
	/usr/bin/test a62d97b3506d05bc2dbd3214d6d07113 == $$(./build/release/mutool draw -N -s5 pdfref17.pdf 1140 2>&1 | grep '^page pdfref17.pdf 1140 ' | cut -d' ' -f4)
	/usr/bin/test 3208e5b2e4f7d2ce91e922d697f2be33 == $$(./build/release/mutool draw -s5 pdfref17.pdf N-1 2>&1 | grep -v '^warning: ' | md5sum - | cut -d' ' -f1)
	/usr/bin/test e0a97c8a2003b8d90edda1e45a45dea0 == $$(./build/release/mutool draw -s5 pdfref17.pdf N-1 2>&1 | md5sum - | cut -d' ' -f1)

make-no-icc-build:
	$(MAKE) -j2 XCFLAGS=-DFZ_ENABLE_ICC=0 build=release build/release/mutool

test-no-icc-build: make-no-icc-build pdfref17.pdf
	/usr/bin/test a62d97b3506d05bc2dbd3214d6d07113 == $$(./build/release/mutool draw -s5 pdfref17.pdf 1140 2>&1 | grep '^page pdfref17.pdf 1140 ' | cut -d' ' -f4)
	/usr/bin/test a62d97b3506d05bc2dbd3214d6d07113 == $$(./build/release/mutool draw -N -s5 pdfref17.pdf 1140 2>&1 | grep '^page pdfref17.pdf 1140 ' | cut -d' ' -f4)

make-no-js-build:
	$(MAKE) -j2 XCFLAGS=-DFZ_ENABLE_JS=0 build=release build/release/mutool

test-no-js-build: make-no-js-build pdfref17.pdf
	/usr/bin/test 38b6fd1d44108881f06fe8a260b0c7b3 == $$(./build/release/mutool draw -s5 pdfref17.pdf 1140 2>&1 | grep '^page pdfref17.pdf 1140 ' | cut -d' ' -f4)

make-sanitize-build:
	$(MAKE) -j2 build=sanitize build/sanitize/mutool

test-sanitize-build: make-sanitize-build pdfref17.pdf
	./build/sanitize/mutool draw -st pdfref17.pdf N-1

make-valgrind-build:
	$(MAKE) -j2 build=valgrind build/valgrind/mutool

test-valgrind-build: make-valgrind-build pdfref17.pdf
	./build/valgrind/mutool draw -st pdfref17.pdf N-1

make-memento-build:
	$(MAKE) -j2 build=memento build/memento/mutool

test-memento-build: make-memento-build pdfref17.pdf
	./build/memento/mutool draw -st pdfref17.pdf N-1

make-all-disabled:
	$(MAKE) -j2 XCFLAGS='-DFZ_ENABLE_CBZ=0 -DFZ_ENABLE_DOCX_OUTPUT=0 -DFZ_ENABLE_EPUB=0 -DFZ_ENABLE_FB2=0 -DFZ_ENABLE_HTML=0 -DFZ_ENABLE_HTML_ENGINE=0 -DFZ_ENABLE_ICC=0 -DFZ_ENABLE_IMG=0 -DFZ_ENABLE_JPX=0 -DFZ_ENABLE_JS=0 -DFZ_ENABLE_MOBI=0 -DFZ_ENABLE_OCR_OUTPUT=0 -DFZ_ENABLE_ODT_OUTPUT=0 -DFZ_ENABLE_OFFICE=0 -DFZ_ENABLE_PDF=0 -DFZ_ENABLE_SPOT_RENDERING=0 -DFZ_ENABLE_SVG=0 -DFZ_ENABLE_TXT=0 -DFZ_ENABLE_XPS=0 -DFZ_ENABLE_BROTLI=0'

make-examples:
	$(MAKE) -j2 build=debug
	$(MAKE) -j2 build=debug examples

test-examples: make-examples pdfref17.pdf
	mkdir -p build/examples
	build/debug/example pdfref17.pdf 1 > build/examples/out.pnm
	cd build/examples; ../debug/multi-threaded ../../pdfref17.pdf; zip -0 ../examples.zip out*; cd ../..
	/usr/bin/test df4d542e96c0fbac488adcba6b870077 == $$(./build/debug/mutool draw -Ds5 build/examples.zip 2>&1 | md5sum - | cut -d' ' -f1)

make-python-build:
	$(MAKE) -j2 python

make-python-with-tesseract-build:
	$(MAKE) -j2 tesseract=yes python

make-cplusplus-build:
	$(MAKE) -j2 c++

make-csharp-build:
	$(MAKE) -j2 csharp

make-java-build:
	$(MAKE) -j2 -C platform/java build=release default

test-java-examples: make-java-build platform/java/pdfref17.pdf
	/usr/bin/test 93ecea2a290429260f0fef4107012a51 == $$(MUPDF_ARGS="pdfref17.pdf 1140" $(MAKE) -C platform/java build=release run-example | grep -Ev '^(make|java)' | md5sum - | cut -d' ' -f1)

make-docs:
	$(MAKE) docs

test-docs: make-docs
	linkchecker file://$(PWD)/build/docs/index.html

test-java-build: make-java-build
	MUPDF_ARGS=pdfref17.pdf $(MAKE) -C platform/java build=release run
nuke:
	$(MAKE) nuke
