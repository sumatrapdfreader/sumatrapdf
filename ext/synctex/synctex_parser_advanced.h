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
 */

#include "synctex_parser.h"
#include "synctex_parser_utils.h"

#ifndef __SYNCTEX_PARSER_PRIVATE__
#   define __SYNCTEX_PARSER_PRIVATE__

#ifdef __cplusplus
extern "C" {
#endif
    /*  Reminder that the argument must not be NULL */
    typedef synctex_node_p synctex_non_null_node_p;

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
    
    /**
     *  These are the masks for the synctex node types.
     *  int's are 32 bits at leats.
     */
    enum {
        synctex_shift_root,
        synctex_shift_no_root,
        synctex_shift_void,
        synctex_shift_no_void,
        synctex_shift_box,
        synctex_shift_no_box,
        synctex_shift_proxy,
        synctex_shift_no_proxy,
        synctex_shift_h,
        synctex_shift_v
    };
    enum {
        synctex_mask_root      = 1,
        synctex_mask_no_root   = synctex_mask_root<<1,
        synctex_mask_void      = synctex_mask_no_root<<1,
        synctex_mask_no_void   = synctex_mask_void<<1,
        synctex_mask_box       = synctex_mask_no_void<<1,
        synctex_mask_no_box    = synctex_mask_box<<1,
        synctex_mask_proxy     = synctex_mask_no_box<<1,
        synctex_mask_no_proxy  = synctex_mask_proxy<<1,
        synctex_mask_h         = synctex_mask_no_proxy<<1,
        synctex_mask_v         = synctex_mask_h<<1,
    };
    enum {
        synctex_mask_non_void_hbox = synctex_mask_no_void
        | synctex_mask_box
        | synctex_mask_h,
        synctex_mask_non_void_vbox = synctex_mask_no_void
        | synctex_mask_box
        | synctex_mask_v
    };
    typedef enum {
        synctex_node_mask_sf =
        synctex_mask_root
        |synctex_mask_no_void
        |synctex_mask_no_box
        |synctex_mask_no_proxy,
        synctex_node_mask_vbox =
        synctex_mask_no_root
        |synctex_mask_no_void
        |synctex_mask_box
        |synctex_mask_no_proxy
        |synctex_mask_v,
        synctex_node_mask_hbox =
        synctex_mask_no_root
        |synctex_mask_no_void
        |synctex_mask_box
        |synctex_mask_no_proxy
        |synctex_mask_h,
        synctex_node_mask_void_vbox =
        synctex_mask_no_root
        |synctex_mask_void
        |synctex_mask_box
        |synctex_mask_no_proxy
        |synctex_mask_v,
        synctex_node_mask_void_hbox =
        synctex_mask_no_root
        |synctex_mask_void
        |synctex_mask_box
        |synctex_mask_no_proxy
        |synctex_mask_h,
        synctex_node_mask_vbox_proxy =
        synctex_mask_no_root
        |synctex_mask_no_void
        |synctex_mask_box
        |synctex_mask_proxy
        |synctex_mask_v,
        synctex_node_mask_hbox_proxy =
        synctex_mask_no_root
        |synctex_mask_no_void
        |synctex_mask_box
        |synctex_mask_proxy
        |synctex_mask_h,
        synctex_node_mask_nvnn =
        synctex_mask_no_root
        |synctex_mask_void
        |synctex_mask_no_box
        |synctex_mask_no_proxy,
        synctex_node_mask_input =
        synctex_mask_root
        |synctex_mask_void
        |synctex_mask_no_box
        |synctex_mask_no_proxy,
        synctex_node_mask_proxy =
        synctex_mask_no_root
        |synctex_mask_void
        |synctex_mask_no_box
        |synctex_mask_proxy
    } synctex_node_mask_t;

    enum {
        /* input */
        synctex_tree_sibling_idx        =  0,
        synctex_tree_s_input_max        =  1,
        /* All */
        synctex_tree_s_parent_idx       =  1,
        synctex_tree_sp_child_idx       =  2,
        synctex_tree_spc_friend_idx     =  3,
        synctex_tree_spcf_last_idx      =  4,
        synctex_tree_spcfl_vbox_max     =  5,
        /* hbox supplement */
        synctex_tree_spcfl_next_hbox_idx  =  5,
        synctex_tree_spcfln_hbox_max      =  6,
        /* hbox proxy supplement */
        synctex_tree_spcfln_target_idx        =  6,
        synctex_tree_spcflnt_proxy_hbox_max   =  7,
        /* vbox proxy supplement */
        synctex_tree_spcfl_target_idx         =  5,
        synctex_tree_spcflt_proxy_vbox_max    =  6,
        /*  spf supplement*/
        synctex_tree_sp_friend_idx  =  2,
        synctex_tree_spf_max        =  3,
        /*  box boundary supplement */
        synctex_tree_spf_arg_sibling_idx   =  3,
        synctex_tree_spfa_max              =  4,
        /*  proxy supplement */
        synctex_tree_spf_target_idx    =  3,
        synctex_tree_spft_proxy_max    =  4,
        /*  last proxy supplement */
        synctex_tree_spfa_target_idx      =  4,
        synctex_tree_spfat_proxy_last_max =  5,
        /* sheet supplement */
        synctex_tree_s_child_idx        =  1,
        synctex_tree_sc_next_hbox_idx   =  2,
        synctex_tree_scn_sheet_max      =  3,
        /* form supplement */
        synctex_tree_sc_target_idx      =  2,
        synctex_tree_sct_form_max       =  3,
        /* spct */
        synctex_tree_spc_target_idx     =  3,
        synctex_tree_spct_handle_max    =  4,
    };
    
    enum {
        /* input */
        synctex_data_input_tag_idx  =  0,
        synctex_data_input_line_idx =  1,
        synctex_data_input_name_idx =  2,
        synctex_data_input_tln_max  =  3,
        /* sheet */
        synctex_data_sheet_page_idx =  0,
        synctex_data_p_sheet_max    =  1,
        /* form */
        synctex_data_form_tag_idx   =  0,
        synctex_data_t_form_max     =  1,
        /* tlchv */
        synctex_data_tag_idx        =  0,
        synctex_data_line_idx       =  1,
        synctex_data_column_idx     =  2,
        synctex_data_h_idx          =  3,
        synctex_data_v_idx          =  4,
        synctex_data_tlchv_max      =  5,
        /* tlchvw */
        synctex_data_width_idx      =  5,
        synctex_data_tlchvw_max     =  6,
        /* box */
        synctex_data_height_idx     =  6,
        synctex_data_depth_idx      =  7,
        synctex_data_box_max        =  8,
        /* hbox supplement */
        synctex_data_mean_line_idx  =  8,
        synctex_data_weight_idx     =  9,
        synctex_data_h_V_idx        = 10,
        synctex_data_v_V_idx        = 11,
        synctex_data_width_V_idx    = 12,
        synctex_data_height_V_idx   = 13,
        synctex_data_depth_V_idx    = 14,
        synctex_data_hbox_max       = 15,
        /* ref */
        synctex_data_ref_tag_idx    =  0,
        synctex_data_ref_h_idx      =  1,
        synctex_data_ref_v_idx      =  2,
        synctex_data_ref_thv_max    =  3,
        /* proxy */
        synctex_data_proxy_h_idx    =  0,
        synctex_data_proxy_v_idx    =  1,
        synctex_data_proxy_hv_max   =  2,
        /* handle */
        synctex_data_handle_w_idx   =  0,
        synctex_data_handle_w_max   =  1,
    };

    /*  each synctex node has a class */
    typedef struct synctex_class_t synctex_class_s;
    typedef synctex_class_s * synctex_class_p;
    
    
    /*  synctex_node_p is a pointer to a node
     *  synctex_node_s is the target of the synctex_node_p pointer
     *  It is a pseudo object oriented program.
     *  class is a pointer to the class object the node belongs to.
     *  implementation is meant to contain the private data of the node
     *  basically, there are 2 kinds of information: navigation information and
     *  synctex information. Both will depend on the type of the node,
     *  thus different nodes will have different private data.
     *  There is no inheritancy overhead.
     */
    typedef union {
        synctex_node_p as_node;
        int    as_integer;
        char * as_string;
        void * as_pointer;
    } synctex_data_u;
    typedef synctex_data_u * synctex_data_p;
    
#   if defined(SYNCTEX_USE_CHARINDEX)
    typedef unsigned int synctex_charindex_t;
    synctex_charindex_t synctex_node_charindex(synctex_node_p node);
    typedef synctex_charindex_t synctex_lineindex_t;
    synctex_lineindex_t synctex_node_lineindex(synctex_node_p node);
    synctex_node_p synctex_scanner_handle(synctex_scanner_p scanner);
#       define SYNCTEX_DECLARE_CHARINDEX \
            synctex_charindex_t char_index;\
            synctex_lineindex_t line_index;
#       define SYNCTEX_DECLARE_CHAR_OFFSET \
            synctex_charindex_t charindex_offset;
#   else
#       define SYNCTEX_DECLARE_CHARINDEX
#       define SYNCTEX_DECLARE_CHAR_OFFSET
#   endif
    struct synctex_node_t {
        SYNCTEX_DECLARE_CHARINDEX
        synctex_class_p class_;
#ifdef DEBUG
        synctex_data_u data[22];
#else
        synctex_data_u data[1];
#endif
    };
    
    typedef synctex_node_p * synctex_node_r;
    
    typedef struct {
        int h;
        int v;
    } synctex_point_s;
    
    typedef synctex_point_s * synctex_point_p;
    
    typedef struct {
        synctex_point_s min;   /* top left */
        synctex_point_s max;   /* bottom right */
    } synctex_box_s;
    
    typedef synctex_box_s * synctex_box_p;
    /**
     *  These are the types of the synctex nodes.
     *  No need to use them but the compiler needs them here.
     *  There are 3 kinds of nodes.
     *  - primary nodes
     *  - proxies
     *  - handles
     *  Primary nodes are created at parse time
     *  of the synctex file.
     *  Proxies are used to support pdf forms.
     *  The ref primary nodes are replaced by a tree
     *  of proxy nodes which duplicate the tree of primary
     *  nodes available in the refered form.
     *  Roughly speaking, the primary nodes of the form
     *  know what to display, the proxy nodes know where.
     *  Handles are used in queries. They point to either
     *  primary nodes or proxies.
     */
    typedef enum {
        synctex_node_type_none = 0,
        synctex_node_type_input,
        synctex_node_type_sheet,
        synctex_node_type_form,
        synctex_node_type_ref,
        synctex_node_type_vbox,
        synctex_node_type_void_vbox,
        synctex_node_type_hbox,
        synctex_node_type_void_hbox,
        synctex_node_type_kern,
        synctex_node_type_glue,
        synctex_node_type_rule,
        synctex_node_type_math,
        synctex_node_type_boundary,
        synctex_node_type_box_bdry,
        synctex_node_type_proxy,
        synctex_node_type_proxy_last,
        synctex_node_type_proxy_vbox,
        synctex_node_type_proxy_hbox,
        synctex_node_type_handle,
        synctex_node_number_of_types
    } synctex_node_type_t;
    /*  synctex_node_type gives the type of a given node,
     *  synctex_node_isa gives the same information as a human readable text. */
    synctex_node_type_t synctex_node_type(synctex_node_p node);
    const char * synctex_node_isa(synctex_node_p node);
    
    synctex_node_type_t synctex_node_target_type(synctex_node_p node);

    synctex_node_type_t synctex_node_type(synctex_node_p node);
    const char * synctex_node_isa(synctex_node_p node);
    
    void synctex_node_log(synctex_node_p node);
    void synctex_node_display(synctex_node_p node);
    
    /*  Given a node, access to the location in the synctex file where it is defined.
     */

    int synctex_node_form_tag(synctex_node_p node);
    
    int synctex_node_weight(synctex_node_p node);
    int synctex_node_child_count(synctex_node_p node);
    
    int synctex_node_h(synctex_node_p node);
    int synctex_node_v(synctex_node_p node);
    int synctex_node_width(synctex_node_p node);
    
    int synctex_node_box_h(synctex_node_p node);
    int synctex_node_box_v(synctex_node_p node);
    int synctex_node_box_width(synctex_node_p node);
    int synctex_node_box_height(synctex_node_p node);
    int synctex_node_box_depth(synctex_node_p node);
    
    int synctex_node_hbox_h(synctex_node_p node);
    int synctex_node_hbox_v(synctex_node_p node);
    int synctex_node_hbox_width(synctex_node_p node);
    int synctex_node_hbox_height(synctex_node_p node);
    int synctex_node_hbox_depth(synctex_node_p node);
    
    synctex_scanner_p synctex_scanner_new(void);
    synctex_node_p synctex_node_new(synctex_scanner_p scanner,synctex_node_type_t type);

    /**
     *  Scanner display switcher getter.
     *  If the switcher is 0, synctex_node_display is disabled.
     *  If the switcher is <0, synctex_node_display has no limit.
     *  If the switcher is >0, only the first switcher (as number) nodes are displayed.
     *  - parameter: a scanner
     *  - returns: an integer
     */
    int synctex_scanner_display_switcher(synctex_scanner_p scanR);
    void synctex_scanner_set_display_switcher(synctex_scanner_p scanR, int switcher);

    /**
     *  Iterator is the structure used to traverse
     *  the answer to client queries.
     *  First answers are the best matches, according
     *  to criteria explained below.
     *  Next answers are not ordered.
     *  Objects are handles to nodes in the synctex node tree starting at scanner.
     */
    typedef struct synctex_iterator_t synctex_iterator_s;
    typedef synctex_iterator_s * synctex_iterator_p;

    /**
     *  Designated creator for a display query, id est,
     *  forward navigation from source to output.
     *  Returns NULL if the query has no answer.
     *  Code example:
     *      synctex_iterator_p iterator = NULL;
     *      if ((iterator = synctex_iterator_new_display(...)) {
     *      synctex_node_p node = NULL;
     *      while((node = synctex_iterator_next_result(iterator))) {
     *          do something with node...
     *      }
     */
    synctex_iterator_p synctex_iterator_new_display(synctex_scanner_p scanner,const char *  name,int line,int column, int page_hint);
    /**
     *  Designated creator for an  edit query, id est,
     *  backward navigation from output to source.
     *  Code example:
     *      synctex_iterator_p iterator = NULL;
     *      if ((iterator = synctex_iterator_new_edit(...)) {
     *      synctex_node_p node = NULL;
     *      while((node = synctex_iterator_next_result(iterator))) {
     *          do something with node...
     *      }
     */
    synctex_iterator_p synctex_iterator_new_edit(synctex_scanner_p scanner,int page,float h,float v);
    /**
     *  Free all the resources.
     *  - argument iterator: the object to free...
     *  You should free the iterator before the scanner
     *  owning the nodes it iterates with.
     */
    void synctex_iterator_free(synctex_iterator_p iterator);
    /**
     *  Whether the iterator actually points to an object.
     *  - argument iterator: the object to iterate on...
     */
    synctex_bool_t synctex_iterator_has_next(synctex_iterator_p iterator);
    /**
     *  Returns the pointed object and advance the cursor
     *  to the next object. Returns NULL and does nothing
     *  if the end has already been reached.
     *  - argument iterator: the object to iterate on...
     */
    synctex_node_p synctex_iterator_next_result(synctex_iterator_p iterator);
    /**
     *  Reset the cursor position to the first result.
     *  - argument iterator: the object to iterate on...
     */
    int synctex_iterator_reset(synctex_iterator_p iterator);
    /**
     *  The number of objects left for traversal.
     *  - argument iterator: the object to iterate on...
     */
    int synctex_iterator_count(synctex_iterator_p iterator);

    /**
     *  The target of the node, either a handle or a proxy.
     */
    synctex_node_p synctex_node_target(synctex_node_p node);
    
#ifndef SYNCTEX_NO_UPDATER
    /*  The main synctex updater object.
     *  This object is used to append information to the synctex file.
     *  Its implementation is considered private.
     *  It is used by the synctex command line tool to take into account modifications
     *  that could occur while postprocessing files by dvipdf like filters.
     */
    typedef struct synctex_updater_t synctex_updater_s;
    typedef synctex_updater_s * synctex_updater_p;
    
    /*  Designated initializer.
     *  Once you are done with your whole job,
     *  free the updater */
    synctex_updater_p synctex_updater_new_with_output_file(const char * output, const char * directory);
    
    /*  Use the next functions to append records to the synctex file,
     *  no consistency tests made on the arguments */
    void synctex_updater_append_magnification(synctex_updater_p updater, char *  magnification);
    void synctex_updater_append_x_offset(synctex_updater_p updater, char *  x_offset);
    void synctex_updater_append_y_offset(synctex_updater_p updater, char *  y_offset);
    
    /*  You MUST free the updater, once everything is properly appended */
    void synctex_updater_free(synctex_updater_p updater);
#endif
    
#if defined(SYNCTEX_DEBUG)
#   include "assert.h"
#   define SYNCTEX_ASSERT assert
#else
#   define SYNCTEX_ASSERT(UNUSED)
#endif

#if defined(SYNCTEX_TESTING)
#warning TESTING IS PROHIBITED
#if __clang__
#define __PRAGMA_PUSH_NO_EXTRA_ARG_WARNINGS \
_Pragma("clang diagnostic push") \
_Pragma("clang diagnostic ignored \"-Wformat-extra-args\"")
    
#define __PRAGMA_POP_NO_EXTRA_ARG_WARNINGS _Pragma("clang diagnostic pop")
#else
#define __PRAGMA_PUSH_NO_EXTRA_ARG_WARNINGS
#define __PRAGMA_POP_NO_EXTRA_ARG_WARNINGS
#endif
    
#   define SYNCTEX_TEST_BODY(counter, condition, desc, ...) \
    do {				\
        __PRAGMA_PUSH_NO_EXTRA_ARG_WARNINGS \
        if (!(condition)) {		\
            ++counter;  \
            printf("**** Test failed: %s\nfile %s\nfunction %s\nline %i\n",#condition,__FILE__,__FUNCTION__,__LINE__); \
            printf((desc), ##__VA_ARGS__); \
        }				\
        __PRAGMA_POP_NO_EXTRA_ARG_WARNINGS \
    } while(0)
        
#   define SYNCTEX_TEST_PARAMETER(counter, condition) SYNCTEX_TEST_BODY(counter, (condition), "Invalid parameter not satisfying: %s", #condition)
    
    int synctex_test_input(synctex_scanner_p scanner);
    int synctex_test_proxy(synctex_scanner_p scanner);
    int synctex_test_tree(synctex_scanner_p scanner);
    int synctex_test_page(synctex_scanner_p scanner);
    int synctex_test_handle(synctex_scanner_p scanner);
    int synctex_test_display_query(synctex_scanner_p scanner);
    int synctex_test_charindex();
    int synctex_test_sheet_1();
    int synctex_test_sheet_2();
    int synctex_test_sheet_3();
    int synctex_test_form();
#endif

#ifdef __cplusplus
}
#endif

#endif
