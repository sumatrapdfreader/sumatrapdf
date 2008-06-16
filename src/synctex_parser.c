/* 
Copyright (c) 2008 jerome DOT laurens AT u-bourgogne DOT fr

This file is part of the SyncTeX package.

License:
--------
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

*/

#ifdef SYNCTEX_FEATURE


/*  When this source code is embedded in a project outside TeXLive,
 *  the web2c/c-auto.h header file is not available.
 *  We assume that high level application like pdf viewers will want
 *  to embed the code, such that we assume that they also have locale.h and setlocale. */

#   if defined(SYNCTEX_NO_TEXLIVE) || defined(_MSC_VER) || defined(__DARWIN_UNIX03)
#       define HAVE_LOCALE_H 1
#       define HAVE_SETLOCALE 1
#       if defined(_MSC_VER) 
#          define inline __inline
#       endif
#   else
#       include "web2c/c-auto.h" /* for inline && HAVE_xxx */
#   endif

#include "stddef.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "limits.h"
#include "time.h"
#include "math.h"
#include "errno.h"

#if defined(HAVE_LOCALE_H)
#include <locale.h>
#endif

/*  This custom malloc functions initializes to 0 the newly allocated memory. */
void *_synctex_malloc(size_t size) {
	void * ptr = malloc(size);
	if(ptr) {
/*  In Visual C, bzero is not available */
#ifdef _MSC_VER
		memset(ptr,0, size);
#else
		bzero(ptr,size);
#endif
	}
	return (void *)ptr;
}

/*  The data is organized in a graph with multiple entries.
 *  The root object is a scanner, it is created with the contents on a synctex file.
 *  Each leaf of the tree is a synctex_node_t object.
 *  There are 3 subtrees, two of them sharing the same leaves.
 *  The first tree is the list of input records, where input file names are associated with tags.
 *  The second tree is the box tree as given by TeX when shipping pages out.
 *  First level objects are sheets, containing boxes, glues, kerns...
 *  The third tree allows to browse leaves according to tag and line.
 */

#include "synctex_parser.h"

/* each synctex node has a class */
typedef struct __synctex_class_t _synctex_class_t;
typedef _synctex_class_t * synctex_class_t;


/*  synctex_node_t is a pointer to a node
 *  _synctex_node is the target of the synctex_node_t pointer
 *  It is a pseudo object oriented program.
 *  class is a pointer to the class object the node belongs to.
 *  implementation is meant to contain the private data of the node
 *  basically, there are 2 kinds of information: navigation information and
 *  synctex information. Both will depend on the type of the node,
 *  thus different nodes will have different private data.
 *  There is no inheritancy overhead.
 */
typedef union _synctex_info_t {
	int    INT;
	char * PTR;
} synctex_info_t;

struct _synctex_node {
	synctex_class_t class;
	synctex_info_t * implementation;
};

/*  Each node of the tree, except the scanner itself belongs to a class.
 *  The class object is just a struct declaring the owning scanner
 *  This is a pointer to the scanner as root of the tree.
 *  The type is used to identify the kind of node.
 *  The class declares pointers to a creator and a destructor method.
 *  The log and display fields are used to log and display the node.
 *  display will also display the child, sibling and parent sibling.
 *  parent, child and sibling are used to navigate the tree,
 *  from TeX box hierarchy point of view.
 *  The friend field points to a method which allows to navigate from friend to friend.
 *  A friend is a node with very close tag and line numbers.
 *  Finally, the info field point to a method giving the private node info offset.
 */

typedef synctex_node_t *(*_synctex_node_getter_t)(synctex_node_t);
typedef synctex_info_t *(*_synctex_info_getter_t)(synctex_node_t);

struct __synctex_class_t {
	synctex_scanner_t scanner;
	int type;
	synctex_node_t (*new)(synctex_scanner_t scanner);
	void (*free)(synctex_node_t);
	void (*log)(synctex_node_t);
	void (*display)(synctex_node_t);
	_synctex_node_getter_t parent;
	_synctex_node_getter_t child;
	_synctex_node_getter_t sibling;
	_synctex_node_getter_t friend;
	_synctex_info_getter_t info;
};

#	ifdef __DARWIN_UNIX03
#       pragma mark -
#       pragma mark Abstract OBJECTS and METHODS
#   endif

/*  These macros are shortcuts
 *  This macro checks if a message can be sent.
 */
#   define SYNCTEX_CAN_PERFORM(NODE,SELECTOR)\
		(NULL!=((((NODE)->class))->SELECTOR))

/*  This macro is some kind of objc_msg_send.
 *  It takes care of sending the proper message if possible.
 */
#   define SYNCTEX_MSG_SEND(NODE,SELECTOR) if(NODE && SYNCTEX_CAN_PERFORM(NODE,SELECTOR)) {\
		(*((((NODE)->class))->SELECTOR))(NODE);\
	}

/*  read only safe getter
 */
#   define SYNCTEX_GET(NODE,SELECTOR)((NODE && SYNCTEX_CAN_PERFORM(NODE,SELECTOR))?SYNCTEX_GETTER(NODE,SELECTOR)[0]:(NULL))

/*  read/write getter
 */
#   define SYNCTEX_GETTER(NODE,SELECTOR)\
		((synctex_node_t *)((*((((NODE)->class))->SELECTOR))(NODE)))

#   define SYNCTEX_FREE(NODE) SYNCTEX_MSG_SEND(NODE,free);

/*  Parent getter and setter
 */
#   define SYNCTEX_PARENT(NODE) SYNCTEX_GET(NODE,parent)
#   define SYNCTEX_SET_PARENT(NODE,NEW_PARENT) if(NODE && NEW_PARENT && SYNCTEX_CAN_PERFORM(NODE,parent)){\
		SYNCTEX_GETTER(NODE,parent)[0]=NEW_PARENT;\
	}

/*  Child getter and setter
 */
#   define SYNCTEX_CHILD(NODE) SYNCTEX_GET(NODE,child)
#   define SYNCTEX_SET_CHILD(NODE,NEW_CHILD) if(NODE && NEW_CHILD){\
		SYNCTEX_GETTER(NODE,child)[0]=NEW_CHILD;\
		SYNCTEX_GETTER(NEW_CHILD,parent)[0]=NODE;\
	}

/*  Sibling getter and setter
 */
#   define SYNCTEX_SIBLING(NODE) SYNCTEX_GET(NODE,sibling)
#   define SYNCTEX_SET_SIBLING(NODE,NEW_SIBLING) if(NODE && NEW_SIBLING) {\
		SYNCTEX_GETTER(NODE,sibling)[0]=NEW_SIBLING;\
		if(SYNCTEX_CAN_PERFORM(NEW_SIBLING,parent) && SYNCTEX_CAN_PERFORM(NODE,parent)) {\
			SYNCTEX_GETTER(NEW_SIBLING,parent)[0]=SYNCTEX_GETTER(NODE,parent)[0];\
		}\
	}
/*  Friend getter and setter
 */
#   define SYNCTEX_FRIEND(NODE) SYNCTEX_GET(NODE,friend)
#   define SYNCTEX_SET_FRIEND(NODE,NEW_FRIEND) if(NODE && NEW_FRIEND){\
		SYNCTEX_GETTER(NODE,friend)[0]=NEW_FRIEND;\
	}

/*  A node is meant to own its child and sibling.
 *  It is not owned by its parent, unless it is its first child.
 *  This destructor is for all nodes with children.
 */
void _synctex_free_node(synctex_node_t node) {
	if(node) {
		(*((node->class)->sibling))(node);
		SYNCTEX_FREE(SYNCTEX_SIBLING(node));
		SYNCTEX_FREE(SYNCTEX_CHILD(node));
		free(node);
	}
	return;
}

/*  A node is meant to own its child and sibling.
 *  It is not owned by its parent, unless it is its first child.
 *  This destructor is for nodes with no child.
 */
void _synctex_free_leaf(synctex_node_t node) {
	if(node) {
		SYNCTEX_FREE(SYNCTEX_SIBLING(node));
		free(node);
	}
	return;
}

#include <zlib.h>

/*  The synctex scanner is the root object.
 *  Is is initialized with the contents of a text file.
 *  The buffer_? are first used to parse the text.
 */
struct __synctex_scanner_t {
	gzFile file;                  /* The (compressed) file */
	unsigned char * buffer_cur;   /* current location in the buffer */
	unsigned char * buffer_start; /* start of the buffer */
	unsigned char * buffer_end;   /* end of the buffer */
	char * output_fmt;            /* dvi or pdf, not yet used */
	char * output;                /* the output name used to create the scanner */
	int version;                  /* 1, not yet used */
	int pre_magnification;        /* magnification from the synctex preamble */
	int pre_unit;                 /* unit from the synctex preamble */
	int pre_x_offset;             /* X offste from the synctex preamble */
	int pre_y_offset;             /* Y offset from the synctex preamble */
	int count;                    /* Number of records, from the synctex postamble */
	float unit;                   /* real unit, from synctex preamble or post scriptum */
	float x_offset;               /* X offset, from synctex preamble or post scriptum */
	float y_offset;               /* Y Offset, from synctex preamble or post scriptum */
	synctex_node_t sheet;         /* The first sheet node, its siblings are the other sheet nodes */
	synctex_node_t input;         /* The first input node, its siblings are the other input nodes */
	int number_of_lists;          /* The number of friend lists */
	synctex_node_t * lists_of_friends;/* The friend lists */
	_synctex_class_t class[synctex_node_type_last]; /* The classes of the nodes of the scanner */
};

/*  SYNCTEX_CUR, SYNCTEX_START and SYNCTEX_END are convenient shortcuts
 */
#   define SYNCTEX_CUR (scanner->buffer_cur)
#   define SYNCTEX_START (scanner->buffer_start)
#   define SYNCTEX_END (scanner->buffer_end)

#	ifdef __DARWIN_UNIX03
#       pragma mark -
#       pragma mark OBJECTS, their creators and destructors.
#   endif

/*  Here, we define the indices for the different informations.
 *  They are used to declare the size of the implementation.
 *  For example, if one object uses SYNCTEX_HORIZ_IDX is its size,
 *  then its info will contain a tag, line, column, horiz but no width nor height nor depth
 */
#   define SYNCTEX_PAGE_IDX 0

/*  The sheet is a first level node.
 *  It has no parent (the parent is the scanner itself)
 *  Its sibling points to another sheet.
 *  Its child points to its first child, in general a box.
 *  A sheet node contains only one synctex information: the page.
 *  This is the 1 based page index as given by TeX.
 */
/*  The next macros are used to access the node info
 *  SYNCTEX_INFO(node) points to the first synctex integer or pointer data of node
 *  SYNCTEX_INFO(node)[index] is the information at index
 *  for example, the page of a sheet is stored in SYNCTEX_INFO(sheet)[SYNCTEX_PAGE_IDX]
 */
#   define SYNCTEX_INFO(NODE) ((*((((NODE)->class))->info))(NODE))
#   define SYNCTEX_PAGE_IDX 0
#   define SYNCTEX_PAGE(NODE) SYNCTEX_INFO(NODE)[SYNCTEX_PAGE_IDX].INT

typedef struct {
	synctex_class_t class;
	synctex_info_t implementation[2+SYNCTEX_PAGE_IDX+1];/* child, sibling
	                         * SYNCTEX_PAGE_IDX */
} synctex_sheet_t;

/*  This macro defines implementation offsets
 *  It is only used for pointer values
 */
#   define SYNCTEX_MAKE_GET(SYNCTEX_GETTER,OFFSET)\
synctex_node_t * SYNCTEX_GETTER (synctex_node_t node) {\
	return node?(synctex_node_t *)((&((node)->implementation))+OFFSET):NULL;\
}
SYNCTEX_MAKE_GET(_synctex_implementation_0,0)
SYNCTEX_MAKE_GET(_synctex_implementation_1,1)
SYNCTEX_MAKE_GET(_synctex_implementation_2,2)
SYNCTEX_MAKE_GET(_synctex_implementation_3,3)
SYNCTEX_MAKE_GET(_synctex_implementation_4,4)

synctex_node_t _synctex_new_sheet(synctex_scanner_t scanner);
void _synctex_display_sheet(synctex_node_t sheet);
void _synctex_log_sheet(synctex_node_t sheet);

static const _synctex_class_t synctex_class_sheet = {
	NULL,                       /* No scanner yet */
	synctex_node_type_sheet,    /* Node type */
	&_synctex_new_sheet,        /* creator */
	&_synctex_free_node,        /* destructor */
	&_synctex_log_sheet,        /* log */
	&_synctex_display_sheet,    /* display */
	NULL,                       /* No parent */
	&_synctex_implementation_0, /* child */
	&_synctex_implementation_1, /* sibling */
	NULL,                       /* No friend */
	(_synctex_info_getter_t)&_synctex_implementation_2  /* info */
};

/*  sheet node creator */
synctex_node_t _synctex_new_sheet(synctex_scanner_t scanner) {
	synctex_node_t node = _synctex_malloc(sizeof(synctex_sheet_t));
	if(node) {
		node->class = scanner?scanner->class+synctex_node_type_sheet:(synctex_class_t)&synctex_class_sheet;
	}
	return node;
}

/*  A box node contains navigation and synctex information
 *  There are different kind of boxes.
 *  Only horizontal boxes are treated differently because of their visible size.
 */
#   define SYNCTEX_TAG_IDX 0
#   define SYNCTEX_LINE_IDX (SYNCTEX_TAG_IDX+1)
#   define SYNCTEX_COLUMN_IDX (SYNCTEX_LINE_IDX+1)
#   define SYNCTEX_HORIZ_IDX (SYNCTEX_COLUMN_IDX+1)
#   define SYNCTEX_VERT_IDX (SYNCTEX_HORIZ_IDX+1)
#   define SYNCTEX_WIDTH_IDX (SYNCTEX_VERT_IDX+1)
#   define SYNCTEX_HEIGHT_IDX (SYNCTEX_WIDTH_IDX+1)
#   define SYNCTEX_DEPTH_IDX (SYNCTEX_HEIGHT_IDX+1)
/*  the corresponding info accessors */
#   define SYNCTEX_TAG(NODE) SYNCTEX_INFO(NODE)[SYNCTEX_TAG_IDX].INT
#   define SYNCTEX_LINE(NODE) SYNCTEX_INFO(NODE)[SYNCTEX_LINE_IDX].INT
#   define SYNCTEX_COLUMN(NODE) SYNCTEX_INFO(NODE)[SYNCTEX_COLUMN_IDX].INT
#   define SYNCTEX_HORIZ(NODE) SYNCTEX_INFO(NODE)[SYNCTEX_HORIZ_IDX].INT
#   define SYNCTEX_VERT(NODE) SYNCTEX_INFO(NODE)[SYNCTEX_VERT_IDX].INT
#   define SYNCTEX_WIDTH(NODE) SYNCTEX_INFO(NODE)[SYNCTEX_WIDTH_IDX].INT
#   define SYNCTEX_HEIGHT(NODE) SYNCTEX_INFO(NODE)[SYNCTEX_HEIGHT_IDX].INT
#   define SYNCTEX_DEPTH(NODE) SYNCTEX_INFO(NODE)[SYNCTEX_DEPTH_IDX].INT

typedef struct {
	synctex_class_t class;
	synctex_info_t implementation[4+SYNCTEX_DEPTH_IDX+1]; /* parent,child,sibling,friend,
						        * SYNCTEX_TAG,SYNCTEX_LINE,SYNCTEX_COLUMN,
								* SYNCTEX_HORIZ,SYNCTEX_VERT,SYNCTEX_WIDTH,SYNCTEX_HEIGHT,SYNCTEX_DEPTH */
} synctex_vert_box_node_t;

synctex_node_t _synctex_new_vbox(synctex_scanner_t scanner);
void _synctex_log_box(synctex_node_t sheet);
void _synctex_display_vbox(synctex_node_t node);

/*  These are static class objects, each scanner will make a copy of them and setup the scanner field.
 */
static const _synctex_class_t synctex_class_vbox = {
	NULL,                       /* No scanner yet */
	synctex_node_type_vbox,     /* Node type */
	&_synctex_new_vbox,         /* creator */
	&_synctex_free_node,        /* destructor */
	&_synctex_log_box,          /* log */
	&_synctex_display_vbox,     /* display */
	&_synctex_implementation_0, /* parent */
	&_synctex_implementation_1, /* child */
	&_synctex_implementation_2, /* sibling */
	&_synctex_implementation_3, /* friend */
	(_synctex_info_getter_t)&_synctex_implementation_4
};

/*  vertical box node creator */
synctex_node_t _synctex_new_vbox(synctex_scanner_t scanner) {
	synctex_node_t node = _synctex_malloc(sizeof(synctex_vert_box_node_t));
	if(node) {
		node->class = scanner?scanner->class+synctex_node_type_vbox:(synctex_class_t)&synctex_class_vbox;
	}
	return node;
}

