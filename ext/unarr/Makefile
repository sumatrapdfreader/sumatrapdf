# GNU Makefile

build ?= debug

OUT := build/$(build)

default: all

# --- Configuration ---

CFLAGS += -Wall -D_FILE_OFFSET_BITS=64
LIBS += -lm

ifeq "$(build)" "debug"
CFLAGS += -pipe -g -DDEBUG
else ifeq "$(build)" "profile"
CFLAGS += -pipe -O3 -DNDEBUG -pg
LDFLAGS += -pg
else ifeq "$(build)" "release"
CFLAGS += -pipe -O3 -DNDEBUG -fomit-frame-pointer
else ifeq "$(build)" "coverage"
CFLAGS += -pipe -g -DDEBUG -pg -fprofile-arcs -ftest-coverage
LIBS += -lgcov
else
$(error unknown build setting: '$(build)')
endif

# --- Commands ---

ifneq "$(verbose)" "yes"
QUIET_AR = @ echo ' ' ' ' AR $@ ;
QUIET_CC = @ echo ' ' ' ' CC $@ ;
QUIET_LINK = @ echo ' ' ' ' LINK $@ ;
endif

CC_CMD = $(QUIET_CC) mkdir -p $(@D) ; $(CC) $(CFLAGS) -o $@ -c $<
AR_CMD = $(QUIET_AR) $(AR) cr $@ $^
LINK_CMD = $(QUIET_LINK) $(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# --- Third party libraries ---

# TODO: build zlib, bzip2 and 7z when available

# --- unarr files ---

UNARR_OUT := $(OUT)/unarr

UNARR_DIRS := common lzmasdk rar tar zip _7z
UNARR_SRC := $(wildcard $(UNARR_DIRS:=/*.c))
UNARR_OBJ := $(addprefix $(UNARR_OUT)/, $(addsuffix .o, $(basename $(UNARR_SRC))))

$(UNARR_OUT)/%.o : %.c
	$(CC_CMD)

UNARR_LIB := $(OUT)/libunarr.a

$(UNARR_LIB): $(UNARR_OBJ)
	$(AR_CMD)

UNARR_TEST := $(OUT)/unarr-test

$(UNARR_TEST) : $(UNARR_OUT)/main.o $(UNARR_LIB)
	$(LINK_CMD)

# TODO: add header dependencies

# --- Clean and Default ---

all: $(UNARR_TEST)

clean:
	rm -rf build

.PHONY: all clean
