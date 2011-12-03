/*******************************************************

   CoolReader Engine

   lvstsheet.cpp:  style sheet implementation

   (c) Vadim Lopatin, 2000-2006

   This source code is distributed under the terms of
   GNU General Public License.

   See LICENSE file for details.

*******************************************************/

#include "../include/lvstsheet.h"
#include "../include/lvtinydom.h"
#include "../include/fb2def.h"
#include "../include/lvstream.h"

// define to dump all tokens
//#define DUMP_CSS_PARSING

enum css_decl_code {
    cssd_unknown,
    cssd_display,
    cssd_white_space,
    cssd_text_align,
    cssd_text_align_last,
    cssd_text_decoration,
    cssd_hyphenate, // hyphenate
    cssd_hyphenate2, // -webkit-hyphens
    cssd_hyphenate3, // adobe-hyphenate
    cssd_hyphenate4, // adobe-text-layout
    cssd_color,
    cssd_background_color,
    cssd_vertical_align,
    cssd_font_family, // id families like serif, sans-serif
    cssd_font_names,   // string font name like Arial, Courier
    cssd_font_size,
    cssd_font_style,
    cssd_font_weight,
    cssd_text_indent,
    cssd_line_height,
    cssd_letter_spacing,
    cssd_width,
    cssd_height,
    cssd_margin_left,
    cssd_margin_right,
    cssd_margin_top,
    cssd_margin_bottom,
    cssd_margin,
    cssd_padding_left,
    cssd_padding_right,
    cssd_padding_top,
    cssd_padding_bottom,
    cssd_padding,
    cssd_page_break_before,
    cssd_page_break_after,
    cssd_page_break_inside,
    cssd_list_style,
    cssd_list_style_type,
    cssd_list_style_position,
    cssd_list_style_image,
    cssd_stop,
};

static const char * css_decl_name[] = {
    "",
    "display",
    "white-space",
    "text-align",
    "text-align-last",
    "text-decoration",
    "hyphenate",
    "-webkit-hyphens",
    "adobe-hyphenate",
    "adobe-text-layout",
    "color",
    "background-color",
    "vertical-align",
    "font-family",
    "$dummy-for-font-names$",
    "font-size",
    "font-style",
    "font-weight",
    "text-indent",
    "line-height",
    "letter-spacing",
    "width",
    "height",
    "margin-left",
    "margin-right",
    "margin-top",
    "margin-bottom",
    "margin",
    "padding-left",
    "padding-right",
    "padding-top",
    "padding-bottom",
    "padding",
    "page-break-before",
    "page-break-after",
    "page-break-inside",
    "list-style",
    "list-style-type",
    "list-style-position",
    "list-style-image",
    NULL
};

inline bool css_is_alpha( char ch )
{
    return ( (ch>='A' && ch<='Z') || ( ch>='a' && ch<='z' ) || (ch=='-') || (ch=='_') );
}

inline bool css_is_alnum( char ch )
{
    return ( css_is_alpha(ch) || ( ch>='0' && ch<='9' ) );
}

static int substr_compare( const char * sub, const char * & str )
{
    int j;
    for ( j=0; sub[j] == str[j] && sub[j] && str[j]; j++)
        ;
    if (!sub[j])
    {
        //bool last_alpha = css_is_alpha( sub[j-1] );
        //bool next_alnum = css_is_alnum( str[j] );
        if ( !css_is_alpha( sub[j-1] ) || !css_is_alnum( str[j] ) )
        {
            str+=j;
            return j;
        }
    }
    return 0;
}

inline char toLower( char c )
{
    if ( c>='A' && c<='Z' )
        return c - 'A' + 'a';
    return c;
}

static int substr_icompare( const char * sub, const char * & str )
{
    int j;
    for ( j=0; toLower(sub[j]) == toLower(str[j]) && sub[j] && str[j]; j++)
        ;
    if (!sub[j])
    {
        //bool last_alpha = css_is_alpha( sub[j-1] );
        //bool next_alnum = css_is_alnum( str[j] );
        if ( !css_is_alpha( sub[j-1] ) || !css_is_alnum( str[j] ) )
        {
            str+=j;
            return j;
        }
    }
    return 0;
}

static bool skip_spaces( const char * & str )
{
    while (*str==' ' || *str=='\t' || *str=='\n' || *str == '\r')
        str++;
    if ( *str=='/' && str[1]=='*' ) {
        // comment found
        while ( *str && str[1] && (str[0]!='*' || str[1]!='/') )
            str++;
    }
    while (*str==' ' || *str=='\t' || *str=='\n' || *str == '\r')
        str++;
    return *str != 0;
}