#   define SYNCTEX_HORIZ_V_IDX (SYNCTEX_DEPTH_IDX+1)
#   define SYNCTEX_VERT_V_IDX (SYNCTEX_HORIZ_V_IDX+1)
#   define SYNCTEX_WIDTH_V_IDX (SYNCTEX_VERT_V_IDX+1)
#   define SYNCTEX_HEIGHT_V_IDX (SYNCTEX_WIDTH_V_IDX+1)
#   define SYNCTEX_DEPTH_V_IDX (SYNCTEX_HEIGHT_V_IDX+1)
/*  the corresponding info accessors */
#   define SYNCTEX_HORIZ_V(NODE) SYNCTEX_INFO(NODE)[SYNCTEX_HORIZ_V_IDX].INT
#   define SYNCTEX_VERT_V(NODE) SYNCTEX_INFO(NODE)[SYNCTEX_VERT_V_IDX].INT
#   define SYNCTEX_WIDTH_V(NODE) SYNCTEX_INFO(NODE)[SYNCTEX_WIDTH_V_IDX].INT
#   define SYNCTEX_HEIGHT_V(NODE) SYNCTEX_INFO(NODE)[SYNCTEX_HEIGHT_V_IDX].INT
#   define SYNCTEX_DEPTH_V(NODE) SYNCTEX_INFO(NODE)[SYNCTEX_DEPTH_V_IDX].INT

/*  Horizontal boxes must contain visible size, because 0 width does not mean emptiness */
typedef struct {
	synctex_class_t class;
	synctex_info_t implementation[4+SYNCTEX_DEPTH_V_IDX+1]; /*parent,child,sibling,friend,
						* SYNCTEX_TAG,SYNCTEX_LINE,SYNCTEX_COLUMN,
						* SYNCTEX_HORIZ,SYNCTEX_VERT,SYNCTEX_WIDTH,SYNCTEX_HEIGHT,SYNCTEX_DEPTH,
						* SYNCTEX_HORIZ_V,SYNCTEX_VERT_V,SYNCTEX_WIDTH_V,SYNCTEX_HEIGHT_V,SYNCTEX_DEPTH_V*/
} synctex_horiz_box_node_t;

synctex_node_t _synctex_new_hbox(synctex_scanner_t scanner);
void _synctex_display_hbox(synctex_node_t node);
void _synctex_log_horiz_box(synctex_node_t sheet);


static const _synctex_class_t synctex_class_hbox = {
	NULL,                       /* No scanner yet */
	synctex_node_type_hbox,     /* Node type */
	&_synctex_new_hbox,         /* creator */
	&_synctex_free_node,        /* destructor */
	&_synctex_log_horiz_box,    /* log */
	&_synctex_display_hbox,     /* display */
	&_synctex_implementation_0, /* parent */
	&_synctex_implementation_1, /* child */
	&_synctex_implementation_2, /* sibling */
	&_synctex_implementation_3, /* friend */
	(_synctex_info_getter_t)&_synctex_implementation_4
};

/*  horizontal box node creator */
synctex_node_t _synctex_new_hbox(synctex_scanner_t scanner) {
	synctex_node_t node = _synctex_malloc(sizeof(synctex_horiz_box_node_t));
	if(node) {
		node->class = scanner?scanner->class+synctex_node_type_hbox:(synctex_class_t)&synctex_class_hbox;
	}
	return node;
}

/*  This void box node implementation is either horizontal or vertical
 *  It does not contain a child field.
 */
typedef struct {
	synctex_class_t class;
	synctex_info_t implementation[3+SYNCTEX_DEPTH_IDX+1]; /* parent,sibling,friend,
	                  * SYNCTEX_TAG,SYNCTEX_LINE,SYNCTEX_COLUMN,
					  * SYNCTEX_HORIZ,SYNCTEX_VERT,SYNCTEX_WIDTH,SYNCTEX_HEIGHT,SYNCTEX_DEPTH*/
} synctex_void_box_node_t;

synctex_node_t _synctex_new_void_vbox(synctex_scanner_t scanner);
void _synctex_log_void_box(synctex_node_t sheet);
void _synctex_display_void_vbox(synctex_node_t node);

static const _synctex_class_t synctex_class_void_vbox = {
	NULL,                       /* No scanner yet */
	synctex_node_type_void_vbox,/* Node type */
	&_synctex_new_void_vbox,    /* creator */
	&_synctex_free_node,        /* destructor */
	&_synctex_log_void_box,     /* log */
	&_synctex_display_void_vbox,/* display */
	&_synctex_implementation_0, /* parent */
	NULL,                       /* No child */
	&_synctex_implementation_1, /* sibling */
	&_synctex_implementation_2, /* friend */
	(_synctex_info_getter_t)&_synctex_implementation_3
};

/*  vertical void box node creator */
synctex_node_t _synctex_new_void_vbox(synctex_scanner_t scanner) {
	synctex_node_t node = _synctex_malloc(sizeof(synctex_void_box_node_t));
	if(node) {
		node->class = scanner?scanner->class+synctex_node_type_void_vbox:(synctex_class_t)&synctex_class_void_vbox;
	}
	return node;
}

synctex_node_t _synctex_new_void_hbox(synctex_scanner_t scanner);
void _synctex_display_void_hbox(synctex_node_t node);

static const _synctex_class_t synctex_class_void_hbox = {
	NULL,                       /* No scanner yet */
	synctex_node_type_void_hbox,/* Node type */
	&_synctex_new_void_hbox,    /* creator */
	&_synctex_free_node,        /* destructor */
	&_synctex_log_void_box,     /* log */
	&_synctex_display_void_hbox,/* display */
	&_synctex_implementation_0, /* parent */
	NULL,                       /* No child */
	&_synctex_implementation_1, /* sibling */
	&_synctex_implementation_2, /* friend */
	(_synctex_info_getter_t)&_synctex_implementation_3
};

/*  horizontal void box node creator */
synctex_node_t _synctex_new_void_hbox(synctex_scanner_t scanner) {
	synctex_node_t node = _synctex_malloc(sizeof(synctex_void_box_node_t));
	if(node) {
		node->class = scanner?scanner->class+synctex_node_type_void_hbox:(synctex_class_t)&synctex_class_void_hbox;
	}
	return node;
}

/*  The medium nodes correspond to kern, glue and math nodes.  */
typedef struct {
	synctex_class_t class;
	synctex_info_t implementation[3+SYNCTEX_WIDTH_IDX+1]; /* parent,sibling,friend,
	                  * SYNCTEX_TAG,SYNCTEX_LINE,SYNCTEX_COLUMN,
					  * SYNCTEX_HORIZ,SYNCTEX_VERT,SYNCTEX_WIDTH */
} synctex_medium_node_t;

synctex_node_t _synctex_new_math(synctex_scanner_t scanner);
void _synctex_log_medium_node(synctex_node_t sheet);
void _synctex_display_math(synctex_node_t node);

static const _synctex_class_t synctex_class_math = {
	NULL,                       /* No scanner yet */
	synctex_node_type_math,     /* Node type */
	&_synctex_new_math,         /* creator */
	&_synctex_free_leaf,        /* destructor */
	&_synctex_log_medium_node,  /* log */
	&_synctex_display_math,     /* display */
	&_synctex_implementation_0, /* parent */
	NULL,                       /* No child */
	&_synctex_implementation_1, /* sibling */
	&_synctex_implementation_2, /* friend */
	(_synctex_info_getter_t)&_synctex_implementation_3
};

/*  math node creator */
synctex_node_t _synctex_new_math(synctex_scanner_t scanner) {
	synctex_node_t node = _synctex_malloc(sizeof(synctex_medium_node_t));
	if(node) {
		node->class = scanner?scanner->class+synctex_node_type_math:(synctex_class_t)&synctex_class_math;
	}
	return node;
}

synctex_node_t _synctex_new_glue(synctex_scanner_t scanner);
void _synctex_display_glue(synctex_node_t node);

static const _synctex_class_t synctex_class_glue = {
	NULL,                       /* No scanner yet */
	synctex_node_type_glue,     /* Node type */
	&_synctex_new_glue,         /* creator */
	&_synctex_free_leaf,        /* destructor */
	&_synctex_log_medium_node,  /* log */
	&_synctex_display_glue,     /* display */
	&_synctex_implementation_0, /* parent */
	NULL,                       /* No child */
	&_synctex_implementation_1, /* sibling */
	&_synctex_implementation_2, /* friend */
	(_synctex_info_getter_t)&_synctex_implementation_3
};
/*  glue node creator */
synctex_node_t _synctex_new_glue(synctex_scanner_t scanner) {
	synctex_node_t node = _synctex_malloc(sizeof(synctex_medium_node_t));
	if(node) {
		node->class = scanner?scanner->class+synctex_node_type_glue:(synctex_class_t)&synctex_class_glue;
	}
	return node;
}

synctex_node_t _synctex_new_kern(synctex_scanner_t scanner);
void _synctex_display_kern(synctex_node_t node);

static const _synctex_class_t synctex_class_kern = {
	NULL,                       /* No scanner yet */
	synctex_node_type_kern,     /* Node type */
	&_synctex_new_kern,         /* creator */
	&_synctex_free_leaf,        /* destructor */
	&_synctex_log_medium_node,  /* log */
	&_synctex_display_kern,     /* display */
	&_synctex_implementation_0, /* parent */
	NULL,                       /* No child */
	&_synctex_implementation_1, /* sibling */
	&_synctex_implementation_2, /* friend */
	(_synctex_info_getter_t)&_synctex_implementation_3
};

/*  kern node creator */
synctex_node_t _synctex_new_kern(synctex_scanner_t scanner) {
	synctex_node_t node = _synctex_malloc(sizeof(synctex_medium_node_t));
	if(node) {
		node->class = scanner?scanner->class+synctex_node_type_kern:(synctex_class_t)&synctex_class_kern;
	}
	return node;
}

#   define SYNCTEX_NAME_IDX (SYNCTEX_TAG_IDX+1)
#   define SYNCTEX_NAME(NODE) SYNCTEX_INFO(NODE)[SYNCTEX_NAME_IDX].PTR

/*  Input nodes only know about their sibling, which is another input node.
 *  The synctex information is the SYNCTEX_TAG and SYNCTEX_NAME*/
typedef struct {
	synctex_class_t class;
	synctex_info_t implementation[1+SYNCTEX_NAME_IDX+1]; /* sibling,
	                          * SYNCTEX_TAG,SYNCTEX_NAME */
} synctex_input_t;

synctex_node_t _synctex_new_input(synctex_scanner_t scanner);
void _synctex_free_input(synctex_node_t node);
void _synctex_display_input(synctex_node_t node);
void _synctex_log_input(synctex_node_t sheet);

static const _synctex_class_t synctex_class_input = {
	NULL,                       /* No scanner yet */
	synctex_node_type_input,    /* Node type */
	&_synctex_new_input,        /* creator */
	&_synctex_free_input,       /* destructor */
	&_synctex_log_input,        /* log */
	&_synctex_display_input,    /* display */
	NULL,                       /* No parent */
	NULL,                       /* No child */
	&_synctex_implementation_0, /* sibling */
	NULL,                       /* No friend */
	(_synctex_info_getter_t)&_synctex_implementation_1
};

synctex_node_t _synctex_new_input(synctex_scanner_t scanner) {
	synctex_node_t node = _synctex_malloc(sizeof(synctex_input_t));
	if(node) {
		node->class = scanner?scanner->class+synctex_node_type_input:(synctex_class_t)&synctex_class_input;
	}
	return node;
}
void _synctex_free_input(synctex_node_t node){
	if(node) {
		SYNCTEX_FREE(SYNCTEX_SIBLING(node));
		free(SYNCTEX_NAME(node));
		free(node);
	}
}
#	ifdef __DARWIN_UNIX03
#       pragma mark -
#       pragma mark Navigation
#   endif
synctex_node_t synctex_node_parent(synctex_node_t node)
{
	return SYNCTEX_PARENT(node);
}
synctex_node_t synctex_node_sheet(synctex_node_t node)
{
	while(node && node->class->type != synctex_node_type_sheet) {
		node = SYNCTEX_PARENT(node);
	}
	/* exit the while loop either when node is NULL or node is a sheet */
	return node;
}
synctex_node_t synctex_node_child(synctex_node_t node)
{
	return SYNCTEX_CHILD(node);
}
synctex_node_t synctex_node_sibling(synctex_node_t node)
{
	return SYNCTEX_SIBLING(node);
}
synctex_node_t synctex_node_next(synctex_node_t node) {
	if(SYNCTEX_CHILD(node)) {
		return SYNCTEX_CHILD(node);
	}
sibling:
	if(SYNCTEX_SIBLING(node)) {
		return SYNCTEX_SIBLING(node);
	}
	if((node = SYNCTEX_PARENT(node)) != NULL) {
		if(node->class->type == synctex_node_type_sheet) {/* EXC_BAD_ACCESS? */
			return NULL;
		}
		goto sibling;
	}
	return NULL;
}
#	ifdef __DARWIN_UNIX03
#       pragma mark -
#       pragma mark CLASS
#   endif

/*  Public node accessor: the type  */
synctex_node_type_t synctex_node_type(synctex_node_t node) {
	if(node) {
		return (((node)->class))->type;
	}
	return synctex_node_type_error;
}

/*  Public node accessor: the human readable type  */
const char * synctex_node_isa(synctex_node_t node) {
static const char * isa[synctex_node_type_last] =
		{"Not a node","sheet","vbox","void vbox","hbox","void hbox","kern","glue","math","input"};
	return isa[synctex_node_type(node)];
}

#	ifdef __DARWIN_UNIX03
#       pragma mark -
#       pragma mark SYNCTEX_LOG
#   endif

#   define SYNCTEX_LOG(NODE) SYNCTEX_MSG_SEND(NODE,log)

/*  Public node logger  */
void synctex_node_log(synctex_node_t node) {
	SYNCTEX_LOG(node);
}

void _synctex_log_sheet(synctex_node_t sheet) {
	if(sheet) {
		printf("%s:%i\n",synctex_node_isa(sheet),SYNCTEX_PAGE(sheet));
		printf("SELF:%p",sheet);
		printf(" SYNCTEX_PARENT:%p",SYNCTEX_PARENT(sheet));
		printf(" SYNCTEX_CHILD:%p",SYNCTEX_CHILD(sheet));
		printf(" SYNCTEX_SIBLING:%p",SYNCTEX_SIBLING(sheet));
		printf(" SYNCTEX_FRIEND:%p\n",SYNCTEX_FRIEND(sheet));
	}
}

void _synctex_log_medium_node(synctex_node_t node) {
	printf("%s:%i,%i:%i,%i:%i\n",
		synctex_node_isa(node),
		SYNCTEX_TAG(node),
		SYNCTEX_LINE(node),
		SYNCTEX_HORIZ(node),
		SYNCTEX_VERT(node),
		SYNCTEX_WIDTH(node));
	printf("SELF:%p",node);
	printf(" SYNCTEX_PARENT:%p",SYNCTEX_PARENT(node));
	printf(" SYNCTEX_CHILD:%p",SYNCTEX_CHILD(node));
	printf(" SYNCTEX_SIBLING:%p",SYNCTEX_SIBLING(node));
	printf(" SYNCTEX_FRIEND:%p\n",SYNCTEX_FRIEND(node));
}

void _synctex_log_void_box(synctex_node_t node) {
	printf("%s",synctex_node_isa(node));
	printf(":%i",SYNCTEX_TAG(node));
	printf(",%i",SYNCTEX_LINE(node));
	printf(",%i",0);
	printf(":%i",SYNCTEX_HORIZ(node));
	printf(",%i",SYNCTEX_VERT(node));
	printf(":%i",SYNCTEX_WIDTH(node));
	printf(",%i",SYNCTEX_HEIGHT(node));
	printf(",%i",SYNCTEX_DEPTH(node));
	printf("\nSELF:%p",node);
	printf(" SYNCTEX_PARENT:%p",SYNCTEX_PARENT(node));
	printf(" SYNCTEX_CHILD:%p",SYNCTEX_CHILD(node));
	printf(" SYNCTEX_SIBLING:%p",SYNCTEX_SIBLING(node));
	printf(" SYNCTEX_FRIEND:%p\n",SYNCTEX_FRIEND(node));
}

