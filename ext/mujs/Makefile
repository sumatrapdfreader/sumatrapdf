# Makefile for building MuJS libraries, shell, and pretty-printer.
#
# Useful targets are: release, install, uninstall.

default: build/debug/mujs build/debug/mujs-pp

CFLAGS = -std=c99 -pedantic -Wall -Wextra -Wno-unused-parameter

OPTIM = -O3

prefix = /usr/local
bindir = $(prefix)/bin
incdir = $(prefix)/include
libdir = $(prefix)/lib

ifeq ($(wildcard .git),.git)
  VERSION = $(shell git describe --tags --always)
else
  VERSION = $(patsubst mujs-%,%,$(notdir $(CURDIR)))
endif

ifeq ($(shell uname),Darwin)
  SO = dylib
else
  SO = so
endif

ifeq ($(shell uname),FreeBSD)
  CFLAGS += -I/usr/local/include -L/usr/local/lib
endif

HDRS = mujs.h jsi.h regexp.h utf.h astnames.h opnames.h

ifneq ($(HAVE_READLINE),no)
  READLINE_CFLAGS = -DHAVE_READLINE
  READLINE_LIBS = -lreadline
endif

SRCS = \
	jsarray.c \
	jsboolean.c \
	jsbuiltin.c \
	jscompile.c \
	jsdate.c \
	jsdtoa.c \
	jserror.c \
	jsfunction.c \
	jsgc.c \
	jsintern.c \
	jslex.c \
	jsmath.c \
	jsnumber.c \
	jsobject.c \
	json.c \
	jsparse.c \
	jsproperty.c \
	jsregexp.c \
	jsrepr.c \
	jsrun.c \
	jsstate.c \
	jsstring.c \
	jsvalue.c \
	regexp.c \
	utf.c

one.c:
	for F in $(SRCS); do echo "#include \"$$F\""; done > $@

astnames.h: jsi.h
	grep -E '\<(AST|EXP|STM)_' jsi.h | sed 's/^[^A-Z]*\(AST_\)*/"/;s/,.*/",/' | tr A-Z a-z > $@

opnames.h: jsi.h
	grep -E '\<OP_' jsi.h | sed 's/^[^A-Z]*OP_/"/;s/,.*/",/' | tr A-Z a-z > $@

UnicodeData.txt:
	curl -s -o $@ https://www.unicode.org/Public/UCD/latest/ucd/UnicodeData.txt

utfdata.h: genucd.py UnicodeData.txt
	python3 genucd.py UnicodeData.txt >$@

build/sanitize/mujs: main.c one.c $(SRCS) $(HDRS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -g -fsanitize=address -fno-omit-frame-pointer -o $@ main.c one.c -lm $(READLINE_CFLAGS) $(READLINE_LIBS)

build/debug/libmujs.$(SO): one.c $(SRCS) $(HDRS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -g -fPIC -shared -o $@ one.c -lm
build/debug/libmujs.o: one.c $(SRCS) $(HDRS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -g -c -o $@ one.c
build/debug/libmujs.a: build/debug/libmujs.o
	$(AR) cr $@ $^
build/debug/mujs: main.c build/debug/libmujs.o
	$(CC) $(CFLAGS) -g -o $@ $^ -lm $(READLINE_CFLAGS) $(READLINE_LIBS)
build/debug/mujs-pp: pp.c build/debug/libmujs.o
	$(CC) $(CFLAGS) -g -o $@ $^ -lm

build/release/libmujs.$(SO): one.c $(SRCS) $(HDRS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(OPTIM) -fPIC -shared -o $@ one.c -lm
build/release/libmujs.o: one.c $(SRCS) $(HDRS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(OPTIM) -c -o $@ one.c
build/release/libmujs.a: build/release/libmujs.o
	$(AR) cr $@ $^
build/release/mujs: main.c build/release/libmujs.o
	$(CC) $(CFLAGS) $(OPTIM) -o $@ $^ -lm $(READLINE_CFLAGS) $(READLINE_LIBS)
build/release/mujs-pp: pp.c build/release/libmujs.o
	$(CC) $(CFLAGS) $(OPTIM) -o $@ $^ -lm

build/release/mujs.pc:
	@mkdir -p $(@D)
	echo > $@ Name: mujs
	echo >> $@ Description: MuJS embeddable Javascript interpreter
	echo >> $@ Version: $(VERSION)
	echo >> $@ Cflags: -I$(incdir)
	echo >> $@ Libs: -L$(libdir) -lmujs
	echo >> $@ Libs.private: -lm

install-common: build/release/mujs build/release/mujs-pp build/release/mujs.pc
	install -d $(DESTDIR)$(incdir)
	install -d $(DESTDIR)$(libdir)
	install -d $(DESTDIR)$(libdir)/pkgconfig
	install -d $(DESTDIR)$(bindir)
	install -m 644 mujs.h $(DESTDIR)$(incdir)
	install -m 644 build/release/mujs.pc $(DESTDIR)$(libdir)/pkgconfig
	install -m 755 build/release/mujs $(DESTDIR)$(bindir)
	install -m 755 build/release/mujs-pp $(DESTDIR)$(bindir)

install-static: install-common build/release/libmujs.a
	install -m 644 build/release/libmujs.a $(DESTDIR)$(libdir)

install-shared: install-common build/release/libmujs.$(SO)
	install -m 755 build/release/libmujs.$(SO) $(DESTDIR)$(libdir)

install: install-static

uninstall:
	rm -f $(DESTDIR)$(bindir)/mujs
	rm -f $(DESTDIR)$(bindir)/mujs-pp
	rm -f $(DESTDIR)$(incdir)/mujs.h
	rm -f $(DESTDIR)$(libdir)/pkgconfig/mujs.pc
	rm -f $(DESTDIR)$(libdir)/libmujs.a
	rm -f $(DESTDIR)$(libdir)/libmujs.$(SO)

tarball:
	git archive --format=zip --prefix=mujs-$(VERSION)/ HEAD > mujs-$(VERSION).zip
	git archive --format=tar --prefix=mujs-$(VERSION)/ HEAD | gzip > mujs-$(VERSION).tar.gz

tags: $(SRCS) $(HDRS) main.c pp.c
	ctags $^

clean:
	rm -rf build

nuke: clean
	rm -f one.c astnames.h opnames.h

sanitize: build/sanitize/mujs

debug: build/debug/libmujs.a
debug: build/debug/libmujs.$(SO)
debug: build/debug/mujs
debug: build/debug/mujs-pp

release: build/release/mujs.pc
release: build/release/libmujs.a
release: build/release/libmujs.$(SO)
release: build/release/mujs
release: build/release/mujs-pp
