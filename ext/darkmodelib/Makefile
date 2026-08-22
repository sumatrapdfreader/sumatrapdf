# SPDX-License-Identifier: MPL-2.0

# Copyright (c) 2025-2026 ozone10
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# This file is part of darkmodelib library.


DEBUG ?= 0

# Archiver
AR := ar
ARFLAGS := rcs

# Resource
RC := $(CROSS_COMPILE)windres

# Verbose mode
ifeq ($(VERBOSE),1)
	AT :=
else
	AT := @
	MAKEFLAGS += --no-print-directory
endif

ifeq ($(CXX),clang++)
	CXX := $(CROSS_COMPILE)clang++
	COMPILER_NAME := clang
	COMPILER_FLAGS := -Weverything \
						-Wno-c++98-compat \
						-Wno-c++98-compat-pedantic \
						-Wno-c99-extensions \
						-Wno-padded \
						-Wno-switch-default
else
	CXX := $(CROSS_COMPILE)g++
	COMPILER_NAME := gcc
	COMPILER_FLAGS := -Wmisleading-indentation \
						-Wduplicated-cond \
						-Wduplicated-branches \
						-Wlogical-op \
						-Wnull-dereference \
						-Wuseless-cast
endif

#-Weffc++

# Warning flags for the library
CXXWARNFLAGS = 	-Wall \
				-Wextra \
				-Wshadow \
				-pedantic \
				-Wpedantic \
				-Wcast-align \
				-Wconversion \
				-Wdouble-promotion \
				-Wformat=2 \
				-Wimplicit-fallthrough \
				-Wnon-virtual-dtor \
				-Wold-style-cast \
				-Woverloaded-virtual \
				-Wsign-conversion \
				-Wuninitialized \
				-Wunused

# Warning flags for the demo
DEMO_WARNFLAGS := -Wall \
					-Wextra \
					-Wshadow \
					-pedantic \
					-Wpedantic \
					-Wformat=2 \
					-Wimplicit-fallthrough \
					-Wold-style-cast \
					-Wuninitialized

# Demo additional libraries and flags
LD_LINK := comctl32 comdlg32 dwmapi gdi32 shlwapi uxtheme

LDFLAGS := -municode -mwindows

# Preprocessor flags
CXXPREFLAGS = -Iinclude \
				-D_WIN64 \
				-D_WINDOWS \
				-DSTRICT_TYPED_ITEMIDS \
				-DNOMINMAX \
				-DWIN32_LEAN_AND_MEAN \
				-DVC_EXTRALEAN \
				-DSUPPORT_UTF8 \
				-DUNICODE \
				-D_UNICODE \
				-D_WIN32_WINNT=0x0601

DMLIB_FLAGS = -D_DARKMODELIB_NO_INI_CONFIG -D_DARKMODELIB_CUSTOM_MEM=0x002

# Language standard and architecture
CXXLANGFLAGS = -std=c++20 \
				-m64

RCFLAGS := --codepage=65001

ARCH := x64

ifeq ($(DEBUG),1)
	CXXOPTFLAGS = -O0 -g
	DEMO_OPTFLAGS = -O0 -g
	CONFIGURATION := debug
	CXXPREFLAGS += -DDEBUG
else
	CXXOPTFLAGS = -O3
	DEMO_OPTFLAGS = -O3
	CONFIGURATION := release
	CXXPREFLAGS += -DNDEBUG
endif

# Flags for the library
CXXFLAGS := $(CXXWARNFLAGS) $(CXXOPTFLAGS) $(CXXPREFLAGS) \
			$(CXXLANGFLAGS) $(COMPILER_FLAGS) $(DMLIB_FLAGS)

# Flags for the demo
DEMO_CXXFLAGS := $(DEMO_WARNFLAGS) $(DEMO_OPTFLAGS) $(CXXPREFLAGS) $(CXXLANGFLAGS)

# Output
OUTDIR := build/$(COMPILER_NAME)/lib/$(ARCH)-$(CONFIGURATION)
OBJDIR := build/$(COMPILER_NAME)/obj/$(ARCH)-$(CONFIGURATION)

DEMO_OUTDIR := build/$(COMPILER_NAME)/bin/$(ARCH)-$(CONFIGURATION)
DEMO_OBJDIR := build/$(COMPILER_NAME)/obj_demo/$(ARCH)-$(CONFIGURATION)

LIBNAME := $(OUTDIR)/darkmode.lib

SRC := $(wildcard src/*.cpp)
OBJ := $(patsubst src/%.cpp,$(OBJDIR)/%.o,$(SRC))

DEMO_SRC := $(wildcard dmlib_demo/*.cpp)
DEMO_OBJ := $(patsubst dmlib_demo/%.cpp,$(DEMO_OBJDIR)/%.o,$(DEMO_SRC))

DEMO_RC := $(wildcard dmlib_demo/*.rc)
DEMO_RES := $(patsubst dmlib_demo/%.rc,$(DEMO_OBJDIR)/%.res,$(DEMO_RC))

DEMO_EXE := $(DEMO_OUTDIR)/dmlib-demo.exe

.PHONY: all lib demo clean clean-lib clean-demo

all: lib demo

lib:
	@echo "Building darkmode.lib"
	$(AT)$(MAKE) $(LIBNAME)

demo:
	@echo "Building dmlib_demo.exe"
	$(AT)$(MAKE) $(DEMO_EXE)

# --- Library build ---

$(LIBNAME): $(OBJ) | $(OUTDIR)
	@echo "Archiving library: $@"
	$(AT)$(AR) $(ARFLAGS) $@ $^

$(OBJDIR)/%.o: src/%.cpp | $(OBJDIR)
	@echo "Compiling: $<"
	$(AT)$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR):
	$(AT)mkdir -p $(OBJDIR)

$(OUTDIR):
	$(AT)mkdir -p $(OUTDIR)

# --- Demo build ---

$(DEMO_EXE): $(DEMO_OBJ) $(DEMO_RES) $(LIBNAME) | $(DEMO_OUTDIR)
	@echo "Linking: $@"
	$(AT)$(CXX) $(LDFLAGS) $(DEMO_OBJ) $(DEMO_RES) -static $(LIBNAME) $(addprefix -l,$(LD_LINK)) -o $@

$(DEMO_OBJDIR)/%.res: dmlib_demo/%.rc | $(DEMO_OBJDIR)
	@echo "Compiling: $<"
	$(AT)$(RC) $(RCFLAGS) -O coff -o $@ -i $<

$(DEMO_OBJDIR)/%.o: dmlib_demo/%.cpp | $(DEMO_OBJDIR)
	@echo "Compiling: $<"
	$(AT)$(CXX) $(DEMO_CXXFLAGS) -c $< -o $@

$(DEMO_OBJDIR):
	$(AT)mkdir -p $(DEMO_OBJDIR)

$(DEMO_OUTDIR):
	$(AT)mkdir -p $(DEMO_OUTDIR)

# Cleaning

clean-lib:
	$(AT)rm -rf $(OBJDIR) $(OUTDIR)

clean-demo:
	$(AT)rm -rf $(DEMO_OBJDIR) $(DEMO_OUTDIR)

clean: clean-lib clean-demo
