# GNU make file configuration

# Build configuration name
#BUILD = debug
BUILD = release

# Where to find zlib, JPEG library and Free Type
# The pre-compiled libraries in subversion have Visual C dependencies
INC_DIR =
LIB_DIR =

# Python is optional, but use it if it is found:
#SUMATRA_PYTHON =
SUMATRA_PYTHON = $(SYSTEMDRIVE)/Python25/python
