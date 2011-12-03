/** \file lvfntman.h
    \brief font manager interface

    CoolReader Engine

    (c) Vadim Lopatin, 2000-2006

    This source code is distributed under the terms of
    GNU General Public License.

    See LICENSE file for details.

*/

#ifndef __LV_FNT_MAN_H_INCLUDED__
#define __LV_FNT_MAN_H_INCLUDED__

#include <stdlib.h>
#include "crsetup.h"
#include "lvfnt.h"
#include "cssdef.h"
#include "lvstring.h"
#include "lvref.h"
#include "lvptrvec.h"
#include "hyphman.h"
#include "lvdrawbuf.h"

#if !defined(__SYMBIAN32__) && defined(_WIN32)
extern "C" {
#include <windows.h>
}
#endif

class LVDrawBuf;

struct LVFontGlyphCacheItem;

class LVFontGlobalGlyphCache
{
private:
    LVFontGlyphCacheItem * head;
    LVFontGlyphCacheItem * tail;
    int size;
    int max_size;
public:
    LVFontGlobalGlyphCache( int maxSize )
        : head(NULL), tail(NULL), size(0), max_size(maxSize )
    {
    }
    ~LVFontGlobalGlyphCache()
    {
        clear();
    }
    void put( LVFontGlyphCacheItem * item );
    void remove( LVFontGlyphCacheItem * item );
    void refresh( LVFontGlyphCacheItem * item );
    void clear();
};

class LVFontLocalGlyphCache
{
private:
    LVFontGlyphCacheItem * head;
    LVFontGlyphCacheItem * tail;
    LVFontGlobalGlyphCache * global_cache;
    int size;
public:
    LVFontLocalGlyphCache( LVFontGlobalGlyphCache * globalCache )
        : head(NULL), tail(NULL), global_cache( globalCache )
    { }
    ~LVFontLocalGlyphCache()
    {
        clear();
    }
    void clear();
    LVFontGlyphCacheItem * get( lUInt16 ch );
    void put( LVFontGlyphCacheItem * item );
    void remove( LVFontGlyphCacheItem * item );
};

struct LVFontGlyphCacheItem
{
    LVFontGlyphCacheItem * prev_global;
    LVFontGlyphCacheItem * next_global;
    LVFontGlyphCacheItem * prev_local;
    LVFontGlyphCacheItem * next_local;
    LVFontLocalGlyphCache * local_cache;
    lChar16 ch;
    lUInt8 bmp_width;
    lUInt8 bmp_height;
    lInt8  origin_x;
    lInt8  origin_y;
    lUInt8 advance;
    lUInt8 bmp[1];
    //=======================================================================
    int getSize()
    {
        return sizeof(LVFontGlyphCacheItem)
            + (bmp_width * bmp_height - 1) * sizeof(lUInt8);
    }
    static LVFontGlyphCacheItem * newItem( LVFontLocalGlyphCache * local_cache, lChar16 ch, int w, int h )
    {
        LVFontGlyphCacheItem * item = (LVFontGlyphCacheItem *)malloc( sizeof(LVFontGlyphCacheItem)
            + (w*h - 1)*sizeof(lUInt8) );
        item->ch = ch;
        item->bmp_width = w;
        item->bmp_height = h;
        item->origin_x =   0;
        item->origin_y =   0;
        item->advance =    0;
        item->prev_global = NULL;
        item->next_global = NULL;
        item->prev_local = NULL;
        item->next_local = NULL;
        item->local_cache = local_cache;
        return item;
    }
    static void freeItem( LVFontGlyphCacheItem * item )
    {
        free( item );
    }
};


/** \brief base class for fonts

    implements single interface for font of any engine
*/
class LVFont : public LVRefCounter
{
protected:
    int _visual_alignment_width;
public:
    lUInt32 _hash;
    /// glyph properties structure
    struct glyph_info_t {
        lUInt8  blackBoxX;   ///< 0: width of glyph
        lUInt8  blackBoxY;   ///< 1: height of glyph black box
        lInt8   originX;     ///< 2: X origin for glyph
        lInt8   originY;     ///< 3: Y origin for glyph
        lUInt8  width;       ///< 4: full width of glyph
    };

    /// hyphenation character
    virtual lChar16 getHyphChar() { return UNICODE_SOFT_HYPHEN_CODE; }

