/*
 Copyright (c) 2008-2024 jerome DOT laurens AT u-bourgogne DOT fr
 
 This file is part of the __SyncTeX__ package.
 
 Version: see synctex_version.h
 Latest Revision: Thu Mar 21 14:12:58 UTC 2024

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

/**
 * @file synctex_parser_advanced.h
 * @author LAURENS Jérôme (jerome.laurens@u-bourgogne.fr)
 * @brief More header declaration than in `synctex_parser.h`.
 * @version 0.1
 * @date 2024-03-22
 * 
 * @copyright Copyright (c) 2024
 * 
 */

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

#   if !defined(_SYNCTEX_ONLY_CHAR_DEFINITION_)

#include "synctex_parser.h"
#include "synctex_parser_utils.h"

#ifndef _SYNCTEX_PARSER_ADVANCED_H_
#   define _SYNCTEX_PARSER_ADVANCED_H_

#ifdef __cplusplus
extern "C" {
#endif
    /**
     * @brief Reminder that the argument must not be NULL.
     * 
     * Some functions require a non NULL node pointer argument.
     */
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
    
    /*
     *  These are the mask hekpers for the synctex node types.
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
    /*
     *  These are the masks for the synctex node types.
     *  int's are 32 bits at leats.
     */
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
#if 0
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
#endif
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
    typedef struct _synctex_class_t _synctex_class_s;
    
    /**
     * @brief Pointer to a pseudo class.
     * @typedef synctex_class_p
     * @author: Jérôme LAURENS
     *
     * Each node has a class, it is therefore called an object.
     * Each class has a unique scanner.
     * Each class has a type which is a unique identifier.
     * The class points to various methods,
     * each of them vary amongst objects.
     * Each class has a data model which stores node's attributes.
     * Each class has an tree model which stores children and parent.
     * Inspectors give access to data and tree elements.
     */
    typedef _synctex_class_s * synctex_class_p;
    
    /**
     * @typedef synctex_node_p
     * @brief Pointer to a node structure
     * 
     * The eventual target of the pointer is a private `_synctex_node_s` structure.
     * 
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
    } _synctex_data_u;

    /**
     * @brief Pointer to an opaque node data structure.
     * 
     */
    typedef _synctex_data_u * synctex_data_p;
    
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
/**
 * @brief Node data model.
 * 
 */
    struct _synctex_node_t {
        SYNCTEX_DECLARE_CHARINDEX
        /** Each node has an associate class. */
        synctex_class_p class_;
#ifdef DEBUG
        _synctex_data_u data[22];
#else
        /** Each node has associate data. */
        _synctex_data_u data[1];
#endif
    };
    
    /**
     * @brief A pointer to an node pointer.
     * 
     * First element of an array of node pointers.
     * Mainly the list of friends of a node.
     */
    typedef synctex_node_p * synctex_node_r;
    
    /**
     * @brief 2D point with integer coordinates.
     * 
     * Used at various places.
     * For the hit point in the output file for example.
     */
    typedef struct {
        /** Horizontal coordinate. */
        int h;
        /** Vertical coordinate. */
        int v;
    } synctex_point_s;
    
    /**
     * @brief Pointer to a 2D point.
     * 
     */
    typedef synctex_point_s * synctex_point_p;
    
    /**
     * @brief 2D rectangle
     * Used for boxes.
     */
    typedef struct {
        /** top left */
        synctex_point_s min;
        /** bottom right */
        synctex_point_s max;
    } synctex_box_s;
    
    /**
     * @brief Pointer to 2D rectangle
     * Used for NULL terminated lists of boxes.
     */
    typedef synctex_box_s * synctex_box_p;
    
    /**
     * @brief Types of the synctex nodes.
     *
     * These are exactly the TeX nodes but somehow related.
     * No real need to use them but the compiler needs them here.
     * 
     * Each node type uniquely identifies a node class.
     * 
     * There are 3 kinds of nodes.
     * - primary nodes, created when parsing the `.synctex` file.
     *   They correspond to lines in the `.synctex` file.
     * - proxies, used to support pdf forms,
     *   The ref primary nodes are replaced by a tree
     *   of proxy nodes which duplicate the tree of primary
     *   nodes available in the refered form.
     *   Roughly speaking, the primary nodes of the form
     *   know what to display, the proxy nodes know where.
     * - handles are used in queries. They point to either
     *   primary nodes or proxies.
     */
    typedef enum {
        /** No correspondance in `.synctex` file. */
        synctex_node_type_none = 0,
        /** line starting with `Input:` */
        synctex_node_type_input,
        /** line starting with `{` */
        synctex_node_type_sheet,
        /** line starting with `<` */
        synctex_node_type_form,
        /** line starting with `f` */
        synctex_node_type_ref,
        /** line starting with `[` */
        synctex_node_type_vbox,/*5*/
        /** line starting with `v` */
        synctex_node_type_void_vbox,
        /** line starting with `(` */
        synctex_node_type_hbox,
        /** line starting with `h` */
        synctex_node_type_void_hbox,
        /** line starting with `k` */
        synctex_node_type_kern,
        /** line starting with `g` */
        synctex_node_type_glue,/*10*/
        /** line starting with `r` */
        synctex_node_type_rule,
        /** line starting with `$` */
        synctex_node_type_math,
        /** line starting with `x` */
        synctex_node_type_boundary,
        /** Undocumented */
        synctex_node_type_box_bdry,
        /** Undocumented */
        synctex_node_type_proxy,/*15*/
        /** Undocumented */
        synctex_node_type_proxy_last,
        /** Undocumented */
        synctex_node_type_proxy_vbox,
        /** Undocumented */
        synctex_node_type_proxy_hbox,
        /** Undocumented */
        synctex_node_type_handle,
        /** Number of types */
        synctex_node_number_of_types
    } synctex_node_type_t;
    
    /**
     * @brief The type of a given node, as integer.
    */
    synctex_node_type_t synctex_node_type(synctex_node_p node);
    
    /**
     * @brief The type of a given node, as a human readable text.
    */
    const char * synctex_node_isa(synctex_node_p node);

    /**
     * @brief The node type of the target.
     * 
     * @param node 
     * @return synctex_node_type_t 
     */
    synctex_node_type_t synctex_node_target_type(synctex_node_p node);
    
    /*  Given a node, access to the location in the synctex file where it is defined.
     */

    int synctex_node_weight(synctex_node_p node);
    int synctex_node_child_count(synctex_node_p node);
    
    /** \defgroup AdvancedGeometry Node geometry (advance)
     * 
     *   Available in `synctex_parser_advanced.h`.
     * @{
     */
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
    /** @} */
    
    synctex_node_p synctex_node_new(synctex_scanner_p scanner,synctex_node_type_t type);

    /**
     * @brief Designated scanner constructor.
     * 
     * @return synctex_scanner_p 
     */
    synctex_scanner_p synctex_scanner_new(void);

    /** \addtogroup Scanner
     * @{
     */

/** @defgroup Iterator Managing the answer to queries.
 *  
 * Answers to edit and view queries are special structure.
 * @{
 */

    /**
     * @brief Scanner display switcher getter
     * 
     * @param scanR 
     * @return int 
     * @see `synctex_scanner_set_display_switcher`
     */
    int synctex_scanner_display_switcher(synctex_scanner_p scanR);
    
    /**
     * @brief Scanner display switcher setter.
     * 
     * @param scanR 
     * @param switcher
     * If the switcher is 0, `synctex_node_display` is disabled.
     * If the switcher is <0, `synctex_node_display` has no limit.
     * If the switcher is >0, only the first switcher (as number) nodes are displayed.
     */
    void synctex_scanner_set_display_switcher(synctex_scanner_p scanR, int switcher);

    /** @} */

/** @defgroup Iterator Managing the answer to queries.
 *  
 * Answers to edit and view queries are special structure.
 * @{
 */

    /**
     * @brief Structure used to traverse the answer to client queries.
     * 
     * First answers are the best matches, according
     * to criteria explained below.
     * Next answers are not ordered.
     * Objects are handles to nodes in the synctex node tree starting at scanner.
     */
    typedef struct synctex_iterator_t synctex_iterator_s;
    
    /**
     * @brief Pointer to an iterator structure.
     * 
     */
    typedef synctex_iterator_s * synctex_iterator_p;

    /**
     * Designated creator for a display query.
     * 
     * A display query is used in forward navigation from source to output.
     * 
     * Returns NULL if the query has no answer.
     * Code example:
     * ```
     *    synctex_iterator_p iterator = NULL;
     *    if ((iterator = synctex_iterator_new_display(...)) {
     *      synctex_node_p node = NULL;
     *      while((node = synctex_iterator_next_result(iterator))) {
     *        <do something with node...>
     *      }
     *      synctex_iterator_free(iterator);
     *    }
     * ```
     */
    synctex_iterator_p synctex_iterator_new_display(synctex_scanner_p scanner,const char *  name,int line,int column, int page_hint);
    /**
     *  Designated creator for an  edit query.
     * 
     * An edit query is used in backward navigation from output to source.
     *  Code example:
     * ```
     *    synctex_iterator_p iterator = NULL;
     *    if ((iterator = synctex_iterator_new_edit(...)) {
     *      synctex_node_p node = NULL;
     *      while((node = synctex_iterator_next_result(iterator))) {
     *        <do something with node...>
     *      }
     *      synctex_iterator_free(iterator);
     *    }
     * ```
     */
    synctex_iterator_p synctex_iterator_new_edit(synctex_scanner_p scanner,int page,float h,float v);

    /*
     *  
     *  - argument iterator: the object to free...
     */

    /**
     * @brief Free all the resources associate to the iterator.
     * 
     * You should free the iterator before the scanner
     * owning the nodes it iterates with.
     *
     * @param iterator the object to free...
     */
    void synctex_iterator_free(synctex_iterator_p iterator);

    /**
     * @brief Whether the iterator actually points to a node.
     * 
     * @param iterator the object to iterate on...
     * @return synctex_bool_t 
     */
    synctex_bool_t synctex_iterator_has_next(synctex_iterator_p iterator);
    
    /**
     * @brief Get the next query result.
     * 
     * Returns the pointed object and advance the cursor
     * to the next object. Returns NULL and does nothing
     * if the end has already been reached.
     * 
     * @param iterator the object to iterate on...
     * @return synctex_node_p 
     */
    synctex_node_p synctex_iterator_next_result(synctex_iterator_p iterator);
    
    /**
     * @brief Reset the cursor position to the first result.
     * 
     * @param iterator the object to iterate on...
     * @return int the number of results
     */
    int synctex_iterator_reset(synctex_iterator_p iterator);
    
/** @} */ // end of group Iterator
 
    /**
     * @brief The number of objects left.
     * 
     * @param iterator the object to iterate on...
     * @return int 
     */
    int synctex_iterator_count(synctex_iterator_p iterator);


    /**
     *  The target of the node, either a handle or a proxy.
     */
    synctex_node_p synctex_node_target(synctex_node_p node);
    
#ifndef SYNCTEX_NO_UPDATER
/** @defgroup Updater A way to alter synctex files.
 *  
 * Sometimes it is necessary to modify the `.synctex` file.
 * An updater is used to append information to the synctex file.
 * Its implementation is considered unstable.
 * It is used by the synctex command line tool to take into account modifications
 * that could occur while postprocessing files by dvipdf like filters.
 * @{
 */
   /**
    * @brief Updater data structure.
    * 
    */
    typedef struct _synctex_updater_t _synctex_updater_s;
   /**
    * @brief Pointer to a `synctex_updater_s` updater data structure.
    * 
    */
    typedef _synctex_updater_s * synctex_updater_p;
    
    /**
     * @brief Updater designated creator.
     * 
     * Once you are done with your whole job, free the updater.
     * 
     * @param output name of a pdf file
     * @param directory optional directory where the pdf file is,
     *   defaults to the current working directory.
     * @return synctex_updater_p
     * @see synctex_updater_free
     */
    synctex_updater_p synctex_updater_new_with_output_file(const char * output, const char * directory);
    
    /**
     * @brief Append a magnification record.
     * 
     * @param updater 
     * @param magnification 
     */
    void synctex_updater_append_magnification(synctex_updater_p updater, char *  magnification);

    /**
     * @brief Append an x-offset record.
     * 
     * @param updater 
     * @param x_offset 
     */
    void synctex_updater_append_x_offset(synctex_updater_p updater, char *  x_offset);

    /**
     * @brief Append an y-offset record.
     * 
     * @param updater 
     * @param y_offset 
     */
    void synctex_updater_append_y_offset(synctex_updater_p updater, char *  y_offset);
    
    /*  You MUST free the updater, once everything is properly appended */

    /**
     * @brief Updater designated destructor.
     * 
     * @param updater 
     */
    void synctex_updater_free(synctex_updater_p updater);
/** @} */ // end of group Updater
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

#endif
