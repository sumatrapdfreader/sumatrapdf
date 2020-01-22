# Build type and install directories:

build ?= release

prefix ?= /usr/local
bindir ?= $(prefix)/bin
incdir ?= $(prefix)/include
libdir ?= $(prefix)/lib

ifeq "$(wildcard .git)" ".git"
  VERSION := $(shell git describe --tags --always)
else
  VERSION := $(shell basename $$PWD | sed -e s,^mujs-,,)
endif

# Compiler flags for various configurations:

CFLAGS := -std=c99 -pedantic -Wall -Wextra -Wno-unused-parameter

ifeq "$(CC)" "clang"
  CFLAGS += -Wunreachable-code
endif

ifeq "$(shell uname)" "Linux"
  HAVE_READLINE := yes
endif

ifeq "$(build)" "debug"
  CFLAGS += -g
else ifeq "$(build)" "sanitize"
  CFLAGS += -pipe -g -fsanitize=address -fno-omit-frame-pointer
  LDFLAGS += -fsanitize=address
else
  CFLAGS += -Os
  LDFLAGS += -Wl,-s
endif

ifeq "$(HAVE_READLINE)" "yes"
  CFLAGS += -DHAVE_READLINE
  LIBREADLINE += -lreadline
endif

CFLAGS += $(XCFLAGS)

# You shouldn't need to edit anything below here.

OUT := build/$(build)

SRCS := $(wildcard js*.c utf*.c regexp.c)
HDRS := $(wildcard js*.h mujs.h utf.h regexp.h)

default: shell
shell: $(OUT)/mujs $(OUT)/mujs-pp
static: $(OUT)/libmujs.a
shared: $(OUT)/libmujs.so

astnames.h: jsparse.h
	grep -E '(AST|EXP|STM)_' jsparse.h | sed 's/^[^A-Z]*\(AST_\)*/"/;s/,.*/",/' | tr A-Z a-z > $@

opnames.h: jscompile.h
	grep -E 'OP_' jscompile.h | sed 's/^[^A-Z]*OP_/"/;s/,.*/",/' | tr A-Z a-z > $@

one.c: $(SRCS)
	ls $(SRCS) | awk '{print "#include \""$$1"\""}' > $@

jsdump.c: astnames.h opnames.h

$(OUT)/%.o: %.c $(HDRS)
	@ mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ -c $<

$(OUT)/libmujs.o: one.c $(HDRS)
	@ mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ -c $<

$(OUT)/libmujs.a: $(OUT)/libmujs.o
	@ mkdir -p $(dir $@)
	$(AR) cr $@ $^

$(OUT)/libmujs.so: one.c $(HDRS)
	@ mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $< -lm

$(OUT)/mujs: $(OUT)/libmujs.o $(OUT)/main.o
	@ mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBREADLINE) -lm

$(OUT)/mujs-pp: $(OUT)/libmujs.o $(OUT)/pp.o
	@ mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $^ -lm

.PHONY: $(OUT)/mujs.pc
$(OUT)/mujs.pc:
	@ echo Creating $@
	@ echo > $@ Name: mujs
	@ echo >> $@ Description: MuJS embeddable Javascript interpreter
	@ echo >> $@ Version: $(VERSION)
	@ echo >> $@ Cflags: -I$(incdir)
	@ echo >> $@ Libs: -L$(libdir) -lmujs
	@ echo >> $@ Libs.private: -lm

watch:
	@ while ! inotifywait -q -e modify $(SRCS) $(HDRS) ; do time -p $(MAKE) ; done

install-common: shell $(OUT)/mujs.pc
	install -d $(DESTDIR)$(incdir)
	install -d $(DESTDIR)$(libdir)
	install -d $(DESTDIR)$(libdir)/pkgconfig
	install -d $(DESTDIR)$(bindir)
	install -m 644 mujs.h $(DESTDIR)$(incdir)
	install -m 644 $(OUT)/mujs.pc $(DESTDIR)$(libdir)/pkgconfig
	install -m 755 $(OUT)/mujs $(DESTDIR)$(bindir)

install-static: install-common static
	install -m 644 $(OUT)/libmujs.a $(DESTDIR)$(libdir)

install-shared: install-common shared
	install -m 755 $(OUT)/libmujs.so $(DESTDIR)$(libdir)

install: install-static

tarball:
	git archive --format=zip --prefix=mujs-$(VERSION)/ HEAD > mujs-$(VERSION).zip
	git archive --format=tar --prefix=mujs-$(VERSION)/ HEAD | gzip > mujs-$(VERSION).tar.gz
	git archive --format=tar --prefix=mujs-$(VERSION)/ HEAD | xz > mujs-$(VERSION).tar.xz

tags: $(SRCS) main.c $(HDRS)
	ctags $^

clean:
	rm -rf build

nuke: clean
	rm -f astnames.h opnames.h one.c

debug:
	$(MAKE) build=debug

sanitize:
	$(MAKE) build=sanitize

release:
	$(MAKE) build=release

.PHONY: default static shared shell clean nuke
.PHONY: install install-common install-shared install-static
.PHONY: debug sanitize release