static css_decl_code parse_property_name( const char * & res )
{
    const char * str = res;
    for (int i=1; css_decl_name[i]; i++)
    {
        if (substr_compare( css_decl_name[i], str ))
        {
            // found!
            skip_spaces(str);
            if ( substr_compare( ":", str )) {
#ifdef DUMP_CSS_PARSING
                CRLog::trace("property name: %s", lString8(res, str-res).c_str() );
#endif
                skip_spaces(str);
                res = str;
                return (css_decl_code)i;
            }
        }
    }
    return cssd_unknown;
}

static int parse_name( const char * & str, const char * * names, int def_value )
{
    for (int i=0; names[i]; i++)
    {
        if (substr_compare( names[i], str ))
        {
            // found!
            return i;
        }
    }
    return def_value;
}

static bool next_property( const char * & str )
{
    while (*str && *str !=';' && *str!='}')
        str++;
    if (*str == ';')
        str++;
    return skip_spaces( str );
}

static bool parse_number_value( const char * & str, css_length_t & value )
{
    value.type = css_val_unspecified;
    skip_spaces( str );
    if ( substr_compare( "inherited", str ) )
    {
        value.type = css_val_inherited;
        value.value = 0;
        return true;
    }
    int n = 0;
    if (*str != '.') {
        if (*str<'0' || *str>'9') {
            return false; // not a number
        }
        while (*str>='0' && *str<='9') {
            n = n*10 + (*str - '0');
            str++;
        }
    }
    int frac = 0;
    int frac_div = 1;
    if (*str == '.') {
        str++;
        while (*str>='0' && *str<='9')
        {
            frac = frac*10 + (*str - '0');
            frac_div *= 10;
            str++;
        }
    }
    if ( substr_compare( "em", str ) )
        value.type = css_val_em;
    else if ( substr_compare( "pt", str ) )
        value.type = css_val_pt;
    else if ( substr_compare( "ex", str ) )
        value.type = css_val_pt;
    else if ( substr_compare( "px", str ) )
        value.type = css_val_px;
    else if ( substr_compare( "%", str ) )
        value.type = css_val_percent;
    if ( value.type == css_val_unspecified )
        return false;
    if ( value.type == css_val_px || value.type == css_val_percent )
        value.value = n;                               // normal
    else
        value.value = n * 256 + 256 * frac / frac_div; // *256
    return true;
}

struct standard_color_t
{
    const char * name;
    lUInt32 color;
};

standard_color_t standard_color_table[] = {
    {"black", 0x000000},
    {"green", 0x008000},
    {"silver", 0xC0C0C0},
    {"lime", 0x00FF00},
    {"gray", 0x808080},
    {"olive", 0x808000},
    {"white", 0xFFFFFF},
    {"yellow", 0xFFFF00},
    {"maroon", 0x800000},
    {"navy", 0x000080},
    {"red", 0xFF0000},
    {"blue", 0x0000FF},
    {"purple", 0x800080},
    {"teal", 0x008080},
    {"fuchsia", 0xFF00FF},
    {"aqua", 0x00FFFF},
    {NULL, 0}
};

static int hexDigit( char c )
{
    if ( c >= '0' && c <= '9' )
        return c-'0';
    if ( c >= 'A' && c <= 'F' )
        return c - 'A' + 10;
    if ( c >= 'a' && c <= 'f' )
        return c - 'a' + 10;
    return -1;
}

bool parse_color_value( const char * & str, css_length_t & value )
{
    value.type = css_val_unspecified;
    skip_spaces( str );
    if ( substr_compare( "inherited", str ) )
    {
        value.type = css_val_inherited;
        value.value = 0;
        return true;
    }
    if ( substr_compare( "none", str ) )
    {
        value.type = css_val_unspecified;
        value.value = 0;
        return true;
    }
    if (*str=='#') {
        // #rgb or #rrggbb colors
        str++;
        int nDigits = 0;
        for ( ; hexDigit(str[nDigits])>=0; nDigits++ )
            ;
        if ( nDigits==3 ) {
            int r = hexDigit( *str++ );
            int g = hexDigit( *str++ );
            int b = hexDigit( *str++ );
            value.type = css_val_color;
            value.value = (((r + r*16) * 256) | (g + g*16)) * 256 | (b + b*16);
            return true;
        } else if ( nDigits==6 ) {
            int r = hexDigit( *str++ ) * 16 + hexDigit( *str++ );
            int g = hexDigit( *str++ ) * 16 + hexDigit( *str++ );
            int b = hexDigit( *str++ ) * 16 + hexDigit( *str++ );
            value.type = css_val_color;
            value.value = ((r * 256) | g) * 256 | b;
            return true;
        } else
            return false;
    }
    for ( int i=0; standard_color_table[i].name != NULL; i++ ) {
        if ( substr_icompare( standard_color_table[i].name, str ) ) {
            value.type = css_val_color;
            value.value = standard_color_table[i].color;
            return true;
        }
    }
    return false;
}