    /// hyphen width
    virtual int getHyphenWidth() { return getCharWidth( getHyphChar() ); }

    /**
     * Max width of -/./,/!/? to use for visial alignment by width
     */
    virtual int getVisualAligmentWidth();

    /** \brief get glyph info
        \param glyph is pointer to glyph_info_t struct to place retrieved info
        \return true if glyh was found 
    */
    virtual bool getGlyphInfo( lUInt16 code, glyph_info_t * glyph, lChar16 def_char=0 ) = 0;

    /** \brief measure text
        \param text is text string pointer
        \param len is number of characters to measure
        \param max_width is maximum width to measure line 
        \param def_char is character to replace absent glyphs in font
        \param letter_spacing is number of pixels to add between letters
        \return number of characters before max_width reached 
    */
    virtual lUInt16 measureText( 
                        const lChar16 * text, int len, 
                        lUInt16 * widths,
                        lUInt8 * flags,
                        int max_width,
                        lChar16 def_char,
                        int letter_spacing=0,
                        bool allow_hyphenation=true
                     ) = 0;
    /** \brief measure text
        \param text is text string pointer
        \param len is number of characters to measure
        \return width of specified string 
    */
    virtual lUInt32 getTextWidth(
                        const lChar16 * text, int len
        ) = 0;

//    /** \brief get glyph image in 1 byte per pixel format
//        \param code is unicode character
//        \param buf is buffer [width*height] to place glyph data
//        \return true if glyph was found
//    */
//    virtual bool getGlyphImage(lUInt16 code, lUInt8 * buf, lChar16 def_char=0) = 0;
    /** \brief get glyph item
        \param code is unicode character
        \return glyph pointer if glyph was found, NULL otherwise
    */
    virtual LVFontGlyphCacheItem * getGlyph(lUInt16 ch, lChar16 def_char=0) = 0;
    /// returns font baseline offset
    virtual int getBaseline() = 0;
    /// returns font height including normal interline space
    virtual int getHeight() const = 0;
    /// returns font character size
    virtual int getSize() const = 0;
    /// returns font weight
    virtual int getWeight() const = 0;
    /// returns italic flag
    virtual int getItalic() const = 0;
    /// returns char width
    virtual int getCharWidth( lChar16 ch, lChar16 def_char=0 ) = 0;
    /// retrieves font handle
    virtual void * GetHandle() = 0;
    /// returns font typeface name
    virtual lString8 getTypeFace() const = 0;
    /// returns font family id
    virtual css_font_family_t getFontFamily() const = 0;
    /// draws text string
    virtual void DrawTextString( LVDrawBuf * buf, int x, int y, 
                       const lChar16 * text, int len, 
                       lChar16 def_char, lUInt32 * palette = NULL, bool addHyphen = false,
                       lUInt32 flags=0, int letter_spacing=0 ) = 0;
    /// constructor
    LVFont() : _visual_alignment_width(-1), _hash(0) { }

    /// get bitmap mode (true=monochrome bitmap, false=antialiased)
    virtual bool getBitmapMode() { return false; }
    /// set bitmap mode (true=monochrome bitmap, false=antialiased)
    virtual void setBitmapMode( bool ) { }

    /// get kerning mode: true==ON, false=OFF
    virtual bool getKerning() const { return false; }
    /// get kerning mode: true==ON, false=OFF
    virtual void setKerning( bool ) { }

    /// returns true if font is empty
    virtual bool IsNull() const = 0;
    virtual bool operator ! () const = 0;
    virtual void Clear() = 0;
    virtual ~LVFont() { }
    /// set fallback font for this font
    void setFallbackFont( LVFastRef<LVFont> font ) { }
    /// get fallback font for this font
    LVFont * getFallbackFont() { return NULL; }
};

typedef LVFastRef<LVFont> LVFontRef;


enum font_antialiasing_t
{
    font_aa_none,
    font_aa_big,
    font_aa_all
};