void _synctex_log_box(synctex_node_t node) {
	printf("%s",synctex_node_isa(node));
	printf(":%i",SYNCTEX_TAG(node));
	printf(",%i",SYNCTEX_LINE(node));
	printf(",%i",0);
	printf(":%i",SYNCTEX_HORIZ(node));
	printf(",%i",SYNCTEX_VERT(node));
	printf(":%i",SYNCTEX_WIDTH(node));
	printf(",%i",SYNCTEX_HEIGHT(node));
	printf(",%i",SYNCTEX_DEPTH(node));
	printf("\nSELF:%p",node);
	printf(" SYNCTEX_PARENT:%p",SYNCTEX_PARENT(node));
	printf(" SYNCTEX_CHILD:%p",SYNCTEX_CHILD(node));
	printf(" SYNCTEX_SIBLING:%p",SYNCTEX_SIBLING(node));
	printf(" SYNCTEX_FRIEND:%p\n",SYNCTEX_FRIEND(node));
}

void _synctex_log_horiz_box(synctex_node_t node) {
	printf("%s",synctex_node_isa(node));
	printf(":%i",SYNCTEX_TAG(node));
	printf(",%i",SYNCTEX_LINE(node));
	printf(",%i",0);
	printf(":%i",SYNCTEX_HORIZ(node));
	printf(",%i",SYNCTEX_VERT(node));
	printf(":%i",SYNCTEX_WIDTH(node));
	printf(",%i",SYNCTEX_HEIGHT(node));
	printf(",%i",SYNCTEX_DEPTH(node));
	printf(":%i",SYNCTEX_HORIZ_V(node));
	printf(",%i",SYNCTEX_VERT_V(node));
	printf(":%i",SYNCTEX_WIDTH_V(node));
	printf(",%i",SYNCTEX_HEIGHT_V(node));
	printf(",%i",SYNCTEX_DEPTH_V(node));
	printf("\nSELF:%p",node);
	printf(" SYNCTEX_PARENT:%p",SYNCTEX_PARENT(node));
	printf(" SYNCTEX_CHILD:%p",SYNCTEX_CHILD(node));
	printf(" SYNCTEX_SIBLING:%p",SYNCTEX_SIBLING(node));
	printf(" SYNCTEX_FRIEND:%p\n",SYNCTEX_FRIEND(node));
}

void _synctex_log_input(synctex_node_t node) {
	printf("%s",synctex_node_isa(node));
	printf(":%i",SYNCTEX_TAG(node));
	printf(",%s",SYNCTEX_NAME(node));
	printf(" SYNCTEX_SIBLING:%p",SYNCTEX_SIBLING(node));
}

#   define SYNCTEX_DISPLAY(NODE) SYNCTEX_MSG_SEND(NODE,display)

void _synctex_display_sheet(synctex_node_t sheet) {
	if(sheet) {
		printf("....{%i\n",SYNCTEX_PAGE(sheet));
		SYNCTEX_DISPLAY(SYNCTEX_CHILD(sheet));
		printf("....}\n");
		SYNCTEX_DISPLAY(SYNCTEX_SIBLING(sheet));
	}
}

void _synctex_display_vbox(synctex_node_t node) {
	printf("....[%i,%i:%i,%i:%i,%i,%i\n",
		SYNCTEX_TAG(node),
		SYNCTEX_LINE(node),
		SYNCTEX_HORIZ(node),
		SYNCTEX_VERT(node),
		SYNCTEX_WIDTH(node),
		SYNCTEX_HEIGHT(node),
		SYNCTEX_DEPTH(node));
	SYNCTEX_DISPLAY(SYNCTEX_CHILD(node));
	printf("....]\n");
	SYNCTEX_DISPLAY(SYNCTEX_SIBLING(node));
}

void _synctex_display_hbox(synctex_node_t node) {
	printf("....(%i,%i:%i,%i:%i,%i,%i\n",
		SYNCTEX_TAG(node),
		SYNCTEX_LINE(node),
		SYNCTEX_HORIZ(node),
		SYNCTEX_VERT(node),
		SYNCTEX_WIDTH(node),
		SYNCTEX_HEIGHT(node),
		SYNCTEX_DEPTH(node));
	SYNCTEX_DISPLAY(SYNCTEX_CHILD(node));
	printf("....)\n");
	SYNCTEX_DISPLAY(SYNCTEX_SIBLING(node));
}

void _synctex_display_void_vbox(synctex_node_t node) {
	printf("....v%i,%i;%i,%i:%i,%i,%i\n",
		SYNCTEX_TAG(node),
		SYNCTEX_LINE(node),
		SYNCTEX_HORIZ(node),
		SYNCTEX_VERT(node),
		SYNCTEX_WIDTH(node),
		SYNCTEX_HEIGHT(node),
		SYNCTEX_DEPTH(node));
	SYNCTEX_DISPLAY(SYNCTEX_SIBLING(node));
}

void _synctex_display_void_hbox(synctex_node_t node) {
	printf("....h%i,%i:%i,%i:%i,%i,%i\n",
		SYNCTEX_TAG(node),
		SYNCTEX_LINE(node),
		SYNCTEX_HORIZ(node),
		SYNCTEX_VERT(node),
		SYNCTEX_WIDTH(node),
		SYNCTEX_HEIGHT(node),
		SYNCTEX_DEPTH(node));
	SYNCTEX_DISPLAY(SYNCTEX_SIBLING(node));
}

void _synctex_display_glue(synctex_node_t node) {
	printf("....glue:%i,%i:%i,%i\n",
		SYNCTEX_TAG(node),
		SYNCTEX_LINE(node),
		SYNCTEX_HORIZ(node),
		SYNCTEX_VERT(node));
	SYNCTEX_DISPLAY(SYNCTEX_SIBLING(node));
}

void _synctex_display_math(synctex_node_t node) {
	printf("....math:%i,%i:%i,%i\n",
		SYNCTEX_TAG(node),
		SYNCTEX_LINE(node),
		SYNCTEX_HORIZ(node),
		SYNCTEX_VERT(node));
	SYNCTEX_DISPLAY(SYNCTEX_SIBLING(node));
}

void _synctex_display_kern(synctex_node_t node) {
	printf("....kern:%i,%i:%i,%i:%i\n",
		SYNCTEX_TAG(node),
		SYNCTEX_LINE(node),
		SYNCTEX_HORIZ(node),
		SYNCTEX_VERT(node),
		SYNCTEX_WIDTH(node));
	SYNCTEX_DISPLAY(SYNCTEX_SIBLING(node));
}

void _synctex_display_input(synctex_node_t node) {
	printf("....Input:%i:%s\n",
		SYNCTEX_TAG(node),
		SYNCTEX_NAME(node));
	SYNCTEX_DISPLAY(SYNCTEX_SIBLING(node));
}

#	ifdef __DARWIN_UNIX03
#       pragma mark -
#       pragma mark SCANNER
#   endif

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
typedef int synctex_status_t;
/*  When the end of the synctex file has been reached: */
#   define SYNCTEX_STATUS_EOF 0
/*  When the function could not return the value it was asked for: */
#   define SYNCTEX_STATUS_NOT_OK (SYNCTEX_STATUS_EOF+1)
/*  When the function returns the value it was asked for: */
#   define SYNCTEX_STATUS_OK (SYNCTEX_STATUS_NOT_OK+1)
/*  Generic error: */
#   define SYNCTEX_STATUS_ERROR -1
/*  Parameter error: */
#   define SYNCTEX_STATUS_BAD_ARGUMENT -2

#   define SYNCTEX_FILE scanner->file

/*  Actually, the minimum buffer size is driven by integer and float parsing.
 *  ¬±0.123456789e123
 */
#   define SYNCTEX_BUFFER_MIN_SIZE 16
#   define SYNCTEX_BUFFER_SIZE 32768

#include <stdarg.h>

inline static int _synctex_error(char * reason,...) {
	va_list arg;
	int result;
	result = fprintf(stderr,"SyncTeX ERROR: ");
	va_start (arg, reason);
	result += vfprintf(stderr, reason, arg);
	va_end (arg);
	result = fprintf(stderr,"\n");
  return result;
}

/*  Try to ensure that the buffer contains at least size bytes.
 *  Passing a huge size argument means the whole buffer length.
 *  Passing a null size argument means return the available buffer length, without reading the file.
 *  In that case, the return status is always SYNCTEX_STATUS_OK unless the given scanner is NULL,
 *  in which case, SYNCTEX_STATUS_BAD_ARGUMENT is returned.
 *  The value returned in size_ptr is the number of bytes now available in the buffer.
 *  This is a nonnegative integer, it may take the value 0.
 *  It is the responsibility of the caller to test whether this size is conforming to its needs.
 *  Negative values may returned in case of error, actually
 *  when there was an error reading the synctex file. */
