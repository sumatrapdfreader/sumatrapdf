# GNU Make file for Fitz, Mu PDF, pdftool and Apparition

FITZ_DIR ?= fitz
-include $(FITZ_DIR)/config.mk

# FITZ_BUILTIN_FONTS ?= yes

FITZ_CONFIG ?= $(BUILD)
FITZ_INC_DIR ?= $(INC_DIR)
FITZ_CPPFLAGS ?= $(CPPFLAGS)
FITZ_CFLAGS ?= $(CFLAGS)
FITZ_LDFLAGS ?= $(LDFLAGS)
FITZ_LDLIBS ?= $(LDLIBS)

FITZ_CONFIG_debug = yes
APPARITION_LDFLAGS_debug ?= -mconsole

FITZ_CONFIG_release = yes
FITZ_CPPFLAGS_release ?= -D NDEBUG
FITZ_CFLAGS_release ?= -O3
FITZ_LDFLAGS_release ?= -static -s
APPARITION_LDFLAGS_release ?= -mwindows

ifeq ($(FITZ_CONFIG), )
    FITZ_CONFIG = release
endif
ifeq ($(FITZ_CONFIG_$(FITZ_CONFIG)), )
    $(warning FITZ_CONFIG should be "debug" or "release")
endif
FITZ_CPPFLAGS += $(FITZ_CPPFLAGS_$(FITZ_CONFIG))
FITZ_CFLAGS += $(FITZ_CFLAGS_$(FITZ_CONFIG))
FITZ_LDFLAGS += $(FITZ_LDFLAGS_$(FITZ_CONFIG))

# TODO: precompiled headers?

FITZ_INCLUDE = $(FITZ_DIR)/include
FITZ_INC_DIR += $(FITZ_INCLUDE)
FITZ_CPPFLAGS += $(addprefix -I ,$(FITZ_INC_DIR))
FITZ_RCFLAGS += $(addprefix -I ,$(FITZ_INC_DIR))
FITZ_CFLAGS += -Wall
FITZ_CFLAGS += -g
FITZ_CPPFLAGS += -D NEED_STRLCAT -D NEED_STRLCPY -D NEED_STRSEP
# TODO: use mingw's getopt instead via <unistd.h>; see <fitz/base_sysdep.h>
FITZ_CPPFLAGS += -D NEED_GETOPT
FITZ_CFLAGS += -std=gnu99
FITZ_CPPFLAGS += -D HAVE_C99
# x86 processor detection stuff appears to not be working for GCC under win32
# FITZ_CPPFLAGS += -D ARCH_X86
# FITZ_CPPFLAGS += -D HAVE_JASPER
# FITZ_CPPFLAGS += -D HAVE_JBIG2DEC
FITZ_LDFLAGS += $(addprefix -L ,$(LIB_DIR))

FITZ_BUILD = $(FITZ_DIR)/build/$(FITZ_CONFIG)
$(warning Fitz build directory is $(FITZ_BUILD))

# TODO: automatic header dependancies stuff

fitz: pdftool apparition
.PHONY: fitz

FITZ_BASE_C += base_cpudep.c
FITZ_BASE_C += base_error.c
FITZ_BASE_C += base_hash.c
FITZ_BASE_C += base_matrix.c
FITZ_BASE_C += base_memory.c
FITZ_BASE_C += base_rect.c
FITZ_BASE_C += base_rune.c
# doesn't seem to be used by anything:
FITZ_BASE_C += base_cleanname.c

# Replacements for common functions:
FITZ_BASE_C += util_strlcat.c
FITZ_BASE_C += util_strlcpy.c
FITZ_BASE_C += util_strsep.c
FITZ_BASE_C += util_getopt.c

FITZ_BASE_OBJ = $(addprefix $(FITZ_BUILD)/base/,$(FITZ_BASE_C:.c=.o))
FITZ_BASE_LIB = $(FITZ_BUILD)/base/libbase.a
FITZ_C_OBJ += $(FITZ_BASE_OBJ)
FITZ_PROD += $(FITZ_BASE_OBJ) $(FITZ_BASE_LIB)
FITZ_SUBDIR += base

FITZ_STREAM_C += crypt_arc4.c
FITZ_STREAM_C += crypt_crc32.c
FITZ_STREAM_C += crypt_md5.c

FITZ_STREAM_C += obj_array.c
FITZ_STREAM_C += obj_dict.c
FITZ_STREAM_C += obj_parse.c
FITZ_STREAM_C += obj_print.c
FITZ_STREAM_C += obj_simple.c

FITZ_STREAM_C += stm_buffer.c
FITZ_STREAM_C += stm_filter.c
FITZ_STREAM_C += stm_open.c
FITZ_STREAM_C += stm_read.c
FITZ_STREAM_C += stm_write.c
FITZ_STREAM_C += stm_misc.c