static const char * css_d_names[] = 
{
    "inherit",
    "inline",
    "block",
    "list-item", 
    "run-in", 
    "compact", 
    "marker", 
    "table", 
    "inline-table", 
    "table-row-group", 
    "table-header-group", 
    "table-footer-group", 
    "table-row", 
    "table-column-group", 
    "table-column", 
    "table-cell", 
    "table-caption", 
    "none", 
    NULL
};

static const char * css_ws_names[] = 
{
    "inherit",
    "normal",
    "pre",
    "nowrap",
    NULL
};

static const char * css_ta_names[] = 
{
    "inherit",
    "left",
    "right",
    "center",
    "justify",
    NULL
};

static const char * css_td_names[] = 
{
    "inherit",
    "none",
    "underline",
    "overline",
    "line-through",
    "blink",
    NULL
};

static const char * css_hyph_names[] = 
{
    "inherit",
    "none",
    "auto",
    NULL
};

static const char * css_hyph_names2[] =
{
    "inherit",
    "optimizeSpeed",
    "optimizeQuality",
    NULL
};

static const char * css_hyph_names3[] =
{
    "inherit",
    "none",
    "explicit",
    NULL
};

static const char * css_pb_names[] =
{
    "inherit",
    "auto",
    "always",
    "avoid",
    "left",
    "right",
    NULL
};

static const char * css_fs_names[] = 
{
    "inherit",
    "normal",
    "italic",
    "oblique",
    NULL
};

static const char * css_fw_names[] = 
{
    "inherit",
    "normal",
    "bold",
    "bolder",
    "lighter",
    "100",
    "200",
    "300",
    "400",
    "500",
    "600",
    "700",
    "800",
    "900",
    NULL
};
static const char * css_va_names[] = 
{
    "inherit",
    "baseline", 
    "sub",
    "super",
    "top",
    "text-top",
    "middle",
    "bottom",
    "text-bottom",
    NULL
};

static const char * css_ti_attribute_names[] =
{
    "hanging",
    NULL
};

static const char * css_ff_names[] =
{
    "inherit",
    "serif",
    "sans-serif",
    "cursive",
    "fantasy",
    "monospace",
    NULL
};

static const char * css_lst_names[] =
{
    "inherit",
    "disc",
    "circle",
    "square",
    "decimal",
    "lower-roman",
    "upper-roman",
    "lower-alpha",
    "upper-alpha",
    "none",
    NULL
};

static const char * css_lsp_names[] =
{
    "inherit",
    "inside",
    "outside",
    NULL
};


