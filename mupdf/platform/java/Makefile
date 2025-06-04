# GNU Makefile

-include ../../user.make

OS ?= $(shell uname)

JAVA := java
JAVAC := javac

ifndef build
  build := release
endif

ifeq ($(build),debug)
  BUILD_FLAGS := -DDEBUG -O0 -g
else ifeq ($(build),memento)
  BUILD_FLAGS := -DDEBUG -O0 -g -DMEMENTO
else
  BUILD_FLAGS := -DNDEBUG -O2
endif
BUILD_FLAGS += $(XCFLAGS)

ifndef OUT
  OUT := build/java/$(build)
endif

ifeq ($(OS),Darwin)
MUPDF_JAVA := ../../$(OUT)/libmupdf_java64.jnilib
JAVA_VM := $(shell /usr/libexec/java_home)
JAVA_CFLAGS := \
	-I $(JAVA_VM)/include \
	-I $(JAVA_VM)/include/darwin
JAVA_LDFLAGS := -lstdc++

else

BITS := $(shell getconf LONG_BIT)

JAVA_HOME ?= /usr/lib/jvm/default-java

MUPDF_JAVA := ../../$(OUT)/libmupdf_java$(BITS).so
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

ifeq ($(tesseract), yes)
ifeq ($(USE_SYSTEM_TESSERACT), yes)
JAVA_LIBS += \
	$(shell pkg-config --libs lept) \
	$(shell pkg-config --libs tesseract)

CONFIGFLAGS += tesseract=yes USE_SYSTEM_TESSERACT=yes
else
CONFIGFLAGS += tesseract=yes
endif
endif
endif

MUPDF_JAR := ../../$(OUT)/libmupdf.jar

