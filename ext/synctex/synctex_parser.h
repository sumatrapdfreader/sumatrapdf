/*
 Copyright (c) 2008-2017 jerome DOT laurens AT u-bourgogne DOT fr
 
 This file is part of the __SyncTeX__ package.
 
 [//]: # (Latest Revision: Fri Jul 14 16:20:41 UTC 2017)
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
 
 ## Acknowledgments:
 
 The author received useful remarks from the __pdfTeX__ developers, especially Hahn The Thanh,
 and significant help from __XeTeX__ developer Jonathan Kew.
 
 ## Nota Bene:
 
 If you include or use a significant part of the __SyncTeX__ package into a software,
 I would appreciate to be listed as contributor and see "__SyncTeX__" highlighted.
*/

#ifndef __SYNCTEX_PARSER__
#   define __SYNCTEX_PARSER__

#include "synctex_version.h"

#ifdef __cplusplus
extern "C" {
#endif
    
    /*  The main synctex object is a scanner.
     *  Its implementation is considered private.
     *  The basic workflow is
     *  -   create a "synctex scanner" with the contents of a file
     *  -   perform actions on that scanner like
        synctex_display_query or synctex_edit_query below.
     *  -   perform actions on nodes returned by the scanner
     *  - free the scanner when the work is done
     */
    typedef struct synctex_scanner_t synctex_scanner_s;
    typedef synctex_scanner_s * synctex_scanner_p;
    
    /**
     *  This is the designated method to create
     *  a new synctex scanner object.
     *  - argument output: the pdf/dvi/xdv file associated
     *      to the synctex file.
     *      If necessary, it can be the tex file that
     *      originated the synctex file but this might cause
     *      problems if the \jobname has a custom value.
     *      Despite this method can accept a relative path
     *      in practice, you should only pass full paths.
     *      The path should be encoded by the underlying
     *      file system, assuming that it is based on
     *      8 bits characters, including UTF8,
     *      not 16 bits nor 32 bits.
     *      The last file extension is removed and
     *      replaced by the proper extension,
     *      either synctex or synctex.gz.
     *  - argument build_directory: It is the directory where
     *      all the auxiliary stuff is created.
     *      If no synctex file is found in the same directory
     *      as the output file, then we try to find one in
     *      this build directory.
     *      It is the directory where all the auxiliary
     *      stuff is created. Sometimes, the synctex output
     *      file and the pdf, dvi or xdv files are not 
     *      created in the same location. See MikTeX.
     *      This directory path can be NULL,
     *      it will be ignored then.
     *      It can be either absolute or relative to the
     *      directory of the output pdf (dvi or xdv) file.
     *      Please note that this new argument is provided
     *      as a convenience but should not be used.
     *      Available since version 1.5.
     *  - argument parse: In general, use 1.
     *      Use 0 only if you do not want to parse the
     *      content but just check for existence.
     *      Available since version 1.5
     *   - resturn: a scanner. NULL is returned in case
     *      of an error or non existent file.
     */
    synctex_scanner_p synctex_scanner_new_with_output_file(const char * output, const char * build_directory, int parse);
    
    /**
     *  Designated method to delete a synctex scanner object,
     *  including all its internal resources.
     *  Frees all the memory, you must call it when you are finished with the scanner.
     *  - argument scanner: a scanner.
     *  - returns: an integer used for testing purposes.
     */
    int synctex_scanner_free(synctex_scanner_p scanner);
    
    /**
     *  Send this message to force the scanner to
     *  parse the contents of the synctex output file.
     *  Nothing is performed if the file was already parsed.
     *  In each query below, this message is sent,
     *  but if you need to access information more directly,
     *  you must ensure that the parsing did occur.
     *  Usage:
     *		if((my_scanner = synctex_scanner_parse(my_scanner))) {
     *			continue with my_scanner...
     *		} else {
     *			there was a problem
     *		}
     *  - returns: the argument on success.
     *      On failure, frees scanner and returns NULL.
     */
    synctex_scanner_p synctex_scanner_parse(synctex_scanner_p scanner);
    
    /*  synctex_node_p is the type for all synctex nodes.
     *  Its implementation is considered private.
     *  The synctex file is parsed into a tree of nodes, either sheet, form, boxes, math nodes... */
    
    typedef struct synctex_node_t synctex_node_s;
    typedef synctex_node_s * synctex_node_p;
    
    /*  The main entry points.
     *  Given the file name, a line and a column number, synctex_display_query returns the number of nodes
     *  satisfying the constrain. Use code like
     *
     *      if(synctex_display_query(scanner,name,line,column,page_hint)>0) {
     *         synctex_node_p node;
     *         while((node = synctex_scanner_next_result(scanner))) {
     *             // do something with node
     *             ...
     *         }
     *     }
     *
     *  Please notice that since version 1.19,
     *  there is a new argument page_hint.
     *  The results in pages closer to page_hint are given first.
     *  For example, one can
     * - highlight each resulting node in the output, using synctex_node_visible_h and synctex_node_visible_v
     * - highlight all the rectangles enclosing those nodes, using synctex_node_box_visible_... functions
     * - highlight just the character using that information
     *
     *  Given the page and the position in the page, synctex_edit_query returns the number of nodes
     *  satisfying the constrain. Use code like
     *
     *     if(synctex_edit_query(scanner,page,h,v)>0) {
     *         synctex_node_p node;
     *         while(node = synctex_scanner_next_result(scanner)) {
     *             // do something with node
     *             ...
     *         }
     *     }
     *
     *  For example, one can
     * - highlight each resulting line in the input,
     * - highlight just the character using that information
     *
     *  page is 1 based
     *  h and v are coordinates in 72 dpi unit, relative to the top left corner of the page.
     *  If you make a new query, the result of the previous one is discarded. If you need to make more than one query
     *  in parallel, use the iterator API exposed in
     *  the synctex_parser_private.h header.
     *  If one of this function returns a negative integer,
     *  it means that an error occurred.
     *
     *  Both methods are conservative, in the sense that matching is weak.
     *  If the exact column number is not found, there will be an answer with the whole line.
     *
     *  Sumatra-PDF, Skim, iTeXMac2, TeXShop and Texworks are examples of open source software that use this library.
     *  You can browse their code for a concrete implementation.
     */
    typedef long synctex_status_t;
    /*  The page_hint argument is used to resolve ambiguities.
     *  Whenever, different matches occur, the ones closest
     *  to the page will be given first. Pass a negative number
     *  when in doubt. Using pdf forms may lead to ambiguities.
     */
    synctex_status_t synctex_display_query(synctex_scanner_p scanner,const char *  name,int line,int column, int page_hint);
    synctex_status_t synctex_edit_query(synctex_scanner_p scanner,int page,float h,float v);
    synctex_node_p synctex_scanner_next_result(synctex_scanner_p scanner);
    synctex_status_t synctex_scanner_reset_result(synctex_scanner_p scanner);
    
    /**
     *  The horizontal and vertical location,
     *  the width, height and depth of a box enclosing node.
     *  All dimensions are given in page coordinates
     *  as opposite to TeX coordinates.
     *  The origin is at the top left corner of the page.
     *  Code example for Qt5:
     *  (from TeXworks source TWSynchronize.cpp)
     *  QRectF nodeRect(synctex_node_box_visible_h(node),
     *      synctex_node_box_visible_v(node) - 
     *          synctex_node_box_visible_height(node),
     *      synctex_node_box_visible_width(node),
     *      synctex_node_box_visible_height(node) + 
     *          synctex_node_box_visible_depth(node));
     *  Code example for Cocoa:
     *  NSRect bounds = [pdfPage
     *      boundsForBox:kPDFDisplayBoxMediaBox];
     *  NSRect nodeRect = NSMakeRect(
     *      synctex_node_box_visible_h(node),
     *      NSMaxY(bounds)-synctex_node_box_visible_v(node) +
     *          synctex_node_box_visible_height(node),
     *      synctex_node_box_visible_width(node),
     *      synctex_node_box_visible_height(node) +
     *          synctex_node_box_visible_depth(node)
     *      );
     *  The visible dimensions are bigger than real ones
     *  to compensate 0 width boxes or nodes intentionally
     *  put outside the box (using \kern for example).
     *  - parameter node: a node.
     *  - returns: a float.
     *  - author: JL
     */
    float synctex_node_box_visible_h(synctex_node_p node);
    float synctex_node_box_visible_v(synctex_node_p node);
    float synctex_node_box_visible_width(synctex_node_p node);
    float synctex_node_box_visible_height(synctex_node_p node);
    float synctex_node_box_visible_depth(synctex_node_p node);

    /**
     *  For quite all nodes, horizontal and vertical coordinates, and width.
     *  All dimensions are given in page coordinates
     *  as opposite to TeX coordinates.
     *  The origin is at the top left corner of the page.
     *  The visible dimensions are bigger than real ones
     *  to compensate 0 width boxes or nodes intentionally
     *  put outside the box (using \kern for example).
     *  All nodes have coordinates, but all nodes don't
     *  have non null size. For example, math nodes
     *  have no width according to TeX, and in that case
     *  synctex_node_visible_width simply returns 0.
     *  The same holds for kern nodes that do not have
     *  height nor depth, etc...
     */
    float synctex_node_visible_h(synctex_node_p node);
    float synctex_node_visible_v(synctex_node_p node);
    float synctex_node_visible_width(synctex_node_p node);
    float synctex_node_visible_height(synctex_node_p node);
    float synctex_node_visible_depth(synctex_node_p node);

    /**
     *  Given a node, access to its tag, line and column.
     *  The line and column numbers are 1 based.
     *  The latter is not yet fully supported in TeX,
     *  the default implementation returns 0
     *  which means the whole line.
     *  synctex_node_get_name returns the path of the
     *  TeX source file that was used to create the node.
     *  When the tag is known, the scanner of the node
     *  will also give that same file name, see
     *  synctex_scanner_get_name below.
     *  For an hbox node, the mean line is the mean
     *  of all the lines of the child nodes.
     *  Sometimes, when synchronization form pdf to source
     *  fails with the line, one should try with the
     *  mean line.
     */
    int synctex_node_tag(synctex_node_p node);
    int synctex_node_line(synctex_node_p node);
    int synctex_node_mean_line(synctex_node_p node);
    int synctex_node_column(synctex_node_p node);
    const char * synctex_node_get_name(synctex_node_p node);
    
    /**
     This is the page where the node appears.
     *  This is a 1 based index as given by TeX.
     */
    int synctex_node_page(synctex_node_p node);

    /**
     *  Display all the information contained in the scanner.
     *  If the records are too numerous, only the first ones are displayed.
     *  This is mainly for informational purpose to help developers.
     */
    void synctex_scanner_display(synctex_scanner_p scanner);
    
    /*  Managing the input file names.
     *  Given a tag, synctex_scanner_get_name will return the corresponding file name.
     *  Conversely, given a file name, synctex_scanner_get_tag will return, the corresponding tag.
     *  The file name must be the very same as understood by TeX.
     *  For example, if you \input myDir/foo.tex, the file name is myDir/foo.tex.
     *  No automatic path expansion is performed.
     *  Finally, synctex_scanner_input is the first input node of the scanner.
     *  To browse all the input node, use a loop like
     *      ...
     *      synctex_node_p = input_node;
     *      ...
     *      if((input_node = synctex_scanner_input(scanner))) {
     *          do {
     *              blah
     *          } while((input_node=synctex_node_sibling(input_node)));
     *     }
     *
     *  The output is the name that was used to create the scanner.
     *  The synctex is the real name of the synctex file,
     *  it was obtained from output by setting the proper file extension.
     */
    const char * synctex_scanner_get_name(synctex_scanner_p scanner,int tag);
    
    int synctex_scanner_get_tag(synctex_scanner_p scanner,const char * name);
    
    synctex_node_p synctex_scanner_input(synctex_scanner_p scanner);
    synctex_node_p synctex_scanner_input_with_tag(synctex_scanner_p scanner,int tag);
    const char * synctex_scanner_get_output(synctex_scanner_p scanner);
    const char * synctex_scanner_get_synctex(synctex_scanner_p scanner);
    
    /*  The x and y offset of the origin in TeX coordinates. The magnification
     These are used by pdf viewers that want to display the real box size.
     For example, getting the horizontal coordinates of a node would require
     synctex_node_box_h(node)*synctex_scanner_magnification(scanner)+synctex_scanner_x_offset(scanner)
     Getting its TeX width would simply require
     synctex_node_box_width(node)*synctex_scanner_magnification(scanner)
     but direct methods are available for that below.
     */
    int synctex_scanner_x_offset(synctex_scanner_p scanner);
    int synctex_scanner_y_offset(synctex_scanner_p scanner);
    float synctex_scanner_magnification(synctex_scanner_p scanner);
    
    /**
     *  ## Browsing the nodes
     *  parent, child and sibling are standard names for tree nodes.
     *  The parent is one level higher,
     *  the child is one level deeper,
     *  and the sibling is at the same level.
     *  A node and its sibling have the same parent.
     *  A node is the parent of its children.
     *  A node is either the child of its parent,
     *  or belongs to the sibling chain of its parent's child.
     *  The sheet or form of a node is the topmost ancestor,
     *  it is of type sheet or form.
     *  The next node is either the child, the sibling or the parent's sibling,
     *  unless the parent is a sheet, a form or NULL.
     *  This allows to navigate through all the nodes of a given sheet node:
     *
     *     synctex_node_p node = sheet;
     *     while((node = synctex_node_next(node))) {
     *         // do something with node
     *     }
     *
     *  With synctex_sheet_content and synctex_form_content,
     *  you can retrieve the sheet node given the page
     *  or form tag.
     *  The page is 1 based, according to TeX standards.
     *  Conversely synctex_node_parent_sheet or
     *  synctex_node_parent_form allows to retrieve
     *  the sheet or the form containing a given node.
     *  Notice that a node is not contained in a sheet
     *  and a form at the same time.
     *  Some nodes are not contained in either (handles).
     */
    
    synctex_node_p synctex_node_parent(synctex_node_p node);
    synctex_node_p synctex_node_parent_sheet(synctex_node_p node);
    synctex_node_p synctex_node_parent_form(synctex_node_p node);
    synctex_node_p synctex_node_child(synctex_node_p node);
    synctex_node_p synctex_node_last_child(synctex_node_p node);
    synctex_node_p synctex_node_sibling(synctex_node_p node);
    synctex_node_p synctex_node_last_sibling(synctex_node_p node);
    synctex_node_p synctex_node_arg_sibling(synctex_node_p node);
    synctex_node_p synctex_node_next(synctex_node_p node);
    
    /**
     *  Top level entry points.
     *  The scanner owns a list of sheet siblings and
     *  a list of form siblings.
     *  Sheets or forms have one child which is a box:
     *  their contents.
     *  - argument page: 1 based sheet page number.
     *  - argument tag: 1 based form tag number.
     */
    synctex_node_p synctex_sheet(synctex_scanner_p scanner,int page);
    synctex_node_p synctex_sheet_content(synctex_scanner_p scanner,int page);
    synctex_node_p synctex_form(synctex_scanner_p scanner,int tag);
    synctex_node_p synctex_form_content(synctex_scanner_p scanner,int tag);
    
    /*  This is primarily used for debugging purpose.
     *  The second one logs information for the node and recursively displays information for its next node */
    void synctex_node_log(synctex_node_p node);
    void synctex_node_display(synctex_node_p node);
    
    /*  For quite all nodes, horizontal, vertical coordinates, and width.
     *  These are expressed in TeX small points coordinates, with origin at the top left corner.
     */
    int synctex_node_h(synctex_node_p node);
    int synctex_node_v(synctex_node_p node);
    int synctex_node_width(synctex_node_p node);
    int synctex_node_height(synctex_node_p node);
    int synctex_node_depth(synctex_node_p node);
    
    /*  For all nodes, dimensions of the enclosing box.
     *  These are expressed in TeX small points coordinates, with origin at the top left corner.
     *  A box is enclosing itself.
     */
    int synctex_node_box_h(synctex_node_p node);
    int synctex_node_box_v(synctex_node_p node);
    int synctex_node_box_width(synctex_node_p node);
    int synctex_node_box_height(synctex_node_p node);
    int synctex_node_box_depth(synctex_node_p node);

#ifdef __cplusplus
}
#endif

#endif