bool LVCssDeclaration::parse( const char * &decl )
{
    #define MAX_DECL_SIZE 512
    int buf[MAX_DECL_SIZE];
    int buf_pos = 0;

    if ( !decl )
        return false;

    skip_spaces( decl );
    if (*decl!='{')
        return false;
    decl++;
    while (*decl && *decl!='}') {
        skip_spaces( decl );
        css_decl_code prop_code = parse_property_name( decl );
        skip_spaces( decl );
        lString8 strValue;
        if (prop_code != cssd_unknown)
        {
            // parsed ok
            int n = -1;
            switch ( prop_code )
            {
            case cssd_display:
                n = parse_name( decl, css_d_names, -1 );
                break;
            case cssd_white_space:
                n = parse_name( decl, css_ws_names, -1 );
                break;
            case cssd_text_align:
                n = parse_name( decl, css_ta_names, -1 );
                break;
            case cssd_text_align_last:
                n = parse_name( decl, css_ta_names, -1 );
                break;
            case cssd_text_decoration:
                n = parse_name( decl, css_td_names, -1 );
                break;
            case cssd_hyphenate:
            case cssd_hyphenate2:
            case cssd_hyphenate3:
            case cssd_hyphenate4:
            	prop_code = cssd_hyphenate;
                n = parse_name( decl, css_hyph_names, -1 );
                if ( n==-1 )
                    n = parse_name( decl, css_hyph_names2, -1 );
                if ( n==-1 )
                    n = parse_name( decl, css_hyph_names3, -1 );
                break;
            case cssd_page_break_before:
                n = parse_name( decl, css_pb_names, -1 );
                break;
            case cssd_page_break_inside:
                n = parse_name( decl, css_pb_names, -1 );
                break;
            case cssd_page_break_after:
                n = parse_name( decl, css_pb_names, -1 );
                break;
            case cssd_list_style_type:
                n = parse_name( decl, css_lst_names, -1 );
                break;
            case cssd_list_style_position:
                n = parse_name( decl, css_lsp_names, -1 );
                break;
            case cssd_vertical_align:
                n = parse_name( decl, css_va_names, -1 );
                break;
            case cssd_font_family:
                {
                    lString8Collection list;
                    int processed = splitPropertyValueList( decl, list );
                    decl += processed;
                       n = -1;
                    if (list.length())
                    {
                        for (int i=list.length()-1; i>=0; i--)
                        {
                            const char * name = list[i].c_str();
                            int nn = parse_name( name, css_ff_names, -1 );
                            if (n==-1 && nn!=-1)
                            {
                                n = nn;
                            }
                            if (nn!=-1)
                            {
                                // remove family name from font list
                                list.erase( i, 1 );
                            }
                        }
                        strValue = joinPropertyValueList( list );
                    }
                }
                break;
            case cssd_font_style:
                n = parse_name( decl, css_fs_names, -1 );
                break;
            case cssd_font_weight:
                n = parse_name( decl, css_fw_names, -1 );
                break;
            case cssd_text_indent:
                {
                    // read length
                    css_length_t len;
                    bool negative = false;
                    if ( *decl == '-' ) {
                        decl++;
                        negative = true;
                    }
                    if ( parse_number_value( decl, len ) )
                    {
                        // read optional "hanging" flag
                        skip_spaces( decl );
                        int attr = parse_name( decl, css_ti_attribute_names, -1 );
                        if ( attr==0 || negative ) {
                            len.value = -len.value;
                        }
                        // save result
                        buf[ buf_pos++ ] = prop_code;
                        buf[ buf_pos++ ] = len.type;
                        buf[ buf_pos++ ] = len.value;
                    }
                }
                break;
            case cssd_line_height:
            case cssd_letter_spacing:
            case cssd_font_size:
            case cssd_width:
            case cssd_height:
            case cssd_margin_left:
            case cssd_margin_right:
            case cssd_margin_top:
            case cssd_margin_bottom:
            case cssd_margin:
            case cssd_padding_left:
            case cssd_padding_right:
            case cssd_padding_top:
            case cssd_padding_bottom:
            case cssd_padding:
                {
                    css_length_t len;
                    if ( parse_number_value( decl, len ) )
                    {
                        buf[ buf_pos++ ] = prop_code;
                        buf[ buf_pos++ ] = len.type;
                        buf[ buf_pos++ ] = len.value;
                    }
                }
                break;
            case cssd_color:
            case cssd_background_color:
            {
                css_length_t len;
                if ( parse_color_value( decl, len ) )
                {
                    buf[ buf_pos++ ] = prop_code;
                    buf[ buf_pos++ ] = len.type;
                    buf[ buf_pos++ ] = len.value;
                }
            }
            break;
            case cssd_stop:
            case cssd_unknown:
            default:
                break;
            }
            if ( n!= -1)
            {
                // add enum property
                buf[buf_pos++] = prop_code;
                buf[buf_pos++] = n;
            }
            if (!strValue.empty())
            {
                // add string property
                if (prop_code==cssd_font_family)
                {
                    // font names
                    buf[buf_pos++] = cssd_font_names;
                    buf[buf_pos++] = strValue.length();
                    for (unsigned i=0; i<strValue.length(); i++)
                        buf[buf_pos++] = strValue[i];
                }
            }
        }
        else
        {
            // error: unknown property?
        }
        next_property( decl );
    }

    // store parsed result
    if (buf_pos)
    {
        buf[buf_pos++] = cssd_stop; // add end marker
        _data = new int[buf_pos];
        for (int i=0; i<buf_pos; i++)
            _data[i] = buf[i];
    }

    // skip }
    skip_spaces( decl );
    if (*decl == '}')
    {
        decl++;
        return true;
    }
    return false;
}

static css_length_t read_length( int * &data )
{
    css_length_t len;
    len.type = (css_value_type_t) (*data++);
    len.value = (*data++);
    return len;
}