FITZ_STREAM_C += filt_pipeline.c
FITZ_STREAM_C += filt_arc4.c
FITZ_STREAM_C += filt_null.c

FITZ_STREAM_C += filt_a85d.c
FITZ_STREAM_C += filt_a85e.c
FITZ_STREAM_C += filt_ahxd.c
FITZ_STREAM_C += filt_ahxe.c
FITZ_STREAM_C += filt_dctd.c
FITZ_STREAM_C += filt_dcte.c
FITZ_STREAM_C += filt_faxd.c
FITZ_STREAM_C += filt_faxdtab.c
FITZ_STREAM_C += filt_faxe.c
FITZ_STREAM_C += filt_faxetab.c
FITZ_STREAM_C += filt_flate.c
FITZ_STREAM_C += filt_lzwd.c
FITZ_STREAM_C += filt_lzwe.c
FITZ_STREAM_C += filt_predict.c
FITZ_STREAM_C += filt_rld.c
FITZ_STREAM_C += filt_rle.c
# Jasper:
# FITZ_STREAM_C += filt_jpxd.c
# jbig2dec:
# FITZ_STREAM_C += filt_jbig2d.c

FITZ_STREAM_OBJ = $(addprefix $(FITZ_BUILD)/stream/,$(FITZ_STREAM_C:.c=.o))
FITZ_STREAM_LIB = $(FITZ_BUILD)/stream/libstream.a
FITZ_C_OBJ += $(FITZ_STREAM_OBJ)
FITZ_PROD += $(FITZ_STREAM_OBJ) $(FITZ_STREAM_LIB)
FITZ_SUBDIR += stream

FITZ_RASTER_C += glyphcache.c
FITZ_RASTER_C += pixmap.c
FITZ_RASTER_C += porterduff.c
FITZ_RASTER_C += meshdraw.c
FITZ_RASTER_C += imagedraw.c
FITZ_RASTER_C += imageunpack.c
FITZ_RASTER_C += imagescale.c
FITZ_RASTER_C += pathscan.c
FITZ_RASTER_C += pathfill.c
FITZ_RASTER_C += pathstroke.c
FITZ_RASTER_C += render.c
# doesn't seem to be used by anything; has many unused static functions:
# FITZ_RASTER_C += blendmodes.c
# ARCH_X86: (see also archppc and archsparc)
# FITZ_RASTER_C += archx86.c

FITZ_RASTER_OBJ = $(addprefix $(FITZ_BUILD)/raster/,$(FITZ_RASTER_C:.c=.o))
FITZ_RASTER_LIB = $(FITZ_BUILD)/raster/libraster.a
FITZ_C_OBJ += $(FITZ_RASTER_OBJ)
FITZ_PROD += $(FITZ_RASTER_OBJ) $(FITZ_RASTER_LIB)
FITZ_SUBDIR += raster

FITZ_WORLD_C += node_toxml.c
FITZ_WORLD_C += node_misc1.c
FITZ_WORLD_C += node_misc2.c
FITZ_WORLD_C += node_optimize.c
FITZ_WORLD_C += node_path.c
FITZ_WORLD_C += node_text.c
# doesn't compile; missing from jamfile; seems to be replaced by toxml:
# FITZ_WORLD_C += node_tolisp.c
FITZ_WORLD_C += node_tree.c

FITZ_WORLD_C += res_colorspace.c
FITZ_WORLD_C += res_font.c
FITZ_WORLD_C += res_image.c
FITZ_WORLD_C += res_shade.c

FITZ_WORLD_OBJ = $(addprefix $(FITZ_BUILD)/world/,$(FITZ_WORLD_C:.c=.o))
FITZ_WORLD_LIB = $(FITZ_BUILD)/world/libworld.a
FITZ_C_OBJ += $(FITZ_WORLD_OBJ)
FITZ_PROD += $(FITZ_WORLD_OBJ) $(FITZ_WORLD_LIB)
FITZ_SUBDIR += world

# syntax layer
MUPDF_C += pdf_crypt.c
MUPDF_C += pdf_debug.c
MUPDF_C += pdf_doctor.c
MUPDF_C += pdf_lex.c
MUPDF_C += pdf_nametree.c
MUPDF_C += pdf_open.c
MUPDF_C += pdf_parse.c
MUPDF_C += pdf_repair.c
MUPDF_C += pdf_save.c
MUPDF_C += pdf_stream.c
MUPDF_C += pdf_xref.c

# metadata layer
MUPDF_C += pdf_annot.c
MUPDF_C += pdf_outline.c

