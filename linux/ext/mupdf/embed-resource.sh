#!/bin/sh
# Embed a binary resource file as a linkable ELF object.
# Usage: embed-resource.sh <mupdf_root> <relative_resource_path> <output.o>
# Symbols are derived from the relative path (matching MuPDF's Makefile convention).

MUPDF_ROOT="$1"
RESOURCE_REL="$2"
OUTPUT="$(realpath -m "$3")"

cd "$MUPDF_ROOT" && ld -r -b binary -z noexecstack -o "$OUTPUT" "$RESOURCE_REL"
