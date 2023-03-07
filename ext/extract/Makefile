# Example commands:
#
#   make
#   make test
#       Runs all tests.
#
#   make test-exe
#       Runs exe regression tests. These use $(gs) and $(mutool) to generate
#       intermediate data about pdf content, then uses $(exe) to convert to
#       docx.
#
#   make test-mutool
#       Runs mutool regression tests. This uses $(mutool) to convert directly
#       from pdf to docx. We require that $(mutool) was built with extract=yes.
#
#   make test-gs
#       Runs gs regression tests. This uses $(gs) to convert directly from pdf
#       to docx. We require that $(gs) was built with --with-extract-dir=... We
#       also do a simple test of output-file-per-page.
#
#   make test-tables
#       Tests handling of tables, using mutool with docx device's html output.
#
#   make test-buffer test-misc test-src
#       Runs unit tests etc.
#
#   make build=debug-opt ...
#       Set build flags.
#
#   make build=memento msqueeze
#       Run memento squeeze test.
#
#   Assuming we are in mupdf/thirdparty/extract, and there is a ghostpdl at
#   the same level as mupdf, with a softlink from ghostpdl to extract:
#
#   make test-rebuild-dependent-binaries
#       Clean/Configure/Rebuild the required mupdf and gs binaries.
#
#   make test-build-dependent-binaries
#       Build the required mupdf and gs binaries.


# Build flags.
#
# Note that OpenBSD's clang-8 appears to ignore -Wdeclaration-after-statement.
#
build = debug

flags_link      = -W -Wall -lm
flags_compile   = -W -Wall -Wextra -Wpointer-sign -Wmissing-declarations -Wmissing-prototypes -Wdeclaration-after-statement -Wpointer-arith -Wconversion -Wno-sign-conversion -Werror -MMD -MP -Iinclude -Isrc

uname = $(shell uname)

ifeq ($(build),)
    $(error Need to specify build=debug|opt|debug-opt|memento)
else ifeq ($(build),debug)
    flags_link      += -g
    flags_compile   += -g
else ifeq ($(build),opt)
    flags_link      += -O2
    flags_compile   += -O2
else ifeq ($(build),debug-opt)
    flags_link      += -g -O2
    flags_compile   += -g -O2
else ifeq ($(build),memento)
    flags_link      += -g -dl
    ifeq ($(uname),OpenBSD)
        flags_link += -l execinfo
    endif
    flags_compile   += -g -D MEMENTO
else
    $(error unrecognised $$(build)=$(build))
endif

regenerate ?= no
ifeq ($(regenerate),yes)
    DIFF_OR_CP = rm -rf $(word 2,%$^); cp -r
else
    DIFF_OR_CP = diff -ru
endif

gdb = gdb
ifeq ($(uname),OpenBSD)
    flags_link += -L /usr/local/lib -l execinfo
    $(warning have added -L /usr/local/lib)
    gdb = egdb
    # For some reason OpenBSD's gmake defaults CXX to g++, which is not helpful.
    CXX = c++
endif

# Locations of mutool and gs. By default we assume these are not available.
#
# If this extract checkout is within a mupdf tree (typically as a git
# submodule) we assume ghostpdl is checked out nearby and both mutool gs and gs
# binaries are available and built with extract enabled.
#
# Disable this by running: make we_are_mupdf_thirdparty= ...
#
we_are_mupdf_thirdparty = $(findstring /mupdf/thirdparty/extract, $(abspath .))
ifneq ($(we_are_mupdf_thirdparty),)
    $(warning we are mupdf thirdparty)
    ifeq ($(build),memento)
        mutool := ../../build/memento/mutool
    else
        mutool := ../../build/debug/mutool
    endif
    gs := ../../../ghostpdl/debug-extract-bin/gs
    libbacktrace = ../../../libbacktrace/.libs
endif

# If mutool/gs are specified, they must exist.
#
ifneq ($(mutool),)
ifeq ($(wildcard $(mutool)),)
    $(error mutool does not exist: $(mutool))
