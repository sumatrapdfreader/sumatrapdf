run-release-test:
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-tags
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-find-try-return
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make test-release-build
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make test-no-icc-build
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make test-no-js-build
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make test-examples
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make test-java-examples
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-apps
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-extra-apps
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make test-docs
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-docs-markdown
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make test-manpages
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-wasm-build
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-python-build
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make test-disable-threads-build
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make test-sanitize-build
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make test-valgrind-build
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make test-memento-build
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-one-disabled-config
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-one-enabled-config
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-all-disabled-defines
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-all-disabled-config
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-all-disabled-one-enabled-config
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-all-enabled-config
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-shared
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-debug
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-small
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-profile
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-coverage
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-gperf
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-native
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-third
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-libs
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-extra-libs
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-gperf
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-python-with-tesseract-build
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-cplusplus-build
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-csharp-build
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make make-wasm-build
	$(MAKE) nuke
	$(MAKE) -f scripts/release-test.make test-java-build

make-tags:
	$(MAKE) -j2 tags

make-find-try-return:
	$(MAKE) -j2 find-try-return

make-release-build:
	$(MAKE) -j2 build=release build/release/mutool

test-release-build: make-release-build pdfref17.pdf
	/usr/bin/test 38b6fd1d44108881f06fe8a260b0c7b3 == $$(./build/release/mutool draw -s5 pdfref17.pdf 1140 2>&1 | grep '^page pdfref17.pdf 1140 ' | cut -d' ' -f4)
	/usr/bin/test a62d97b3506d05bc2dbd3214d6d07113 == $$(./build/release/mutool draw -N -s5 pdfref17.pdf 1140 2>&1 | grep '^page pdfref17.pdf 1140 ' | cut -d' ' -f4)
	/usr/bin/test 208b6e8eec9fcd9d8c3d7df8d2a50241 == $$(./build/release/mutool draw -s5 pdfref17.pdf N-1 2>&1 | grep -v '^warning: ' | md5sum - | cut -d' ' -f1)
	/usr/bin/test 208b6e8eec9fcd9d8c3d7df8d2a50241 == $$(./build/release/mutool draw -s5 pdfref17.pdf N-1 2>&1 | md5sum - | cut -d' ' -f1)

make-no-icc-build:
	$(MAKE) -j2 XCFLAGS=-DFZ_ENABLE_ICC=0 build=release build/release/mutool

test-no-icc-build: make-no-icc-build pdfref17.pdf
	/usr/bin/test a62d97b3506d05bc2dbd3214d6d07113 == $$(./build/release/mutool draw -s5 pdfref17.pdf 1140 2>&1 | grep '^page pdfref17.pdf 1140 ' | cut -d' ' -f4)
	/usr/bin/test a62d97b3506d05bc2dbd3214d6d07113 == $$(./build/release/mutool draw -N -s5 pdfref17.pdf 1140 2>&1 | grep '^page pdfref17.pdf 1140 ' | cut -d' ' -f4)

make-no-js-build:
	$(MAKE) -j2 XCFLAGS=-DFZ_ENABLE_JS=0 build=release build/release/mutool

test-no-js-build: make-no-js-build pdfref17.pdf
	/usr/bin/test 38b6fd1d44108881f06fe8a260b0c7b3 == $$(./build/release/mutool draw -s5 pdfref17.pdf 1140 2>&1 | grep '^page pdfref17.pdf 1140 ' | cut -d' ' -f4)

make-disable-threads-build:
	$(MAKE) -j2 XCFLAGS=-DDISABLE_MUTHREADS build=release build/release/mutool

test-disable-threads-build: make-disable-threads-build pdfref17.pdf
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

make-all-disabled-defines:
	$(MAKE) -j2 XCFLAGS='-DFZ_ENABLE_CBZ=0 -DFZ_ENABLE_DOCX_OUTPUT=0 -DFZ_ENABLE_EPUB=0 -DFZ_ENABLE_FB2=0 -DFZ_ENABLE_HTML=0 -DFZ_ENABLE_MD=0 -DFZ_ENABLE_HTML_ENGINE=0 -DFZ_ENABLE_ICC=0 -DFZ_ENABLE_IMG=0 -DFZ_ENABLE_JPX=0 -DFZ_ENABLE_JS=0 -DFZ_ENABLE_MOBI=0 -DFZ_ENABLE_OCR_OUTPUT=0 -DFZ_ENABLE_ODT_OUTPUT=0 -DFZ_ENABLE_OFFICE=0 -DFZ_ENABLE_PDF=0 -DFZ_ENABLE_SPOT_RENDERING=0 -DFZ_ENABLE_SVG=0 -DFZ_ENABLE_TXT=0 -DFZ_ENABLE_XPS=0 -DFZ_ENABLE_BROTLI=0' build=release