# fonts
MUPDF_C += pdf_fontagl.c
MUPDF_C += pdf_fontenc.c
MUPDF_C += pdf_cmap.c
MUPDF_C += pdf_unicode.c
MUPDF_C += pdf_font.c
MUPDF_C += pdf_type3.c

ifeq ($(FITZ_BUILTIN_FONTS), yes)
    # use built-in fonts:
    MUPDF_C += pdf_fontfile.c
else
    # use fontconfig (apparently not for win32):
    # MUPDF_C += pdf_fontfilefc.c
    # scan $WINDIR/Fonts:
    MUPDF_C += pdf_fontfilems.c
endif

# other resources
MUPDF_C += pdf_function.c
MUPDF_C += pdf_colorspace1.c
MUPDF_C += pdf_colorspace2.c
MUPDF_C += pdf_image.c
MUPDF_C += pdf_pattern.c
MUPDF_C += pdf_shade.c
MUPDF_C += pdf_shade1.c
MUPDF_C += pdf_shade4.c
MUPDF_C += pdf_xobject.c

# pages, resource dictionaries, . . .
MUPDF_C += pdf_build.c
MUPDF_C += pdf_interpret.c
MUPDF_C += pdf_page.c
MUPDF_C += pdf_pagetree.c
MUPDF_C += pdf_resources.c
MUPDF_C += pdf_store.c

MUPDF_OBJ = $(addprefix $(FITZ_BUILD)/mupdf/,$(MUPDF_C:.c=.o))
MUPDF_LIB = $(FITZ_BUILD)/mupdf/libmupdf.a
FITZ_C_OBJ += $(MUPDF_OBJ)
FITZ_PROD += $(MUPDF_OBJ) $(MUPDF_LIB)
FITZ_SUBDIR += mupdf

MUPDF_FONTS += Dingbats.cff
MUPDF_FONTS += StandardSymL.cff
MUPDF_FONTS += URWChanceryL-MediItal.cff
MUPDF_FONTS += NimbusMonL-Bold.cff
MUPDF_FONTS += NimbusMonL-BoldObli.cff
MUPDF_FONTS += NimbusMonL-Regu.cff
MUPDF_FONTS += NimbusMonL-ReguObli.cff
MUPDF_FONTS += NimbusRomNo9L-Medi.cff
MUPDF_FONTS += NimbusRomNo9L-MediItal.cff
MUPDF_FONTS += NimbusRomNo9L-Regu.cff
MUPDF_FONTS += NimbusRomNo9L-ReguItal.cff
MUPDF_FONTS += NimbusSanL-Bold.cff
MUPDF_FONTS += NimbusSanL-BoldItal.cff
MUPDF_FONTS += NimbusSanL-Regu.cff
MUPDF_FONTS += NimbusSanL-ReguItal.cff

MUPDF_FONTS_OBJ = \
    $(addprefix $(FITZ_BUILD)/fonts/,$(addsuffix .o,$(MUPDF_FONTS)))
MUPDF_FONTS_LIB = $(FITZ_BUILD)/fonts/libfonts.a
FITZ_PROD += $(MUPDF_FONTS_OBJ) $(MUPDF_FONTS_LIB)
FITZ_SUBDIR += fonts

MUPDF_LIBS += $(MUPDF_LIB) $(FITZ_BASE_LIB) $(FITZ_STREAM_LIB)
MUPDF_LIBS += $(FITZ_RASTER_LIB) $(FITZ_WORLD_LIB)
ifeq ($(FITZ_BUILTIN_FONTS), yes)
    MUPDF_LIBS += $(MUPDF_FONTS_LIB)
endif
MUPDF_LDLIBS = -lfreetype -lz -ljpeg

PDFTOOL_OBJ = $(FITZ_BUILD)/apps/pdftool.o
PDFTOOL_EXE = $(FITZ_BUILD)/apps/pdftool.exe
LDLIBS_$(notdir $(PDFTOOL_EXE)) += $(MUPDF_LDLIBS) 
FITZ_C_OBJ += $(PDFTOOL_OBJ)
FITZ_PROD += $(PDFTOOL_OBJ) $(PDFTOOL_EXE)
FITZ_SUBDIR += apps

APPARITION_LIB_C += pdfapp.c
APPARITION_LIB_OBJ = \
    $(addprefix $(FITZ_BUILD)/apps/common/,$(APPARITION_LIB_C:.c=.o))
APPARITION_LIB = $(FITZ_BUILD)/apps/common/libpdfapp.a
FITZ_C_OBJ += $(APPARITION_LIB_OBJ)
FITZ_PROD += $(APPARITION_LIB_OBJ) $(APPARITION_LIB)
FITZ_SUBDIR += apps/common

