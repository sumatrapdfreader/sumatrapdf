/** \file lvtextfm.h
    
    \brief Text formatter API

   CoolReader Engine C-compatible text formatter API

   (c) Vadim Lopatin, 2000-2006
   This source code is distributed under the terms of
   GNU General Public License.

   See LICENSE file for details.

*/

#ifndef __LVTEXTFM_H_INCLUDED__
#define __LVTEXTFM_H_INCLUDED__

#include "lvfntman.h"
#include "lvbmpbuf.h"

// comment out following line to use old formatter
#define USE_NEW_FORMATTER 1

#ifdef __cplusplus
extern "C" {
#endif

// text flags
#define LTEXT_ALIGN_LEFT       0x0001  /**< \brief new left-aligned paragraph */
#define LTEXT_ALIGN_RIGHT      0x0002  /**< \brief new right-aligned paragraph */
#define LTEXT_ALIGN_CENTER     0x0003  /**< \brief new centered paragraph */
#define LTEXT_ALIGN_WIDTH      0x0004  /**< \brief new justified paragraph */

#define LTEXT_LAST_LINE_ALIGN_LEFT       0x00010000  /**< \brief last line of justified paragraph should be left-aligned */
#define LTEXT_LAST_LINE_ALIGN_RIGHT      0x00020000  /**< \brief last line of justified paragraph should be right-aligned */
#define LTEXT_LAST_LINE_ALIGN_CENTER     0x00030000  /**< \brief last line of justified paragraph should be centered */
#define LTEXT_LAST_LINE_ALIGN_WIDTH      0x00040000  /**< \brief last line of justified paragraph should be justified */

#define LTEXT_FLAG_NEWLINE     0x0007  /**< \brief new line flags mask */
#define LTEXT_FLAG_OWNTEXT     0x0008  /**< \brief store local copy of text instead of pointer */

#define LTEXT_VALIGN_MASK      0x0070  /**< \brief vertical align flags mask */
#define LTEXT_VALIGN_BASELINE  0x0000  /**< \brief baseline vertical align */
#define LTEXT_VALIGN_SUB       0x0010  /**< \brief subscript */
#define LTEXT_VALIGN_SUPER     0x0020  /**< \brief superscript */

#define LTEXT_TD_UNDERLINE     0x0100  /**< \brief underlined text */
#define LTEXT_TD_OVERLINE      0x0200  /**< \brief overlined text */
#define LTEXT_TD_LINE_THROUGH  0x0400  /**< \brief striked through text */
#define LTEXT_TD_BLINK         0x0800  /**< \brief blinking text */
#define LTEXT_TD_MASK          0x0F00  /**< \brief text decoration mask */

#define LTEXT_SRC_IS_OBJECT    0x8000  /**< \brief object (image) */
#define LTEXT_IS_LINK          0x4000  /**< \brief link */
#define LTEXT_HYPHENATE        0x1000  /**< \brief allow hyphenation */
#define LTEXT_RUNIN_FLAG       0x2000  /**< \brief element display mode is runin */

#define LTEXT_FLAG_PREFORMATTED 0x0080 /**< \brief element space mode is preformatted */


/** \brief Source text line
*/
typedef struct
{
    void *          object;   /**< \brief pointer to object which represents source */
    lInt16          margin;   /**< \brief first line margin */
    lUInt8          interval; /**< \brief line interval, *16 (16=normal, 32=double) */
    lInt8           letter_spacing; /**< \brief additional letter spacing, pixels */
    lUInt32         color;    /**< \brief color */
    lUInt32         bgcolor;  /**< \brief background color */
    lUInt32         flags;    /**< \brief flags */
    lUInt16         index;
    // move unions bottom to simplify debugging
    union {
        struct {
            lvfont_handle   font;     /**< \brief handle of font to draw string */
            const lChar16 * text;     /**< \brief pointer to unicode text string */
            lUInt16         len;      /**< \brief number of chars in text */
            lUInt16         offset;   /**< \brief offset from node start to beginning of line */
        } t;
        struct {
            lUInt16         width;    /**< \brief handle of font to draw string */
            lUInt16         height;   /**< \brief pointer to unicode text string */
        } o;
    };
} src_text_fragment_t;


/** \brief Formatted word
*/
typedef struct
{
   lUInt16  src_text_index;  /**< \brief 00 index of source text line */
   lUInt16  width;           /**< \brief 06 word width, pixels, when at line end */
   lUInt16  x;               /**< \brief 08 word x position in line */
   lInt8    y;               /**< \brief 10 baseline y position */
   lUInt8   flags;           /**< \brief 11 flags */
   lUInt16  inline_width;    /**< \brief 12 word width, pixels when inside line */
    // move unions bottom to simplify debugging
   union {
          /// for text word
       struct {
           lUInt16  start;           /**< \brief 02 position of word in source text */
           lUInt16  len;             /**< \brief 04 number of chars in word */
       } t;
       /// for object
       struct {
           lUInt16  height;           /**< \brief 02 height of image */
       } o;
   };
} formatted_word_t;

/// can add space after this word
#define LTEXT_WORD_CAN_ADD_SPACE_AFTER       1
/// can break line after this word
#define LTEXT_WORD_CAN_BREAK_LINE_AFTER      2
/// can break with hyphenation after this word
#define LTEXT_WORD_CAN_HYPH_BREAK_LINE_AFTER 4
/// must break line after this word
#define LTEXT_WORD_MUST_BREAK_LINE_AFTER     8
/// object flag
#define LTEXT_WORD_IS_OBJECT         0x80
/// first word of link flag
#define LTEXT_WORD_IS_LINK_START     0x40

//#define LTEXT_BACKGROUND_MARK_FLAGS 0xFFFF0000l

/** \brief Text formatter formatted line
*/
typedef struct
{
   formatted_word_t * words;       /**< array of words */
   lUInt32            word_count;  /**< number of words */
   lUInt32            y;           /**< start y position of line */
   lUInt16            x;           /**< start x position */
   lUInt16            width;       /**< width */
   lUInt16            height;      /**< height */
   lUInt16            baseline;    /**< baseline y offset */
   lUInt8             flags;       /**< flags */
   lUInt8             align;       /**< alignment */
} formatted_line_t;

/** \brief Text formatter container
*/
typedef struct
{
   src_text_fragment_t * srctext;       /**< source text lines */
   lUInt32               srctextlen;    /**< number of source text lines */
   formatted_line_t   ** frmlines;      /**< formatted lines */
   lUInt32               frmlinecount;  /**< formatted lines count*/
   lUInt32               height;        /**< height of text fragment */
   lUInt16               width;         /**< width of text fragment */
   lUInt16               page_height;   /**< width of text fragment */
} formatted_text_fragment_t;

/**  Alloc & init formatted text buffer

    \param width is width of formatted text fragment
*/
formatted_text_fragment_t * lvtextAllocFormatter( lUInt16 width );

/** Free formatted text buffer

    dont't forget to call it when object is no longer used

    \param pbuffer is pointer to formatted text buffer
*/
void lvtextFreeFormatter( formatted_text_fragment_t * pbuffer );

/** Add source text line

    Call this function after lvtextInitFormatter for each source fragment
*/
void lvtextAddSourceLine( 
   formatted_text_fragment_t * pbuffer,
   lvfont_handle   font,     /* handle of font to draw string */
   const lChar16 * text,     /* pointer to unicode text string */
   lUInt32         len,      /* number of chars in text, 0 for auto(strlen) */
   lUInt32         color,    /* text color */
   lUInt32         bgcolor,  /* background color */
   lUInt32         flags,    /* flags */
   lUInt8          interval, /* interline space, *16 (16=single, 32=double) */
   lUInt16         margin,   /* first line margin */
   void *          object,   /* pointer to custom object */
   lUInt16         offset,    /* offset from node/object start to start of line */
   lInt8           letter_spacing
                         );

/** Add source object

    Call this function after lvtextInitFormatter for each source fragment
*/
void lvtextAddSourceObject( 
   formatted_text_fragment_t * pbuffer,
   lUInt16         width,
   lUInt16         height,
   lUInt32         flags,    /* flags */
   lUInt8          interval, /* interline space, *16 (16=single, 32=double) */
   lUInt16         margin,   /* first line margin */
   void *          object,    /* pointer to custom object */
   lInt8           letter_spacing
                         );

/** Formats source lines stored in buffer into formatted lines

   \return height (in pixels) of formatted text
*/
lUInt32 lvtextFormat( formatted_text_fragment_t * pbuffer );

/** Reformats source lines stored in buffer into formatted lines

   \return height (in pixels) of formatted text
*/
lUInt32 lvtextResize( formatted_text_fragment_t * pbuffer, int width, int page_height );

/** \brief Draws formatted text to draw buffer (C API) */
void lvtextDraw( formatted_text_fragment_t * text, draw_buf_t * buf, int x, int y );

#ifdef __cplusplus
}