void LVCssDeclaration::apply( css_style_rec_t * style )
{
    if (!_data)
        return;
    int * p = _data;
    for (;;)
    {
        switch (*p++)
        {
        case cssd_display:
            style->display = (css_display_t) *p++;
            break;
        case cssd_white_space:
            style->white_space = (css_white_space_t) *p++;
            break;
        case cssd_text_align:
            style->text_align = (css_text_align_t) *p++;
            break;
        case cssd_text_align_last:
            style->text_align_last = (css_text_align_t) *p++;
            break;
        case cssd_text_decoration:
            style->text_decoration = (css_text_decoration_t) *p++;
            break;
        case cssd_hyphenate:
            style->hyphenate = (css_hyphenate_t) *p++;
            break;
        case cssd_list_style_type:
            style->list_style_type = (css_list_style_type_t) *p++;
            break;
        case cssd_list_style_position:
            style->list_style_position = (css_list_style_position_t) *p++;
            break;
        case cssd_page_break_before:
            style->page_break_before = (css_page_break_t) *p++;
            break;
        case cssd_page_break_after:
            style->page_break_after = (css_page_break_t) *p++;
            break;
        case cssd_page_break_inside:
            style->page_break_inside = (css_page_break_t) *p++;
            break;
        case cssd_vertical_align:
            style->vertical_align = (css_vertical_align_t) *p++;
            break;
        case cssd_font_family:
            style->font_family = (css_font_family_t) *p++;
            break;
        case cssd_font_names:
            {
                lString8 names;
                names.reserve(64);
                int len = *p++;
                for (int i=0; i<len; i++)
                    names << (lChar8)(*p++);
                names.pack();
                style->font_name = names;
            }
            break;
        case cssd_font_style:
            style->font_style = (css_font_style_t) *p++;
            break;
        case cssd_font_weight:
            style->font_weight = (css_font_weight_t) *p++;
            break;
        case cssd_font_size:
            style->font_size = read_length( p );
            break;
        case cssd_text_indent:
            style->text_indent = read_length( p );
            break;
        case cssd_line_height:
            style->line_height = read_length( p );
            break;
        case cssd_letter_spacing:
            style->letter_spacing = read_length( p );
            break;
        case cssd_color:
            style->color = read_length( p );
            break;
        case cssd_background_color:
            style->background_color = read_length( p );
            break;
        case cssd_width:
            style->width = read_length( p );
            break;
        case cssd_height:
            style->height = read_length( p );
            break;
        case cssd_margin_left:
            style->margin[0] = read_length( p );
            break;
        case cssd_margin_right:
            style->margin[1] = read_length( p );
            break;
        case cssd_margin_top:
            style->margin[2] = read_length( p );
            break;
        case cssd_margin_bottom:
            style->margin[3] = read_length( p );
            break;
        case cssd_margin:
            style->margin[3] = style->margin[2] = 
                style->margin[1] = style->margin[0] = read_length( p );
            break;
        case cssd_padding_left:
            style->padding[0] = read_length( p );
            break;
        case cssd_padding_right:
            style->padding[1] = read_length( p );
            break;
        case cssd_padding_top:
            style->padding[2] = read_length( p );
            break;
        case cssd_padding_bottom:
            style->padding[3] = read_length( p );
            break;
        case cssd_padding:
            style->padding[3] = style->padding[2] = 
                style->padding[1] = style->padding[0] = read_length( p );
            break;
        case cssd_stop:
            return;
        }
    }
}

lUInt32 LVCssDeclaration::getHash() {
    if (!_data)
        return 0;
    int * p = _data;
    lUInt32 hash = 0;
    for (;*p != cssd_stop;p++)
        hash = hash * 31 + *p;
    return hash;
}

static bool parse_ident( const char * &str, char * ident )
{
    *ident = 0;
    skip_spaces( str );
    if ( !css_is_alpha( *str ) )
        return false;
    int i;
    for (i=0; css_is_alnum(str[i]); i++)
        ident[i] = str[i];
    ident[i] = 0;
    str += i;
    return true;
}

bool LVCssSelectorRule::check( const ldomNode * & node )
{
    if (node->isNull() || node->isRoot())
        return false;
    switch (_type)
    {
    case cssrt_parent:        // E > F
        //
        {
            node = node->getParentNode();
            if (node->isNull())
                return false;
            return node->getNodeId() == _id;
        }
        break;
    case cssrt_ancessor:      // E F
        //
        {
            for (;;)
            {
                node = node->getParentNode();
                if (node->isNull())
                    return false;
                if (node->getNodeId() == _id)
                    return true;
            }
        }
        break;
    case cssrt_predecessor:   // E + F
        //
        {
            int index = node->getNodeIndex();
            // while
            if (index>0) {
                ldomNode * elem = node->getParentNode()->getChildElementNode(index-1, _id);
                if ( elem ) {
                    node = elem;
                    //CRLog::trace("+ selector: found pred element");
                    return true;
                }
                //index--;
            }
            return false;
        }
        break;
    case cssrt_attrset:       // E[foo]
        {
            if ( !node->hasAttributes() )
                return false;
            return node->hasAttribute(_attrid);
        }
        break;
    case cssrt_attreq:        // E[foo="value"]
        {
            lString16 val = node->getAttributeValue(_attrid);
            bool res = (val == _value);
            //if ( res )
            //    return true;
            //CRLog::trace("attreq: %s %s", LCSTR(val), LCSTR(_value) );
            return res;
        }
        break;
    case cssrt_attrhas:       // E[foo~="value"]
        // one of space separated values
        {
            lString16 val = node->getAttributeValue(_attrid);
            int p = val.pos( lString16(_value.c_str()) );            
            if (p<0)
                return false;
            if ( (p>0 && val[p-1]!=' ') 
                    || (p+_value.length()<val.length() && val[p+_value.length()]!=' ') )
                return false;
            return true;
        }
        break;
    case cssrt_attrstarts:    // E[foo|="value"]
        // todo
        {
            lString16 val = node->getAttributeValue(_attrid);
            if (_value.length()>val.length())
                return false;
            val = val.substr(0, _value.length());
            return val == _value;
        }
        break;
    case cssrt_id:            // E#id
        // todo
        {
            lString16 val = node->getAttributeValue(attr_id);
            if (_value.length()>val.length())
                return false;
            return val == _value;
        }
        break;
    case cssrt_class:         // E.class
        // todo
        {
            lString16 val = node->getAttributeValue(attr_class);
            val.lowercase();
//            if ( val.length() != _value.length() )
//                return false;
            //CRLog::trace("attr_class: %s %s", LCSTR(val), LCSTR(_value) );
            return val == _value;
        }
        break;
    case cssrt_universal:     // *
        return true;
    }
    return true;
}

