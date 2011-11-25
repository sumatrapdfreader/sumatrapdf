/** \file cssdef.h
    \brief Cascading Style Sheet definitions

    Defines enums and structures for subset of CSS2 properties.

    CoolReader Engine

    (c) Vadim Lopatin, 2000-2006

    This source code is distributed under the terms of
    GNU General Public License.

    See LICENSE file for details.
*/

#if !defined(__CSS_DEF_H_INCLUDED__)
#define __CSS_DEF_H_INCLUDED__

#include "lvtypes.h"
#include "lvref.h"
#include "lvstring.h"

/// display property values
enum css_display_t {
    css_d_inherit,
    css_d_inline,
    css_d_block,
    css_d_list_item, 
    css_d_run_in, 
    css_d_compact, 
    css_d_marker, 
    css_d_table, 
    css_d_inline_table, 
    css_d_table_row_group, 
    css_d_table_header_group, 
    css_d_table_footer_group, 
    css_d_table_row, 
    css_d_table_column_group, 
    css_d_table_column, 
    css_d_table_cell, 
    css_d_table_caption, 
    css_d_none, 
};

/// white-space property values
enum css_white_space_t {
    css_ws_inherit,
    css_ws_normal,
    css_ws_pre,
    css_ws_nowrap,
};

/// text-align property values
enum css_text_align_t {
    css_ta_inherit,
    css_ta_left,
    css_ta_right,
    css_ta_center,
    css_ta_justify,
};

/// vertical-align property values
enum css_vertical_align_t {
    css_va_inherit,
    css_va_baseline, 
    css_va_sub,
    css_va_super,
    css_va_top,
    css_va_text_top,
    css_va_middle,
    css_va_bottom,
    css_va_text_bottom,
};

/// text-decoration property values
enum css_text_decoration_t {
    // TODO: support multiple flags
    css_td_inherit = 0,
    css_td_none = 1,
    css_td_underline = 2,
    css_td_overline = 3,
    css_td_line_through = 4,
    css_td_blink = 5,
};

/// hyphenate property values
enum css_hyphenate_t {
    css_hyph_inherit = 0,
    css_hyph_none = 1,
    css_hyph_auto = 2,
};

/// font-style property values
enum css_font_style_t {
    css_fs_inherit,
    css_fs_normal,
    css_fs_italic,
    css_fs_oblique,
};

/// font-weight property values
enum css_font_weight_t {
    css_fw_inherit,
    css_fw_normal,
    css_fw_bold,
    css_fw_bolder,
    css_fw_lighter,
    css_fw_100,
    css_fw_200,
    css_fw_300,
    css_fw_400,
    css_fw_500,
    css_fw_600,
    css_fw_700,
    css_fw_800,
    css_fw_900,
};

/// font-family property values
enum css_font_family_t {
    css_ff_inherit,
    css_ff_serif,
    css_ff_sans_serif,
    css_ff_cursive,
    css_ff_fantasy,
    css_ff_monospace,
};

/// page split property values
enum css_page_break_t {
    css_pb_inherit,
    css_pb_auto,
    css_pb_always,
    css_pb_avoid,
    css_pb_left,
    css_pb_right,
};

/// list-style-type property values
enum css_list_style_type_t {
    css_lst_inherit,
    css_lst_disc,
    css_lst_circle,
    css_lst_square,
    css_lst_decimal,
    css_lst_lower_roman,
    css_lst_upper_roman,
    css_lst_lower_alpha,
    css_lst_upper_alpha,
    css_lst_none,
};

/// list-style-position property values
enum css_list_style_position_t {
    css_lsp_inherit,
    css_lsp_inside,
    css_lsp_outside,
};

/// css length value types
enum css_value_type_t {
    css_val_inherited,
    css_val_unspecified,
    css_val_px,
    css_val_em,
    css_val_ex,
    css_val_in, // 2.54 cm
    css_val_cm,
    css_val_mm,
    css_val_pt, // 1/72 in
    css_val_pc, // 12 pt
    css_val_percent,
    css_val_color,
};

/// css length value
typedef struct css_length_tag {
    css_value_type_t type;  ///< type of value
    int         value;      ///< value (*256 for all types except % and px)
    css_length_tag()
        : type (css_val_px), value (0)
    {
    }
    css_length_tag( int px_value )
        : type (css_val_px), value (px_value)
    {
    }
    css_length_tag(css_value_type_t n_type, int n_value)
        : type(n_type), value(n_value)
    {
    }
    bool operator == ( const css_length_tag & v ) const
    {
        return type == v.type 
            && value == v.value;
    }
    int pack() { return (int)type + (value<<4); }
} css_length_t;

#endif // __CSS_DEF_H_INCLUDED__