class LVDrawBuf;
class ldomMarkedRangeList;

/* C++ wrapper class */
class LFormattedText
{
    friend class LGrayDrawBuf;
private:
    formatted_text_fragment_t * m_pbuffer;
public:
    formatted_text_fragment_t * GetBuffer() { return m_pbuffer; }

    void Clear()
    { 
        lUInt16 width = m_pbuffer->width;
        lvtextFreeFormatter( m_pbuffer );
        m_pbuffer = lvtextAllocFormatter( width );
    }

    void AddSourceObject(
                lUInt16         flags,    /* flags */
                lUInt8          interval, /* interline space, *16 (16=single, 32=double) */
                lUInt16         margin,   /* first line margin */
                void *          object,    /* pointer to custom object */
                lInt8           letter_spacing=0
         );

    void AddSourceLine(
           const lChar16 * text,        /* pointer to unicode text string */
           lUInt32         len,         /* number of chars in text, 0 for auto(strlen) */
           lUInt32         color,       /* text color */
           lUInt32         bgcolor,     /* background color */
           LVFont          * font,        /* font to draw string */
           lUInt32         flags=LTEXT_ALIGN_LEFT|LTEXT_FLAG_OWNTEXT,
           lUInt8          interval=16, /* interline space, *16 (16=single, 32=double) */
           lUInt16         margin=0,    /* first line margin */
           void *          object=NULL,
           lUInt32         offset=0,
           lInt8           letter_spacing=0
        )
    {
        lvtextAddSourceLine(m_pbuffer, 
            font,  //font->GetHandle()
            text, len, color, bgcolor, 
            flags, interval, margin, object, (lUInt16)offset, letter_spacing );
    }