/// font manager interface class
class LVFontManager
{
protected:
    int _antialiasMode;
    bool _allowKerning;
public:
    /// garbage collector frees unused fonts
    virtual void gc() = 0;
    /// returns most similar font
    virtual LVFontRef GetFont(int size, int weight, bool italic, css_font_family_t family, lString8 typeface ) = 0;
    /// set fallback font face (returns true if specified font is found)
    virtual bool SetFallbackFontFace( lString8 face ) { return false; }
    /// get fallback font face (returns empty string if no fallback font is set)
    virtual lString8 GetFallbackFontFace() { return lString8::empty_str; }
    /// returns fallback font for specified size
    virtual LVFontRef GetFallbackFont(int size) { return LVFontRef(); }
    /// registers font by name
    virtual bool RegisterFont( lString8 name ) = 0;
    /// initializes font manager
    virtual bool Init( lString8 path ) = 0;
    /// get count of registered fonts
    virtual int GetFontCount() = 0;
    /// get hash of installed fonts and fallback font
    virtual lUInt32 GetFontListHash() { return 0; }
    /// clear glyph cache
    virtual void clearGlyphCache() { }

    /// get antialiasing mode
    virtual int GetAntialiasMode() { return _antialiasMode; }
    /// set antialiasing mode
    virtual void SetAntialiasMode( int mode ) { _antialiasMode = mode; gc(); clearGlyphCache(); }

    /// get kerning mode: true==ON, false=OFF
    virtual bool getKerning() { return _allowKerning; }
    /// get kerning mode: true==ON, false=OFF
    virtual void setKerning( bool kerningEnabled ) { _allowKerning = kerningEnabled; gc(); clearGlyphCache(); }

    /// constructor
    LVFontManager() : _antialiasMode(font_aa_all), _allowKerning(false) { }
    /// destructor
    virtual ~LVFontManager() { }
    /// returns available typefaces
    virtual void getFaceList( lString16Collection & ) { }

    /// fills array with list of available gamma levels
    void GetGammaLevels(LVArray<double> dst);
    /// returns current gamma level index
    int  GetGammaIndex();
    /// sets current gamma level index
    void SetGammaIndex( int gammaIndex );
    /// returns current gamma level
    double GetGamma();
    /// sets current gamma level
    void SetGamma( double gamma );
};

class LVBaseFont : public LVFont
{
protected:
    lString8 _typeface;
    css_font_family_t _family;
public:
    /// returns font typeface name
    virtual lString8 getTypeFace() { return _typeface; }
    /// returns font family id
    virtual css_font_family_t getFontFamily() { return _family; }
    /// draws text string
    virtual void DrawTextString( LVDrawBuf * buf, int x, int y, 
                       const lChar16 * text, int len, 
                       lChar16 def_char, lUInt32 * palette, bool addHyphen, lUInt32 flags=0, int letter_spacing=0 );
};

#if (USE_FREETYPE!=1) && (USE_BITMAP_FONTS==1)
/* C++ wrapper class */
class LBitmapFont : public LVBaseFont
{
private:
    lvfont_handle m_font;
public:
    LBitmapFont() : m_font(NULL) { }
    virtual bool getGlyphInfo( lUInt16 code, LVFont::glyph_info_t * glyph, lChar16 def_char=0 );
    virtual lUInt16 measureText( 
                        const lChar16 * text, int len, 
                        lUInt16 * widths,
                        lUInt8 * flags,
                        int max_width,
                        lChar16 def_char,
                        int letter_spacing=0
                     );
    /** \brief measure text
        \param text is text string pointer
        \param len is number of characters to measure
        \return width of specified string 
    */
    virtual lUInt32 getTextWidth(
                        const lChar16 * text, int len
        );
    /// returns font baseline offset
    virtual int getBaseline();
    /// returns font height
    virtual int getHeight() const;
    
    virtual bool getGlyphImage(lUInt16 code, lUInt8 * buf, lChar16 def_char=0 );
    
    /// returns char width
    virtual int getCharWidth( lChar16 ch, lChar16 def_char=0 )
    {
        glyph_info_t glyph;
        if ( getGlyphInfo(ch, &glyph, def_char ) )
            return glyph.width;
        return 0;
    }

    virtual lvfont_handle GetHandle() { return m_font; }
    
    int LoadFromFile( const char * fname );
    
    // LVFont functions overrides
    virtual void Clear() { if (m_font) lvfontClose( m_font ); m_font = NULL; }
    virtual bool IsNull() const { return m_font==NULL; }
    virtual bool operator ! () const { return IsNull(); }
    virtual ~LBitmapFont() { Clear(); }
};
#endif