endif
$(warning mutool=$(mutool))
endif

ifeq ($(build),memento)
    mutool_run := MEMENTO_ABORT_ON_LEAK=1 $(mutool)
else
    mutool_run := $(mutool)
endif

ifneq ($(gs),)
ifeq ($(wildcard $(gs)),)
    $(error gs does not exist: $(gs))
endif
$(warning gs=$(gs))
endif


# Default target - run all tests.
#
test: test-buffer test-misc test-src test-exe test-mutool test-gs test-html test-tables
	@echo $@: passed

# Define the main test targets.
#
# test/Python2clipped.pdf is same as test/Python2.pdf except it as a modified
# MediaBox that excludes some glyphs.
#
pdfs = test/Python2.pdf test/Python2clipped.pdf test/zlib.3.pdf test/text_graphic_image.pdf
pdfs_generated = $(patsubst test/%, test/generated/%, $(pdfs))

tests_exe := \
        $(patsubst %, %.extract.docx,                   $(tests_exe)) \
        $(patsubst %, %.extract-rotate.docx,            $(tests_exe)) \
        $(patsubst %, %.extract-rotate-spacing.docx,    $(tests_exe)) \
        $(patsubst %, %.extract-autosplit.docx,         $(tests_exe)) \
        $(patsubst %, %.extract-template.docx,          $(tests_exe)) \

tests_exe := $(patsubst %, %.diff, $(tests_exe))

ifneq ($(mutool),)
# Targets that test direct conversion with mutool.
#
    tests_mutool := \
            $(patsubst %, %.mutool.docx.dir.diff, $(pdfs_generated)) \
            $(patsubst %, %.mutool-norotate.docx.dir.diff, $(pdfs_generated)) \
            $(patsubst %, %.mutool.odt.dir.diff, $(pdfs_generated)) \
            $(patsubst %, %.mutool.text.diff, $(pdfs_generated)) \

    tests_mutool_odt := \
            $(patsubst %, %.mutool.odt.diff, $(pdfs_generated)) \

    tests_mutool_text := \
            $(patsubst %, %.mutool.text.diff, $(pdfs_generated)) \

    tests_html := test/generated/table.pdf.mutool.html.diff
endif
ifneq ($(gs),)
# Targets that test direct conversion with gs.
#
    tests_gs := \
            $(patsubst %, %.gs.docx.dir.diff, $(pdfs_generated)) \
            test_gs_fpp

    # We don't yet do clipping with gs so exclude Python2clipped.pdf.*:
    tests_gs := $(filter-out test/generated/Python2clipped.pdf.%, $(tests_gs))

    #$(warning tests_gs: $(tests_gs))
endif
#$(warning $(tests))

test-exe: $(tests_exe)
	@echo $@: passed

# Checks output of mutool conversion from .pdf to .docx/.odt.
#
test-mutool: $(tests_mutool)
	@echo $@: passed

# Checks output of mutool conversion from .pdf to .odt.
#
test-mutool-odt: $(tests_mutool_odt)
	@echo $@: passed

# Checks output of mutool conversion from .pdf to .text.
#
test-mutool-text: $(tests_mutool_text)
	@echo $@: passed

# Checks output of gs conversion from .pdf to .docx. Requires that gs was built
# with extract as a third-party library. As of 2021-02-10 this requires, for
# example ghostpdl/extract being a link to an extract checkout and configuring
# with --with-extract-dir=extract.
#
test-gs: $(tests_gs)
	@echo $@: passed

