/*******************************************************

   CoolReader Engine

   lvstyles.cpp:  CSS styles hash

   (c) Vadim Lopatin, 2000-2006
   This source code is distributed under the terms of
   GNU General Public License
   See LICENSE file for details

*******************************************************/

#include "../include/lvstyles.h"


//DEFINE_NULL_REF( css_style_rec_t )

/// calculate font instance record hash
lUInt32 calcHash(font_ref_t & f)
{
    if ( !f )
        return 14321;
    if ( f->_hash )
        return f->_hash;
    lUInt32 v = 31;
    v = v * 31 + (lUInt32)f->getFontFamily();
    v = v * 31 + (lUInt32)f->getSize();
    v = v * 31 + (lUInt32)f->getWeight();
    v = v * 31 + (lUInt32)f->getItalic();
    v = v * 31 + (lUInt32)f->getKerning();
    v = v * 31 + (lUInt32)f->getBitmapMode();
    v = v * 31 + (lUInt32)f->getTypeFace().getHash();
    v = v * 31 + (lUInt32)f->getBaseline();
    f->_hash = v;
    return v;
}


lUInt32 calcHash(css_style_rec_t & rec)
{
    if ( !rec.hash )
        rec.hash = (((((((((((((((((((((((((((((((lUInt32)rec.display * 31
         + (lUInt32)rec.white_space) * 31
         + (lUInt32)rec.text_align) * 31
         + (lUInt32)rec.text_align_last) * 31
         + (lUInt32)rec.text_decoration) * 31
         + (lUInt32)rec.hyphenate) * 31
         + (lUInt32)rec.list_style_type) * 31
         + (lUInt32)rec.letter_spacing.pack()) * 31
         + (lUInt32)rec.list_style_position) * 31
         + (lUInt32)(rec.page_break_before | (rec.page_break_before<<4) | (rec.page_break_before<<8))) * 31
         + (lUInt32)rec.vertical_align) * 31
         + (lUInt32)rec.font_size.type) * 31
         + (lUInt32)rec.font_size.value) * 31
         + (lUInt32)rec.font_style) * 31
         + (lUInt32)rec.font_weight) * 31
         + (lUInt32)rec.line_height.pack()) * 31
         + (lUInt32)rec.color.pack()) * 31
         + (lUInt32)rec.background_color.pack()) * 31
         + (lUInt32)rec.width.pack()) * 31
         + (lUInt32)rec.height.pack()) * 31
         + (lUInt32)rec.text_indent.pack()) * 31
         + (lUInt32)rec.margin[0].pack()) * 31
         + (lUInt32)rec.margin[1].pack()) * 31
         + (lUInt32)rec.margin[2].pack()) * 31
         + (lUInt32)rec.margin[3].pack()) * 31
         + (lUInt32)rec.padding[0].pack()) * 31
         + (lUInt32)rec.padding[1].pack()) * 31
         + (lUInt32)rec.padding[2].pack()) * 31
         + (lUInt32)rec.padding[3].pack()) * 31
         + (lUInt32)rec.font_family) * 31
         + (lUInt32)rec.font_name.getHash());
    return rec.hash;
}

bool operator == (const css_style_rec_t & r1, const css_style_rec_t & r2)
{
    return 
           r1.display == r2.display &&
           r1.white_space == r2.white_space &&
           r1.text_align == r2.text_align &&
           r1.text_align_last == r2.text_align_last &&
           r1.text_decoration == r2.text_decoration &&
           r1.list_style_type == r2.list_style_type &&
           r1.list_style_position == r2.list_style_position &&
           r1.hyphenate == r2.hyphenate &&
           r1.vertical_align == r2.vertical_align &&
           r1.line_height == r2.line_height &&
           r1.width == r2.width &&
           r1.height == r2.height &&
           r1.color == r2.color &&
           r1.background_color == r2.background_color &&
           r1.text_indent == r2.text_indent &&
           r1.margin[0] == r2.margin[0] &&
           r1.margin[1] == r2.margin[1] &&
           r1.margin[2] == r2.margin[2] &&
           r1.margin[3] == r2.margin[3] &&
           r1.padding[0] == r2.padding[0] &&
           r1.padding[1] == r2.padding[1] &&
           r1.padding[2] == r2.padding[2] &&
           r1.padding[3] == r2.padding[3] &&
           r1.font_size.type == r2.font_size.type &&
           r1.font_size.value == r2.font_size.value &&
           r1.font_style == r2.font_style &&
           r1.font_weight == r2.font_weight &&
           r1.font_name == r2.font_name &&
           r1.font_family == r2.font_family;
}