#if !defined(__SYMBIAN32__) && defined(_WIN32) && USE_FREETYPE!=1
class LVBaseWin32Font : public LVBaseFont
{
protected:
    HFONT   _hfont;
    LOGFONTA _logfont;
    int     _height;
    int     _baseline;
    LVColorDrawBuf _drawbuf;
    
public:    

    LVBaseWin32Font() : _hfont(NULL), _height(0), _baseline(0), _drawbuf(1,1) 
        { }
        
    virtual ~LVBaseWin32Font() { Clear(); }

    /// returns font baseline offset
    virtual int getBaseline()
    {
        return _baseline;
    }
    
    /// returns font height
    virtual int getHeight() const
    {
        return _height;
    }
    
    /// retrieves font handle
    virtual void * GetHandle()
    {
        return (void*)_hfont;
    }
    
    /// returns char width
    virtual int getCharWidth( lChar16 ch, lChar16 def_char=0 )
    {
        glyph_info_t glyph;
        if ( getGlyphInfo(ch, &glyph, def_char) )
            return glyph.width;
        return 0;
    }
    /// returns true if font is empty
    virtual bool IsNull() const 
    {
        return (_hfont == NULL);
    }
    
    virtual bool operator ! () const
    {
        return (_hfont == NULL);
    }
    
    virtual void Clear();

    virtual bool Create( const LOGFONTA & lf );

    virtual bool Create(int size, int weight, bool italic, css_font_family_t family, lString8 typeface );
    
    virtual int getWeight() const {
        return _logfont.lfWeight;
    }

    virtual int getItalic() const {
        return _logfont.lfItalic;
    }

    virtual lString8 getTypeFace() const {
        return lString8();
    }

    virtual css_font_family_t getFontFamily() const {
        return css_ff_inherit;
    }

    virtual LVFontGlyphCacheItem * getGlyph(lUInt16 ch, lChar16 def_char=0) {
        return NULL;
    }

    virtual int getSize() const {
        return 0;
    }

};


class LVWin32DrawFont : public LVBaseWin32Font
{
private:
    int _hyphen_width;
public:

    LVWin32DrawFont() : _hyphen_width(0) { }

    /** \brief get glyph info
        \param glyph is pointer to glyph_info_t struct to place retrieved info
        \return true if glyh was found 
    */
    virtual bool getGlyphInfo( lUInt16 code, glyph_info_t * glyph, lChar16 def_char=0 );

    /** \brief measure text
        \param glyph is pointer to glyph_info_t struct to place retrieved info
        \return true if glyph was found 
    */
    virtual lUInt16 measureText( 
                        const lChar16 * text, int len, 
                        lUInt16 * widths,
                        lUInt8 * flags,
                        int max_width,
                        lChar16 def_char,
                        int letter_spacing=0,
                        bool allow_hyphenation=true
                     );
    /** \brief measure text
        \param text is text string pointer
        \param len is number of characters to measure
        \return width of specified string 
    */
    virtual lUInt32 getTextWidth(
                        const lChar16 * text, int len
        );

    /// returns char width
    virtual int getCharWidth( lChar16 ch, lChar16 def_char=0 );

    /// draws text string
    virtual void DrawTextString( LVDrawBuf * buf, int x, int y, 
                       const lChar16 * text, int len, 
                       lChar16 def_char, lUInt32 * palette, bool addHyphen, lUInt32 flags=0, int letter_spacing=0 );
        
    /** \brief get glyph image in 1 byte per pixel format
        \param code is unicode character
        \param buf is buffer [width*height] to place glyph data
        \return true if glyph was found 
    */
    virtual bool getGlyphImage(lUInt16 code, lUInt8 * buf, lChar16 def_char=0);
    
};

struct glyph_t {
    lUInt8 *     glyph;
    lChar16      ch;
    bool         flgNotExists;
    bool         flgValid;
    LVFont::glyph_info_t gi;
    glyph_t *    next;
    glyph_t(lChar16 c)
    : glyph(NULL), ch(c), flgNotExists(false), flgValid(false), next(NULL)
    {
        memset( &gi, 0, sizeof(gi) );
    }
    ~glyph_t()
    {
        if (glyph)
            delete glyph;
    }
};