make-all-disabled-config:
	$(MAKE) -j2 build=release brotli=no mujs=no html=no xps=no svg=no extract=no tesseract=no barcode=no archive=no tofu=no tofu_cjk=no tofu_cjk_ext=no tofu_cjk_lang=no

make-all-disabled-one-enabled-config:
	$(MAKE) nuke
	$(MAKE) -j2 build=release brotli=yes mujs=no html=no xps=no svg=no extract=no tesseract=no barcode=no archive=no tofu=no tofu_cjk=no tofu_cjk_ext=no tofu_cjk_lang=no
	$(MAKE) nuke
	$(MAKE) -j2 build=release brotli=no mujs=yes html=no xps=no svg=no extract=no tesseract=no barcode=no archive=no tofu=no tofu_cjk=no tofu_cjk_ext=no tofu_cjk_lang=no
	$(MAKE) nuke
	$(MAKE) -j2 build=release brotli=no mujs=no html=yes xps=no svg=no extract=no tesseract=no barcode=no archive=no tofu=no tofu_cjk=no tofu_cjk_ext=no tofu_cjk_lang=no
	$(MAKE) nuke
	$(MAKE) -j2 build=release brotli=no mujs=no html=no xps=yes svg=no extract=no tesseract=no barcode=no archive=no tofu=no tofu_cjk=no tofu_cjk_ext=no tofu_cjk_lang=no
	$(MAKE) nuke
	$(MAKE) -j2 build=release brotli=no mujs=no html=no xps=no svg=yes extract=no tesseract=no barcode=no archive=no tofu=no tofu_cjk=no tofu_cjk_ext=no tofu_cjk_lang=no
	$(MAKE) nuke
	$(MAKE) -j2 build=release brotli=no mujs=no html=no xps=no svg=no extract=yes tesseract=no barcode=no archive=no tofu=no tofu_cjk=no tofu_cjk_ext=no tofu_cjk_lang=no
	$(MAKE) nuke
	$(MAKE) -j2 build=release brotli=no mujs=no html=no xps=no svg=no extract=no tesseract=yes barcode=no archive=no tofu=no tofu_cjk=no tofu_cjk_ext=no tofu_cjk_lang=no
	$(MAKE) nuke
	$(MAKE) -j2 build=release brotli=no mujs=no html=no xps=no svg=no extract=no tesseract=no barcode=yes archive=no tofu=no tofu_cjk=no tofu_cjk_ext=no tofu_cjk_lang=no
	$(MAKE) nuke
	$(MAKE) -j2 build=release brotli=no mujs=no html=no xps=no svg=no extract=no tesseract=no barcode=no archive=yes tofu=no tofu_cjk=no tofu_cjk_ext=no tofu_cjk_lang=no
	$(MAKE) nuke
	$(MAKE) -j2 build=release brotli=no mujs=no html=no xps=no svg=no extract=no tesseract=no barcode=no archive=no tofu=yes tofu_cjk=no tofu_cjk_ext=no tofu_cjk_lang=no
	$(MAKE) nuke
	$(MAKE) -j2 build=release brotli=no mujs=no html=no xps=no svg=no extract=no tesseract=no barcode=no archive=no tofu=no tofu_cjk=yes tofu_cjk_ext=no tofu_cjk_lang=no
	$(MAKE) nuke
	$(MAKE) -j2 build=release brotli=no mujs=no html=no xps=no svg=no extract=no tesseract=no barcode=no archive=no tofu=no tofu_cjk=no tofu_cjk_ext=yes tofu_cjk_lang=no
	$(MAKE) nuke
	$(MAKE) -j2 build=release brotli=no mujs=no html=no xps=no svg=no extract=no tesseract=no barcode=no archive=no tofu=no tofu_cjk=no tofu_cjk_ext=no tofu_cjk_lang=yes

