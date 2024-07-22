/*
 Copyright (c) 2008-2017 jerome DOT laurens AT u-bourgogne DOT fr
 
 This file is part of the __SyncTeX__ package.
 
 [//]: # (Latest Revision: Sun Oct 15 15:09:55 UTC 2017)
 [//]: # (Version: 1.21)
 
 See `synctex_parser_readme.md` for more details
 
 ## License
 
 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation
 files (the "Software"), to deal in the Software without
 restriction, including without limitation the rights to use,
 copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following
 conditions:
 
 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE
 
 Except as contained in this notice, the name of the copyright holder
 shall not be used in advertising or otherwise to promote the sale,
 use or other dealings in this Software without prior written
 authorization from the copyright holder.
 
 Acknowledgments:
 ----------------
 The author received useful remarks from the pdfTeX developers, especially Hahn The Thanh,
 and significant help from XeTeX developer Jonathan Kew
 
 Nota Bene:
 ----------
 If you include or use a significant part of the synctex package into a software,
 I would appreciate to be listed as contributor and see "SyncTeX" highlighted.
 
 */

/*  We assume that high level application like pdf viewers will want
 *  to embed this code as is. We assume that they also have locale.h and setlocale.
 *  For other tools such as TeXLive tools, you must define SYNCTEX_USE_LOCAL_HEADER,
 *  when building. You also have to create and customize synctex_parser_local.h to fit your system.
 *  In particular, the HAVE_LOCALE_H and HAVE_SETLOCALE macros should be properly defined.
 *  With this design, you should not need to edit this file. */

/**
 *  \file synctex_parser.c
 *  \brief SyncTeX file parser and controller.
 *  - author: Jérôme LAURENS
 *  \version 1.21
 *  \date Sun Oct 15 15:09:55 UTC 2017
 *
 *  Reads and parse *.synctex[.gz] files,
 *  performs edit and display queries.
 *
 *  See
 *  - synctex_scanner_new_with_output_file
 *  - synctex_scanner_parse
 *  - synctex_scanner_free
 *  - synctex_display_query
 *  - synctex_edit_query
 *  - synctex_scanner_next_result
 *  - synctex_scanner_reset_result
 *
 *  The data is organized in a graph with multiple entries.
 *  The root object is a scanner, it is created with the contents on a synctex file.
 *  Each node of the tree is a synctex_node_t object.
 *  There are 3 subtrees, two of them sharing the same leaves.
 *  The first tree is the list of input records, where input file names are associated with tags.
 *  The second tree is the box tree as given by TeX when shipping pages out.
 *  First level objects are sheets and forms, containing boxes, glues, kerns...
 *  The third tree allows to browse leaves according to tag and line.
 */
/* Declare _GNU_SOURCE for accessing vasprintf. For MSC compiler, vasprintf is
 * defined in this file
 */
#define _GNU_SOURCE

#   if defined(SYNCTEX_USE_LOCAL_HEADER)
#       include "synctex_parser_local.h"
#   else
#       define HAVE_LOCALE_H 1
#       define HAVE_SETLOCALE 1
#       if defined(_MSC_VER)
#          define SYNCTEX_INLINE __inline
#       else
#          define SYNCTEX_INLINE inline
#       endif
#   endif

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#if defined(HAVE_LOCALE_H)
#include <locale.h>
#endif

/* Mark unused parameters, so that there will be no compile warnings. */
#ifdef __DARWIN_UNIX03
#   define SYNCTEX_UNUSED(x) SYNCTEX_PRAGMA(unused(x))
#   define SYNCTEX_PRAGMA(x) _Pragma ( #x )
#else
#   define SYNCTEX_UNUSED(x) (void)(x);
#endif

#include "synctex_parser_advanced.h"

SYNCTEX_INLINE static int _synctex_abs(int x) {
    return x>0? x: -x;
}
/*  These are the possible extensions of the synctex file */
const char * synctex_suffix = ".synctex";
const char * synctex_suffix_gz = ".gz";

typedef synctex_node_p(*synctex_node_new_f)(synctex_scanner_p);
typedef void(*synctex_node_fld_f)(synctex_node_p);
typedef char *(*synctex_node_str_f)(synctex_node_p);

/**
 *  Pseudo class.
 *  - author: J. Laurens
 *
 *  Each nodes has a class, it is therefore called an object.
 *  Each class has a unique scanner.
 *  Each class has a type which is a unique identifier.
 *  The class points to various methods,
 *  each of them vary amongst objects.
 *  Each class has a data model which stores node's attributes.
 *  Each class has an tree model which stores children and parent.
 *  Inspectors give access to data and tree elements.
 */

/*  8 fields + size: spcflnat */
typedef struct synctex_tree_model_t {
    int sibling;
    int parent;
    int child;
    int friend;
    int last;
    int next_hbox;
    int arg_sibling;
    int target;
    int size;
} synctex_tree_model_s;
typedef const synctex_tree_model_s * synctex_tree_model_p;

typedef struct synctex_data_model_t {
    int tag;
    int line;
    int column;
    int h;
    int v;
    int width;
    int height;
    int depth;
    int mean_line;
    int weight;
    int h_V;
    int v_V;
    int width_V;
    int height_V;
    int depth_V;
    int name;
    int page;
    int size;
} synctex_data_model_s;

typedef const synctex_data_model_s * synctex_data_model_p;

typedef int (*synctex_int_getter_f)(synctex_node_p);
typedef struct synctex_tlcpector_t {
    synctex_int_getter_f tag;
    synctex_int_getter_f line;
    synctex_int_getter_f column;
} synctex_tlcpector_s;
typedef const synctex_tlcpector_s * synctex_tlcpector_p;
static int _synctex_int_none(synctex_node_p node) {
    SYNCTEX_UNUSED(node)
    return 0;
}
static const synctex_tlcpector_s synctex_tlcpector_none = {
    &_synctex_int_none, /* tag */
    &_synctex_int_none, /* line */
    &_synctex_int_none, /* column */
};

typedef struct synctex_inspector_t {
    synctex_int_getter_f h;
    synctex_int_getter_f v;
    synctex_int_getter_f width;
    synctex_int_getter_f height;
    synctex_int_getter_f depth;
} synctex_inspector_s;
typedef const synctex_inspector_s * synctex_inspector_p;
static const synctex_inspector_s synctex_inspector_none = {
    &_synctex_int_none, /* h */
    &_synctex_int_none, /* v */
    &_synctex_int_none, /* width */
    &_synctex_int_none, /* height */
    &_synctex_int_none, /* depth */
};

typedef float (*synctex_float_getter_f)(synctex_node_p);
typedef struct synctex_vispector_t {
    synctex_float_getter_f h;
    synctex_float_getter_f v;
    synctex_float_getter_f width;
    synctex_float_getter_f height;
    synctex_float_getter_f depth;
} synctex_vispector_s;
static float _synctex_float_none(synctex_node_p node) {
    SYNCTEX_UNUSED(node)
    return 0;
}
static const synctex_vispector_s synctex_vispector_none = {
    &_synctex_float_none, /* h */
    &_synctex_float_none, /* v */
    &_synctex_float_none, /* width */
    &_synctex_float_none, /* height */
    &_synctex_float_none, /* depth */
};
typedef const synctex_vispector_s * synctex_vispector_p;

struct synctex_class_t {
    synctex_scanner_p scanner;
    synctex_node_type_t type;
    synctex_node_new_f new;
    synctex_node_fld_f free;
    synctex_node_fld_f log;
    synctex_node_fld_f display;
    synctex_node_str_f abstract;
    synctex_tree_model_p navigator;
    synctex_data_model_p modelator;
    synctex_tlcpector_p tlcpector;
    synctex_inspector_p inspector;
    synctex_vispector_p vispector;
};

/**
 *  Nota bene: naming convention.
 *  For static API, when the name contains "proxy", it applies to proxies.
 *  When the name contains "noxy", it applies to non proxies only.
 *  When the name contains "node", well it depends...
 */

typedef synctex_node_p synctex_proxy_p;
typedef synctex_node_p synctex_noxy_p;

#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark Abstract OBJECTS and METHODS
#   endif

/**
 *  \def SYNCTEX_MSG_SEND
 *  \brief Takes care of sending the given message if possible.
 *  - parameter NODE: of type synctex_node_p
 *  - parameter SELECTOR: one of the class_ pointer properties
 */
#   define SYNCTEX_MSG_SEND(NODE,SELECTOR) do {\
    synctex_node_p N__ = NODE;\
    if (N__ && N__->class_->SELECTOR) {\
        (*(N__->class_->SELECTOR))(N__);\
    }\
} while (synctex_NO)

/**
 *  Free the given node by sending the free message.
 *  - parameter NODE: of type synctex_node_p
 */
static void synctex_node_free(synctex_node_p node) {
    SYNCTEX_MSG_SEND(node,free);
}
#   if defined(SYNCTEX_TESTING)
#       if !defined(SYNCTEX_USE_HANDLE)
#           define SYNCTEX_USE_HANDLE 1
#       endif
#       if !defined(SYNCTEX_USE_CHARINDEX)
#           define SYNCTEX_USE_CHARINDEX 1
#       endif
#   endif
SYNCTEX_INLINE static synctex_node_p _synctex_new_handle_with_target(synctex_node_p target);
#   if defined(SYNCTEX_USE_HANDLE)
#       define SYNCTEX_SCANNER_FREE_HANDLE(SCANR) \
__synctex_scanner_free_handle(SCANR)
#       define SYNCTEX_SCANNER_REMOVE_HANDLE_TO(WHAT) \
__synctex_scanner_remove_handle_to(WHAT)
#       define SYNCTEX_REGISTER_HANDLE_TO(NODE) \
__synctex_scanner_register_handle_to(NODE)
#   else
#       define SYNCTEX_SCANNER_FREE_HANDLE(SCANR)
#       define SYNCTEX_SCANNER_REMOVE_HANDLE_TO(WHAT)
#       define SYNCTEX_REGISTER_HANDLE_TO(NODE)
#   endif

#   if defined(SYNCTEX_USE_CHARINDEX)
#       define SYNCTEX_CHARINDEX(NODE) (NODE->char_index)
#       define SYNCTEX_LINEINDEX(NODE) (NODE->line_index)
#       define SYNCTEX_PRINT_CHARINDEX_FMT "#%i"
#       define SYNCTEX_PRINT_CHARINDEX_WHAT ,SYNCTEX_CHARINDEX(node)
#       define SYNCTEX_PRINT_CHARINDEX \
            printf(SYNCTEX_PRINT_CHARINDEX_FMT SYNCTEX_PRINT_CHARINDEX_WHAT)
#       define SYNCTEX_PRINT_LINEINDEX_FMT "L#%i"
#       define SYNCTEX_PRINT_LINEINDEX_WHAT ,SYNCTEX_LINEINDEX(node)
#       define SYNCTEX_PRINT_LINEINDEX \
            printf(SYNCTEX_PRINT_LINEINDEX_FMT SYNCTEX_PRINT_LINEINDEX_WHAT)
#       define SYNCTEX_PRINT_CHARINDEX_NL \
            printf(SYNCTEX_PRINT_CHARINDEX_FMT "\n" SYNCTEX_PRINT_CHARINDEX_WHAT)
#       define SYNCTEX_PRINT_LINEINDEX_NL \
            printf(SYNCTEX_PRINT_CHARINDEX_FMT "\n"SYNCTEX_PRINT_LINEINDEX_WHAT)
#       define SYNCTEX_IMPLEMENT_CHARINDEX(NODE,CORRECTION)\
            NODE->char_index = (synctex_charindex_t)(scanner->reader->charindex_offset+SYNCTEX_CUR-SYNCTEX_START+(CORRECTION)); \
            NODE->line_index = scanner->reader->line_number;
#   else
#       define SYNCTEX_CHARINDEX(NODE) 0
#       define SYNCTEX_LINEINDEX(NODE) 0
#       define SYNCTEX_PRINT_CHARINDEX_FMT
#       define SYNCTEX_PRINT_CHARINDEX_WHAT
#       define SYNCTEX_PRINT_CHARINDEX
#       define SYNCTEX_PRINT_CHARINDEX
#       define SYNCTEX_PRINT_LINEINDEX_FMT
#       define SYNCTEX_PRINT_LINEINDEX_WHAT
#       define SYNCTEX_PRINT_LINEINDEX
#       define SYNCTEX_PRINT_CHARINDEX_NL printf("\n")
#       define SYNCTEX_PRINT_LINEINDEX_NL printf("\n")
#       define SYNCTEX_IMPLEMENT_CHARINDEX(NODE,CORRECTION)
#   endif

/**
 *  The next macros are used to access the node tree info
 *  SYNCTEX_DATA(node) points to the first synctex integer or pointer data of node
 *  SYNCTEX_DATA(node)[index] is the information at index
 *  for example, the page of a sheet is stored in SYNCTEX_DATA(sheet)[_synctex_data_page_idx]
 *  - parameter NODE: of type synctex_node_p
 *  If the name starts with "__", the argument is nonullable
 */
#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark Tree SETGET
#   endif

#if SYNCTEX_DEBUG > 1000
#define SYNCTEX_PARAMETER_ASSERT(WHAT) \
    do { \
        if (!(WHAT)) { \
            printf("! Parameter failure: %s\n",#WHAT); \
        } \
    } while (synctex_NO)
#define DEFINE_SYNCTEX_TREE_HAS(WHAT)\
static synctex_bool_t _synctex_tree_has_##WHAT(synctex_node_p node) {\
    if (node) {\
        if (node->class_->navigator->WHAT>=0) {\
            return synctex_YES; \
        } else {\
            printf("WARNING: NO tree %s for %s\n", #WHAT, synctex_node_isa(node));\
        }\
    }\
    return synctex_NO;\
}
#else
#   define SYNCTEX_PARAMETER_ASSERT(WHAT)
#   define DEFINE_SYNCTEX_TREE_HAS(WHAT) \
SYNCTEX_INLINE static synctex_bool_t _synctex_tree_has_##WHAT(synctex_node_p node) {\
    return (node && (node->class_->navigator->WHAT>=0));\
}
#endif

#   define DEFINE_SYNCTEX_TREE__GET(WHAT) \
SYNCTEX_INLINE static synctex_node_p __synctex_tree_##WHAT(synctex_non_null_node_p node) {\
    return node->data[node->class_->navigator->WHAT].as_node;\
}
#   define DEFINE_SYNCTEX_TREE_GET(WHAT) \
DEFINE_SYNCTEX_TREE__GET(WHAT) \
SYNCTEX_INLINE static synctex_node_p _synctex_tree_##WHAT(synctex_node_p node) {\
    if (_synctex_tree_has_##WHAT(node)) {\
        return __synctex_tree_##WHAT(node);\
    }\
    return 0;\
}
#   define DEFINE_SYNCTEX_TREE__RESET(WHAT) \
SYNCTEX_INLINE static synctex_node_p __synctex_tree_reset_##WHAT(synctex_non_null_node_p node) {\
    synctex_node_p old = node->data[node->class_->navigator->WHAT].as_node;\
    node->data[node->class_->navigator->WHAT].as_node=NULL;\
    return old;\
}
#   define DEFINE_SYNCTEX_TREE_RESET(WHAT) \
DEFINE_SYNCTEX_TREE__RESET(WHAT) \
SYNCTEX_INLINE static synctex_node_p _synctex_tree_reset_##WHAT(synctex_node_p node) {\
        return _synctex_tree_has_##WHAT(node)? \
            __synctex_tree_reset_##WHAT(node): NULL; \
}
#   define DEFINE_SYNCTEX_TREE__SET(WHAT) \
SYNCTEX_INLINE static synctex_node_p __synctex_tree_set_##WHAT(synctex_non_null_node_p node, synctex_node_p new_value) {\
    synctex_node_p old = __synctex_tree_##WHAT(node);\
    node->data[node->class_->navigator->WHAT].as_node=new_value;\
    return old;\
}
#   define DEFINE_SYNCTEX_TREE_SET(WHAT) \
DEFINE_SYNCTEX_TREE__SET(WHAT) \
SYNCTEX_INLINE static synctex_node_p _synctex_tree_set_##WHAT(synctex_node_p node, synctex_node_p new_value) {\
    return _synctex_tree_has_##WHAT(node)?\
        __synctex_tree_set_##WHAT(node,new_value):NULL;\
}
#   define DEFINE_SYNCTEX_TREE__GETSETRESET(WHAT) \
DEFINE_SYNCTEX_TREE__GET(WHAT) \
DEFINE_SYNCTEX_TREE__SET(WHAT) \
DEFINE_SYNCTEX_TREE__RESET(WHAT)

#   define DEFINE_SYNCTEX_TREE_GETSET(WHAT) \
DEFINE_SYNCTEX_TREE_HAS(WHAT) \
DEFINE_SYNCTEX_TREE_GET(WHAT) \
DEFINE_SYNCTEX_TREE_SET(WHAT)

#   define DEFINE_SYNCTEX_TREE_GETRESET(WHAT) \
DEFINE_SYNCTEX_TREE_HAS(WHAT) \
DEFINE_SYNCTEX_TREE_GET(WHAT) \
DEFINE_SYNCTEX_TREE_RESET(WHAT)

#   define DEFINE_SYNCTEX_TREE_GETSETRESET(WHAT) \
DEFINE_SYNCTEX_TREE_HAS(WHAT) \
DEFINE_SYNCTEX_TREE_GET(WHAT) \
DEFINE_SYNCTEX_TREE_SET(WHAT) \
DEFINE_SYNCTEX_TREE_RESET(WHAT)

/*
 *  _synctex_tree_set_... methods return the old value.
 *  The return value of _synctex_tree_set_child and 
 *  _synctex_tree_set_sibling must be released somehow.
 */
/* The next macro call creates:
 SYNCTEX_INLINE static synctex_node_p __synctex_tree_sibling(synctex_node_p node)
 SYNCTEX_INLINE static synctex_node_p __synctex_tree_set_sibling(synctex_node_p node, synctex_node_p new_value)
 SYNCTEX_INLINE static synctex_node_p __synctex_tree_reset_sibling(synctex_node_p node)
 */
DEFINE_SYNCTEX_TREE__GETSETRESET(sibling)
/* The next macro call creates:
 SYNCTEX_INLINE static synctex_bool_t _synctex_tree_has_parent(synctex_node_p node);
 SYNCTEX_INLINE static synctex_node_p __synctex_tree_parent(synctex_non_null_node_p node);
 SYNCTEX_INLINE static synctex_node_p _synctex_tree_parent(synctex_node_p node);
 SYNCTEX_INLINE static synctex_node_p __synctex_tree_set_parent(synctex_node_p node, synctex_node_p new_value);
 SYNCTEX_INLINE static synctex_node_p _synctex_tree_set_parent(synctex_node_p node, synctex_node_p new_value);
 SYNCTEX_INLINE static synctex_node_p __synctex_tree_reset_parent(synctex_node_p node);
 SYNCTEX_INLINE static synctex_node_p _synctex_tree_reset_parent(synctex_node_p node);
 */
DEFINE_SYNCTEX_TREE_GETSETRESET(parent)
DEFINE_SYNCTEX_TREE_GETSETRESET(child)
DEFINE_SYNCTEX_TREE_GETSETRESET(friend)
/* The next macro call creates:
 SYNCTEX_INLINE static synctex_bool_t _synctex_tree_has_last(synctex_node_p node);
 SYNCTEX_INLINE static synctex_node_p __synctex_tree_last(synctex_non_null_node_p node);
 SYNCTEX_INLINE static synctex_node_p _synctex_tree_last(synctex_node_p node);
 SYNCTEX_INLINE static synctex_node_p __synctex_tree_set_last(synctex_node_p node, synctex_node_p new_value);
 SYNCTEX_INLINE static synctex_node_p _synctex_tree_set_last(synctex_node_p node, synctex_node_p new_value);
 */
DEFINE_SYNCTEX_TREE_GETSET(last)
DEFINE_SYNCTEX_TREE_GETSET(next_hbox)
DEFINE_SYNCTEX_TREE_GETSET(arg_sibling)
DEFINE_SYNCTEX_TREE_GETSETRESET(target)

#if SYNCTEX_DEBUG>1000
#   undef SYNCTEX_USE_NODE_COUNT
#   define SYNCTEX_USE_NODE_COUNT 1
#endif
#if SYNCTEX_USE_NODE_COUNT>0
#   define SYNCTEX_DECLARE_NODE_COUNT int node_count;
#   define SYNCTEX_INIT_NODE_COUNT \
        do { node_count = 0; } while(synctex_NO)
#else
#   define SYNCTEX_DECLARE_NODE_COUNT
#   define SYNCTEX_INIT_NODE_COUNT
#endif

#if SYNCTEX_USE_NODE_COUNT>10
#   define SYNCTEX_DID_NEW(N)   _synctex_did_new(N)
#   define SYNCTEX_WILL_FREE(N) _synctex_will_free(N)
#else
#   define SYNCTEX_DID_NEW(N)
#   define SYNCTEX_WILL_FREE(N)
#endif

#define SYNCTEX_HAS_CHILDREN(NODE) (NODE && _synctex_tree_child(NODE))
#	ifdef	__SYNCTEX_WORK__
#		include "/usr/local/include/node/zlib.h"
#	else
#		include <zlib.h>
#	endif

#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark STATUS
#   endif
/*  When the end of the synctex file has been reached: */
#   define SYNCTEX_STATUS_EOF 0
/*  When the function could not return the value it was asked for: */
#   define SYNCTEX_STATUS_NOT_OK (SYNCTEX_STATUS_EOF+1)
/*  When the function returns the value it was asked for:
 It must be the biggest one */
#   define SYNCTEX_STATUS_OK (SYNCTEX_STATUS_NOT_OK+1)
/*  Generic error: */
#   define SYNCTEX_STATUS_ERROR (SYNCTEX_STATUS_EOF-1)
/*  Parameter error: */
#   define SYNCTEX_STATUS_BAD_ARGUMENT (SYNCTEX_STATUS_ERROR-1)

#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark File reader
#   endif

/*  We ensure that SYNCTEX_BUFFER_SIZE < UINT_MAX, I don't know if it makes sense... */
/*  Actually, the minimum buffer size is driven by integer and float parsing, including the unit.
 *  ±0.123456789e123??
 */
#   define SYNCTEX_BUFFER_MIN_SIZE 32
#   define SYNCTEX_BUFFER_SIZE 32768

#if SYNCTEX_BUFFER_SIZE >= UINT_MAX
#   error BAD BUFFER SIZE(1)
#endif
#if SYNCTEX_BUFFER_SIZE < SYNCTEX_BUFFER_MIN_SIZE
#   error BAD BUFFER SIZE(2)
#endif

typedef struct synctex_reader_t {
    gzFile file;    /*  The (possibly compressed) file */
    char * output;
    char * synctex;
    char * current; /*  current location in the buffer */
    char * start;   /*  start of the buffer */
    char * end;     /*  end of the buffer */
    size_t min_size;
    size_t size;
    int lastv;
    int line_number;
    SYNCTEX_DECLARE_CHAR_OFFSET
} synctex_reader_s;

typedef synctex_reader_s * synctex_reader_p;

typedef struct {
    synctex_status_t status;
    char * synctex;
    gzFile file;
    synctex_io_mode_t io_mode;
} synctex_open_s;

/*	This functions opens the file at the "output" given location.
 *  It manages the problem of quoted filenames that appear with pdftex and filenames containing the space character.
 *  In TeXLive 2008, the synctex file created with pdftex did contain unexpected quotes.
 *	This function will remove them if possible.
 *  All the reference arguments will take a value on return. They must be non NULL.
 *	- returns: an open structure which status is
 *      SYNCTEX_STATUS_OK on success,
 *      SYNCTEX_STATUS_ERROR on failure.
 *  - note: on success, the caller is the owner
 *      of the fields of the returned open structure.
 */
static synctex_open_s __synctex_open_v2(const char * output, synctex_io_mode_t io_mode, synctex_bool_t add_quotes) {
    synctex_open_s open = {SYNCTEX_STATUS_ERROR, NULL, NULL, io_mode};
    char * quoteless_synctex_name = NULL;
    const char * mode = _synctex_get_io_mode_name(open.io_mode);
    size_t size = strlen(output)+strlen(synctex_suffix)+strlen(synctex_suffix_gz)+1;
    if (NULL == (open.synctex = (char *)malloc(size))) {
        _synctex_error("!  __synctex_open_v2: Memory problem (1)\n");
        return open;
    }
    /*  we have reserved for synctex enough memory to copy output (including its 2 eventual quotes), both suffices,
     *  including the terminating character. size is free now. */
    if (open.synctex != strcpy(open.synctex,output)) {
        _synctex_error("!  __synctex_open_v2: Copy problem\n");
    return_on_error:
        free(open.synctex);
        open.synctex = NULL;
        free(quoteless_synctex_name);/* We MUST have quoteless_synctex_name<>synctex_name */
        return open;
    }
    /*  remove the last path extension if any */
    _synctex_strip_last_path_extension(open.synctex);
    if (!strlen(open.synctex)) {
        goto return_on_error;
    }
    /*  now insert quotes. */
    if (add_quotes) {
        char * quoted = NULL;
        if (_synctex_copy_with_quoting_last_path_component(open.synctex,&quoted,size) || quoted == NULL) {
            /*	There was an error or quoting does not make sense: */
            goto return_on_error;
        }
        quoteless_synctex_name = open.synctex;
        open.synctex = quoted;
    }
    /*	Now add to open.synctex the first path extension. */
    if (open.synctex != strcat(open.synctex,synctex_suffix)){
        _synctex_error("!  __synctex_open_v2: Concatenation problem (can't add suffix '%s')\n",synctex_suffix);
        goto return_on_error;
    }
    /*	Add to quoteless_synctex_name as well, if relevant. */
    if (quoteless_synctex_name && (quoteless_synctex_name != strcat(quoteless_synctex_name,synctex_suffix))){
        free(quoteless_synctex_name);
        quoteless_synctex_name = NULL;
    }
    if (NULL == (open.file = gzopen(open.synctex,mode))) {
        /*  Could not open this file */
        if (errno != ENOENT) {
            /*  The file does exist, this is a lower level error, I can't do anything. */
            _synctex_error("could not open %s, error %i\n",open.synctex,errno);
            goto return_on_error;
        }
        /*  Apparently, there is no uncompressed synctex file. Try the compressed version */
        if (open.synctex != strcat(open.synctex,synctex_suffix_gz)){
            _synctex_error("!  __synctex_open_v2: Concatenation problem (can't add suffix '%s')\n",synctex_suffix_gz);
            goto return_on_error;
        }
        open.io_mode |= synctex_io_gz_mask;
        mode = _synctex_get_io_mode_name(open.io_mode); /* the file is a compressed and is a binary file, this caused errors on Windows */
        /*	Add the suffix to the quoteless_synctex_name as well. */
        if (quoteless_synctex_name && (quoteless_synctex_name != strcat(quoteless_synctex_name,synctex_suffix_gz))){
            free(quoteless_synctex_name);
            quoteless_synctex_name = NULL;
        }
        if (NULL == (open.file = gzopen(open.synctex,mode))) {
            /*  Could not open this file */
            if (errno != ENOENT) {
                /*  The file does exist, this is a lower level error, I can't do anything. */
                _synctex_error("Could not open %s, error %i\n",open.synctex,errno);
            }
            goto return_on_error;
        }
    }
    /*	At this point, the file is properly open.
     *  If we are in the add_quotes mode, we change the file name by removing the quotes. */
    if (quoteless_synctex_name) {
        gzclose(open.file);
        if (rename(open.synctex,quoteless_synctex_name)) {
            _synctex_error("Could not rename %s to %s, error %i\n",open.synctex,quoteless_synctex_name,errno);
            /*	We could not rename, reopen the file with the quoted name. */
            if (NULL == (open.file = gzopen(open.synctex,mode))) {
                /*  No luck, could not re open this file, something has happened meanwhile */
                if (errno != ENOENT) {
                    /*  The file does not exist any more, it has certainly be removed somehow
                     *  this is a lower level error, I can't do anything. */
                    _synctex_error("Could not open again %s, error %i\n",open.synctex,errno);
                }
                goto return_on_error;
            }
        } else {
            /*  The file has been successfully renamed */
            if (NULL == (open.file = gzopen(quoteless_synctex_name,mode))) {
                /*  Could not open this file */
                if (errno != ENOENT) {
                    /*  The file does exist, this is a lower level error, I can't do anything. */
                    _synctex_error("Could not open renamed %s, error %i\n",quoteless_synctex_name,errno);
                }
                goto return_on_error;
            }
            /*  The quote free file name should replace the old one:*/
            free(open.synctex);
            open.synctex = quoteless_synctex_name;
            quoteless_synctex_name = NULL;
        }
    }
    /*  The operation is successful, return the arguments by value.    */
    open.status = SYNCTEX_STATUS_OK;
    return open;
}

/*	Opens the output file, taking into account the eventual build_directory.
 *	- returns: an open structure which status is
 *      SYNCTEX_STATUS_OK on success,
 *      SYNCTEX_STATUS_ERROR on failure.
 *  - note: on success, the caller is the owner
 *      of the fields of the returned open structure.
 */
static synctex_open_s _synctex_open_v2(const char * output, const char * build_directory, synctex_io_mode_t io_mode, synctex_bool_t add_quotes) {
    synctex_open_s open = __synctex_open_v2(output,io_mode,add_quotes);
    if (open.status == SYNCTEX_STATUS_OK) {
        return open;
    }
    if (build_directory && strlen(build_directory)) {
        char * build_output;
        const char *lpc;
        size_t size;
        synctex_bool_t is_absolute;
        build_output = NULL;
        lpc = _synctex_last_path_component(output);
        size = strlen(build_directory)+strlen(lpc)+2;   /*  One for the '/' and one for the '\0'.   */
        is_absolute = _synctex_path_is_absolute(build_directory);
        if (!is_absolute) {
            size += strlen(output);
        }
        if ((build_output = (char *)_synctex_malloc(size))) {
            if (is_absolute) {
                build_output[0] = '\0';
            } else {
                if (build_output != strcpy(build_output,output)) {
                    _synctex_free(build_output);
                    return open;
                }
                build_output[lpc-output]='\0';
            }
            if (build_output == strcat(build_output,build_directory)) {
                /*	Append a path separator if necessary. */
                if (!SYNCTEX_IS_PATH_SEPARATOR(build_output[strlen(build_directory)-1])) {
                    if (build_output != strcat(build_output,"/")) {
                        _synctex_free(build_output);
                        return open;
                    }
                }
                /*	Append the last path component of the output. */
                if (build_output != strcat(build_output,lpc)) {
                    _synctex_free(build_output);
                    return open;
                }
                open = __synctex_open_v2(build_output,io_mode,add_quotes);
            }
            _synctex_free(build_output);
        } /* if ((build_output... */
    } /* if (build_directory...) */
    return open;
}
static void synctex_reader_free(synctex_reader_p reader) {
    if (reader) {
        _synctex_free(reader->output);
        _synctex_free(reader->synctex);
        _synctex_free(reader->start);
        gzclose(reader->file);
        _synctex_free(reader);
    }
}
/*
 *  Return reader on success.
 *  Deallocate reader and return NULL on failure.
 */
static synctex_reader_p synctex_reader_init_with_output_file(synctex_reader_p reader, const char * output, const char * build_directory) {
    if (reader) {
        /*  now open the synctex file */
        synctex_open_s open = _synctex_open_v2(output,build_directory,0,synctex_ADD_QUOTES);
        if (open.status<SYNCTEX_STATUS_OK) {
            open = _synctex_open_v2(output,build_directory,0,synctex_DONT_ADD_QUOTES);
            if (open.status<SYNCTEX_STATUS_OK) {
                return NULL;
            }
        }
        reader->synctex = open.synctex;
        reader->file = open.file;
        /*  make a private copy of output */
        if (NULL == (reader->output = (char *)_synctex_malloc(strlen(output)+1))){
            _synctex_error("!  synctex_scanner_new_with_output_file: Memory problem (2), reader's output is not reliable.");
        } else if (reader->output != strcpy(reader->output,output)) {
            _synctex_free(reader->output);
            reader->output = NULL;
            _synctex_error("!  synctex_scanner_new_with_output_file: Copy problem, reader's output is not reliable.");
        }
        reader->start = reader->end = reader->current = NULL;
        reader->min_size = SYNCTEX_BUFFER_MIN_SIZE;
        reader->size = SYNCTEX_BUFFER_SIZE;
        reader->start = reader->current =
            (char *)_synctex_malloc(reader->size+1); /*  one more character for null termination */
        if (NULL == reader->start) {
            _synctex_error("!  malloc error in synctex_reader_init_with_output_file.");
#ifdef SYNCTEX_DEBUG
            return reader;
#else
            synctex_reader_free(reader);
            return NULL;
#endif
        }
        reader->end = reader->start+reader->size;
        /*  reader->end always points to a null terminating character.
         *  Maybe there is another null terminating character between reader->current and reader->end-1.
         *  At least, we are sure that reader->current points to a string covering a valid part of the memory. */
#   if defined(SYNCTEX_USE_CHARINDEX)
        reader->charindex_offset = -reader->size;
#   endif
    }
    return reader;
}

#   if defined(SYNCTEX_USE_HANDLE)
#       define SYNCTEX_DECLARE_HANDLE synctex_node_p handle;
#   else
#       define SYNCTEX_DECLARE_HANDLE
#   endif

#   ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark SCANNER
#   endif
/**
 *  The synctex scanner is the root object.
 *  Is is initialized with the contents of a text file or a gzipped file.
 *  The buffer_.* are first used to parse the text.
 */
struct synctex_scanner_t {
    synctex_reader_p reader;
    SYNCTEX_DECLARE_NODE_COUNT
    SYNCTEX_DECLARE_HANDLE
    char * output_fmt;          /*  dvi or pdf, not yet used */
    synctex_iterator_p iterator;/*  result iterator */
    int version;                /*  1, not yet used */
    struct {
        unsigned has_parsed:1;		/*  Whether the scanner has parsed its underlying synctex file. */
        unsigned postamble:1;		/*  Whether the scanner has parsed its underlying synctex file. */
        unsigned reserved:sizeof(unsigned)-2;	/*  alignment */
    } flags;
    int pre_magnification;  /*  magnification from the synctex preamble */
    int pre_unit;           /*  unit from the synctex preamble */
    int pre_x_offset;       /*  X offset from the synctex preamble */
    int pre_y_offset;       /*  Y offset from the synctex preamble */
    int count;              /*  Number of records, from the synctex postamble */
    float unit;             /*  real unit, from synctex preamble or post scriptum */
    float x_offset;         /*  X offset, from synctex preamble or post scriptum */
    float y_offset;         /*  Y Offset, from synctex preamble or post scriptum */
    synctex_node_p input;   /*  The first input node, its siblings are the other input nodes */
    synctex_node_p sheet;   /*  The first sheet node, its siblings are the other sheet nodes */
    synctex_node_p form;    /*  The first form, its siblings are the other forms */
    synctex_node_p ref_in_sheet; /*  The first form ref node in sheet, its friends are the other form ref nodes */
    synctex_node_p ref_in_form;  /*  The first form ref node, its friends are the other form ref nodes in sheet */
    int number_of_lists;    /*  The number of friend lists */
    synctex_node_r lists_of_friends;/*  The friend lists */
    synctex_class_s class_[synctex_node_number_of_types]; /*  The classes of the nodes of the scanner */
    int display_switcher;
    char * display_prompt;
};

/**
 *  Create a new node of the given type.
 *  - parameter scanner: of type synctex_node_p
 *  - parameter type: a type, the client is responsible
 *  to ask for an acceptable type.
 */
synctex_node_p synctex_node_new(synctex_scanner_p scanner, synctex_node_type_t type) {
    return scanner? scanner->class_[type].new(scanner):NULL;
}
#   if defined(SYNCTEX_USE_HANDLE)
SYNCTEX_INLINE static void __synctex_scanner_free_handle(synctex_scanner_p scanner) {
    synctex_node_free(scanner->handle);
}
SYNCTEX_INLINE static void __synctex_scanner_remove_handle_to(synctex_node_p node) {
    synctex_node_p arg_sibling = NULL;
    synctex_node_p handle = node->class_->scanner->handle;
    while (handle) {
        synctex_node_p sibling;
        if (node == _synctex_tree_target(handle)) {
            sibling = __synctex_tree_reset_sibling(handle);
            if (arg_sibling) {
                __synctex_tree_set_sibling(arg_sibling, sibling);
            } else {
                node->class_->scanner->handle = sibling;
            }
            synctex_node_free(handle);
            break;
        } else {
            sibling = __synctex_tree_sibling(handle);
        }
        arg_sibling = handle;
        handle = sibling;
    }
}
SYNCTEX_INLINE static void __synctex_scanner_register_handle_to(synctex_node_p  node) {
    synctex_node_p NNN = _synctex_new_handle_with_target(node);
    __synctex_tree_set_sibling(NNN,node->class_->scanner->handle);
    node->class_->scanner->handle = NNN;
}
#endif
#if SYNCTEX_USE_NODE_COUNT>10
SYNCTEX_INLINE static void _synctex_did_new(synctex_node_p node) {
    printf("NODE CREATED # %i, %s, %p\n",
           (node->class_->scanner->node_count)++,
           synctex_node_isa(node),
           node);
}
SYNCTEX_INLINE static void _synctex_will_free(synctex_node_p node) {
    printf("NODE DELETED # %i, %s, %p\n",
           --(node->class_->scanner->node_count),
           synctex_node_isa(node),
           node);
}
#endif

/**
 *  Free the given node.
 *  - parameter node: of type synctex_node_p
 *  - note: a node is meant to own its child and sibling.
 *  It is not owned by its parent, unless it is its first child.
 *  This destructor is for all nodes with children.
 */
static void _synctex_free_node(synctex_node_p node) {
    if (node) {
        SYNCTEX_SCANNER_REMOVE_HANDLE_TO(node);
        SYNCTEX_WILL_FREE(node);
        synctex_node_free(__synctex_tree_sibling(node));
        synctex_node_free(_synctex_tree_child(node));
        _synctex_free(node);
    }
    return;
}
/**
 *  Free the given handle.
 *  - parameter node: of type synctex_node_p
 *  - note: a node is meant to own its child and sibling.
 *  It is not owned by its parent, unless it is its first child.
 *  This destructor is for all handles.
 */
/*
static void _synctex_free_handle_old(synctex_node_p handle) {
  if (handle) {
    _synctex_free_handle_old(__synctex_tree_sibling(handle));
    _synctex_free_handle_old(_synctex_tree_child(handle));
    _synctex_free(handle);
  }
  return;
}
*/
static void _synctex_free_handle(synctex_node_p handle) {
  if (handle) {
    synctex_node_p n = handle;
    synctex_node_p nn;
    __synctex_tree_set_parent(n, NULL);
  down:
    while ((nn = _synctex_tree_child(n))) {
      __synctex_tree_set_parent(nn, n);
      n = nn;
    };
  right:
    nn = __synctex_tree_sibling(n);
    if (nn) {
      _synctex_free(n);
      n = nn;
      goto down;
    }
    nn = __synctex_tree_parent(n);
    _synctex_free(n);
    if (nn) {
      n = nn;
      goto right;
    }
  }
  return;
}

/**
 *  Free the given leaf node.
 *  - parameter node: of type synctex_node_p, with no child nor sibling.
 *  - note: a node is meant to own its child and sibling.
 *  It is not owned by its parent, unless it is its first child.
 *  This destructor is for all nodes with no children.
 */
static void _synctex_free_leaf(synctex_node_p node) {
    if (node) {
        SYNCTEX_SCANNER_REMOVE_HANDLE_TO(node);
        SYNCTEX_WILL_FREE(node);
        synctex_node_free(__synctex_tree_sibling(node));
        _synctex_free(node);
    }
    return;
}

/**
 SYNCTEX_CUR, SYNCTEX_START and SYNCTEX_END are convenient shortcuts
 */
#   define SYNCTEX_CUR (scanner->reader->current)
#   define SYNCTEX_START (scanner->reader->start)
#   define SYNCTEX_END (scanner->reader->end)

/*  Here are gathered all the possible status that the next scanning functions will return.
 *  All these functions return a status, and pass their result through pointers.
 *  Negative values correspond to errors.
 *  The management of the buffer is causing some significant overhead.
 *  Every function that may access the buffer returns a status related to the buffer and file state.
 *  status >= SYNCTEX_STATUS_OK means the function worked as expected
 *  status < SYNCTEX_STATUS_OK means the function did not work as expected
 *  status == SYNCTEX_STATUS_NOT_OK means the function did not work as expected but there is still some material to parse.
 *  status == SYNCTEX_STATUS_EOF means the function did not work as expected and there is no more material.
 *  status<SYNCTEX_STATUS_EOF means an error
 */
#if defined(SYNCTEX_USE_CHARINDEX)
synctex_node_p synctex_scanner_handle(synctex_scanner_p scanner) {
    return scanner? scanner->handle:NULL;
}
#endif

#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark Decoding prototypes
#   endif

typedef struct {
    int integer;
    synctex_status_t status;
} synctex_is_s;

static synctex_is_s _synctex_decode_int(synctex_scanner_p scanner);
static synctex_is_s _synctex_decode_int_opt(synctex_scanner_p scanner, int default_value);
static synctex_is_s _synctex_decode_int_v(synctex_scanner_p scanner);

typedef struct {
    char * string;
    synctex_status_t status;
} synctex_ss_s;

static synctex_ss_s _synctex_decode_string(synctex_scanner_p scanner);

#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark Data SETGET
#   endif

/**
 *  The next macros are used to access the node data info
 *  through the class modelator integer fields.
  *  - parameter NODE: of type synctex_node_p
 */
#   define SYNCTEX_DATA(NODE) ((*((((NODE)->class_))->info))(NODE))
#if defined SYNCTEX_DEBUG > 1000
#   define DEFINE_SYNCTEX_DATA_HAS(WHAT) \
SYNCTEX_INLINE static synctex_bool_t __synctex_data_has_##WHAT(synctex_node_p node) {\
    return (node && (node->class_->modelator->WHAT>=0));\
}\
SYNCTEX_INLINE static synctex_bool_t _synctex_data_has_##WHAT(synctex_node_p node) {\
    if (node && (node->class_->modelator->WHAT<0)) {\
        printf("WARNING: NO %s for %s\n", #WHAT, synctex_node_isa(node));\
    }\
    return __synctex_data_has_##WHAT(node);\
}
#else
#   define DEFINE_SYNCTEX_DATA_HAS(WHAT) \
SYNCTEX_INLINE static synctex_bool_t __synctex_data_has_##WHAT(synctex_node_p node) {\
    return (node && (node->class_->modelator->WHAT>=0));\
}\
SYNCTEX_INLINE static synctex_bool_t _synctex_data_has_##WHAT(synctex_node_p node) {\
    return __synctex_data_has_##WHAT(node);\
}
#endif

SYNCTEX_INLINE static synctex_data_p __synctex_data(synctex_node_p node) {
    return node->data+node->class_->navigator->size;
}
#   define DEFINE_SYNCTEX_DATA_INT_GETSET(WHAT) \
DEFINE_SYNCTEX_DATA_HAS(WHAT)\
static int _synctex_data_##WHAT(synctex_node_p node) {\
    if (_synctex_data_has_##WHAT(node)) {\
        return __synctex_data(node)[node->class_->modelator->WHAT].as_integer;\
    }\
    return 0;\
}\
static int _synctex_data_set_##WHAT(synctex_node_p node, int new_value) {\
    int old = 0;\
    if (_synctex_data_has_##WHAT(node)) {\
        old = __synctex_data(node)[node->class_->modelator->WHAT].as_integer;\
        __synctex_data(node)[node->class_->modelator->WHAT].as_integer=new_value;\
    }\
    return old;\
}
#define DEFINE_SYNCTEX_DATA_INT_DECODE(WHAT) \
static synctex_status_t _synctex_data_decode_##WHAT(synctex_node_p node) {\
    if (_synctex_data_has_##WHAT(node)) {\
        synctex_is_s is = _synctex_decode_int(node->class_->scanner);\
        if (is.status == SYNCTEX_STATUS_OK) {\
            _synctex_data_set_##WHAT(node,is.integer);\
        } \
        return is.status;\
    }\
    return SYNCTEX_STATUS_BAD_ARGUMENT;\
}
#   define DEFINE_SYNCTEX_DATA_INT_DECODE_v(WHAT) \
static synctex_status_t _synctex_data_decode_##WHAT##_v(synctex_node_p node) {\
    if (_synctex_data_has_##WHAT(node)) {\
        synctex_is_s is = _synctex_decode_int_v(node->class_->scanner);\
        if (is.status == SYNCTEX_STATUS_OK) {\
            _synctex_data_set_##WHAT(node,is.integer);\
        } \
        return is.status;\
    }\
    return SYNCTEX_STATUS_BAD_ARGUMENT;\
}
#define DEFINE_SYNCTEX_DATA_STR_GETSET(WHAT) \
DEFINE_SYNCTEX_DATA_HAS(WHAT)\
static char * _synctex_data_##WHAT(synctex_node_p node) {\
    if (_synctex_data_has_##WHAT(node)) {\
        return node->data[node->class_->navigator->size+node->class_->modelator->WHAT].as_string;\
    }\
    return NULL;\
}\
static char * _synctex_data_set_##WHAT(synctex_node_p node, char * new_value) {\
    char * old = "";\
    if (_synctex_data_has_##WHAT(node)) {\
        old = node->data[node->class_->navigator->size+node->class_->modelator->WHAT].as_string;\
        node->data[node->class_->navigator->size+node->class_->modelator->WHAT].as_string =new_value;\
    }\
    return old;\
}
#define DEFINE_SYNCTEX_DATA_STR_DECODE(WHAT) \
static synctex_status_t _synctex_data_decode_##WHAT(synctex_node_p node) {\
    if (_synctex_data_has_##WHAT(node)) {\
        synctex_ss_s ss = _synctex_decode_string(node->class_->scanner);\
        if (ss.status == SYNCTEX_STATUS_OK) {\
            _synctex_data_set_##WHAT(node,ss.string);\
        } \
        return ss.status;\
    }\
    return SYNCTEX_STATUS_BAD_ARGUMENT;\
}
#define DEFINE_SYNCTEX_DATA_INT_GETSET_DECODE(WHAT) \
DEFINE_SYNCTEX_DATA_INT_GETSET(WHAT) \
DEFINE_SYNCTEX_DATA_INT_DECODE(WHAT)
#define DEFINE_SYNCTEX_DATA_INT_GETSET_DECODE_v(WHAT) \
DEFINE_SYNCTEX_DATA_INT_GETSET(WHAT) \
DEFINE_SYNCTEX_DATA_INT_DECODE_v(WHAT)
#define DEFINE_SYNCTEX_DATA_STR_GETSET_DECODE(WHAT) \
DEFINE_SYNCTEX_DATA_STR_GETSET(WHAT) \
DEFINE_SYNCTEX_DATA_STR_DECODE(WHAT)

#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark OBJECTS, their creators and destructors.
#   endif

#	ifdef SYNCTEX_NOTHING
#       pragma mark input.
#   endif

DEFINE_SYNCTEX_DATA_INT_GETSET_DECODE(tag)
DEFINE_SYNCTEX_DATA_INT_GETSET_DECODE(line)
DEFINE_SYNCTEX_DATA_STR_GETSET_DECODE(name)

/*  Input nodes only know about their sibling, which is another input node.
 *  The synctex information is the _synctex_data_tag and _synctex_data_name
 *  note: the input owns its name. */

#   define SYNCTEX_INPUT_MARK "Input:"

static const synctex_tree_model_s synctex_tree_model_input = {
    synctex_tree_sibling_idx, /* sibling */
    -1, /* parent */
    -1, /* child */
    -1, /* friend */
    -1, /* last */
    -1, /* next_hbox */
    -1, /* arg_sibling */
    -1, /* target */
    synctex_tree_s_input_max
};
static const synctex_data_model_s synctex_data_model_input = {
    synctex_data_input_tag_idx, /* tag */
    synctex_data_input_line_idx,/* line */
    -1, /* column */
    -1, /* h */
    -1, /* v */
    -1, /* width */
    -1, /* height */
    -1, /* depth */
    -1, /* mean_line */
    -1, /* weight */
    -1, /* h_V */
    -1, /* v_V */
    -1, /* width_V */
    -1, /* height_V */
    -1, /* depth_V */
    synctex_data_input_name_idx, /* name */
    -1, /* page */
    synctex_data_input_tln_max
};

#define SYNCTEX_INSPECTOR_GETTER_F(WHAT)\
&_synctex_data_##WHAT, &_synctex_data_set_##WHAT

static synctex_node_p _synctex_new_input(synctex_scanner_p scanner);
static void _synctex_free_input(synctex_node_p node);
static void _synctex_log_input(synctex_node_p node);
static char * _synctex_abstract_input(synctex_node_p node);
static void _synctex_display_input(synctex_node_p node);

static const synctex_tlcpector_s synctex_tlcpector_input = {
    &_synctex_data_tag, /* tag */
    &_synctex_int_none, /* line */
    &_synctex_int_none, /* column */
};

static synctex_class_s synctex_class_input = {
    NULL,                       /*  No scanner yet */
    synctex_node_type_input,    /*  Node type */
    &_synctex_new_input,        /*  creator */
    &_synctex_free_input,       /*  destructor */
    &_synctex_log_input,        /*  log */
    &_synctex_display_input,    /*  display */
    &_synctex_abstract_input,   /*  abstract */
    &synctex_tree_model_input,  /*  tree model */
    &synctex_data_model_input,  /*  data model */
    &synctex_tlcpector_input,   /*  inspector */
    &synctex_inspector_none,    /*  inspector */
    &synctex_vispector_none,    /*  vispector */
};

typedef struct {
    SYNCTEX_DECLARE_CHARINDEX
    synctex_class_p class_;
    synctex_data_u data[synctex_tree_s_input_max+synctex_data_input_tln_max];
} synctex_input_s;

static synctex_node_p _synctex_new_input(synctex_scanner_p scanner) {
    if (scanner) {
        synctex_node_p node = _synctex_malloc(sizeof(synctex_input_s));
        if (node) {
            node->class_ = scanner->class_+synctex_node_type_input;
            SYNCTEX_DID_NEW(node);
            SYNCTEX_IMPLEMENT_CHARINDEX(node,0);
            SYNCTEX_REGISTER_HANDLE_TO(node);
        }
        return node;
    }
    return NULL;
}

static void _synctex_free_input(synctex_node_p node){
    if (node) {
        SYNCTEX_SCANNER_REMOVE_HANDLE_TO(node);
        SYNCTEX_WILL_FREE(node);
        synctex_node_free(__synctex_tree_sibling(node));
        _synctex_free(_synctex_data_name(node));
        _synctex_free(node);
    }
}

/*  The sheet is a first level node.
 *  It has no parent (the owner is the scanner itself)
 *  Its sibling points to another sheet.
 *  Its child points to its first child, in general a box.
 *  A sheet node contains only one synctex information: the page.
 *  This is the 1 based page index as given by TeX.
 */

#	ifdef SYNCTEX_NOTHING
#       pragma mark sheet.
#   endif
/**
 *  Every node has the same structure, but not the same size.
 */

DEFINE_SYNCTEX_DATA_INT_GETSET_DECODE(page)

typedef struct {
    SYNCTEX_DECLARE_CHARINDEX
    synctex_class_p class_;
    synctex_data_u data[synctex_tree_scn_sheet_max+synctex_data_p_sheet_max];
} synctex_node_sheet_s;

/*  sheet node creator */

#define DEFINE_synctex_new_scanned_NODE(NAME)\
static synctex_node_p _synctex_new_##NAME(synctex_scanner_p scanner) {\
    if (scanner) {\
        ++SYNCTEX_CUR;\
        synctex_node_p node = _synctex_malloc(sizeof(synctex_node_##NAME##_s));\
        if (node) {\
            node->class_ = scanner->class_+synctex_node_type_##NAME;\
            SYNCTEX_DID_NEW(node); \
            SYNCTEX_IMPLEMENT_CHARINDEX(node,-1);\
            SYNCTEX_REGISTER_HANDLE_TO(node); \
        }\
        return node;\
    }\
    return NULL;\
}
/*  NB: -1 in SYNCTEX_IMPLEMENT_CHARINDEX above because
 *  the first char of the line has been scanned
 */
DEFINE_synctex_new_scanned_NODE(sheet)
static void _synctex_log_sheet(synctex_node_p node);
static char * _synctex_abstract_sheet(synctex_node_p node);
static void _synctex_display_sheet(synctex_node_p node);

static const synctex_tree_model_s synctex_tree_model_sheet = {
    synctex_tree_sibling_idx, /* sibling */
    -1, /* parent */
    synctex_tree_s_child_idx, /* child */
    -1, /* friend */
    -1, /* last */
    synctex_tree_sc_next_hbox_idx, /* next_hbox */
    -1, /* arg_sibling */
    -1, /* target */
    synctex_tree_scn_sheet_max
};
static const synctex_data_model_s synctex_data_model_sheet = {
    -1, /* tag */
    -1, /* line */
    -1, /* column */
    -1, /* h */
    -1, /* v */
    -1, /* width */
    -1, /* height */
    -1, /* depth */
    -1, /* mean_line */
    -1, /* weight */
    -1, /* h_V */
    -1, /* v_V */
    -1, /* width_V */
    -1, /* height_V */
    -1, /* depth_V */
    -1, /* name */
    synctex_data_sheet_page_idx, /* page */
    synctex_data_p_sheet_max
};
static synctex_class_s synctex_class_sheet = {
    NULL,                       /*  No scanner yet */
    synctex_node_type_sheet,    /*  Node type */
    &_synctex_new_sheet,        /*  creator */
    &_synctex_free_node,        /*  destructor */
    &_synctex_log_sheet,        /*  log */
    &_synctex_display_sheet,    /*  display */
    &_synctex_abstract_sheet,   /*  abstract */
    &synctex_tree_model_sheet,  /*  tree model */
    &synctex_data_model_sheet,  /*  data model */
    &synctex_tlcpector_none,    /*  tlcpector */
    &synctex_inspector_none,    /*  inspector */
    &synctex_vispector_none,    /*  vispector */
};

#	ifdef SYNCTEX_NOTHING
#       pragma mark form.
#   endif
/**
 *  Every node has the same structure, but not the same size.
 */
typedef struct {
    SYNCTEX_DECLARE_CHARINDEX
    synctex_class_p class_;
    synctex_data_u data[synctex_tree_sct_form_max+synctex_data_t_form_max];
} synctex_node_form_s;

DEFINE_synctex_new_scanned_NODE(form)

static char * _synctex_abstract_form(synctex_node_p node);
static void _synctex_display_form(synctex_node_p node);
static void _synctex_log_form(synctex_node_p node);

static const synctex_tree_model_s synctex_tree_model_form = {
    synctex_tree_sibling_idx, /* sibling */
    -1, /* parent */
    synctex_tree_s_child_idx, /* child */
    -1, /* friend */
    -1, /* last */
    -1, /* next_hbox */
    -1, /* arg_sibling */
    synctex_tree_sc_target_idx, /* target */
    synctex_tree_sct_form_max
};
static const synctex_data_model_s synctex_data_model_form = {
    synctex_data_form_tag_idx, /* tag */
    -1, /* line */
    -1, /* column */
    -1, /* h */
    -1, /* v */
    -1, /* width */
    -1, /* height */
    -1, /* depth */
    -1, /* mean_line */
    -1, /* weight */
    -1, /* h_V */
    -1, /* v_V */
    -1, /* width_V */
    -1, /* height_V */
    -1, /* depth_V */
    -1, /* name */
    -1, /* page */
    synctex_data_t_form_max
};
static synctex_class_s synctex_class_form = {
    NULL,                       /*  No scanner yet */
    synctex_node_type_form,     /*  Node type */
    &_synctex_new_form,         /*  creator */
    &_synctex_free_node,        /*  destructor */
    &_synctex_log_form,         /*  log */
    &_synctex_display_form,     /*  display */
    &_synctex_abstract_form,    /*  abstract */
    &synctex_tree_model_form,   /*  tree model */
    &synctex_data_model_form,   /*  data model */
    &synctex_tlcpector_none,    /*  tlcpector */
    &synctex_inspector_none,    /*  inspector */
    &synctex_vispector_none,    /*  vispector */
};

#	ifdef SYNCTEX_NOTHING
#       pragma mark vbox.
#   endif

/*  A box node contains navigation and synctex information
 *  There are different kinds of boxes.
 *  Only horizontal boxes are treated differently because of their visible size.
 */
typedef struct {
    SYNCTEX_DECLARE_CHARINDEX
    synctex_class_p class_;
    synctex_data_u data[synctex_tree_spcfl_vbox_max+synctex_data_box_max];
} synctex_node_vbox_s;

/*  vertical box node creator */
DEFINE_synctex_new_scanned_NODE(vbox)

static char * _synctex_abstract_vbox(synctex_node_p node);
static void _synctex_display_vbox(synctex_node_p node);
static void _synctex_log_vbox(synctex_node_p node);

static const synctex_tree_model_s synctex_tree_model_vbox = {
    synctex_tree_sibling_idx,       /* sibling */
    synctex_tree_s_parent_idx,      /* parent */
    synctex_tree_sp_child_idx,      /* child */
    synctex_tree_spc_friend_idx,    /* friend */
    synctex_tree_spcf_last_idx,     /* last */
    -1, /* next_hbox */
    -1, /* arg_sibling */
    -1, /* target */
    synctex_tree_spcfl_vbox_max
};

#define SYNCTEX_DFLT_COLUMN -1

DEFINE_SYNCTEX_DATA_INT_GETSET(column)
static synctex_status_t _synctex_data_decode_column(synctex_node_p node) {
    if (_synctex_data_has_column(node)) {
        synctex_is_s is = _synctex_decode_int_opt(node->class_->scanner,
            SYNCTEX_DFLT_COLUMN);
        if (is.status == SYNCTEX_STATUS_OK) {
            _synctex_data_set_column(node,is.integer);
        }
        return is.status;
    }
    return SYNCTEX_STATUS_BAD_ARGUMENT;
}
DEFINE_SYNCTEX_DATA_INT_GETSET_DECODE(h)
DEFINE_SYNCTEX_DATA_INT_GETSET_DECODE_v(v)
DEFINE_SYNCTEX_DATA_INT_GETSET_DECODE(width)
DEFINE_SYNCTEX_DATA_INT_GETSET_DECODE(height)
DEFINE_SYNCTEX_DATA_INT_GETSET_DECODE(depth)

SYNCTEX_INLINE static void _synctex_data_set_tlc(synctex_node_p node, synctex_node_p model) {
    _synctex_data_set_tag(node, _synctex_data_tag(model));
    _synctex_data_set_line(node, _synctex_data_line(model));
    _synctex_data_set_column(node, _synctex_data_column(model));
}
SYNCTEX_INLINE static void _synctex_data_set_tlchv(synctex_node_p node, synctex_node_p model) {
    _synctex_data_set_tlc(node,model);
    _synctex_data_set_h(node, _synctex_data_h(model));
    _synctex_data_set_v(node, _synctex_data_v(model));
}

static const synctex_data_model_s synctex_data_model_box = {
    synctex_data_tag_idx, /* tag */
    synctex_data_line_idx,  /* line */
    synctex_data_column_idx,/* column */
    synctex_data_h_idx,     /* h */
    synctex_data_v_idx,     /* v */
    synctex_data_width_idx, /* width */
    synctex_data_height_idx,/* height */
    synctex_data_depth_idx, /* depth */
    -1, /* mean_line */
    -1, /* weight */
    -1, /* h_V */
    -1, /* v_V */
    -1, /* width_V */
    -1, /* height_V */
    -1, /* depth_V */
    -1, /* name */
    -1, /* page */
    synctex_data_box_max
};
static const synctex_tlcpector_s synctex_tlcpector_default = {
    &_synctex_data_tag, /* tag */
    &_synctex_data_line, /* line */
    &_synctex_data_column, /* column */
};
static const synctex_inspector_s synctex_inspector_box = {
    &_synctex_data_h,
    &_synctex_data_v,
    &_synctex_data_width,
    &_synctex_data_height,
    &_synctex_data_depth,
};
static float __synctex_node_visible_h(synctex_node_p node);
static float __synctex_node_visible_v(synctex_node_p node);
static float __synctex_node_visible_width(synctex_node_p node);
static float __synctex_node_visible_height(synctex_node_p node);
static float __synctex_node_visible_depth(synctex_node_p node);
static synctex_vispector_s synctex_vispector_box = {
    &__synctex_node_visible_h,
    &__synctex_node_visible_v,
    &__synctex_node_visible_width,
    &__synctex_node_visible_height,
    &__synctex_node_visible_depth,
};
/*  These are static class objects, each scanner will make a copy of them and setup the scanner field.
 */
static synctex_class_s synctex_class_vbox = {
    NULL,                       /*  No scanner yet */
    synctex_node_type_vbox,     /*  Node type */
    &_synctex_new_vbox,         /*  creator */
    &_synctex_free_node,        /*  destructor */
    &_synctex_log_vbox,         /*  log */
    &_synctex_display_vbox,     /*  display */
    &_synctex_abstract_vbox,    /*  abstract */
    &synctex_tree_model_vbox,   /*  tree model */
    &synctex_data_model_box,    /*  data model */
    &synctex_tlcpector_default, /*  tlcpector */
    &synctex_inspector_box,     /*  inspector */
    &synctex_vispector_box,     /*  vispector */
};

#	ifdef SYNCTEX_NOTHING
#       pragma mark hbox.
#   endif

/*  Horizontal boxes must contain visible size, because 0 width does not mean emptiness.
 *  They also contain an average of the line numbers of the containing nodes. */

static const synctex_tree_model_s synctex_tree_model_hbox = {
    synctex_tree_sibling_idx,       /* sibling */
    synctex_tree_s_parent_idx,      /* parent */
    synctex_tree_sp_child_idx,      /* child */
    synctex_tree_spc_friend_idx,    /* friend */
    synctex_tree_spcf_last_idx,     /* last */
    synctex_tree_spcfl_next_hbox_idx, /* next_hbox */
    -1, /* arg_sibling */
    -1, /* target */
    synctex_tree_spcfln_hbox_max
};

DEFINE_SYNCTEX_DATA_INT_GETSET(mean_line)
DEFINE_SYNCTEX_DATA_INT_GETSET(weight)
DEFINE_SYNCTEX_DATA_INT_GETSET(h_V)
DEFINE_SYNCTEX_DATA_INT_GETSET(v_V)
DEFINE_SYNCTEX_DATA_INT_GETSET(width_V)
DEFINE_SYNCTEX_DATA_INT_GETSET(height_V)
DEFINE_SYNCTEX_DATA_INT_GETSET(depth_V)

/**
 *  The hbox model.
 *  It contains V variants of geometrical information.
 *  It happens that hboxes contain material that is not used to compute
 *  the bounding box. Some letters may appear out of the box given by TeX.
 *  In such a situation, the visible bouding box is bigger ence the V variant.
 *  Only hboxes have such variant. It does not make sense for void boxes
 *  and it is not used here for vboxes.
 *  - author: JL
 */

static const synctex_data_model_s synctex_data_model_hbox = {
    synctex_data_tag_idx, /* tag */
    synctex_data_line_idx,  /* line */
    synctex_data_column_idx,/* column */
    synctex_data_h_idx,     /* h */
    synctex_data_v_idx,     /* v */
    synctex_data_width_idx, /* width */
    synctex_data_height_idx,/* height */
    synctex_data_depth_idx, /* depth */
    synctex_data_mean_line_idx, /* mean_line */
    synctex_data_weight_idx, /* weight */
    synctex_data_h_V_idx, /* h_V */
    synctex_data_v_V_idx, /* v_V */
    synctex_data_width_V_idx, /* width_V */
    synctex_data_height_V_idx, /* height_V */
    synctex_data_depth_V_idx, /* depth_V */
    -1, /* name */
    -1, /* page */
    synctex_data_hbox_max
};

typedef struct {
    SYNCTEX_DECLARE_CHARINDEX
    synctex_class_p class_;
    synctex_data_u data[synctex_tree_spcfln_hbox_max+synctex_data_hbox_max];
} synctex_node_hbox_s;

/*  horizontal box node creator */
DEFINE_synctex_new_scanned_NODE(hbox)

static void _synctex_log_hbox(synctex_node_p node);
static char * _synctex_abstract_hbox(synctex_node_p node);
static void _synctex_display_hbox(synctex_node_p node);

static synctex_class_s synctex_class_hbox = {
    NULL,                       /*  No scanner yet */
    synctex_node_type_hbox,     /*  Node type */
    &_synctex_new_hbox,         /*  creator */
    &_synctex_free_node,        /*  destructor */
    &_synctex_log_hbox,         /*  log */
    &_synctex_display_hbox,     /*  display */
    &_synctex_abstract_hbox,    /*  abstract */
    &synctex_tree_model_hbox,   /*  tree model */
    &synctex_data_model_hbox,   /*  data model */
    &synctex_tlcpector_default, /*  tlcpector */
    &synctex_inspector_box,     /*  inspector */
    &synctex_vispector_box,     /*  vispector */
};

#	ifdef SYNCTEX_NOTHING
#       pragma mark void vbox.
#   endif

/*  This void box node implementation is either horizontal or vertical
 *  It does not contain a child field.
 */
static const synctex_tree_model_s synctex_tree_model_spf = {
    synctex_tree_sibling_idx,   /* sibling */
    synctex_tree_s_parent_idx,  /* parent */
    -1, /* child */
    synctex_tree_sp_friend_idx, /* friend */
    -1, /* last */
    -1, /* next_hbox */
    -1, /* arg_sibling */
    -1, /* target */
    synctex_tree_spf_max
};
typedef struct {
    SYNCTEX_DECLARE_CHARINDEX
    synctex_class_p class_;
    synctex_data_u data[synctex_tree_spf_max+synctex_data_box_max];
} synctex_node_void_vbox_s;

/*  vertical void box node creator */
DEFINE_synctex_new_scanned_NODE(void_vbox)

static void _synctex_log_void_box(synctex_node_p node);
static char * _synctex_abstract_void_vbox(synctex_node_p node);
static void _synctex_display_void_vbox(synctex_node_p node);

static synctex_class_s synctex_class_void_vbox = {
    NULL,                       /*  No scanner yet */
    synctex_node_type_void_vbox,/*  Node type */
    &_synctex_new_void_vbox,    /*  creator */
    &_synctex_free_leaf,        /*  destructor */
    &_synctex_log_void_box,     /*  log */
    &_synctex_display_void_vbox,/*  display */
    &_synctex_abstract_void_vbox,/*  abstract */
    &synctex_tree_model_spf,    /*  tree model */
    &synctex_data_model_box,    /*  data model */
    &synctex_tlcpector_default, /*  tlcpector */
    &synctex_inspector_box,     /*  inspector */
    &synctex_vispector_box,     /*  vispector */
};

#	ifdef SYNCTEX_NOTHING
#       pragma mark void hbox.
#   endif

typedef synctex_node_void_vbox_s synctex_node_void_hbox_s;

/*  horizontal void box node creator */
DEFINE_synctex_new_scanned_NODE(void_hbox)

static char * _synctex_abstract_void_hbox(synctex_node_p node);
static void _synctex_display_void_hbox(synctex_node_p node);

static synctex_class_s synctex_class_void_hbox = {
    NULL,                       /*  No scanner yet */
    synctex_node_type_void_hbox,/*  Node type */
    &_synctex_new_void_hbox,    /*  creator */
    &_synctex_free_leaf,        /*  destructor */
    &_synctex_log_void_box,     /*  log */
    &_synctex_display_void_hbox,/*  display */
    &_synctex_abstract_void_hbox,/*  abstract */
    &synctex_tree_model_spf,    /*  tree model */
    &synctex_data_model_box,    /*  data model */
    &synctex_tlcpector_default, /*  tlcpector */
    &synctex_inspector_box,     /*  inspector */
    &synctex_vispector_box,     /*  vispector */
};

#	ifdef SYNCTEX_NOTHING
#       pragma mark form ref.
#   endif

/*  The form ref node.  */
typedef struct {
    SYNCTEX_DECLARE_CHARINDEX
    synctex_class_p class_;
    synctex_data_u data[synctex_tree_spfa_max+synctex_data_ref_thv_max];
} synctex_node_ref_s;

/*  form ref node creator */
DEFINE_synctex_new_scanned_NODE(ref)

static void _synctex_log_ref(synctex_node_p node);
static char * _synctex_abstract_ref(synctex_node_p node);
static void _synctex_display_ref(synctex_node_p node);

static const synctex_tree_model_s synctex_tree_model_spfa = {
    synctex_tree_sibling_idx,   /* sibling */
    synctex_tree_s_parent_idx,  /* parent */
    -1, /* child */
    synctex_tree_sp_friend_idx, /* friend */
    -1, /* last */
    -1, /* next_hbox */
    synctex_tree_spf_arg_sibling_idx, /* arg_sibling */
    -1, /* target */
    synctex_tree_spfa_max
};
static const synctex_data_model_s synctex_data_model_ref = {
    synctex_data_tag_idx, /* tag */
    -1, /* line */
    -1, /* column */
    synctex_data_ref_h_idx, /* h */
    synctex_data_ref_v_idx, /* v */
    -1, /* width */
    -1, /* height */
    -1, /* depth */
    -1, /* mean_line */
    -1, /* weight */
    -1, /* h_V */
    -1, /* v_V */
    -1, /* width_V */
    -1, /* height_V */
    -1, /* depth_V */
    -1, /* name */
    -1, /* page */
    synctex_data_ref_thv_max /* size */
};
static synctex_class_s synctex_class_ref = {
    NULL,                       /*  No scanner yet */
    synctex_node_type_ref,      /*  Node type */
    &_synctex_new_ref,          /*  creator */
    &_synctex_free_leaf,        /*  destructor */
    &_synctex_log_ref,          /*  log */
    &_synctex_display_ref,      /*  display */
    &_synctex_abstract_ref,     /*  abstract */
    &synctex_tree_model_spfa,   /*  navigator */
    &synctex_data_model_ref,    /*  data model */
    &synctex_tlcpector_none,    /*  tlcpector */
    &synctex_inspector_none,    /*  inspector */
    &synctex_vispector_none,    /*  vispector */
};
#	ifdef SYNCTEX_NOTHING
#       pragma mark small node.
#   endif

/*  The small nodes correspond to glue, penalty, math and boundary nodes. */
static const synctex_data_model_s synctex_data_model_tlchv = {
    synctex_data_tag_idx, /* tag */
    synctex_data_line_idx, /* line */
    synctex_data_column_idx, /* column */
    synctex_data_h_idx, /* h */
    synctex_data_v_idx, /* v */
    -1, /* width */
    -1, /* height */
    -1, /* depth */
    -1, /* mean_line */
    -1, /* weight */
    -1, /* h_V */
    -1, /* v_V */
    -1, /* width_V */
    -1, /* height_V */
    -1, /* depth_V */
    -1, /* name */
    -1, /* page */
    synctex_data_tlchv_max
};

typedef struct {
    SYNCTEX_DECLARE_CHARINDEX
    synctex_class_p class_;
    synctex_data_u data[synctex_tree_spf_max+synctex_data_tlchv_max];
} synctex_node_tlchv_s;

static void _synctex_log_tlchv_node(synctex_node_p node);

#	ifdef SYNCTEX_NOTHING
#       pragma mark math.
#   endif

typedef synctex_node_tlchv_s synctex_node_math_s;

/*  math node creator */
DEFINE_synctex_new_scanned_NODE(math)

static char * _synctex_abstract_math(synctex_node_p node);
static void _synctex_display_math(synctex_node_p node);
static synctex_inspector_s synctex_inspector_hv = {
    &_synctex_data_h,
    &_synctex_data_v,
    &_synctex_int_none,
    &_synctex_int_none,
    &_synctex_int_none,
};
static synctex_vispector_s synctex_vispector_hv = {
    &__synctex_node_visible_h,
    &__synctex_node_visible_v,
    &_synctex_float_none,
    &_synctex_float_none,
    &_synctex_float_none,
};

static synctex_class_s synctex_class_math = {
    NULL,                       /*  No scanner yet */
    synctex_node_type_math,     /*  Node type */
    &_synctex_new_math,         /*  creator */
    &_synctex_free_leaf,        /*  destructor */
    &_synctex_log_tlchv_node,   /*  log */
    &_synctex_display_math,     /*  display */
    &_synctex_abstract_math,    /*  abstract */
    &synctex_tree_model_spf,    /*  tree model */
    &synctex_data_model_tlchv,  /*  data model */
    &synctex_tlcpector_default, /*  tlcpector */
    &synctex_inspector_hv,      /*  inspector */
    &synctex_vispector_hv,      /*  vispector */
};

#	ifdef SYNCTEX_NOTHING
#       pragma mark kern node.
#   endif

static const synctex_data_model_s synctex_data_model_tlchvw = {
    synctex_data_tag_idx,   /* tag */
    synctex_data_line_idx,  /* line */
    synctex_data_column_idx,/* column */
    synctex_data_h_idx,     /* h */
    synctex_data_v_idx,     /* v */
    synctex_data_width_idx, /* width */
    -1, /* height */
    -1, /* depth */
    -1, /* mean_line */
    -1, /* weight */
    -1, /* h_V */
    -1, /* v_V */
    -1, /* width_V */
    -1, /* height_V */
    -1, /* depth_V */
    -1, /* name */
    -1, /* page */
    synctex_data_tlchvw_max
};
typedef struct {
    SYNCTEX_DECLARE_CHARINDEX
    synctex_class_p class_;
    synctex_data_u data[synctex_tree_spf_max+synctex_data_tlchvw_max];
} synctex_node_kern_s;

/*  kern node creator */
DEFINE_synctex_new_scanned_NODE(kern)

static void _synctex_log_kern_node(synctex_node_p node);
static char * _synctex_abstract_kern(synctex_node_p node);
static void _synctex_display_kern(synctex_node_p node);

static synctex_inspector_s synctex_inspector_kern = {
    &_synctex_data_h,
    &_synctex_data_v,
    &_synctex_data_width,
    &_synctex_int_none,
    &_synctex_int_none,
};
static float __synctex_kern_visible_h(synctex_node_p node);
static float __synctex_kern_visible_width(synctex_node_p node);
static synctex_vispector_s synctex_vispector_kern = {
    &__synctex_kern_visible_h,
    &__synctex_node_visible_v,
    &__synctex_kern_visible_width,
    &_synctex_float_none,
    &_synctex_float_none,
};

static synctex_class_s synctex_class_kern = {
    NULL,                       /*  No scanner yet */
    synctex_node_type_kern,     /*  Node type */
    &_synctex_new_kern,         /*  creator */
    &_synctex_free_leaf,        /*  destructor */
    &_synctex_log_kern_node,    /*  log */
    &_synctex_display_kern,     /*  display */
    &_synctex_abstract_kern,    /*  abstract */
    &synctex_tree_model_spf,    /*  tree model */
    &synctex_data_model_tlchvw, /*  data model */
    &synctex_tlcpector_default, /*  tlcpector */
    &synctex_inspector_kern,    /*  inspector */
    &synctex_vispector_kern,    /*  vispector */
};

#	ifdef SYNCTEX_NOTHING
#       pragma mark glue.
#   endif

/*  glue node creator */
typedef synctex_node_tlchv_s synctex_node_glue_s;
DEFINE_synctex_new_scanned_NODE(glue)

static char * _synctex_abstract_glue(synctex_node_p node);
static void _synctex_display_glue(synctex_node_p node);

static synctex_class_s synctex_class_glue = {
    NULL,                       /*  No scanner yet */
    synctex_node_type_glue,     /*  Node type */
    &_synctex_new_glue,         /*  creator */
    &_synctex_free_leaf,        /*  destructor */
    &_synctex_log_tlchv_node,   /*  log */
    &_synctex_display_glue,     /*  display */
    &_synctex_abstract_glue,    /*  abstract */
    &synctex_tree_model_spf,    /*  tree model */
    &synctex_data_model_tlchv,  /*  data model */
    &synctex_tlcpector_default, /*  tlcpector */
    &synctex_inspector_hv,      /*  inspector */
    &synctex_vispector_hv,      /*  vispector */
};

/*  The small nodes correspond to glue and boundary nodes.  */

#	ifdef SYNCTEX_NOTHING
#       pragma mark rule.
#   endif

typedef struct {
    SYNCTEX_DECLARE_CHARINDEX
    synctex_class_p class_;
    synctex_data_u data[synctex_tree_spf_max+synctex_data_box_max];
} synctex_node_rule_s;

DEFINE_synctex_new_scanned_NODE(rule)

static void _synctex_log_rule(synctex_node_p node);
static char * _synctex_abstract_rule(synctex_node_p node);
static void _synctex_display_rule(synctex_node_p node);

static float __synctex_rule_visible_h(synctex_node_p node);
static float __synctex_rule_visible_v(synctex_node_p node);
static float __synctex_rule_visible_width(synctex_node_p node);
static float __synctex_rule_visible_height(synctex_node_p node);
static float __synctex_rule_visible_depth(synctex_node_p node);
static synctex_vispector_s synctex_vispector_rule = {
    &__synctex_rule_visible_h,
    &__synctex_rule_visible_v,
    &__synctex_rule_visible_width,
    &__synctex_rule_visible_height,
    &__synctex_rule_visible_depth,
};

static synctex_class_s synctex_class_rule = {
    NULL,                       /*  No scanner yet */
    synctex_node_type_rule,     /*  Node type */
    &_synctex_new_rule,         /*  creator */
    &_synctex_free_leaf,        /*  destructor */
    &_synctex_log_rule,         /*  log */
    &_synctex_display_rule,     /*  display */
    &_synctex_abstract_rule,    /*  abstract */
    &synctex_tree_model_spf,    /*  tree model */
    &synctex_data_model_box,    /*  data model */
    &synctex_tlcpector_default, /*  tlcpector */
    &synctex_inspector_box,     /*  inspector */
    &synctex_vispector_rule,    /*  vispector */
};

#	ifdef SYNCTEX_NOTHING
#       pragma mark boundary.
#   endif

/*  boundary node creator */
typedef synctex_node_tlchv_s synctex_node_boundary_s;
DEFINE_synctex_new_scanned_NODE(boundary)

static char * _synctex_abstract_boundary(synctex_node_p node);
static void _synctex_display_boundary(synctex_node_p node);

static synctex_class_s synctex_class_boundary = {
    NULL,                       /*  No scanner yet */
    synctex_node_type_boundary, /*  Node type */
    &_synctex_new_boundary,     /*  creator */
    &_synctex_free_leaf,        /*  destructor */
    &_synctex_log_tlchv_node,   /*  log */
    &_synctex_display_boundary, /*  display */
    &_synctex_abstract_boundary,/*  abstract */
    &synctex_tree_model_spf,    /*  tree model */
    &synctex_data_model_tlchv,  /*  data model */
    &synctex_tlcpector_default, /*  tlcpector */
    &synctex_inspector_hv,      /*  inspector */
    &synctex_vispector_hv,      /*  vispector */
};

#	ifdef SYNCTEX_NOTHING
#       pragma mark box boundary.
#   endif

typedef struct {
    SYNCTEX_DECLARE_CHARINDEX
    synctex_class_p class_;
    synctex_data_u data[synctex_tree_spfa_max+synctex_data_tlchv_max];
} synctex_node_box_bdry_s;

#define DEFINE_synctex_new_unscanned_NODE(NAME)\
SYNCTEX_INLINE static synctex_node_p _synctex_new_##NAME(synctex_scanner_p scanner) {\
    if (scanner) {\
        synctex_node_p node = _synctex_malloc(sizeof(synctex_node_##NAME##_s));\
        if (node) {\
            node->class_ = scanner->class_+synctex_node_type_##NAME;\
            SYNCTEX_DID_NEW(node); \
        }\
        return node;\
    }\
    return NULL;\
}
DEFINE_synctex_new_unscanned_NODE(box_bdry)

static char * _synctex_abstract_box_bdry(synctex_node_p node);
static void _synctex_display_box_bdry(synctex_node_p node);

static synctex_class_s synctex_class_box_bdry = {
    NULL,                       /*  No scanner yet */
    synctex_node_type_box_bdry, /*  Node type */
    &_synctex_new_box_bdry,     /*  creator */
    &_synctex_free_leaf,        /*  destructor */
    &_synctex_log_tlchv_node,   /*  log */
    &_synctex_display_box_bdry, /*  display */
    &_synctex_abstract_box_bdry,/*  display */
    &synctex_tree_model_spfa,   /*  tree model */
    &synctex_data_model_tlchv,  /*  data model */
    &synctex_tlcpector_default, /*  tlcpector */
    &synctex_inspector_hv,      /*  inspector */
    &synctex_vispector_hv,      /*  vispector */
};

#	ifdef SYNCTEX_NOTHING
#       pragma mark hbox proxy.
#   endif

/**
 *  Standard nodes refer to TeX nodes: math, kern, boxes...
 *  Proxy nodes are used to support forms.
 *  A form is parsed as a tree of standard nodes starting
 *  at the top left position.
 *  When a reference is used, the form is duplicated
 *  to the location specified by the reference.
 *  As the same form can be duplicated at different locations,
 *  the geometrical information is relative to its own top left point.
 *  As we need absolute locations, we use proxy nodes.
 *  A proxy node records an offset and the target node.
 *  The target partly acts as a delegate.
 *  The h and v position of the proxy node is the h and v
 *  position of the target shifted by the proxy's offset.
 *  The width, height and depth are not sensitive to offsets.
 *  When are proxies created ?
 *  1)  when the synctex file has been parsed, all the form refs
 *  are replaced by proxies to the content of a form.
 *  This content is a node with siblings (actually none).
 *  Those root proxies have the parent of the ref they replace,
 *  so their parents exist and are no proxy.
 *  Moreover, if they have no sibling, it means that their target have no
 *  sibling as well.
 *  Such nodes are called root proxies.
 *  2)  On the fly, when a proxy is asked for its child
 *  (or sibling) and has none, a proxy to its target's child
 *  (or sibling) is created if any. There are only 2 possible situations:
 *  either the newly created proxy is the child of a proxy,
 *  or it is the sibling of a proxy created on the fly.
 *  In both cases, the parent is a proxy with children.
 *  Such nodes are called child proxies.
 *  How to compute the offset of a proxy ?
 *  The offset of root proxy objects is exactly
 *  the offset of the ref they replace.
 *  The offset of other proxies is their owner's,
 *  except when pointing to a root proxy.
 *  What happens for cascading forms ?
 *  Here is an example diagram
 *
 *  At parse time, the arrow means "owns":
 *  sheet0 -> ref_to1
 *
 *            target1 -> ref_to2
 *
 *                       target2 -> child22
 *
 *  After replacing the refs:
 *  sheet0 -> proxy00 -> proxy01 -> proxy02
 *               |          |          |
 *            target1 -> proxy11 -> proxy12
 *                          |          |
 *                       target2 -> proxy22
 *
 *  proxy00, proxy11 and proxy22 are root proxies.
 *  Their offset is the one of the ref they replace
 *  proxy01, proxy02 and proxy12 are child proxies.
 *  Their proxy is the one of their parent.
 *  Optimization.
 *  After all the refs are replaced, there are only root nodes
 *  targeting standard node. We make sure that each child proxy
 *  also targets a standard node.
 *  It is possible for a proxy to have a standard sibling 
 *  whereas its target has no sibling at all. Root proxies
 *  are such nodes, and are the only ones.
 *  The consequence is that proxies created on the fly
 *  must take into account this situation.
 */

/*  A proxy to a hbox.
 *  A proxy do have a target, which can be a proxy
 */

static const synctex_tree_model_s synctex_tree_model_proxy_hbox = {
    synctex_tree_sibling_idx,       /* sibling */
    synctex_tree_s_parent_idx,      /* parent */
    synctex_tree_sp_child_idx,      /* child */
    synctex_tree_spc_friend_idx,    /* friend */
    synctex_tree_spcf_last_idx,     /* last */
    synctex_tree_spcfl_next_hbox_idx,   /* next_hbox */
    -1, /* arg_sibling */
    synctex_tree_spcfln_target_idx, /* target */
    synctex_tree_spcflnt_proxy_hbox_max
};
static const synctex_data_model_s synctex_data_model_proxy = {
    -1, /* tag */
    -1, /* line */
    -1, /* column */
    synctex_data_proxy_h_idx, /* h */
    synctex_data_proxy_v_idx, /* v */
    -1, /* width */
    -1, /* height */
    -1, /* depth */
    -1, /* mean_line */
    -1, /* weight */
    -1, /* h_V */
    -1, /* v_V */
    -1, /* width_V */
    -1, /* height_V */
    -1, /* depth_V */
    -1, /* name */
    -1, /* page */
    synctex_data_proxy_hv_max
};
typedef struct {
    SYNCTEX_DECLARE_CHARINDEX
    synctex_class_p class_;
    synctex_data_u data[synctex_tree_spcflnt_proxy_hbox_max+synctex_data_proxy_hv_max];
} synctex_node_proxy_hbox_s;

/*  box proxy node creator */
DEFINE_synctex_new_unscanned_NODE(proxy_hbox)

static void _synctex_log_proxy(synctex_node_p node);
static char * _synctex_abstract_proxy_hbox(synctex_node_p node);
static void _synctex_display_proxy_hbox(synctex_node_p node);

static int _synctex_proxy_tag(synctex_node_p);
static int _synctex_proxy_line(synctex_node_p);
static int _synctex_proxy_column(synctex_node_p);

static synctex_tlcpector_s synctex_tlcpector_proxy = {
    &_synctex_proxy_tag,
    &_synctex_proxy_line,
    &_synctex_proxy_column,
};
static int _synctex_proxy_h(synctex_node_p);
static int _synctex_proxy_v(synctex_node_p);
static int _synctex_proxy_width(synctex_node_p);
static int _synctex_proxy_height(synctex_node_p);
static int _synctex_proxy_depth(synctex_node_p);
static synctex_inspector_s synctex_inspector_proxy_box = {
    &_synctex_proxy_h,
    &_synctex_proxy_v,
    &_synctex_proxy_width,
    &_synctex_proxy_height,
    &_synctex_proxy_depth,
};

static float __synctex_proxy_visible_h(synctex_node_p);
static float __synctex_proxy_visible_v(synctex_node_p);
static float __synctex_proxy_visible_width(synctex_node_p);
static float __synctex_proxy_visible_height(synctex_node_p);
static float __synctex_proxy_visible_depth(synctex_node_p);

static synctex_vispector_s synctex_vispector_proxy_box = {
    &__synctex_proxy_visible_h,
    &__synctex_proxy_visible_v,
    &__synctex_proxy_visible_width,
    &__synctex_proxy_visible_height,
    &__synctex_proxy_visible_depth,
};

static synctex_class_s synctex_class_proxy_hbox = {
    NULL,                           /*  No scanner yet */
    synctex_node_type_proxy_hbox,   /*  Node type */
    &_synctex_new_proxy_hbox,       /*  creator */
    &_synctex_free_node,            /*  destructor */
    &_synctex_log_proxy,            /*  log */
    &_synctex_display_proxy_hbox,   /*  display */
    &_synctex_abstract_proxy_hbox,  /*  abstract */
    &synctex_tree_model_proxy_hbox, /*  tree model */
    &synctex_data_model_proxy,      /*  data model */
    &synctex_tlcpector_proxy,       /*  tlcpector */
    &synctex_inspector_proxy_box,   /*  inspector */
    &synctex_vispector_proxy_box,   /*  vispector */
};

#	ifdef SYNCTEX_NOTHING
#       pragma mark vbox proxy.
#   endif

/*  A proxy to a vbox. */

static const synctex_tree_model_s synctex_tree_model_proxy_vbox = {
    synctex_tree_sibling_idx,       /* sibling */
    synctex_tree_s_parent_idx,      /* parent */
    synctex_tree_sp_child_idx,      /* child */
    synctex_tree_spc_friend_idx,    /* friend */
    synctex_tree_spcf_last_idx, /* last */
    -1, /* next_hbox */
    -1, /* arg_sibling */
    synctex_tree_spcfl_target_idx,    /* target */
    synctex_tree_spcflt_proxy_vbox_max
};

typedef struct {
    SYNCTEX_DECLARE_CHARINDEX
    synctex_class_p class_;
    synctex_data_u data[synctex_tree_spcflt_proxy_vbox_max+synctex_data_proxy_hv_max];
} synctex_node_proxy_vbox_s;

/*  box proxy node creator */
DEFINE_synctex_new_unscanned_NODE(proxy_vbox)

static void _synctex_log_proxy(synctex_node_p node);
static char * _synctex_abstract_proxy_vbox(synctex_node_p node);
static void _synctex_display_proxy_vbox(synctex_node_p node);

static synctex_class_s synctex_class_proxy_vbox = {
    NULL,                           /*  No scanner yet */
    synctex_node_type_proxy_vbox,   /*  Node type */
    &_synctex_new_proxy_vbox,       /*  creator */
    &_synctex_free_node,            /*  destructor */
    &_synctex_log_proxy,            /*  log */
    &_synctex_display_proxy_vbox,   /*  display */
    &_synctex_abstract_proxy_vbox,  /*  abstract */
    &synctex_tree_model_proxy_vbox, /*  tree model */
    &synctex_data_model_proxy,      /*  data model */
    &synctex_tlcpector_proxy,       /*  tlcpector */
    &synctex_inspector_proxy_box,   /*  inspector */
    &synctex_vispector_proxy_box,   /*  vispector */
};

#	ifdef SYNCTEX_NOTHING
#       pragma mark proxy.
#   endif

/**
 *  A proxy to a node but a box.
 */

static const synctex_tree_model_s synctex_tree_model_proxy = {
    synctex_tree_sibling_idx,   /* sibling */
    synctex_tree_s_parent_idx,  /* parent */
    -1, /* child */
    synctex_tree_sp_friend_idx, /* friend */
    -1, /* last */
    -1, /* next_hbox */
    -1, /* arg_sibling */
    synctex_tree_spf_target_idx,/* target */
    synctex_tree_spft_proxy_max
};

typedef struct {
    SYNCTEX_DECLARE_CHARINDEX
    synctex_class_p class_;
    synctex_data_u data[synctex_tree_spft_proxy_max+synctex_data_proxy_hv_max];
} synctex_node_proxy_s;

/*  proxy node creator */
DEFINE_synctex_new_unscanned_NODE(proxy)

static void _synctex_log_proxy(synctex_node_p node);
static char * _synctex_abstract_proxy(synctex_node_p node);
static void _synctex_display_proxy(synctex_node_p node);

static synctex_vispector_s synctex_vispector_proxy = {
    &__synctex_proxy_visible_h,
    &__synctex_proxy_visible_v,
    &__synctex_proxy_visible_width,
    &_synctex_float_none,
    &_synctex_float_none,
};

static synctex_class_s synctex_class_proxy = {
    NULL,                       /*  No scanner yet */
    synctex_node_type_proxy,    /*  Node type */
    &_synctex_new_proxy,        /*  creator */
    &_synctex_free_leaf,        /*  destructor */
    &_synctex_log_proxy,        /*  log */
    &_synctex_display_proxy,    /*  display */
    &_synctex_abstract_proxy,   /*  abstract */
    &synctex_tree_model_proxy,  /*  tree model */
    &synctex_data_model_proxy,  /*  data model */
    &synctex_tlcpector_proxy,   /*  tlcpector */
    &synctex_inspector_proxy_box,   /*  inspector */
    &synctex_vispector_proxy,   /*  vispector */
};

#	ifdef SYNCTEX_NOTHING
#       pragma mark last proxy.
#   endif

/**
 *  A proxy to the last proxy/box boundary.
 */

static const synctex_tree_model_s synctex_tree_model_proxy_last = {
    synctex_tree_sibling_idx,   /* sibling */
    synctex_tree_s_parent_idx,  /* parent */
    -1, /* child */
    synctex_tree_sp_friend_idx, /* friend */
    -1, /* last */
    -1, /* next_hbox */
    synctex_tree_spf_arg_sibling_idx, /* arg_sibling */
    synctex_tree_spfa_target_idx,     /* target */
    synctex_tree_spfat_proxy_last_max
};

typedef struct {
    SYNCTEX_DECLARE_CHARINDEX
    synctex_class_p class_;
    synctex_data_u data[synctex_tree_spfat_proxy_last_max+synctex_data_proxy_hv_max];
} synctex_node_proxy_last_s;

/*  proxy node creator */
DEFINE_synctex_new_unscanned_NODE(proxy_last)

static void _synctex_log_proxy(synctex_node_p node);
static char * _synctex_abstract_proxy(synctex_node_p node);
static void _synctex_display_proxy(synctex_node_p node);

static synctex_class_s synctex_class_proxy_last = {
    NULL,                           /*  No scanner yet */
    synctex_node_type_proxy_last,   /*  Node type */
    &_synctex_new_proxy,            /*  creator */
    &_synctex_free_leaf,            /*  destructor */
    &_synctex_log_proxy,            /*  log */
    &_synctex_display_proxy,        /*  display */
    &_synctex_abstract_proxy,       /*  abstract */
    &synctex_tree_model_proxy_last, /*  tree model */
    &synctex_data_model_proxy,      /*  data model */
    &synctex_tlcpector_proxy,       /*  tlcpector */
    &synctex_inspector_proxy_box,       /*  inspector */
    &synctex_vispector_proxy,       /*  vispector */
};

#	ifdef SYNCTEX_NOTHING
#       pragma mark handle.
#   endif

/**
 *  A handle node.
 *  A handle is never the target of a proxy
 *  or another handle.
 *  The child of a handle is always a handle if any.
 *  The sibling of a handle is always a handle if any.
 *  The parent of a handle is always a handle if any.
 */

static const synctex_tree_model_s synctex_tree_model_handle = {
    synctex_tree_sibling_idx,   /* sibling */
    synctex_tree_s_parent_idx,  /* parent */
    synctex_tree_sp_child_idx,  /* child */
    -1, /* friend */
    -1, /* last */
    -1, /* next_hbox */
    -1, /* arg_sibling */
    synctex_tree_spc_target_idx,/* target */
    synctex_tree_spct_handle_max
};

static const synctex_data_model_s synctex_data_model_handle = {
    -1, /* tag */
    -1, /* line */
    -1, /* column */
    -1, /* h */
    -1, /* v */
    -1, /* width */
    -1, /* height */
    -1, /* depth */
    -1, /* mean_line */
    synctex_data_handle_w_idx, /* weight */
    -1, /* h_V */
    -1, /* v_V */
    -1, /* width_V */
    -1, /* height_V */
    -1, /* depth_V */
    -1, /* name */
    -1, /* page */
    synctex_data_handle_w_max
};

typedef struct {
    SYNCTEX_DECLARE_CHARINDEX
    synctex_class_p class_;
    synctex_data_u data[synctex_tree_spct_handle_max+synctex_data_handle_w_max];
} synctex_node_handle_s;

/*  handle node creator */
DEFINE_synctex_new_unscanned_NODE(handle)

static void _synctex_log_handle(synctex_node_p node);
static char * _synctex_abstract_handle(synctex_node_p node);
static void _synctex_display_handle(synctex_node_p node);

static synctex_class_s synctex_class_handle = {
    NULL,                       /*  No scanner yet */
    synctex_node_type_handle,   /*  Node type */
    &_synctex_new_handle,       /*  creator */
    &_synctex_free_handle,      /*  destructor */
    &_synctex_log_handle,       /*  log */
    &_synctex_display_handle,   /*  display */
    &_synctex_abstract_handle,  /*  abstract */
    &synctex_tree_model_handle, /*  tree model */
    &synctex_data_model_handle, /*  data model */
    &synctex_tlcpector_proxy,   /*  tlcpector */
    &synctex_inspector_proxy_box,   /*  inspector */
    &synctex_vispector_proxy_box,   /*  vispector */
};

SYNCTEX_INLINE static synctex_node_p _synctex_new_handle_with_target(synctex_node_p target) {
    if (target) {
        synctex_node_p result = _synctex_new_handle(target->class_->scanner);
        if (result) {
            _synctex_tree_set_target(result,target);
            return result;
        }
    }
    return NULL;
}
SYNCTEX_INLINE static synctex_node_p _synctex_new_handle_with_child(synctex_node_p child) {
    if (child) {
        synctex_node_p result = _synctex_new_handle(child->class_->scanner);
        if (result) {
            _synctex_tree_set_child(result,child);
            return result;
        }
    }
    return NULL;
}

#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark Navigation
#   endif
synctex_node_p synctex_node_parent(synctex_node_p node)
{
    return _synctex_tree_parent(node);
}
synctex_node_p synctex_node_parent_sheet(synctex_node_p node)
{
    while(node && synctex_node_type(node) != synctex_node_type_sheet) {
        node = _synctex_tree_parent(node);
    }
    /*  exit the while loop either when node is NULL or node is a sheet */
    return node;
}
synctex_node_p synctex_node_parent_form(synctex_node_p node)
{
    while(node && synctex_node_type(node) != synctex_node_type_form) {
        node = _synctex_tree_parent(node);
    }
    /*  exit the while loop either when node is NULL or node is a form */
    return node;
}

/**
 *  The returned proxy will be the child or a sibling of source.
 *  The returned proxy has no parent, child nor sibling.
 *  Used only by __synctex_replace_ref.
 *  argument to_node: a box, not a proxy nor anything else.
 */
SYNCTEX_INLINE static synctex_node_p __synctex_new_proxy_from_ref_to(synctex_node_p ref, synctex_node_p to_node) {
    synctex_node_p proxy = NULL;
    if (!ref || !to_node) {
        return NULL;
    }
    switch(synctex_node_type(to_node)) {
        case synctex_node_type_vbox:
            proxy = _synctex_new_proxy_vbox(ref->class_->scanner);
            break;
        case synctex_node_type_hbox:
            proxy = _synctex_new_proxy_hbox(ref->class_->scanner);
            break;
        default:
            _synctex_error("!  __synctex_new_proxy_from_ref_to. Unexpected form child (%s). Please report.", synctex_node_isa(to_node));
            return NULL;
    }
    if (!proxy) {
        _synctex_error("!  __synctex_new_proxy_from_ref_to. Internal error. Please report.");
        return NULL;
    }
    _synctex_data_set_h(proxy, _synctex_data_h(ref));
    _synctex_data_set_v(proxy, _synctex_data_v(ref)-_synctex_data_height(to_node));
    _synctex_tree_set_target(proxy,to_node);
#   if defined(SYNCTEX_USE_CHARINDEX)
    proxy->line_index=to_node?to_node->line_index:0;
    proxy->char_index=to_node?to_node->char_index:0;
#   endif
    return proxy;
}
/**
 *  The returned proxy will be the child or a sibling of owning_proxy.
 *  The returned proxy has no parent, nor child.
 *  Used only by synctex_node_child and synctex_node_sibling
 *  to create proxies on the fly.
 *  If the to_node has an already computed sibling,
 *  then the returned proxy has itself a sibling
 *  pointing to that already computed sibling.
 */
SYNCTEX_INLINE static synctex_node_p __synctex_new_child_proxy_to(synctex_node_p owner, synctex_node_p to_node) {
    synctex_node_p proxy = NULL;
    synctex_node_p target = to_node;
    if (!owner) {
        return NULL;
    }
    switch(synctex_node_type(target)) {
        case synctex_node_type_vbox:
            if ((proxy = _synctex_new_proxy_vbox(owner->class_->scanner))) {
            exit_standard:
                _synctex_data_set_h(proxy, _synctex_data_h(owner));
                _synctex_data_set_v(proxy, _synctex_data_v(owner));
            exit0:
                _synctex_tree_set_target(proxy,target);
#   if defined(SYNCTEX_USE_CHARINDEX)
                proxy->line_index=to_node?to_node->line_index:0;
                proxy->char_index=to_node?to_node->char_index:0;
#   endif
                return proxy;
            };
            break;
        case synctex_node_type_proxy_vbox:
            if ((proxy = _synctex_new_proxy_vbox(owner->class_->scanner))) {
            exit_proxy:
                target = _synctex_tree_target(to_node);
                _synctex_data_set_h(proxy, _synctex_data_h(owner)+_synctex_data_h(to_node));
                _synctex_data_set_v(proxy, _synctex_data_v(owner)+_synctex_data_v(to_node));
                goto exit0;
            };
            break;
        case synctex_node_type_hbox:
            if ((proxy = _synctex_new_proxy_hbox(owner->class_->scanner))) {
                goto exit_standard;
            };
            break;
        case synctex_node_type_proxy_hbox:
            if ((proxy = _synctex_new_proxy_hbox(owner->class_->scanner))) {
                goto exit_proxy;
            };
            break;
        case synctex_node_type_proxy:
        case synctex_node_type_proxy_last:
            if ((proxy = _synctex_new_proxy(owner->class_->scanner))) {
                goto exit_proxy;
            };
            break;
        default:
            if ((proxy = _synctex_new_proxy(owner->class_->scanner))) {
                goto exit_standard;
            };
            break;
    }
    _synctex_error("!  __synctex_new_child_proxy_to. "
                   "Internal error. "
                   "Please report.");
    return NULL;
}
SYNCTEX_INLINE static synctex_node_p _synctex_tree_set_sibling(synctex_node_p node, synctex_node_p new_sibling);
typedef struct synctex_nns_t {
    synctex_node_p first;
    synctex_node_p last;
    synctex_status_t status;
} synctex_nns_s;
/**
 *  Given a target node, create a list of proxies.
 *  The first proxy points to the target node,
 *  its sibling points to the target's sibling and so on.
 *  Returns the first created proxy, the last one and
 *  an error status.
 */
SYNCTEX_INLINE static synctex_nns_s _synctex_new_child_proxies_to(synctex_node_p owner, synctex_node_p to_node) {
    synctex_nns_s nns = {NULL,NULL,SYNCTEX_STATUS_OK};
    if ((nns.first = nns.last = __synctex_new_child_proxy_to(owner,to_node))) {
        synctex_node_p to_next_sibling = __synctex_tree_sibling(to_node);
        synctex_node_p to_sibling;
        while ((to_sibling = to_next_sibling)) {
            synctex_node_p sibling;
            if ((to_next_sibling = __synctex_tree_sibling(to_sibling))) {
                /*  This is not the last sibling */
                if((sibling = __synctex_new_child_proxy_to(owner,to_sibling))) {
                    _synctex_tree_set_sibling(nns.last,sibling);
                    nns.last = sibling;
                    continue;
                } else {
                    _synctex_error("!  _synctex_new_child_proxy_to. "
                                   "Internal error (1). "
                                   "Please report.");
                    nns.status = SYNCTEX_STATUS_ERROR;
                }
            } else if((sibling = _synctex_new_proxy_last(owner->class_->scanner))) {
                _synctex_tree_set_sibling(nns.last,sibling);
                nns.last = sibling;
                _synctex_data_set_h(nns.last, _synctex_data_h(nns.first));
                _synctex_data_set_v(nns.last, _synctex_data_v(nns.first));
                _synctex_tree_set_target(nns.last,to_sibling);
#   if defined(SYNCTEX_USE_CHARINDEX)
                nns.last->line_index=to_sibling->line_index;
                nns.last->char_index=to_sibling->char_index;
#   endif
            } else {
                _synctex_error("!  _synctex_new_child_proxy_to. "
                               "Internal error (2). "
                               "Please report.");
                nns.status = SYNCTEX_STATUS_ERROR;
            }
            break;
        }
    }
    return nns;
}
static char * _synctex_node_abstract(synctex_node_p node);
SYNCTEX_INLINE static synctex_node_p synctex_tree_set_friend(synctex_node_p node,synctex_node_p new_friend) {
#if SYNCTEX_DEBUG
    synctex_node_p F = new_friend;
    while (F) {
        if (node == F) {
            printf("THIS IS AN ERROR\n");
            F = new_friend;
            while (F) {
                printf("%s\n",_synctex_node_abstract(F));
                if (node == F) {
                    return NULL;
                }
                F = _synctex_tree_friend(F);
            }
            return NULL;
        }
        F = _synctex_tree_friend(F);
    }
#endif
    return new_friend?_synctex_tree_set_friend(node,new_friend):_synctex_tree_reset_friend(node);
}
/**
 *
 */
SYNCTEX_INLINE static synctex_node_p __synctex_node_make_friend(synctex_node_p node, int i) {
    synctex_node_p old = NULL;
    if (i>=0) {
        i = i%(node->class_->scanner->number_of_lists);
        old = synctex_tree_set_friend(node,(node->class_->scanner->lists_of_friends)[i]);
        (node->class_->scanner->lists_of_friends)[i] = node;
#if SYNCTEX_DEBUG>500
        printf("tl(%i)=>",i);
        synctex_node_log(node);
        if (synctex_node_parent_form(node)) {
            printf("!  ERROR. No registration expected!\n");
        }
#endif
    }
    return old;
}
/**
 *  All proxies have tlc attributes, on behalf of their target.
 *  The purpose is to register all af them.
 *  - argument node: is the proxy, must not be NULL
 */
SYNCTEX_INLINE static synctex_node_p __synctex_proxy_make_friend_and_next_hbox(synctex_node_p node) {
    synctex_node_p old = NULL;
    synctex_node_p target = _synctex_tree_target(node);
    if (target) {
        int i = _synctex_data_tag(target)+_synctex_data_line(target);
        old = __synctex_node_make_friend(node,i);
    } else {
        old = __synctex_tree_reset_friend(node);
    }
    if (synctex_node_type(node) == synctex_node_type_proxy_hbox) {
        synctex_node_p sheet = synctex_node_parent_sheet(node);
        if (sheet) {
            _synctex_tree_set_next_hbox(node,_synctex_tree_next_hbox(sheet));
            _synctex_tree_set_next_hbox(sheet,node);
        }
    }
    return old;
}
/**
 *  Register a node which have tag, line and column.
 *  - argument node: the node
 */
SYNCTEX_INLINE static synctex_node_p __synctex_node_make_friend_tlc(synctex_node_p node) {
    int i = synctex_node_tag(node)+synctex_node_line(node);
    return __synctex_node_make_friend(node,i);
}
/**
 *  Register a node which have tag, line and column.
 *  Does nothing if the argument is NULL.
 *  Calls __synctex_node_make_friend_tlc.
 *  - argument node: the node
 */
SYNCTEX_INLINE static void _synctex_node_make_friend_tlc(synctex_node_p node) {
    if (node) {
        __synctex_node_make_friend_tlc(node);
    }
}
static synctex_node_p _synctex_node_set_child(synctex_node_p node, synctex_node_p new_child);
/**
 *  The (first) child of the node, if any, NULL otherwise.
 *  At parse time, non void box nodes have children.
 *  All other nodes have no children.
 *  In order to support pdf forms, proxies are created
 *  to place form nodes at real locations.
 *  Ref nodes are replaced by root proxies targeting
 *  form contents. If root proxies have no children,
 *  they are created on the fly as proxies to the
 *  children of the targeted box.
 *  As such, proxies created here are targeting a
 *  node that belongs to a form.
 *  This is the only place where child proxies are created.
 */
synctex_node_p synctex_node_child(synctex_node_p node) {
    synctex_node_p child = NULL;
    synctex_node_p target = NULL;
    if ((child = _synctex_tree_child(node))) {
        return child;
    } else if ((target = _synctex_tree_target(node))) {
        if ((child = synctex_node_child(target))) {
            /*  This is a proxy with no child
             *  which target does have a child. */
            synctex_nns_s nns = _synctex_new_child_proxies_to(node, child);
            if (nns.first) {
                _synctex_node_set_child(node,nns.first);
                return nns.first;
            } else {
                _synctex_error("!  synctex_node_child. Internal inconsistency. Please report.");
            }
        }
    }
    return NULL;
}
/*
 *  Set the parent/child bound.
 *  Things get complicated when new_child has siblings.
 *  The caller is responsible for releasing the returned value.
 */
static synctex_node_p _synctex_node_set_child(synctex_node_p parent, synctex_node_p new_child) {
    if (parent) {
        synctex_node_p old = _synctex_tree_set_child(parent,new_child);
        synctex_node_p last_child = NULL;
        synctex_node_p child;
        if ((child = old)) {
            do {
                _synctex_tree_reset_parent(child);
            } while ((child = __synctex_tree_sibling(child)));
        }
        if ((child = new_child)) {
            do {
                _synctex_tree_set_parent(child,parent);
                last_child = child;
            } while ((child = __synctex_tree_sibling(child)));
        }
        _synctex_tree_set_last(parent,last_child);
        return old;
    }
    return NULL;
}

/*  The last child of the given node, or NULL.
 */
synctex_node_p synctex_node_last_child(synctex_node_p node) {
    return _synctex_tree_last(node);
}
/**
 *  All nodes siblings are properly set up at parse time
 *  except for non root proxies.
 */
synctex_node_p synctex_node_sibling(synctex_node_p node) {
    return node? __synctex_tree_sibling(node): NULL;
}
/**
 *  All the _synctex_tree_... methods refer to the tree model.
 *  __synctex_tree_... methods are low level.
 */
/**
 *  Replace the sibling.
 *  Connect to the arg_sibling of the new_sibling if relevant.
 *  - returns the old sibling.
 *  The caller is responsible for releasing the old sibling.
 *  The bound to the parent is managed below.
 */
SYNCTEX_INLINE static synctex_node_p _synctex_tree_set_sibling(synctex_node_p node, synctex_node_p new_sibling) {
    if (node == new_sibling) {
        printf("BOF\n");
    }
    synctex_node_p old = node? __synctex_tree_set_sibling(node,new_sibling): NULL;
    _synctex_tree_set_arg_sibling(new_sibling,node);
    return old;
}
/**
 *  Replace the sibling.
 *  Set the parent of the new sibling (and further siblings)
 *  to the parent of the receiver.
 *  Also set the last sibling of parent.
 *  - argument new_sibling: must not be NULL.
 *  - returns the old sibling.
 *  The caller is responsible for releasing the old sibling.
 */
static synctex_node_p _synctex_node_set_sibling(synctex_node_p node, synctex_node_p new_sibling) {
    if (node && new_sibling) {
        synctex_node_p old = _synctex_tree_set_sibling(node,new_sibling);
        if (_synctex_tree_has_parent(node)) {
            synctex_node_p parent = __synctex_tree_parent(node);
            if (parent) {
                synctex_node_p N = new_sibling;
                while (synctex_YES) {
                    if (_synctex_tree_has_parent(N)) {
                        __synctex_tree_set_parent(N,parent);
                        _synctex_tree_set_last(parent,N);
                        N = __synctex_tree_sibling(N);
                        continue;
                    } else if (N) {
                        _synctex_error("!  synctex_node_sibling. "
                                       "Internal inconsistency. "
                                       "Please report.");
                    }
                    break;
                }
            }
        }
        return old;
    }
    return NULL;
}
/**
 *  The last sibling of the given node, or NULL with node.
 */
synctex_node_p synctex_node_last_sibling(synctex_node_p node) {
    synctex_node_p sibling;
    do {
        sibling = node;
    } while((node = synctex_node_sibling(node)));
    return sibling;
}
/**
 *  The next nodes corresponds to a deep first tree traversal.
 *  Does not create child proxies as side effect contrary to
 *  the synctex_node_next method above.
 *  May loop infinitely many times if the tree
 *  is not properly built (contains loops).
 */
SYNCTEX_INLINE static synctex_node_p _synctex_node_sibling_or_parents(synctex_node_p node) {
    while (node) {
        synctex_node_p N;
        if ((N = __synctex_tree_sibling(node))) {
            return N;
        } else if ((node = _synctex_tree_parent(node))) {
            if (synctex_node_type(node) == synctex_node_type_sheet) {/*  EXC_BAD_ACCESS? */
                return NULL;
            } else if (synctex_node_type(node) == synctex_node_type_form) {
                return NULL;
            }
        } else {
            return NULL;
        }
    }
    return NULL;
}
/**
 *  The next nodes corresponds to a deep first tree traversal.
 *  Creates child proxies as side effect.
 *  May loop infinitely many times if the tree
 *  is not properly built (contains loops).
 */
synctex_node_p synctex_node_next(synctex_node_p node) {
    synctex_node_p N = synctex_node_child(node);
    if (N) {
        return N;
    }
    return _synctex_node_sibling_or_parents(node);
}
/**
 *  The node which argument is the sibling.
 *  - return: NULL if the argument has no parent or
 *      is the first child of its parent.
 *  - Input nodes have no arg siblings
 */
synctex_node_p synctex_node_arg_sibling(synctex_node_p node) {
#if 1
    return _synctex_tree_arg_sibling(node);
#else
    synctex_node_p N = _synctex_tree_parent(node);
    if ((N = _synctex_tree_child(N))) {
        do {
            synctex_node_p NN = __synctex_tree_sibling(N);
            if (NN == node) {
                return N;
            }
            N = NN;
        } while (N);
    }
    return N;
#endif
}
#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark CLASS
#   endif

/*  Public node accessor: the type  */
synctex_node_type_t synctex_node_type(synctex_node_p node) {
    return node? node->class_->type: synctex_node_type_none;
}

/*  Public node accessor: the type  */
synctex_node_type_t synctex_node_target_type(synctex_node_p node) {
    synctex_node_p target = _synctex_tree_target(node);
    if (target) {
        return (((target)->class_))->type;
    } else if (node) {
        return (((node)->class_))->type;
    }
    return synctex_node_type_none;
}

/*  Public node accessor: the human readable type  */
const char * synctex_node_isa(synctex_node_p node) {
    static const char * isa[synctex_node_number_of_types] =
    {"Not a node",
        "input",
        "sheet",
        "form",
        "ref",
        "vbox",
        "void vbox",
        "hbox",
        "void hbox",
        "kern",
        "glue",
        "rule",
        "math",
        "boundary",
        "box_bdry",
        "proxy",
        "last proxy",
        "vbox proxy",
        "hbox proxy",
        "handle"};
    return isa[synctex_node_type(node)];
}

#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark LOG
#   endif

/*  Public node logger  */
void synctex_node_log(synctex_node_p node) {
    SYNCTEX_MSG_SEND(node,log);
}

static void _synctex_log_input(synctex_node_p node) {
    if (node) {
        printf("%s:%i,%s(%i)\n",synctex_node_isa(node),
               _synctex_data_tag(node),
               _synctex_data_name(node),
               _synctex_data_line(node));
        printf("SELF:%p\n",(void *)node);
        printf("    SIBLING:%p\n",
               (void *)__synctex_tree_sibling(node));
    }
}

static void _synctex_log_sheet(synctex_node_p node) {
    if (node) {
        printf("%s:%i",synctex_node_isa(node),_synctex_data_page(node));
        SYNCTEX_PRINT_CHARINDEX_NL;
        printf("SELF:%p\n",(void *)node);
        printf("    SIBLING:%p\n",(void *)__synctex_tree_sibling(node));
        printf("    PARENT:%p\n",(void *)_synctex_tree_parent(node));
        printf("    CHILD:%p\n",(void *)_synctex_tree_child(node));
        printf("    LEFT:%p\n",(void *)_synctex_tree_friend(node));
        printf("    NEXT_hbox:%p\n",(void *)_synctex_tree_next_hbox(node));
    }
}

static void _synctex_log_form(synctex_node_p node) {
    if (node) {
        printf("%s:%i",synctex_node_isa(node),_synctex_data_tag(node));
        SYNCTEX_PRINT_CHARINDEX_NL;
        printf("SELF:%p\n",(void *)node);
        printf("    SIBLING:%p\n",(void *)__synctex_tree_sibling(node));
        printf("    PARENT:%p\n",(void *)_synctex_tree_parent(node));
        printf("    CHILD:%p\n",(void *)_synctex_tree_child(node));
        printf("    LEFT:%p\n",(void *)_synctex_tree_friend(node));
    }
}

static void _synctex_log_ref(synctex_node_p node) {
    if (node) {
        printf("%s:%i:%i,%i",
               synctex_node_isa(node),
               _synctex_data_tag(node),
               _synctex_data_h(node),
               _synctex_data_v(node));
        SYNCTEX_PRINT_CHARINDEX_NL;
        printf("SELF:%p\n",(void *)node);
        printf("    SIBLING:%p\n",(void *)__synctex_tree_sibling(node));
        printf("    PARENT:%p\n",(void *)_synctex_tree_parent(node));
    }
}

static void _synctex_log_tlchv_node(synctex_node_p node) {
    if (node) {
        printf("%s:%i,%i,%i:%i,%i",
               synctex_node_isa(node),
               _synctex_data_tag(node),
               _synctex_data_line(node),
               _synctex_data_column(node),
               _synctex_data_h(node),
               _synctex_data_v(node));
        SYNCTEX_PRINT_CHARINDEX_NL;
        printf("SELF:%p\n",(void *)node);
        printf("    SIBLING:%p\n",(void *)__synctex_tree_sibling(node));
        printf("    PARENT:%p\n",(void *)_synctex_tree_parent(node));
        printf("    CHILD:%p\n",(void *)_synctex_tree_child(node));
        printf("    LEFT:%p\n",(void *)_synctex_tree_friend(node));
    }
}

static void _synctex_log_kern_node(synctex_node_p node) {
    if (node) {
        printf("%s:%i,%i,%i:%i,%i:%i",
               synctex_node_isa(node),
               _synctex_data_tag(node),
               _synctex_data_line(node),
               _synctex_data_column(node),
               _synctex_data_h(node),
               _synctex_data_v(node),
               _synctex_data_width(node));
        SYNCTEX_PRINT_CHARINDEX_NL;
        printf("SELF:%p\n",(void *)node);
        printf("    SIBLING:%p\n",(void *)__synctex_tree_sibling(node));
        printf("    PARENT:%p\n",(void *)_synctex_tree_parent(node));
        printf("    CHILD:%p\n",(void *)_synctex_tree_child(node));
        printf("    LEFT:%p\n",(void *)_synctex_tree_friend(node));
    }
}

static void _synctex_log_rule(synctex_node_p node) {
    if (node) {
        printf("%s:%i,%i,%i:%i,%i",
               synctex_node_isa(node),
               _synctex_data_tag(node),
               _synctex_data_line(node),
               _synctex_data_column(node),
               _synctex_data_h(node),
               _synctex_data_v(node));
        printf(":%i",_synctex_data_width(node));
        printf(",%i",_synctex_data_height(node));
        printf(",%i",_synctex_data_depth(node));
        SYNCTEX_PRINT_CHARINDEX_NL;
        printf("SELF:%p\n",(void *)node);
        printf("    SIBLING:%p\n",(void *)__synctex_tree_sibling(node));
        printf("    PARENT:%p\n",(void *)_synctex_tree_parent(node));
        printf("    LEFT:%p\n",(void *)_synctex_tree_friend(node));
    }
}

static void _synctex_log_void_box(synctex_node_p node) {
    if (node) {
        printf("%s",synctex_node_isa(node));
        printf(":%i",_synctex_data_tag(node));
        printf(",%i",_synctex_data_line(node));
        printf(",%i",_synctex_data_column(node));
        printf(":%i",_synctex_data_h(node));
        printf(",%i",_synctex_data_v(node));
        printf(":%i",_synctex_data_width(node));
        printf(",%i",_synctex_data_height(node));
        printf(",%i",_synctex_data_depth(node));
        SYNCTEX_PRINT_CHARINDEX_NL;
        printf("SELF:%p\n",(void *)node);
        printf("    SIBLING:%p\n",(void *)__synctex_tree_sibling(node));
        printf("    PARENT:%p\n",(void *)_synctex_tree_parent(node));
        printf("    CHILD:%p\n",(void *)_synctex_tree_child(node));
        printf("    LEFT:%p\n",(void *)_synctex_tree_friend(node));
    }
}

static void _synctex_log_vbox(synctex_node_p node) {
    if (node) {
        printf("%s",synctex_node_isa(node));
        printf(":%i",_synctex_data_tag(node));
        printf(",%i",_synctex_data_line(node));
        printf(",%i",_synctex_data_column(node));
        printf(":%i",_synctex_data_h(node));
        printf(",%i",_synctex_data_v(node));
        printf(":%i",_synctex_data_width(node));
        printf(",%i",_synctex_data_height(node));
        printf(",%i",_synctex_data_depth(node));
        SYNCTEX_PRINT_CHARINDEX_NL;
        printf("SELF:%p\n",(void *)node);
        printf("    SIBLING:%p\n",(void *)__synctex_tree_sibling(node));
        printf("    PARENT:%p\n",(void *)_synctex_tree_parent(node));
        printf("    CHILD:%p\n",(void *)_synctex_tree_child(node));
        printf("    LEFT:%p\n",(void *)_synctex_tree_friend(node));
        printf("    NEXT_hbox:%p\n",(void *)_synctex_tree_next_hbox(node));
    }
}

static void _synctex_log_hbox(synctex_node_p node) {
    if (node) {
        printf("%s",synctex_node_isa(node));
        printf(":%i",_synctex_data_tag(node));
        printf(",%i~%i*%i",_synctex_data_line(node),_synctex_data_mean_line(node),_synctex_data_weight(node));
        printf(",%i",_synctex_data_column(node));
        printf(":%i",_synctex_data_h(node));
        printf(",%i",_synctex_data_v(node));
        printf(":%i",_synctex_data_width(node));
        printf(",%i",_synctex_data_height(node));
        printf(",%i",_synctex_data_depth(node));
        printf("/%i",_synctex_data_h_V(node));
        printf(",%i",_synctex_data_v_V(node));
        printf(":%i",_synctex_data_width_V(node));
        printf(",%i",_synctex_data_height_V(node));
        printf(",%i",_synctex_data_depth_V(node));
        SYNCTEX_PRINT_CHARINDEX_NL;
        printf("SELF:%p\n",(void *)node);
        printf("    SIBLING:%p\n",(void *)__synctex_tree_sibling(node));
        printf("    PARENT:%p\n",(void *)_synctex_tree_parent(node));
        printf("    CHILD:%p\n",(void *)_synctex_tree_child(node));
        printf("    LEFT:%p\n",(void *)_synctex_tree_friend(node));
        printf("    NEXT_hbox:%p\n",(void *)_synctex_tree_next_hbox(node));
    }
}
static void _synctex_log_proxy(synctex_node_p node) {
    if (node) {
        synctex_node_p N = _synctex_tree_target(node);
        printf("%s",synctex_node_isa(node));
        printf(":%i",_synctex_data_h(node));
        printf(",%i",_synctex_data_v(node));
        SYNCTEX_PRINT_CHARINDEX_NL;
        printf("SELF:%p\n",(void *)node);
        printf("    SIBLING:%p\n",(void *)__synctex_tree_sibling(node));
        printf("    LEFT:%p\n",(void *)_synctex_tree_friend(node));
        printf("    ->%s\n",_synctex_node_abstract(N));
    }
}
static void _synctex_log_handle(synctex_node_p node) {
    if (node) {
        synctex_node_p N = _synctex_tree_target(node);
        printf("%s",synctex_node_isa(node));
        SYNCTEX_PRINT_CHARINDEX_NL;
        printf("SELF:%p\n",(void *)node);
        printf("    SIBLING:%p\n",(void *)__synctex_tree_sibling(node));
        printf("    ->%s\n",_synctex_node_abstract(N));
    }
}

#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark SYNCTEX_DISPLAY
#   endif

int synctex_scanner_display_switcher(synctex_scanner_p scanR) {
    return scanR->display_switcher;
}
void synctex_scanner_set_display_switcher(synctex_scanner_p scanR, int switcher) {
    scanR->display_switcher = switcher;
}
static const char * const _synctex_display_prompt = "................................";

static char * _synctex_scanner_display_prompt_down(synctex_scanner_p scanR) {
    if (scanR->display_prompt>_synctex_display_prompt) {
        --scanR->display_prompt;
    }
    return scanR->display_prompt;
}
static char * _synctex_scanner_display_prompt_up(synctex_scanner_p scanR) {
    if (scanR->display_prompt+1<_synctex_display_prompt+strlen(_synctex_display_prompt)) {
        ++scanR->display_prompt;
    }
    return scanR->display_prompt;
}

void synctex_node_display(synctex_node_p node) {
    if (node) {
        synctex_scanner_p scanR = node->class_->scanner;
        if (scanR) {
            if (scanR->display_switcher<0) {
                SYNCTEX_MSG_SEND(node, display);
            } else if (scanR->display_switcher>0 && --scanR->display_switcher>0) {
                SYNCTEX_MSG_SEND(node, display);
            } else if (scanR->display_switcher-->=0) {
                printf("%s Next display skipped. Reset display switcher.\n",node->class_->scanner->display_prompt);
            }
        } else {
            SYNCTEX_MSG_SEND(node, display);
        }
    }
}
static char * _synctex_node_abstract(synctex_node_p node) {
    SYNCTEX_PARAMETER_ASSERT(node || node->class_);
    return (node && node->class_->abstract)? node->class_->abstract(node):"none";
}

SYNCTEX_INLINE static void _synctex_display_child(synctex_node_p node) {
    synctex_node_p N = _synctex_tree_child(node);
    if (N) {
        _synctex_scanner_display_prompt_down(N->class_->scanner);
        synctex_node_display(N);
        _synctex_scanner_display_prompt_up(N->class_->scanner);
    }
}

SYNCTEX_INLINE static void _synctex_display_sibling(synctex_node_p node) {
    synctex_node_display(__synctex_tree_sibling(node));
}
#define SYNCTEX_ABSTRACT_MAX 128
static char * _synctex_abstract_input(synctex_node_p node) {
    static char abstract[SYNCTEX_ABSTRACT_MAX] = "none";
    if (node) {
        snprintf(abstract,SYNCTEX_ABSTRACT_MAX,"Input:%i:%s(%i)" SYNCTEX_PRINT_CHARINDEX_FMT,
               _synctex_data_tag(node),
               _synctex_data_name(node),
               _synctex_data_line(node)
               SYNCTEX_PRINT_CHARINDEX_WHAT);
    }
    return abstract;
}

static void _synctex_display_input(synctex_node_p node) {
    if (node) {
        printf("Input:%i:%s(%i)"
               SYNCTEX_PRINT_CHARINDEX_FMT
               "\n",
               _synctex_data_tag(node),
               _synctex_data_name(node),
               _synctex_data_line(node)
                SYNCTEX_PRINT_CHARINDEX_WHAT);
        synctex_node_display(__synctex_tree_sibling(node));
    }
}

static char * _synctex_abstract_sheet(synctex_node_p node) {
    static char abstract[SYNCTEX_ABSTRACT_MAX] = "none";
    if (node) {
        snprintf(abstract,SYNCTEX_ABSTRACT_MAX,"{%i...}" SYNCTEX_PRINT_CHARINDEX_FMT,
               _synctex_data_page(node)
               SYNCTEX_PRINT_CHARINDEX_WHAT);
    }
    return abstract;
}

static void _synctex_display_sheet(synctex_node_p node) {
    if (node) {
        printf("%s{%i"
               SYNCTEX_PRINT_CHARINDEX_FMT
               "\n",
               node->class_->scanner->display_prompt,
               _synctex_data_page(node)
               SYNCTEX_PRINT_CHARINDEX_WHAT);
        _synctex_display_child(node);
        printf("%s}\n",node->class_->scanner->display_prompt);
        _synctex_display_sibling(node);
    }
}

static char * _synctex_abstract_form(synctex_node_p node) {
    static char abstract[SYNCTEX_ABSTRACT_MAX] = "none";
    if (node) {
        snprintf(abstract,SYNCTEX_ABSTRACT_MAX,"<%i...>" SYNCTEX_PRINT_CHARINDEX_FMT,
               _synctex_data_tag(node)
               SYNCTEX_PRINT_CHARINDEX_WHAT);
        SYNCTEX_PRINT_CHARINDEX;
    }
    return abstract;
}

static void _synctex_display_form(synctex_node_p node) {
    if (node) {
        printf("%s<%i"
               SYNCTEX_PRINT_CHARINDEX_FMT
               "\n",
               node->class_->scanner->display_prompt,
               _synctex_data_tag(node)
               SYNCTEX_PRINT_CHARINDEX_WHAT);
        _synctex_display_child(node);
        printf("%s>\n",node->class_->scanner->display_prompt);
        _synctex_display_sibling(node);
    }
}

static char * _synctex_abstract_vbox(synctex_node_p node) {
    static char abstract[SYNCTEX_ABSTRACT_MAX] = "none";
    if (node) {
        snprintf(abstract,SYNCTEX_ABSTRACT_MAX,"[%i,%i:%i,%i:%i,%i,%i...]"
               SYNCTEX_PRINT_CHARINDEX_FMT,
               _synctex_data_tag(node),
               _synctex_data_line(node),
               _synctex_data_h(node),
               _synctex_data_v(node),
               _synctex_data_width(node),
               _synctex_data_height(node),
               _synctex_data_depth(node)
               SYNCTEX_PRINT_CHARINDEX_WHAT);
    }
    return abstract;
}

static void _synctex_display_vbox(synctex_node_p node) {
    if (node) {
        printf("%s[%i,%i:%i,%i:%i,%i,%i"
               SYNCTEX_PRINT_CHARINDEX_FMT
               "\n",
               node->class_->scanner->display_prompt,
               _synctex_data_tag(node),
               _synctex_data_line(node),
               _synctex_data_h(node),
               _synctex_data_v(node),
               _synctex_data_width(node),
               _synctex_data_height(node),
               _synctex_data_depth(node)
               SYNCTEX_PRINT_CHARINDEX_WHAT);
        _synctex_display_child(node);
        printf("%s]\n%slast:%s\n",
               node->class_->scanner->display_prompt,
               node->class_->scanner->display_prompt,
               _synctex_node_abstract(_synctex_tree_last(node)));
        _synctex_display_sibling(node);
    }
}

static char * _synctex_abstract_hbox(synctex_node_p node) {
    static char abstract[SYNCTEX_ABSTRACT_MAX] = "none";
    if (node) {
        snprintf(abstract,SYNCTEX_ABSTRACT_MAX,"(%i,%i~%i*%i:%i,%i:%i,%i,%i...)"
               SYNCTEX_PRINT_CHARINDEX_FMT,
               _synctex_data_tag(node),
               _synctex_data_line(node),
               _synctex_data_mean_line(node),
               _synctex_data_weight(node),
               _synctex_data_h(node),
               _synctex_data_v(node),
               _synctex_data_width(node),
               _synctex_data_height(node),
               _synctex_data_depth(node)
               SYNCTEX_PRINT_CHARINDEX_WHAT);
    }
    return abstract;
}

static void _synctex_display_hbox(synctex_node_p node) {
    if (node) {
        printf("%s(%i,%i~%i*%i:%i,%i:%i,%i,%i"
               SYNCTEX_PRINT_CHARINDEX_FMT
               "\n",
               node->class_->scanner->display_prompt,
               _synctex_data_tag(node),
               _synctex_data_line(node),
               _synctex_data_mean_line(node),
               _synctex_data_weight(node),
               _synctex_data_h(node),
               _synctex_data_v(node),
               _synctex_data_width(node),
               _synctex_data_height(node),
               _synctex_data_depth(node)
               SYNCTEX_PRINT_CHARINDEX_WHAT);
        _synctex_display_child(node);
        printf("%s)\n%slast:%s\n",
               node->class_->scanner->display_prompt,
               node->class_->scanner->display_prompt,
               _synctex_node_abstract(_synctex_tree_last(node)));
        _synctex_display_sibling(node);
    }
}

static char * _synctex_abstract_void_vbox(synctex_node_p node) {
    static char abstract[SYNCTEX_ABSTRACT_MAX] = "none";
    if (node) {
        snprintf(abstract,SYNCTEX_ABSTRACT_MAX,"v%i,%i;%i,%i:%i,%i,%i"
                       SYNCTEX_PRINT_CHARINDEX_FMT
                       "\n",
               _synctex_data_tag(node),
               _synctex_data_line(node),
               _synctex_data_h(node),
               _synctex_data_v(node),
               _synctex_data_width(node),
               _synctex_data_height(node),
               _synctex_data_depth(node)
                       SYNCTEX_PRINT_CHARINDEX_WHAT);
    }
    return abstract;
}

static void _synctex_display_void_vbox(synctex_node_p node) {
    if (node) {
        printf("%sv%i,%i;%i,%i:%i,%i,%i"
               SYNCTEX_PRINT_CHARINDEX_FMT
               "\n",
               node->class_->scanner->display_prompt,
               _synctex_data_tag(node),
               _synctex_data_line(node),
               _synctex_data_h(node),
               _synctex_data_v(node),
               _synctex_data_width(node),
               _synctex_data_height(node),
               _synctex_data_depth(node)
               SYNCTEX_PRINT_CHARINDEX_WHAT);
        _synctex_display_sibling(node);
    }
}

static char * _synctex_abstract_void_hbox(synctex_node_p node) {
    static char abstract[SYNCTEX_ABSTRACT_MAX] = "none";
    if (node) {
        snprintf(abstract,SYNCTEX_ABSTRACT_MAX,"h%i,%i:%i,%i:%i,%i,%i"
                       SYNCTEX_PRINT_CHARINDEX_FMT,
               _synctex_data_tag(node),
               _synctex_data_line(node),
               _synctex_data_h(node),
               _synctex_data_v(node),
               _synctex_data_width(node),
               _synctex_data_height(node),
               _synctex_data_depth(node)
                       SYNCTEX_PRINT_CHARINDEX_WHAT);
    }
    return abstract;
}

static void _synctex_display_void_hbox(synctex_node_p node) {
    if (node) {
        printf("%sh%i,%i:%i,%i:%i,%i,%i"
               SYNCTEX_PRINT_CHARINDEX_FMT
               "\n",
               node->class_->scanner->display_prompt,
               _synctex_data_tag(node),
               _synctex_data_line(node),
               _synctex_data_h(node),
               _synctex_data_v(node),
               _synctex_data_width(node),
               _synctex_data_height(node),
               _synctex_data_depth(node)
               SYNCTEX_PRINT_CHARINDEX_WHAT);
        _synctex_display_sibling(node);
    }
}

static char * _synctex_abstract_glue(synctex_node_p node) {
    static char abstract[SYNCTEX_ABSTRACT_MAX] = "none";
    if (node) {
        snprintf(abstract,SYNCTEX_ABSTRACT_MAX,"glue:%i,%i:%i,%i"
                       SYNCTEX_PRINT_CHARINDEX_FMT,
               _synctex_data_tag(node),
               _synctex_data_line(node),
               _synctex_data_h(node),
               _synctex_data_v(node)
                       SYNCTEX_PRINT_CHARINDEX_WHAT);
    }
    return abstract;
}

static void _synctex_display_glue(synctex_node_p node) {
    if (node) {
        printf("%sglue:%i,%i:%i,%i"
               SYNCTEX_PRINT_CHARINDEX_FMT
               "\n",
               node->class_->scanner->display_prompt,
               _synctex_data_tag(node),
               _synctex_data_line(node),
               _synctex_data_h(node),
               _synctex_data_v(node)
               SYNCTEX_PRINT_CHARINDEX_WHAT);
        _synctex_display_sibling(node);
    }
}

static char * _synctex_abstract_rule(synctex_node_p node) {
    static char abstract[SYNCTEX_ABSTRACT_MAX] = "none";
    if (node) {
        snprintf(abstract,SYNCTEX_ABSTRACT_MAX,"rule:%i,%i:%i,%i:%i,%i,%i"
               SYNCTEX_PRINT_CHARINDEX_FMT,
               _synctex_data_tag(node),
               _synctex_data_line(node),
               _synctex_data_h(node),
               _synctex_data_v(node),
               _synctex_data_width(node),
               _synctex_data_height(node),
               _synctex_data_depth(node)
               SYNCTEX_PRINT_CHARINDEX_WHAT);
    }
    return abstract;
}

static void _synctex_display_rule(synctex_node_p node) {
    if (node) {
        printf("%srule:%i,%i:%i,%i:%i,%i,%i"
               SYNCTEX_PRINT_CHARINDEX_FMT
               "\n",
               node->class_->scanner->display_prompt,
               _synctex_data_tag(node),
               _synctex_data_line(node),
               _synctex_data_h(node),
               _synctex_data_v(node),
               _synctex_data_width(node),
               _synctex_data_height(node),
               _synctex_data_depth(node)
               SYNCTEX_PRINT_CHARINDEX_WHAT);
        _synctex_display_sibling(node);
    }
}

static char * _synctex_abstract_math(synctex_node_p node) {
    static char abstract[SYNCTEX_ABSTRACT_MAX] = "none";
    if (node) {
        snprintf(abstract,SYNCTEX_ABSTRACT_MAX,"math:%i,%i:%i,%i"
                       SYNCTEX_PRINT_CHARINDEX_FMT,
               _synctex_data_tag(node),
               _synctex_data_line(node),
               _synctex_data_h(node),
               _synctex_data_v(node)
                       SYNCTEX_PRINT_CHARINDEX_WHAT);
    }
    return abstract;
}

static void _synctex_display_math(synctex_node_p node) {
    if (node) {
        printf("%smath:%i,%i:%i,%i"
               SYNCTEX_PRINT_CHARINDEX_FMT
               "\n",
               node->class_->scanner->display_prompt,
               _synctex_data_tag(node),
               _synctex_data_line(node),
               _synctex_data_h(node),
               _synctex_data_v(node)
               SYNCTEX_PRINT_CHARINDEX_WHAT);
        _synctex_display_sibling(node);
    }
}

static char * _synctex_abstract_kern(synctex_node_p node) {
    static char abstract[SYNCTEX_ABSTRACT_MAX] = "none";
    if (node) {
        snprintf(abstract,SYNCTEX_ABSTRACT_MAX,"kern:%i,%i:%i,%i:%i"
                       SYNCTEX_PRINT_CHARINDEX_FMT,
               _synctex_data_tag(node),
               _synctex_data_line(node),
               _synctex_data_h(node),
               _synctex_data_v(node),
               _synctex_data_width(node)
                       SYNCTEX_PRINT_CHARINDEX_WHAT);
    }
    return abstract;
}

static void _synctex_display_kern(synctex_node_p node) {
    if (node) {
        printf("%skern:%i,%i:%i,%i:%i"
               SYNCTEX_PRINT_CHARINDEX_FMT
               "\n",
               node->class_->scanner->display_prompt,
               _synctex_data_tag(node),
               _synctex_data_line(node),
               _synctex_data_h(node),
               _synctex_data_v(node),
               _synctex_data_width(node)
               SYNCTEX_PRINT_CHARINDEX_WHAT);
        _synctex_display_sibling(node);
    }
}

static char * _synctex_abstract_boundary(synctex_node_p node) {
    static char abstract[SYNCTEX_ABSTRACT_MAX] = "none";
    if (node) {
        snprintf(abstract,SYNCTEX_ABSTRACT_MAX,"boundary:%i,%i:%i,%i"
                       SYNCTEX_PRINT_CHARINDEX_FMT,
               _synctex_data_tag(node),
               _synctex_data_line(node),
               _synctex_data_h(node),
               _synctex_data_v(node)
                       SYNCTEX_PRINT_CHARINDEX_WHAT);
    }
    return abstract;
}

static void _synctex_display_boundary(synctex_node_p node) {
    if (node) {
        printf("%sboundary:%i,%i:%i,%i"
               SYNCTEX_PRINT_CHARINDEX_FMT
               "\n",
               node->class_->scanner->display_prompt,
               _synctex_data_tag(node),
               _synctex_data_line(node),
               _synctex_data_h(node),
               _synctex_data_v(node)
               SYNCTEX_PRINT_CHARINDEX_WHAT);
        _synctex_display_sibling(node);
    }
}

static char * _synctex_abstract_box_bdry(synctex_node_p node) {
    static char abstract[SYNCTEX_ABSTRACT_MAX] = "none";
    if (node) {
        snprintf(abstract,SYNCTEX_ABSTRACT_MAX,"box bdry:%i,%i:%i,%i" SYNCTEX_PRINT_CHARINDEX_FMT,
               _synctex_data_tag(node),
               _synctex_data_line(node),
               _synctex_data_h(node),
               _synctex_data_v(node)
               SYNCTEX_PRINT_CHARINDEX_WHAT);
    }
    return abstract;
}

static void _synctex_display_box_bdry(synctex_node_p node) {
    if (node) {
        printf("%sbox bdry:%i,%i:%i,%i",
               node->class_->scanner->display_prompt,
               _synctex_data_tag(node),
               _synctex_data_line(node),
               _synctex_data_h(node),
               _synctex_data_v(node));
        SYNCTEX_PRINT_CHARINDEX_NL;
        _synctex_display_sibling(node);
    }
}

static char * _synctex_abstract_ref(synctex_node_p node) {
    static char abstract[SYNCTEX_ABSTRACT_MAX] = "none";
    if (node) {
        snprintf(abstract,SYNCTEX_ABSTRACT_MAX,"form ref:%i:%i,%i" SYNCTEX_PRINT_CHARINDEX_FMT,
               _synctex_data_tag(node),
               _synctex_data_h(node),
               _synctex_data_v(node)
                       SYNCTEX_PRINT_CHARINDEX_WHAT);
    }
    return abstract;
}

static void _synctex_display_ref(synctex_node_p node) {
    if (node) {
        printf("%sform ref:%i:%i,%i",
               node->class_->scanner->display_prompt,
               _synctex_data_tag(node),
               _synctex_data_h(node),
               _synctex_data_v(node));
        SYNCTEX_PRINT_CHARINDEX_NL;
        _synctex_display_sibling(node);
    }
}
static char * _synctex_abstract_proxy(synctex_node_p node) {
    static char abstract[SYNCTEX_ABSTRACT_MAX] = "none";
    if (node) {
        synctex_node_p N = _synctex_tree_target(node);
        snprintf(abstract,SYNCTEX_ABSTRACT_MAX,"%s:%i,%i:%i,%i/%p%s",
               synctex_node_isa(node),
               synctex_node_tag(node),
               synctex_node_line(node),
               _synctex_data_h(node),
               _synctex_data_v(node),
               (void*)node, // Fix GCC warning: %p expects a void* according to POSIX
               _synctex_node_abstract(N));
    }
    return abstract;
}
static void _synctex_display_proxy(synctex_node_p node) {
    if (node) {
        synctex_node_p N = _synctex_tree_target(node);
        printf("%s%s:%i,%i:%i,%i",
               node->class_->scanner->display_prompt,
               synctex_node_isa(node),
               synctex_node_tag(node),
               synctex_node_line(node),
               _synctex_data_h(node),
               _synctex_data_v(node));
        if (N) {
            printf("=%i,%i:%i,%i,%i->%s",
                   synctex_node_h(node),
                   synctex_node_v(node),
                   synctex_node_width(node),
                   synctex_node_height(node),
                   synctex_node_depth(node),
                   _synctex_node_abstract(N));
        }
        printf("\n");
        _synctex_display_child(node);
        _synctex_display_sibling(node);
    }
}
static char * _synctex_abstract_proxy_vbox(synctex_node_p node) {
    static char abstract[SYNCTEX_ABSTRACT_MAX] = "none";
    if (node) {
        snprintf(abstract,SYNCTEX_ABSTRACT_MAX,
                 "[*%i,%i:%i,%i:%i,%i,%i...*]"
               SYNCTEX_PRINT_CHARINDEX_FMT,
               synctex_node_tag(node),
               synctex_node_line(node),
               synctex_node_h(node),
               synctex_node_v(node),
               synctex_node_width(node),
               synctex_node_height(node),
               synctex_node_depth(node)
               SYNCTEX_PRINT_CHARINDEX_WHAT);
    }
    return abstract;
}

static void _synctex_display_proxy_vbox(synctex_node_p node) {
    if (node) {
        printf("%s[*%i,%i:%i,%i:%i,%i,%i"
               SYNCTEX_PRINT_CHARINDEX_FMT
               "\n",
               node->class_->scanner->display_prompt,
               synctex_node_tag(node),
               synctex_node_line(node),
               synctex_node_h(node),
               synctex_node_v(node),
               synctex_node_width(node),
               synctex_node_height(node),
               synctex_node_depth(node)
               SYNCTEX_PRINT_CHARINDEX_WHAT);
        _synctex_display_child(node);
        printf("%s*]\n%slast:%s\n",
               node->class_->scanner->display_prompt,
               node->class_->scanner->display_prompt,
               _synctex_node_abstract(_synctex_tree_last(node)));
        _synctex_display_sibling(node);
    }
}

static char * _synctex_abstract_proxy_hbox(synctex_node_p node) {
    static char abstract[SYNCTEX_ABSTRACT_MAX] = "none";
    if (node) {
        snprintf(abstract,SYNCTEX_ABSTRACT_MAX,"(*%i,%i~%i*%i:%i,%i:%i,%i,%i...*)/%p"
               SYNCTEX_PRINT_CHARINDEX_FMT,
               synctex_node_tag(node),
               synctex_node_line(node),
               synctex_node_mean_line(node),
               synctex_node_weight(node),
               synctex_node_h(node),
               synctex_node_v(node),
               synctex_node_width(node),
               synctex_node_height(node),
               synctex_node_depth(node),
               (void*)node // Fix GCC warning: %p expects a void* according to POSIX
               SYNCTEX_PRINT_CHARINDEX_WHAT);
    }
    return abstract;
}

static void _synctex_display_proxy_hbox(synctex_node_p node) {
    if (node) {
        printf("%s(*%i,%i~%i*%i:%i,%i:%i,%i,%i"
               SYNCTEX_PRINT_CHARINDEX_FMT
               "\n",
               node->class_->scanner->display_prompt,
               synctex_node_tag(node),
               synctex_node_line(node),
               synctex_node_mean_line(node),
               synctex_node_weight(node),
               synctex_node_h(node),
               synctex_node_v(node),
               synctex_node_width(node),
               synctex_node_height(node),
               synctex_node_depth(node)
               SYNCTEX_PRINT_CHARINDEX_WHAT);
        _synctex_display_child(node);
        printf("%s*)\n%slast:%s\n",
               node->class_->scanner->display_prompt,
               node->class_->scanner->display_prompt,
               _synctex_node_abstract(_synctex_tree_last(node)));
        _synctex_display_sibling(node);
    }
}

static char * _synctex_abstract_handle(synctex_node_p node) {
    static char abstract[SYNCTEX_ABSTRACT_MAX] = "none";
    if (node) {
        synctex_node_p N = _synctex_tree_target(node);
        if (N && !N->class_) {
            exit(1);
        }
        snprintf(abstract,SYNCTEX_ABSTRACT_MAX,"%s:%s",
               synctex_node_isa(node),
               (N?_synctex_node_abstract(N):""));
    }
    return abstract;
}
static void _synctex_display_handle(synctex_node_p node) {
    if (node) {
        synctex_node_p N = _synctex_tree_target(node);
        printf("%s%s(%i):->%s\n",
               node->class_->scanner->display_prompt,
               synctex_node_isa(node),
               _synctex_data_weight(N),
               _synctex_node_abstract(N));
        _synctex_display_child(node);
        _synctex_display_sibling(node);
    }
}
#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark STATUS
#   endif

#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark Prototypes
#   endif
typedef struct {
    size_t size;
    synctex_status_t status;
} synctex_zs_s;
static synctex_zs_s _synctex_buffer_get_available_size(synctex_scanner_p scanner, size_t size);
static synctex_status_t _synctex_next_line(synctex_scanner_p scanner);
static synctex_status_t _synctex_match_string(synctex_scanner_p scanner, const char * the_string);

typedef struct synctex_ns_t {
    synctex_node_p node;
    synctex_status_t status;
} synctex_ns_s;
static synctex_ns_s __synctex_parse_new_input(synctex_scanner_p scanner);
static synctex_status_t _synctex_scan_preamble(synctex_scanner_p scanner);
typedef struct {
    float value;
    synctex_status_t status;
} synctex_fs_s;
static synctex_fs_s _synctex_scan_float_and_dimension(synctex_scanner_p scanner);
static synctex_status_t _synctex_scan_post_scriptum(synctex_scanner_p scanner);
static synctex_status_t _synctex_scan_postamble(synctex_scanner_p scanner);
static synctex_status_t _synctex_setup_visible_hbox(synctex_node_p box);
static synctex_status_t _synctex_scan_content(synctex_scanner_p scanner);
int synctex_scanner_pre_x_offset(synctex_scanner_p scanner);
int synctex_scanner_pre_y_offset(synctex_scanner_p scanner);
const char * synctex_scanner_get_output_fmt(synctex_scanner_p scanner);

#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark SCANNER UTILITIES
#   endif

#   define SYNCTEX_FILE (scanner->reader->file)

/**
 *  Try to ensure that the buffer contains at least size bytes.
 *  Passing a huge size argument means the whole buffer length.
 *  Passing a 0 size argument means return the available buffer length, without reading the file.
 *  In that case, the return status is always SYNCTEX_STATUS_OK unless the given scanner is NULL.
 *  The size_t value returned is the number of bytes now available in the buffer. This is a nonnegative integer, it may take the value 0.
 *  It is the responsibility of the caller to test whether this size is conforming to its needs.
 *  Negative values may return in case of error, actually
 *  when there was an error reading the synctex file.
 *  - parameter scanner: The owning scanner. When NULL, returns SYNCTEX_STATUS_BAD_ARGUMENT.
 *  - parameter expected: expected number of bytes.
 *  - returns: a size and a status.
 */
static synctex_zs_s _synctex_buffer_get_available_size(synctex_scanner_p scanner, size_t expected) {
    size_t size = 0;
    if (NULL == scanner) {
        return (synctex_zs_s){0,SYNCTEX_STATUS_BAD_ARGUMENT};
    }
    if (expected>scanner->reader->size){
        expected = scanner->reader->size;
    }
    size = SYNCTEX_END - SYNCTEX_CUR; /*  available is the number of unparsed chars in the buffer */
    if (expected<=size) {
        /*  There are already sufficiently many characters in the buffer */
        return (synctex_zs_s){size,SYNCTEX_STATUS_OK};
    }
    if (SYNCTEX_FILE) {
        /*  Copy the remaining part of the buffer to the beginning,
         *  then read the next part of the file */
        int already_read = 0;
#   if defined(SYNCTEX_USE_CHARINDEX)
        scanner->reader->charindex_offset += SYNCTEX_CUR - SYNCTEX_START;
#   endif
        if (size) {
            memmove(SYNCTEX_START, SYNCTEX_CUR, size);
        }
        SYNCTEX_CUR = SYNCTEX_START + size; /*  the next character after the move, will change. */
        /*  Fill the buffer up to its end */
        already_read = gzread(SYNCTEX_FILE,(void *)SYNCTEX_CUR,(int)(SYNCTEX_BUFFER_SIZE - size));
        if (already_read>0) {
            /*  We assume that 0<already_read<=SYNCTEX_BUFFER_SIZE - size, such that
             *  SYNCTEX_CUR + already_read = SYNCTEX_START + size  + already_read <= SYNCTEX_START + SYNCTEX_BUFFER_SIZE */
            SYNCTEX_END = SYNCTEX_CUR + already_read;
            /*  If the end of the file was reached, all the required SYNCTEX_BUFFER_SIZE - available
             *  may not be filled with values from the file.
             *  In that case, the buffer should stop properly after already_read characters. */
            * SYNCTEX_END = '\0'; /* there is enough room */
            SYNCTEX_CUR = SYNCTEX_START;
            /*  May be available is less than size, the caller will have to test. */
            return (synctex_zs_s){SYNCTEX_END - SYNCTEX_CUR,SYNCTEX_STATUS_OK};
        } else if (0>already_read) {
            /*  There is a possible error in reading the file */
            int errnum = 0;
            const char * error_string = gzerror(SYNCTEX_FILE, &errnum);
            if (Z_ERRNO == errnum) {
                /*  There is an error in zlib caused by the file system */
                _synctex_error("gzread error from the file system (%i)",errno);
                return (synctex_zs_s){0,SYNCTEX_STATUS_ERROR};
            } else if (errnum) {
                _synctex_error("gzread error (%i:%i,%s)",already_read,errnum,error_string);
                return (synctex_zs_s){0,SYNCTEX_STATUS_ERROR};
            }
        }
        /*  Nothing was read, we are at the end of the file. */
        gzclose(SYNCTEX_FILE);
        SYNCTEX_FILE = NULL;
        SYNCTEX_END = SYNCTEX_CUR;
        SYNCTEX_CUR = SYNCTEX_START;
        * SYNCTEX_END = '\0';/*  Terminate the string properly.*/
        /*  there might be a bit of text left */
        return (synctex_zs_s){SYNCTEX_END - SYNCTEX_CUR,SYNCTEX_STATUS_EOF};
    }
    /*  We cannot enlarge the buffer because the end of the file was reached. */
    return (synctex_zs_s){size,SYNCTEX_STATUS_EOF};
}

/*  Used when parsing the synctex file.
 *  Advance to the next character starting a line.
 *  Actually, only '\n' is recognized as end of line marker.
 *  On normal completion, the returned value is the number of unparsed characters available in the buffer.
 *  In general, it is a positive value, 0 meaning that the end of file was reached.
 *  -1 is returned in case of error, actually because there was an error while feeding the buffer.
 *  When the function returns with no error, SYNCTEX_CUR points to the first character of the next line, if any.
 *  J. Laurens: Sat May 10 07:52:31 UTC 2008
 */
static synctex_status_t _synctex_next_line(synctex_scanner_p scanner) {
    synctex_status_t status = SYNCTEX_STATUS_OK;
    if (NULL == scanner) {
        return SYNCTEX_STATUS_BAD_ARGUMENT;
    }
infinite_loop:
    while(SYNCTEX_CUR<SYNCTEX_END) {
        if (*SYNCTEX_CUR == '\n') {
            ++SYNCTEX_CUR;
            ++scanner->reader->line_number;
            return _synctex_buffer_get_available_size(scanner, 1).status;
        }
        ++SYNCTEX_CUR;
    }
    /*  Here, we have SYNCTEX_CUR == SYNCTEX_END, such that the next call to _synctex_buffer_get_available_size
     *  will read another bunch of synctex file. Little by little, we advance to the end of the file. */
    status = _synctex_buffer_get_available_size(scanner, 1).status;
    if (status<=SYNCTEX_STATUS_EOF) {
        return status;
    }
    goto infinite_loop;
}

/*  Scan the given string.
 *  Both scanner and the_string must not be NULL, and the_string must not be 0 length.
 *  SYNCTEX_STATUS_OK is returned if the string is found,
 *  SYNCTEX_STATUS_EOF is returned when the EOF is reached,
 *  SYNCTEX_STATUS_NOT_OK is returned is the string is not found,
 *  an error status is returned otherwise.
 *  This is a critical method because buffering renders things more difficult.
 *  The given string might be as long as the maximum size_t value.
 *  As side effect, the buffer state may have changed if the given argument string can't fit into the buffer.
 */
static synctex_status_t _synctex_match_string(synctex_scanner_p scanner, const char * the_string) {
    size_t tested_len = 0; /*  the number of characters at the beginning of the_string that match */
    size_t remaining_len = 0; /*  the number of remaining characters of the_string that should match */
    size_t available = 0;
    synctex_zs_s zs = {0,0};
    if (NULL == scanner || NULL == the_string) {
        return SYNCTEX_STATUS_BAD_ARGUMENT;
    }
    remaining_len = strlen(the_string); /*  All the_string should match */
    if (0 == remaining_len) {
        return SYNCTEX_STATUS_BAD_ARGUMENT;
    }
    /*  How many characters available in the buffer? */
    zs = _synctex_buffer_get_available_size(scanner,remaining_len);
    if (zs.status<SYNCTEX_STATUS_EOF) {
        return zs.status;
    }
    /*  Maybe we have less characters than expected because the buffer is too small. */
    if (zs.size>=remaining_len) {
        /*  The buffer is sufficiently big to hold the expected number of characters. */
        if (strncmp((char *)SYNCTEX_CUR,the_string,remaining_len)) {
            return SYNCTEX_STATUS_NOT_OK;
        }
    return_OK:
        /*  Advance SYNCTEX_CUR to the next character after the_string. */
        SYNCTEX_CUR += remaining_len;
        return SYNCTEX_STATUS_OK;
    } else if (strncmp((char *)SYNCTEX_CUR,the_string,zs.size)) {
        /*  No need to go further, this is not the expected string in the buffer. */
        return SYNCTEX_STATUS_NOT_OK;
    } else if (SYNCTEX_FILE) {
        /*  The buffer was too small to contain remaining_len characters.
         *  We have to cut the string into pieces. */
        z_off_t offset = 0L;
        /*  the first part of the string is found, advance the_string to the next untested character. */
        the_string += zs.size;
        /*  update the remaining length and the parsed length. */
        remaining_len -= zs.size;
        tested_len += zs.size;
        SYNCTEX_CUR += zs.size; /*  We validate the tested characters. */
        if (0 == remaining_len) {
            /*  Nothing left to test, we have found the given string. */
            return SYNCTEX_STATUS_OK;
        }
        /*  We also have to record the current state of the file cursor because
         *  if the_string does not match, all this should be a totally blank operation,
         *  for which the file and buffer states should not be modified at all.
         *  In fact, the states of the buffer before and after this function are in general different
         *  but they are totally equivalent as long as the values of the buffer before SYNCTEX_CUR
         *  can be safely discarded.  */
        offset = gztell(SYNCTEX_FILE);
        /*  offset now corresponds to the first character of the file that was not buffered. */
        /*  SYNCTEX_CUR - SYNCTEX_START is the number of chars that where already buffered and
         *  that match the head of the_string. If in fine the_string does not match, all these chars must be recovered
         *  because the whole buffer contents is replaced in _synctex_buffer_get_available_size.
         *  They were buffered from offset-len location in the file. */
        offset -= SYNCTEX_CUR - SYNCTEX_START;
    more_characters:
        /*  There is still some work to be done, so read another bunch of file.
         *  This is the second call to _synctex_buffer_get_available_size,
         *  which means that the actual contents of the buffer will be discarded.
         *  We will definitely have to recover the previous state in case we do not find the expected string. */
        zs = _synctex_buffer_get_available_size(scanner,remaining_len);
        if (zs.status<SYNCTEX_STATUS_EOF) {
            return zs.status; /*  This is an error, no need to go further. */
        }
        if (zs.size==0) {
            /*  Missing characters: recover the initial state of the file and return. */
        return_NOT_OK:
            if (offset != gzseek(SYNCTEX_FILE,offset,SEEK_SET)) {
                /*  This is a critical error, we could not recover the previous state. */
                _synctex_error("Can't seek file");
                return SYNCTEX_STATUS_ERROR;
            }
            /*  Next time we are asked to fill the buffer,
             *  we will read a complete bunch of text from the file. */
            SYNCTEX_CUR = SYNCTEX_END;
            return SYNCTEX_STATUS_NOT_OK;
        }
        if (zs.size<remaining_len) {
            /*  We'll have to loop one more time. */
            if (strncmp((char *)SYNCTEX_CUR,the_string,zs.size)) {
                /*  This is not the expected string, recover the previous state and return. */
                goto return_NOT_OK;
            }
            /*  Advance the_string to the first untested character. */
            the_string += available;
            /*  update the remaining length and the parsed length. */
            remaining_len -= zs.size;
            tested_len += zs.size;
            SYNCTEX_CUR += zs.size; /*  We validate the tested characters. */
            goto more_characters;
        }
        /*  This is the last step. */
        if (strncmp((char *)SYNCTEX_CUR,the_string,remaining_len)) {
            /*  This is not the expected string, recover the previous state and return. */
            goto return_NOT_OK;
        }
        goto return_OK;
    } else {
        /*  The buffer can't contain the given string argument, and the EOF was reached */
        return SYNCTEX_STATUS_EOF;
    }
}

/*  Used when parsing the synctex file.
 *  Decode an integer.
 *  First, field separators, namely ':' and ',' characters are skipped
 *  The returned value is negative if there is an unrecoverable error.
 *  It is SYNCTEX_STATUS_NOT_OK if an integer could not be parsed, for example
 *  if the characters at the current cursor position are not digits or
 *  if the end of the file has been reached.
 *  It is SYNCTEX_STATUS_OK if an int has been successfully parsed.
 *  The given scanner argument must not be NULL, on the contrary, value_ref may be NULL.
 */
static synctex_is_s _synctex_decode_int(synctex_scanner_p scanner) {
    char * ptr = NULL;
    char * end = NULL;
    synctex_zs_s zs = {0,0};
    int result;
    if (NULL == scanner) {
        return (synctex_is_s){0, SYNCTEX_STATUS_BAD_ARGUMENT};
    }
    zs = _synctex_buffer_get_available_size(scanner, SYNCTEX_BUFFER_MIN_SIZE);
    if (zs.status<SYNCTEX_STATUS_EOF) {
        return (synctex_is_s){0,zs.status};
    }
    if (zs.size==0) {
        return (synctex_is_s){0,SYNCTEX_STATUS_NOT_OK};
    }
    ptr = SYNCTEX_CUR;
    /*  Optionally parse the separator */
    if (*ptr==':' || *ptr==',') {
        ++ptr;
        --zs.size;
        if (zs.size==0) {
            return (synctex_is_s){0,SYNCTEX_STATUS_NOT_OK};
        }
    }
    result = (int)strtol(ptr, &end, 10);
    if (end>ptr) {
        SYNCTEX_CUR = end;
        return (synctex_is_s){result,SYNCTEX_STATUS_OK};
    }
    return (synctex_is_s){result,SYNCTEX_STATUS_NOT_OK};
}
static synctex_is_s _synctex_decode_int_opt(synctex_scanner_p scanner, int default_value) {
    char * ptr = NULL;
    char * end = NULL;
    synctex_zs_s zs = {0, 0};
    if (NULL == scanner) {
        return (synctex_is_s){default_value, SYNCTEX_STATUS_BAD_ARGUMENT};
    }
    zs = _synctex_buffer_get_available_size(scanner, SYNCTEX_BUFFER_MIN_SIZE);
    if (zs.status<SYNCTEX_STATUS_EOF) {
        return (synctex_is_s){default_value,zs.status};
    }
    if (zs.size==0) {
        return (synctex_is_s){default_value,SYNCTEX_STATUS_OK};
    }
    ptr = SYNCTEX_CUR;
    /*  Comma separator required */
    if (*ptr==',') {
        int result;
        ++ptr;
        --zs.size;
        if (zs.size==0) {
            return (synctex_is_s){default_value,SYNCTEX_STATUS_NOT_OK};
        }
        result = (int)strtol(ptr, &end, 10);
        if (end>ptr) {
            SYNCTEX_CUR = end;
            return (synctex_is_s){result,SYNCTEX_STATUS_OK};
        }
        return (synctex_is_s){default_value,SYNCTEX_STATUS_NOT_OK};
    }
    return (synctex_is_s){default_value,SYNCTEX_STATUS_OK};
}
/*  Used when parsing the synctex file.
 *  Decode an integer for a v field.
 *  Try the _synctex_decode_int version and set the last v field scanned.
 *  If it does not succeed, tries to match an '=' sign,
 *  which is a shortcut for the last v field scanned.
 */
#   define SYNCTEX_INPUT_COMEQUALS ",="
static synctex_is_s _synctex_decode_int_v(synctex_scanner_p scanner) {
    synctex_is_s is = _synctex_decode_int(scanner);
    if (SYNCTEX_STATUS_OK == is.status) {
        scanner->reader->lastv = is.integer;
        return is;
    }
    is.status = _synctex_match_string(scanner,SYNCTEX_INPUT_COMEQUALS);
    if (is.status<SYNCTEX_STATUS_OK) {
        return is;
    }
    is.integer = scanner->reader->lastv;
    return is;
}

/*  The purpose of this function is to read a string.
 *  A string is an array of characters from the current parser location
 *  and before the next '\n' character.
 *  If a string was properly decoded, it is returned in value_ref and
 *  the cursor points to the new line marker.
 *  The returned string was alloced on the heap, the caller is the owner and
 *  is responsible to free it in due time,
 *  unless it transfers the ownership to another object.
 *  If no string is parsed, * value_ref is undefined.
 *  The maximum length of a string that a scanner can decode is platform dependent, namely UINT_MAX.
 *  If you just want to blindly parse the file up to the end of the current line,
 *  use _synctex_next_line instead.
 *  On return, the scanner cursor is unchanged if a string could not be scanned or
 *  points to the terminating '\n' character otherwise. As a consequence,
 *  _synctex_next_line is necessary after.
 *  If either scanner or value_ref is NULL, it is considered as an error and
 *  SYNCTEX_STATUS_BAD_ARGUMENT is returned.
 */
static synctex_ss_s _synctex_decode_string(synctex_scanner_p scanner) {
    char * end = NULL;
    size_t len = 0;/*  The number of bytes to copy */
    size_t already_len = 0;
    synctex_zs_s zs = {0,0};
    char * string = NULL;
    if (NULL == scanner) {
        return (synctex_ss_s){NULL,SYNCTEX_STATUS_BAD_ARGUMENT};
    }
    /*  The buffer must at least contain one character: the '\n' end of line marker */
    if (SYNCTEX_CUR>=SYNCTEX_END) {
more_characters:
        zs = _synctex_buffer_get_available_size(scanner,1);
        if (zs.status < SYNCTEX_STATUS_EOF) {
            return (synctex_ss_s){NULL,zs.status};
        } else if (0 == zs.size) {
            return (synctex_ss_s){NULL,SYNCTEX_STATUS_EOF};
        }
    }
    /*  Now we are sure that there is at least one available character, either because
     *  SYNCTEX_CUR was already < SYNCTEX_END, or because the buffer has been properly filled. */
    /*  end will point to the next unparsed '\n' character in the file, when mapped to the buffer. */
    end = SYNCTEX_CUR;
    /*  We scan all the characters up to the next '\n' */
    while (end<SYNCTEX_END && *end != '\n') {
        ++end;
    }
    /*  OK, we found where to stop:
     *      either end == SYNCTEX_END
     *      or *end == '\n' */
    len = end - SYNCTEX_CUR;
    if (len<UINT_MAX-already_len) {
        if ((string = realloc(string,len+already_len+1)) != NULL) {
            if (memcpy(string+already_len,SYNCTEX_CUR,len)) {
                already_len += len;
                string[already_len]='\0'; /*  Terminate the string */
                SYNCTEX_CUR += len;/*  Eventually advance to the terminating '\n' */
                if (SYNCTEX_CUR==SYNCTEX_END) {
                    /* No \n found*/
                    goto more_characters;
                }
                /* trim the trailing whites */
                len = already_len;
                while (len>0) {
                    already_len = len--;
                    if (string[len]!=' ') {
                        break;
                    }
                }
                string[already_len] = '\0';
                return (synctex_ss_s){string,SYNCTEX_STATUS_OK};
            }
            free(string);
            _synctex_error("could not copy memory (1).");
            return (synctex_ss_s){NULL,SYNCTEX_STATUS_ERROR};
        }
    }
    _synctex_error("could not (re)allocate memory (1).");
    return (synctex_ss_s){NULL,SYNCTEX_STATUS_ERROR};
}

/*  Used when parsing the synctex file.
 *  Read an Input record.
 *  - parameter scanner: non NULL scanner
 *  - returns SYNCTEX_STATUS_OK on successful completions, others values otherwise.
 */
static synctex_ns_s __synctex_parse_new_input(synctex_scanner_p scanner) {
    synctex_node_p input = NULL;
    synctex_status_t status = SYNCTEX_STATUS_BAD_ARGUMENT;
    synctex_zs_s zs = {0,0};
    if (NULL == scanner) {
        return (synctex_ns_s){NULL,status};
    }
    if ((status=_synctex_match_string(scanner,SYNCTEX_INPUT_MARK))<SYNCTEX_STATUS_OK) {
        return (synctex_ns_s){NULL,status};
    }
    /*  Create a node */
    if (NULL == (input = _synctex_new_input(scanner))) {
        _synctex_error("Could not create an input node.");
        return (synctex_ns_s){NULL,SYNCTEX_STATUS_ERROR};
    }
    /*  Decode the tag  */
    if ((status=_synctex_data_decode_tag(input))<SYNCTEX_STATUS_OK) {
        _synctex_error("Bad format of input node.");
        synctex_node_free(input);
        return (synctex_ns_s){NULL,status};
    }
    /*  The next character is a field separator, we expect one character in the buffer. */
    zs = _synctex_buffer_get_available_size(scanner, 1);
    if (zs.status<=SYNCTEX_STATUS_ERROR) {
        return (synctex_ns_s){NULL,status};
    }
    if (0 == zs.size) {
        return (synctex_ns_s){NULL,SYNCTEX_STATUS_EOF};
    }
    /*  We can now safely advance to the next character, stepping over the field separator. */
    ++SYNCTEX_CUR;
    --zs.size;
    /*  Then we scan the file name */
    if ((status=_synctex_data_decode_name(input))<SYNCTEX_STATUS_OK) {
        synctex_node_free(input);
        _synctex_next_line(scanner);/* Ignore this whole line */
        return (synctex_ns_s){NULL,status};
    }
    /*  Prepend this input node to the input linked list of the scanner */
    __synctex_tree_set_sibling(input,scanner->input);/* input has no parent */
    scanner->input = input;
#   if SYNCTEX_VERBOSE
    synctex_node_log(input);
#   endif
    return (synctex_ns_s){input,_synctex_next_line(scanner)};/*  read the line termination character, if any */
}

typedef synctex_is_s (*synctex_decoder_t)(synctex_scanner_p);

/*  Used when parsing the synctex file.
 *  Read one of the settings.
 *  On normal completion, returns SYNCTEX_STATUS_OK.
 *  On error, returns SYNCTEX_STATUS_ERROR.
 *  Both arguments must not be NULL.
 *  On return, the scanner points to the next character after the decoded object whatever it is.
 *  It is the responsibility of the caller to prepare the scanner for the next line.
 */
static synctex_status_t _synctex_scan_named(synctex_scanner_p scanner,const char * name) {
    synctex_status_t status = 0;
    if (NULL == scanner || NULL == name) {
        return SYNCTEX_STATUS_BAD_ARGUMENT;
    }
not_found:
    status = _synctex_match_string(scanner,name);
    if (status<SYNCTEX_STATUS_NOT_OK) {
        return status;
    } else if (status == SYNCTEX_STATUS_NOT_OK) {
        status = _synctex_next_line(scanner);
        if (status<SYNCTEX_STATUS_OK) {
            return status;
        }
        goto not_found;
    }
    return SYNCTEX_STATUS_OK;
}

/*  Used when parsing the synctex file.
 *  Read the preamble.
 */
static synctex_status_t _synctex_scan_preamble(synctex_scanner_p scanner) {
    synctex_status_t status = 0;
    synctex_is_s is = {0,0};
    synctex_ss_s ss = {NULL,0};
    if (NULL == scanner) {
        return SYNCTEX_STATUS_BAD_ARGUMENT;
    }
    status = _synctex_scan_named(scanner,"SyncTeX Version:");
    if (status<SYNCTEX_STATUS_OK) {
        return status;
    }
    is = _synctex_decode_int(scanner);
    if (is.status<SYNCTEX_STATUS_OK) {
        return is.status;
    }
    status = _synctex_next_line(scanner);
    if (status<SYNCTEX_STATUS_OK) {
        return status;
    }
    scanner->version = is.integer;
    /*  Read all the input records */
    do {
        status = __synctex_parse_new_input(scanner).status;
        if (status<SYNCTEX_STATUS_NOT_OK) {
            return status;
        }
    } while(status == SYNCTEX_STATUS_OK);
    /*  the loop exits when status == SYNCTEX_STATUS_NOT_OK */
    /*  Now read all the required settings. */
    if ((status=_synctex_scan_named(scanner,"Output:"))<SYNCTEX_STATUS_OK) {
        return status;
    }
    if ((ss=_synctex_decode_string(scanner)).status<SYNCTEX_STATUS_OK) {
        return is.status;
    }
    if ((status=_synctex_next_line(scanner))<SYNCTEX_STATUS_OK) {
        return status;
    }
    scanner->output_fmt = ss.string;
    if ((status=_synctex_scan_named(scanner,"Magnification:"))<SYNCTEX_STATUS_OK) {
        return status;
    }
    if ((is=_synctex_decode_int(scanner)).status<SYNCTEX_STATUS_OK) {
        return is.status;
    }
    if ((status=_synctex_next_line(scanner))<SYNCTEX_STATUS_OK) {
        return status;
    }
    scanner->pre_magnification = is.integer;
    if ((status=_synctex_scan_named(scanner,"Unit:"))<SYNCTEX_STATUS_OK) {
        return status;
    }
    if ((is=_synctex_decode_int(scanner)).status<SYNCTEX_STATUS_OK) {
        return is.status;
    }
    if ((status=_synctex_next_line(scanner))<SYNCTEX_STATUS_OK) {
        return status;
    }
    scanner->pre_unit = is.integer;
    if ((status=_synctex_scan_named(scanner,"X Offset:"))<SYNCTEX_STATUS_OK) {
        return status;
    }
    if ((is=_synctex_decode_int(scanner)).status<SYNCTEX_STATUS_OK) {
        return is.status;
    }
    if ((status=_synctex_next_line(scanner))<SYNCTEX_STATUS_OK) {
        return status;
    }
    scanner->pre_x_offset = is.integer;
    if ((status=_synctex_scan_named(scanner,"Y Offset:"))<SYNCTEX_STATUS_OK) {
        return status;
    }
    if ((is=_synctex_decode_int(scanner)).status<SYNCTEX_STATUS_OK) {
        return is.status;
    }
    if ((status=_synctex_next_line(scanner))<SYNCTEX_STATUS_OK) {
        return status;
    }
    scanner->pre_y_offset = is.integer;
    return SYNCTEX_STATUS_OK;
}

/*  parse a float with a dimension */
static synctex_fs_s _synctex_scan_float_and_dimension(synctex_scanner_p scanner) {
    synctex_fs_s fs = {0,0};
    synctex_zs_s zs = {0,0};
    char * endptr = NULL;
#ifdef HAVE_SETLOCALE
    char * loc = setlocale(LC_NUMERIC, NULL);
#endif
    if (NULL == scanner) {
        return (synctex_fs_s){0,SYNCTEX_STATUS_BAD_ARGUMENT};
    }
    zs = _synctex_buffer_get_available_size(scanner, SYNCTEX_BUFFER_MIN_SIZE);
    if (zs.status<SYNCTEX_STATUS_EOF) {
        _synctex_error("Problem with float.");
        return (synctex_fs_s){0,zs.status};
    }
#ifdef HAVE_SETLOCALE
    setlocale(LC_NUMERIC, "C");
#endif
    fs.value = strtod(SYNCTEX_CUR,&endptr);
#ifdef HAVE_SETLOCALE
    setlocale(LC_NUMERIC, loc);
#endif
    if (endptr == SYNCTEX_CUR) {
        _synctex_error("A float was expected.");
        return (synctex_fs_s){0,SYNCTEX_STATUS_ERROR};
    }
    SYNCTEX_CUR = endptr;
    if ((fs.status = _synctex_match_string(scanner,"in")) >= SYNCTEX_STATUS_OK) {
        fs.value *= 72.27f*65536;
    } else if (fs.status<SYNCTEX_STATUS_EOF) {
    report_unit_error:
        _synctex_error("problem with unit.");
        return fs;
    } else if ((fs.status = _synctex_match_string(scanner,"cm")) >= SYNCTEX_STATUS_OK) {
        fs.value *= 72.27f*65536/2.54f;
    } else if (fs.status<SYNCTEX_STATUS_EOF) {
        goto report_unit_error;
    } else if ((fs.status = _synctex_match_string(scanner,"mm")) >= SYNCTEX_STATUS_OK) {
        fs.value *= 72.27f*65536/25.4f;
    } else if (fs.status<SYNCTEX_STATUS_EOF) {
        goto report_unit_error;
    } else if ((fs.status = _synctex_match_string(scanner,"pt")) >= SYNCTEX_STATUS_OK) {
        fs.value *= 65536.0f;
    } else if (fs.status<SYNCTEX_STATUS_EOF) {
        goto report_unit_error;
    } else if ((fs.status = _synctex_match_string(scanner,"bp")) >= SYNCTEX_STATUS_OK) {
        fs.value *= 72.27f/72*65536.0f;
    }  else if (fs.status<SYNCTEX_STATUS_EOF) {
        goto report_unit_error;
    } else if ((fs.status = _synctex_match_string(scanner,"pc")) >= SYNCTEX_STATUS_OK) {
        fs.value *= 12.0*65536.0f;
    }  else if (fs.status<SYNCTEX_STATUS_EOF) {
        goto report_unit_error;
    } else if ((fs.status = _synctex_match_string(scanner,"sp")) >= SYNCTEX_STATUS_OK) {
        fs.value *= 1.0f;
    }  else if (fs.status<SYNCTEX_STATUS_EOF) {
        goto report_unit_error;
    } else if ((fs.status = _synctex_match_string(scanner,"dd")) >= SYNCTEX_STATUS_OK) {
        fs.value *= 1238.0f/1157*65536.0f;
    }  else if (fs.status<SYNCTEX_STATUS_EOF) {
        goto report_unit_error;
    } else if ((fs.status = _synctex_match_string(scanner,"cc")) >= SYNCTEX_STATUS_OK) {
        fs.value *= 14856.0f/1157*65536;
    } else if (fs.status<SYNCTEX_STATUS_EOF) {
        goto report_unit_error;
    } else if ((fs.status = _synctex_match_string(scanner,"nd")) >= SYNCTEX_STATUS_OK) {
        fs.value *= 685.0f/642*65536;
    }  else if (fs.status<SYNCTEX_STATUS_EOF) {
        goto report_unit_error;
    } else if ((fs.status = _synctex_match_string(scanner,"nc")) >= SYNCTEX_STATUS_OK) {
        fs.value *= 1370.0f/107*65536;
    } else if (fs.status<SYNCTEX_STATUS_EOF) {
        goto report_unit_error;
    }
    return fs;
}

/*  parse the post scriptum
 *  SYNCTEX_STATUS_OK is returned on completion
 *  a negative error is returned otherwise */
static synctex_status_t _synctex_scan_post_scriptum(synctex_scanner_p scanner) {
    synctex_status_t status = 0;
    synctex_fs_s fs = {0,0};
    char * endptr = NULL;
#ifdef HAVE_SETLOCALE
    char * loc = setlocale(LC_NUMERIC, NULL);
#endif
    if (NULL == scanner) {
        return SYNCTEX_STATUS_BAD_ARGUMENT;
    }
    /*  Scan the file until a post scriptum line is found */
post_scriptum_not_found:
    status = _synctex_match_string(scanner,"Post scriptum:");
    if (status<SYNCTEX_STATUS_NOT_OK) {
        return status;
    }
    if (status == SYNCTEX_STATUS_NOT_OK) {
        status = _synctex_next_line(scanner);
        if (status<SYNCTEX_STATUS_EOF) {
            return status;
        } else if (status<SYNCTEX_STATUS_OK) {
            return SYNCTEX_STATUS_OK;/*  The EOF is found, we have properly scanned the file */
        }
        goto post_scriptum_not_found;
    }
    /*  We found the name, advance to the next line. */
next_line:
    status = _synctex_next_line(scanner);
    if (status<SYNCTEX_STATUS_EOF) {
        return status;
    } else if (status<SYNCTEX_STATUS_OK) {
        return SYNCTEX_STATUS_OK;/*  The EOF is found, we have properly scanned the file */
    }
    /*  Scanning the information */
    status = _synctex_match_string(scanner,"Magnification:");
    if (status == SYNCTEX_STATUS_OK ) {
#ifdef HAVE_SETLOCALE
        setlocale(LC_NUMERIC, "C");
#endif
        scanner->unit = strtod(SYNCTEX_CUR,&endptr);
#ifdef HAVE_SETLOCALE
        setlocale(LC_NUMERIC, loc);
#endif
        if (endptr == SYNCTEX_CUR) {
            _synctex_error("bad magnification in the post scriptum, a float was expected.");
            return SYNCTEX_STATUS_ERROR;
        }
        if (scanner->unit<=0) {
            _synctex_error("bad magnification in the post scriptum, a positive float was expected.");
            return SYNCTEX_STATUS_ERROR;
        }
        SYNCTEX_CUR = endptr;
        goto next_line;
    }
    if (status<SYNCTEX_STATUS_EOF){
    report_record_problem:
        _synctex_error("Problem reading the Post Scriptum records");
        return status; /*  echo the error. */
    }
    status = _synctex_match_string(scanner,"X Offset:");
    if (status == SYNCTEX_STATUS_OK) {
        fs = _synctex_scan_float_and_dimension(scanner);
        if (fs.status<SYNCTEX_STATUS_OK) {
            _synctex_error("Problem with X offset in the Post Scriptum.");
            return fs.status;
        }
        scanner->x_offset = fs.value;
        goto next_line;
    } else if (status<SYNCTEX_STATUS_EOF){
        goto report_record_problem;
    }
    status = _synctex_match_string(scanner,"Y Offset:");
    if (status==SYNCTEX_STATUS_OK) {
        fs = _synctex_scan_float_and_dimension(scanner);
        if (fs.status<SYNCTEX_STATUS_OK) {
            _synctex_error("Problem with Y offset in the Post Scriptum.");
            return fs.status;
        }
        scanner->y_offset = fs.value;
        goto next_line;
    } else if (status<SYNCTEX_STATUS_EOF){
        goto report_record_problem;
    }
    goto next_line;
}

/*  SYNCTEX_STATUS_OK is returned if the postamble is read
 *  SYNCTEX_STATUS_NOT_OK is returned if the postamble is not at the current location
 *  a negative error otherwise
 *  The postamble comprises the post scriptum section.
 */
static synctex_status_t _synctex_scan_postamble(synctex_scanner_p scanner) {
    synctex_status_t status = 0;
    synctex_is_s is = {0,0};
    if (NULL == scanner) {
        return SYNCTEX_STATUS_BAD_ARGUMENT;
    }
    if (!scanner->flags.postamble && (status=_synctex_match_string(scanner,"Postamble:"))<SYNCTEX_STATUS_OK) {
        return status;
    }
count_again:
    if ((status=_synctex_next_line(scanner))<SYNCTEX_STATUS_OK) {
        return status;
    }
    if ((status=_synctex_scan_named(scanner,"Count:"))< SYNCTEX_STATUS_EOF) {
        return status; /*  forward the error */
    } else if (status < SYNCTEX_STATUS_OK) { /*  No Count record found */
        goto count_again;
    }
    if ((is=_synctex_decode_int(scanner)).status<SYNCTEX_STATUS_OK) {
        return is.status;
    }
    if ((status=_synctex_next_line(scanner))<SYNCTEX_STATUS_OK) {
        return status;
    }
    scanner->count = is.integer;
    /*  Now we scan the last part of the SyncTeX file: the Post Scriptum section. */
    return _synctex_scan_post_scriptum(scanner);
}

/*  Horizontal boxes also have visible size.
 *  Visible size are bigger than real size.
 *  For example 0 width boxes may contain text.
 *  At creation time, the visible size is set to the values of the real size.
 */
static synctex_status_t _synctex_setup_visible_hbox(synctex_node_p box) {
    if (box) {
        switch(synctex_node_type(box)) {
            case synctex_node_type_hbox:
                _synctex_data_set_h_V(box,_synctex_data_h(box));
                _synctex_data_set_v_V(box,_synctex_data_v(box));
                _synctex_data_set_width_V(box,_synctex_data_width(box));
                _synctex_data_set_height_V(box,_synctex_data_height(box));
                _synctex_data_set_depth_V(box,_synctex_data_depth(box));
                return SYNCTEX_STATUS_OK;
            default:
                break;
        }
    }
    return SYNCTEX_STATUS_BAD_ARGUMENT;
}

/*  This method is sent to an horizontal box to setup the visible size
 *  Some box have 0 width but do contain text material.
 *  With this method, one can enlarge the box to contain the given point (h,v).
 */
static synctex_status_t _synctex_make_hbox_contain_point(synctex_node_p node,synctex_point_s point) {
    int min, max, n;
    if (NULL == node || synctex_node_type(node) != synctex_node_type_hbox) {
        return SYNCTEX_STATUS_BAD_ARGUMENT;
    }
    if ((n = _synctex_data_width_V(node))<0) {
        max = _synctex_data_h_V(node);
        min = max+n;
        if (point.h<min) {
            _synctex_data_set_width_V(node,point.h-max);
        } else if (point.h>max) {
            _synctex_data_set_h_V(node,point.h);
            _synctex_data_set_width_V(node,min-point.h);
        }
    } else {
        min = _synctex_data_h_V(node);
        max = min+n;
        if (point.h<min) {
            _synctex_data_set_h_V(node,point.h);
            _synctex_data_set_width_V(node,max - point.h);
        } else if (point.h>max) {
            _synctex_data_set_width_V(node,point.h - min);
        }
    }
    n = _synctex_data_v_V(node);
    min = n - _synctex_data_height_V(node);
    max = n + _synctex_data_depth_V(node);
    if (point.v<min) {
        _synctex_data_set_height_V(node,n-point.v);
    } else if (point.v>max) {
        _synctex_data_set_depth_V(node,point.v-n);
    }
    return SYNCTEX_STATUS_OK;
}
static synctex_status_t _synctex_make_hbox_contain_box(synctex_node_p node,synctex_box_s box) {
    int min, max, n;
    if (NULL == node || synctex_node_type(node) != synctex_node_type_hbox) {
        return SYNCTEX_STATUS_BAD_ARGUMENT;
    }
    if ((n = _synctex_data_width_V(node))<0) {
        max = _synctex_data_h_V(node);
        min = max+n;
        if (box.min.h <min) {
            _synctex_data_set_width_V(node,box.min.h-max);
        } else if (box.max.h>max) {
            _synctex_data_set_h_V(node,box.max.h);
            _synctex_data_set_width_V(node,min-box.max.h);
        }
    } else {
        min = _synctex_data_h_V(node);
        max = min+n;
        if (box.min.h<min) {
            _synctex_data_set_h_V(node,box.min.h);
            _synctex_data_set_width_V(node,max - box.min.h);
        } else if (box.max.h>max) {
            _synctex_data_set_width_V(node,box.max.h - min);
        }
    }
    n = _synctex_data_v_V(node);
    min = n - _synctex_data_height_V(node);
    max = n + _synctex_data_depth_V(node);
    if (box.min.v<min) {
        _synctex_data_set_height_V(node,n-box.min.v);
    } else if (box.max.v>max) {
        _synctex_data_set_depth_V(node,box.max.v-n);
    }
    return SYNCTEX_STATUS_OK;
}
#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark SPECIAL CHARACTERS
#   endif


/*  Here are the control characters that strat each line of the synctex output file.
 *  Their values define the meaning of the line.
 */
#   define SYNCTEX_CHAR_BEGIN_SHEET '{'
#   define SYNCTEX_CHAR_END_SHEET   '}'
#   define SYNCTEX_CHAR_BEGIN_FORM  '<'
#   define SYNCTEX_CHAR_END_FORM    '>'
#   define SYNCTEX_CHAR_BEGIN_VBOX  '['
#   define SYNCTEX_CHAR_END_VBOX    ']'
#   define SYNCTEX_CHAR_BEGIN_HBOX  '('
#   define SYNCTEX_CHAR_END_HBOX    ')'
#   define SYNCTEX_CHAR_ANCHOR      '!'
#   define SYNCTEX_CHAR_VOID_VBOX   'v'
#   define SYNCTEX_CHAR_VOID_HBOX   'h'
#   define SYNCTEX_CHAR_KERN        'k'
#   define SYNCTEX_CHAR_GLUE        'g'
#   define SYNCTEX_CHAR_RULE        'r'
#   define SYNCTEX_CHAR_MATH        '$'
#   define SYNCTEX_CHAR_FORM_REF    'f'
#   define SYNCTEX_CHAR_BOUNDARY    'x'
#   define SYNCTEX_CHAR_CHARACTER   'c'
#   define SYNCTEX_CHAR_COMMENT     '%'

#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark SCANNERS & PARSERS
#   endif

#   define SYNCTEX_DECODE_FAILED(NODE,WHAT) \
(_synctex_data_decode_##WHAT(NODE)<SYNCTEX_STATUS_OK)
#   define SYNCTEX_DECODE_FAILED_V(NODE,WHAT) \
(_synctex_data_decode_##WHAT##_v(NODE)<SYNCTEX_STATUS_OK)

#define SYNCTEX_NS_NULL (synctex_ns_s){NULL,SYNCTEX_STATUS_NOT_OK}
static synctex_ns_s _synctex_parse_new_sheet(synctex_scanner_p scanner) {
    synctex_node_p node;
    if ((node = _synctex_new_sheet(scanner))) {
        if (
            SYNCTEX_DECODE_FAILED(node,page)) {
            _synctex_error("Bad sheet record.");
        } else if (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
            _synctex_error("Missing end of sheet.");
        } else {
            /* Now set the owner */
            if (scanner->sheet) {
                synctex_node_p last_sheet = scanner->sheet;
                synctex_node_p next_sheet = NULL;
                while ((next_sheet = __synctex_tree_sibling(last_sheet))) {
                    last_sheet = next_sheet;
                }
                /* sheets have no parent */
                __synctex_tree_set_sibling(last_sheet,node);
            } else {
                scanner->sheet = node;
            }
            return (synctex_ns_s){node,SYNCTEX_STATUS_OK};
        }
        _synctex_free_node(node);
    }
    return (synctex_ns_s){NULL,SYNCTEX_STATUS_ERROR};
}
/**
 *  - requirement: scanner != NULL
 */
static synctex_ns_s _synctex_parse_new_form(synctex_scanner_p scanner) {
    synctex_node_p node;
    if ((node = _synctex_new_form(scanner))) {
        if (
            SYNCTEX_DECODE_FAILED(node,tag)) {
            _synctex_error("Bad sheet record.");
        } else if (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
            _synctex_error("Missing end of form.");
        } else {
            /* Now set the owner */
            if (scanner->form) {
                synctex_node_p last_form = scanner->form;
                synctex_node_p next_form = NULL;
                while ((next_form = __synctex_tree_sibling(last_form))) {
                    last_form = next_form;
                }
                __synctex_tree_set_sibling(last_form,node);
            } else {
                scanner->form = node;
            }
            return (synctex_ns_s){node,SYNCTEX_STATUS_OK};
        }
        _synctex_free_node(node);
    }
    return (synctex_ns_s){NULL,SYNCTEX_STATUS_ERROR};
}
#   define SYNCTEX_SHOULD_DECODE_FAILED(NODE,WHAT) \
(_synctex_data_has_##WHAT(NODE) &&(_synctex_data_decode_##WHAT(NODE)<SYNCTEX_STATUS_OK))
#   define SYNCTEX_SHOULD_DECODE_FAILED_V(NODE,WHAT) \
(_synctex_data_has_##WHAT(NODE) &&(_synctex_data_decode_##WHAT##_v(NODE)<SYNCTEX_STATUS_OK))

static synctex_status_t _synctex_data_decode_tlchvwhd(synctex_node_p node) {
    return SYNCTEX_SHOULD_DECODE_FAILED(node,tag)
    || SYNCTEX_SHOULD_DECODE_FAILED(node,line)
    || SYNCTEX_SHOULD_DECODE_FAILED(node,column)
    || SYNCTEX_SHOULD_DECODE_FAILED(node,h)
    || SYNCTEX_SHOULD_DECODE_FAILED_V(node,v)
    || SYNCTEX_SHOULD_DECODE_FAILED(node,width)
    || SYNCTEX_SHOULD_DECODE_FAILED(node,height)
    || SYNCTEX_SHOULD_DECODE_FAILED(node,depth);
}
static synctex_ns_s _synctex_parse_new_vbox(synctex_scanner_p scanner) {
    synctex_node_p node;
    if ((node = _synctex_new_vbox(scanner))) {
        if (_synctex_data_decode_tlchvwhd(node)) {
            _synctex_error("Bad vbox record.");
            _synctex_next_line(scanner);
        out:
            _synctex_free_node(node);
            return (synctex_ns_s){NULL,SYNCTEX_STATUS_ERROR};
        }
        if (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
            _synctex_error("Missing end of vbox.");
            goto out;
        }
        return (synctex_ns_s){node,SYNCTEX_STATUS_OK};
    }
    _synctex_next_line(scanner);
    return (synctex_ns_s){NULL,SYNCTEX_STATUS_ERROR};
}
SYNCTEX_INLINE static synctex_node_p __synctex_node_make_friend_tlc(synctex_node_p node);
static synctex_ns_s _synctex_parse_new_hbox(synctex_scanner_p scanner) {
    synctex_node_p node;
    if ((node = _synctex_new_hbox(scanner))) {
        if (_synctex_data_decode_tlchvwhd(node)) {
            _synctex_error("Bad hbox record.");
            _synctex_next_line(scanner);
        out:
            _synctex_free_node(node);
            return (synctex_ns_s){NULL,SYNCTEX_STATUS_ERROR};
        }
        if (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
            _synctex_error("Missing end of hbox.");
            goto out;
        }
        if (_synctex_setup_visible_hbox(node)<SYNCTEX_STATUS_OK) {
            _synctex_error("Unexpected error (_synctex_parse_new_hbox).");
            goto out;
        }
        return (synctex_ns_s){node,SYNCTEX_STATUS_OK};
    }
    _synctex_next_line(scanner);
    return (synctex_ns_s){NULL,SYNCTEX_STATUS_ERROR};
}
static synctex_ns_s _synctex_parse_new_void_vbox(synctex_scanner_p scanner) {
    synctex_node_p node;
    if ((node = _synctex_new_void_vbox(scanner))) {
        if (_synctex_data_decode_tlchvwhd(node)) {
            _synctex_error("Bad void vbox record.");
            _synctex_next_line(scanner);
        out:
            _synctex_free_node(node);
            return (synctex_ns_s){NULL,SYNCTEX_STATUS_ERROR};
        }
        if (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
            _synctex_error("Missing end of container.");
            goto out;
        }
        return (synctex_ns_s){node,SYNCTEX_STATUS_OK};
    }
    _synctex_next_line(scanner);
    return (synctex_ns_s){NULL,SYNCTEX_STATUS_ERROR};
}
static synctex_ns_s _synctex_parse_new_void_hbox(synctex_scanner_p scanner) {
    synctex_node_p node;
    if ((node = _synctex_new_void_hbox(scanner))) {
        if (_synctex_data_decode_tlchvwhd(node)) {
            _synctex_error("Bad void hbox record.");
            _synctex_next_line(scanner);
        out:
            _synctex_free_node(node);
            return (synctex_ns_s){NULL,SYNCTEX_STATUS_ERROR};
        }
        if (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
            _synctex_error("Missing end of container.");
            goto out;
        }
        return (synctex_ns_s){node,SYNCTEX_STATUS_OK};
    }
    _synctex_next_line(scanner);
    return (synctex_ns_s){NULL,SYNCTEX_STATUS_ERROR};
}
static synctex_ns_s _synctex_parse_new_kern(synctex_scanner_p scanner) {
    synctex_node_p node;
    if ((node = _synctex_new_kern(scanner))) {
        if (_synctex_data_decode_tlchvwhd(node)) {
            _synctex_error("Bad kern record.");
            _synctex_next_line(scanner);
        out:
            _synctex_free_node(node);
            return (synctex_ns_s){NULL,SYNCTEX_STATUS_ERROR};
        }
        if (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
            _synctex_error("Missing end of container.");
            goto out;
        }
        return (synctex_ns_s){node,SYNCTEX_STATUS_OK};
    }
    _synctex_next_line(scanner);
    return (synctex_ns_s){NULL,SYNCTEX_STATUS_ERROR};
}
static synctex_ns_s _synctex_parse_new_glue(synctex_scanner_p scanner) {
    synctex_node_p node;
    if ((node = _synctex_new_glue(scanner))) {
        if (_synctex_data_decode_tlchvwhd(node)) {
            _synctex_error("Bad glue record.");
            _synctex_next_line(scanner);
        out:
            _synctex_free_node(node);
            return (synctex_ns_s){NULL,SYNCTEX_STATUS_ERROR};
        }
        if (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
            _synctex_error("Missing end of container.");
            goto out;
        }
        return (synctex_ns_s){node,SYNCTEX_STATUS_OK};
    }
    _synctex_next_line(scanner);
    return (synctex_ns_s){NULL,SYNCTEX_STATUS_ERROR};
}
static synctex_ns_s _synctex_parse_new_rule(synctex_scanner_p scanner) {
    synctex_node_p node;
    if ((node = _synctex_new_rule(scanner))) {
        if (_synctex_data_decode_tlchvwhd(node)) {
            _synctex_error("Bad rule record.");
            _synctex_next_line(scanner);
        out:
            _synctex_free_node(node);
            return (synctex_ns_s){NULL,SYNCTEX_STATUS_ERROR};
        }
        if (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
            _synctex_error("Missing end of container.");
            goto out;
        }
        return (synctex_ns_s){node,SYNCTEX_STATUS_OK};
    }
    _synctex_next_line(scanner);
    return (synctex_ns_s){NULL,SYNCTEX_STATUS_ERROR};
}
static synctex_ns_s _synctex_parse_new_math(synctex_scanner_p scanner) {
    synctex_node_p node;
    if ((node = _synctex_new_math(scanner))) {
        if (_synctex_data_decode_tlchvwhd(node)) {
            _synctex_error("Bad math record.");
            _synctex_next_line(scanner);
        out:
            _synctex_free_node(node);
            return (synctex_ns_s){NULL,SYNCTEX_STATUS_ERROR};
        }
        if (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
            _synctex_error("Missing end of container.");
            goto out;
        }
        return (synctex_ns_s){node,SYNCTEX_STATUS_OK};
    }
    _synctex_next_line(scanner);
    return (synctex_ns_s){NULL,SYNCTEX_STATUS_ERROR};
}
static synctex_ns_s _synctex_parse_new_boundary(synctex_scanner_p scanner) {
    synctex_node_p node;
    if ((node = _synctex_new_boundary(scanner))) {
        if (_synctex_data_decode_tlchvwhd(node)) {
            _synctex_error("Bad boundary record.");
            _synctex_next_line(scanner);
        out:
            _synctex_free_node(node);
            return (synctex_ns_s){NULL,SYNCTEX_STATUS_ERROR};
        }
        if (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
            _synctex_error("Missing end of container.");
            goto out;
        }
        return (synctex_ns_s){node,SYNCTEX_STATUS_OK};
    }
    _synctex_next_line(scanner);
    return (synctex_ns_s){NULL,SYNCTEX_STATUS_ERROR};
}
SYNCTEX_INLINE static synctex_ns_s _synctex_parse_new_ref(synctex_scanner_p scanner) {
    synctex_node_p node;
    if ((node = _synctex_new_ref(scanner))) {
        if (SYNCTEX_DECODE_FAILED(node,tag)
            || SYNCTEX_DECODE_FAILED(node,h)
            || SYNCTEX_DECODE_FAILED_V(node,v)) {
            _synctex_error("Bad form ref record.");
            _synctex_next_line(scanner);
        out:
            _synctex_free_node(node);
            return (synctex_ns_s){NULL,SYNCTEX_STATUS_ERROR};
        }
        if (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
            _synctex_error("Missing end of container.");
            goto out;
        }
        return (synctex_ns_s){node,SYNCTEX_STATUS_OK};
    }
    _synctex_next_line(scanner);
    return (synctex_ns_s){NULL,SYNCTEX_STATUS_ERROR};
}
#   undef SYNCTEX_DECODE_FAILED
#   undef SYNCTEX_DECODE_FAILED_V

SYNCTEX_INLINE static synctex_point_s _synctex_data_point(synctex_node_p node);
SYNCTEX_INLINE static synctex_point_s _synctex_data_point_V(synctex_node_p node);
SYNCTEX_INLINE static synctex_point_s _synctex_data_set_point(synctex_node_p node, synctex_point_s point);
SYNCTEX_INLINE static synctex_box_s _synctex_data_box(synctex_node_p node);
SYNCTEX_INLINE static synctex_box_s _synctex_data_xob(synctex_node_p node);
SYNCTEX_INLINE static synctex_box_s _synctex_data_box_V(synctex_node_p node);

SYNCTEX_INLINE static synctex_node_p _synctex_input_register_line(synctex_node_p input,synctex_node_p node) {
    if (node && _synctex_data_tag(input) != _synctex_data_tag(node)) {
        input = synctex_scanner_input_with_tag(node->class_->scanner,_synctex_data_tag(node));
    }
    if (_synctex_data_line(node)>_synctex_data_line(input)) {
        _synctex_data_set_line(input,_synctex_data_line(node));
    }
    return input;
}
/**
 *  Free node and its siblings and return its detached child.
 */
SYNCTEX_INLINE static synctex_node_p _synctex_handle_pop_child(synctex_node_p handle) {
    synctex_node_p child = _synctex_tree_reset_child(handle);
    synctex_node_free(handle);
    return child;
}
/**
 *  Set the tlc of all the x nodes that are targets of
 *  x_handle and its sibling.
 *  Reset the target of x_handle and deletes its siblings.
 *  child is a node that has just been parsed and is not a boundary node.
 */
SYNCTEX_INLINE static void _synctex_handle_set_tlc(synctex_node_p x_handle, synctex_node_p child, synctex_bool_t make_friend) {
    if (x_handle) {
        synctex_node_p sibling = x_handle;
        if (child) {
            synctex_node_p target;
            while ((target = synctex_node_target(sibling))) {
                _synctex_data_set_tlc(target,child);
                if (make_friend) {
                    _synctex_node_make_friend_tlc(target);
                }
                if ((sibling = __synctex_tree_sibling(sibling))) {
                    continue;
                } else {
                    break;
                }
            }
        }
        _synctex_tree_reset_target(x_handle);
        sibling = __synctex_tree_reset_sibling(x_handle);
        synctex_node_free(sibling);
    }
}
/**
 *  When we have parsed a box, we must register
 *  all the contained heading boundary nodes
 *  that have not yet been registered.
 *  Those handles will be deleted when poping.
 */
SYNCTEX_INLINE static void _synctex_handle_make_friend_tlc(synctex_node_p node) {
    while (node) {
        synctex_node_p target = _synctex_tree_reset_target(node);
        _synctex_node_make_friend_tlc(target);
        node = __synctex_tree_sibling(node);
    }
}
/**
 *  Scan sheets, forms and input records.
 *  - parameter scanner: owning scanner
 *  - returns: status
 */
static synctex_status_t __synctex_parse_sfi(synctex_scanner_p scanner) {
    synctex_status_t status = SYNCTEX_STATUS_OK;
    synctex_zs_s zs = {0,0};
    synctex_ns_s input = SYNCTEX_NS_NULL;
    synctex_node_p sheet = NULL;
    synctex_node_p form = NULL;
    synctex_node_p parent = NULL;
    synctex_node_p child = NULL;
    /*
     *  Experimentations lead to the forthcoming conclusion:
     *  Sometimes, the first nodes of a box have the wrong line number.
     *  These are only boundary (x) nodes.
     *  We observed that boundary nodes do have the proper line number
     *  if they follow a node with a different type.
     *  We keep track of these leading x nodes in a handle tree.
     */
    synctex_node_p x_handle = NULL;
#   define SYNCTEX_RETURN(STATUS) \
        synctex_node_free(x_handle);\
        return STATUS
    synctex_node_p last_k = NULL;
    synctex_node_p last_g = NULL;
    synctex_ns_s ns = SYNCTEX_NS_NULL;
    int form_depth = 0;
    int ignored_form_depth = 0;
    synctex_bool_t try_input = synctex_YES;
    if (!(x_handle = _synctex_new_handle(scanner))) {
        SYNCTEX_RETURN(SYNCTEX_STATUS_ERROR);
    }
#	ifdef SYNCTEX_NOTHING
#       pragma mark MAIN LOOP
#   endif
main_loop:
    status = SYNCTEX_STATUS_OK;
    sheet = form = parent = child = NULL;
#   define SYNCTEX_START_SCAN(WHAT)\
(*SYNCTEX_CUR == SYNCTEX_CHAR_##WHAT)
    if (SYNCTEX_CUR<SYNCTEX_END) {
        if (SYNCTEX_START_SCAN(BEGIN_FORM)) {
#	ifdef SYNCTEX_NOTHING
#       pragma mark + SCAN FORM
#   endif
        scan_form:
            ns = _synctex_parse_new_form(scanner);
            if (ns.status == SYNCTEX_STATUS_OK) {
                ++form_depth;
                if (_synctex_tree_parent(form)) {
                    /* This form is already being parsed */
                    ++ignored_form_depth;
                    goto ignore_loop;
                }
                _synctex_tree_set_parent(ns.node,form);
                form = ns.node;
                parent = form;
                child = NULL;
                last_k = last_g = NULL;
                goto content_loop;
            }
            if (form || sheet) {
                last_k = last_g = NULL;
                goto content_loop;
            }
            try_input = synctex_YES;
            goto main_loop;
        } else if (SYNCTEX_START_SCAN(BEGIN_SHEET)) {
#	ifdef SYNCTEX_NOTHING
#       pragma mark + SCAN SHEET
#   endif
            try_input = synctex_YES;
            ns = _synctex_parse_new_sheet(scanner);
            if (ns.status == SYNCTEX_STATUS_OK) {
                sheet = ns.node;
                parent = sheet;
                last_k = last_g = NULL;
                goto content_loop;
            }
            goto main_loop;
        } else if (SYNCTEX_START_SCAN(ANCHOR)) {
#	ifdef SYNCTEX_NOTHING
#       pragma mark + SCAN ANCHOR
#   endif
        scan_anchor:
            ++SYNCTEX_CUR;
            if (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
                _synctex_error("Missing anchor.");
                SYNCTEX_RETURN(SYNCTEX_STATUS_ERROR);
            }
            if (form || sheet) {
                last_k = last_g = NULL;
                goto content_loop;
            }
            try_input = synctex_YES;
            goto main_loop;
        } else if (SYNCTEX_START_SCAN(ANCHOR)) {
#	ifdef SYNCTEX_NOTHING
#       pragma mark + SCAN COMMENT
#   endif
            ++SYNCTEX_CUR;
            _synctex_next_line(scanner);
            try_input = synctex_YES;
            goto main_loop;
        } else if (try_input) {
#	ifdef SYNCTEX_NOTHING
#       pragma mark + SCAN INPUT
#   endif
            try_input = synctex_NO;
            do {
                input = __synctex_parse_new_input(scanner);
            } while (input.status == SYNCTEX_STATUS_OK);
            goto main_loop;
        }
        status = _synctex_match_string(scanner,"Postamble:");
        if (status==SYNCTEX_STATUS_OK) {
            scanner->flags.postamble = 1;
            SYNCTEX_RETURN(status);
        }
        status = _synctex_next_line(scanner);
        if (status<SYNCTEX_STATUS_OK) {
            SYNCTEX_RETURN(status);
        }
   }
    /* At least 1 more character */
    zs = _synctex_buffer_get_available_size(scanner,1);
    if (zs.size == 0){
        _synctex_error("Incomplete synctex file, postamble missing.");
        SYNCTEX_RETURN(SYNCTEX_STATUS_ERROR);
    }
    goto main_loop;
    /*  Unreachable. */
#	ifdef SYNCTEX_NOTHING
#       pragma mark IGNORE LOOP
#   endif
ignore_loop:
    ns = SYNCTEX_NS_NULL;
    if (SYNCTEX_CUR<SYNCTEX_END) {
        if (SYNCTEX_START_SCAN(BEGIN_FORM)) {
            ++ignored_form_depth;
        } else if (SYNCTEX_START_SCAN(END_FORM)) {
            --ignored_form_depth;
        }
        if (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
            _synctex_error("Incomplete container.");
            SYNCTEX_RETURN(SYNCTEX_STATUS_ERROR);
        }
    } else {
        zs = _synctex_buffer_get_available_size(scanner,1);
        if (zs.size == 0){
            _synctex_error("Incomplete synctex file, postamble missing.");
            SYNCTEX_RETURN(SYNCTEX_STATUS_ERROR);
        }
    }
    if (ignored_form_depth) {
        goto ignore_loop;
    } else {
        last_k = last_g = NULL;
        goto content_loop;
    }

#	ifdef SYNCTEX_NOTHING
#       pragma mark CONTENT LOOP
#   endif
content_loop:
    /*  Either in a form, a sheet or a box.
     *  - in a sheet, "{" is not possible, only boxes and "}" at top level.
     *  - in a form, "{" is not possible, only boxes, "<" and ">" at top level.
     *  - in a box, the unique possibility is '<', '[', '(' or ">".
     *  We still keep the '(' for a sheet, because that dos not cost too much.
     *  We must also consider void boxes as children.
     */
    /* forms are everywhere */
    ns = SYNCTEX_NS_NULL;
#if SYNCTEX_VERBOSE
    synctex_scanner_set_display_switcher(scanner,-1);
    printf("NEW CONTENT LOOP\n");
#if SYNCTEX_DEBUG>500
    synctex_node_display(sheet);
#endif
#endif
    if (SYNCTEX_CUR<SYNCTEX_END) {
        if (SYNCTEX_START_SCAN(BEGIN_FORM)) {
            goto scan_form;
        } else if (SYNCTEX_START_SCAN(BEGIN_VBOX)) {
#	ifdef SYNCTEX_NOTHING
#       pragma mark + SCAN VBOX
#   endif
            ns = _synctex_parse_new_vbox(scanner);
            if (ns.status == SYNCTEX_STATUS_OK) {
                x_handle = _synctex_new_handle_with_child(x_handle);
                if (child) {
                    _synctex_node_set_sibling(child,ns.node);
                } else {
                    _synctex_node_set_child(parent,ns.node);
                }
                parent = ns.node;
                child = _synctex_tree_last(parent);
#   if SYNCTEX_VERBOSE
                synctex_node_log(parent);
#   endif
                input.node = _synctex_input_register_line(input.node,parent);
                last_k = last_g = NULL;
                goto content_loop;
            }
        } else if (SYNCTEX_START_SCAN(END_VBOX)) {
            if (synctex_node_type(parent) == synctex_node_type_vbox) {
#	ifdef SYNCTEX_NOTHING
#       pragma mark + SCAN XOBV
#   endif
                ++SYNCTEX_CUR;
                if (NULL == _synctex_tree_child(parent) && !form) {
                    /*  only void v boxes are friends */
                    _synctex_node_make_friend_tlc(parent);
                }
                child = parent;
                parent = _synctex_tree_parent(child);
                if (!form) {
                    _synctex_handle_make_friend_tlc(x_handle);
                }
                x_handle = _synctex_handle_pop_child(x_handle);
                _synctex_handle_set_tlc(x_handle,child,!form);
#   if SYNCTEX_VERBOSE
                synctex_node_log(child);
#   endif
                if (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
                    _synctex_error("Incomplete container.");
                    SYNCTEX_RETURN(SYNCTEX_STATUS_ERROR);
                }
                last_k = last_g = NULL;
                goto content_loop;
            }
        } else if (SYNCTEX_START_SCAN(BEGIN_HBOX)) {
#	ifdef SYNCTEX_NOTHING
#       pragma mark + SCAN HBOX
#   endif
#   if defined(SYNCTEX_USE_CHARINDEX)
            synctex_charindex_t char_index = (synctex_charindex_t)(scanner->reader->charindex_offset+SYNCTEX_CUR-SYNCTEX_START);
            synctex_lineindex_t line_index = scanner->reader->line_number;
#   endif
            ns = _synctex_parse_new_hbox(scanner);
            if (ns.status == SYNCTEX_STATUS_OK) {
                x_handle = _synctex_new_handle_with_child(x_handle);
                if (child) {
                    _synctex_node_set_sibling(child,ns.node);
                } else {
                    _synctex_node_set_child(parent,ns.node);
                }
                parent = ns.node;
                /*  add a box boundary node at the start */
                if ((child = _synctex_new_box_bdry(scanner))) {
#   if defined(SYNCTEX_USE_CHARINDEX)
                    child->line_index=line_index;
                    child->char_index=char_index;
#   endif
                    _synctex_node_set_child(parent,child);
                    _synctex_data_set_tlchv(child,parent);
                    if (!form) {
                        __synctex_node_make_friend_tlc(child);
                    }
                } else {
                    _synctex_error("Can't create box bdry record.");
                }
#   if SYNCTEX_VERBOSE
                synctex_node_log(parent);
#   endif
                input.node = _synctex_input_register_line(input.node,parent);
                last_k = last_g = NULL;
                goto content_loop;
            }
        } else if (SYNCTEX_START_SCAN(END_HBOX)) {
            if (synctex_node_type(parent) == synctex_node_type_hbox) {
#	ifdef SYNCTEX_NOTHING
#       pragma mark + SCAN XOBH
#   endif
                ++SYNCTEX_CUR;
                /*  setting the next horizontal box at the end ensures
                 * that a child is recorded before any of its ancestors.
                 */
                if (form == NULL /* && sheet != NULL*/ ) {
                    _synctex_tree_set_next_hbox(parent,_synctex_tree_next_hbox(sheet));
                    _synctex_tree_set_next_hbox(sheet,parent);
                }
                {
                    /*  Update the mean line number */
                    synctex_node_p node = _synctex_tree_child(parent);
                    synctex_node_p sibling = NULL;
                    /*  Ignore the first node (a box_bdry) */
                    if (node && (sibling = __synctex_tree_sibling(node))) {
                        unsigned int node_weight = 0;
                        unsigned int cumulated_line_numbers = 0;
                        _synctex_data_set_line(node, _synctex_data_line(sibling));
                        node = sibling;
                        do {
                            if (synctex_node_type(node)==synctex_node_type_hbox) {
                                if (_synctex_data_weight(node)) {
                                    node_weight += _synctex_data_weight(node);
                                    cumulated_line_numbers += _synctex_data_mean_line(node)*_synctex_data_weight(node);
                                } else {
                                    ++node_weight;
                                    cumulated_line_numbers += _synctex_data_mean_line(node);
                                }
                            } else {
                                ++node_weight;
                                cumulated_line_numbers += synctex_node_line(node);
                            }
                        } while ((node = __synctex_tree_sibling(node)));
                        _synctex_data_set_mean_line(parent,(cumulated_line_numbers + node_weight/2)/node_weight);
                        _synctex_data_set_weight(parent,node_weight);
                    } else {
                        _synctex_data_set_mean_line(parent,_synctex_data_line(parent));
                        _synctex_data_set_weight(parent,1);
                    }
                    if ((sibling = _synctex_new_box_bdry(scanner))) {
#   if defined(SYNCTEX_USE_CHARINDEX)
                        sibling->line_index=child->line_index;
                        sibling->char_index=child->char_index;
#   endif
                        _synctex_node_set_sibling(child,sibling);
                        {
                            synctex_node_p N = child;
                            while (synctex_node_type(N) == synctex_node_type_ref) {
                                N = _synctex_tree_arg_sibling(N);
                            }
                            _synctex_data_set_tlc(sibling,N);
                        }
                        _synctex_data_set_h(sibling,_synctex_data_h_V(parent)+_synctex_data_width_V(parent));
                        _synctex_data_set_v(sibling,_synctex_data_v_V(parent));
                        child = sibling;
                    } else {
                        _synctex_error("Can't create box bdry record.");
                    }
                    sibling = _synctex_tree_child(parent);
                    _synctex_data_set_point(sibling,_synctex_data_point_V(parent));
                    if (last_k && last_g && (child = synctex_node_child(parent))) {
                        /* Find the node preceding last_k */
                        synctex_node_p next;
                        while ((next = __synctex_tree_sibling(child))) {
                            if (next == last_k) {
                                _synctex_data_set_tlc(last_k,child);
                                _synctex_data_set_tlc(last_g,child);
                                break;
                            }
                            child = next;
                        }
                    }
                    child = parent;
                    parent = _synctex_tree_parent(child);
                    if (!form) {
                        _synctex_handle_make_friend_tlc(x_handle);
                    }
                    x_handle = _synctex_handle_pop_child(x_handle);
                    _synctex_handle_set_tlc(x_handle,child,!form);
                    _synctex_make_hbox_contain_box(parent,                                    _synctex_data_box_V(child));
#   if SYNCTEX_VERBOSE
                    synctex_node_log(child);
#   endif
                }
                if (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
                    _synctex_error("Incomplete container.");
                    SYNCTEX_RETURN(SYNCTEX_STATUS_ERROR);
                }
                last_k = last_g = NULL;
                goto content_loop;
            }
        } else if (SYNCTEX_START_SCAN(VOID_VBOX)) {
#	ifdef SYNCTEX_NOTHING
#       pragma mark + SCAN VOID VBOX
#   endif
            ns = _synctex_parse_new_void_vbox(scanner);
            if (ns.status == SYNCTEX_STATUS_OK) {
                if (child) {
                    _synctex_node_set_sibling(child,ns.node);
                } else {
                    _synctex_node_set_child(parent,ns.node);
                }
                child = ns.node;
                _synctex_handle_set_tlc(x_handle, child,!form);
#   if SYNCTEX_VERBOSE
                synctex_node_log(child);
#   endif
                input.node = _synctex_input_register_line(input.node,child);
                last_k = last_g = NULL;
                goto content_loop;
            }
        } else if (SYNCTEX_START_SCAN(VOID_HBOX)) {
#	ifdef SYNCTEX_NOTHING
#       pragma mark + SCAN VOID HBOX
#   endif
            ns = _synctex_parse_new_void_hbox(scanner);
            if (ns.status == SYNCTEX_STATUS_OK) {
                if (_synctex_data_width(ns.node)<0) {
                    printf("Negative width\n");
                }
                if (child) {
                    _synctex_node_set_sibling(child,ns.node);
                } else {
                    _synctex_node_set_child(parent,ns.node);
                }
                child = ns.node;
                _synctex_handle_set_tlc(x_handle, child,!form);
                _synctex_make_hbox_contain_box(parent,_synctex_data_box(child));
#   if SYNCTEX_VERBOSE
                synctex_node_log(child);
#   endif
                input.node = _synctex_input_register_line(input.node,child);
                last_k = last_g = NULL;
                goto content_loop;
            }
        } else if (SYNCTEX_START_SCAN(KERN)) {
#	ifdef SYNCTEX_NOTHING
#       pragma mark + SCAN KERN
#   endif
            ns = _synctex_parse_new_kern(scanner);
            if (ns.status == SYNCTEX_STATUS_OK) {
                if (child) {
                    _synctex_node_set_sibling(child,ns.node);
                } else {
                    _synctex_node_set_child(parent,ns.node);
                }
                child = ns.node;
                if (!form) {
                    __synctex_node_make_friend_tlc(child);
                }
                _synctex_handle_set_tlc(x_handle, child,!form);
                _synctex_make_hbox_contain_box(parent,_synctex_data_xob(child));
#   if SYNCTEX_VERBOSE
                synctex_node_log(child);
#   endif
                input.node = _synctex_input_register_line(input.node,child);
                last_k = child;
                last_g = NULL;
                goto content_loop;
            }
        } else if (SYNCTEX_START_SCAN(GLUE)) {
#	ifdef SYNCTEX_NOTHING
#       pragma mark + SCAN GLUE
#   endif
            ns = _synctex_parse_new_glue(scanner);
            if (ns.status == SYNCTEX_STATUS_OK) {
                if (child) {
                    _synctex_node_set_sibling(child,ns.node);
                } else {
                    _synctex_node_set_child(parent,ns.node);
                }
                child = ns.node;
                if (!form) {
                    __synctex_node_make_friend_tlc(child);
                }
                _synctex_handle_set_tlc(x_handle, child,!form);
                _synctex_make_hbox_contain_point(parent,_synctex_data_point(child));
#   if SYNCTEX_VERBOSE
                synctex_node_log(child);
#   endif
                input.node = _synctex_input_register_line(input.node,child);
                if (last_k) {
                    last_g = child;
                } else {
                    last_k = last_g = NULL;
                }
                goto content_loop;
            }
        } else if (SYNCTEX_START_SCAN(RULE)) {
#	ifdef SYNCTEX_NOTHING
#       pragma mark + SCAN RULE
#   endif
            ns = _synctex_parse_new_rule(scanner);
            if (ns.status == SYNCTEX_STATUS_OK) {
                if (child) {
                    _synctex_node_set_sibling(child,ns.node);
                } else {
                    _synctex_node_set_child(parent,ns.node);
                }
                child = ns.node;
                if (!form) {
                    __synctex_node_make_friend_tlc(child);
                }
                _synctex_handle_set_tlc(x_handle, child,!form);
                /* Rules are sometimes far too big
_synctex_make_hbox_contain_box(parent,_synctex_data_box(child));
                 */
#   if SYNCTEX_VERBOSE
                synctex_node_log(child);
#   endif
                input.node = _synctex_input_register_line(input.node,child);
                last_k = last_g = NULL;
                goto content_loop;
            }
        } else if (SYNCTEX_START_SCAN(MATH)) {
#	ifdef SYNCTEX_NOTHING
#       pragma mark + SCAN MATH
#   endif
            ns = _synctex_parse_new_math(scanner);
            if (ns.status == SYNCTEX_STATUS_OK) {
                if (child) {
                    _synctex_node_set_sibling(child,ns.node);
                } else {
                    _synctex_node_set_child(parent,ns.node);
                }
                child = ns.node;
                if (!form) {
                    __synctex_node_make_friend_tlc(child);
                }
                _synctex_handle_set_tlc(x_handle, child,!form);
                _synctex_make_hbox_contain_point(parent,_synctex_data_point(child));
#   if SYNCTEX_VERBOSE
                synctex_node_log(child);
#   endif
                input.node = _synctex_input_register_line(input.node,child);
                last_k = last_g = NULL;
                goto content_loop;
            }
        } else if (SYNCTEX_START_SCAN(FORM_REF)) {
#	ifdef SYNCTEX_NOTHING
#       pragma mark + SCAN FORM REF
#   endif
#if SYNCTEX_DEBUG>500
            synctex_node_display(parent);
            synctex_node_display(child);
#endif
            ns = _synctex_parse_new_ref(scanner);
            if (ns.status == SYNCTEX_STATUS_OK) {
                if (child) {
                    _synctex_node_set_sibling(child,ns.node);
                } else {
                    _synctex_node_set_child(parent,ns.node);
                }
                child = ns.node;
                if (form) {
                    if (scanner->ref_in_form) {
                        synctex_tree_set_friend(child,scanner->ref_in_form);
                    }
                    scanner->ref_in_form = child;
                } else {
                    if (scanner->ref_in_sheet) {
                        synctex_tree_set_friend(child,scanner->ref_in_sheet);
                    }
                    scanner->ref_in_sheet = child;
                }
#   if SYNCTEX_VERBOSE
                synctex_node_log(child);
#   endif
                last_k = last_g = NULL;
                goto content_loop;
            }
        } else if (SYNCTEX_START_SCAN(BOUNDARY)) {
#	ifdef SYNCTEX_NOTHING
#       pragma mark + SCAN BOUNDARY
#   endif
            ns = _synctex_parse_new_boundary(scanner);
            if (ns.status == SYNCTEX_STATUS_OK) {
                if (child) {
                    _synctex_node_set_sibling(child,ns.node);
                } else {
                    _synctex_node_set_child(parent,ns.node);
                }
                if (synctex_node_type(child)==synctex_node_type_box_bdry
                    || _synctex_tree_target(x_handle)) {
                    child = _synctex_tree_reset_child(x_handle);
                    child = _synctex_new_handle_with_child(child);
                    __synctex_tree_set_sibling(child, x_handle);
                    x_handle = child;
                    _synctex_tree_set_target(x_handle,ns.node);
                } else if (!form) {
                    __synctex_node_make_friend_tlc(ns.node);
                }
                child = ns.node;
                _synctex_make_hbox_contain_point(parent,_synctex_data_point(child));
#   if SYNCTEX_VERBOSE
                synctex_node_log(child);
#   endif
                input.node = _synctex_input_register_line(input.node,child);
                last_k = last_g = NULL;
                goto content_loop;
            }
        } else if (SYNCTEX_START_SCAN(CHARACTER)) {
#	ifdef SYNCTEX_NOTHING
#       pragma mark + SCAN CHARACTER
#   endif
            ++SYNCTEX_CUR;
            if (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
                _synctex_error("Missing end of container.");
                SYNCTEX_RETURN(SYNCTEX_STATUS_ERROR);
            }
            last_k = last_g = NULL;
            goto content_loop;
        } else if (SYNCTEX_START_SCAN(ANCHOR)) {
#	ifdef SYNCTEX_NOTHING
#       pragma mark + SCAN ANCHOR
#   endif
            goto scan_anchor;
        } else if (SYNCTEX_START_SCAN(END_SHEET)) {
            if (sheet && parent == sheet) {
#	ifdef SYNCTEX_NOTHING
#       pragma mark + SCAN TEEHS
#   endif
                ++SYNCTEX_CUR;
                if (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
                    _synctex_error("Missing anchor.");
                }
                parent = sheet = NULL;
                goto main_loop;
            }
        } else if (SYNCTEX_START_SCAN(END_FORM)) {
            if (parent == form && form_depth > 0) {
#	ifdef SYNCTEX_NOTHING
#       pragma mark + SCAN MROF
#   endif
                ++SYNCTEX_CUR;
                --form_depth;
                if (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK
                    && (form_depth || sheet)) {
                    _synctex_error("Missing end of container.");
                    SYNCTEX_RETURN(SYNCTEX_STATUS_ERROR);
                }
                if ((parent = _synctex_tree_parent(form))) {
                    _synctex_tree_reset_parent(form);
                    child = form;
                    form = parent;
                    goto content_loop;
                } else if (sheet) {
                    form = NULL;
                    parent = sheet;
                    child = synctex_node_last_sibling(child);
                    goto content_loop;
                }
                goto main_loop;
            }
        }
        _synctex_error("Ignored record <%.20s...>(line %i)\n",SYNCTEX_CUR, scanner->reader->line_number+1);
        if (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
            _synctex_error("Missing end of sheet/form.");
            SYNCTEX_RETURN(SYNCTEX_STATUS_ERROR);
        }
        last_k = last_g = NULL;
        goto content_loop;
    }
    zs = _synctex_buffer_get_available_size(scanner,1);
    if (zs.size == 0){
        _synctex_error("Incomplete synctex file, postamble missing.");
        SYNCTEX_RETURN(SYNCTEX_STATUS_ERROR);
    }
    last_k = last_g = NULL;
    goto content_loop;
}
#undef SYNCTEX_RETURN
/**
 *  Replace ref in its tree hierarchy by a single box
 *  proxy to the contents of the associated form.
 *  - argument ref: a ref node with no friend
 *  - return the proxy created.
 *  - note: Does nothing if ref is not owned.
 *  - note: On return, ref will have no parent nor sibling.
 *      The caller is responsible for releasing ref.
 *  - note: this is where root proxies are created.
 *  - note: the target of the root proxy is the content
 *      of a form.
 */
SYNCTEX_INLINE static synctex_ns_s __synctex_replace_ref(synctex_node_p ref) {
    synctex_ns_s ns = {NULL,SYNCTEX_STATUS_OK};
    synctex_node_p parent;
    if ((parent = _synctex_tree_parent(ref))) {
        synctex_node_p sibling = __synctex_tree_reset_sibling(ref);
        synctex_node_p arg_sibling = synctex_node_arg_sibling(ref);
        /*  arg_sibling != NULL because the child of a box
         *  is always a box boundary, not a ref. */
        synctex_node_p target = synctex_form_content(ref->class_->scanner, _synctex_data_tag(ref));
        /*  The target is a single node (box)
         *  with children and no siblings. */
        if ((ns.node = __synctex_new_proxy_from_ref_to(ref, target))) {
            /*  Insert this proxy instead of ref. */
            _synctex_node_set_sibling(arg_sibling,ns.node);
            /*  Then append the original sibling of ref. */
            _synctex_node_set_sibling(ns.node,sibling);
#   if defined(SYNCTEX_USE_CHARINDEX)
            if (synctex_node_type(sibling) == synctex_node_type_box_bdry) {
                /*  The sibling is the last box boundary
                 *  which may have a less accurate information */
                sibling->char_index = arg_sibling->char_index;
                sibling->line_index = arg_sibling->line_index;
            }
#endif
#if SYNCTEX_DEBUG>500
            printf("!  Ref replacement:\n");
            synctex_node_log(ref);
            synctex_node_display(synctex_node_sibling(ref));
#endif
        } else /*  simply remove ref */ {
            _synctex_tree_set_sibling(arg_sibling,sibling);
        }
        __synctex_tree_reset_parent(ref);
    } else {
        _synctex_error("!  Missing parent in __synctex_replace_ref. "
                       "Please report.");
        ns.status = SYNCTEX_STATUS_BAD_ARGUMENT;
    }
    return ns;
}
/**
 *  - argument ref: is the starting point of a linked list
 *      of refs. The link is made through the friend field.
 *  - returns: the status and the list of all the proxies
 *      created. The link is made through the friend field.
 *  - note: All refs are freed
 */
SYNCTEX_INLINE static synctex_ns_s _synctex_post_process_ref(synctex_node_p ref) {
    synctex_ns_s ns = {NULL, SYNCTEX_STATUS_OK};
    while (ref) {
        synctex_node_p next_ref = _synctex_tree_reset_friend(ref);
        synctex_ns_s sub_ns = __synctex_replace_ref(ref);
        if (sub_ns.status < ns.status) {
            ns.status = sub_ns.status;
        } else {
            /*  Insert all the created proxies in the list
             *  sub_ns.node is the last friend,
             */
            synctex_tree_set_friend(sub_ns.node,ns.node);
            ns.node = sub_ns.node;
        }
        synctex_node_free(ref);
        ref = next_ref;
    }
    return ns;
}
typedef synctex_node_p (* synctex_processor_f)(synctex_node_p node);
/**
 *  Apply the processor f to the tree hierarchy rooted at proxy.
 *  proxy has replaced a form ref, no children yet.
 *  As a side effect all the hierarchy of nodes will be created.
 */
SYNCTEX_INLINE static synctex_status_t _synctex_post_process_proxy(synctex_node_p proxy, synctex_processor_f f) {
    while(proxy) {
        synctex_node_p next_proxy = _synctex_tree_friend(proxy);
        synctex_node_p halt = __synctex_tree_sibling(proxy);
        /*  if proxy is the last sibling, halt is NULL.
         *  Find what should be a next node,
         *  without creating new nodes. */
        if (!halt) {
            synctex_node_p parent = _synctex_tree_parent(proxy);
            halt = __synctex_tree_sibling(parent);
            while (!halt && parent) {
                parent = _synctex_tree_parent(parent);
                halt = __synctex_tree_sibling(parent);
            }
        }
        do {
#if SYNCTEX_DEBUG>500
            printf("POST PROCESSING %s\n",_synctex_node_abstract(proxy));
            {
                int i,j = 0;
                for (i=0;i<proxy->class_->scanner->number_of_lists;++i) {
                    synctex_node_p N = proxy->class_->scanner->lists_of_friends[i];
                    do {
                        if (N==proxy) {
                            ++j;
                            printf("%s",_synctex_node_abstract(N));
                        }
                    } while ((N = _synctex_tree_friend(N)));
                }
                if (j) {
                    printf("\nBeforehand %i match\n",j);
                }
            }
#endif
            f(proxy);
#if SYNCTEX_DEBUG>500
            {
                int i,j = 0;
                for (i=0;i<proxy->class_->scanner->number_of_lists;++i) {
                    synctex_node_p N = proxy->class_->scanner->lists_of_friends[i];
                    do {
                        if (N==proxy) {
                            ++j;
                            printf("%s",_synctex_node_abstract(N));
                        }
                    } while ((N = _synctex_tree_friend(N)));
                }
                if (j) {
                    printf("\n%i match\n",j);
                }
            }
#endif
            /*  Side effect: create the hierarchy on the fly */
            proxy = synctex_node_next(proxy); /*  Change is here */
#if SYNCTEX_DEBUG>500
            if (proxy) {
                int i,j = 0;
                for (i=0;i<proxy->class_->scanner->number_of_lists;++i) {
                    synctex_node_p N = proxy->class_->scanner->lists_of_friends[i];
                    do {
                        if (N==proxy) {
                            ++j;
                            printf("%s",_synctex_node_abstract(N));
                        }
                    } while ((N = _synctex_tree_friend(N)));
                }
                if (j) {
                    printf("\nnext %i match\n",j);
                }
            }
#endif
        } while (proxy && proxy != halt);
        proxy = next_proxy;
    }
    return SYNCTEX_STATUS_OK;
}
/**
 *  Replace all the form refs by root box proxies.
 *  Create the node hierarchy and update the friends.
 *  On entry, the refs are collected as a friend list
 *  in either a form or a sheet
 *  - parameter: the owning scanner
 */
SYNCTEX_INLINE static synctex_status_t _synctex_post_process(synctex_scanner_p scanner) {
    synctex_status_t status = SYNCTEX_STATUS_OK;
    synctex_ns_s ns = {NULL,SYNCTEX_STATUS_NOT_OK};
#if SYNCTEX_DEBUG>500
    printf("!  entering _synctex_post_process.\n");
    synctex_node_display(scanner->sheet);
    synctex_node_display(scanner->form);
#endif
    /*  replace form refs inside forms by box proxies */
    ns = _synctex_post_process_ref(scanner->ref_in_form);
    scanner->ref_in_form = NULL;/*  it was just released */
    if (ns.status<status) {
        status = ns.status;
    }
#if SYNCTEX_DEBUG>500
    printf("!  ref replaced in form _synctex_post_process.\n");
    synctex_node_display(scanner->form);
#endif
    /*  Create all the form proxy nodes on the fly.
     *  ns.node is the root of the list of
     *  newly created proxies.
     *  There might be a problem with cascading proxies.
     *  In order to be properly managed, the data must
     *  be organized in the right way.
     *  The inserted form must be defined before
     *  the inserting one. *TeX will take care of that.   */
    ns.status = _synctex_post_process_proxy(ns.node,&_synctex_tree_reset_friend);
    if (ns.status<status) {
        status = ns.status;
    }
    /*  replace form refs inside sheets by box proxies */
    ns = _synctex_post_process_ref(scanner->ref_in_sheet);
    if (ns.status<status) {
        status = ns.status;
    }
    scanner->ref_in_sheet = NULL;
#if SYNCTEX_DEBUG>500
    printf("!  ref replaced in sheet _synctex_post_process.\n");
    synctex_node_display(scanner->sheet);
#endif
#if 0
    {
        int i;
        for (i=0;i<scanner->number_of_lists;++i) {
            synctex_node_p P = ns.node;
            do {
                synctex_node_p N = scanner->lists_of_friends[i];
                do {
                    if (P == N) {
                        printf("Already registered.\n");
                        synctex_node_display(N);
                        break;
                    }
                } while ((N = _synctex_tree_friend(N)));
            } while((P = _synctex_tree_friend(P)));
        }
    }
#endif
#if SYNCTEX_DEBUG>10000
    {
        int i;
        for (i=0;i<scanner->number_of_lists;++i) {
            synctex_node_p P = scanner->lists_of_friends[i];
            int j = 0;
            while (P) {
                ++j;
                synctex_node_log(P);
                P = _synctex_tree_friend(P);
            }
            if (j) {
                printf("friends %i -> # %i\n",i,j);
            }
        }
    }
#endif
    ns.status = _synctex_post_process_proxy(ns.node,&__synctex_proxy_make_friend_and_next_hbox);
    if (ns.status<status) {
        status = ns.status;
    }
#if SYNCTEX_DEBUG>500
    printf("!  exiting _synctex_post_process.\n");
    synctex_node_display(scanner->sheet);
    synctex_node_display(scanner->form);
    printf("!  display all.\n");
    synctex_node_display(scanner->sheet);
    synctex_node_display(scanner->form);
#endif
    return status;
}
/*  Used when parsing the synctex file
 */
static synctex_status_t _synctex_scan_content(synctex_scanner_p scanner) {
    if (NULL == scanner) {
        return SYNCTEX_STATUS_BAD_ARGUMENT;
    }
    scanner->reader->lastv = -1;
    synctex_status_t status = 0;
    /*  Find where this section starts */
content_not_found:
    status = _synctex_match_string(scanner,"Content:");
    if (status<SYNCTEX_STATUS_EOF) {
        return status;
    }
    if (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
        _synctex_error("Incomplete Content.");
        return SYNCTEX_STATUS_ERROR;
    }
    if (status == SYNCTEX_STATUS_NOT_OK) {
        goto content_not_found;
    }
    status = __synctex_parse_sfi(scanner);
    if (status == SYNCTEX_STATUS_OK) {
        status = _synctex_post_process(scanner);
    }
    return status;
}
synctex_scanner_p synctex_scanner_new() {
    synctex_scanner_p scanner =(synctex_scanner_p)_synctex_malloc(sizeof(synctex_scanner_s));
    if (scanner) {
        if (!(scanner->reader = _synctex_malloc(sizeof(synctex_reader_s)))) {
            _synctex_free(scanner);
            return NULL;
        }
#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#   endif
#   define DEFINE_synctex_scanner_class(NAME)\
    scanner->class_[synctex_node_type_##NAME] = synctex_class_##NAME;\
(scanner->class_[synctex_node_type_##NAME]).scanner = scanner
        DEFINE_synctex_scanner_class(input);
        DEFINE_synctex_scanner_class(sheet);
        DEFINE_synctex_scanner_class(form);
        DEFINE_synctex_scanner_class(hbox);
        DEFINE_synctex_scanner_class(void_hbox);
        DEFINE_synctex_scanner_class(vbox);
        DEFINE_synctex_scanner_class(void_vbox);
        DEFINE_synctex_scanner_class(kern);
        DEFINE_synctex_scanner_class(glue);
        DEFINE_synctex_scanner_class(rule);
        DEFINE_synctex_scanner_class(math);
        DEFINE_synctex_scanner_class(boundary);
        DEFINE_synctex_scanner_class(box_bdry);
        DEFINE_synctex_scanner_class(ref);
        DEFINE_synctex_scanner_class(proxy_hbox);
        DEFINE_synctex_scanner_class(proxy_vbox);
        DEFINE_synctex_scanner_class(proxy);
        DEFINE_synctex_scanner_class(proxy_last);
        DEFINE_synctex_scanner_class(handle);
        /*  set up the lists of friends */
        scanner->number_of_lists = 1024;
        scanner->lists_of_friends = (synctex_node_r)_synctex_malloc(scanner->number_of_lists*sizeof(synctex_node_p));
        if (NULL == scanner->lists_of_friends) {
            synctex_scanner_free(scanner);
            _synctex_error("malloc:2");
            return NULL;
        }
        scanner->display_switcher = 100;
        scanner->display_prompt = (char *)_synctex_display_prompt+strlen(_synctex_display_prompt)-1;
    }
    return scanner;
}
/*  Where the synctex scanner is created. */
synctex_scanner_p synctex_scanner_new_with_output_file(const char * output, const char * build_directory, int parse) {
    synctex_scanner_p scanner = synctex_scanner_new();
    if (NULL == scanner) {
        _synctex_error("malloc problem");
        return NULL;
    }
    if (synctex_reader_init_with_output_file(scanner->reader, output, build_directory)) {
        return parse? synctex_scanner_parse(scanner):scanner;
    }
    // don't warn to terminal if no file is present, this is a library.
    // _synctex_error("No file?");
    synctex_scanner_free(scanner);
    return NULL;
}

/*  The scanner destructor
 */
int synctex_scanner_free(synctex_scanner_p scanner) {
    int node_count = 0;
    if (scanner) {
        synctex_node_free(scanner->sheet);
        synctex_node_free(scanner->form);
        synctex_node_free(scanner->input);
        synctex_reader_free(scanner->reader);
        SYNCTEX_SCANNER_FREE_HANDLE(scanner);
        synctex_iterator_free(scanner->iterator);
        free(scanner->output_fmt);
        free(scanner->lists_of_friends);
#if SYNCTEX_USE_NODE_COUNT>0
        node_count = scanner->node_count;
#endif
        free(scanner);
    }
    return node_count;
}

/*  Where the synctex scanner parses the contents of the file. */
synctex_scanner_p synctex_scanner_parse(synctex_scanner_p scanner) {
    synctex_status_t status = 0;
    if (!scanner || scanner->flags.has_parsed) {
        return scanner;
    }
    scanner->flags.has_parsed=1;
    scanner->pre_magnification = 1000;
    scanner->pre_unit = 8192;
    scanner->pre_x_offset = scanner->pre_y_offset = 578;
    /*  initialize the offset with a fake improbable value,
     *  If there is a post scriptum section, this value will be overridden by the real life value */
    scanner->x_offset = scanner->y_offset = 6.027e23f;
    scanner->reader->line_number = 1;
    
    synctex_scanner_set_display_switcher(scanner, 1000);
    SYNCTEX_END = SYNCTEX_START+SYNCTEX_BUFFER_SIZE;
    /*  SYNCTEX_END always points to a null terminating character.
     *  Maybe there is another null terminating character between SYNCTEX_CUR and SYNCTEX_END-1.
     *  At least, we are sure that SYNCTEX_CUR points to a string covering a valid part of the memory. */
    *SYNCTEX_END = '\0';
    SYNCTEX_CUR = SYNCTEX_END;
#   if defined(SYNCTEX_USE_CHARINDEX)
    scanner->reader->charindex_offset = -SYNCTEX_BUFFER_SIZE;
#   endif
    status = _synctex_scan_preamble(scanner);
    if (status<SYNCTEX_STATUS_OK) {
        _synctex_error("Bad preamble\n");
        bailey:
#ifdef SYNCTEX_DEBUG
            return scanner;
#else
            synctex_scanner_free(scanner);
            return NULL;
#endif
    }
    status = _synctex_scan_content(scanner);
    if (status<SYNCTEX_STATUS_OK) {
        _synctex_error("Bad content\n");
        goto bailey;
    }
    status = _synctex_scan_postamble(scanner);
    if (status<SYNCTEX_STATUS_OK) {
        _synctex_error("Bad postamble. Ignored\n");
    }
#if SYNCTEX_DEBUG>500
    synctex_scanner_set_display_switcher(scanner, 100);
    synctex_node_display(scanner->sheet);
    synctex_node_display(scanner->form);
#endif
    synctex_scanner_set_display_switcher(scanner, 1000);
    /*  Everything is finished, free the buffer, close the file */
    free((void *)SYNCTEX_START);
    SYNCTEX_START = SYNCTEX_CUR = SYNCTEX_END = NULL;
    gzclose(SYNCTEX_FILE);
    SYNCTEX_FILE = NULL;
    /*  Final tuning: set the default values for various parameters */
    /*  1 pre_unit = (scanner->pre_unit)/65536 pt = (scanner->pre_unit)/65781.76 bp
     * 1 pt = 65536 sp */
    if (scanner->pre_unit<=0) {
        scanner->pre_unit = 8192;
    }
    if (scanner->pre_magnification<=0) {
        scanner->pre_magnification = 1000;
    }
    if (scanner->unit <= 0) {
        /*  no post magnification */
        scanner->unit = scanner->pre_unit / 65781.76;/*  65781.76 or 65536.0*/
    } else {
        /*  post magnification */
        scanner->unit *= scanner->pre_unit / 65781.76;
    }
    scanner->unit *= scanner->pre_magnification / 1000.0;
    if (scanner->x_offset > 6e23) {
        /*  no post offset */
        scanner->x_offset = scanner->pre_x_offset * (scanner->pre_unit / 65781.76);
        scanner->y_offset = scanner->pre_y_offset * (scanner->pre_unit / 65781.76);
    } else {
        /*  post offset */
        scanner->x_offset /= 65781.76f;
        scanner->y_offset /= 65781.76f;
    }
    return scanner;
#undef SYNCTEX_FILE
}

/*  Scanner accessors.
 */
int synctex_scanner_pre_x_offset(synctex_scanner_p scanner){
    return scanner?scanner->pre_x_offset:0;
}
int synctex_scanner_pre_y_offset(synctex_scanner_p scanner){
    return scanner?scanner->pre_y_offset:0;
}
int synctex_scanner_x_offset(synctex_scanner_p scanner){
    return scanner?scanner->x_offset:0;
}
int synctex_scanner_y_offset(synctex_scanner_p scanner){
    return scanner?scanner->y_offset:0;
}
float synctex_scanner_magnification(synctex_scanner_p scanner){
    return scanner?scanner->unit:1;
}
void synctex_scanner_display(synctex_scanner_p scanner) {
    if (NULL == scanner) {
        return;
    }
    printf("The scanner:\noutput:%s\noutput_fmt:%s\nversion:%i\n",scanner->reader->output,scanner->output_fmt,scanner->version);
    printf("pre_unit:%i\nx_offset:%i\ny_offset:%i\n",scanner->pre_unit,scanner->pre_x_offset,scanner->pre_y_offset);
    printf("count:%i\npost_magnification:%f\npost_x_offset:%f\npost_y_offset:%f\n",
           scanner->count,scanner->unit,scanner->x_offset,scanner->y_offset);
    printf("The input:\n");
    synctex_node_display(scanner->input);
    if (scanner->count<1000) {
        printf("The sheets:\n");
        synctex_node_display(scanner->sheet);
        printf("The friends:\n");
        if (scanner->lists_of_friends) {
            int i = scanner->number_of_lists;
            synctex_node_p node;
            while(i--) {
                printf("Friend index:%i\n",i);
                node = (scanner->lists_of_friends)[i];
                while(node) {
                    printf("%s:%i,%i\n",
                           synctex_node_isa(node),
                           _synctex_data_tag(node),
                           _synctex_data_line(node)
                           );
                    node = _synctex_tree_friend(node);
                }
            }
        }
    } else {
        printf("SyncTeX Warning: Too many objects\n");
    }
}
/*  Public */
const char * synctex_scanner_get_name(synctex_scanner_p scanner,int tag) {
    synctex_node_p input = NULL;
    if (NULL == scanner) {
        return NULL;
    }
    if ((input = scanner->input)) {;
        do {
            if (tag == _synctex_data_tag(input)) {
                return (_synctex_data_name(input));
            }
        } while((input = __synctex_tree_sibling(input)));
    }
    return NULL;
}
const char * synctex_node_get_name(synctex_node_p node) {
    if (node) {
        return synctex_scanner_get_name(node->class_->scanner,_synctex_data_tag(node));
    }
    return NULL;
}

static int _synctex_scanner_get_tag(synctex_scanner_p scanner,const char * name);
static int _synctex_scanner_get_tag(synctex_scanner_p scanner,const char * name) {
    synctex_node_p input = NULL;
    if (NULL == scanner) {
        return 0;
    }
    if ((input = scanner->input)) {
        do {
            if (_synctex_is_equivalent_file_name(name,(_synctex_data_name(input)))) {
                return _synctex_data_tag(input);
            }
        } while((input = __synctex_tree_sibling(input)));
    }
    //  2011 version
    name = _synctex_base_name(name);
    if ((input = scanner->input)) {
        do {
            if (_synctex_is_equivalent_file_name(name,_synctex_base_name(_synctex_data_name(input)))) {
                synctex_node_p other_input = input;
                while((other_input = __synctex_tree_sibling(other_input))) {
                    if (_synctex_is_equivalent_file_name(name,_synctex_base_name(_synctex_data_name(other_input)))
                        && (strlen(_synctex_data_name(input))!=strlen(_synctex_data_name(other_input))
                            || strncmp(_synctex_data_name(other_input),_synctex_data_name(input),strlen(_synctex_data_name(input))))) {
                            // There is a second possible candidate
                            return 0;
                        }
                }
                return _synctex_data_tag(input);
            }
        } while((input = __synctex_tree_sibling(input)));
    }
    return 0;
}

int synctex_scanner_get_tag(synctex_scanner_p scanner,const char * name) {
    size_t char_index = strlen(name);
    if ((scanner = synctex_scanner_parse(scanner)) && (0 < char_index)) {
        /*  the name is not void */
        char_index -= 1;
        if (!SYNCTEX_IS_PATH_SEPARATOR(name[char_index])) {
            /*  the last character of name is not a path separator */
            int result = _synctex_scanner_get_tag(scanner,name);
            if (result) {
                return result;
            } else {
                /*  the given name was not the one known by TeX
                 *  try a name relative to the enclosing directory of the scanner->output file */
                const char * relative = name;
                const char * ptr = scanner->reader->output;
                while((strlen(relative) > 0) && (strlen(ptr) > 0) && (*relative == *ptr))
                {
                    relative += 1;
                    ptr += 1;
                }
                /*  Find the last path separator before relative */
                while(relative > name) {
                    if (SYNCTEX_IS_PATH_SEPARATOR(*(relative-1))) {
                        break;
                    }
                    relative -= 1;
                }
                if ((relative > name) && (result = _synctex_scanner_get_tag(scanner,relative))) {
                    return result;
                }
                if (SYNCTEX_IS_PATH_SEPARATOR(name[0])) {
                    /*  No tag found for the given absolute name,
                     *  Try each relative path starting from the shortest one */
                    while(0<char_index) {
                        char_index -= 1;
                        if (SYNCTEX_IS_PATH_SEPARATOR(name[char_index])
                            && (result = _synctex_scanner_get_tag(scanner,name+char_index+1))) {
                            return result;
                        }
                    }
                }
            }
            return result;
        }
    }
    return 0;
}
synctex_node_p synctex_scanner_input(synctex_scanner_p scanner) {
    return scanner?scanner->input:NULL;
}
synctex_node_p synctex_scanner_input_with_tag(synctex_scanner_p scanner, int tag) {
    synctex_node_p input = scanner?scanner->input:NULL;
    while (_synctex_data_tag(input)!=tag) {
        if ((input = __synctex_tree_sibling(input))) {
            continue;
        }
        break;
    }
    return input;
}
const char * synctex_scanner_get_output_fmt(synctex_scanner_p scanner) {
    return NULL != scanner && scanner->output_fmt?scanner->output_fmt:"";
}
const char * synctex_scanner_get_output(synctex_scanner_p scanner) {
    return NULL != scanner && scanner->reader->output?scanner->reader->output:"";
}
const char * synctex_scanner_get_synctex(synctex_scanner_p scanner) {
    return NULL != scanner && scanner->reader->synctex?scanner->reader->synctex:"";
}
#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark Public node attributes
#   endif

#   define SYNCTEX_DEFINE_NODE_HVWHD(WHAT) \
int synctex_node_##WHAT(synctex_node_p node) { \
    return (node && node->class_->inspector->WHAT)? \
        node->class_->inspector->WHAT(node): 0; \
}
#   define SYNCTEX_DEFINE_PROXY_HV(WHAT) \
static int _synctex_proxy_##WHAT(synctex_proxy_p proxy) { \
    synctex_node_p target = _synctex_tree_target(proxy); \
    if (target) { \
        return _synctex_data_##WHAT(proxy)+synctex_node_##WHAT(target); \
    } else { \
        return proxy? _synctex_data_##WHAT(proxy): 0; \
    } \
}
#define SYNCTEX_DEFINE_PROXY_TLCWVD(WHAT) \
static int _synctex_proxy_##WHAT(synctex_proxy_p proxy) { \
    synctex_node_p target = _synctex_tree_target(proxy); \
    return target? synctex_node_##WHAT(target): 0; \
}

/**
 *  The horizontal location of the node.
 *  Idem for v, width, height and depth.
 *  - parameter node: a node with geometrical information.
 *  - returns: an integer.
 *  - requires: every proxy node has a target.
 *  - note: recursive call if the parameter has a proxy.
 *  - author: JL
 */
SYNCTEX_DEFINE_NODE_HVWHD(h)
SYNCTEX_DEFINE_NODE_HVWHD(v)
SYNCTEX_DEFINE_NODE_HVWHD(width)
SYNCTEX_DEFINE_NODE_HVWHD(height)
SYNCTEX_DEFINE_NODE_HVWHD(depth)
SYNCTEX_DEFINE_PROXY_TLCWVD(tag)
SYNCTEX_DEFINE_PROXY_TLCWVD(line)
SYNCTEX_DEFINE_PROXY_TLCWVD(column)
SYNCTEX_DEFINE_PROXY_HV(h)
SYNCTEX_DEFINE_PROXY_HV(v)
SYNCTEX_DEFINE_PROXY_TLCWVD(width)
SYNCTEX_DEFINE_PROXY_TLCWVD(height)
SYNCTEX_DEFINE_PROXY_TLCWVD(depth)

/**
 *  Whether the argument is a box,
 *  either vertical or horizontal,
 *  either void or not,
 *  or a proxy to such a box.
 *  - parameter NODE: of type synctex_node_p
 *  - returns: yorn
 */

SYNCTEX_INLINE static synctex_bool_t _synctex_node_is_box(synctex_node_p node) {
    return node &&
    (node->class_->type == synctex_node_type_hbox
     || node->class_->type == synctex_node_type_void_hbox
     || node->class_->type == synctex_node_type_vbox
     || node->class_->type == synctex_node_type_void_vbox
     || _synctex_node_is_box(_synctex_tree_target(node)));
}

/**
 *  Whether the argument is a handle.
 *  Handles are similar to proxies because they have a target.
 *  They are used for query results.
 *  - parameter NODE: of type synctex_node_p
 *  - returns: yorn
 */

SYNCTEX_INLINE static synctex_bool_t _synctex_node_is_handle(synctex_node_p node) {
    return node &&
    (node->class_->type == synctex_node_type_handle);
}

/**
 *  Resolves handle indirection.
 *  - parameter node: of type synctex_node_p
 *  - returns: node if it is not a handle,
 *  its target otherwise.
 */

SYNCTEX_INLINE static synctex_node_p _synctex_node_or_handle_target(synctex_node_p node) {
    return _synctex_node_is_handle(node)?
    _synctex_tree_target(node):node;
}

/**
 *  Whether the argument is an hbox.
 *  - parameter NODE: of type synctex_node_p
 *  - returns: yorn
 */

SYNCTEX_INLINE static synctex_bool_t _synctex_node_is_hbox(synctex_node_p node) {
    return node &&
    (node->class_->type == synctex_node_type_hbox
     || node->class_->type == synctex_node_type_void_hbox
     || _synctex_node_is_hbox(_synctex_tree_target(node)));
}

/**
 *  The horizontal location of the first box enclosing node.
 *  - parameter node: a node with geometrical information.
 *  - returns: an integer.
 *  - author: JL
 */
int synctex_node_box_h(synctex_node_p node) {
    if (_synctex_node_is_box(node) || (node = _synctex_tree_parent(node))) {
        return synctex_node_h(node);
    }
    return 0;
}
/**
 *  The vertical location of the first box enclosing node.
 *  - parameter node: a node with geometrical information.
 *  - returns: an integer.
 *  - author: JL
 */
int synctex_node_box_v(synctex_node_p node) {
    if (_synctex_node_is_box(node) || (node = _synctex_tree_parent(node))) {
        return synctex_node_v(node);
    }
    return 0;
}
/**
 *  The width of the first box enclosing node.
 *  - parameter node: a node with geometrical information.
 *  - returns: an integer.
 *  - author: JL
 */
int synctex_node_box_width(synctex_node_p node) {
    if (_synctex_node_is_box(node) || (node = _synctex_tree_parent(node))) {
        return synctex_node_width(node);
    }
    return 0;
}
/**
 *  The height of the first box enclosing node.
 *  - parameter node: a node with geometrical information.
 *  - returns: an integer.
 *  - author: JL
 */
int synctex_node_box_height(synctex_node_p node) {
    if (_synctex_node_is_box(node) || (node = _synctex_tree_parent(node))) {
        return synctex_node_height(node);
    }
    return 0;
}
/**
 *  The depth of the first box enclosing node.
 *  - parameter node: a node with geometrical information.
 *  - returns: an integer.
 *  - author: JL
 */
int synctex_node_box_depth(synctex_node_p node) {
    if (_synctex_node_is_box(node) || (node = _synctex_tree_parent(node))) {
        return synctex_node_depth(node);
    }
    return 0;
}
/**
 *  The horizontal location of an hbox, corrected with contents.
 *  - parameter node: an hbox node.
 *  - returns: an integer, 0 if node is not an hbox or an hbox proxy.
 *  - note: recursive call when node is an hbox proxy.
 *  - author: JL
 */
int synctex_node_hbox_h(synctex_node_p node) {
    switch(synctex_node_type(node)) {
        case synctex_node_type_hbox:
            return _synctex_data_h_V(node);
        case synctex_node_type_proxy_hbox:
            return _synctex_data_h(node)+synctex_node_hbox_h(_synctex_tree_target(node));
        default:
            return 0;
    }
}
/**
 *  The vertical location of an hbox, corrected with contents.
 *  - parameter node: an hbox node.
 *  - returns: an integer, 0 if node is not an hbox or an hbox proxy.
 *  - note: recursive call when node is an hbox proxy.
 *  - author: JL
 */
int synctex_node_hbox_v(synctex_node_p node) {
    switch(synctex_node_type(node)) {
        case synctex_node_type_hbox:
            return _synctex_data_v_V(node);
        case synctex_node_type_proxy_hbox:
            return _synctex_data_v(node)+synctex_node_hbox_v(_synctex_tree_target(node));
        default:
            return 0;
    }
}
/**
 *  The width of an hbox, corrected with contents.
 *  - parameter node: an hbox node, 0 if node is not an hbox or an hbox proxy.
 *  - returns: an integer.
 *  - author: JL
 */
int synctex_node_hbox_width(synctex_node_p node) {
    synctex_node_p target = _synctex_tree_target(node);
    if (target) {
        node = target;
    }
    return synctex_node_type(node) == synctex_node_type_hbox?
    _synctex_data_width_V(node): 0;
}
/**
 *  The height of an hbox, corrected with contents.
 *  - parameter node: an hbox node.
 *  - returns: an integer, 0 if node is not an hbox or an hbox proxy.
 *  - author: JL
 */
int synctex_node_hbox_height(synctex_node_p node) {
    synctex_node_p target = _synctex_tree_target(node);
    if (target) {
        node = target;
    }
    return synctex_node_type(node) == synctex_node_type_hbox?
    _synctex_data_height_V(node): 0;
}
/**
 *  The depth of an hbox, corrected with contents.
 *  - parameter node: an hbox node.
 *  - returns: an integer, 0 if node is not an hbox or an hbox proxy.
 *  - note: recursive call when node is an hbox proxy.
 *  - author: JL
 */
int synctex_node_hbox_depth(synctex_node_p node) {
    synctex_node_p target = _synctex_tree_target(node);
    if (target) {
        node = target;
    }
    return synctex_node_type(node) == synctex_node_type_hbox?
    _synctex_data_depth_V(node): 0;
}
#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark Public node visible attributes
#   endif

#define SYNCTEX_VISIBLE_SIZE(node,s) \
(s)*node->class_->scanner->unit
#define SYNCTEX_VISIBLE_DISTANCE_h(node,d) \
((d)*node->class_->scanner->unit+node->class_->scanner->x_offset)
#define SYNCTEX_VISIBLE_DISTANCE_v(node,d) \
((d)*node->class_->scanner->unit+node->class_->scanner->y_offset)
static float __synctex_node_visible_h(synctex_node_p node) {
    return SYNCTEX_VISIBLE_DISTANCE_h(node,synctex_node_h(node));
}
static float __synctex_node_visible_v(synctex_node_p node) {
    return SYNCTEX_VISIBLE_DISTANCE_v(node,synctex_node_v(node));
}
static float __synctex_node_visible_width(synctex_node_p node) {
    return SYNCTEX_VISIBLE_SIZE(node,synctex_node_width(node));
}
static float __synctex_node_visible_height(synctex_node_p node) {
    return SYNCTEX_VISIBLE_SIZE(node,synctex_node_height(node));
}
static float __synctex_node_visible_depth(synctex_node_p node) {
    return SYNCTEX_VISIBLE_SIZE(node,synctex_node_depth(node));
}
static float __synctex_proxy_visible_h(synctex_node_p node) {
    return SYNCTEX_VISIBLE_DISTANCE_h(node,synctex_node_h(node));
}
static float __synctex_proxy_visible_v(synctex_node_p node) {
    return SYNCTEX_VISIBLE_DISTANCE_v(node,synctex_node_v(node));
}
static float __synctex_proxy_visible_width(synctex_node_p node) {
    synctex_node_p target = _synctex_tree_target(node);
    return __synctex_node_visible_width(target);
}
static float __synctex_proxy_visible_height(synctex_node_p node) {
    synctex_node_p target = _synctex_tree_target(node);
    return __synctex_node_visible_height(target);
}
static float __synctex_proxy_visible_depth(synctex_node_p node) {
    synctex_node_p target = _synctex_tree_target(node);
    return __synctex_node_visible_depth(target);
}
static float __synctex_kern_visible_h(synctex_noxy_p noxy) {
    int h = _synctex_data_h(noxy);
    int width = _synctex_data_width(noxy);
    return SYNCTEX_VISIBLE_DISTANCE_h(noxy, width>0?h-width:h);
}
static float __synctex_kern_visible_width(synctex_noxy_p noxy) {
    int width = _synctex_data_width(noxy);
    return SYNCTEX_VISIBLE_SIZE(noxy, width>0?width:-width);
}
static float __synctex_rule_visible_h(synctex_noxy_p noxy) {
    int h = _synctex_data_h(noxy);
    int width = _synctex_data_width(noxy);
    return SYNCTEX_VISIBLE_DISTANCE_h(noxy, width>0?h:h-width);
}
static float __synctex_rule_visible_width(synctex_noxy_p noxy) {
    int width = _synctex_data_width(noxy);
    return SYNCTEX_VISIBLE_SIZE(noxy, width>0?width:-width);
}
static float __synctex_rule_visible_v(synctex_noxy_p noxy) {
    return __synctex_node_visible_v(noxy);
}
static float __synctex_rule_visible_height(synctex_noxy_p noxy) {
    return __synctex_node_visible_height(noxy);
}
static float __synctex_rule_visible_depth(synctex_noxy_p noxy) {
    return __synctex_node_visible_depth(noxy);
}

/**
 *  The horizontal location of node, in page coordinates.
 *  - parameter node: a node.
 *  - returns: a float.
 *  - author: JL
 */
float synctex_node_visible_h(synctex_node_p node){
    return node? node->class_->vispector->h(node): 0;
}
/**
 *  The vertical location of node, in page coordinates.
 *  - parameter node: a node.
 *  - returns: a float.
 *  - author: JL
 */
float synctex_node_visible_v(synctex_node_p node){
    return node? node->class_->vispector->v(node): 0;
}
/**
 *  The width of node, in page coordinates.
 *  - parameter node: a node.
 *  - returns: a float.
 *  - author: JL
 */
float synctex_node_visible_width(synctex_node_p node){
    return node? node->class_->vispector->width(node): 0;
}
/**
 *  The height of node, in page coordinates.
 *  - parameter node: a node.
 *  - returns: a float.
 *  - author: JL
 */
float synctex_node_visible_height(synctex_node_p node){
    return node? node->class_->vispector->height(node): 0;
}
/**
 *  The depth of node, in page coordinates.
 *  - parameter node: a node.
 *  - returns: a float.
 *  - author: JL
 */
float synctex_node_visible_depth(synctex_node_p node){
    return node? node->class_->vispector->depth(node): 0;
}

/**
 *  The V variant of geometrical information.
 *  - parameter node: a node.
 *  - returns: an integer.
 *  - author: JL
 */
#define SYNCTEX_DEFINE_V(WHAT)\
SYNCTEX_INLINE static int _synctex_node_##WHAT##_V(synctex_node_p node) { \
    synctex_node_p target = _synctex_tree_target(node); \
    if (target) { \
        return _synctex_data_##WHAT(node)+_synctex_node_##WHAT##_V(target); \
    } else if (_synctex_data_has_##WHAT##_V(node)) { \
        return _synctex_data_##WHAT##_V(node); \
    } else { \
        return _synctex_data_##WHAT(node); \
    } \
}
SYNCTEX_DEFINE_V(h)
SYNCTEX_DEFINE_V(v)
SYNCTEX_DEFINE_V(width)
SYNCTEX_DEFINE_V(height)
SYNCTEX_DEFINE_V(depth)

SYNCTEX_INLINE static synctex_point_s _synctex_data_point(synctex_node_p node) {
    return (synctex_point_s){synctex_node_h(node),synctex_node_v(node)};
}
SYNCTEX_INLINE static synctex_point_s _synctex_data_point_V(synctex_node_p node) {
    return (synctex_point_s){_synctex_node_h_V(node),_synctex_node_v_V(node)};
}
SYNCTEX_INLINE static synctex_point_s _synctex_data_set_point(synctex_node_p node, synctex_point_s point) {
    synctex_point_s old = _synctex_data_point(node);
    _synctex_data_set_h(node,point.h);
    _synctex_data_set_v(node,point.v);
    return old;
}
SYNCTEX_INLINE static synctex_box_s _synctex_data_box(synctex_node_p node) {
    synctex_box_s box = {{0,0},{0,0}};
    int n;
    n = synctex_node_width(node);
    if (n<0) {
        box.max.h = synctex_node_h(node);
        box.min.h = box.max.h + n;
    } else {
        box.min.h = synctex_node_h(node);
        box.max.h = box.min.h + n;
    }
    n = synctex_node_v(node);
    box.min.v = n - synctex_node_height(node);
    box.max.v = n + synctex_node_depth(node);
    return box;
}
SYNCTEX_INLINE static synctex_box_s _synctex_data_xob(synctex_node_p node) {
    synctex_box_s box = {{0,0},{0,0}};
    int n;
    n = synctex_node_width(node);
    if (n>0) {
        box.max.h = synctex_node_h(node);
        box.min.h = box.max.h - n;
    } else {
        box.min.h = synctex_node_h(node);
        box.max.h = box.min.h - n;
    }
    n = synctex_node_v(node);
    box.min.v = n - synctex_node_height(node);
    box.max.v = n + synctex_node_depth(node);
    return box;
}
SYNCTEX_INLINE static synctex_box_s _synctex_data_box_V(synctex_node_p node) {
    synctex_box_s box = {{0,0},{0,0}};
    int n;
    n = _synctex_node_width_V(node);
    if (n<0) {
        box.max.h = _synctex_node_h_V(node);
        box.min.h = box.max.h + n;
    } else {
        box.min.h = _synctex_node_h_V(node);
        box.max.h = box.min.h + n;
    }
    n = _synctex_node_v_V(node);
    box.min.v = n - _synctex_node_height_V(node);
    box.max.v = n + _synctex_node_depth_V(node);
    return box;
}

/**
 *  The higher box node in the parent hierarchy which
 *  mean line number is the one of node ±1.
 *  This enclosing box is computed as follows
 *  1) get the first hbox in the parent linked list
 *  starting at node.
 *  If there is none, simply return the parent of node.
 *  2) compute the mean line number
 *  3) scans up the tree for the higher hbox with
 *  the same mean line number, ±1 eventually
*  - parameter node: a node.
 *  - returns: a (proxy to a) box node.
 *  - author: JL
 */
static synctex_node_p _synctex_node_box_visible(synctex_node_p node) {
    if ((node = _synctex_node_or_handle_target(node))) {
        int mean = 0;
        int bound = 1500000/(node->class_->scanner->pre_magnification/1000.0);
        synctex_node_p parent = NULL;
        /*  get the first enclosing parent
         *  then get the highest enclosing parent with the same mean line ±1 */
        node = _synctex_node_or_handle_target(node);
        if (!_synctex_node_is_box(node)) {
            if ((parent = _synctex_tree_parent(node))) {
                node = parent;
            } else if ((node = _synctex_tree_target(node))) {
                if (!_synctex_node_is_box(node)) {
                    if ((parent = _synctex_tree_parent(node))) {
                        node = parent;
                    } else {
                        return NULL;
                    }
                }
            }
        }
        parent = node;
        mean = synctex_node_mean_line(node);
        while ((parent = _synctex_tree_parent(parent))) {
            if (_synctex_node_is_hbox(parent)) {
                if (_synctex_abs(mean-synctex_node_mean_line(parent))>1) {
                    return node;
                } else if (synctex_node_width(parent)>bound) {
                    return parent;
                } else if (synctex_node_height(parent)+synctex_node_depth(parent)>bound) {
                    return parent;
                }
                node = parent;
            }
        }
    }
    return node;
}
/**
 *  The horizontal location of the first box enclosing node, in page coordinates.
 *  - parameter node: a node.
 *  - returns: a float.
 *  - author: JL
 */
float synctex_node_box_visible_h(synctex_node_p node) {
    return SYNCTEX_VISIBLE_DISTANCE_h(node,_synctex_node_h_V(_synctex_node_box_visible(node)));
}
/**
 *  The vertical location of the first box enclosing node, in page coordinates.
 *  - parameter node: a node.
 *  - returns: a float.
 *  - author: JL
 */
float synctex_node_box_visible_v(synctex_node_p node) {
    return SYNCTEX_VISIBLE_DISTANCE_v(node,_synctex_node_v_V(_synctex_node_box_visible(node)));
}
/**
 *  The width of the first box enclosing node, in page coordinates.
 *  - parameter node: a node.
 *  - returns: a float.
 *  - author: JL
 */
float synctex_node_box_visible_width(synctex_node_p node) {
    return SYNCTEX_VISIBLE_SIZE(node,_synctex_node_width_V(_synctex_node_box_visible(node)));
}
/**
 *  The height of the first box enclosing node, in page coordinates.
 *  - parameter node: a node.
 *  - returns: a float.
 *  - author: JL
 */
float synctex_node_box_visible_height(synctex_node_p node) {
    return SYNCTEX_VISIBLE_SIZE(node,_synctex_node_height_V(_synctex_node_box_visible(node)));
}
/**
 *  The depth of the first box enclosing node, in page coordinates.
 *  - parameter node: a node.
 *  - returns: a float.
 *  - author: JL
 */
float synctex_node_box_visible_depth(synctex_node_p node) {
    return SYNCTEX_VISIBLE_SIZE(node,_synctex_node_depth_V(_synctex_node_box_visible(node)));
}
#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark Other public node attributes
#   endif

/**
 *  The page number of the sheet enclosing node.
 *  - parameter node: a node.
 *  - returns: the page number or -1 if node does not belong to a sheet tree.
 *  - note: a proxy target does not belong to a sheet
 *      but a form, its page number is always -1.
 *  - note: a handles does not belong to a sheet not a form.
 *      its page number is -1.
 *  - author: JL
 */
int synctex_node_page(synctex_node_p node){
    synctex_node_p parent = NULL;
    while((parent = _synctex_tree_parent(node))) {
        node = parent;
    }
    if (synctex_node_type(node) == synctex_node_type_sheet) {
        return _synctex_data_page(node);
    }
    return -1;
}
/**
 *  The page number of the target.
 *  - author: JL
 */
SYNCTEX_INLINE static int _synctex_node_target_page(synctex_node_p node){
    return synctex_node_page(_synctex_tree_target(node));
}

#if defined (SYNCTEX_USE_CHARINDEX)
synctex_charindex_t synctex_node_charindex(synctex_node_p node) {
    synctex_node_p target = _synctex_tree_target(node);
    return target? SYNCTEX_CHARINDEX(target):(node?SYNCTEX_CHARINDEX(node):0);
}
#endif

/**
 *  The tag of the node.
 *  - parameter node: a node.
 *  - returns: the tag or -1 if node is NULL.
 *  - author: JL
 */
int synctex_node_tag(synctex_node_p node) {
    return node? node->class_->tlcpector->tag(node): -1;
}
/**
 *  The line of the node.
 *  - parameter node: a node.
 *  - returns: the line or -1 if node is NULL.
 *  - author: JL
 */
int synctex_node_line(synctex_node_p node) {
    return node? node->class_->tlcpector->line(node): -1;
}
/**
 *  The column of the node.
 *  - parameter node: a node.
 *  - returns: the column or -1 if node is NULL.
 *  - author: JL
 */
int synctex_node_column(synctex_node_p node) {
    return node? node->class_->tlcpector->column(node): -1;
}
/**
 *  The mean line number of the node.
 *  - parameter node: a node.
 *  - returns: the mean line or -1 if node is NULL.
 *  - author: JL
 */
int synctex_node_mean_line(synctex_node_p node) {
    synctex_node_p other = _synctex_tree_target(node);
    if (other) {
        node = other;
    }
    if (_synctex_data_has_mean_line(node)) {
        return _synctex_data_mean_line(node);
    }
    if ((other = synctex_node_parent(node))) {
        if (_synctex_data_has_mean_line(other)) {
            return _synctex_data_mean_line(other);
        }
    }
    return synctex_node_line(node);
}
/**
 *  The weight of the node.
 *  - parameter node: a node.
 *  - returns: the weight or -1 if node is NULL.
 *  - author: JL
 */
int synctex_node_weight(synctex_node_p node) {
    synctex_node_p target = _synctex_tree_target(node);
    if (target) {
        node = target;
    }
    return node?(synctex_node_type(node)==synctex_node_type_hbox?_synctex_data_weight(node):0):-1;
}
/**
 *  The number of children of the node.
 *  - parameter node: a node.
 *  - returns: the count or -1 if node is NULL.
 *  - author: JL
 */
int synctex_node_child_count(synctex_node_p node) {
    synctex_node_p target = _synctex_tree_target(node);
    if (target) {
        node = target;
    }
    return node?(synctex_node_type(node)==synctex_node_type_hbox?_synctex_data_weight(node):0):-1;
}
#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark Sheet & Form
#   endif

/**
 *  The sheet of the scanner with a given page number.
 *  - parameter scanner: a scanner.
 *  - parameter page: a 1 based page number.
 *      If page == 0, returns the first sheet.
 *  - returns: a sheet or NULL.
 *  - author: JL
 */
synctex_node_p synctex_sheet(synctex_scanner_p scanner,int page) {
    if (scanner) {
        synctex_node_p sheet = scanner->sheet;
        while(sheet) {
            if (page == _synctex_data_page(sheet)) {
                return sheet;
            }
            sheet = __synctex_tree_sibling(sheet);
        }
        if (page == 0) {
            return scanner->sheet;
        }
    }
    return NULL;
}
/**
 *  The form of the scanner with a given tag.
 *  - parameter scanner: a scanner.
 *  - parameter tag: an integer identifier.
 *      If tag == 0, returns the first form.
 *  - returns: a form.
 *  - author: JL
 */
synctex_node_p synctex_form(synctex_scanner_p scanner,int tag) {
    if (scanner) {
        synctex_node_p form = scanner->form;
        while(form) {
            if (tag == _synctex_data_tag(form)) {
                return form;
            }
            form = __synctex_tree_sibling(form);
        }
        if (tag == 0) {
            return scanner->form;
        }
    }
    return NULL;
}

/**
 *  The content of the sheet with given page number.
 *  - parameter scanner: a scanner.
 *  - parameter page: a 1 based page number.
 *  - returns: a (vertical) box node.
 *  - author: JL
 */
synctex_node_p synctex_sheet_content(synctex_scanner_p scanner,int page) {
    if (scanner) {
        return _synctex_tree_child(synctex_sheet(scanner,page));
    }
    return NULL;
}

/**
 *  The content of the sheet with given page number.
 *  - parameter scanner: a scanner.
 *  - parameter tag: an integer identifier.
 *  - returns: a box node.
 *  - author: JL
 */
synctex_node_p synctex_form_content(synctex_scanner_p scanner,int tag) {
    if (scanner) {
        return _synctex_tree_child(synctex_form(scanner,tag));
    }
    return NULL;
}

SYNCTEX_INLINE static synctex_node_p _synctex_scanner_friend(synctex_scanner_p scanner,int i) {
    if (i>=0) {
        i = _synctex_abs(i)%(scanner->number_of_lists);
        return (scanner->lists_of_friends)[i];
    }
    return NULL;
}
SYNCTEX_INLINE static synctex_bool_t _synctex_nodes_are_friend(synctex_node_p left, synctex_node_p right) {
    return synctex_node_tag(left) == synctex_node_tag(right) && synctex_node_line(left) == synctex_node_line(right);
}
/**
 *  The sibling argument is a parent/child list of nodes of the same page.
 */
typedef struct {
    int count;
    synctex_node_p node;
} synctex_counted_node_s;

SYNCTEX_INLINE static synctex_counted_node_s _synctex_vertically_sorted_v2(synctex_node_p sibling) {
    /* Clean the weights of the parents */
    synctex_counted_node_s result = {0, NULL};
    synctex_node_p h = NULL;
    synctex_node_p next_h = NULL;
    synctex_node_p parent = NULL;
    int weight = 0;
    synctex_node_p N = NULL;
    h = sibling;
    do {
        N = _synctex_tree_target(h);
        parent = _synctex_tree_parent(N);
        _synctex_data_set_weight(parent, 0);
    } while((h = _synctex_tree_child(h)));
    /* Compute the weights of the nodes */
    h = sibling;
    do {
        N = _synctex_tree_target(h);
        parent = _synctex_tree_parent(N);
        weight = _synctex_data_weight(parent);
        if (weight==0) {
            N = _synctex_tree_child(parent);
            do {
                if (_synctex_nodes_are_friend(N,sibling)) {
                    ++ weight;
                }
            } while ((N = __synctex_tree_sibling(N)));
            _synctex_data_set_weight(h,weight);
            _synctex_data_set_weight(parent,weight);
        }
    } while((h = _synctex_tree_child(h)));
    /* Order handle nodes according to the weight */
    h = _synctex_tree_reset_child(sibling);
    result.node = sibling;
    weight = 0;
    while((h)) {
        N = result.node;
        if (_synctex_data_weight(h)>_synctex_data_weight(N)) {
            next_h = _synctex_tree_set_child(h,N);
            result.node = h;
        } else if (_synctex_data_weight(h) == 0) {
            ++ weight;
            next_h = _synctex_tree_reset_child(h);
            synctex_node_free(h);
        } else {
            synctex_node_p next_N = NULL;
            while((next_N = _synctex_tree_child(N))) {
                N = next_N;
                if (_synctex_data_weight(h)<_synctex_data_weight(next_N)) {
                    continue;
                }
                break;
            }
            next_h = _synctex_tree_set_child(h,_synctex_tree_set_child(N,h));
        }
        h = next_h;
    };
    h = result.node;
    weight = 0;
    do {
        ++weight;
    } while((h = _synctex_tree_child(h)));
    result.count = 1;
    h = result.node;
    while((next_h = _synctex_tree_child(h))) {
        if (_synctex_data_weight(next_h)==0) {
            _synctex_tree_reset_child(h);
            weight = 1;
            h = next_h;
            while((h = _synctex_tree_child(h))) {
                ++weight;
            }
            synctex_node_free(next_h);
            break;
        }
        ++result.count;
        h = next_h;
    }
    return result;
}

SYNCTEX_INLINE static synctex_bool_t _synctex_point_in_box_v2(synctex_point_p hitP, synctex_node_p node);

/*  This struct records distances, the left one is non negative and the right one is non positive.
 *  When comparing the locations of 2 different graphical objects on the page, we will have to also record the
 *  horizontal distance as signed to keep track of the typesetting order.*/

typedef struct {
    synctex_node_p node;
    int distance;
} synctex_nd_s;

#define SYNCTEX_ND_0 (synctex_nd_s){NULL,INT_MAX}

typedef synctex_nd_s * synctex_nd_p;

typedef struct {
    synctex_nd_s l;
    synctex_nd_s r;
} synctex_nd_lr_s;

/*  The best container is the deeper box that contains the hit point (H,V).
 *  _synctex_eq_deepest_container_v2 starts with node whereas
 *  _synctex_box_child_deepest starts with node's children, if any
 *  if node is not a box, or a void box, NULL is returned.
 *  We traverse the node tree in a deep first manner and stop as soon as a result is found. */
static synctex_node_p _synctex_eq_deepest_container_v2(synctex_point_p hitP, synctex_node_p node);

SYNCTEX_INLINE static synctex_nd_lr_s _synctex_eq_get_closest_children_in_box_v2(synctex_point_p hitP, synctex_node_p node);

/*  Closest child, recursive.  */
static synctex_nd_s __synctex_closest_deep_child_v2(synctex_point_p hitP, synctex_node_p node);

/*  The smallest container between two has the smallest width or height.
 *  This comparison is used when there are 2 overlapping boxes that contain the hit point.
 *  For ConTeXt, the problem appears at each page.
 *  The chosen box is the one with the smallest height, then the smallest width. */
SYNCTEX_INLINE static synctex_node_p _synctex_smallest_container_v2(synctex_node_p node, synctex_node_p other_node);

/*  Returns the distance between the hit point hit point=(H,V) and the given node. */

static int _synctex_point_node_distance_v2(synctex_point_p hitP, synctex_node_p node);

/*  The closest container is the box that is the one closest to the given point.
 *  The "visible" version takes into account the visible dimensions instead of the real ones given by TeX. */
static synctex_nd_s _synctex_eq_closest_child_v2(synctex_point_p hitP, synctex_node_p node);

#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark Queries
#   endif

/**
 *  iterator for a deep first tree traversal.
 */
struct synctex_iterator_t {
    synctex_node_p seed;
    synctex_node_p top;
    synctex_node_p next;
    int count0;
    int count;
};

SYNCTEX_INLINE static synctex_iterator_p _synctex_iterator_new(synctex_node_p result, int count) {
    synctex_iterator_p iterator;
    if ((iterator = _synctex_malloc(sizeof(synctex_iterator_s)))) {
        iterator->seed = iterator->top = iterator->next = result;
        iterator->count0 = iterator->count = count;
    }
    return iterator;
}

void synctex_iterator_free(synctex_iterator_p iterator) {
    if (iterator) {
        synctex_node_free(iterator->seed);
        _synctex_free(iterator);
    }
}
synctex_bool_t synctex_iterator_has_next(synctex_iterator_p iterator) {
    return iterator?iterator->count>0:0;
}
int synctex_iterator_count(synctex_iterator_p iterator) {
    return iterator? iterator->count: 0;
}

/**
 *  The next result of the iterator.
 *  Internally, the iterator stores handles to nodes.
 *  Externally, it returns the targets,
 *  such that the caller only sees nodes.
 */
synctex_node_p synctex_iterator_next_result(synctex_iterator_p iterator) {
    if (iterator && iterator->count>0) {
        synctex_node_p N = iterator->next;
        if(!(iterator->next = _synctex_tree_child(N))) {
            iterator->next = iterator->top = __synctex_tree_sibling(iterator->top);
        }
        --iterator->count;
        return _synctex_tree_target(N);
    }
    return NULL;
}
int synctex_iterator_reset(synctex_iterator_p iterator) {
    if (iterator) {
        iterator->next = iterator->top = iterator->seed;
        return iterator->count = iterator->count0;
    }
    return 0;
}

synctex_iterator_p synctex_iterator_new_edit(synctex_scanner_p scanner,int page,float h,float v){
    if (scanner) {
        synctex_node_p sheet = NULL;
        synctex_point_s hit;
        synctex_node_p node = NULL;
        synctex_nd_lr_s nds = {{NULL,0},{NULL,0}};
        if (NULL == (scanner = synctex_scanner_parse(scanner)) || 0 >= scanner->unit) {/*  scanner->unit must be >0 */
            return NULL;
        }
        /*  Find the proper sheet */
        sheet = synctex_sheet(scanner,page);
        if (NULL == sheet) {
            return NULL;
        }
        /*  Now sheet points to the sheet node with proper page number. */
        /*  Now that scanner has been initialized, we can convert
         *  the given point to scanner integer coordinates */
        hit = (synctex_point_s)
        {(h-scanner->x_offset)/scanner->unit,
            (v-scanner->y_offset)/scanner->unit};
        /*  At first, we browse all the horizontal boxes of the sheet
         *  until we find one containing the hit point. */
        if ((node = _synctex_tree_next_hbox(sheet))) {
            do {
                if (_synctex_point_in_box_v2(&hit,node)) {
                    /*  Maybe the hit point belongs to a contained vertical box.
                     *  This is the most likely situation.
                     */
                    synctex_node_p next = node;
#if defined(SYNCTEX_DEBUG)
                    printf("--- We are lucky\n");
#endif
                    /*  This trick is for catching overlapping boxes */
                    while ((next = _synctex_tree_next_hbox(next))) {
                        if (_synctex_point_in_box_v2(&hit,next)) {
                            node = _synctex_smallest_container_v2(next,node);
                        }
                    }
                    /*  node is the smallest horizontal box that contains hit,
                     *  unless there is no hbox at all.
                     */
                    node = _synctex_eq_deepest_container_v2(&hit, node);
                    nds = _synctex_eq_get_closest_children_in_box_v2(&hit, node);
                end:
                    if (nds.r.node && nds.l.node) {
                        if ((_synctex_data_tag(nds.r.node)!=_synctex_data_tag(nds.l.node))
                            || (_synctex_data_line(nds.r.node)!=_synctex_data_line(nds.l.node))
                            || (_synctex_data_column(nds.r.node)!=_synctex_data_column(nds.l.node))) {
                            if (_synctex_data_line(nds.r.node)<_synctex_data_line(nds.l.node)) {
                                node = nds.r.node;
                                nds.r.node = nds.l.node;
                                nds.l.node = node;
                            } else if (_synctex_data_line(nds.r.node)==_synctex_data_line(nds.l.node)) {
                                if (nds.l.distance>nds.r.distance) {
                                    node = nds.r.node;
                                    nds.r.node = nds.l.node;
                                    nds.l.node = node;
                                }
                            }
                            if((node = _synctex_new_handle_with_target(nds.l.node))) {
                                synctex_node_p other_handle;
                                if((other_handle = _synctex_new_handle_with_target(nds.r.node))) {
                                    _synctex_tree_set_sibling(node,other_handle);
                                    return _synctex_iterator_new(node,2);
                                }
                                return _synctex_iterator_new(node,1);
                            }
                            return NULL;
                        }
                        /*  both nodes have the same input coordinates
                         *  We choose the one closest to the hit point  */
                        if (nds.l.distance>nds.r.distance) {
                            nds.l.node = nds.r.node;
                        }
                        nds.r.node = NULL;
                    } else if (nds.r.node) {
                        nds.l = nds.r;
                    } else if (!nds.l.node) {
                        nds.l.node = node;
                    }
                    if((node = _synctex_new_handle_with_target(nds.l.node))) {
                        return _synctex_iterator_new(node,1);
                    }
                    return 0;
                }
            } while ((node = _synctex_tree_next_hbox(node)));
            /*  All the horizontal boxes have been tested,
             *  None of them contains the hit point.
             */
        }
        /*  We are not lucky,
         *  we test absolutely all the node
         *  to find the closest... */
        if ((node = _synctex_tree_child(sheet))) {
#if defined(SYNCTEX_DEBUG)
            printf("--- We are not lucky\n");
#endif
            nds.l = __synctex_closest_deep_child_v2(&hit, node);
#if defined(SYNCTEX_DEBUG)
            printf("Edit query best: %i\n", nds.l.distance);
#endif
            goto end;
        }
    }
    return NULL;
}

/**
 *  Loop the candidate friendly list to find the ones with the proper
 *  tag and line.
 *  Returns a tree of results targeting the found candidates.
 *  At the top level each sibling has its own page number.
 *  All the results with the same page number are linked by child/parent entry.
 *  - parameter candidate: a friendly list of candidates
 */
static synctex_node_p _synctex_display_query_v2(synctex_node_p target, int tag, int line, synctex_bool_t exclude_box) {
    synctex_node_p first_handle = NULL;
    /*  Search the first match */
    if (target == NULL) {
        return first_handle;
    }
    do {
        int page;
        if ((exclude_box
             && _synctex_node_is_box(target))
            || (tag != synctex_node_tag(target))
            || (line != synctex_node_line(target))) {
            continue;
        }
        /*  We found a first match, create
         *  a result handle targeting that candidate. */
        first_handle = _synctex_new_handle_with_target(target);
        if (first_handle == NULL) {
            return first_handle;
        }
        /*  target is either a node,
         *  or a proxy to some node, in which case,
         *  the target's target belongs to a form,
         *  not a sheet. */
        page = synctex_node_page(target);
        /*  Now create all the other results  */
        while ((target = _synctex_tree_friend(target))) {
            synctex_node_p result = NULL;
            if ((exclude_box
                 && _synctex_node_is_box(target))
                || (tag != synctex_node_tag(target))
                || (line != synctex_node_line(target))) {
                continue;
            }
            /*  Another match, same page number ? */
            result = _synctex_new_handle_with_target(target);
            if (NULL == result ) {
                return first_handle;
            }
            /*  is it the same page number ? */
            if (synctex_node_page(target) == page) {
                __synctex_tree_set_child(result, first_handle);
                first_handle = result;
            } else {
                /*  We have 2 page numbers involved */
                __synctex_tree_set_sibling(first_handle, result);
                while ((target = _synctex_tree_friend(target))) {
                    synctex_node_p same_page_node;
                    if ((exclude_box
                         && _synctex_node_is_box(target))
                        || (tag != synctex_node_tag(target))
                        || (line != synctex_node_line(target))) {
                        continue;
                    }
                    /*  New match found, which page? */
                    result = _synctex_new_handle_with_target(target);
                    if (NULL == result) {
                        return first_handle;
                    }
                    same_page_node = first_handle;
                    page = synctex_node_page(target);
                    /*  Find a result with the same page number */;
                    do {
                        if (_synctex_node_target_page(same_page_node) == page) {
                            /* Insert result between same_page_node and its child */
                            _synctex_tree_set_child(result,_synctex_tree_set_child(same_page_node,result));
                        } else if ((same_page_node = __synctex_tree_sibling(same_page_node))) {
                            continue;
                        } else {
                            /*  This is a new page number */
                            __synctex_tree_set_sibling(result,first_handle);
                            first_handle = result;
                        }
                        break;
                    } while (synctex_YES);
                }
                return first_handle;
            }
        }
    } while ((target = _synctex_tree_friend(target)));
    return first_handle;
}
synctex_iterator_p synctex_iterator_new_display(synctex_scanner_p scanner,const char * name,int line,int column, int page_hint) {
    SYNCTEX_UNUSED(column)
    if (scanner) {
        int tag = synctex_scanner_get_tag(scanner,name);/* parse if necessary */
        int max_line = 0;
        int line_offset = 1;
        int try_count = 100;
        synctex_node_p node = NULL;
        synctex_node_p result = NULL;
        if (tag == 0) {
            printf("SyncTeX Warning: No tag for %s\n",name);
            return NULL;
        }
        node = synctex_scanner_input_with_tag(scanner, tag);
        max_line = _synctex_data_line(node);
        /*  node = NULL; */
        if (line>max_line) {
            line = max_line;
        }
        while(try_count--) {
            if (line<=max_line) {
                /*  This loop will only be performed once for advanced viewers */
                synctex_node_p friend = _synctex_scanner_friend(scanner,tag+line);
                if ((node = friend)) {
                    result = _synctex_display_query_v2(node,tag,line,synctex_YES);
                    if (!result) {
                        /*  We did not find any matching boundary, retry including boxes */
                        node = friend;/*  no need to test it again, already done */
                        result = _synctex_display_query_v2(node,tag,line,synctex_NO);
                    }
                    /*  Now reverse the order to have nodes in display order, and then keep just a few nodes.
                     *  Order first the best node. */
                    /*  The result is a tree. At the root level, all nodes
                     *  correspond to different page numbers.
                     *  Each node has a child which corresponds to the same
                     *  page number if relevant.
                     *  Then reorder the nodes to put first the one which fits best.
                     *  The idea is to count the number of nodes
                     *  with the same tag and line number in the parents
                     *  and choose the ones with the biggest count.
                     */
                    if (result) {
                        /*  navigate through siblings, then children   */
                        synctex_node_p next_sibling = __synctex_tree_reset_sibling(result);
                        int best_match = abs(page_hint-_synctex_node_target_page(result));
                        synctex_node_p sibling;
                        int match;
                        synctex_counted_node_s cn = _synctex_vertically_sorted_v2(result);
                        int count = cn.count;
                        result = cn.node;
                        while((sibling = next_sibling)) {
                            /* What is next? Do not miss that step! */
                            next_sibling = __synctex_tree_reset_sibling(sibling);
                            cn = _synctex_vertically_sorted_v2(sibling);
                            count += cn.count;
                            sibling = cn.node;
                            match = abs(page_hint-_synctex_node_target_page(sibling));
                            if (match<best_match) {
                                /*  Order this node first */
                                __synctex_tree_set_sibling(sibling,result);
                                result = sibling;
                                best_match = match;
                            } else /*if (match>=best_match)*/ {
                                __synctex_tree_set_sibling(sibling,__synctex_tree_sibling(result));
                                __synctex_tree_set_sibling(result,sibling);
                            }
                        }
                        return _synctex_iterator_new(result,count);
                    }
                }
#       if defined(__SYNCTEX_STRONG_DISPLAY_QUERY__)
                break;
#       else
                line += line_offset;
                line_offset=line_offset<0?-(line_offset-1):-(line_offset+1);
                if (line <= 0) {
                    line += line_offset;
                    line_offset=line_offset<0?-(line_offset-1):-(line_offset+1);
                }
#       endif
            }
        }
    }
    return NULL;
}
synctex_status_t synctex_display_query(synctex_scanner_p scanner,const char *  name,int line,int column, int page_hint) {
    if (scanner) {
        synctex_iterator_free(scanner->iterator);
        scanner->iterator = synctex_iterator_new_display(scanner, name,line,column, page_hint);
        return synctex_iterator_count(scanner->iterator);
    }
    return SYNCTEX_STATUS_ERROR;
}
synctex_status_t synctex_edit_query(synctex_scanner_p scanner,int page,float h,float v) {
    if (scanner) {
        synctex_iterator_free(scanner->iterator);
        scanner->iterator = synctex_iterator_new_edit(scanner, page, h, v);
        return synctex_iterator_count(scanner->iterator);
    }
    return SYNCTEX_STATUS_ERROR;
}
/**
 *  The next result of a query.
 */
synctex_node_p synctex_scanner_next_result(synctex_scanner_p scanner) {
    return scanner? synctex_iterator_next_result(scanner->iterator): NULL;
}
synctex_status_t synctex_scanner_reset_result(synctex_scanner_p scanner) {
    return scanner? synctex_iterator_reset(scanner->iterator): SYNCTEX_STATUS_ERROR;
}

synctex_node_p synctex_node_target(synctex_node_p node) {
    return _synctex_tree_target(node);
}

#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark Geometric utilities
#   endif

/** Rougly speaking, this is:
 *  node's h coordinate - hit point's h coordinate.
 *  If node is to the right of the hit point, then this distance is positive,
 *  if node is to the left of the hit point, this distance is negative.
 *  If the argument is a pdf form reference, then the child is used and returned instead.
 *  Last Revision: Mon Apr 24 07:05:27 UTC 2017
 */
static synctex_nd_s _synctex_point_h_ordered_distance_v2
(synctex_point_p hit, synctex_node_p node) {
    synctex_nd_s nd = {node,INT_MAX};
    if (node) {
        int min,med,max,width;
        switch(synctex_node_type(node)) {
                /*  The distance between a point and a box is special.
                 *  It is not the euclidian distance, nor something similar.
                 *  We have to take into account the particular layout,
                 *  and the box hierarchy.
                 *  Given a box, there are 9 regions delimited by the lines of the edges of the box.
                 *  The origin being at the top left corner of the page,
                 *  we also give names to the vertices of the box.
                 *
                 *   1 | 2 | 3
                 *  ---A---B--->
                 *   4 | 5 | 6
                 *  ---C---D--->
                 *   7 | 8 | 9
                 *     v   v
                 */
            case synctex_node_type_vbox:
            case synctex_node_type_void_vbox:
            case synctex_node_type_void_hbox:
                /*  getting the box bounds, taking into account negative width, height and depth. */
                width = _synctex_data_width(node);
                min = _synctex_data_h(node);
                max = min + (width>0?width:-width);
                /*  We always have min <= max */
                if (hit->h<min) {
                    nd.distance = min - hit->h; /*  regions 1+4+7, result is > 0 */
                } else if (hit->h>max) {
                    nd.distance = max - hit->h; /*  regions 3+6+9, result is < 0 */
                } else {
                    nd.distance = 0; /*  regions 2+5+8, inside the box, except for vertical coordinates */
                }
                break;
            case synctex_node_type_proxy_vbox:
                /*  getting the box bounds, taking into account negative width, height and depth. */
                width = synctex_node_width(node);
                min = synctex_node_h(node);
                max = min + (width>0?width:-width);
                /*  We always have min <= max */
                if (hit->h<min) {
                    nd.distance = min - hit->h; /*  regions 1+4+7, result is > 0 */
                } else if (hit->h>max) {
                    nd.distance = max - hit->h; /*  regions 3+6+9, result is < 0 */
                } else {
                    nd.distance = 0; /*  regions 2+5+8, inside the box, except for vertical coordinates */
                }
                break;
            case synctex_node_type_hbox:
            case synctex_node_type_proxy_hbox:
                /*  getting the box bounds, taking into account negative width, height and depth. */
                width = synctex_node_hbox_width(node);
                min = synctex_node_hbox_h(node);
                max = min + (width>0?width:-width);
                /*  We always have min <= max */
                if (hit->h<min) {
                    nd.distance = min - hit->h; /*  regions 1+4+7, result is > 0 */
                } else if (hit->h>max) {
                    nd.distance = max - hit->h; /*  regions 3+6+9, result is < 0 */
                } else {
                    nd.distance = 0; /*  regions 2+5+8, inside the box, except for vertical coordinates */
                }
                break;
            case synctex_node_type_kern:
                /*  IMPORTANT NOTICE: the location of the kern is recorded AFTER the move.
                 *  The distance to the kern is very special,
                 *  in general, there is no text material in the kern,
                 *  this is why we compute the offset relative to the closest edge of the kern.*/
                max = _synctex_data_width(node);
                if (max<0) {
                    min = _synctex_data_h(node);
                    max = min - max;
                } else {
                    min = -max;
                    max = _synctex_data_h(node);
                    min += max;
                }
                med = (min+max)/2;
                /*  positive kern: '.' means text, '>' means kern offset
                 *      .............
                 *                   min>>>>med>>>>max
                 *                                    ...............
                 *  negative kern: '.' means text, '<' means kern offset
                 *      ............................
                 *                 min<<<<med<<<<max
                 *                 .................................
                 *  Actually, we do not take into account negative widths.
                 *  There is a problem for such situation when there is effectively overlapping text.
                 *  But this should be extremely rare. I guess that in that case, many different choices
                 *  could be made, one being in contradiction with the other.
                 *  It means that the best choice should be made according to the situation that occurs
                 *  most frequently.
                 */
                if (hit->h<min) {
                    nd.distance = min - hit->h + 1; /*  penalty to ensure other nodes are chosen first in case of overlapping ones */
                } else if (hit->h>max) {
                    nd.distance = max - hit->h - 1; /*  same kind of penalty */
                } else if (hit->h>med) {
                    /*  do things like if the node had 0 width and was placed at the max edge + 1*/
                    nd.distance = max - hit->h + 1; /*  positive, the kern is to the right of the hit point */
                } else {
                    nd.distance = min - hit->h - 1; /*  negative, the kern is to the left of the hit point */
                }
                break;
            case synctex_node_type_rule:/* to do: special management */
            case synctex_node_type_glue:
            case synctex_node_type_math:
            case synctex_node_type_boundary:
            case synctex_node_type_box_bdry:
                nd.distance = _synctex_data_h(node) - hit->h;
                break;
            case synctex_node_type_ref:
                nd.node = synctex_node_child(node);
                nd = _synctex_point_h_ordered_distance_v2(hit,nd.node);
                break;
            case synctex_node_type_proxy:
            case synctex_node_type_proxy_last:
            {
                /* shift the hit point to be relative to the proxy origin,
                 *  then compute the distance to the target
                 */
                synctex_point_s otherHit = *hit;
                otherHit.h -= _synctex_data_h(node);
                otherHit.v -= _synctex_data_v(node);
                nd.node = _synctex_tree_target(node);
                nd = _synctex_point_h_ordered_distance_v2(&otherHit,nd.node);
                nd.node = node;
            }
            default:
                break;
        }
    }
    return nd;
}
/** Rougly speaking, this is:
 *  node's v coordinate - hit point's v coordinate.
 *  If node is at the top of the hit point, then this distance is positive,
 *  if node is at the bottom of the hit point, this distance is negative.
 */
static synctex_nd_s _synctex_point_v_ordered_distance_v2
(synctex_point_p hit, synctex_node_p node) {
    synctex_nd_s nd = {node, INT_MAX};
    int min,max,depth,height;
    switch(synctex_node_type(node)) {
            /*  The distance between a point and a box is special.
             *  It is not the euclidian distance, nor something similar.
             *  We have to take into account the particular layout,
             *  and the box hierarchy.
             *  Given a box, there are 9 regions delimited by the lines of the edges of the box.
             *  The origin being at the top left corner of the page,
             *  we also give names to the vertices of the box.
             *
             *   1 | 2 | 3
             *  ---A---B--->
             *   4 | 5 | 6
             *  ---C---D--->
             *   7 | 8 | 9
             *     v   v
             */
        case synctex_node_type_vbox:
        case synctex_node_type_void_vbox:
        case synctex_node_type_void_hbox:
            /*  getting the box bounds, taking into account negative width, height and depth. */
            min = synctex_node_v(node);
            max = min + _synctex_abs(_synctex_data_depth(node));
            min -= _synctex_abs(_synctex_data_height(node));
            /*  We always have min <= max */
            if (hit->v<min) {
                nd.distance = min - hit->v; /*  regions 1+2+3, result is > 0 */
            } else if (hit->v>max) {
                nd.distance = max - hit->v; /*  regions 7+8+9, result is < 0 */
            } else {
                nd.distance = 0; /*  regions 4.5.6, inside the box, except for horizontal coordinates */
            }
            break;
        case synctex_node_type_proxy_vbox:
            /*  getting the box bounds, taking into account negative width, height and depth. */
            min = synctex_node_v(node);
            max = min + _synctex_abs(synctex_node_depth(node));
            min -= _synctex_abs(synctex_node_height(node));
            /*  We always have min <= max */
            if (hit->v<min) {
                nd.distance = min - hit->v; /*  regions 1+2+3, result is > 0 */
            } else if (hit->v>max) {
                nd.distance = max - hit->v; /*  regions 7+8+9, result is < 0 */
            } else {
                nd.distance = 0; /*  regions 4.5.6, inside the box, except for horizontal coordinates */
            }
            break;
        case synctex_node_type_hbox:
        case synctex_node_type_proxy_hbox:
            /*  getting the box bounds, taking into account negative height and depth. */
            min = synctex_node_hbox_v(node);
            depth = synctex_node_hbox_depth(node);
            max = min + (depth>0?depth:-depth);
            height = synctex_node_hbox_height(node);
            min -= (height>0?height:-height);
            /*  We always have min <= max */
            if (hit->v<min) {
                nd.distance = min - hit->v; /*  regions 1+2+3, result is > 0 */
            } else if (hit->v>max) {
                nd.distance = max - hit->v; /*  regions 7+8+9, result is < 0 */
            } else {
                nd.distance = 0; /*  regions 4.5.6, inside the box, except for horizontal coordinates */
            }
            break;
        case synctex_node_type_rule:/* to do: special management */
        case synctex_node_type_kern:
        case synctex_node_type_glue:
        case synctex_node_type_math:
            min = _synctex_data_v(node);
            max = min + _synctex_abs(_synctex_data_depth(_synctex_tree_parent(node)));
            min -= _synctex_abs(_synctex_data_height(_synctex_tree_parent(node)));
            /*  We always have min <= max */
            if (hit->v<min) {
                nd.distance = min - hit->v; /*  regions 1+2+3, result is > 0 */
            } else if (hit->v>max) {
                nd.distance = max - hit->v; /*  regions 7+8+9, result is < 0 */
            } else {
                nd.distance = 0; /*  regions 4.5.6, inside the box, except for horizontal coordinates */
            }
            break;
        case synctex_node_type_ref:
            nd.node = synctex_node_child(node);
            nd = _synctex_point_v_ordered_distance_v2(hit,nd.node);
            break;
        case synctex_node_type_proxy:
        case synctex_node_type_proxy_last:
        {
            synctex_point_s otherHit = *hit;
            otherHit.h -= _synctex_data_h(node);
            otherHit.v -= _synctex_data_v(node);
            nd.node = _synctex_tree_target(node);
            nd = _synctex_point_v_ordered_distance_v2(&otherHit,nd.node);
            nd.node = node;
        }
        default: break;
    }
    return nd;
}
/**
 *  The best is the one with the smallest area.
 *  The area is width*height where width and height may be big.
 *  So there is a real risk of overflow if we stick with ints.
 */
SYNCTEX_INLINE static synctex_node_p _synctex_smallest_container_v2(synctex_node_p node, synctex_node_p other_node) {
    long total_height, other_total_height;
    unsigned long area, other_area;
    long width = synctex_node_hbox_width(node);
    long other_width = synctex_node_hbox_width(other_node);
    if (width<0) {
        width = -width;
    }
    if (other_width<0) {
        other_width = -other_width;
    }
    total_height = _synctex_abs(synctex_node_hbox_depth(node)) + _synctex_abs(synctex_node_hbox_height(node));
    other_total_height = _synctex_abs(synctex_node_hbox_depth(other_node)) + _synctex_abs(synctex_node_hbox_height(other_node));
    area = total_height*width;
    other_area = other_total_height*other_width;
    if (area<other_area) {
        return node;
    }
    if (area>other_area) {
        return other_node;
    }
    if (_synctex_abs(_synctex_data_width(node))>_synctex_abs(_synctex_data_width(other_node))) {
        return node;
    }
    if (_synctex_abs(_synctex_data_width(node))<_synctex_abs(_synctex_data_width(other_node))) {
        return other_node;
    }
    if (total_height<other_total_height) {
        return node;
    }
    if (total_height>other_total_height) {
        return other_node;
    }
    return node;
}

SYNCTEX_INLINE static synctex_bool_t _synctex_point_in_box_v2(synctex_point_p hit, synctex_node_p node) {
    if (node) {
        if (0 == _synctex_point_h_ordered_distance_v2(hit,node).distance
            && 0 == _synctex_point_v_ordered_distance_v2(hit,node).distance) {
            return synctex_YES;
        }
    }
    return synctex_NO;
}

static int _synctex_distance_to_box_v2(synctex_point_p hit,synctex_box_p box) {
    /*  The distance between a point and a box is special.
     *  It is not the euclidian distance, nor something similar.
     *  We have to take into account the particular layout,
     *  and the box hierarchy.
     *  Given a box, there are 9 regions delimited by the lines of the edges of the box.
     *  The origin being at the top left corner of the page,
     *  we also give names to the vertices of the box.
     *
     *   1 | 2 | 3
     *  ---A---B--->
     *   4 | 5 | 6
     *  ---C---D--->
     *   7 | 8 | 9
     *     v   v
     *  In each region, there is a different formula.
     *  In the end we have a continuous distance which may not be a mathematical distance but who cares. */
    if (hit->v<box->min.v) {
        /*  Regions 1, 2 or 3 */
        if (hit->h<box->min.h) {
            /*  This is region 1. The distance to the box is the L1 distance PA. */
            return box->min.v - hit->v + box->min.h - hit->h;/*  Integer overflow? probability epsilon */
        } else if (hit->h<=box->max.h) {
            /*  This is region 2. The distance to the box is the geometrical distance to the top edge.  */
            return box->min.v - hit->v;
        } else {
            /*  This is region 3. The distance to the box is the L1 distance PB. */
            return box->min.v - hit->v + hit->h - box->max.h;
        }
    } else if (hit->v<=box->max.v) {
        /*  Regions 4, 5 or 6 */
        if (hit->h<box->min.h) {
            /*  This is region 4. The distance to the box is the geometrical distance to the left edge.  */
            return box->min.h - hit->h;
        } else if (hit->h<=box->max.h) {
            /*  This is region 5. We are inside the box.  */
            return 0;
        } else {
            /*  This is region 6. The distance to the box is the geometrical distance to the right edge.  */
            return hit->h - box->max.h;
        }
    } else {
        /*  Regions 7, 8 or 9 */
        if (hit->h<box->min.h) {
            /*  This is region 7. The distance to the box is the L1 distance PC. */
            return hit->v - box->max.v + box->min.h - hit->h;
        } else if (hit->h<=box->max.h) {
            /*  This is region 8. The distance to the box is the geometrical distance to the top edge.  */
            return hit->v - box->max.v;
        } else {
            /*  This is region 9. The distance to the box is the L1 distance PD. */
            return hit->v - box->max.v + hit->h - box->max.h;
        }
    }
}

/**
 *  The distance from the hit point to the node.
 */
static int _synctex_point_node_distance_v2(synctex_point_p hit, synctex_node_p node) {
    int d = INT_MAX;
    if (node) {
        synctex_box_s box = {{0,0},{0,0}};
        int dd = INT_MAX;
        switch(synctex_node_type(node)) {
            case synctex_node_type_vbox:
                box.min.h = _synctex_data_h(node);
                box.max.h = box.min.h + _synctex_abs(_synctex_data_width(node));
                box.min.v = synctex_node_v(node);
                box.max.v = box.min.v + _synctex_abs(_synctex_data_depth(node));
                box.min.v -= _synctex_abs(_synctex_data_height(node));
                return _synctex_distance_to_box_v2(hit,&box);
            case synctex_node_type_proxy_vbox:
                box.min.h = synctex_node_h(node);
                box.max.h = box.min.h + _synctex_abs(synctex_node_width(node));
                box.min.v = synctex_node_v(node);
                box.max.v = box.min.v + _synctex_abs(synctex_node_depth(node));
                box.min.v -= _synctex_abs(synctex_node_height(node));
                return _synctex_distance_to_box_v2(hit,&box);
            case synctex_node_type_hbox:
            case synctex_node_type_proxy_hbox:
                box.min.h = synctex_node_hbox_h(node);
                box.max.h = box.min.h + _synctex_abs(synctex_node_hbox_width(node));
                box.min.v = synctex_node_hbox_v(node);
                box.max.v = box.min.v + _synctex_abs(synctex_node_hbox_depth(node));
                box.min.v -= _synctex_abs(synctex_node_hbox_height(node));
                return _synctex_distance_to_box_v2(hit,&box);
            case synctex_node_type_void_vbox:
            case synctex_node_type_void_hbox:
                /*  best of distances from the left edge and right edge*/
                box.min.h = _synctex_data_h(node);
                box.max.h = box.min.h;
                box.min.v = _synctex_data_v(node);
                box.max.v = box.min.v + _synctex_abs(_synctex_data_depth(node));
                box.min.v -= _synctex_abs(_synctex_data_height(node));
                d = _synctex_distance_to_box_v2(hit,&box);
                box.min.h = box.min.h + _synctex_abs(_synctex_data_width(node));
                box.max.h = box.min.h;
                dd = _synctex_distance_to_box_v2(hit,&box);
                return d<dd ? d:dd;
            case synctex_node_type_kern:
                box.min.h = _synctex_data_h(node);
                box.max.h = box.min.h;
                box.max.v = _synctex_data_v(node);
                box.min.v = box.max.v - _synctex_abs(_synctex_data_height(_synctex_tree_parent(node)));
                d = _synctex_distance_to_box_v2(hit,&box);
                box.min.h -= _synctex_data_width(node);
                box.max.h = box.min.h;
                dd = _synctex_distance_to_box_v2(hit,&box);
                return d<dd ? d:dd;
            case synctex_node_type_glue:
            case synctex_node_type_math:
            case synctex_node_type_boundary:
            case synctex_node_type_box_bdry:
                box.min.h = _synctex_data_h(node);
                box.max.h = box.min.h;
                box.max.v = _synctex_data_v(node);
                box.min.v = box.max.v - _synctex_abs(_synctex_data_height(_synctex_tree_parent(node)));
                return _synctex_distance_to_box_v2(hit,&box);
            case synctex_node_type_proxy:
            case synctex_node_type_proxy_last:
            {
                synctex_point_s otherHit = *hit;
                otherHit.h -= _synctex_data_h(node);
                otherHit.v -= _synctex_data_v(node);
                return _synctex_point_node_distance_v2(&otherHit, _synctex_tree_target(node));
            }
            default: break;
        }
    }
    return d;
}
static synctex_node_p _synctex_eq_deepest_container_v2(synctex_point_p hit, synctex_node_p node) {
    if (node) {
        /**/
        synctex_node_p child;
        if ((child = synctex_node_child(node))) {
            /*  Non void hbox or vbox, form ref or proxy */
            /*  We go deep first because some boxes have 0 dimensions
             *  despite they do contain some black material.
             */
            do {
                if ((_synctex_point_in_box_v2(hit,child))) {
                    synctex_node_p deep = _synctex_eq_deepest_container_v2(hit,child);
                    if (deep) {
                        /*  One of the children contains the hit. */
                        return deep;
                    }
                }
            } while((child = synctex_node_sibling(child)));
            /*  is the hit point inside the box? */
            if (synctex_node_type(node) == synctex_node_type_vbox
                || synctex_node_type(node) == synctex_node_type_proxy_vbox) {
                /*  For vboxes we try to use some node inside.
                 *  Walk through the list of siblings until we find the closest one.
                 *  Only consider siblings with children inside. */
                if ((child = _synctex_tree_child(node))) {
                    synctex_nd_s best = SYNCTEX_ND_0;
                    do {
                        if (_synctex_tree_child(child)) {
                            int d = _synctex_point_node_distance_v2(hit,child);
                            if (d <= best.distance) {
                                best = (synctex_nd_s){child, d};
                            }
                        }
                    } while((child = __synctex_tree_sibling(child)));
                    if (best.node) {
                        return best.node;
                    }
                }
            }
            if (_synctex_point_in_box_v2(hit,node)) {
                return node;
            }
        }
    }
    return NULL;
}
static synctex_nd_s _synctex_eq_deepest_container_v3(synctex_point_p hit, synctex_node_p node) {
    if (node) {
        synctex_node_p child = NULL;
        if ((child = synctex_node_child(node))) {
            /*  Non void hbox, vbox, box proxy or form ref */
            /*  We go deep first because some boxes have 0 dimensions
             *  despite they do contain some black material.
             */
            do {
                synctex_nd_s deep = _synctex_eq_deepest_container_v3(hit, child);
                if (deep.node) {
                    /*  One of the children contains the hit-> */
                    return deep;
                }
            } while((child = synctex_node_sibling(child)));
            /*  For vboxes we try to use some node inside.
             *  Walk through the list of siblings until we find the closest one.
             *  Only consider siblings with children inside. */
            if (synctex_node_type(node) == synctex_node_type_vbox
                || synctex_node_type(node) == synctex_node_type_proxy_vbox) {
                if ((child = synctex_node_child(node))) {
                    synctex_nd_s best = SYNCTEX_ND_0;
                    do {
                        if (synctex_node_child(child)) {
                            int d = _synctex_point_node_distance_v2(hit,child);
                            if (d < best.distance) {
                                best = (synctex_nd_s){child,d};
                            }
                        }
                    } while((child = synctex_node_sibling(child)));
                    if (best.node) {
                        return best;
                    }
                }
            }
            /*  is the hit point inside the box? */
            if (_synctex_point_in_box_v2(hit,node)) {
                return (synctex_nd_s){node, 0};
            }
        }
    }
    return SYNCTEX_ND_0;
}

/*  Compares the locations of the hit point with the locations of
 *  the various nodes contained in the box.
 *  As it is an horizontal box, we only compare horizontal coordinates.
 */
SYNCTEX_INLINE static synctex_nd_lr_s __synctex_eq_get_closest_children_in_hbox_v2(synctex_point_p hitP, synctex_node_p node) {
    synctex_nd_s childd = SYNCTEX_ND_0;
    synctex_nd_lr_s nds = {SYNCTEX_ND_0,SYNCTEX_ND_0};
    if ((childd.node = synctex_node_child(node))) {
        synctex_nd_s nd = SYNCTEX_ND_0;
        do {
            childd = _synctex_point_h_ordered_distance_v2(hitP,childd.node);
            if (childd.distance > 0) {
                /*  node is to the right of the hit point.
                 *  We compare node and the previously recorded one, through the recorded distance.
                 *  If the nodes have the same tag, prefer the one with the smallest line number,
                 *  if the nodes also have the same line number, prefer the one with the smallest column. */
                if (nds.r.distance > childd.distance) {
                    nds.r = childd;
                } else if (nds.r.distance == childd.distance && nds.r.node) {
                    if (_synctex_data_tag(nds.r.node) == _synctex_data_tag(childd.node)
                        && (_synctex_data_line(nds.r.node) > _synctex_data_line(childd.node)
                            || (_synctex_data_line(nds.r.node) == _synctex_data_line(childd.node)
                                && _synctex_data_column(nds.r.node) > _synctex_data_column(childd.node)))) {
                                nds.r = childd;
                            }
                }
            } else if (childd.distance == 0) {
                /*  hit point is inside node. */
                if (_synctex_tree_child(childd.node)) {
                    return _synctex_eq_get_closest_children_in_box_v2(hitP, childd.node);
                }
                nds.l = childd;
            } else { /*  here childd.distance < 0, the hit point is to the right of node */
                childd.distance = -childd.distance;
                if (nds.l.distance > childd.distance) {
                    nds.l = childd;
                } else if (nds.l.distance == childd.distance && nds.l.node) {
                    if (_synctex_data_tag(nds.l.node) == _synctex_data_tag(childd.node)
                        && (_synctex_data_line(nds.l.node) > _synctex_data_line(childd.node)
                            || (_synctex_data_line(nds.l.node) == _synctex_data_line(childd.node)
                                && _synctex_data_column(nds.l.node) > _synctex_data_column(childd.node)))) {
                                nds.l = childd;
                            }
                }
            }
        } while((childd.node = synctex_node_sibling(childd.node)));
        if (nds.l.node) {
            /*  the left node is new, try to narrow the result */
            if ((nd = _synctex_eq_deepest_container_v3(hitP,nds.l.node)).node) {
                nds.l = nd;
            }
            if((nd = __synctex_closest_deep_child_v2(hitP,nds.l.node)).node) {
                nds.l.node = nd.node;
            }
        }
        if (nds.r.node) {
            /*  the right node is new, try to narrow the result */
            if ((nd = _synctex_eq_deepest_container_v3(hitP,nds.r.node)).node) {
                nds.r = nd;
            }
            if((nd = __synctex_closest_deep_child_v2(hitP,nds.r.node)).node) {
                nds.r.node = nd.node;
            }
        }
    }
    return nds;
}

#if 0
SYNCTEX_INLINE static synctex_nd_lr_s __synctex_eq_get_closest_children_in_hbox_v3(synctex_point_p hitP, synctex_node_p nodeP) {
    synctex_nd_s nd = SYNCTEX_ND_0;
    synctex_nd_lr_s nds = {SYNCTEX_ND_0,SYNCTEX_ND_0};
    if ((nd.node = _synctex_tree_child(nodeP))) {
        do {
            nd = _synctex_point_h_ordered_distance_v2(hitP,nd.node);
            if (nd.distance > 0) {
                /*  node is to the right of the hit point.
                 *  We compare node and the previously recorded one, through the recorded distance.
                 *  If the nodes have the same tag, prefer the one with the smallest line number,
                 *  if the nodes also have the same line number, prefer the one with the smallest column. */
                if (nds.r.distance > nd.distance) {
                    nds.r = nd;
                } else if (nds.r.distance == nd.distance && nds.r.node) {
                    if (_synctex_data_tag(nds.r.node) == _synctex_data_tag(nd.node)
                        && (_synctex_data_line(nds.r.node) > _synctex_data_line(nd.node)
                            || (_synctex_data_line(nds.r.node) == _synctex_data_line(nd.node)
                                && _synctex_data_column(nds.r.node) > _synctex_data_column(nd.node)))) {
                                nds.r = nd;
                            }
                }
            } else if (nd.distance == 0) {
                /*  hit point is inside node. */
                nds.l = nd;
            } else { /*  here nd.d < 0, the hit point is to the right of node */
                nd.distance = -nd.distance;
                if (nds.l.distance > nd.distance) {
                    nds.l = nd;
                } else if (nds.l.distance == nd.distance && nds.l.node) {
                    if (_synctex_data_tag(nds.l.node) == _synctex_data_tag(nd.node)
                        && (_synctex_data_line(nds.l.node) > _synctex_data_line(nd.node)
                            || (_synctex_data_line(nds.l.node) == _synctex_data_line(nd.node)
                                && _synctex_data_column(nds.l.node) > _synctex_data_column(nd.node)))) {
                                nds.l = nd;
                            }
                }
            }
        } while((nd.node = __synctex_tree_sibling(nd.node)));
        if (nds.l.node) {
            /*  the left node is new, try to narrow the result */
            if ((nd.node = _synctex_eq_deepest_container_v2(hitP,nds.l.node))) {
                nds.l.node = nd.node;
            }
            if((nd = _synctex_eq_closest_child_v2(hitP,nds.l.node)).node) {
                nds.l.node = nd.node;
            }
        }
        if (nds.r.node) {
            /*  the right node is new, try to narrow the result */
            if ((nd.node = _synctex_eq_deepest_container_v2(hitP,nds.r.node))) {
                nds.r.node = nd.node;
            }
            if((nd = _synctex_eq_closest_child_v2(hitP,nds.r.node)).node) {
                nds.r.node = nd.node;
            }
        }
    }
    return nds;
}
#endif
SYNCTEX_INLINE static synctex_nd_lr_s __synctex_eq_get_closest_children_in_vbox_v2(synctex_point_p hitP, synctex_node_p nodeP) {
    SYNCTEX_UNUSED(nodeP)
    synctex_nd_lr_s nds = {SYNCTEX_ND_0,SYNCTEX_ND_0};
    synctex_nd_s nd = SYNCTEX_ND_0;
    if ((nd.node = synctex_node_child(nd.node))) {
        do {
            nd = _synctex_point_v_ordered_distance_v2(hitP,nd.node);
            /*  this is what makes the difference with the h version above */
            if (nd.distance > 0) {
                /*  node is to the top of the hit point (below because TeX is oriented from top to bottom.
                 *  We compare node and the previously recorded one, through the recorded distance.
                 *  If the nodes have the same tag, prefer the one with the smallest line number,
                 *  if the nodes also have the same line number, prefer the one with the smallest column. */
                if (nds.r.distance > nd.distance) {
                    nds.r = nd;
                } else if (nds.r.distance == nd.distance && nds.r.node) {
                    if (_synctex_data_tag(nds.r.node) == _synctex_data_tag(nd.node)
                        && (_synctex_data_line(nds.r.node) > _synctex_data_line(nd.node)
                            || (_synctex_data_line(nds.r.node) == _synctex_data_line(nd.node)
                                && _synctex_data_column(nds.r.node) > _synctex_data_column(nd.node)))) {
                                nds.r = nd;
                            }
                }
            } else if (nd.distance == 0) {
                nds.l = nd;
            } else { /*  here nd < 0 */
                nd.distance = -nd.distance;
                if (nds.l.distance > nd.distance) {
                    nds.l = nd;
                } else if (nds.l.distance == nd.distance && nds.l.node) {
                    if (_synctex_data_tag(nds.l.node) == _synctex_data_tag(nd.node)
                        && (_synctex_data_line(nds.l.node) > _synctex_data_line(nd.node)
                            || (_synctex_data_line(nds.l.node) == _synctex_data_line(nd.node)
                                && _synctex_data_column(nds.l.node) > _synctex_data_column(nd.node)))) {
                                nds.l = nd;
                            }
                }
            }
        } while((nd.node = synctex_node_sibling(nd.node)));
        if (nds.l.node) {
            if ((nd.node = _synctex_eq_deepest_container_v2(hitP,nds.l.node))) {
                nds.l.node = nd.node;
            }
            if((nd = _synctex_eq_closest_child_v2(hitP,nds.l.node)).node) {
                nds.l.node = nd.node;
            }
        }
        if (nds.r.node) {
            if ((nd.node = _synctex_eq_deepest_container_v2(hitP,nds.r.node))) {
                nds.r.node = nd.node;
            }
            if((nd = _synctex_eq_closest_child_v2(hitP,nds.r.node)).node) {
                nds.r.node = nd.node;
            }
        }
    }
    return nds;
}

/**
 *  Get the child closest to the hit point.
 *  - parameter: hit point
 *  - parameter: containing node
 *  - returns: the child and the distance to the hit point.
 *      SYNCTEX_ND_0 if the parameter node has no children.
 *  - note: recursive call.
 */
static synctex_nd_s __synctex_closest_deep_child_v2(synctex_point_p hitP, synctex_node_p node) {
    synctex_nd_s best = SYNCTEX_ND_0;
    synctex_node_p child = NULL;
    if ((child = synctex_node_child(node))) {
#if defined(SYNCTEX_DEBUG)
        printf("Closest deep child on box at line %i\n",
               SYNCTEX_LINEINDEX(node));
#endif
        do {
#if SYNCTEX_DEBUG>500
            synctex_node_display(child);
#endif
            synctex_nd_s nd = SYNCTEX_ND_0;
            if (_synctex_node_is_box(child)) {
                nd = __synctex_closest_deep_child_v2(hitP,child);
            } else {
                nd = (synctex_nd_s) {child, _synctex_point_node_distance_v2(hitP,child)};
            }
            if (nd.distance < best.distance ||(nd.distance == best.distance
                                               && synctex_node_type(nd.node) != synctex_node_type_kern)) {
#if defined(SYNCTEX_DEBUG)
                if(nd.node) {
                    printf("New best %i<=%i line %i\n",nd.distance,
                           best.distance,SYNCTEX_LINEINDEX(nd.node));
                }
#endif
                best = nd;
            }
        } while((child = synctex_node_sibling(child)));
#if defined(SYNCTEX_DEBUG)
        if(best.node) {
            printf("Found new best %i line %i\n",best.distance,SYNCTEX_LINEINDEX(best.node));
        }
#endif
    }
    return best;
}

/**
 *  Return the closest child.
 *  - parameter: a pointer to the hit point,
 *  - parameter: the container
 *  - return: SYNCTEX_ND_0 if node has no child,
 *      the __synctex_closest_deep_child_v2 otherwise.
 */
static synctex_nd_s _synctex_eq_closest_child_v2(synctex_point_p hitP, synctex_node_p node) {
    synctex_nd_s nd = SYNCTEX_ND_0;
    if (_synctex_node_is_box(node)) {
        nd = __synctex_closest_deep_child_v2(hitP, node);
        if (_synctex_node_is_box(nd.node)) {
            synctex_node_p child = NULL;
            if ((child = synctex_node_child(nd.node))) {
                synctex_nd_s best = {child,_synctex_point_node_distance_v2(hitP,child)};
                while((child = synctex_node_sibling(child))) {
                    int d = _synctex_point_node_distance_v2(hitP,child);
                    if (d < best.distance) {
                        best = (synctex_nd_s){child,d};
                    } else if (d == best.distance && synctex_node_type(child) != synctex_node_type_kern) {
                        best.node = child;
                    }
                }
                return best;
            }
        }
        return nd;
    }
    return SYNCTEX_ND_0;
}
SYNCTEX_INLINE static synctex_nd_lr_s _synctex_eq_get_closest_children_in_box_v2(synctex_point_p hitP, synctex_node_p node) {
    synctex_nd_lr_s nds = {SYNCTEX_ND_0,SYNCTEX_ND_0};
    if(_synctex_tree_has_child(node)) { /* node != NULL */
        if (node->class_->type==synctex_node_type_hbox ||
            node->class_->type==synctex_node_type_proxy_hbox) {
            return __synctex_eq_get_closest_children_in_hbox_v2(hitP,node);
        } else {
            return __synctex_eq_get_closest_children_in_vbox_v2(hitP,node);
        }
    }
    return nds;
}

#ifndef SYNCTEX_NO_UPDATER

#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark Updater
#   endif

typedef int (*synctex_print_f)(synctex_updater_p, const char * , ...); /*  print formatted to either FILE *  or gzFile */
typedef void (*synctex_close_f)(synctex_updater_p); /*  close FILE *  or gzFile */

#   define SYNCTEX_BITS_PER_BYTE 8

typedef union {
    gzFile as_gzFile;
    FILE * as_FILE_p;
    void * as_ptr;
} synctex_file_u;

struct synctex_updater_t {
    synctex_file_u file;
    synctex_print_f print;
    synctex_close_f close;
    int length;             /*  the number of chars appended */
};

SYNCTEX_ATTRIBUTE_FORMAT_PRINTF(2, 3)
static int _synctex_updater_print(synctex_updater_p updater, const char * format, ...) {
    int result = 0;
    if (updater) {
        va_list va;
        va_start(va, format);
        result = vfprintf(updater->file.as_FILE_p,
                           format,
                           va);
        va_end(va);
    }
    return result;
}
#if defined(_MSC_VER)
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

static int vasprintf(char **ret,
                     const char *format,
                     va_list ap)
{
    int len;
    len = _vsnprintf(NULL, 0, format, ap);
    if (len < 0) return -1;
    *ret = malloc(len + 1);
    if (!*ret) return -1;
    _vsnprintf(*ret, len+1, format, ap);
    (*ret)[len] = '\0';
    return len;
}

#endif

/**
 *  gzvprintf is not available until OSX 10.10
 */
SYNCTEX_ATTRIBUTE_FORMAT_PRINTF(2, 3)
static int _synctex_updater_print_gz(synctex_updater_p updater, const char * format, ...) {
    int result = 0;
    if (updater) {
        char * buffer;
        va_list va;
        va_start(va, format);
        if (vasprintf(&buffer, format, va) < 0) {
            _synctex_error("Out of memory...");
        } else if ((result = (int)strlen(buffer))) {
            result = gzwrite(updater->file.as_gzFile, buffer, (unsigned)result);
        }
        va_end(va);
        free(buffer);
    }
    return result;
}

static void _synctex_updater_close(synctex_updater_p updater) {
    if (updater) {
        fclose(updater->file.as_FILE_p);
    }
}

static void _synctex_updater_close_gz(synctex_updater_p updater) {
    if (updater) {
        gzclose(updater->file.as_gzFile);
    }
}

synctex_updater_p synctex_updater_new_with_output_file(const char * output, const char * build_directory) {
    synctex_updater_p updater = NULL;
    const char * mode = NULL;
    synctex_open_s open;
    /*  prepare the updater, the memory is the only one dynamically allocated */
    updater = (synctex_updater_p)_synctex_malloc(sizeof(synctex_updater_s));
    if (NULL == updater) {
        _synctex_error("!  synctex_updater_new_with_file: malloc problem");
        return NULL;
    }
    open = _synctex_open_v2(output,build_directory,0,synctex_ADD_QUOTES);
    if (open.status < SYNCTEX_STATUS_OK) {
        open = _synctex_open_v2(output,build_directory,0,synctex_DONT_ADD_QUOTES);
        if (open.status < SYNCTEX_STATUS_OK) {
        return_on_error:
            _synctex_free(updater);
            return updater = NULL;
        }
    }
    /*  OK, the file exists, we close it and reopen it with the correct mode.
     *  The receiver is now the owner of the "synctex" variable. */
    gzclose(open.file);
    updater->file.as_ptr = NULL;
    mode = _synctex_get_io_mode_name(open.io_mode|synctex_io_append_mask);/* either "a" or "ab", depending on the file extension */
    if (open.io_mode&synctex_io_gz_mask) {
        if (NULL == (updater->file.as_FILE_p = fopen(open.synctex,mode))) {
        no_write_error:
            _synctex_error("!  synctex_updater_new_with_file: Can't append to %s",open.synctex);
            free(open.synctex);
            goto return_on_error;
        }
        updater->print = &_synctex_updater_print;
        updater->close = &_synctex_updater_close;
    } else {
        if (NULL == (updater->file.as_gzFile = gzopen(open.synctex,mode))) {
            goto no_write_error;
        }
        updater->print = &_synctex_updater_print_gz;
        updater->close = &_synctex_updater_close_gz;
    }
    printf("SyncTeX: updating %s...",open.synctex);
    _synctex_free(open.synctex);
    return updater;
}

void synctex_updater_append_magnification(synctex_updater_p updater, char * magnification){
    if (NULL==updater) {
        return;
    }
    if (magnification && strlen(magnification)) {
        updater->length +=
        updater->print(updater,"Magnification:%s\n",magnification);
    }
}

void synctex_updater_append_x_offset(synctex_updater_p updater, char * x_offset){
    if (NULL==updater) {
        return;
    }
    if (x_offset && strlen(x_offset)) {
        updater->length += updater->print(updater,"X Offset:%s\n",x_offset);
    }
}

void synctex_updater_append_y_offset(synctex_updater_p updater, char * y_offset){
    if (NULL==updater) {
        return;
    }
    if (y_offset && strlen(y_offset)) {
        updater->length += updater->print(updater,"Y Offset:%s\n",y_offset);
    }
}

void synctex_updater_free(synctex_updater_p updater){
    if (NULL==updater) {
        return;
    }
    if (updater->length>0) {
        updater->print(updater,"!%i\n",updater->length);
    }
    updater->close(updater);
    _synctex_free(updater);
    printf("... done.\n");
    return;
}
#endif

#if defined(SYNCTEX_TESTING)
#	ifdef SYNCTEX_NOTHING
#       pragma mark -
#       pragma mark Testers
#   endif
/**
 *  The next nodes corresponds to a deep first tree traversal.
 *  Does not create child proxies as side effect contrary to
 *  the synctex_node_next method above.
 *  May loop infinitely many times if the tree
 *  is not properly built (contains loops).
 */
static synctex_node_p _synctex_node_next(synctex_node_p node) {
    synctex_node_p N = _synctex_tree_child(node);
    if (N) {
        return N;
    }
    return _synctex_node_sibling_or_parents(node);
}
static int _synctex_input_copy_name(synctex_node_p input, char * name) {
    char * copy = _synctex_malloc(strlen(name)+1);
    memcpy(copy,name,strlen(name)+1);
    _synctex_data_set_name(input,copy);
    return 0;
}
int synctex_test_setup_scanner_sheets_421(synctex_scanner_p scanner) {
    int TC = 0;
    synctex_node_p sheet = synctex_node_new(scanner,synctex_node_type_sheet);
    _synctex_data_set_page(sheet,4);
    SYNCTEX_TEST_BODY(TC, _synctex_data_page(sheet)==4,"");
    synctex_node_free(scanner->sheet);
    scanner->sheet = sheet;
    sheet = synctex_node_new(scanner,synctex_node_type_sheet);
    _synctex_data_set_page(sheet,2);
    SYNCTEX_TEST_BODY(TC, _synctex_data_page(sheet)==2,"");
    __synctex_tree_set_sibling(sheet, scanner->sheet);
    scanner->sheet = sheet;
    sheet = synctex_node_new(scanner,synctex_node_type_sheet);
    _synctex_data_set_page(sheet,1);
    SYNCTEX_TEST_BODY(TC, _synctex_data_page(sheet)==1,"");
    __synctex_tree_set_sibling(sheet, scanner->sheet);
    scanner->sheet = sheet;
    return TC;
}
int synctex_test_input(synctex_scanner_p scanner) {
    int TC = 0;
    synctex_node_p input = synctex_node_new(scanner,synctex_node_type_input);
    _synctex_data_set_tag(input,421);
    SYNCTEX_TEST_BODY(TC, _synctex_data_tag(input)==421,"");
    _synctex_data_set_tag(input,124);
    SYNCTEX_TEST_BODY(TC, _synctex_data_tag(input)==124,"");
    _synctex_data_set_line(input,421);
    SYNCTEX_TEST_BODY(TC, _synctex_data_line(input)==421,"");
    _synctex_data_set_line(input,214);
    SYNCTEX_TEST_BODY(TC, _synctex_data_line(input)==214,"");
    _synctex_data_set_line(input,214);
    SYNCTEX_TEST_BODY(TC, _synctex_data_line(input)==214,"");
    _synctex_input_copy_name(input,"214");
    SYNCTEX_TEST_BODY(TC, 0==memcmp(_synctex_data_name(input),"214",4),"");
    _synctex_input_copy_name(input,"421421");

    SYNCTEX_TEST_BODY(TC,
                      0==memcmp(_synctex_data_name(input),
                                "421421",
                                4),
                      "");
    synctex_node_free(input);
    return TC;
}
int synctex_test_proxy(synctex_scanner_p scanner) {
    int TC = 0;
    synctex_node_p proxy = synctex_node_new(scanner,synctex_node_type_proxy);
    synctex_node_p target = synctex_node_new(scanner,synctex_node_type_rule);
    _synctex_tree_set_target(proxy,target);
    _synctex_data_set_tag(target,421);
    SYNCTEX_TEST_BODY(TC, _synctex_data_tag(target)==421,"");
    SYNCTEX_TEST_BODY(TC, synctex_node_tag(target)==421,"");
    SYNCTEX_TEST_BODY(TC, synctex_node_tag(proxy)==421,"");
    synctex_node_free(proxy);
    synctex_node_free(target);
    return TC;
}
int synctex_test_handle(synctex_scanner_p scanner) {
    int TC = 0;
    synctex_node_p handle = synctex_node_new(scanner,synctex_node_type_handle);
    synctex_node_p proxy = synctex_node_new(scanner, synctex_node_type_proxy);
    synctex_node_p target = synctex_node_new(scanner,synctex_node_type_rule);
    _synctex_tree_set_target(handle,target);
    _synctex_data_set_tag(target,421);
    SYNCTEX_TEST_BODY(TC, _synctex_data_tag(target)==421,"");
    SYNCTEX_TEST_BODY(TC, synctex_node_tag(target)==421,"");
    SYNCTEX_TEST_BODY(TC, synctex_node_tag(handle)==421,"");
    _synctex_data_set_line(target,214);
    SYNCTEX_TEST_BODY(TC, _synctex_data_line(target)==214,"");
    SYNCTEX_TEST_BODY(TC, synctex_node_line(target)==214,"");
    SYNCTEX_TEST_BODY(TC, synctex_node_line(handle)==214,"");
    _synctex_data_set_column(target,142);
    SYNCTEX_TEST_BODY(TC, _synctex_data_column(target)==142,"");
    SYNCTEX_TEST_BODY(TC, synctex_node_column(target)==142,"");
    SYNCTEX_TEST_BODY(TC, synctex_node_column(handle)==142,"");
    _synctex_tree_set_target(proxy,target);
    _synctex_tree_set_target(handle,proxy);
    _synctex_data_set_tag(target,412);
    SYNCTEX_TEST_BODY(TC, _synctex_data_tag(target)==412,"");
    SYNCTEX_TEST_BODY(TC, synctex_node_tag(target)==412,"");
    SYNCTEX_TEST_BODY(TC, synctex_node_tag(handle)==412,"");
    _synctex_data_set_line(target,124);
    SYNCTEX_TEST_BODY(TC, _synctex_data_line(target)==124,"");
    SYNCTEX_TEST_BODY(TC, synctex_node_line(target)==124,"");
    SYNCTEX_TEST_BODY(TC, synctex_node_line(handle)==124,"");
    _synctex_data_set_column(target,241);
    SYNCTEX_TEST_BODY(TC, _synctex_data_column(target)==241,"");
    SYNCTEX_TEST_BODY(TC, synctex_node_column(target)==241,"");
    SYNCTEX_TEST_BODY(TC, synctex_node_column(handle)==241,"");
    synctex_node_free(handle);
    synctex_node_free(proxy);
    synctex_node_free(target);
    return TC;
}
int synctex_test_setup_scanner_input(synctex_scanner_p scanner) {
    int TC = 0;
    synctex_node_p input = synctex_node_new(scanner,synctex_node_type_input);
    _synctex_data_set_tag(input,4);
    _synctex_input_copy_name(input,"21");
    _synctex_data_set_line(input,421);
    synctex_node_free(scanner->input);
    scanner->input = input;
    SYNCTEX_TEST_BODY(TC, _synctex_data_tag(input)==4,"");
    SYNCTEX_TEST_BODY(TC, strcmp(_synctex_data_name(input),"21")==0,"");
    SYNCTEX_TEST_BODY(TC, _synctex_data_line(input)==421,"");
    return TC;
}
int synctex_test_setup_nodes(synctex_scanner_p scanner, synctex_node_r nodes) {
    int TC = 0;
    int n;
    for (n=0;n<synctex_node_number_of_types;++n) {
        nodes[n] = synctex_node_new(scanner,n);
        SYNCTEX_TEST_BODY(TC, nodes[n]!=NULL,"");
    }
    return TC;
}
int synctex_test_teardown_nodes(synctex_scanner_p scanner, synctex_node_r nodes) {
    int n;
    for (n=0;n<synctex_node_number_of_types;++n) {
        synctex_node_free(nodes[n]);
        nodes[n]=NULL;
    }
    return 1;
}
int synctex_test_tree(synctex_scanner_p scanner) {
    int TC = 0;
    synctex_node_p nodes1[synctex_node_number_of_types];
    synctex_node_p nodes2[synctex_node_number_of_types];
    synctex_node_p nodes3[synctex_node_number_of_types];
    int i,j;
    TC += synctex_test_setup_nodes(scanner,nodes1);
    TC += synctex_test_setup_nodes(scanner,nodes2);
    TC += synctex_test_setup_nodes(scanner,nodes3);
    /*  Every node has a sibling */
    for (i=0;i<synctex_node_number_of_types;++i) {
        for (j=0;j<synctex_node_number_of_types;++j) {
            _synctex_tree_set_sibling(nodes1[i],nodes2[i]);
            SYNCTEX_TEST_BODY(TC, nodes2[i]==synctex_node_sibling(nodes1[i]),"");
        }
    }
    synctex_test_teardown_nodes(scanner,nodes3);
    synctex_test_teardown_nodes(scanner,nodes2);
    synctex_test_teardown_nodes(scanner,nodes1);
    return TC;
}
int synctex_test_page(synctex_scanner_p scanner) {
    int TC = synctex_test_setup_scanner_sheets_421(scanner);
    synctex_node_p sheet = scanner->sheet;
    synctex_node_p node = synctex_node_new(scanner,synctex_node_type_rule);
    _synctex_data_set_tag(node,4);
    _synctex_data_set_line(node,21);
    synctex_node_free(_synctex_node_set_child(sheet,node));
    SYNCTEX_TEST_BODY(TC, synctex_node_page(node)==synctex_node_page(sheet),"");
    return TC;
}
int synctex_test_display_query(synctex_scanner_p scanner) {
    int TC = synctex_test_setup_scanner_sheets_421(scanner);
    synctex_node_p sheet = scanner->sheet;
    synctex_node_p node = synctex_node_new(scanner,synctex_node_type_rule);
    _synctex_data_set_tag(node,4);
    _synctex_data_set_line(node,21);
    synctex_node_free(_synctex_node_set_child(sheet,node));
    SYNCTEX_TEST_BODY(TC, node==synctex_node_child(sheet),"");
    __synctex_node_make_friend_tlc(node);
    SYNCTEX_TEST_BODY(TC, _synctex_scanner_friend(scanner, 25)==node,"");
    sheet = __synctex_tree_sibling(sheet);
    node = synctex_node_new(scanner,synctex_node_type_rule);
    _synctex_data_set_tag(node,4);
    _synctex_data_set_line(node,21);
    synctex_node_free(_synctex_node_set_child(sheet,node));
    SYNCTEX_TEST_BODY(TC, node==synctex_node_child(sheet),"");
    __synctex_node_make_friend_tlc(node);
    SYNCTEX_TEST_BODY(TC, _synctex_scanner_friend(scanner, 25)==node,"");
    sheet = __synctex_tree_sibling(sheet);
    node = synctex_node_new(scanner,synctex_node_type_rule);
    _synctex_data_set_tag(node,4);
    _synctex_data_set_line(node,21);
    synctex_node_free(_synctex_node_set_child(sheet,node));
    SYNCTEX_TEST_BODY(TC, node==synctex_node_child(sheet),"");
    __synctex_node_make_friend_tlc(node);
    SYNCTEX_TEST_BODY(TC, (_synctex_scanner_friend(scanner, 25)==node),"");
    synctex_test_setup_scanner_input(scanner);
    scanner->flags.has_parsed = synctex_YES;
#if 1
    SYNCTEX_TEST_BODY(TC, (synctex_display_query(scanner,"21",21,4,-1)==3),"");
#endif
    return TC;
}
typedef struct {
    int s;      /* status */
    char n[25]; /* name */
} synctex_test_sn_s;

synctex_test_sn_s synctex_test_tmp_sn(char * content) {
    synctex_test_sn_s sn = {0, "/tmp/test.XXXXXX.synctex"};
    FILE *sfp;
    int fd = mkstemps(sn.n,8);
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", sn.n, strerror(errno));
        sn.s = -1;
        return sn;
    }
    if ((sfp = fdopen(fd, "w+")) == NULL) {
        unlink(sn.n);
        close(fd);
        fprintf(stderr, "%s: %s\n", sn.n, strerror(errno));
        sn.s = -2;
        return sn;
    }
    sn.s = fputs(content,sfp);
    printf("temp:%s\n%i\n",sn.n,sn.s);
    fclose(sfp);
    if (sn.s==0) {
        sn.s = -2;
        unlink(sn.n);
    }
    return sn;
}
int synctex_test_sheet_1() {
    int TC = 0;
    char * content =
    "SyncTeX Version:1  \n" /*00-19*/
    "Input:1:./1.tex    \n" /*20-39*/
    "Output:pdf         \n" /*40-59*/
    "Magnification:100000000      \n" /*60-89*/
    "Unit:1   \n"           /*90-99*/
    "X Offset:0         \n" /*00-19*/
    "Y Offset:0         \n" /*20-39*/
    "Content: \n"           /*40-49*/
    "{1       \n"           /*50-59*/
    "[1,10:20,350:330,330,0       \n" /*60-89*/
    "]        \n"           /*90-99*/
    "}        \n"           /*00-09*/
    "Postamble:\n";
    synctex_test_sn_s sn = synctex_test_tmp_sn(content);
    if (sn.s>0) {
        synctex_scanner_p scanner = synctex_scanner_new_with_output_file(sn.n, NULL, synctex_YES);
        synctex_node_p node = synctex_scanner_handle(scanner);
        printf("Created nodes:\n");
        while (node) {
            printf("%s\n",_synctex_node_abstract(node));
            node = synctex_node_next(node);
        }
        synctex_scanner_free(scanner);
        unlink(sn.n);
    } else {
        ++TC;
    }
    return TC;
}
int synctex_test_sheet_2() {
    int TC = 0;
    char * content =
    "SyncTeX Version:1  \n" /*00-19*/
    "Input:1:./1.tex    \n" /*20-39*/
    "Output:pdf         \n" /*40-59*/
    "Magnification:100000000      \n" /*60-89*/
    "Unit:1   \n"           /*90-99*/
    "X Offset:0         \n" /*00-19*/
    "Y Offset:0         \n" /*20-39*/
    "Content: \n"           /*40-49*/
    "{1       \n"           /*50-59*/
    "(1,10:20,350:330,330,0       \n" /*60-89*/
    ")        \n"           /*90-99*/
    "}        \n"           /*00-09*/
    "Postamble:\n";
    synctex_test_sn_s sn = synctex_test_tmp_sn(content);
    if (sn.s>0) {
        synctex_scanner_p scanner = synctex_scanner_new_with_output_file(sn.n, NULL, synctex_YES);
        synctex_node_p node = synctex_scanner_handle(scanner);
        printf("Created nodes:\n");
        while (node) {
            printf("%s\n",_synctex_node_abstract(node));
            node = _synctex_node_next(node);
        }
        TC += synctex_scanner_free(scanner);
        unlink(sn.n);
    } else {
        ++TC;
    }
    return TC;
}
int synctex_test_charindex() {
    int TC = 0;
    char * content =
    "SyncTeX Version:1  \n" /*00-19*/
    "Input:1:./1.tex    \n" /*20-39*/
    "Output:pdf         \n" /*40-59*/
    "Magnification:100000000      \n" /*60-89*/
    "Unit:1   \n"           /*90-99*/
    "X Offset:0         \n" /*00-19*/
    "Y Offset:0         \n" /*20-39*/
    "Content: \n"           /*40-49*/
    "{1       \n"           /*50-59*/
    "[1,10:20,350:330,330,0       \n" /*60-89*/
    "(1,58:20,100:250,10,5        \n" /*90-119*/
    "f1000:50,100       \n" /*20-39*/
    ")        \n"           /*40-49*/
    "]        \n"           /*50-59*/
    "}        \n"           /*60-69*/
    "Postamble:\n";
    synctex_test_sn_s sn = synctex_test_tmp_sn(content);
    if (sn.s>0) {
        synctex_scanner_p scanner = synctex_scanner_new_with_output_file(sn.n, NULL, synctex_YES);
        synctex_node_p node = synctex_scanner_handle(scanner);
        printf("Created nodes:\n");
        while (node) {
            printf("%s\n",_synctex_node_abstract(node));
            node = synctex_node_next(node);
        }
        TC += synctex_scanner_free(scanner);
        unlink(sn.n);
    } else {
        ++TC;
    }
    return TC;
}
int synctex_test_form() {
    int TC = 0;
    char * content =
    "SyncTeX Version:1  \n" /*00-19*/
    "Input:1:./1.tex    \n" /*20-39*/
    "Output:pdf         \n" /*40-59*/
    "Magnification:100000000      \n" /*60-89*/
    "Unit:1   \n"           /*90-99*/
    "X Offset:0         \n" /*00-19*/
    "Y Offset:0         \n" /*20-39*/
    "Content: \n"           /*40-49*/
    "{1       \n"           /*50-59*/
    "[1,10:20,350:330,330,0       \n" /*60-89*/
    "(1,58:20,100:250,10,5        \n" /*90-119*/
    "f1000:50,100       \n" /*20-39*/
    ")        \n"           /*40-49*/
    "]        \n"           /*50-59*/
    "}        \n"           /*60-69*/
    "<1000    \n"           /*70-79*/
    "(1,63:0,0:100,8,3  \n" /*80-99*/
    ")        \n"           /*00-09*/
    ">        \n"           /*10-19*/
    "Postamble:\n";
    synctex_test_sn_s sn = synctex_test_tmp_sn(content);
    if (sn.s>0) {
        synctex_scanner_p scanner = synctex_scanner_new_with_output_file(sn.n, NULL, synctex_YES);
        synctex_node_p node = synctex_scanner_handle(scanner);
        while (node) {
            printf("%s\n",_synctex_node_abstract(node));
            node = _synctex_node_next(node);
        }
        TC += synctex_scanner_free(scanner);
        unlink(sn.n);
    } else {
        ++TC;
    }
    return TC;
}
#endif