class GlyphCache
{
private:
    lUInt32 _size;
    glyph_t * * _hashtable;        
public:
    GlyphCache( lUInt32 size )
    : _size(size)
    {
        _hashtable = new glyph_t * [_size];
        for (lUInt32 i=0; i<_size; i++)
            _hashtable[i] = NULL;
    }
    void clear()
    {
        for (lUInt32 i=0; i<_size; i++)
        {
            glyph_t * p = _hashtable[i];
            while (p)
            {
                glyph_t * next = p->next;
                delete p;
                p = next;
            }
            _hashtable[i] = NULL;
        }
    }
    ~GlyphCache()
    {
        if (_hashtable)
        {
            clear();
            delete _hashtable;
        }
    }
    glyph_t * find( lChar16 ch )
    {
        lUInt32 index = (((lUInt32)ch)*113) % _size;
        glyph_t * p = _hashtable[index];
        // 3 levels
        if (!p)
            return NULL;
        if (p->ch == ch)
            return p;
        p = p->next;
        if (!p)
            return NULL;
        if (p->ch == ch)
            return p;
        p = p->next;
        if (!p)
            return NULL;
        if (p->ch == ch)
            return p;
        return NULL;
    }
    /// returns found or creates new
    glyph_t * get( lChar16 ch )
    {
        lUInt32 index = (((lUInt32)ch)*113) % _size;
        glyph_t * * p = &_hashtable[index];
        // 3 levels
        if (!*p)
        {
            return (*p = new glyph_t(ch));
        }
        if ((*p)->ch == ch)
        {
            return *p;
        }
        p = &(*p)->next;
        if (!*p)
        {
            return (*p = new glyph_t(ch));
        }
        if ((*p)->ch == ch)
        {
            return *p;
        }
        p = &(*p)->next;
        if (!*p)
        {
            return (*p = new glyph_t(ch));
        }
        if ((*p)->ch == ch)
        {
            return *p;
        }

        delete (*p);
        *p = NULL;

        glyph_t * pp = new glyph_t(ch);
        pp->next = _hashtable[index];
        _hashtable[index] = pp;
        return pp;
    }
    
};

class LVWin32Font : public LVBaseWin32Font
{
private:    
    lChar16 _unknown_glyph_index;
    GlyphCache _cache;
    
    static int GetGlyphIndex( HDC hdc, wchar_t code );
    
    glyph_t * GetGlyphRec( lChar16 ch );

public:
    /** \brief get glyph info
        \param glyph is pointer to glyph_info_t struct to place retrieved info
        \return true if glyh was found 
    */
    virtual bool getGlyphInfo( lUInt16 code, glyph_info_t * glyph, lChar16 def_char=0 );

    /** \brief measure text
        \param glyph is pointer to glyph_info_t struct to place retrieved info
        \return true if glyph was found 
    */
    virtual lUInt16 measureText( 
                        const lChar16 * text, int len, 
                        lUInt16 * widths,
                        lUInt8 * flags,
                        int max_width,
                        lChar16 def_char,
                        int letter_spacing=0,
                        bool allow_hyphenation=true
                     );
    /** \brief measure text
        \param text is text string pointer
        \param len is number of characters to measure
        \return width of specified string 
    */
    virtual lUInt32 getTextWidth(
                        const lChar16 * text, int len
        );

    /** \brief get glyph image in 1 byte per pixel format
        \param code is unicode character
        \param buf is buffer [width*height] to place glyph data
        \return true if glyph was found 
    */
    virtual bool getGlyphImage(lUInt16 code, lUInt8 * buf, lChar16 def_char=0);
    
    virtual void Clear();

    virtual bool Create( const LOGFONTA & lf );

    virtual bool Create(int size, int weight, bool italic, css_font_family_t family, lString8 typeface );

    LVWin32Font() : _cache(256) {  }
    
    virtual ~LVWin32Font() { }
};

#endif


#define LVFONT_TRANSFORM_EMBOLDEN 1
/// create transform for font
LVFontRef LVCreateFontTransform( LVFontRef baseFont, int transformFlags );

/// current font manager pointer
extern LVFontManager * fontMan;

/// initializes font manager
bool InitFontManager( lString8 path );

/// deletes font manager
bool ShutdownFontManager();

LVFontRef LoadFontFromFile( const char * fname );

/// to compare two fonts
bool operator == (const LVFont & r1, const LVFont & r2);

#endif //__LV_FNT_MAN_H_INCLUDED__