make-one-disabled-config:
	$(MAKE) nuke
	$(MAKE) -j2 build=release brotli=no
	$(MAKE) nuke
	$(MAKE) -j2 build=release mujs=no
	$(MAKE) nuke
	$(MAKE) -j2 build=release html=no
	$(MAKE) nuke
	$(MAKE) -j2 build=release xps=no
	$(MAKE) nuke
	$(MAKE) -j2 build=release svg=no
	$(MAKE) nuke
	$(MAKE) -j2 build=release extract=no

make-one-enabled-config:
	$(MAKE) nuke
	$(MAKE) -j2 build=release tesseract=yes
	$(MAKE) nuke
	$(MAKE) -j2 build=release barcode=yes
	$(MAKE) nuke
	$(MAKE) -j2 build=release tofu=yes
	$(MAKE) nuke
	$(MAKE) -j2 build=release tofu_cjk=yes
	$(MAKE) nuke
	$(MAKE) -j2 build=release tofu_cjk_ext=yes
	$(MAKE) nuke
	$(MAKE) -j2 build=release tofu_cjk_lang=yes

make-all-enabled-config:
	$(MAKE) -j2 build=release brotli=yes mujs=yes html=yes xps=yes svg=yes extract=yes tesseract=yes barcode=yes archive=yes tofu=yes tofu_cjk=yes tofu_cjk_ext=yes tofu_cjk_lang=yes

make-shared:
	$(MAKE) -j2 build=release shared

make-debug:
	$(MAKE) -j2 build=debug

make-small:
	$(MAKE) -j2 build=small

make-profile:
	$(MAKE) -j2 build=profile

make-coverage:
	$(MAKE) -j2 build=coverage

make-gperf:
	$(MAKE) -j2 build=gperf

make-native:
	$(MAKE) -j2 build=native

make-third:
	$(MAKE) -j2 build=release third

make-libs:
	$(MAKE) -j2 build=release libs

make-extra-libs:
	$(MAKE) -j2 build=release extra-libs

make-apps:
	$(MAKE) -j2 build=release apps

make-extra-apps:
	$(MAKE) -j2 build=release extra-apps

make-examples:
	$(MAKE) -j2 build=release
	$(MAKE) -j2 build=release examples

