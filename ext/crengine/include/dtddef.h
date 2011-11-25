/** \file dtddef.h
    \brief Document type definition helper structures and defs.

    Allows to use hardcoded document definition.

    Defines a set of macros for element names, attribute names and namespaces.

    See fb2def.h for example schema definition.

    When included w/o XS_IMPLEMENT_SCHEME defined,
    declares macros for element, attribute, and namespace enums.

    When included with XS_IMPLEMENT_SCHEME defined,
    defines macros for fb2_elem_table, fb2_attr_table and fb2_ns_table tables
    which can be passed to document to define schema.

    Please include it with XS_IMPLEMENT_SCHEME only into once in project.

    CoolReader Engine

    (c) Vadim Lopatin, 2000-2006
    This source code is distributed under the terms of
    GNU General Public License.

    See LICENSE file for details.

*/

#if !defined(__DTD_DEF_H_INCLUDED__) || defined(XS_IMPLEMENT_SCHEME)
#include "lvtypes.h"
#include "cssdef.h"

#if !defined(__DTD_DEF_H_INCLUDED__)
#define __DTD_DEF_H_INCLUDED__


/// default element type definition struct
struct css_elem_def_props_t {
    bool                 allow_text;   ///< is true if element allows text nodes as children
    bool                 is_object;    ///< is true if element is object (i.e. image)
    css_display_t        display;      ///< default display property value
    css_white_space_t    white_space;  ///< default white-space property value
};

/// known element names and styles table entry
struct elem_def_t {
    lUInt16      id;
    const char * name;
    css_elem_def_props_t props;
};

/// known attribute names table entry
struct attr_def_t {
    lUInt16      id;
    const char * name;
};

/// known namespace names table entry
struct ns_def_t {
    lUInt16      id;
    const char * name;
};
#endif

#ifndef XS_IMPLEMENT_SCHEME
#define XS_BEGIN_TAGS \
        enum { \
        el_NULL = 0,
#define XS_TAG1(itm) \
        el_ ## itm,
#define XS_TAG2(itm, name) \
        el_ ## itm,
#define XS_TAG1T(itm) \
        el_ ## itm,
#define XS_TAG1OBJ(itm) \
        el_ ## itm,
#define XS_TAG2T(itm, name) \
        el_ ## itm,
#define XS_TAG1I(itm) \
        el_ ## itm,
#define XS_TAG2I(itm, name) \
        el_ ## itm,
#define XS_TAG1D(itm, txt, disp, ws) \
        el_ ## itm,
#define XS_TAG2D(itm, name, txt, disp, ws) \
        el_ ## itm,
#define XS_END_TAGS \
        el_MAX_ID \
        };

#define XS_BEGIN_ATTRS \
        enum { \
        attr_NULL = 0,
#define XS_ATTR(itm) \
        attr_ ## itm,
#define XS_ATTR2(itm, name) \
        attr_ ## itm,
#define XS_END_ATTRS \
        attr_MAX_ID \
        };

#define XS_BEGIN_NS \
        enum { \
        ns_NULL = 0,
#define XS_NS(itm) \
        ns_ ## itm,
#define XS_END_NS \
        ns_MAX_ID \
        };

#else


#undef  XS_BEGIN_TAGS
#undef  XS_TAG1
#undef  XS_TAG2
#undef  XS_TAG1T
#undef  XS_TAG1OBJ
#undef  XS_TAG2T
#undef  XS_TAG1I
#undef  XS_TAG2I
#undef  XS_TAG1D
#undef  XS_TAG2D
#undef  XS_END_TAGS
#define XS_BEGIN_TAGS \
        static elem_def_t fb2_elem_table [] =  {
#define XS_TAG1(itm) \
        { el_ ## itm, #itm, {false, false, css_d_block, css_ws_normal} },
#define XS_TAG2(itm, name) \
        { el_ ## itm, name, {false, false, css_d_block, css_ws_normal} },
#define XS_TAG1T(itm) \
        { el_ ## itm, #itm, {true, false, css_d_block, css_ws_normal} },
#define XS_TAG1OBJ(itm) \
        { el_ ## itm, #itm, {false, true, css_d_inline, css_ws_normal} },
#define XS_TAG2T(itm, name) \
        { el_ ## itm, name, {true, false, css_d_block, css_ws_normal} },
#define XS_TAG1I(itm) \
        { el_ ## itm, #itm, {true, false, css_d_inline, css_ws_normal} },
#define XS_TAG2I(itm, name) \
        { el_ ## itm, name, {true, false, css_d_inline, css_ws_normal} },
#define XS_TAG1D(itm, txt, disp, ws) \
        { el_ ## itm, #itm, {txt, false, disp, ws} },
#define XS_TAG2D(itm, name, txt, false, disp, ws) \
        { el_ ## itm, name, {txt, false, disp, ws} },
#define XS_END_TAGS \
        { 0, NULL, {false, false, css_d_block, css_ws_normal} } \
        };

#undef  XS_BEGIN_ATTRS
#undef  XS_ATTR
#undef  XS_ATTR2
#undef  XS_END_ATTRS
#define XS_BEGIN_ATTRS \
        static attr_def_t fb2_attr_table [] = {
#define XS_ATTR(itm) \
        { attr_ ## itm, #itm },
#define XS_ATTR2(itm, name) \
        { attr_ ## itm, name },
#define XS_END_ATTRS \
        { 0, NULL } \
        };

#undef  XS_BEGIN_NS
#undef  XS_NS
#undef  XS_END_NS
#define XS_BEGIN_NS \
        static ns_def_t fb2_ns_table [] = {
#define XS_NS(itm) \
        { ns_ ## itm, #itm },
#define XS_END_NS \
        { 0, NULL } \
        };

#endif


#endif // __DTD_DEF_H_INCLUDED__