bool LVCssSelector::check( const ldomNode * node ) const
{
    // check main Id
    if (_id!=0 && node->getNodeId() != _id)
        return false;
    if (!_rules)
        return true;
    // check additional rules
    const ldomNode * n = node;
    LVCssSelectorRule * rule = _rules;
    do
    {
        if ( !rule->check(n) )
            return false;
        rule = rule->getNext();
    } while (rule!=NULL);
    return true;
}

bool parse_attr_value( const char * &str, char * buf )
{
    int pos = 0;
    skip_spaces( str );
    if (*str=='\"')
    {
        str++;
        for ( ; str[pos] && str[pos]!='\"'; pos++)
        {
            if (pos>=64)
                return false;
        }
        if (str[pos]!='\"')
            return false;
        for (int i=0; i<pos; i++)
            buf[i] = str[i];
        buf[pos] = 0;
        str += pos+1;
        skip_spaces( str );
        if (*str != ']')
            return false;
        str++;
        return true;
    }
    else
    {
        for ( ; str[pos] && str[pos]!=' ' && str[pos]!='\t' && str[pos]!=']'; pos++)
        {
            if (pos>=64)
                return false;
        }
        if (str[pos]!=']')
            return false;
        for (int i=0; i<pos; i++)
            buf[i] = str[i];
        buf[pos] = 0;
        str+=pos;
        str++;
        return true;
    }
}

LVCssSelectorRule * parse_attr( const char * &str, lxmlDocBase * doc )
{
    char attrname[64];
    char attrvalue[64];
    LVCssSelectorRuleType st = cssrt_universal;
    if (*str=='.') {
        // E.class
        str++;
        skip_spaces( str );
        if (!parse_ident( str, attrvalue ))
            return NULL;
        skip_spaces( str );
        LVCssSelectorRule * rule = new LVCssSelectorRule(cssrt_class);
        lString16 s( attrvalue );
        s.lowercase();
        rule->setAttr(attr_class, s);
        return rule;
    } else if ( *str=='#' ) {
        // E#id
        str++;
        skip_spaces( str );
        if (!parse_ident( str, attrvalue ))
            return NULL;
        skip_spaces( str );
        LVCssSelectorRule * rule = new LVCssSelectorRule(cssrt_id);
        lString16 s( attrvalue );
        rule->setAttr(attr_id, s);
        return rule;
    } else if (*str != '[')
        return NULL;
    str++;
    skip_spaces( str );
    if (!parse_ident( str, attrname ))
        return NULL;
    skip_spaces( str );
    attrvalue[0] = 0;
    if (*str==']')
    {
        st = cssrt_attrset;
        str++;
    }
    else if (*str=='=')
    {
        str++;
        if (!parse_attr_value( str, attrvalue))
            return NULL;
        st = cssrt_attreq;
    }
    else if (*str=='~' && str[1]=='=')
    {
        str+=2;
        if (!parse_attr_value( str, attrvalue))
            return NULL;
        st = cssrt_attrhas;
    }
    else if (*str=='|' && str[1]=='=')
    {
        str+=2;
        if (!parse_attr_value( str, attrvalue))
            return NULL;
        st = cssrt_attrstarts;
    }
    else
    {
        return NULL;
    }
    LVCssSelectorRule * rule = new LVCssSelectorRule(st);
    lString16 s( attrvalue );
    lUInt16 id = doc->getAttrNameIndex( lString16(attrname).c_str() );
    rule->setAttr(id, s);
    return rule;
}

void LVCssSelector::insertRuleStart( LVCssSelectorRule * rule )
{
    rule->setNext( _rules );
    _rules = rule;
}

void LVCssSelector::insertRuleAfterStart( LVCssSelectorRule * rule )
{
    if ( !_rules ) {
        _rules = rule;
        return;
    }
    rule->setNext( _rules->getNext() );
    _rules->setNext( rule );
}