EXAMPLE_JAVA_SOURCES := $(sort $(wildcard example/*.java))
EXAMPLE_JAVA_OBJECTS := $(subst example/,../../$(OUT)/example/,$(EXAMPLE_JAVA_SOURCES:%.java=%.class))

default: $(MUPDF_JAVA) $(MUPDF_JAR) $(EXAMPLE_JAVA_OBJECTS)

jar: $(MUPDF_JAR)

MUPDF_CORE := ../../$(OUT)/libmupdf.a ../../$(OUT)/libmupdf-third.a
ifeq "$(OS)" "Linux"
$(MUPDF_CORE) : .FORCE
	$(MAKE) -C ../.. OUT=$(OUT) XCFLAGS=-fPIC build=$(build) USE_SYSTEM_FREETYPE=yes USE_SYSTEM_ZLIB=yes $(CONFIGFLAGS) libs
else
$(MUPDF_CORE) : .FORCE
	$(MAKE) -C ../.. OUT=$(OUT) XCFLAGS=-fPIC build=$(build) $(CONFIGFLAGS) libs
endif

LIBRARY_JAVA_SOURCES := $(sort $(wildcard src/com/artifex/mupdf/fitz/*.java))
LIBRARY_JAVA_OBJECTS := $(subst src/,../../$(OUT)/,$(LIBRARY_JAVA_SOURCES:%.java=%.class))
LIBRARY_JAVA_CLASSES := $(subst src/com/artifex/mupdf/fitz/,com.artifex.mupdf.fitz.,$(LIBRARY_JAVA_SOURCES:%.java=%))

JAVAC_FLAGS := $(JAVAC_XFLAGS) -source 1.7 -target 1.7

$(LIBRARY_JAVA_OBJECTS) : $(LIBRARY_JAVA_SOURCES)
	mkdir -p ../../$(OUT)
	$(JAVAC) $(JAVAC_FLAGS) -h ../../$(OUT)/ -d ../../$(OUT)/ $^

$(EXAMPLE_JAVA_OBJECTS): $(EXAMPLE_JAVA_SOURCES)
	$(JAVAC) $(JAVAC_FLAGS) -classpath ../../$(OUT) -d ../../$(OUT)/ $^

$(MUPDF_JAR) : $(LIBRARY_JAVA_OBJECTS)
	rm -f $@
	cd ../../$(OUT) && jar cf libmupdf.jar com

mupdf_native.h : $(LIBRARY_JAVA_OBJECTS)
	rm -f $@
	cat $(sort $(wildcard ../../$(OUT)/*.h)) > mupdf_native.h

../../$(OUT)/mupdf_native.o : mupdf_native.c mupdf_native.h $(wildcard jni/*.c) $(MUPDF_CORE)
	$(CC) -g -o $@ -c $< -fPIC -Wall -Wextra -Wno-unused-parameter -Wpedantic -I ../../include \
		$(JAVA_CFLAGS) $(BUILD_FLAGS)

$(MUPDF_JAVA) : ../../$(OUT)/mupdf_native.o $(MUPDF_CORE)
	$(CC) -shared $(JAVA_LDFLAGS) -o $(MUPDF_JAVA) $^ $(JAVA_LIBS) $(BUILD_FLAGS)

jshell: $(MUPDF_JAVA) $(EXAMPLE_JAVA_OBJECTS) $(MUPDF_JAR)
	jshell --class-path ../../$(OUT)/ -R-Djava.library.path="../../$(OUT)" init.jshell $$MUPDF_ARGS

run: $(MUPDF_JAVA) $(EXAMPLE_JAVA_OBJECTS) $(MUPDF_JAR)
	$(JAVA) -classpath ../../$(OUT)/ -Djava.library.path="../../$(OUT)" example.Viewer $$MUPDF_ARGS

examples: $(MUPDF_JAVA) $(EXAMPLE_JAVA_OBJECTS) $(MUPDF_JAR)

run-storytest: $(MUPDF_JAVA) $(EXAMPLE_JAVA_OBJECTS) $(MUPDF_JAR)
	$(JAVA) -classpath ../../$(OUT)/ -Djava.library.path="../../$(OUT)" example.StoryTest $$MUPDF_ARGS

run-tracedevice: $(MUPDF_JAVA) $(EXAMPLE_JAVA_OBJECTS) $(MUPDF_JAR)
	$(JAVA) -classpath ../../$(OUT)/ -Djava.library.path="../../$(OUT)" example.TraceDevice $$MUPDF_ARGS

run-pdftrace: $(MUPDF_JAVA) $(EXAMPLE_JAVA_OBJECTS) $(MUPDF_JAR)
	$(JAVA) -classpath ../../$(OUT)/ -Djava.library.path="../../$(OUT)" example.PDFTrace $$MUPDF_ARGS

run-example: $(MUPDF_JAVA) $(EXAMPLE_JAVA_OBJECTS) $(MUPDF_JAR)
	$(JAVA) -classpath ../../$(OUT)/ -Djava.library.path="../../$(OUT)" example.Example $$MUPDF_ARGS

run-multithreaded: $(MUPDF_JAVA) $(EXAMPLE_JAVA_OBJECTS) $(MUPDF_JAR)
	$(JAVA) -classpath ../../$(OUT)/ -Djava.library.path="../../$(OUT)" example.MultiThreaded $$MUPDF_ARGS

run-multithreadedwithpool: $(MUPDF_JAVA) $(EXAMPLE_JAVA_OBJECTS) $(MUPDF_JAR)
	$(JAVA) -classpath ../../$(OUT)/ -Djava.library.path="../../$(OUT)" example.MultiThreadedWithPool $$MUPDF_ARGS

gdb: $(MUPDF_JAVA) $(EXAMPLE_JAVA_OBJECTS) $(MUPDF_JAR)
	gdb --args $(JAVA) -classpath ../../$(OUT) -Djava.library.path="../../$(OUT)" example.Viewer $$MUPDF_ARGS

clean:
	rm -f $(LIBRARY_JAVA_OBJECTS) $(subst .class,\$$*.class,$(LIBRARY_JAVA_OBJECTS))
	rm -f $(EXAMPLE_JAVA_OBJECTS) $(subst .class,\$$*.class,$(EXAMPLE_JAVA_OBJECTS))
	rm -f ../../$(OUT)/mupdf_native.o
	rm -fr ../../$(OUT)/resources
	rm -fr ../../$(OUT)/source
	rm -fr ../../$(OUT)/thirdparty
	rm -f $(MUPDF_JAR)
	rm -f $(MUPDF_JAVA)
	rm -f $(MUPDF_CORE)
	rm -f ../../$(OUT)/*.h

nuke: clean
	$(MAKE) -C ../.. OUT=$(OUT) build=$(build) clean

.NOTPARALLEL : # disable -j option (it breaks since javac compiles all class files in one command)
.PHONY: .FORCE
