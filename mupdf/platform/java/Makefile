OS ?= $(shell uname)

JAVA := java
JAVAC := javac

ifeq "$(OS)" "Darwin"
MUPDF_JAVA := libmupdf_java64.jnilib
JAVA_CFLAGS := \
	-I /Library/Java/JavaVirtualMachines/jdk1.8.0_66.jdk/Contents/Home/include \
	-I /Library/Java/JavaVirtualMachines/jdk1.8.0_66.jdk/Contents/Home/include/darwin
JAVA_LDFLAGS :=

else

BITS := $(shell getconf LONG_BIT)

JAVA_HOME ?= /usr/lib/jvm/default-java

MUPDF_JAVA := libmupdf_java$(BITS).so
JAVA_CFLAGS := \
	-I $(JAVA_HOME)/include \
	-I $(JAVA_HOME)/include/linux \
	$(shell pkg-config --cflags freetype2) \
	$(shell pkg-config --cflags libcrypto)
JAVA_LIBS := \
	$(shell pkg-config --libs freetype2) \
	$(shell pkg-config --libs libcrypto) \
	-lz
JAVA_LDFLAGS := \
	-z noexecstack
endif

default: $(MUPDF_JAVA) libmupdf.jar

MUPDF_CORE := ../../build/java/libmupdf.a ../../build/java/libmupdf-third.a
ifeq "$(OS)" "Linux"
$(MUPDF_CORE) : .FORCE
	$(MAKE) -C ../.. OUT=build/java XCFLAGS=-fPIC USE_SYSTEM_FREETYPE=yes USE_SYSTEM_ZLIB=yes libs
else
$(MUPDF_CORE) : .FORCE
	$(MAKE) -C ../.. OUT=build/java XCFLAGS=-fPIC libs
endif

LIBRARY_JAVA_SOURCES := $(sort $(wildcard src/com/artifex/mupdf/fitz/*.java))
LIBRARY_JAVA_OBJECTS := $(LIBRARY_JAVA_SOURCES:%.java=%.class)
LIBRARY_JAVA_CLASSES := $(subst src/com/artifex/mupdf/fitz/,com.artifex.mupdf.fitz.,$(LIBRARY_JAVA_SOURCES:%.java=%))

JAVAC_FLAGS := $(JAVAC_XFLAGS) -source 1.7 -target 1.7

$(LIBRARY_JAVA_OBJECTS) : $(LIBRARY_JAVA_SOURCES)
	$(JAVAC) $(JAVAC_FLAGS) -h ../../build/java/ $^

EXAMPLE_JAVA_SOURCES := $(sort $(wildcard example/*.java))
EXAMPLE_JAVA_OBJECTS := $(EXAMPLE_JAVA_SOURCES:%.java=%.class)

$(EXAMPLE_JAVA_OBJECTS) : $(EXAMPLE_JAVA_SOURCES)
	$(JAVAC) $(JAVAC_FLAGS) -classpath .:src $^

libmupdf.jar : $(LIBRARY_JAVA_OBJECTS)
	rm -f $@
	jar cf $@ $^

mupdf_native.h : $(LIBRARY_JAVA_OBJECTS)
	rm -f $@
	cat $(sort $(wildcard ../../build/java/*.h)) > mupdf_native.h

mupdf_native.o : mupdf_native.c mupdf_native.h $(MUPDF_CORE)
	$(CC) -g -o $@ -c $< -fPIC -Wall -Wextra -Wno-unused-parameter -I ../../include \
		$(JAVA_CFLAGS)

$(MUPDF_JAVA) : mupdf_native.o $(MUPDF_CORE)
	$(CC) -shared $(JAVA_LDFLAGS) -o $(MUPDF_JAVA) $^ $(JAVA_LIBS)

run: $(MUPDF_JAVA) $(EXAMPLE_JAVA_OBJECTS)
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:. $(JAVA) -classpath .:src example.Viewer

gdb: $(MUPDF_JAVA) $(EXAMPLE_JAVA_OBJECTS)
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:. gdb --args $(JAVA) -classpath .:src example.Viewer

clean:
	rm -f src/com/artifex/mupdf/fitz/*.class
	rm -f example/*.class
	rm -f mupdf_native.o
	rm -f libmupdf.jar
	rm -f $(MUPDF_JAVA)
	rm -f ../../build/java/*.h

nuke: clean
	$(MAKE) -C ../.. OUT=build/java clean

.NOTPARALLEL : # disable -j option (it breaks since javac compiles all class files in one command)
.PHONY: .FORCE