bool LVCssSelector::parse( const char * &str, lxmlDocBase * doc )
{
    if (!str || !*str)
        return false;
    for (;;)
    {
        skip_spaces( str );
        if ( *str == '*' ) // universal selector
        {
            str++;
            skip_spaces( str );
            _id = 0;
        } 
        else if ( *str == '.' ) // classname follows
        {
            _id = 0;
        }
        else if ( css_is_alpha( *str ) )
        {
            // ident
            char ident[64];
            if (!parse_ident( str, ident ))
                return false;
            _id = doc->getElementNameIndex( lString16(ident).c_str() );
            skip_spaces( str );
        }
        else
        {
            return false;
        }
        if ( *str == ',' || *str == '{' )
            return true;
        // one or more attribute rules
        bool attr_rule = false;
        while ( *str == '[' || *str=='.' || *str=='#' )
        {
            LVCssSelectorRule * rule = parse_attr( str, doc );
            if (!rule)
                return false;
            insertRuleStart( rule ); //insertRuleAfterStart
            //insertRuleAfterStart( rule ); //insertRuleAfterStart

            /*
            if ( _id!=0 ) {
                LVCssSelectorRule * rule = new LVCssSelectorRule(cssrt_parent);
                rule->setId(_id);
                insertRuleStart( rule );
                _id=0;
            }
            */

            skip_spaces( str );
            attr_rule = true;
            //continue;
        }
        // element relation
        if (*str == '>')
        {
            str++;
            LVCssSelectorRule * rule = new LVCssSelectorRule(cssrt_parent);
            rule->setId(_id);
            insertRuleStart( rule );
            _id=0;
            continue;
        }
        else if (*str == '+')
        {
            str++;
            LVCssSelectorRule * rule = new LVCssSelectorRule(cssrt_predecessor);
            rule->setId(_id);
            insertRuleStart( rule );
            _id=0;
            continue;
        }
        else if (css_is_alpha( *str ))
        {
            LVCssSelectorRule * rule = new LVCssSelectorRule(cssrt_ancessor);
            rule->setId(_id);
            insertRuleStart( rule );
            _id=0;
            continue;
        }
        if ( !attr_rule )
            return false;
        else if ( *str == ',' || *str == '{' )
            return true;
    }
    return false; // error: end of selector expected
}

static bool skip_until_end_of_rule( const char * &str )
{
    while ( *str && *str!='}' )
        str++;
    if ( *str == '}' )
        str++;
    return *str != 0;
}

LVCssSelectorRule::LVCssSelectorRule( LVCssSelectorRule & v )
: _type(v._type), _id(v._id), _attrid(v._attrid)
, _next(NULL)
, _value( v._value )
{
    if ( v._next )
        _next = new LVCssSelectorRule( *v._next );
}

LVCssSelector::LVCssSelector( LVCssSelector & v )
: _id(v._id), _decl(v._decl), _specificity(v._specificity), _next(NULL), _rules(NULL)
{
    if ( v._next )
        _next = new LVCssSelector( *v._next );
    if ( v._rules )
        _rules = new LVCssSelectorRule( *v._rules );
}

void LVStyleSheet::set(LVPtrVector<LVCssSelector> & v  )
{
    _selectors.clear();
    if ( !v.size() )
        return;
    _selectors.reserve( v.size() );
    for ( int i=0; i<v.size(); i++ ) {
        LVCssSelector * selector = v[i];
        if ( selector )
            _selectors.add( new LVCssSelector( *selector ) );
        else
            _selectors.add( NULL );
    }
}

LVStyleSheet::LVStyleSheet( LVStyleSheet & sheet )
:   _doc( sheet._doc )
{
    set( sheet._selectors );
}

void LVStyleSheet::apply( const ldomNode * node, css_style_rec_t * style )
{
    if (!_selectors.length())
        return; // no rules!
        
    lUInt16 id = node->getNodeId();
    
    LVCssSelector * selector_0 = _selectors[0];
    LVCssSelector * selector_id = id>0 && id<_selectors.length() ? _selectors[id] : NULL;

    for (;;)
    {
        if (selector_0!=NULL)
        {
            if (selector_id==NULL || selector_0->getSpecificity() < selector_id->getSpecificity() )
            {
                // step by sel_0
                selector_0->apply( node, style );
                selector_0 = selector_0->getNext();
            }
            else
            {
                // step by sel_id
                selector_id->apply( node, style );
                selector_id = selector_id->getNext();
            }
        }
        else if (selector_id!=NULL)
        {
            // step by sel_id
            selector_id->apply( node, style );
            selector_id = selector_id->getNext();
        }
        else
        {
            break; // end of chains
        }
    }
}

lUInt32 LVCssSelectorRule::getHash()
{
    lUInt32 hash = 0;
    hash = ( ( ( (lUInt32)_type * 31
        + (lUInt32)_id ) *31 )
        + (lUInt32)_attrid * 31 )
        + ::getHash(_value);
    return hash;
}

lUInt32 LVCssSelector::getHash()
{
    lUInt32 hash = 0;
    lUInt32 nextHash = 0;

    if (_next)
        nextHash = _next->getHash();
    for (LVCssSelectorRule * p = _rules; p; p = p->getNext()) {
        lUInt32 ruleHash = p->getHash();
        hash = hash * 31 + ruleHash;
    }
    hash = hash * 31 + nextHash;
    if (!_decl.isNull())
        hash = hash * 31 + _decl->getHash();
    return hash;
}