    lUInt32 FormatOld(lUInt16 width, lUInt16 page_height) { return lvtextResize( m_pbuffer, width, page_height ); }
    lUInt32 FormatNew(lUInt16 width, lUInt16 page_height);
    lUInt32 FormatNew2(lUInt16 width, lUInt16 page_height);

#if (USE_NEW_FORMATTER==1)
    lUInt32 Format(lUInt16 width, lUInt16 page_height) { return FormatNew( width, page_height ); }
#else
    lUInt32 Format(lUInt16 width, lUInt16 page_height) { return FormatOld( width, page_height ); }
#endif

    int GetSrcCount()
    {
        return m_pbuffer->srctextlen;
    }

    int GetWidth()
    {
        return m_pbuffer->width;
    }

    const src_text_fragment_t * GetSrcInfo(int index)
    {
        return &m_pbuffer->srctext[index];
    }

    int GetLineCount()
    {
        return m_pbuffer->frmlinecount;
    }

    const formatted_line_t * GetLineInfo(int index)
    {
        return m_pbuffer->frmlines[index];
    }

    void Draw( LVDrawBuf * buf, int x, int y, ldomMarkedRangeList * marks );

    LFormattedText() { m_pbuffer = lvtextAllocFormatter( 0 ); }

    ~LFormattedText() { lvtextFreeFormatter( m_pbuffer ); }
};

#endif

#endif