# Check behaviour of gs when writing file-per-page.
#
test_gs_fpp: $(gs)
	@echo
	@echo == Testing gs file-page-page
	rm test/generated/text_graphic_image.pdf.gs.*.docx || true
	$(gs) -sDEVICE=docxwrite -o test/generated/Python2.pdf.gs.%i.docx test/Python2.pdf
	rm test/generated/text_graphic_image.pdf.gs.*.docx || true
	$(gs) -sDEVICE=docxwrite -o test/generated/zlib.3.pdf.gs.%i.docx test/zlib.3.pdf
	rm test/generated/text_graphic_image.pdf.gs.*.docx || true
	$(gs) -sDEVICE=docxwrite -o test/generated/text_graphic_image.pdf.gs.%i.docx test/text_graphic_image.pdf
	@echo Checking for correct number of generated files.
	ls -l test/generated/*.pdf.gs.*.docx
	ls test/generated/text_graphic_image.pdf.gs.*.docx | wc -l | grep '^ *1$$'
	ls test/generated/Python2.pdf.gs.*.docx | wc -l | grep '^ *1$$'
	ls test/generated/zlib.3.pdf.gs.*.docx | wc -l | grep '^ *2$$'


test-html: $(tests_html)

test-rebuild-dependent-binaries:
	@echo == Rebuilding gs and mupdf binaries
	cd ../../../ghostpdl && ./autogen.sh --with-extract-dir=extract && make -j 8 debugclean DEBUGDIRPREFIX=debug-extract- && make -j 8 debug DEBUGDIRPREFIX=debug-extract-
	cd ../.. && make -j 8 build=debug clean && make -j 8 build=debug

test-build-dependent-binaries:
	@echo == Building gs and mupdf binaries
	cd ../../../ghostpdl && make -j 8 debug DEBUGDIRPREFIX=debug-extract-
	cd ../.. && make -j 8 build=debug


ifneq ($(mutool),)
    test_tables_pdfs = \
            test/agstat.pdf \
            test/background_lines_1.pdf \
            test/background_lines_2.pdf \
            test/column_span_1.pdf \
            test/column_span_2.pdf \
            test/electoral_roll.pdf \
            test/rotated.pdf \
            test/row_span.pdf \
            test/table.pdf \
            test/twotables_1.pdf \
            test/twotables_2.pdf \

    test_tables_generated = $(patsubst test/%, test/generated/%, $(test_tables_pdfs))

    test_tables_html    = $(patsubst test/%.pdf, test/generated/%.pdf.mutool.html.diff, $(test_tables_pdfs))
    test_tables_docx    = $(patsubst test/%.pdf, test/generated/%.pdf.mutool.docx.dir.diff, $(test_tables_pdfs))
    test_tables_odt     = $(patsubst test/%.pdf, test/generated/%.pdf.mutool.odt.dir.diff,  $(test_tables_pdfs))

    test_tables = $(test_tables_html) $(test_tables_docx) $(test_tables_odt)
endif

test-tables-html:       $(test_tables_html)
test-tables-docx:       $(test_tables_docx)
test-tables-odt:        $(test_tables_odt)

test-tables:            $(test_tables)
	@echo $@: passed

test/generated/%.pdf.mutool.html.diff: test/generated/%.pdf.mutool.html test/%.pdf.mutool.html.ref
	@echo
	@echo == Checking $<
	$(DIFF_OR_CP) $^

test/generated/%.pdf.mutool.cv.html.diff: test/generated/%.pdf.mutool.cv.html test/%.pdf.mutool.html.ref
	@echo
	@echo == Checking $<
	$(DIFF_OR_CP) $^

test/generated/%.pdf.mutool.cv.html: test/%.pdf $(mutool)
	$(mutool) convert -O resolution=300 -o $<..png $<
	EXTRACT_OPENCV_IMAGE_BASE=$< $(mutool_run) convert -F docx -O html -o $@ $<

test/generated/%.pdf.mutool.text.diff: test/generated/%.pdf.mutool.text test/%.pdf.mutool.text.ref
	@echo
	@echo == Checking $<
	$(DIFF_OR_CP) $^


# Main executable.
#
exe = src/build/extract-$(build).exe
exe_src = \
        src/alloc.c \
        src/astring.c \
        src/boxer.c \
        src/buffer.c \
        src/document.c \
        src/docx.c \
        src/docx_template.c \
        src/extract-exe.c \
        src/extract.c \
        src/html.c \
        src/join.c \
        src/mem.c \
        src/odt.c \
        src/odt_template.c \
        src/outf.c \
        src/rect.c \
        src/sys.c \
        src/text.c \
        src/xml.c \
        src/zip.c \


ifeq ($(build),memento)
    exe_src += src/memento.c
    ifeq ($(uname),Linux)
        flags_compile += -D HAVE_LIBDL
        flags_link += -L $(libbacktrace) -l backtrace -l dl
    endif
endif
exe_obj := $(exe_src)
exe_obj := $(patsubst src/%.c, src/build/%.c-$(build).o, $(exe_obj))
exe_obj := $(patsubst src/%.cpp, src/build/%.cpp-$(build).o, $(exe_obj))
exe_dep = $(exe_obj:.o=.d)
exe: $(exe)
$(exe): $(exe_obj)
	$(CXX) $(flags_link) -o $@ $^ -lz -lm

run_exe = $(exe)
ifeq ($(build),memento)
    ifeq ($(uname),Linux)
        run_exe = MEMENTO_ABORT_ON_LEAK=1 MEMENTO_HIDE_MULTIPLE_REALLOCS=1 LD_LIBRARY_PATH=$(libbacktrace) $(exe)
        #run_exe = LD_LIBRARY_PATH=../libbacktrace/.libs $(exe)
    endif
    ifeq ($(uname),OpenBSD)
        run_exe = MEMENTO_ABORT_ON_LEAK=1 MEMENTO_HIDE_MULTIPLE_REALLOCS=1 $(exe)
    endif
endif

exe_tables = src/build/extract-tables-$(build).exe
exe-tables: $(exe_tables)
exe-tables-test: $(exe_tables)
	$< test/agstat.pdf

ifeq (0,1)
# Do not commit changes to above line.
#
# Special rules for populating .ref directories with current output. Useful to
# initialise references outputs for new output type.
#
test/%.docx.dir.ref/: test/generated/%.docx.dir/
	rsync -ai $< $@
test/%.odt.dir.ref/: test/generated/%.odt.dir/
	rsync -ai $< $@
test/%.text.ref: test/generated/%.text
	rsync -ai $< $@

_update_tables_leafs = $(patsubst test/%, %, $(test_tables_pdfs))
# Update all table docx reference outputs.
#
_update-docx-tables:
	for i in $(_update_tables_leafs); do rsync -ai test/generated/$$i.mutool.docx.dir/ test/$$i.mutool.docx.dir.ref/; done
# Update all table odt reference outputs.
#
_update-odt-tables:
	for i in $(_update_tables_leafs); do rsync -ai test/generated/$$i.mutool.odt.dir/ test/$$i.mutool.odt.dir.ref/; done
endif

# Rules that make the various intermediate targets required by $(tests).
#

%.extract.docx: % $(exe)
	@echo
	@echo == Generating docx with extract.exe
	$(run_exe) -r 0 -i $< -f docx -o $@

%.extract.odt: % $(exe)
	@echo
	@echo == Generating odt with extract.exe
	$(run_exe) -r 0 -i $< -f odt -o $@

%.extract-rotate.docx: % $(exe) Makefile
	@echo
	@echo == Generating docx with rotation with extract.exe
	$(run_exe) -r 1 -s 0 -i $< -f docx -o $@

%.extract-rotate-spacing.docx: % $(exe) Makefile
	@echo
	@echo == Generating docx with rotation with extract.exe
	$(run_exe) -r 1 -s 1 -i $< -f docx -o $@

%.extract-autosplit.docx: % $(exe)
	@echo
	@echo == Generating docx with autosplit with extract.exe
	$(run_exe) -r 0 -i $< -f docx --autosplit 1 -o $@

%.extract-template.docx: % $(exe)
	@echo
	@echo == Generating docx using src/template.docx with extract.exe
	$(run_exe) -r 0 -i $< -f docx -t src/template.docx -o $@

test/generated/%.dir.diff: test/generated/%.dir test/%.dir.ref
	@echo
	@echo == Checking $<
	$(DIFF_OR_CP) $^
#if diff -ruq $^; then true; else echo "@@@ failure... fix with: rsync -ai" $^; false; fi

test/generated/%.html.diff: test/generated/%.html test/%.html.ref
	@echo
	@echo == Checking $<
	$(DIFF_OR_CP) $^

# This checks that -t src/template.docx gives identical results.
#
test/generated/%.extract-template.docx.diff: test/generated/%.extract-template.docx.dir test/%.extract.docx.dir.ref
	@echo
	@echo == Checking $<
	$(DIFF_OR_CP) $^

# Unzips .docx into .docx.dir/ directory, and prettyfies the .xml files.
%.docx.dir: %.docx .ALWAYS
	@echo
	@echo == Extracting .docx into directory.
	@rm -r $@ 2>/dev/null || true
	unzip -q -d $@ $<

# Unzips .odt into .odt.dir/ directory, and prettyfies the .xml files.
%.odt.dir: %.odt
	@echo
	@echo == Extracting .odt into directory.
	@rm -r $@ 2>/dev/null || true
	unzip -q -d $@ $<

%.xml.pretty.xml: %.xml
	xmllint --format $< > $@

# Uses zip to create .docx file by zipping up a directory. Useful to recreate
# .docx from reference directory test/*.docx.dir.ref.
%.docx: %
	@echo
	@echo == Zipping directory into .docx file.
	@rm -r $@ 2>/dev/null || true
	cd $< && zip -r ../$(notdir $@) .

# Uses zip to create .odt file by zipping up a directory. Useful to recreate
# .docx from reference directory test/*.odt.dir.ref.
%.odt: %
	@echo
	@echo == Zipping directory into .odt file.
	@rm -r $@ 2>/dev/null || true
	cd $< && zip -r ../$(notdir $@) .

# Prettifies each .xml file within .docx.dir/ directory.
%.docx.dir.pretty: %.docx.dir/
	@rm -r $@ $@- 2>/dev/null || true
	cp -pr $< $@-
	./src/docx_template_build.py --pretty $@-
	mv $@- $@

# Converts .pdf directly to .docx using mutool.
test/generated/%.pdf.mutool.docx: test/%.pdf $(mutool)
	@echo
	@echo == Converting .pdf directly to .docx using mutool.
	@mkdir -p test/generated
	$(mutool_run) convert -O mediabox-clip=yes -o $@ $<

test/generated/%.pdf.mutool-norotate.docx: test/%.pdf $(mutool)
	@echo
	@echo == Converting .pdf directly to .docx using mutool.
	@mkdir -p test/generated
	$(mutool_run) convert -O mediabox-clip=yes,rotation=no -o $@ $<

test/generated/%.pdf.mutool-spacing.docx: test/%.pdf $(mutool)
	@echo
	@echo == Converting .pdf directly to .docx using mutool.
	@mkdir -p test/generated
	$(mutool_run) convert -O mediabox-clip=yes,spacing=yes -o $@ $<

# Converts .pdf directly to .docx using gs.
test/generated/%.pdf.gs.docx: test/%.pdf $(gs)
	@echo
	@echo == Converting .pdf directly to .docx using gs.
	@mkdir -p test/generated
	$(gs) -sDEVICE=docxwrite -o $@ $<

# Converts .pdf directly to .odt using mutool.
test/generated/%.pdf.mutool.odt: test/%.pdf $(mutool)
	@echo
	@echo == Converting .pdf directly to .odt using mutool.
	@mkdir -p test/generated
	$(mutool_run) convert -O mediabox-clip=no -o $@ $<

# Converts .pdf directly to .html using mutool
test/generated/%.pdf.mutool.html: test/%.pdf $(mutool)
	@echo
	@echo == Converting .pdf directly to .html using mutool.
	@mkdir -p test/generated
	$(mutool_run) convert -F docx -O html -o $@ $<

# Converts .pdf directly to .text using mutool
test/generated/%.pdf.mutool.text: test/%.pdf $(mutool)
	@echo
	@echo == Converting .pdf directly to .text using mutool.
	@mkdir -p test/generated
	$(mutool_run) convert -F docx -O text -o $@ $<

# Valgrind test
#
#valgrind: $(exe) test/generated/Python2.pdf.intermediate-mu.xml
#	valgrind --leak-check=full $(exe) -h -r 1 -s 0 -i test/generated/Python2.pdf.intermediate-mu.xml -o test/generated/valgrind-out.docx
#	@echo $@: passed

# Memento tests.
#
ifeq ($(build),memento)
mutool_memento_extract = ../../build/memento/mutool
memento_failat_gdb := $(gdb) -ex 'b Memento_breakpoint' -ex r -ex c -ex bt --args

# Memento squeeze with test/text_graphic_image.pdf runs quickly - just 2,100 events taking 20s.
#
# test/Python2.pdf is much slower - 301,900 events, taking around 8h.
#
msqueeze-mutool-docx:
	MEMENTO_SQUEEZEAT=1 ./src/memento.py -q 100 $(mutool_run) convert -o $@.docx test/text_graphic_image.pdf
msqueeze-mutool-docx-failat:
	MEMENTO_FAILAT=1960 $(memento_failat_gdb) $(mutool) convert -o $@.docx test/text_graphic_image.pdf
msqueeze-mutool-odt:
	MEMENTO_SQUEEZEAT=1 ./src/memento.py -q 100 $(mutool_run) convert -o $@.docx test/text_graphic_image.pdf
msqueeze-mutool-odt2:
	MEMENTO_SQUEEZEAT=4000 ./src/memento.py -q 100 $(mutool_run) convert -o $@.docx test/Python2.pdf
msqueeze-mutool-table:
	MEMENTO_SQUEEZEAT=1 ./src/memento.py -q 100 $(mutool_run) convert -F docx -O html -o $@.html test/agstat.pdf
msqueeze-mutool-table-docx:
	MEMENTO_SQUEEZEAT=1 ./src/memento.py -q 100 $(mutool_run) convert -o $@.docx test/agstat.pdf
msqueeze-mutool-table-odt:
	MEMENTO_SQUEEZEAT=1 ./src/memento.py -q 100 $(mutool_run) convert -o $@.odt test/agstat.pdf
msqueeze-mutool-table-failat:
	MEMENTO_FAILAT=296643 MEMENTO_HIDE_MULTIPLE_REALLOCS=1 $(gdb) -ex 'b Memento_breakpoint' -ex r -ex c -ex bt --args $(mutool_memento_extract) convert -F docx -O html -o $@.html test/agstat.pdf
endif


# Temporary rules for generating reference files.
#
#test/%.xml.extract-rotate-spacing.docx.dir.ref: test/generated/%.xml.extract-rotate-spacing.docx.dir
#	@echo
#	@echo copying $< to %@
#	rsync -ai $</ $@/


# Buffer unit test.
#
exe_buffer_test = src/build/buffer-test-$(build).exe
exe_buffer_test_src = src/buffer.c src/buffer-test.c src/outf.c src/alloc.c src/mem.c
ifeq ($(build),memento)
    exe_buffer_test_src += src/memento.c
endif
exe_buffer_test_obj = $(patsubst src/%.c, src/build/%.c-$(build).o, $(exe_buffer_test_src))
exe_buffer_test_dep = $(exe_buffer_test_obj:.o=.d)
$(exe_buffer_test): $(exe_buffer_test_obj)
	$(CC) $(flags_link) -o $@ $^
test-buffer: $(exe_buffer_test)
	@echo
	@echo == Running test-buffer
	mkdir -p test/generated
	./$<
	@echo $@: passed
test-buffer-valgrind: $(exe_buffer_test)
	@echo
	@echo == Running test-buffer with valgrind
	mkdir -p test/generated
	valgrind --leak-check=full ./$<
	@echo $@: passed

ifeq ($(build),memento)
test-buffer-msqueeze: $(exe_buffer_test)
	MEMENTO_SQUEEZEAT=1 ./src/memento.py -q 1 ./$<
endif

# Misc unit test.
#
exe_misc_test = src/build/misc-test-$(build).exe
exe_misc_test_src = \
        src/alloc.c \
        src/astring.c \
        src/buffer.c \
        src/mem.c \
        src/misc-test.c \
        src/outf.c \
        src/xml.c \

ifeq ($(build),memento)
    exe_misc_test_src += src/memento.c
endif
exe_misc_test_obj = $(patsubst src/%.c, src/build/%.c-$(build).o, $(exe_misc_test_src))
exe_misc_test_dep = $(exe_buffer_test_obj:.o=.d)
$(exe_misc_test): $(exe_misc_test_obj)
	$(CC) $(flags_link) -o $@ $^
test-misc: $(exe_misc_test)
	@echo
	@echo == Running test-misc
	./$<
	@echo $@: passed

# Source code check.
#
test-src:
	@echo
	@echo == Checking for use of ssize_t in source.
	if grep -wn ssize_t src/*.c src/*.h include/*.h; then false; else true; fi
	@echo == Checking for use of strdup in source.
	if grep -wn strdup `ls -d src/*.c src/*.h|grep -v src/memento.` include; then false; else true; fi
	@echo == Checking for use of bzero in source.
	if grep -wn bzero src/*.c src/*.h include/*.h; then false; else true; fi
	@echo Checking for variables defined inside for-loop '(...)'.
	if egrep -wn 'for *[(] *[a-zA-Z0-9]+ [a-zA-Z0-9]' src/*.c src/*.h; then false; else true; fi
	@echo $@: passed

# Check that all defined global symbols start with 'extract_'. This is not
# included in the overall 'test' target because the use of '!egrep ...' appears
# to break on some cluster machines.
#
test-obj:
	@echo
	nm -egPC $(exe_obj) | egrep '^[a-zA-Z0-9_]+ T' | grep -vw ^main | ! egrep -v ^extract_
	@echo $@: passed

# Compile rule. We always include src/docx_template.c as a prerequisite in case
# code #includes docx_template.h. We use -std=gnu90 to catch 'ISO C90 forbids
# mixing declarations and code' errors while still supporting 'inline'.
#
src/build/%.c-$(build).o: src/%.c src/docx_template.c src/odt_template.c
	@mkdir -p src/build
	$(CC) -std=gnu90 -c $(flags_compile) -o $@ $<

src/build/%.cpp-$(build).o: src/%.cpp
	@mkdir -p src/build
	$(CXX) -c -Wall -W -I /usr/local/include/opencv4 -o $@ $<

# Rule for machine-generated source code, src/docx_template.c. Also generates
# src/docx_template.h.
#
# These files are also in git to allow builds if python is not available.
#
src/docx_template.c: src/docx_template_build.py .ALWAYS
	@echo
	@echo == Building $@
	./src/docx_template_build.py -i src/template.docx -n docx -o src/docx_template

src/odt_template.c: src/docx_template_build.py .ALWAYS
	@echo
	@echo == Building $@
	./src/docx_template_build.py -i src/template.odt -n odt -o src/odt_template

.ALWAYS:
.PHONY: .ALWAYS

# Tell make to preserve all intermediate files.
#
.SECONDARY:


# Rule for tags.
#
tags: .ALWAYS
	ectags -R --extra=+fq --c-kinds=+px .


# Clean rule.
#
clean:
	rm -r src/build test/generated src/template.docx.dir 2>/dev/null || true

# Cleans test/generated except for intermediate files, which are slow to create
# (when using gs).
clean2:
	rm -r test/generated/*.pdf.mutool*.docx* 2>/dev/null || true
	rm -r src/build 2>/dev/null || true
.PHONY: clean


# Include dynamic dependencies.
#
# We use $(sort ...) to remove duplicates
#
dep = $(sort $(exe_dep) $(exe_buffer_test_dep) $(exe_misc_test_dep) $(exe_ziptest_dep))

-include $(dep)