/// calculate hash
lUInt32 LVStyleSheet::getHash()
{
    lUInt32 hash = 0;
    for ( int i=0; i<_selectors.length(); i++ ) {
        if ( _selectors[i] )
            hash = hash * 31 + _selectors[i]->getHash() + i*15324;
    }
    return hash;
}

bool LVStyleSheet::parse( const char * str )
{
    LVCssSelector * selector = NULL;
    LVCssSelector * prev_selector;
    int err_count = 0;
    int rule_count = 0;
    for (;*str;)
    {
        // new rule
        prev_selector = NULL;
        bool err = false;
        for (;*str;)
        {
            // parse selector(s)
            selector = new LVCssSelector;
            selector->setNext( prev_selector );
            if ( !selector->parse(str, _doc) )
            {
                err = true;
                break;
            }
            else
            {
                if ( *str == ',' )
                {
                    str++;
                    prev_selector = selector;
                    continue; // next selector
                }
            }
            // parse declaration
            LVCssDeclRef decl( new LVCssDeclaration );
            if ( !decl->parse( str ) )
            {
                err = true;
                err_count++;
            }
            else
            {
                // set decl to selectors
                for (LVCssSelector * p = selector; p; p=p->getNext())
                    p->setDeclaration( decl );
                rule_count++;
            }
            break;
        }
        if (err)
        {
            // error:
            // delete chain of selectors
            delete selector;
            // ignore current rule
            skip_until_end_of_rule( str );
        }
        else
        {
            // Ok:
            // place rules to sheet
            for (LVCssSelector * p = selector; p;  )
            {
                LVCssSelector * item = p;
                p=p->getNext();
                lUInt16 id = item->getElementNameId();
                if (_selectors.length()<=id)
                    _selectors.set(id, NULL);
                // insert with specificity sorting
                if ( _selectors[id] == NULL 
                    || _selectors[id]->getSpecificity() > item->getSpecificity() )
                {
                    // insert as first item
                    item->setNext( _selectors[id] );
                    _selectors[id] = item;
                }
                else
                {
                    // insert as internal item
                    for (LVCssSelector * p = _selectors[id]; p; p = p->getNext() )
                    {
                        if ( p->getNext() == NULL
                            || p->getNext()->getSpecificity() > item->getSpecificity() )
                        {
                            item->setNext( p->getNext() );
                            p->setNext( item );
                            break;
                        }
                    }
                }
            }
        }
    }
    return _selectors.length() > 0;
}

/// extract @import filename from beginning of CSS
bool LVProcessStyleSheetImport( const char * &str, lString8 & import_file )
{
    const char * p = str;
    import_file.clear();
    skip_spaces( p );
    if ( *p !='@' )
        return false;
    p++;
    if (strncmp(p, "import", 6) != 0)
        return false;
    p+=6;
    skip_spaces( p );
    bool in_url = false;
    char quote_ch = 0;
    if ( !strncmp(p, "url", 3) ) {
        p+=3;
        skip_spaces( p );
        if ( *p != '(' )
            return false;
        p++;
        skip_spaces( p );
        in_url = true;
    }
    if ( *p == '\'' || *p=='\"' )
        quote_ch = *p++;
    while (*p) {
        if ( quote_ch && *p==quote_ch ) {
            p++;
            break;
        }
        if ( !quote_ch ) {
            if ( in_url && *p==')' ) {
                break;
            }
            if ( *p==' ' || *p=='\t' || *p=='\r' || *p=='\n' )
                break;
        }
        import_file << *p++;
    }
    skip_spaces( p );
    if ( in_url ) {
        if ( *p!=')' )
            return false;
        p++;
    }
    if ( import_file.empty() )
        return false;
    str = p;
    return true;
}

/// load stylesheet from file, with processing of import
bool LVLoadStylesheetFile( lString16 pathName, lString8 & css )
{
    LVStreamRef file = LVOpenFileStream( pathName.c_str(), LVOM_READ );
    if ( file.isNull() )
        return false;
    lString8 txt = UnicodeToUtf8( LVReadTextFile( file ) );
    lString8 txt2;
    const char * s = txt.c_str();
    lString8 import_file;
    if ( LVProcessStyleSheetImport( s, import_file ) ) {
        lString16 importFilename = LVMakeRelativeFilename( pathName, Utf8ToUnicode(import_file) );
        //lString8 ifn = UnicodeToLocal(importFilename);
        //const char * ifns = ifn.c_str();
        if ( !importFilename.empty() ) {
            LVStreamRef file2 = LVOpenFileStream( importFilename.c_str(), LVOM_READ );
            if ( !file2.isNull() )
                txt2 = UnicodeToUtf8( LVReadTextFile( file2 ) );
        }
    }
    if ( !txt2.empty() )
        txt2 << "\r\n";
    css = txt2 + s;
    return !css.empty();
}