/// splits string like "Arial", Times New Roman, Courier;  into list
// returns number of characters processed
int splitPropertyValueList( const char * str, lString8Collection & list )
{
    //
    int i=0;
    lChar8 quote_char = 0;
    lString8 name;
    name.reserve(32);
    bool last_space = false;
    for (i=0; str[i]; i++)
    {
        switch(str[i])
        {
        case '\'':
        case '\"':
            {
                if (quote_char==0)
                {
                    if (!name.empty())
                    {
                        list.add( name );
                        name.clear();
                    }
                    quote_char = str[i];
                }
                else if (quote_char==str[i])
                {
                    if (!name.empty())
                    {
                        list.add( name );
                        name.clear();
                    }
                    quote_char = 0;
                }
                else
                {
                    // append char
                    name << str[i];
                }
                last_space = false;
            }
            break;
        case ',':
            {
                if (quote_char==0)
                {
                    if (!name.empty())
                    {
                        list.add( name );
                        name.clear();
                    }
                }
                else
                {
                    // inside quotation: append char
                    name << str[i];
                }
                last_space = false;
            }
            break;
        case '\t':
        case ' ':
            {
                if (quote_char!=0)
                {
                    name << str[i];
                }
                last_space = true;
            }
            break;
        case ';':
        case '}':
                if (quote_char==0)
                {
                    if (!name.empty())
                    {
                        list.add( name );
                        name.clear();
                    }
                    return i;
                }
                else
                {
                    // inside quotation: append char
                    name << str[i];
                    last_space = false;
                }
            break;
        default:
            if (last_space && !name.empty() && quote_char==0)
                name << ' ';
            name += str[i];
            last_space = false;
            break;
        }
    }
    if (!name.empty())
    {
        list.add( name );
    }
    return i;
}

/// splits string like "Arial", Times New Roman, Courier  into list
lString8 joinPropertyValueList( const lString8Collection & list )
{
    lString8 res;
    res.reserve(100);
    
    for (unsigned i=0; i<list.length(); i++)
    {
        if (i>0)
            res << ", ";
        res << "\"" << list[i] << "\"";
    }
    
    res.pack();
    return res;
}

static const char * style_magic = "CR3STYLE";
#define ST_PUT_ENUM(v) buf << (lUInt8)v
#define ST_GET_ENUM(t,v) { lUInt8 tmp; buf >> tmp; v=(t)tmp; if (buf.error()) return false; }
#define ST_PUT_LEN(v) buf << (lUInt8)v.type << (lInt32)v.value;
#define ST_GET_LEN(v) { lUInt8 t; buf >> t; lInt32 val; buf >> val; v.type = (css_value_type_t)t; v.value = val; if (buf.error()) return false; }
#define ST_PUT_LEN4(v) ST_PUT_LEN(v[0]);ST_PUT_LEN(v[1]);ST_PUT_LEN(v[2]);ST_PUT_LEN(v[3]);
#define ST_GET_LEN4(v) ST_GET_LEN(v[0]);ST_GET_LEN(v[1]);ST_GET_LEN(v[2]);ST_GET_LEN(v[3]);
bool css_style_rec_t::serialize( SerialBuf & buf )
{
    if ( buf.error() )
        return false;
    buf.putMagic(style_magic);
    ST_PUT_ENUM(display);           //    css_display_t        display;
    ST_PUT_ENUM(white_space);       //    css_white_space_t    white_space;
    ST_PUT_ENUM(text_align);        //    css_text_align_t     text_align;
    ST_PUT_ENUM(text_align_last);   //    css_text_align_t     text_align_last;
    ST_PUT_ENUM(text_decoration);   //    css_text_decoration_t text_decoration;
    ST_PUT_ENUM(vertical_align);    //    css_vertical_align_t vertical_align;
    ST_PUT_ENUM(font_family);       //    css_font_family_t    font_family;
    buf << font_name;               //    lString8             font_name;
    ST_PUT_LEN(font_size);          //    css_length_t         font_size;
    ST_PUT_ENUM(font_style);        //    css_font_style_t     font_style;
    ST_PUT_ENUM(font_weight);       //    css_font_weight_t    font_weight;
    ST_PUT_LEN(text_indent);        //    css_length_t         text_indent;
    ST_PUT_LEN(line_height);        //    css_length_t         line_height;
    ST_PUT_LEN(width);              //    css_length_t         width;
    ST_PUT_LEN(height);             //    css_length_t         height;
    ST_PUT_LEN4(margin);            //    css_length_t         margin[4]; ///< margin-left, -right, -top, -bottom
    ST_PUT_LEN4(padding);           //    css_length_t         padding[4]; ///< padding-left, -right, -top, -bottom
    ST_PUT_LEN(color);              //    css_length_t         color;
    ST_PUT_LEN(background_color);   //    css_length_t         background_color;
    ST_PUT_LEN(letter_spacing);     //    css_length_t         letter_spacing;
    ST_PUT_ENUM(page_break_before); //    css_page_break_t     page_break_before;
    ST_PUT_ENUM(page_break_after);  //    css_page_break_t     page_break_after;
    ST_PUT_ENUM(page_break_inside); //    css_page_break_t     page_break_inside;
    ST_PUT_ENUM(hyphenate);         //    css_hyphenate_t      hyphenate;
    ST_PUT_ENUM(list_style_type);   //    css_list_style_type_t list_style_type;
    ST_PUT_ENUM(list_style_position);//    css_list_style_position_t list_style_position;
    lUInt32 hash = calcHash(*this);
    buf << hash;
    return !buf.error();
}

