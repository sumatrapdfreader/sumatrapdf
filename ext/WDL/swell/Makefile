# use make 
# or make NOGDK=1
# or make DEBUG=1 
# etc

ARCH := $(shell uname -m)
UNAME_S := $(shell uname -s)

PKG_CONFIG = pkg-config

COMPILER=GCC

CFLAGS = -pipe -fvisibility=hidden -fno-math-errno -fPIC -DPIC -Wall -Wshadow -Wno-unused-function -Wno-multichar

CFLAGS += -DSWELL_MAKING_DYLIB

ifeq ($(UNAME_S),Darwin)
  CFLAGS += -Wno-unused-private-field -DSWELL_FORCE_GENERIC
  DLL_EXT=.dylib
  COMPILER=CLANG
else
  DLL_EXT=.so
endif

ifeq ($(COMPILER),CLANG)
  CC = clang
  CXX = clang++
endif

ifeq ($(COMPILER),ICC)
  CC = icc
  CXX = icpc
  CFLAGS += -D__PURE_SYS_C99_HEADERS__
else
  CFLAGS +=  -Wno-unused-result
endif

ifndef ALLOW_WARNINGS
  CFLAGS += -Werror
endif
ifndef DEPRECATED_WARNINGS
  CFLAGS +=  -Wno-deprecated-declarations
endif

ifneq ($(filter arm%,$(ARCH)),)
  CFLAGS += -fsigned-char -marm
endif
ifeq ($(ARCH),aarch64)
  CFLAGS += -fsigned-char
endif


ifdef DEBUG
CFLAGS += -O0 -g -D_DEBUG
else
CFLAGS += -O2 -DNDEBUG
  ifdef DEBUG_INFO
    CFLAGS += -g
  else
    ifneq ($(COMPILER),CLANG)
      CFLAGS += -s
    endif
  endif
endif

LINKEXTRA =  -lpthread -ldl 


EXTRA_OBJS = 




vpath %.cpp .. ../lice 

SWELL_OBJS = swell.o swell-ini.o swell-miscdlg-generic.o swell-wnd-generic.o \
             swell-menu-generic.o swell-kb-generic.o swell-dlg-generic.o \
             swell-gdi-generic.o swell-misc-generic.o swell-gdi-lice.o \
             swell-generic-headless.o swell-generic-gdk.o \
             swell-appstub-generic.o swell-modstub-generic.o 

LICE_OBJS = lice.o  lice_arc.o lice_colorspace.o lice_line.o lice_text.o \
            lice_textnew.o lice_ico.o lice_bmp.o

OBJS = $(SWELL_OBJS)

ifndef NOGDK
  ifdef GDK2
    CFLAGS += -DSWELL_TARGET_GDK=2 $(shell $(PKG_CONFIG) --cflags gdk-2.0)
    ifndef PRELOAD_GDK
      LINKEXTRA += $(shell $(PKG_CONFIG) --libs gdk-2.0)
    else
      LINKEXTRA += -lX11 -lXi
      CFLAGS += -DSWELL_PRELOAD="libgdk-x11-2.0.so.0"
    endif
  else
    ifdef SWELL_SUPPORT_GTK
      CFLAGS += -DSWELL_TARGET_GDK=3 $(shell $(PKG_CONFIG) --cflags gtk+-3.0) -DSWELL_SUPPORT_GTK
    else
      CFLAGS += -DSWELL_TARGET_GDK=3 $(shell $(PKG_CONFIG) --cflags gdk-3.0)
    endif
    ifndef PRELOAD_GDK
      ifdef SWELL_SUPPORT_GTK
        LINKEXTRA += $(shell $(PKG_CONFIG) --libs gtk+-3.0)
      else
        LINKEXTRA += $(shell $(PKG_CONFIG) --libs gdk-3.0)
      endif
    else
      LINKEXTRA += -lX11 -lXi
      ifdef SWELL_SUPPORT_GTK
        CFLAGS += -DSWELL_PRELOAD="libgtk+-3.so.0"
      else
        CFLAGS += -DSWELL_PRELOAD="libgdk-3.so.0"
      endif
    endif
  endif
  CFLAGS += -DSWELL_LICE_GDI
  OBJS += $(LICE_OBJS)

  ifndef NOFREETYPE
    CFLAGS += -DSWELL_FREETYPE $(shell $(PKG_CONFIG) --cflags freetype2)
    ifndef PRELOAD_GDK
      LINKEXTRA += $(shell $(PKG_CONFIG) --libs freetype2)
    endif
    ifndef NOFONTCONFIG
      CFLAGS += -DSWELL_FONTCONFIG
      LINKEXTRA += -lfontconfig
    endif
  endif
endif

CXXFLAGS = $(CFLAGS)

default: libSwell$(DLL_EXT)

.PHONY: clean 

SWELL_HEADERS=swell-dlggen.h swell-gdi-internalpool.h swell-menugen.h swell-types.h swell-functions.h swell-internal.h

swell.o: $(SWELL_HEADERS)
swell-ini.o: $(SWELL_HEADERS)
swell-wnd-generic.o: $(SWELL_HEADERS)
swell-dlg-generic.o: $(SWELL_HEADERS)
swell-menu-generic.o: $(SWELL_HEADERS)
swell-misc-generic.o: $(SWELL_HEADERS)
swell-gdi-generic.o: $(SWELL_HEADERS)
swell-gdi-lice.o: $(SWELL_HEADERS)
swell-kb-generic.o: $(SWELL_HEADERS)
swell-miscdlg-generic.o: $(SWELL_HEADERS)
swell-modstub-generic.o: $(SWELL_HEADERS)
swell-appstub-generic.o: $(SWELL_HEADERS)

libSwell$(DLL_EXT): $(OBJS)
	$(CXX) -shared -o $@ $(CFLAGS) $(LFLAGS) $^ $(LINKEXTRA)

test: $(OBJS) test.o
	$(CXX) -o test $(CFLAGS) $(LFLAGS) $^ $(LINKEXTRA)

libSwell.colortheme: swell-gdi-generic.cpp $(SWELL_HEADERS)
	$(CXX) $(CFLAGS) -o make-theme -DSWELL__MAKE_THEME swell-gdi-generic.cpp -lpthread
	./make-theme > libSwell.colortheme

clean: 
	-rm $(OBJS) libSwell$(DLL_EXT) make-theme libSwell.colortheme