APPARITION_C_OBJ = $(FITZ_BUILD)/apps/windows/winmain.o
APPARITION_RES_OBJ = $(FITZ_BUILD)/apps/windows/winres.o
APPARITION_OBJ = $(APPARITION_C_OBJ) $(APPARITION_RES_OBJ)
APPARITION_EXE = $(FITZ_BUILD)/apps/windows/apparition.exe
LDFLAGS_$(notdir $(APPARITION_EXE)) += $(APPARITION_LDFLAGS_$(FITZ_CONFIG))
LDLIBS_$(notdir $(APPARITION_EXE)) += -lcomdlg32 -lgdi32 $(MUPDF_LDLIBS) 
FITZ_C_OBJ += $(APPARITION_C_OBJ)
FITZ_PROD += $(APPARITION_OBJ) $(APPARITION_EXE)
FITZ_SUBDIR += apps/windows

pdftool: $(PDFTOOL_EXE)
apparition: $(APPARITION_EXE)
.PHONY: pdftool apparition

.SUFFIXES:

define FITZ_LD_CMD
    @echo CC $(LDFLAGS_$(notdir $@)) -o $@ $(LDLIBS_$(notdir $@))
    @$(CC) $(FITZ_LDFLAGS) $(LDFLAGS_$(notdir $@)) -o $@ $^ \
        $(LDLIBS_$(notdir $@)) $(FITZ_LDLIBS)
endef
$(PDFTOOL_EXE): $(PDFTOOL_OBJ) $(MUPDF_LIBS)
	$(FITZ_LD_CMD)
$(APPARITION_EXE): $(APPARITION_OBJ) $(APPARITION_LIB) $(MUPDF_LIBS)
	$(FITZ_LD_CMD)

define FITZ_AR_CMD
    @echo AR r $@
    @$(AR) r $@ $?
endef
$(FITZ_BASE_LIB): $(FITZ_BASE_OBJ)
	$(FITZ_AR_CMD)
$(FITZ_STREAM_LIB): $(FITZ_STREAM_OBJ)
	$(FITZ_AR_CMD)
$(FITZ_RASTER_LIB): $(FITZ_RASTER_OBJ)
	$(FITZ_AR_CMD)
$(FITZ_WORLD_LIB): $(FITZ_WORLD_OBJ)
	$(FITZ_AR_CMD)
$(MUPDF_LIB): $(MUPDF_OBJ)
	$(FITZ_AR_CMD)
$(MUPDF_FONTS_LIB): $(MUPDF_FONTS_OBJ)
	$(FITZ_AR_CMD)
$(APPARITION_LIB): $(APPARITION_LIB_OBJ)
	$(FITZ_AR_CMD)

$(FITZ_BASE_OBJ): $(FITZ_BUILD)/base
$(FITZ_STREAM_OBJ): $(FITZ_BUILD)/stream
$(FITZ_RASTER_OBJ): $(FITZ_BUILD)/raster
$(FITZ_WORLD_OBJ): $(FITZ_BUILD)/world
$(MUPDF_OBJ): $(FITZ_BUILD)/mupdf
$(MUPDF_FONTS_OBJ): $(FITZ_BUILD)/fonts
$(APPARITION_LIB_OBJ): $(FITZ_BUILD)/apps/common
$(PDFTOOL_OBJ): $(FITZ_BUILD)/apps
$(APPARITION_OBJ): $(FITZ_BUILD)/apps/windows

$(MUPDF_FONTS_OBJ): $(FITZ_BUILD)/%.o: $(FITZ_DIR)/%
	@echo XXD -i $< "|" SED "|" CC -c
	@(cd $(FITZ_DIR) && xxd -i $*) | \
	    sed -e "s/unsigned/const unsigned/" | \
	    $(CC) -c $(FITZ_CPPFLAGS) $(FITZ_CFLAGS) -o $@ -x c -

$(APPARITION_RES_OBJ): $(FITZ_BUILD)/%.o: $(FITZ_DIR)/%.rc
	windres $(FITZ_RCFLAGS) -I $(dir $<) $< $@

$(FITZ_C_OBJ): $(FITZ_BUILD)/%.o: $(FITZ_DIR)/%.c
	@echo CC -c $<
	@$(CC) -c $(FITZ_CPPFLAGS) $(FITZ_CFLAGS) -o $@ $<

$(addprefix $(FITZ_BUILD)/,$(FITZ_SUBDIR)):
	mkdir -p $@

clean-fitz:
	@echo CLEAN $(FITZ_BUILD)
	@rm -f $(FITZ_PROD)
realclean-fitz:
	rm -rf $(FITZ_BUILD) || rmdir $(FITZ_BUILD)
.PHONY: clean-fitz realclean-fitz