bool css_style_rec_t::deserialize( SerialBuf & buf )
{
    if ( buf.error() )
        return false;
    buf.putMagic(style_magic);
    ST_GET_ENUM(css_display_t, display);                    //    css_display_t        display;
    ST_GET_ENUM(css_white_space_t, white_space);            //    css_white_space_t    white_space;
    ST_GET_ENUM(css_text_align_t, text_align);              //    css_text_align_t     text_align;
    ST_GET_ENUM(css_text_align_t, text_align_last);         //    css_text_align_t     text_align_last;
    ST_GET_ENUM(css_text_decoration_t, text_decoration);    //    css_text_decoration_t text_decoration;
    ST_GET_ENUM(css_vertical_align_t, vertical_align);      //    css_vertical_align_t vertical_align;
    ST_GET_ENUM(css_font_family_t, font_family);            //    css_font_family_t    font_family;
    buf >> font_name;                                       //    lString8             font_name;
    ST_GET_LEN(font_size);                                  //    css_length_t         font_size;
    ST_GET_ENUM(css_font_style_t, font_style);              //    css_font_style_t     font_style;
    ST_GET_ENUM(css_font_weight_t, font_weight);            //    css_font_weight_t    font_weight;
    ST_GET_LEN(text_indent);                                //    css_length_t         text_indent;
    ST_GET_LEN(line_height);                                //    css_length_t         line_height;
    ST_GET_LEN(width);                                      //    css_length_t         width;
    ST_GET_LEN(height);                                     //    css_length_t         height;
    ST_GET_LEN4(margin);                                    //    css_length_t         margin[4]; ///< margin-left, -right, -top, -bottom
    ST_GET_LEN4(padding);                                   //    css_length_t         padding[4]; ///< padding-left, -right, -top, -bottom
    ST_GET_LEN(color);                                      //    css_length_t         color;
    ST_GET_LEN(background_color);                           //    css_length_t         background_color;
    ST_GET_LEN(letter_spacing);                             //    css_length_t         letter_spacing;
    ST_GET_ENUM(css_page_break_t, page_break_before);       //    css_page_break_t     page_break_before;
    ST_GET_ENUM(css_page_break_t, page_break_after);        //    css_page_break_t     page_break_after;
    ST_GET_ENUM(css_page_break_t, page_break_inside);       //    css_page_break_t     page_break_inside;
    ST_GET_ENUM(css_hyphenate_t, hyphenate);                //    css_hyphenate_t        hyphenate;
    ST_GET_ENUM(css_list_style_type_t, list_style_type);    //    css_list_style_type_t list_style_type;
    ST_GET_ENUM(css_list_style_position_t, list_style_position);//    css_list_style_position_t list_style_position;
    lUInt32 hash = 0;
    buf >> hash;
    lUInt32 newhash = calcHash(*this);
    if ( hash!=newhash )
        buf.seterror();
    return !buf.error();
}