test-examples: make-examples pdfref17.pdf
	mkdir -p build/examples
	build/release/example pdfref17.pdf 1 > build/examples/out.pnm
	cd build/examples; ../release/multi-threaded ../../pdfref17.pdf; zip -0 ../examples.zip out*; cd ../..
	/usr/bin/test 575cf1e8383db6c82b7a9a4496c698bc == $$(./build/release/mutool draw -Ds5 build/examples.zip 2>&1 | md5sum - | cut -d' ' -f1)
	cd build/examples; ../release/storytest; cd ../..
	/usr/bin/test 16ee5292a566ef337f663798603e6d82 == $$(./build/release/mutool draw -Ds5 build/examples/leftright.pdf 2>&1 | md5sum - | cut -d' ' -f1)
	/usr/bin/test fb0056830bf6670f675a0d8068016c52 == $$(./build/release/mutool draw -Ds5 build/examples/out2.pdf 2>&1 | md5sum - | cut -d' ' -f1)
	/usr/bin/test 7904ed25c086adced8280f94f3366120 == $$(./build/release/mutool draw -Ds5 build/examples/out3.pdf 2>&1 | md5sum - | cut -d' ' -f1)
	/usr/bin/test c1bd90c127a6758168280b1eddd25462 == $$(./build/release/mutool draw -Ds5 build/examples/out.pdf 2>&1 | md5sum - | cut -d' ' -f1)
	/usr/bin/test c88e11423ca72c9751f716456dd36e84 == $$(./build/release/mutool draw -Ds5 build/examples/out_toc.pdf 2>&1 | md5sum - | cut -d' ' -f1)
	/usr/bin/test 963dd1503dd1eb93d84052fa56b155df == $$(./build/release/mutool draw -Ds5 build/examples/pos_absolute.pdf 2>&1 | md5sum - | cut -d' ' -f1)
	/usr/bin/test 428ec7e05b35b7b8566f7c0c9f913ef4 == $$(./build/release/mutool draw -Ds5 build/examples/pos_absolute_sizes.pdf 2>&1 | md5sum - | cut -d' ' -f1)
	/usr/bin/test 7a351e2e067b31bffc540bb0265cb190 == $$(./build/release/mutool draw -Ds5 build/examples/pos_fixed.pdf 2>&1 | md5sum - | cut -d' ' -f1)
	/usr/bin/test 41bd070f0bb2f262faff55b841e85795 == $$(./build/release/mutool draw -Ds5 build/examples/pos_fixed_sizes.pdf 2>&1 | md5sum - | cut -d' ' -f1)
	/usr/bin/test 1e07e6b307dbab62b3a684288f587ef0 == $$(./build/release/mutool draw -Ds5 build/examples/pos_relative.pdf 2>&1 | md5sum - | cut -d' ' -f1)
	/usr/bin/test cb1dcd8beaafabc2d51aad6d1dde07e5 == $$(./build/release/mutool draw -Ds5 build/examples/pos_static.pdf 2>&1 | md5sum - | cut -d' ' -f1)
	/usr/bin/test f546ac2bce80445818cbe8c5cd954147 == $$(./build/release/mutool draw -Ds5 build/examples/tablebordercollapse.pdf 2>&1 | md5sum - | cut -d' ' -f1)
	/usr/bin/test 812191fbbd7ce8bbe814e85ff2bbef86 == $$(./build/release/mutool draw -Ds5 build/examples/tableborders.pdf 2>&1 | md5sum - | cut -d' ' -f1)
	/usr/bin/test 74f0236ff7ca9bd51f380e455623ccf8 == $$(./build/release/mutool draw -Ds5 build/examples/tableborderwidths.pdf 2>&1 | md5sum - | cut -d' ' -f1)
	/usr/bin/test 2867732089375db03f2c3e6f129c635b == $$(./build/release/mutool draw -Ds5 build/examples/tablespan.pdf 2>&1 | md5sum - | cut -d' ' -f1)
	/usr/bin/test b7442e961aff29d7d11bc9f35d67fd4f == $$(./build/release/mutool draw -Ds5 build/examples/tables.pdf 2>&1 | md5sum - | cut -d' ' -f1)
	/usr/bin/test 5c17970f54c4a7ad1ff35b2160cc2238 == $$(./build/release/mutool draw -Ds5 build/examples/tablewidths.pdf 2>&1 | md5sum - | cut -d' ' -f1)
	/usr/bin/test d12a8b9cda4634aad371af5db4883a11 == $$(./build/release/searchtest 2>&1 | md5sum - | cut -d' ' -f1)

make-python-build:
	$(MAKE) -j2 python

make-python-with-tesseract-build:
	$(MAKE) -j2 tesseract=yes python

make-cplusplus-build:
	$(MAKE) -j2 c++

make-csharp-build:
	$(MAKE) -j2 csharp

make-wasm-build:
	$(MAKE) -j2 wasm

make-java-build:
	$(MAKE) -j2 -C platform/java build=release default

test-java-examples: make-java-build platform/java/pdfref17.pdf
	/usr/bin/test 93ecea2a290429260f0fef4107012a51 == $$(MUPDF_ARGS="pdfref17.pdf 1140" $(MAKE) -C platform/java build=release run-example | grep -Ev '^(make|java)' | md5sum - | cut -d' ' -f1)

make-docs:
	$(MAKE) docs

test-docs: make-docs
	codespell --ignore-words-list flate,Oce,thirdparty,worl,FitH \
		docs/bookbook docs/guide docs/other docs/reference docs/tools
	linkchecker --ignore-url doxygen file://$(PWD)/build/docs/index.html

make-docs-markdown:
	$(MAKE) docs-markdown

test-manpages:
	man --warnings -E UTF-8 -l -Tutf8 -Z docs/man/mupdf.1 1> /dev/null
	mandoc -T lint -K utf-8 docs/man/mupdf.1
	grep -Hn '^$$' docs/man/mutool.1 | sed -e 's/^/empty line:/'
	codespell --ignore-words-list flate,Oce docs/man/mupdf.1
	man --warnings -E UTF-8 -l -Tutf8 -Z docs/man/mutool.1 1> /dev/null
	mandoc -T lint -K utf-8 docs/man/mutool.1
	grep -Hn '^$$' docs/man/mupdf.1 | sed -e 's/^/empty line:/'
	codespell --ignore-words-list flate,Oce docs/man/mutool.1

test-java-build: make-java-build
	MUPDF_ARGS=pdfref17.pdf $(MAKE) -C platform/java build=release run

nuke:
	$(MAKE) nuke
