# GNU make file configuration

# Build configuration name
#BUILD = debug
BUILD = release

# Where to find zlib, IJG JPEG, Free Type, JasPer, JBIG2dec libraries. They
# must be pre-built.
INC_DIR =
LIB_DIR =

# FITZ_BUILTIN_FONTS = yes
FITZ_JASPER = yes
FITZ_JBIG2DEC = yes
FITZ_STATIC_CMAPS = yes
FITZ_DIR = $(SUMATRA_DIR)/fitz
# SUMATRA_PDFSYNC_GUI = yes
SUMATRA_SYNCTEX = yes

# Python is optional, but use it if it is found:
#SUMATRA_PYTHON =
SUMATRA_PYTHON = $(SYSTEMDRIVE)/Python25/python