synctex_status_t _synctex_buffer_get_available_size(synctex_scanner_t scanner, size_t * size_ptr) {
  	size_t available = 0;
	if(NULL == scanner || NULL == size_ptr) {
		return SYNCTEX_STATUS_BAD_ARGUMENT;
	}
#   define size (* size_ptr)
	if(size>SYNCTEX_BUFFER_SIZE){
		size = SYNCTEX_BUFFER_SIZE;
	}
	available = SYNCTEX_END - SYNCTEX_CUR; /* available is the number of unparsed chars in the buffer */
	if(size<=available) {
		/* There are already sufficiently many characters in the buffer */
		size = available;
		return SYNCTEX_STATUS_OK;
	}
	if(SYNCTEX_FILE) {
		/* Copy the remaining part of the buffer to the beginning,
		 * then read the next part of the file */
		int read = 0;
		if(available) {
			memmove(SYNCTEX_START, SYNCTEX_CUR, available);
		}
		SYNCTEX_CUR = SYNCTEX_START + available; /* the next character after the move, will change. */
		/* Fill the buffer up to its end */
		read = gzread(SYNCTEX_FILE,(void *)SYNCTEX_CUR,SYNCTEX_BUFFER_SIZE - available);
		if(read>0) {
			/*  We assume that 0<read<=SYNCTEX_BUFFER_SIZE - available, such that
			 *  SYNCTEX_CUR + read = SYNCTEX_START + available  + read <= SYNCTEX_START + SYNCTEX_BUFFER_SIZE */
			SYNCTEX_END = SYNCTEX_CUR + read;
			/*  If the end of the file was reached, all the required SYNCTEX_BUFFER_SIZE - available
			 *  may not be filled with values from the file.
			 *  In that case, the buffer should stop properly after read characters. */
			* SYNCTEX_END = '\0';
			SYNCTEX_CUR = SYNCTEX_START;
			size = SYNCTEX_END - SYNCTEX_CUR; /* == old available + read*/
			return SYNCTEX_STATUS_OK; /* May be available is less than size, the caller will have to test. */
		} else if(read<0) {
			/*There is an error */
			_synctex_error("gzread error (1:%i)",read);
			return SYNCTEX_STATUS_ERROR;
		} else {
			/*  Nothing was read, we are at the end of the file. */
			gzclose(SYNCTEX_FILE);
			SYNCTEX_FILE = NULL;
 			SYNCTEX_END = SYNCTEX_CUR;
 			SYNCTEX_CUR = SYNCTEX_START;
			* SYNCTEX_END = '\0';/* Terminate the string properly.*/
			size = SYNCTEX_END - SYNCTEX_CUR;
 			return SYNCTEX_STATUS_EOF; /* there might be a bit of text left */
		}
		/* At this point, the function has already returned from above */
	}
	/* We cannot enlarge the buffer because the end of the file was reached. */
	size = available;
 	return SYNCTEX_STATUS_EOF;
#   undef size
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
synctex_status_t _synctex_next_line(synctex_scanner_t scanner) {
	synctex_status_t status = SYNCTEX_STATUS_OK;
	size_t available = 0;
	if(NULL == scanner) {
		return SYNCTEX_STATUS_BAD_ARGUMENT;
	}
infinite_loop:
	while(SYNCTEX_CUR<SYNCTEX_END) {
		if(*SYNCTEX_CUR == '\n') {
			++SYNCTEX_CUR;
			available = 1;
			return _synctex_buffer_get_available_size(scanner, &available);
		}
		++SYNCTEX_CUR;
	}
	/*  Here, we have SYNCTEX_CUR == SYNCTEX_END, such that the next call to _synctex_buffer_get_available_size
	 *  will read another bunch of synctex file. Little by little, we advance to the end of the file. */
	available = 1;
	status = _synctex_buffer_get_available_size(scanner, &available);
	if(status<=0) {
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
synctex_status_t _synctex_match_string(synctex_scanner_t scanner, const char * the_string) {
	size_t tested_len = 0; /* the number of characters at the beginning of the_string that match */
	size_t remaining_len = 0; /* the number of remaining characters of the_string that should match */
	size_t available = 0;
	synctex_status_t status = 0;
	if(NULL == scanner || NULL == the_string) {
		return SYNCTEX_STATUS_BAD_ARGUMENT;
	}
	remaining_len = strlen(the_string); /* All the_string should match */
	if(0 == remaining_len) {
		return SYNCTEX_STATUS_BAD_ARGUMENT;
	}
	/*  How many characters available in the buffer? */
	available = remaining_len;
	status = _synctex_buffer_get_available_size(scanner,&available);
	if(status<SYNCTEX_STATUS_EOF) {
		return status;
	}
	/*  Maybe we have less characters than expected because the buffer is too small. */
	if(available>=remaining_len) {
		/*  The buffer is sufficiently big to hold the expected number of characters. */
		if(strncmp((char *)SYNCTEX_CUR,the_string,remaining_len)) {
			return SYNCTEX_STATUS_NOT_OK;
		}
return_OK:
		/*  Advance SYNCTEX_CUR to the next character after the_string. */
		SYNCTEX_CUR += remaining_len;
		return SYNCTEX_STATUS_OK;
	} else if(strncmp((char *)SYNCTEX_CUR,the_string,available)) {
			/*  No need to goo further, this is not the expected string in the buffer. */
			return SYNCTEX_STATUS_NOT_OK;
	} else if(SYNCTEX_FILE) {
		/*  The buffer was too small to contain remaining_len characters.
		 *  We have to cut the string into pieces. */
		z_off_t offset = 0L;
		/*  the first part of the string is found, advance the_string to the next untested character. */
		the_string += available;
		/*  update the remaining length and the parsed length. */
		remaining_len -= available;
		tested_len += available;
		SYNCTEX_CUR += available; /* We validate the tested characters. */
		if(0 == remaining_len) {
			/* Nothing left to test, we have found the given string, we return the length. */
			return tested_len;
		}
		/*  We also have to record the current state of the file cursor because
		 *  if the_string does not match, all this should be a totally blank operation,
		 *  for which the file and buffer states should not be modified at all.
		 *  In fact, the states of the buffer before and after this function are in general different
		 *  but they are totally equivalent as long as the values of the buffer before SYNCTEX_CUR
		 *  can be safely discarded.  */
		offset = gztell(SYNCTEX_FILE);
		/*  offset now corresponds to the first character of the file that was not buffered. */
		available = SYNCTEX_CUR - SYNCTEX_START; /* available can be used as temporary placeholder. */
		/*  available now corresponds to the number of chars that where already buffered and
		 *  that match the head of the_string. If in fine the_string does not match, all these chars must be recovered
		 *  because the buffer contents is completely replaced by _synctex_buffer_get_available_size.
		 *  They were buffered from offset-len location in the file. */
		offset -= available;
more_characters:
		/*  There is still some work to be done, so read another bunch of file.
		 *  This is the second call to _synctex_buffer_get_available_size,
		 *  which means that the actual contents of the buffer will be discarded.
		 *  We will definitely have to recover the previous state in case we do not find the expected string. */
		available = remaining_len;
		status = _synctex_buffer_get_available_size(scanner,&available);
		if(status<SYNCTEX_STATUS_EOF) {
			return status; /* This is an error, no need to go further. */
		}
		if(available==0) {
			/* Missing characters: recover the initial state of the file and return. */
return_NOT_OK:
			if(offset != gzseek(SYNCTEX_FILE,offset,SEEK_SET)) {
				/*  This is a critical error, we could not recover the previous state. */
				fprintf(stderr,"SyncTeX critical ERROR: can't seek file\n");
				return SYNCTEX_STATUS_ERROR;
			}
			/*  Next time we are asked to fill the buffer,
			 *  we will read a complete bunch of text from the file. */
			SYNCTEX_CUR = SYNCTEX_END;
			return SYNCTEX_STATUS_NOT_OK;
		}
		if(available<remaining_len) {
			/*  We'll have to loop one more time. */
			if(strncmp((char *)SYNCTEX_CUR,the_string,available)) {
				/* This is not the expected string, recover the previous state and return. */
				goto return_NOT_OK;
			}
			/*  Advance the_string to the first untested character. */
			the_string += available;
			/*  update the remaining length and the parsed length. */
			remaining_len -= available;
			tested_len += available;
			SYNCTEX_CUR += available; /* We validate the tested characters. */
			if(0 == remaining_len) {
				/* Nothing left to test, we have found the given string. */
				return SYNCTEX_STATUS_OK;
			}
			goto more_characters;
		}
		/*  This is the last step. */
		if(strncmp((char *)SYNCTEX_CUR,the_string,remaining_len)) {
			/* This is not the expected string, recover the previous state and return. */
			goto return_NOT_OK;
		}
		goto return_OK;
	} else {
		/* The buffer can't contain the given string argument, and the EOF was reached */
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
synctex_status_t _synctex_decode_int(synctex_scanner_t scanner, int* value_ref) {
	unsigned char * ptr = NULL;
	unsigned char * end = NULL;
	int result = 0;
	size_t available = 0;
	synctex_status_t status = 0;
	if(NULL == scanner) {
		 return SYNCTEX_STATUS_BAD_ARGUMENT;
	}
	available = SYNCTEX_BUFFER_MIN_SIZE;
	status = _synctex_buffer_get_available_size(scanner, &available);
	if(status<SYNCTEX_STATUS_EOF) {
		return status;/* Forward error. */
	}
	if(available==0) {
		return SYNCTEX_STATUS_EOF;/* it is the end of file. */
	}
	ptr = SYNCTEX_CUR;
	if(*ptr==':' || *ptr==',') {
		++ptr;
		--available;
		if(available==0) {
			return SYNCTEX_STATUS_NOT_OK;/* It is not possible to scan an int */
		}
	}
	result = (int)strtol((char *)ptr, (char **)&end, 10);
	if(end>ptr) {
		SYNCTEX_CUR = end;
		if(value_ref) {
			* value_ref = result;
		}
		return SYNCTEX_STATUS_OK;/* Successfully scanned an int */
	}	
	return SYNCTEX_STATUS_NOT_OK;/* Could not scan an int */
}

/*  The purpose of this function is to read a string.
 *  A string is an array of characters from the current parser location
 *  and before the next '\n' character.
 *  If a string was properly decoded, it is returned in value_ref and
 *  the cursor points to the new line marker.
 *  The returned string was alloced on the heap, the caller is the owner and
 *  is responsible to free it in due time.
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
synctex_status_t _synctex_decode_string(synctex_scanner_t scanner, char ** value_ref) {
	unsigned char * end = NULL;
	size_t current_size = 0;
	size_t new_size = 0;
	size_t len = 0;/* The number of bytes to copy */
	size_t available = 0;
	synctex_status_t status = 0;
	if(NULL == scanner || NULL == value_ref) {
		return SYNCTEX_STATUS_BAD_ARGUMENT;
	}
	/* The buffer must at least contain one character: the '\n' end of line marker */
	if(SYNCTEX_CUR>=SYNCTEX_END) {
		available = 1;
		status = _synctex_buffer_get_available_size(scanner,&available);
		if(status < 0) {
			return status;
		}
		if(0 == available) {
			return SYNCTEX_STATUS_EOF;
		}
	}
	/* Now we are sure that there is at least one available character, either because
	 * SYNCTEX_CUR was already < SYNCTEX_END, or because the buffer has been properly filled. */
	/* end will point to the next unparsed '\n' character in the file, when mapped to the buffer. */
	end = SYNCTEX_CUR;
	* value_ref = NULL;/* Initialize, it will be realloc'ed */
	/* We scan all the characters up to the next '\n' */
next_character:
	if(end<SYNCTEX_END) {
		if(*end == '\n') {
			/* OK, we found where to stop */
			len = end - SYNCTEX_CUR;
			if(current_size>UINT_MAX-len-1) {
				/*  But we have reached the limit: we do not have current_size+len+1>UINT_MAX.
				 *  We return the missing amount of memory.
				 *  This will never occur in practice. */
				return UINT_MAX-len-1 - current_size;
			}
			new_size = current_size+len;
			/*  We have current_size+len+1<=UINT_MAX
			 *  or equivalently new_size<UINT_MAX,
			 *  where we have assumed that len<UINT_MAX */
			if((* value_ref = realloc(* value_ref,new_size+1)) != NULL) {
				if(memcpy((*value_ref)+current_size,SYNCTEX_CUR,len)) {
					(* value_ref)[new_size]='\0'; /* Terminate the string */
					SYNCTEX_CUR += len;/* Advance to the terminating '\n' */
					return SYNCTEX_STATUS_OK;
				}
				free(* value_ref);
				* value_ref = NULL;
				_synctex_error("could not copy memory (1).");
				return SYNCTEX_STATUS_ERROR;
			}
			_synctex_error("could not allocate memory (1).");
			return SYNCTEX_STATUS_ERROR;
		} else {
			++end;
			goto next_character;
		}
	} else {
		/* end == SYNCTEX_END */
		len = SYNCTEX_END - SYNCTEX_CUR;
		if(current_size>UINT_MAX-len-1) {
			/*  We have reached the limit. */
			_synctex_error("limit reached (missing %i).",current_size-(UINT_MAX-len-1));
			return SYNCTEX_STATUS_ERROR;
		}
		new_size = current_size+len;
		if((* value_ref = realloc(* value_ref,new_size+1)) != NULL) {
			if(memcpy((*value_ref)+current_size,SYNCTEX_CUR,len)) {
				(* value_ref)[new_size]='\0'; /* Terminate the string */
				SYNCTEX_CUR = SYNCTEX_END;/* Advance the cursor to the end of the bufer */
				return SYNCTEX_STATUS_OK;
			}
			free(* value_ref);
			* value_ref = NULL;
			_synctex_error("could not copy memory (2).");
			return SYNCTEX_STATUS_ERROR;
		}
		/* Huge memory problem */
		_synctex_error("could not allocate memory (2).");
		return SYNCTEX_STATUS_ERROR;
	}
}

/*  Used when parsing the synctex file.
 *  Read an Input record.
 */
synctex_status_t _synctex_scan_input(synctex_scanner_t scanner) {
	synctex_status_t status = 0;
	size_t available = 0;
	synctex_node_t input = NULL;
	if(NULL == scanner) {
		return SYNCTEX_STATUS_BAD_ARGUMENT;
	}
	status = _synctex_match_string(scanner,"Input:");
	if(status<SYNCTEX_STATUS_OK) {
		return status;
	}
	/*  Create a node */
	input = _synctex_new_input(scanner);
	if(NULL == input) {
		_synctex_error("could not create an input node.");
		return SYNCTEX_STATUS_ERROR;
	}
	/*  Decode the synctag  */
	status = _synctex_decode_int(scanner,&(SYNCTEX_TAG(input)));
	if(status<SYNCTEX_STATUS_OK) {
		_synctex_error("bad format of input node.");
		SYNCTEX_FREE(input);
		return status;
	}
	/*  The next character is a field separator, we expect one character in the buffer. */
	available = 1;
	status = _synctex_buffer_get_available_size(scanner, &available);
	if(status<=SYNCTEX_STATUS_ERROR) {
		return status;
	}
	if(0 == available) {
		return SYNCTEX_STATUS_EOF;
	}
	/*  We can now safely advance to the next character, stepping over the field separator. */
	++SYNCTEX_CUR;
	--available;
	/*  Then we scan the file name */
	status = _synctex_decode_string(scanner,&(SYNCTEX_NAME(input)));
	if(status<SYNCTEX_STATUS_OK) {
		SYNCTEX_FREE(input);
		return status;
	}
	/*  Prepend this input node to the input linked list of the scanner */
	SYNCTEX_SET_SIBLING(input,scanner->input);
	scanner->input = input;
	return _synctex_next_line(scanner);/* read the line termination character, if any */
	/*  Now, set up the path */
}

typedef synctex_status_t (*synctex_decoder_t)(synctex_scanner_t,void *);

/*  Used when parsing the synctex file.
 *  Read one of the settings.
 *  On normal completion, returns SYNCTEX_STATUS_OK.
 *  On error, returns SYNCTEX_STATUS_ERROR.
 *  Both arguments must not be NULL.
 *  On return, the scanner points to the next character after the decoded object whatever it is.
 *  It is the responsibility of the caller to prepare the scanner for the next line.
 */
synctex_status_t _synctex_scan_named(synctex_scanner_t scanner,char * name,void * value_ref,synctex_decoder_t decoder) {
	synctex_status_t status = 0;
	if(NULL == scanner || NULL == name || NULL == value_ref || NULL == decoder) {
		return SYNCTEX_STATUS_BAD_ARGUMENT;
	}
not_found:
	status = _synctex_match_string(scanner,name);
	if(status<SYNCTEX_STATUS_NOT_OK) {
		return status;
	} else if(status == SYNCTEX_STATUS_NOT_OK) {
		status = _synctex_next_line(scanner);
		if(status<SYNCTEX_STATUS_OK) {
			return status;
		}
		goto not_found;
	}
	/* A line is found, scan the value */
	return (*decoder)(scanner,value_ref);
}

/*  Used when parsing the synctex file.
 *  Read the preamble.
 */
synctex_status_t _synctex_scan_preamble(synctex_scanner_t scanner) {
	synctex_status_t status = 0;
	if(NULL == scanner) {
		return SYNCTEX_STATUS_BAD_ARGUMENT;
	}
	status = _synctex_scan_named(scanner,"SyncTeX Version:",&(scanner->version),(synctex_decoder_t)&_synctex_decode_int);
	if(status<SYNCTEX_STATUS_OK) {
		return status;
	}
	status = _synctex_next_line(scanner);
	if(status<SYNCTEX_STATUS_OK) {
		return status;
	}
	/*  Read all the input records */
	do {
		status = _synctex_scan_input(scanner);
		if(status<SYNCTEX_STATUS_NOT_OK) {
			return status;
		}
	} while(status == SYNCTEX_STATUS_OK);
	/*  the loop exits when status == SYNCTEX_STATUS_NOT_OK */
	/*  Now read all the required settings. */
	status = _synctex_scan_named(scanner,"Output:",&(scanner->output_fmt),(synctex_decoder_t)&_synctex_decode_string);
	if(status<SYNCTEX_STATUS_NOT_OK) {
		return status;
	}
	status = _synctex_next_line(scanner);
	if(status<SYNCTEX_STATUS_OK) {
		return status;
	}
	status = _synctex_scan_named(scanner,"Magnification:",&(scanner->pre_magnification),(synctex_decoder_t)&_synctex_decode_int);
	if(status<SYNCTEX_STATUS_OK) {
		return status;
	}
	status = _synctex_next_line(scanner);
	if(status<SYNCTEX_STATUS_OK) {
		return status;
	}
	status = _synctex_scan_named(scanner,"Unit:",&(scanner->pre_unit),(synctex_decoder_t)&_synctex_decode_int);
	if(status<SYNCTEX_STATUS_OK) {
		return status;
	}
	status = _synctex_next_line(scanner);
	if(status<SYNCTEX_STATUS_OK) {
		return status;
	}
	status = _synctex_scan_named(scanner,"X Offset:",&(scanner->pre_x_offset),(synctex_decoder_t)&_synctex_decode_int);
	if(status<SYNCTEX_STATUS_OK) {
		return status;
	}
	status = _synctex_next_line(scanner);
	if(status<SYNCTEX_STATUS_OK) {
		return status;
	}
	status = _synctex_scan_named(scanner,"Y Offset:",&(scanner->pre_y_offset),(synctex_decoder_t)&_synctex_decode_int);
	if(status<SYNCTEX_STATUS_OK) {
		return status;
	}
	return _synctex_next_line(scanner);
}

/*  parse a float with a dimension */
synctex_status_t _synctex_scan_float_and_dimension(synctex_scanner_t scanner, float * value_ref) {
	synctex_status_t status = 0;
	unsigned char * endptr = NULL;
	float f = 0;
#ifdef HAVE_SETLOCALE
	char * loc = setlocale(LC_NUMERIC, NULL);
#endif
	size_t available = 0;
	if(NULL == scanner || NULL == value_ref) {
		return SYNCTEX_STATUS_BAD_ARGUMENT;
	}
	available = SYNCTEX_BUFFER_MIN_SIZE;
	status = _synctex_buffer_get_available_size(scanner, &available);
	if(status<SYNCTEX_STATUS_EOF) {
		_synctex_error("problem with float.");
		return status;
	}
#ifdef HAVE_SETLOCALE
	setlocale(LC_NUMERIC, "C");
#endif
	f = strtod((char *)SYNCTEX_CUR,(char **)&endptr);
#ifdef HAVE_SETLOCALE
	setlocale(LC_NUMERIC, loc);
#endif
	if(endptr == SYNCTEX_CUR) {
		_synctex_error("a float was expected.");
		return SYNCTEX_STATUS_ERROR;
	}
	SYNCTEX_CUR = endptr;
	if((status = _synctex_match_string(scanner,"in")) >= SYNCTEX_STATUS_OK) {
		f *= 72.27f*65536;
	} else if(status<SYNCTEX_STATUS_EOF) {
report_unit_error:
		_synctex_error("problem with unit.");
		return status;
	} else if((status = _synctex_match_string(scanner,"cm")) >= SYNCTEX_STATUS_OK) {
		f *= 72.27f*65536/2.54f;
	} else if(status<0) {
		goto report_unit_error;
	} else if((status = _synctex_match_string(scanner,"mm")) >= SYNCTEX_STATUS_OK) {
		f *= 72.27f*65536/25.4f;
	} else if(status<0) {
		goto report_unit_error;
	} else if((status = _synctex_match_string(scanner,"pt")) >= SYNCTEX_STATUS_OK) {
		f *= 65536.0f;
	} else if(status<0) {
		goto report_unit_error;
	} else if((status = _synctex_match_string(scanner,"bp")) >= SYNCTEX_STATUS_OK) {
		f *= 72.27f/72*65536.0f;
	}  else if(status<0) {
		goto report_unit_error;
	} else if((status = _synctex_match_string(scanner,"pc")) >= SYNCTEX_STATUS_OK) {
		f *= 12.0*65536.0f;
	}  else if(status<0) {
		goto report_unit_error;
	} else if((status = _synctex_match_string(scanner,"sp")) >= SYNCTEX_STATUS_OK) {
		f *= 1.0f;
	}  else if(status<0) {
		goto report_unit_error;
	} else if((status = _synctex_match_string(scanner,"dd")) >= SYNCTEX_STATUS_OK) {
		f *= 1238.0f/1157*65536.0f;
	}  else if(status<0) {
		goto report_unit_error;
	} else if((status = _synctex_match_string(scanner,"cc")) >= SYNCTEX_STATUS_OK) {
		f *= 14856.0f/1157*65536;
	} else if(status<0) {
		goto report_unit_error;
	} else if((status = _synctex_match_string(scanner,"nd")) >= SYNCTEX_STATUS_OK) {
		f *= 685.0f/642*65536;
	}  else if(status<0) {
		goto report_unit_error;
	} else if((status = _synctex_match_string(scanner,"nc")) >= SYNCTEX_STATUS_OK) {
		f *= 1370.0f/107*65536;
	} else if(status<0) {
		goto report_unit_error;
	}
	*value_ref = f;
	return SYNCTEX_STATUS_OK;
}

/*  parse the post scriptum
 *  SYNCTEX_STATUS_OK is returned on completion
 *  a negative error is returned otherwise */
synctex_status_t _synctex_scan_post_scriptum(synctex_scanner_t scanner) {
	synctex_status_t status = 0;
	unsigned char * endptr = NULL;
#ifdef HAVE_SETLOCALE
	char * loc = setlocale(LC_NUMERIC, NULL);
#endif
	if(NULL == scanner) {
		return SYNCTEX_STATUS_BAD_ARGUMENT;
	}
	/*  Scan the file until a post scriptum line is found */
post_scriptum_not_found:
	status = _synctex_match_string(scanner,"Post scriptum:");
	if(status<SYNCTEX_STATUS_NOT_OK) {
		return status;
	}
	if(status == SYNCTEX_STATUS_NOT_OK) {
		status = _synctex_next_line(scanner);
		if(status<SYNCTEX_STATUS_EOF) {
			return status;
		} else if(status<SYNCTEX_STATUS_OK) {
			return SYNCTEX_STATUS_OK;/* The EOF is found, we have properly scanned the file */
		}
		goto post_scriptum_not_found;
	}
	/* We found the name, advance to the next line. */
next_line:
	status = _synctex_next_line(scanner);
	if(status<SYNCTEX_STATUS_EOF) {
		return status;
	} else if(status<SYNCTEX_STATUS_OK) {
		return SYNCTEX_STATUS_OK;/* The EOF is found, we have properly scanned the file */
	}
	/* Scanning the information */
	status = _synctex_match_string(scanner,"Magnification:");
	if(status == SYNCTEX_STATUS_OK ) {
#ifdef HAVE_SETLOCALE
		setlocale(LC_NUMERIC, "C");
#endif
		scanner->unit = strtod((char *)SYNCTEX_CUR,(char **)&endptr);
#ifdef HAVE_SETLOCALE
		setlocale(LC_NUMERIC, loc);
#endif
		if(endptr == SYNCTEX_CUR) {
			_synctex_error("bad magnification in the post scriptum, a float was expected.");
			return SYNCTEX_STATUS_ERROR;
		}
		if(scanner->unit<=0) {
			_synctex_error("bad magnification in the post scriptum, a positive float was expected.");
			return SYNCTEX_STATUS_ERROR;
		}
		SYNCTEX_CUR = endptr;
		goto next_line;
	}
	if(status<SYNCTEX_STATUS_EOF){
report_record_problem:
		_synctex_error("Problem reading the Post Scriptum records");
		return status; /* echo the error. */
	}
	status = _synctex_match_string(scanner,"X Offset:");
	if(status == SYNCTEX_STATUS_OK) {
		status = _synctex_scan_float_and_dimension(scanner, &(scanner->x_offset));
		if(status<SYNCTEX_STATUS_OK) {
			_synctex_error("problem with X offset in the Post Scriptum.");
			return status;
		}
		goto next_line;
	} else if(status<SYNCTEX_STATUS_EOF){
		goto report_record_problem;
	}
	status = _synctex_match_string(scanner,"Y Offset:");
	if(status==SYNCTEX_STATUS_OK) {
		status = _synctex_scan_float_and_dimension(scanner, &(scanner->y_offset));
		if(status<SYNCTEX_STATUS_OK) {
			_synctex_error("problem with Y offset in the Post Scriptum.");
			return status;
		}
		goto next_line;
	} else if(status<SYNCTEX_STATUS_EOF){
		goto report_record_problem;
	}
	goto next_line;
}

/*  SYNCTEX_STATUS_OK is returned if the postamble is read
 *  SYNCTEX_STATUS_NOT_OK is returned if the postamble is not at the current location
 *  a negative error otherwise
 *  The postamble comprises the post scriptum section.
 */
int _synctex_scan_postamble(synctex_scanner_t scanner) {
	int status = 0;
	if(NULL == scanner) {
		return SYNCTEX_STATUS_BAD_ARGUMENT;
	}
	status = _synctex_match_string(scanner,"Postamble:");
	if(status < SYNCTEX_STATUS_OK) {
		return status;
	}
count_again:
	status = _synctex_next_line(scanner);
	if(status < SYNCTEX_STATUS_OK) {
		return status;
	}
	status = _synctex_scan_named(scanner,"Count:",&(scanner->count),(synctex_decoder_t)&_synctex_decode_int);
	if(status < SYNCTEX_STATUS_EOF) {
		return status; /* forward the error */
	} else if(status < SYNCTEX_STATUS_OK) { /* No Count record found */
		status = _synctex_next_line(scanner); /* Advance one more line */
		if(status<SYNCTEX_STATUS_OK) {
			return status;
		}
		goto count_again;
	}
	/*  Now we scan the last part of the SyncTeX file: the Post Scriptum section. */
	return _synctex_scan_post_scriptum(scanner);
}

/*  Horizontal boxes also have visible size.
 *  Visible size are bigger than real size.
 *  For example 0 width boxes may contain text.
 *  At creation time, the visible size is set to the values of the real size.
 */
synctex_status_t _synctex_setup_visible_box(synctex_node_t box) {
	if(NULL == box || box->class->type != synctex_node_type_hbox) {
		return SYNCTEX_STATUS_BAD_ARGUMENT;
	}
	if(SYNCTEX_INFO(box) != NULL) {
		SYNCTEX_HORIZ_V(box)  = SYNCTEX_HORIZ(box);
		SYNCTEX_VERT_V(box)   = SYNCTEX_VERT(box);
		SYNCTEX_WIDTH_V(box)  = SYNCTEX_WIDTH(box);
		SYNCTEX_HEIGHT_V(box) = SYNCTEX_HEIGHT(box);
		SYNCTEX_DEPTH_V(box)  = SYNCTEX_DEPTH(box);
		return SYNCTEX_STATUS_OK;
	}
	return SYNCTEX_STATUS_ERROR;
}

/*  This method is sent to an horizontal box to setup the visible size
 *  Some box have 0 width but do contain text material.
 *  With this method, one can enlarge the box to contain the given point (h,v).
 */
synctex_status_t _synctex_horiz_box_setup_visible(synctex_node_t node,int h, int v) {
#	ifdef __DARWIN_UNIX03
#       pragma unused(v)
#   endif
	int itsBtm, itsTop;
	if(NULL == node || node->class->type != synctex_node_type_hbox) {
		return SYNCTEX_STATUS_BAD_ARGUMENT;
	}
	if(SYNCTEX_WIDTH_V(node)<0) {
		itsBtm = SYNCTEX_HORIZ_V(node)+SYNCTEX_WIDTH_V(node);
		itsTop = SYNCTEX_HORIZ_V(node);
		if(h<itsBtm) {
			itsBtm -= h;
			SYNCTEX_WIDTH_V(node) -= itsBtm;
		}
		if(h>itsTop) {
			h -= itsTop;
			SYNCTEX_WIDTH_V(node) -= h;
			SYNCTEX_HORIZ_V(node) += h;
		}
	} else {
		itsBtm = SYNCTEX_HORIZ_V(node);
		itsTop = SYNCTEX_HORIZ_V(node)+SYNCTEX_WIDTH_V(node);
		if(h<itsBtm) {
			itsBtm -= h;
			SYNCTEX_HORIZ_V(node) -= itsBtm;
			SYNCTEX_WIDTH_V(node) += itsBtm;
		}
		if(h>itsTop) {
			h -= itsTop;
			SYNCTEX_WIDTH_V(node) += h;
		}
	}
	return SYNCTEX_STATUS_OK;
}

int synctex_bail(void);

/*  Used when parsing the synctex file.
 *  The parent is a newly created sheet node that will hold the contents.
 *  Something is returned in case of error.
 */
synctex_status_t _synctex_scan_sheet(synctex_scanner_t scanner, synctex_node_t parent) {
	synctex_node_t child = NULL;
	synctex_node_t sibling = NULL;
	int friend_index = 0;
	synctex_info_t * info = NULL;
	int curh, curv;
	synctex_status_t status = 0;
	size_t available = 0;
	if((NULL == scanner) || (NULL == parent)) {
		return SYNCTEX_STATUS_BAD_ARGUMENT;
	}
vertical_loop:
	if(SYNCTEX_CUR<SYNCTEX_END) {
		if(*SYNCTEX_CUR == '[') {
			++SYNCTEX_CUR;
			if(NULL != (child = _synctex_new_vbox(scanner))
					&& NULL != (info = SYNCTEX_INFO(child))) {
#               define SYNCTEX_DECODE_FAILED(WHAT) \
					(_synctex_decode_int(scanner,&(info[WHAT].INT))<SYNCTEX_STATUS_OK)
				if(SYNCTEX_DECODE_FAILED(SYNCTEX_TAG_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_LINE_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_HORIZ_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_VERT_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_WIDTH_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_HEIGHT_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_DEPTH_IDX)
						|| (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK)) {
					_synctex_error("Bad vbox record.");
					return SYNCTEX_STATUS_ERROR;
				}
				SYNCTEX_SET_CHILD(parent,child);
				parent = child;
				child = NULL;
				goto vertical_loop;
			} else {
				_synctex_error("Can't create vbox record.");
				return SYNCTEX_STATUS_ERROR;
			}
		} else if(*SYNCTEX_CUR == ']') {
			++SYNCTEX_CUR;
			if(NULL != parent && parent->class->type == synctex_node_type_vbox) {
				#define SYNCTEX_UPDATE_BOX_FRIEND(NODE)\
				friend_index = ((SYNCTEX_INFO(NODE))[SYNCTEX_TAG_IDX].INT+(SYNCTEX_INFO(NODE))[SYNCTEX_LINE_IDX].INT)%(scanner->number_of_lists);\
				SYNCTEX_SET_FRIEND(NODE,(scanner->lists_of_friends)[friend_index]);\
				(scanner->lists_of_friends)[friend_index] = NODE;
				if(NULL == SYNCTEX_CHILD(parent)) {
					/* only void boxes are friends */
					SYNCTEX_UPDATE_BOX_FRIEND(parent);
				}
				child = parent;
				parent = SYNCTEX_PARENT(child);
			} else {
				_synctex_error("Unexpected ']', ignored.");
			}
			if(_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
				_synctex_error("Uncomplete sheet.");
				return SYNCTEX_STATUS_ERROR;
			}
			goto horizontal_loop;
		} else if(*SYNCTEX_CUR == '(') {
			++SYNCTEX_CUR;
			if(NULL != (child = _synctex_new_hbox(scanner))
					&& NULL != (info = SYNCTEX_INFO(child))) {
				if(SYNCTEX_DECODE_FAILED(SYNCTEX_TAG_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_LINE_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_HORIZ_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_VERT_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_WIDTH_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_HEIGHT_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_DEPTH_IDX)
						|| _synctex_setup_visible_box(child)<SYNCTEX_STATUS_OK
						|| (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK)) {
					_synctex_error("Bad hbox record.");
					return SYNCTEX_STATUS_ERROR;
				}
				SYNCTEX_SET_CHILD(parent,child);
				parent = child;
				child = NULL;
				goto vertical_loop;
			} else {
				_synctex_error("Can't create hbox record.");
				return SYNCTEX_STATUS_ERROR;
			}
		} else if(*SYNCTEX_CUR == ')') {
			++SYNCTEX_CUR;
			if(NULL != parent && parent->class->type == synctex_node_type_hbox) {
				if(NULL == child) {
					SYNCTEX_UPDATE_BOX_FRIEND(parent);
				}
				child = parent;
				parent = SYNCTEX_PARENT(child);
			} else {
				_synctex_error("Unexpected ')', ignored.");
			}
			if(_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
				_synctex_error("Uncomplete sheet.");
				return SYNCTEX_STATUS_ERROR;
			}
			goto horizontal_loop;
		} else if(*SYNCTEX_CUR == 'v') {
			++SYNCTEX_CUR;
			if(NULL != (child = _synctex_new_void_vbox(scanner))
					&& NULL != (info = SYNCTEX_INFO(child))) {
				if(SYNCTEX_DECODE_FAILED(SYNCTEX_TAG_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_LINE_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_HORIZ_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_VERT_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_WIDTH_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_HEIGHT_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_DEPTH_IDX)
						|| (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK)) {
					_synctex_error("Bad void vbox record.");
					return SYNCTEX_STATUS_ERROR;
				}
				SYNCTEX_SET_CHILD(parent,child);
				#define SYNCTEX_UPDATE_FRIEND(NODE)\
				friend_index = (info[SYNCTEX_TAG_IDX].INT+info[SYNCTEX_LINE_IDX].INT)%(scanner->number_of_lists);\
				SYNCTEX_SET_FRIEND(NODE,(scanner->lists_of_friends)[friend_index]);\
				(scanner->lists_of_friends)[friend_index] = NODE;
				SYNCTEX_UPDATE_FRIEND(child);
				goto horizontal_loop;
			} else {
				_synctex_error("Can't create vbox record.");
				return SYNCTEX_STATUS_ERROR;
			}
		} else if(*SYNCTEX_CUR == 'h') {
			++SYNCTEX_CUR;
			if(NULL != (child = _synctex_new_void_hbox(scanner))
					&& NULL != (info = SYNCTEX_INFO(child))) {
				if(SYNCTEX_DECODE_FAILED(SYNCTEX_TAG_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_LINE_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_HORIZ_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_VERT_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_WIDTH_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_HEIGHT_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_DEPTH_IDX)
						|| (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK)) {
					_synctex_error("Bad void hbox record.");
					return SYNCTEX_STATUS_ERROR;
				}
				SYNCTEX_SET_CHILD(parent,child);
				SYNCTEX_UPDATE_FRIEND(child);
				_synctex_horiz_box_setup_visible(parent,synctex_node_h(child),synctex_node_v(child));
				_synctex_horiz_box_setup_visible(parent,synctex_node_h(child)+synctex_node_width(child),synctex_node_v(child));
				goto horizontal_loop;
			} else {
				_synctex_error("Can't create void hbox record.");
				return SYNCTEX_STATUS_ERROR;
			}
		} else if(*SYNCTEX_CUR == 'k') {
			++SYNCTEX_CUR;
			if(NULL != (child = _synctex_new_kern(scanner))
					&& NULL != (info = SYNCTEX_INFO(child))) {
				if(SYNCTEX_DECODE_FAILED(SYNCTEX_TAG_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_LINE_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_HORIZ_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_VERT_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_WIDTH_IDX)
						|| (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK)) {
					_synctex_error("Bad kern record.");
					return SYNCTEX_STATUS_ERROR;
				}
				SYNCTEX_SET_CHILD(parent,child);
				_synctex_horiz_box_setup_visible(parent,synctex_node_h(child),synctex_node_v(child));
				SYNCTEX_UPDATE_FRIEND(child);
				if(NULL == parent) {
					_synctex_error("Missing parent for Child");
					synctex_node_log(child);
					return SYNCTEX_STATUS_ERROR;
				}
				goto horizontal_loop;
			} else {
				_synctex_error("Can't create kern record.");
				return SYNCTEX_STATUS_ERROR;
			}
		} else if(*SYNCTEX_CUR == 'x') {
			++SYNCTEX_CUR;
			if(_synctex_decode_int(scanner,NULL)<SYNCTEX_STATUS_OK
						|| _synctex_decode_int(scanner,NULL)<SYNCTEX_STATUS_OK
						|| _synctex_decode_int(scanner,&curh)<SYNCTEX_STATUS_OK
						|| _synctex_decode_int(scanner,&curv)<SYNCTEX_STATUS_OK
						|| (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK)) {
				_synctex_error("Bad x record.");
				return SYNCTEX_STATUS_ERROR;
			}
			_synctex_horiz_box_setup_visible(parent,curh,curv);
			goto vertical_loop;
		} else if(*SYNCTEX_CUR == 'g') {
			++SYNCTEX_CUR;
			if(NULL != (child = _synctex_new_glue(scanner))
					&& NULL != (info = SYNCTEX_INFO(child))) {
				if(SYNCTEX_DECODE_FAILED(SYNCTEX_TAG_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_LINE_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_HORIZ_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_VERT_IDX)
						|| (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK)) {
					_synctex_error("Bad glue record.");
					return SYNCTEX_STATUS_ERROR;
				}
				SYNCTEX_SET_CHILD(parent,child);
				_synctex_horiz_box_setup_visible(parent,synctex_node_h(child),synctex_node_v(child));
				SYNCTEX_UPDATE_FRIEND(child);
				goto horizontal_loop;
			} else {
				_synctex_error("Can't create glue record.");
				return SYNCTEX_STATUS_ERROR;
			}
		} else if(*SYNCTEX_CUR == '$') {
			++SYNCTEX_CUR;
			if(NULL != (child = _synctex_new_math(scanner))
					&& NULL != (info = SYNCTEX_INFO(child))) {
				if(SYNCTEX_DECODE_FAILED(SYNCTEX_TAG_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_LINE_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_HORIZ_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_VERT_IDX)
						|| (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK)) {
					_synctex_error("Bad math record.");
					return SYNCTEX_STATUS_ERROR;
				}
				SYNCTEX_SET_CHILD(parent,child);
				_synctex_horiz_box_setup_visible(parent,synctex_node_h(child),synctex_node_v(child));
				SYNCTEX_UPDATE_FRIEND(child);
				goto horizontal_loop;
			} else {
				_synctex_error("Can't create math record.");
				return SYNCTEX_STATUS_ERROR;
			}
		} else if(*SYNCTEX_CUR == '}') {
			++SYNCTEX_CUR;
			if(NULL == parent || parent->class->type != synctex_node_type_sheet
					|| (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK)) {
				_synctex_error("Unexpected end of sheet.");
				return SYNCTEX_STATUS_ERROR;
			}
			return SYNCTEX_STATUS_OK;
		} else if(*SYNCTEX_CUR == '!') {
			++SYNCTEX_CUR;
			if(_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
				_synctex_error("Missing anchor.");
				return SYNCTEX_STATUS_ERROR;
			}
			goto vertical_loop;
		} else {
			/* _synctex_error("Ignored record %c\n",*SYNCTEX_CUR); */
			++SYNCTEX_CUR;
			if(_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
				_synctex_error("Unexpected end.");
				return SYNCTEX_STATUS_ERROR;
			}
			goto vertical_loop;
		}
	} else {
		available = 1;
		status = _synctex_buffer_get_available_size(scanner,&available);
		 if(status<SYNCTEX_STATUS_OK && available>0){
			_synctex_error("Uncomplete sheet(0)");
			return SYNCTEX_STATUS_ERROR;
		} else {
			goto vertical_loop;
		}
	}
	synctex_bail();
horizontal_loop:
	if(SYNCTEX_CUR<SYNCTEX_END) {
		if(*SYNCTEX_CUR == '[') {
			++SYNCTEX_CUR;
			if(NULL != (sibling = _synctex_new_vbox(scanner))
					&& NULL != (info = SYNCTEX_INFO(sibling))) {
				if(SYNCTEX_DECODE_FAILED(SYNCTEX_TAG_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_LINE_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_HORIZ_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_VERT_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_WIDTH_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_HEIGHT_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_DEPTH_IDX)
						|| (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK)) {
					_synctex_error("Bad vbox record (2).");
					return SYNCTEX_STATUS_ERROR;
				}
				SYNCTEX_SET_SIBLING(child,sibling);
				parent = sibling;
				child = NULL;
				goto vertical_loop;
			} else {
				_synctex_error("Can't create vbox record (2).");
				return SYNCTEX_STATUS_ERROR;
			}
		} else if(*SYNCTEX_CUR == ']') {
			++SYNCTEX_CUR;
			if(NULL != parent && parent->class->type == synctex_node_type_vbox) {
				if(NULL == child) {
					SYNCTEX_UPDATE_BOX_FRIEND(parent);
				}
				child = parent;
				parent = SYNCTEX_PARENT(child);
			} else {
				_synctex_error("Unexpected end of vbox record (2), ignored.");
			}
			if(_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
				_synctex_error("Unexpected end of file (2).");
				return SYNCTEX_STATUS_ERROR;
			}
			goto horizontal_loop;
		} else if(*SYNCTEX_CUR == '(') {
			++SYNCTEX_CUR;
			if(NULL != (sibling = _synctex_new_hbox(scanner)) &&
					NULL != (info = SYNCTEX_INFO(sibling))) {
				if(SYNCTEX_DECODE_FAILED(SYNCTEX_TAG_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_LINE_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_HORIZ_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_VERT_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_WIDTH_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_HEIGHT_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_DEPTH_IDX)
						|| _synctex_setup_visible_box(sibling)<SYNCTEX_STATUS_OK
						|| (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK)) {
					_synctex_error("Bad hbox record (2).");
					return SYNCTEX_STATUS_ERROR;
				}
				SYNCTEX_SET_SIBLING(child,sibling);
				parent = sibling;
				child = NULL;
				goto vertical_loop;
			} else {
				_synctex_error("Can't create hbox record (2).");
				return SYNCTEX_STATUS_ERROR;
			}
		} else if(*SYNCTEX_CUR == ')') {
			++SYNCTEX_CUR;
			if(NULL != parent && parent->class->type == synctex_node_type_hbox) {
				if(NULL == child) {
					SYNCTEX_UPDATE_BOX_FRIEND(parent);
				}
				child = parent;
				parent = SYNCTEX_PARENT(child);
			} else {
				_synctex_error("Unexpected end of hbox record (2).");
			}
			if(_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
				_synctex_error("Unexpected end of file (2,')').");
				return SYNCTEX_STATUS_ERROR;
			}
			goto horizontal_loop;
		} else if(*SYNCTEX_CUR == 'v') {
			++SYNCTEX_CUR;
			if(NULL != (sibling = _synctex_new_void_vbox(scanner)) &&
					NULL != (info = SYNCTEX_INFO(sibling))) {
				if(SYNCTEX_DECODE_FAILED(SYNCTEX_TAG_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_LINE_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_HORIZ_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_VERT_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_WIDTH_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_HEIGHT_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_DEPTH_IDX)
						|| (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK)) {
					_synctex_error("Bad void vbox record (2).");
					return SYNCTEX_STATUS_ERROR;
				}
				SYNCTEX_SET_SIBLING(child,sibling);
				SYNCTEX_UPDATE_FRIEND(sibling);
				child = sibling;
				goto horizontal_loop;
			} else {
				_synctex_error("can't create void vbox record (2).");
				return SYNCTEX_STATUS_ERROR;
			}
		} else if(*SYNCTEX_CUR == 'h') {
			++SYNCTEX_CUR;
			if(NULL != (sibling = _synctex_new_void_hbox(scanner)) &&
					NULL != (info = SYNCTEX_INFO(sibling))) {
				if(SYNCTEX_DECODE_FAILED(SYNCTEX_TAG_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_LINE_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_HORIZ_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_VERT_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_WIDTH_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_HEIGHT_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_DEPTH_IDX)
						|| (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK)) {
					_synctex_error("Bad void hbox record (2).");
					return SYNCTEX_STATUS_ERROR;
				}
				SYNCTEX_SET_SIBLING(child,sibling);
				SYNCTEX_UPDATE_FRIEND(sibling);
				child = sibling;
				_synctex_horiz_box_setup_visible(parent,synctex_node_h(child),synctex_node_v(child));
				_synctex_horiz_box_setup_visible(parent,synctex_node_h(child)+synctex_node_width(child),synctex_node_v(child));
				goto horizontal_loop;
			} else {
				_synctex_error("can't create void hbox record (2).");
				return SYNCTEX_STATUS_ERROR;
			}
		} else if(*SYNCTEX_CUR == 'x') {
			++SYNCTEX_CUR;
			if(_synctex_decode_int(scanner,NULL)<SYNCTEX_STATUS_OK
						|| _synctex_decode_int(scanner,NULL)<SYNCTEX_STATUS_OK
						|| _synctex_decode_int(scanner,&curh)<SYNCTEX_STATUS_OK
						|| _synctex_decode_int(scanner,&curv)<SYNCTEX_STATUS_OK
						|| (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK)) {
				_synctex_error("Bad x record (2).");
				return SYNCTEX_STATUS_ERROR;
			}
			_synctex_horiz_box_setup_visible(parent,curh,curv);
			goto horizontal_loop;
		} else if(*SYNCTEX_CUR == 'k') {
			++SYNCTEX_CUR;
			if(NULL != (sibling = _synctex_new_kern(scanner))
					&& NULL != (info = SYNCTEX_INFO(sibling))) {
				if(SYNCTEX_DECODE_FAILED(SYNCTEX_TAG_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_LINE_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_HORIZ_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_VERT_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_WIDTH_IDX)
						|| (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK)) {
					_synctex_error("Bad kern record (2).");
					return SYNCTEX_STATUS_ERROR;
				}
				SYNCTEX_SET_SIBLING(child,sibling);
				SYNCTEX_UPDATE_FRIEND(sibling);
				child = sibling;
				_synctex_horiz_box_setup_visible(parent,synctex_node_h(child),synctex_node_v(child));
				goto horizontal_loop;
			} else {
				_synctex_error("Can't create kern record (2).");
				return SYNCTEX_STATUS_ERROR;
			}
		} else if(*SYNCTEX_CUR == 'g') {
			++SYNCTEX_CUR;
			if(NULL != (sibling = _synctex_new_glue(scanner))
					&& NULL != (info = SYNCTEX_INFO(sibling))) {
				if(SYNCTEX_DECODE_FAILED(SYNCTEX_TAG_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_LINE_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_HORIZ_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_VERT_IDX)
						|| (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK)) {
					_synctex_error("Bad glue record (2).");
					return SYNCTEX_STATUS_ERROR;
				}
				SYNCTEX_SET_SIBLING(child,sibling);
				SYNCTEX_UPDATE_FRIEND(sibling);
				child = sibling;
				_synctex_horiz_box_setup_visible(parent,synctex_node_h(child),synctex_node_v(child));
				goto horizontal_loop;
			} else {
				_synctex_error("Can't create glue record (2).");
				return SYNCTEX_STATUS_ERROR;
			}
		} else if(*SYNCTEX_CUR == '$') {
			++SYNCTEX_CUR;
			if(NULL != (sibling = _synctex_new_math(scanner))
					&& NULL != (info = SYNCTEX_INFO(sibling))) {
				if(SYNCTEX_DECODE_FAILED(SYNCTEX_TAG_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_LINE_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_HORIZ_IDX)
						|| SYNCTEX_DECODE_FAILED(SYNCTEX_VERT_IDX)
						|| (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK)) {
					_synctex_error("Bad math record (2).");
					return SYNCTEX_STATUS_ERROR;
				}
				SYNCTEX_SET_SIBLING(child,sibling);
				_synctex_horiz_box_setup_visible(parent,synctex_node_h(sibling),synctex_node_v(sibling));
				SYNCTEX_UPDATE_FRIEND(sibling);
				child = sibling;
				goto horizontal_loop;
			} else {
				_synctex_error("Can't create math record (2).");
				return SYNCTEX_STATUS_ERROR;
			}
		} else if(*SYNCTEX_CUR == '}') {
			++SYNCTEX_CUR;
			if(NULL == parent || parent->class->type != synctex_node_type_sheet
					|| (_synctex_next_line(scanner)<SYNCTEX_STATUS_OK)) {
				_synctex_error("Unexpected end of sheet (2).");
				return SYNCTEX_STATUS_ERROR;
			}
			return SYNCTEX_STATUS_OK;/* This is where we exit normally */
		} else if(*SYNCTEX_CUR == '!') {
			++SYNCTEX_CUR;
			if(_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
				_synctex_error("Missing anchor (2).");
				return SYNCTEX_STATUS_ERROR;
			}
			goto horizontal_loop;
		} else {
			++SYNCTEX_CUR;
			/* _synctex_error("Ignored record %c(2)\n",*SYNCTEX_CUR); */
			if(_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
				return SYNCTEX_STATUS_ERROR;
			}
			goto horizontal_loop;
		}
	} else {
		available = 1;
		status = _synctex_buffer_get_available_size(scanner,&available);
		if(status<SYNCTEX_STATUS_OK && available>0){
			goto horizontal_loop;
		} else {
			_synctex_error("Uncomplete sheet(2)");
			return SYNCTEX_STATUS_ERROR;
		}
	}
#   undef SYNCTEX_DECODE_FAILED
}

/*  Used when parsing the synctex file
 */
synctex_status_t _synctex_scan_content(synctex_scanner_t scanner) {
	synctex_node_t sheet = NULL;
	synctex_status_t status = 0;
	if(NULL == scanner) {
		return SYNCTEX_STATUS_BAD_ARGUMENT;
	}
	/* set up the lists of friends */
	if(NULL == scanner->lists_of_friends) {
		scanner->number_of_lists = 1024;
		scanner->lists_of_friends = (synctex_node_t *)_synctex_malloc(scanner->number_of_lists*sizeof(synctex_node_t));
		if(NULL == scanner->lists_of_friends) {
			_synctex_error("malloc:2");
			return SYNCTEX_STATUS_ERROR;
		}
	}
	/* Find where this section starts */
content_not_found:
	status = _synctex_match_string(scanner,"Content:");
	if(status<SYNCTEX_STATUS_EOF) {
		return status;
	}
	if(_synctex_next_line(scanner)<SYNCTEX_STATUS_OK) {
		_synctex_error("Uncomplete Content.");
		return SYNCTEX_STATUS_ERROR;
	}
	if(status == SYNCTEX_STATUS_NOT_OK) {
		goto content_not_found;
	}
next_sheet:
	if(*SYNCTEX_CUR != '{') {
		status = _synctex_scan_postamble(scanner);
		if(status < SYNCTEX_STATUS_EOF) {
			_synctex_error("Bad content.");
			return status;
		}
		if(status<SYNCTEX_STATUS_OK) {
			status = _synctex_next_line(scanner);
			if(status < SYNCTEX_STATUS_OK) {
				_synctex_error("Bad content.");
				return status;
			}
			goto next_sheet;
		}
		return SYNCTEX_STATUS_OK;
	}
	++SYNCTEX_CUR;
	/* Create a new sheet node */
	sheet = _synctex_new_sheet(scanner);
	status = _synctex_decode_int(scanner,&(SYNCTEX_PAGE(sheet)));
	if(status<SYNCTEX_STATUS_OK) {
		_synctex_error("Missing sheet number.");
bail:
		SYNCTEX_FREE(sheet);
		return SYNCTEX_STATUS_ERROR;
	}
	status = _synctex_next_line(scanner);
	if(status<SYNCTEX_STATUS_OK) {
		_synctex_error("Uncomplete file.");
		goto bail;
	}
	status = _synctex_scan_sheet(scanner,sheet);
	if(status<SYNCTEX_STATUS_OK) {
		_synctex_error("Bad sheet content.");
		goto bail;
	}
	SYNCTEX_SET_SIBLING(sheet,scanner->sheet);
	scanner->sheet = sheet;
	sheet = NULL;
	/* Now read the list of Inputs between 2 sheets. */
	do {
		status = _synctex_scan_input(scanner);
		if(status<SYNCTEX_STATUS_EOF) {
			_synctex_error("Bad input section.");
			goto bail;
		}
	}
	while(status >= SYNCTEX_STATUS_OK);
	goto next_sheet;
}

/*  These are the possible extensions of the synctex file */
static const char * synctex_suffix = ".synctex";
static const char * synctex_suffix_gz = ".gz";

/*  strip the last extension of the given string */
void synctex_strip_last_path_extension(char * string) {
	if(NULL != string){
		char * last_component = NULL;
		char * last_extension = NULL;
		char * next = NULL;
		/* first we find the last path component */
		if(NULL == (last_component = strstr(string,"/"))){
			last_component = string;
		} else {
			++last_component;
			while(NULL != (next = strstr(last_component,"/"))){
				last_component = next+1;
			}
		}
		/* then we find the last path extension */
		if(NULL != (last_extension = strstr(last_component,"."))){
			++last_extension;
			while(NULL != (next = strstr(last_extension,"."))){
				last_extension = next+1;
			}
			--last_extension;/* back to the "."*/
			if(last_extension>last_component){/* filter out paths like ....my/dir/.hidden"*/
				last_extension[0] = '\0';
			}
		}
	}
}

synctex_scanner_t synctex_scanner_new_with_contents_of_file(const char * name);

/*  Where the synctex scanner is created.
 *  output is the full or relative path of the uncompressed or compressed synctex file.
 *  On error, NULL is returned.
 *  This can be due to allocation error, or an internal inconsistency (bad SYNCTEX_BUFFER_SIZE). */
synctex_scanner_t synctex_scanner_new_with_output_file(const char * output) {
	synctex_scanner_t scanner = NULL;
	char * synctex = NULL;
	size_t size = 0;
	/*  Here we assume that int are smaller than void * */
	if(sizeof(int)>sizeof(void*)) {
		fputs("SyncTeX INTERNAL INCONSISTENCY: int's are unexpectedly bigger than pointers, bailing out.",stderr);
		return NULL;
	}
	/*  now create the synctex file name */
	size = strlen(output)+strlen(synctex_suffix)+strlen(synctex_suffix_gz)+1;
	synctex = (char *)malloc(size);
	if(NULL == synctex) {
		fprintf(stderr,"!  synctex_scanner_new_with_output_file: Memory problem (1)\n");
		return NULL;
	}
	/*  we have reserved for synctex enough memory to copy output and both suffices,
	 *  including the terminating character */
	if(synctex != strcpy(synctex,output)) {
		fprintf(stderr,"!  synctex_scanner_new_with_output_file: Copy problem\n");
return_on_error:
		free(synctex);
		return NULL;
	}
	/*  remove the last path extension if any */
	synctex_strip_last_path_extension(synctex);
	if(synctex != strcat(synctex,synctex_suffix)){
		fprintf(stderr,"!  synctex_scanner_new_with_output_file: Concatenation problem (can't add suffix '%s')\n",synctex_suffix);
		goto return_on_error;
	}
	scanner = synctex_scanner_new_with_contents_of_file(synctex);
	if(NULL == scanner) {
		if(synctex != strcat(synctex,synctex_suffix_gz)){
			fprintf(stderr,"!  synctex_scanner_new_with_output_file: Concatenation problem (can't add suffix '%s')\n",synctex_suffix_gz);
			goto return_on_error;
		}
		scanner = synctex_scanner_new_with_contents_of_file(synctex);
	}
	/* make a private copy of output for the scanner */
	if(NULL != scanner) {
		if(NULL == (scanner->output = (char *)malloc(strlen(output)+1))){
			fputs("!  synctex_scanner_new_with_output_file: Memory problem (2)",stderr);
			goto return_on_error;
		}
		if(scanner->output != strcpy(scanner->output,output)) {
			fprintf(stderr,"!  synctex_scanner_new_with_output_file: Copy problem\n");
			goto return_on_error;
		}
	}
	free(synctex);
	return scanner;
}

/*  Where the synctex scanner is created.
 *  name is the full path of the uncompressed or compressed synctex file.
 *  It is not the designated initializer because the scanner's core_path field is not initialized here.
 *  On error, NULL is returned.
 *  This can be due to allocation error, or an internal inconsistency (bad SYNCTEX_BUFFER_SIZE). */
synctex_scanner_t synctex_scanner_new_with_contents_of_file(const char * name) {
	synctex_scanner_t scanner = NULL;
	synctex_status_t status = 0;
	/*  We ensure that SYNCTEX_BUFFER_SIZE < UINT_MAX, I don't know if it makes sense... */
	if(SYNCTEX_BUFFER_SIZE >= UINT_MAX) {
		fprintf(stderr,"SyncTeX BUG: Internal inconsistency, bad SYNCTEX_BUFFER_SIZE (1)");
		return NULL;
	}
	/* for integers: */
	if(SYNCTEX_BUFFER_SIZE < SYNCTEX_BUFFER_MIN_SIZE) {
		fprintf(stderr,"SyncTeX BUG: Internal inconsistency, bad SYNCTEX_BUFFER_SIZE (2)");
		return NULL;
	}
	scanner = (synctex_scanner_t)_synctex_malloc(sizeof(_synctex_scanner_t));
	if(NULL == scanner) {
		fprintf(stderr,"SyncTeX: malloc problem");
		return NULL;
	}
	scanner->pre_magnification = 1000;
	scanner->pre_unit = 8192;
	scanner->pre_x_offset = scanner->pre_y_offset = 578;
	/*  initialize the offset with a fake unprobable value,
	 *  If there is a post scriptum section, this value will be overriden by the real life value */
	scanner->x_offset = scanner->y_offset = 6.027e23f;
	scanner->class[synctex_node_type_sheet] = synctex_class_sheet;
	(scanner->class[synctex_node_type_sheet]).scanner = scanner;
	scanner->class[synctex_node_type_vbox] = synctex_class_vbox;
	(scanner->class[synctex_node_type_vbox]).scanner = scanner;
	scanner->class[synctex_node_type_void_vbox] = synctex_class_void_vbox;
	(scanner->class[synctex_node_type_void_vbox]).scanner = scanner;
	scanner->class[synctex_node_type_hbox] = synctex_class_hbox;
	(scanner->class[synctex_node_type_hbox]).scanner = scanner;
	scanner->class[synctex_node_type_void_hbox] = synctex_class_void_hbox;
	(scanner->class[synctex_node_type_void_hbox]).scanner = scanner;
	scanner->class[synctex_node_type_kern] = synctex_class_kern;
	(scanner->class[synctex_node_type_kern]).scanner = scanner;
	scanner->class[synctex_node_type_glue] = synctex_class_glue;
	(scanner->class[synctex_node_type_glue]).scanner = scanner;
	scanner->class[synctex_node_type_math] = synctex_class_math;
	(scanner->class[synctex_node_type_math]).scanner = scanner;
	scanner->class[synctex_node_type_input] = synctex_class_input;
	(scanner->class[synctex_node_type_input]).scanner = scanner;
	SYNCTEX_FILE = gzopen(name,"r");
	if(NULL == SYNCTEX_FILE) {
		if(errno != ENOENT) {
			fprintf(stderr,"SyncTeX: could not open %s, error %i\n",name,errno);
		}
bail:
		synctex_scanner_free(scanner);
		return NULL;
	}
	SYNCTEX_START = (unsigned char *)malloc(SYNCTEX_BUFFER_SIZE+1); /* one more character for null termination */
	if(NULL == SYNCTEX_START) {
		fprintf(stderr,"SyncTeX: malloc error");
		gzclose(SYNCTEX_FILE);
		goto bail;
	}
	SYNCTEX_END = SYNCTEX_START+SYNCTEX_BUFFER_SIZE;
	/* SYNCTEX_END always points to a null terminating character.
	 * Maybe there is another null terminating character between SYNCTEX_CUR and SYNCTEX_END-1.
	 * At least, we are sure that SYNCTEX_CUR points to a string covering a valid part of the memory. */
	* SYNCTEX_END = '\0';
	SYNCTEX_CUR = SYNCTEX_END;
	status = _synctex_scan_preamble(scanner);
	if(status<SYNCTEX_STATUS_OK) {
		fprintf(stderr,"SyncTeX Error: Bad preamble\n");
bailey:
		gzclose(SYNCTEX_FILE);
		goto bail;
	}
	status = _synctex_scan_content(scanner);
	if(status<SYNCTEX_STATUS_OK) {
		fprintf(stderr,"SyncTeX Error: Bad content\n");
		goto bailey;
	}
	/* Everything is finished, free the buffer, close he file */
	free((void *)SYNCTEX_START);
	SYNCTEX_START = SYNCTEX_CUR = SYNCTEX_END = NULL;
	gzclose(SYNCTEX_FILE);
	SYNCTEX_FILE = NULL;
	/* Final tuning: set the default values for various parameters */
	/* 1 pre_unit = (scanner->pre_unit)/65536 pt = (scanner->pre_unit)/65781.76 bp
	 * 1 pt = 65536 sp */
	if(scanner->pre_unit<=0) {
		scanner->pre_unit = 8192;
	}
	if(scanner->pre_magnification<=0) {
		scanner->pre_magnification = 1000;
	}
	if(scanner->unit <= 0) {
		/* no post magnification */
		scanner->unit = scanner->pre_unit / 65781.76;/* 65781.76 or 65536.0*/
	} else {
		/* post magnification */
		scanner->unit *= scanner->pre_unit / 65781.76;
	}
	scanner->unit *= scanner->pre_magnification / 1000.0;
	if(scanner->x_offset > 6e23) {
		/* no post offset */
		scanner->x_offset = scanner->pre_x_offset * (scanner->pre_unit / 65781.76);
		scanner->y_offset = scanner->pre_y_offset * (scanner->pre_unit / 65781.76);
	} else {
		/* post offset */
		scanner->x_offset /= 65781.76f;
		scanner->y_offset /= 65781.76f;
	}
	return scanner;
	#undef SYNCTEX_FILE
}

/*  The scanner destructor
 */
void synctex_scanner_free(synctex_scanner_t scanner) {
	if(NULL == scanner) {
		return;
	}
	SYNCTEX_FREE(scanner->sheet);
	SYNCTEX_FREE(scanner->input);
	free(SYNCTEX_START);
	free(scanner->output_fmt);
	free(scanner->output);
	free(scanner->lists_of_friends);
	free(scanner);
}

/*  Scanner accessors.
 */
int synctex_scanner_pre_x_offset(synctex_scanner_t scanner){
	return scanner?scanner->pre_x_offset:0;
}
int synctex_scanner_pre_y_offset(synctex_scanner_t scanner){
	return scanner?scanner->pre_y_offset:0;
}
int synctex_scanner_x_offset(synctex_scanner_t scanner){
	return scanner?scanner->x_offset:0;
}
int synctex_scanner_y_offset(synctex_scanner_t scanner){
	return scanner?scanner->y_offset:0;
}
float synctex_scanner_magnification(synctex_scanner_t scanner){
	return scanner?scanner->unit:1;
}
void synctex_scanner_display(synctex_scanner_t scanner) {
	if(NULL == scanner) {
		return;
	}
	printf("The scanner:\noutput:%s\noutput_fmt:%s\nversion:%i\n",scanner->output,scanner->output_fmt,scanner->version);
	printf("pre_unit:%i\nx_offset:%i\ny_offset:%i\n",scanner->pre_unit,scanner->pre_x_offset,scanner->pre_y_offset);
	printf("count:%i\npost_magnification:%f\npost_x_offset:%f\npost_y_offset:%f\n",
		scanner->count,scanner->unit,scanner->x_offset,scanner->y_offset);
	printf("The input:\n");
	SYNCTEX_DISPLAY(scanner->input);
	if(scanner->count<1000) {
		printf("The sheets:\n");
		SYNCTEX_DISPLAY(scanner->sheet);
		printf("The friends:\n");
		if(scanner->lists_of_friends) {
			int i = scanner->number_of_lists;
			synctex_node_t node;
			while(i--) {
				printf("Friend index:%i\n",i);
				node = (scanner->lists_of_friends)[i];
				while(node) {
					printf("%s:%i,%i\n",
						synctex_node_isa(node),
						SYNCTEX_TAG(node),
						SYNCTEX_LINE(node)
					);
					node = SYNCTEX_FRIEND(node);
				}
			}
		}
	} else {
		printf("SyncTeX Warning: Too many objects\n");
	}
}
/*  Public*/
const char * synctex_scanner_get_name(synctex_scanner_t scanner,int tag) {
	synctex_node_t input = NULL;
	if(NULL == scanner) {
		return NULL;
	}
	input = scanner->input;
	do {
		if(tag == SYNCTEX_TAG(input)) {
			return (SYNCTEX_NAME(input));
		}
	} while((input = SYNCTEX_SIBLING(input)) != NULL);
	return NULL;
}
int synctex_scanner_get_tag(synctex_scanner_t scanner,const char * name) {
	synctex_node_t input = NULL;
	if(NULL == scanner) {
		return 0;
	}
	input = scanner->input;
	do {
		if((strlen(name) == strlen((SYNCTEX_NAME(input)))) &&
				(0 == strncmp(name,(SYNCTEX_NAME(input)),strlen(name)))) {
			return SYNCTEX_TAG(input);
		}
	} while((input = SYNCTEX_SIBLING(input)) != NULL);
	return 0;
}
synctex_node_t synctex_scanner_input(synctex_scanner_t scanner) {
	return scanner?scanner->input:NULL;
}
const char * synctex_scanner_get_output_fmt(synctex_scanner_t scanner) {
	return NULL != scanner && scanner->output_fmt?scanner->output_fmt:"";
}
const char * synctex_scanner_get_output(synctex_scanner_t scanner) {
	return NULL != scanner && scanner->output?scanner->output:"";
}
#	ifdef __DARWIN_UNIX03
#       pragma mark -
#       pragma mark Public node attributes
#   endif
float synctex_node_h(synctex_node_t node){
	if(!node) {
		return 0;
	}
	return (float)SYNCTEX_HORIZ(node);
}
float synctex_node_v(synctex_node_t node){
	if(!node) {
		return 0;
	}
	return (float)SYNCTEX_VERT(node);
}
float synctex_node_width(synctex_node_t node){
	if(!node) {
		return 0;
	}
	return (float)SYNCTEX_WIDTH(node);
}
float synctex_node_box_h(synctex_node_t node){
	if(!node) {
		return 0;
	}
	if((node->class->type != synctex_node_type_vbox)
	&& (node->class->type != synctex_node_type_void_vbox)
	&& (node->class->type != synctex_node_type_hbox)
	&& (node->class->type != synctex_node_type_void_hbox)) {
		node = SYNCTEX_PARENT(node);
	}
	return (node->class->type == synctex_node_type_sheet)?0:(float)(SYNCTEX_HORIZ(node));
}
float synctex_node_box_v(synctex_node_t node){
	if(!node) {
		return 0;
	}
	if((node->class->type != synctex_node_type_vbox)
	&& (node->class->type != synctex_node_type_void_vbox)
	&& (node->class->type != synctex_node_type_hbox)
	&& (node->class->type != synctex_node_type_void_hbox)) {
		node = SYNCTEX_PARENT(node);
	}
	return (node->class->type == synctex_node_type_sheet)?0:(float)(SYNCTEX_VERT(node));
}
float synctex_node_box_width(synctex_node_t node){
	if(!node) {
		return 0;
	}
	if((node->class->type != synctex_node_type_vbox)
	&& (node->class->type != synctex_node_type_void_vbox)
	&& (node->class->type != synctex_node_type_hbox)
	&& (node->class->type != synctex_node_type_void_hbox)) {
		node = SYNCTEX_PARENT(node);
	}
	return (node->class->type == synctex_node_type_sheet)?0:(float)(SYNCTEX_WIDTH(node));
}
float synctex_node_box_height(synctex_node_t node){
	if(!node) {
		return 0;
	}
	if((node->class->type != synctex_node_type_vbox)
	&& (node->class->type != synctex_node_type_void_vbox)
	&& (node->class->type != synctex_node_type_hbox)
	&& (node->class->type != synctex_node_type_void_hbox)) {
		node = SYNCTEX_PARENT(node);
	}
	return (node->class->type == synctex_node_type_sheet)?0:(float)(SYNCTEX_HEIGHT(node));
}
float synctex_node_box_depth(synctex_node_t node){
	if(!node) {
		return 0;
	}
	if((node->class->type != synctex_node_type_vbox)
	&& (node->class->type != synctex_node_type_void_vbox)
	&& (node->class->type != synctex_node_type_hbox)
	&& (node->class->type != synctex_node_type_void_hbox)) {
		node = SYNCTEX_PARENT(node);
	}
	return (node->class->type == synctex_node_type_sheet)?0:(float)(SYNCTEX_DEPTH(node));
}
#	ifdef __DARWIN_UNIX03
#       pragma mark -
#       pragma mark Public node visible attributes
#   endif
float synctex_node_visible_h(synctex_node_t node){
	if(!node) {
		return 0;
	}
	return SYNCTEX_HORIZ(node)*node->class->scanner->unit+node->class->scanner->x_offset;
}
float synctex_node_visible_v(synctex_node_t node){
	if(!node) {
		return 0;
	}
	return SYNCTEX_VERT(node)*node->class->scanner->unit+node->class->scanner->y_offset;
}
float synctex_node_visible_width(synctex_node_t node){
	if(!node) {
		return 0;
	}
	return SYNCTEX_WIDTH(node)*node->class->scanner->unit;
}
float synctex_node_box_visible_h(synctex_node_t node){
	if(!node) {
		return 0;
	}
	if((node->class->type == synctex_node_type_vbox)
	|| (node->class->type == synctex_node_type_void_hbox)
	|| (node->class->type == synctex_node_type_void_vbox)) {
result:
		return SYNCTEX_WIDTH(node)<0?
			(SYNCTEX_HORIZ(node)+SYNCTEX_WIDTH(node))*node->class->scanner->unit+node->class->scanner->x_offset:
			SYNCTEX_HORIZ(node)*node->class->scanner->unit+node->class->scanner->x_offset;
	}
	if(node->class->type != synctex_node_type_hbox) {
		node = SYNCTEX_PARENT(node);
	}
	if(node->class->type == synctex_node_type_sheet) {
		return 0;
	}
	if(node->class->type == synctex_node_type_vbox) {
		goto result;
	}
	return SYNCTEX_INFO(node)[SYNCTEX_WIDTH_V_IDX].INT<0?
		(SYNCTEX_INFO(node)[SYNCTEX_HORIZ_V_IDX].INT+SYNCTEX_INFO(node)[SYNCTEX_WIDTH_V_IDX].INT)*node->class->scanner->unit+node->class->scanner->x_offset:
		SYNCTEX_INFO(node)[SYNCTEX_HORIZ_V_IDX].INT*node->class->scanner->unit+node->class->scanner->x_offset;
}
float synctex_node_box_visible_v(synctex_node_t node){
	if(!node) {
		return 0;
	}
	if((node->class->type == synctex_node_type_vbox)
	|| (node->class->type == synctex_node_type_void_hbox)
	|| (node->class->type == synctex_node_type_void_vbox)) {
result:
		return (float)(SYNCTEX_VERT(node))*node->class->scanner->unit+node->class->scanner->y_offset;
	}
	if((node->class->type != synctex_node_type_vbox)
	&& (node->class->type != synctex_node_type_hbox)) {
		node = SYNCTEX_PARENT(node);
	}
	if(node->class->type == synctex_node_type_sheet) {
		return 0;
	}
	if(node->class->type == synctex_node_type_vbox) {
		goto result;
	}
	return SYNCTEX_INFO(node)[SYNCTEX_VERT_V_IDX].INT*node->class->scanner->unit+node->class->scanner->y_offset;
}
float synctex_node_box_visible_width(synctex_node_t node){
	if(!node) {
		return 0;
	}
	if((node->class->type == synctex_node_type_vbox)
	|| (node->class->type == synctex_node_type_void_hbox)
	|| (node->class->type == synctex_node_type_void_vbox)) {
result:
		return SYNCTEX_WIDTH(node)<0?
			-SYNCTEX_WIDTH(node)*node->class->scanner->unit:
			SYNCTEX_WIDTH(node)*node->class->scanner->unit;
	}
	if(node->class->type != synctex_node_type_hbox) {
		node = SYNCTEX_PARENT(node);
	}
	if(node->class->type == synctex_node_type_sheet) {
		return 0;
	}
	if(node->class->type == synctex_node_type_vbox) {
		goto result;
	}
	return SYNCTEX_INFO(node)[SYNCTEX_WIDTH_V_IDX].INT<0?
		-SYNCTEX_INFO(node)[SYNCTEX_WIDTH_V_IDX].INT*node->class->scanner->unit:
		SYNCTEX_INFO(node)[SYNCTEX_WIDTH_V_IDX].INT*node->class->scanner->unit;
}
float synctex_node_box_visible_height(synctex_node_t node){
	if(!node) {
		return 0;
	}
	if((node->class->type == synctex_node_type_vbox)
	|| (node->class->type == synctex_node_type_void_hbox)
	|| (node->class->type == synctex_node_type_void_vbox)) {
result:
		return (float)(SYNCTEX_HEIGHT(node))*node->class->scanner->unit;
	}
	if(node->class->type != synctex_node_type_hbox) {
		node = SYNCTEX_PARENT(node);
	}
	if(node->class->type == synctex_node_type_sheet) {
		return 0;
	}
	if(node->class->type == synctex_node_type_vbox) {
		goto result;
	}
	return SYNCTEX_INFO(node)[SYNCTEX_HEIGHT_V_IDX].INT*node->class->scanner->unit;
}
float synctex_node_box_visible_depth(synctex_node_t node){
	if(!node) {
		return 0;
	}
	if((node->class->type == synctex_node_type_vbox)
	|| (node->class->type == synctex_node_type_void_hbox)
	|| (node->class->type == synctex_node_type_void_vbox)) {
result:
		return (float)(SYNCTEX_DEPTH(node))*node->class->scanner->unit;
	}
	if(node->class->type != synctex_node_type_hbox) {
		node = SYNCTEX_PARENT(node);
	}
	if(node->class->type == synctex_node_type_sheet) {
		return 0;
	}
	if(node->class->type == synctex_node_type_vbox) {
		goto result;
	}
	return SYNCTEX_INFO(node)[SYNCTEX_DEPTH_V_IDX].INT*node->class->scanner->unit;
}
#	ifdef __DARWIN_UNIX03
#       pragma mark -
#       pragma mark Other public node attributes
#   endif

int synctex_node_page(synctex_node_t node){
	synctex_node_t parent = NULL;
	if(!node) {
		return -1;
	}
	parent = SYNCTEX_PARENT(node);
	while(parent) {
		node = parent;
		parent = SYNCTEX_PARENT(node);
	}
	if(node->class->type == synctex_node_type_sheet) {
		return SYNCTEX_PAGE(node);
	}
	return -1;
}
int synctex_node_tag(synctex_node_t node) {
	return node?SYNCTEX_TAG(node):-1;
}
int synctex_node_line(synctex_node_t node) {
	return node?SYNCTEX_LINE(node):-1;
}
int synctex_node_column(synctex_node_t node) {
#	ifdef __DARWIN_UNIX03
#       pragma unused(node)
#   endif
	return -1;
}
#	ifdef __DARWIN_UNIX03
#       pragma mark -
#       pragma mark Query
#   endif

synctex_node_t synctex_sheet_content(synctex_scanner_t scanner,int page) {
	if(scanner) {
		synctex_node_t sheet = scanner->sheet;
		while(sheet) {
			if(page == SYNCTEX_PAGE(sheet)) {
				return SYNCTEX_CHILD(sheet);
			}
			sheet = SYNCTEX_SIBLING(sheet);
		}
	}
	return NULL;
}

int synctex_display_query(synctex_scanner_t scanner,const char * name,int line,int column) {
#	ifdef __DARWIN_UNIX03
#       pragma unused(column)
#   endif
	int tag = synctex_scanner_get_tag(scanner,name);
	size_t size = 0;
	int friend_index = 0;
	synctex_node_t node = NULL;
	if(tag == 0) {
		printf("SyncTeX Warning: No tag for %s\n",name);
		return -1;
	}
	free(SYNCTEX_START);
	SYNCTEX_CUR = SYNCTEX_END = SYNCTEX_START = NULL;
	friend_index = (tag+line)%(scanner->number_of_lists);
	node = (scanner->lists_of_friends)[friend_index];
	while(node) {
		if((tag == SYNCTEX_TAG(node)) && (line == SYNCTEX_LINE(node))) {
			if(SYNCTEX_CUR == SYNCTEX_END) {
				size += 16;
				SYNCTEX_END = realloc(SYNCTEX_START,size*sizeof(synctex_node_t *));
				SYNCTEX_CUR += SYNCTEX_END - SYNCTEX_START;
				SYNCTEX_START = SYNCTEX_END;
				SYNCTEX_END = SYNCTEX_START + size*sizeof(synctex_node_t *);
			}			
			*(synctex_node_t *)SYNCTEX_CUR = node;
			SYNCTEX_CUR += sizeof(synctex_node_t);
		}
		node = SYNCTEX_FRIEND(node);
	}
	SYNCTEX_END = SYNCTEX_CUR;
	SYNCTEX_CUR = NULL;
	return SYNCTEX_END-SYNCTEX_START;
}

int _synctex_node_is_box(synctex_node_t node) {
	switch(synctex_node_type(node)) {
		case synctex_node_type_hbox:
		case synctex_node_type_void_hbox:
		case synctex_node_type_vbox:
		case synctex_node_type_void_vbox:
			return -1;
		default:
			return 0;
	}
}

int _synctex_point_in_visible_box(float h, float v, synctex_node_t node) {
	if(_synctex_node_is_box(node)) {
		v -= synctex_node_box_visible_v(node);
		if((v<=-synctex_node_box_visible_height(node)
					&& v>=synctex_node_box_visible_depth(node))
				|| (v>=-synctex_node_box_visible_height(node)
					&& v<= synctex_node_box_visible_depth(node))) {
			h -= synctex_node_box_visible_h(node);
			if((h<=0 && h>=synctex_node_box_visible_width(node))
					|| (h>=0 && h<=synctex_node_box_visible_width(node))) {
				return -1;
			}
		}
	}
	return 0;
}

int synctex_edit_query(synctex_scanner_t scanner,int page,float h,float v) {
	synctex_node_t sheet = NULL;
	synctex_node_t * start = NULL;
	synctex_node_t * end = NULL;
	synctex_node_t * ptr = NULL;
	size_t size = 0;
	synctex_node_t node = NULL;
	synctex_node_t next = NULL;
	if(NULL == scanner) {
		return 0;
	}
	free(SYNCTEX_START);
	SYNCTEX_START = SYNCTEX_END = SYNCTEX_CUR = NULL;
	sheet = scanner->sheet;
	while(sheet != NULL && SYNCTEX_PAGE(sheet) != page) {
		sheet = SYNCTEX_SIBLING(sheet);
	}
	if(NULL == sheet) {
		return -1;
	}
	/* Now sheet points to the sheet node with proper page number */
	/* Declare memory storage, a buffer to hold found nodes */
	node = SYNCTEX_CHILD(sheet); /* start with the child of the sheet */
has_node_any_child:
	if((next = SYNCTEX_CHILD(node)) != NULL) {
		/* node is a non void box */
		if(_synctex_point_in_visible_box(h,v,node)) {
			/* we found a non void box containing the point */
			if(ptr == end) {
				/* not enough room to store the result, add 16 more node records */
				size += 16;
				end = realloc(start,size*sizeof(synctex_node_t));
				if(end == NULL) {
					return -1;
				}
				ptr += end - start;
				start = end;
				end = start + size;
			}			
			*ptr = node;
			/* Does an included box also contain the hit point?
			 * If this is the case, ptr will be overriden later
			 * This is why we do not increment ptr yet.
			 * ptr will be incremented (registered) later if
			 * no enclosing box contains the hit point */
		}
		node = next;
		goto has_node_any_child;
	}
	/* node has no child */
	if(_synctex_point_in_visible_box(h,v,node)) {
		/* we found a void box containing the hit point */
		if(ptr == end) {
			/* not enough room to store the result, add 16 more node records */
			size += 16;
			end = realloc(start,size*sizeof(synctex_node_t));
			if(end == NULL) {
				return -1;
			}
			ptr += end - start;
			start = end;
			end = start + size*sizeof(synctex_node_t);
		}			
		*ptr = node;
		/* Increment ptr to definitely register the node */
		++ptr;
		/* Ensure that it is the last node */
		*ptr = NULL;
	}
next_sibling:
	if((next = SYNCTEX_SIBLING(node)) != NULL) {
		node = next;
		goto has_node_any_child;
	}
		/* This is the last node at this level
		 * The next step is the parent's sibling */
	next = SYNCTEX_PARENT(node);
	if(ptr && *ptr == next) {
		/* No included box does contain the point
		 * next was already tagged to contain the hit point
		 * but was not fully registered at that time, now we can increment ptr */
		++ptr;
		*ptr = NULL;
	} else if(next == sheet) {
		float best;
		float candidate;
		synctex_node_t * best_node_ref = NULL;
we_are_done:
		end = ptr;
		ptr = NULL;
		/* Up to now, we have found a list of boxes enclosing the hit point. */
		if(end == start) {
			/* No match found */
			return 0;
		}
		/* If there are many different boxes containing the hit point, put the smallest one front.
		 * This is in general the expected box in LaTeX picture environment. */
		ptr = start;
		node = *ptr;
		best = synctex_node_box_visible_width(node);
		while((node = *(++ptr)) != NULL) {
			candidate = synctex_node_box_visible_width(node);
			if(candidate<best) {
				best = candidate;
				best_node_ref = ptr;
			}
		}
		if(best_node_ref) {
			node = *best_node_ref;
			*best_node_ref = *start;
			*start = node;
		}
		/* We do need to check children to find out the node closest to the hit point.
		 * Working with boxes is not very accurate because in general boxes are created asynchronously.
		 * The glue, kern, math are more appropriate for synchronization. */
		if((node = SYNCTEX_CHILD(*start)) != NULL) {
			synctex_node_t best_node = NULL;
			best = HUGE_VAL;
			do {
				switch((node->class)->type) {
					default:
						candidate = fabs(synctex_node_visible_h(node)-h);
						if(candidate<best) {
							best = candidate;
							best_node = node;
						}
					case synctex_node_type_hbox:
					case synctex_node_type_vbox:
						break;
				}			
			} while((node = SYNCTEX_SIBLING(node)) != NULL);
			if(best_node) {
				if((SYNCTEX_START = malloc(sizeof(synctex_node_t))) != NULL) {
					* (synctex_node_t *)SYNCTEX_START = best_node;
					SYNCTEX_END = SYNCTEX_START + sizeof(synctex_node_t);
					SYNCTEX_CUR = NULL;
					free(start);
					return (SYNCTEX_END-SYNCTEX_START)/sizeof(synctex_node_t);
				}
			}
		}
		SYNCTEX_START = (unsigned char *)start;
		SYNCTEX_END = (unsigned char *)end;
		SYNCTEX_CUR = NULL;
		return (SYNCTEX_END-SYNCTEX_START)/sizeof(synctex_node_t);
	} else if(NULL == next) {
		/* What? a node with no parent? */
		_synctex_error("Internal inconsistency, an unexpected node with no parent");
		goto we_are_done;
	}
	node = next;
	goto next_sibling;
}

synctex_node_t synctex_next_result(synctex_scanner_t scanner) {
	if(NULL == SYNCTEX_CUR) {
		SYNCTEX_CUR = SYNCTEX_START;
	} else {
		SYNCTEX_CUR+=sizeof(synctex_node_t);
	}
	if(SYNCTEX_CUR<SYNCTEX_END) {
		return *(synctex_node_t*)SYNCTEX_CUR;
	} else {
		return NULL;
	}
}

int synctex_bail(void) {
		fprintf(stderr,"SyncTeX ERROR\n");
		return -1;
}

#	ifdef __DARWIN_UNIX03
#       pragma mark ===== updater
#   endif

typedef int (*synctex_fprintf_t)(void *, const char * , ...); /* print formatted to either FILE * or gzFile */

#   define SYNCTEX_BITS_PER_BYTE 8

struct __synctex_updater_t {
    void *file;                 /*  the foo.synctex or foo.synctex.gz I/O identifier  */
	synctex_fprintf_t fprintf;  /*  either fprintf or gzprintf */
	int length;                 /*  the number of chars appended */
    struct _flags {
        unsigned int no_gz:1;   /*  Whether zlib is used or not */
        unsigned int reserved:SYNCTEX_BITS_PER_BYTE*sizeof(int)-1; /* Align */
	} flags;
};
#   define SYNCTEX_FILE updater->file
#   define SYNCTEX_NO_GZ ((updater->flags).no_gz)
#   define SYNCTEX_fprintf (*(updater->fprintf))
#   define SYNCTEX_YES (-1)
#   define SYNCTEX_NO  (0)

synctex_updater_t synctex_updater_new_with_output_file(const char * output){
	synctex_updater_t updater = NULL;
	char * synctex = NULL;
	size_t size = 0;
	/* prepare the updater */
	updater = (synctex_updater_t)_synctex_malloc(sizeof(synctex_updater_t));
	if(NULL == updater) {
		fprintf(stderr,"!  synctex_updater_new_with_file: malloc problem");
		return NULL;
	}
	size = strlen(output)+strlen(synctex_suffix)+strlen(synctex_suffix_gz)+1;
	synctex = (char *)malloc(size);
	if(NULL == synctex) {
		fprintf(stderr,"!  synctex_updater_new_with_output_file: Memory problem (1)\n");
return_on_error1:
		free(updater);
		return NULL;
	}
	/*  we have reserved for synctex enough memory to copy output and both suffices,
	 *  including the terminating character */
	if(synctex != strcpy(synctex,output)) {
		fprintf(stderr,"!  synctex_updater_new_with_output_file: Copy problem\n");
return_on_error2:
		free(synctex);
		goto return_on_error1;
	}
	/*  remove the last path extension if any */
	synctex_strip_last_path_extension(synctex);
	/*  append the synctex suffix */
	if(synctex != strcat(synctex,synctex_suffix)){
		fprintf(stderr,"!  synctex_scanner_new_with_output_file: Concatenation problem (can't add suffix '%s')\n",synctex_suffix);
		goto return_on_error2;
	}
	if(NULL != (SYNCTEX_FILE = fopen(synctex,"r"))){
		/* OK, the file exists */
		fclose(SYNCTEX_FILE);
		if(NULL == (SYNCTEX_FILE = (void *)fopen(synctex,"a"))) {
no_write_error:
			fprintf(stderr,"!  synctex_updater_new_with_file: Can't append to %s",synctex);
			goto return_on_error2;
		}
		SYNCTEX_NO_GZ = SYNCTEX_YES;
		updater->fprintf = (synctex_fprintf_t)(&fprintf);
return_updater:
		printf("SyncTeX: updating %s...",synctex);
		free(synctex);
		return updater;
	}
	/*  append the gz suffix */
	if(synctex != strcat(synctex,synctex_suffix_gz)){
		fprintf(stderr,"!  synctex_scanner_new_with_output_file: Concatenation problem (can't add suffix '%s')\n",synctex_suffix_gz);
		goto return_on_error2;
	}
	if(NULL != (SYNCTEX_FILE = gzopen(synctex,"r"))){
		gzclose(SYNCTEX_FILE);
		if(NULL == (SYNCTEX_FILE = gzopen(synctex,"a"))) {
			goto no_write_error;
		}
		SYNCTEX_NO_GZ = SYNCTEX_NO;
		updater->fprintf = (synctex_fprintf_t)(&gzprintf);
		goto return_updater;
	}
	goto return_on_error2;
}

void synctex_updater_append_magnification(synctex_updater_t updater, char * magnification){
	if(NULL==updater) {
		return;
	}
	if(magnification && strlen(magnification)) {
		updater->length += SYNCTEX_fprintf(SYNCTEX_FILE,"Magnification:%s\n",magnification);
	}
}

void synctex_updater_append_x_offset(synctex_updater_t updater, char * x_offset){
	if(NULL==updater) {
		return;
	}
	if(x_offset && strlen(x_offset)) {
		updater->length += SYNCTEX_fprintf(SYNCTEX_FILE,"X Offset:%s\n",x_offset);
	}
}

void synctex_updater_append_y_offset(synctex_updater_t updater, char * y_offset){
	if(NULL==updater) {
		return;
	}
	if(y_offset && strlen(y_offset)) {
		updater->length += SYNCTEX_fprintf(SYNCTEX_FILE,"Y Offset:%s\n",y_offset);
	}
}

void synctex_updater_free(synctex_updater_t updater){
	if(NULL==updater) {
		return;
	}
	if(updater->length>0) {
		SYNCTEX_fprintf(SYNCTEX_FILE,"!%i\n",updater->length);
	}
	if (SYNCTEX_NO_GZ) {
		fclose((FILE *)SYNCTEX_FILE);
	} else {
		gzclose((gzFile)SYNCTEX_FILE);
	}
	free(updater);
	printf("... done.\n");
	return;
}


#endif