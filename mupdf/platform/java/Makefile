OS ?= $(shell uname)

JAVA := java
JAVAC := javac

ifeq "$(OS)" "Darwin"
MUPDF_JAVA := ../../build/java/libmupdf_java64.jnilib
JAVA_CFLAGS := \
	-I /Library/Java/JavaVirtualMachines/jdk1.8.0_66.jdk/Contents/Home/include \
	-I /Library/Java/JavaVirtualMachines/jdk1.8.0_66.jdk/Contents/Home/include/darwin
JAVA_LDFLAGS :=

else

BITS := $(shell getconf LONG_BIT)

JAVA_HOME ?= /usr/lib/jvm/default-java

MUPDF_JAVA := ../../build/java/libmupdf_java$(BITS).so
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

MUPDF_JAR := ../../build/java/libmupdf.jar

default: $(MUPDF_JAVA) $(MUPDF_JAR) $(EXAMPLE_JAVA_SOURCES)

MUPDF_CORE := ../../build/java/libmupdf.a ../../build/java/libmupdf-third.a
ifeq "$(OS)" "Linux"
$(MUPDF_CORE) : .FORCE
	$(MAKE) -C ../.. OUT=build/java XCFLAGS=-fPIC USE_SYSTEM_FREETYPE=yes USE_SYSTEM_ZLIB=yes libs
else
$(MUPDF_CORE) : .FORCE
	$(MAKE) -C ../.. OUT=build/java XCFLAGS=-fPIC libs
endif

LIBRARY_JAVA_SOURCES := $(sort $(wildcard src/com/artifex/mupdf/fitz/*.java))
LIBRARY_JAVA_OBJECTS := $(subst src/,../../build/java/,$(LIBRARY_JAVA_SOURCES:%.java=%.class))
LIBRARY_JAVA_CLASSES := $(subst src/com/artifex/mupdf/fitz/,com.artifex.mupdf.fitz.,$(LIBRARY_JAVA_SOURCES:%.java=%))

JAVAC_FLAGS := $(JAVAC_XFLAGS) -source 1.7 -target 1.7

$(LIBRARY_JAVA_OBJECTS) : $(LIBRARY_JAVA_SOURCES)
	$(JAVAC) $(JAVAC_FLAGS) -h ../../build/java/ -d ../../build/java/ $^

EXAMPLE_JAVA_SOURCES := $(sort $(wildcard example/*.java))
EXAMPLE_JAVA_OBJECTS := $(subst example/,../../build/java/example/,$(EXAMPLE_JAVA_SOURCES:%.java=%.class))

$(EXAMPLE_JAVA_OBJECTS): $(EXAMPLE_JAVA_SOURCES)
	$(JAVAC) $(JAVAC_FLAGS) -classpath ../../build/java -d ../../build/java/ $^

$(MUPDF_JAR) : $(LIBRARY_JAVA_OBJECTS)
	rm -f $@
	jar cf $@ $^

mupdf_native.h : $(LIBRARY_JAVA_OBJECTS)
	rm -f $@
	cat $(sort $(wildcard ../../build/java/*.h)) > mupdf_native.h

../../build/java/mupdf_native.o : mupdf_native.c mupdf_native.h $(wildcard jni/*.c) $(MUPDF_CORE)
	$(CC) -g -o $@ -c $< -fPIC -Wall -Wextra -Wno-unused-parameter -I ../../include \
		$(JAVA_CFLAGS)

$(MUPDF_JAVA) : ../../build/java/mupdf_native.o $(MUPDF_CORE)
	$(CC) -shared $(JAVA_LDFLAGS) -o $(MUPDF_JAVA) $^ $(JAVA_LIBS)

run: $(MUPDF_JAVA) $(EXAMPLE_JAVA_OBJECTS) $(MUPDF_JAR)
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:../../build/java $(JAVA) -classpath ../../build/java/ example.Viewer $$MUPDF_ARGS

gdb: $(MUPDF_JAVA) $(EXAMPLE_JAVA_OBJECTS) $(MUPDF_JAR)
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:../../build/java gdb --args $(JAVA) -classpath ../../build/java example.Viewer $$MUPDF_ARGS

clean:
	rm -f $(LIBRARY_JAVA_OBJECTS) $(subst .class,\$$*.class,$(LIBRARY_JAVA_OBJECTS))
	rm -f $(EXAMPLE_JAVA_OBJECTS) $(subst .class,\$$*.class,$(EXAMPLE_JAVA_OBJECTS))
	rm -f ../../build/java/mupdf_native.o
	rm -f $(MUPDF_JAR)
	rm -f $(MUPDF_JAVA)
	rm -f ../../build/java/*.h

nuke: clean
	$(MAKE) -C ../.. OUT=build/java clean

.NOTPARALLEL : # disable -j option (it breaks since javac compiles all class files in one command)
.PHONY: .FORCE
