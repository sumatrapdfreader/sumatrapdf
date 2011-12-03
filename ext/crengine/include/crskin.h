/***************************************************************************
 *   Copyright (C) 2008 by Vadim Lopatin                                   *
 *   vadim.lopatin@coolreader.org                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#ifndef CR_SKIN_INCLUDED
#define CR_SKIN_INCLUDED

#include "lvtypes.h"
#include "lvptrvec.h"
#include "lvref.h"
#include "lvstring.h"
#include "lvfntman.h"
#include "lvdrawbuf.h"
#include "lvtinydom.h"


// Vertical alignment flags
#define SKIN_VALIGN_MASK    0x0003
#define SKIN_VALIGN_TOP     0x0001
#define SKIN_VALIGN_CENTER  0x0000
#define SKIN_VALIGN_BOTTOM  0x0002

// Horizontal alignment flags
#define SKIN_HALIGN_MASK    0x0030
#define SKIN_HALIGN_LEFT    0x0000
#define SKIN_HALIGN_CENTER  0x0010
#define SKIN_HALIGN_RIGHT   0x0020

#define SKIN_EXTEND_TAB     0x0040

#define SKIN_WORD_WRAP      0x0080

#define SKIN_COORD_PERCENT_FLAG 0x10000000

/// encodes percent value*100 (0..10000), to store in skin
inline int toSkinPercent( int x )
{
    return ((x)|SKIN_COORD_PERCENT_FLAG);
}

/// encodes percent value*100 (0..10000), to store in skin, from string like "75%" or "10"
int toSkinPercent( const lString16 & value, int defValue, bool * res );

/// decodes skin percent to pixels (fullx is value corresponding to 100%)
int fromSkinPercent( int x, int fullx );

/// decodes skin percent point to pixels (fullx is value corresponding to 100%)
lvPoint fromSkinPercent( lvPoint pt, lvPoint fullpt );

/// resizable/autoplaceable icon for using in skins
class CRIconSkin
{
protected:
    LVImageSourceRef _image; /// image to draw
    lUInt32 _bgcolor; /// color to fill area if image is not found
    ImageTransform _hTransform;
    ImageTransform _vTransform;
    lvPoint _splitPoint;
    lvPoint _pos;
    lvPoint _size;
    int _align;
public:
    /// image to draw
    LVImageSourceRef getImage() { return _image; }
    /// color to fill area if image is not found
    lUInt32 getBgColor() { return _bgcolor; }
    /// horizontal image transform
    ImageTransform getHTransform() { return _hTransform; }
    /// vertical image transform
    ImageTransform getVTransform() { return _vTransform; }
    /// splitting point of image for split transform
    lvPoint getSplitPoint() { return _splitPoint; }
    /// position of image inside destination rectangle
    lvPoint getPos() { return _pos; }
    /// size of image (percents are relative to destination rectangle)
    lvPoint getSize() { return _size; }
    /// set image to draw
    void setImage( LVImageSourceRef img ) { _image = img; }
    /// color to fill area if image is not found
    void setBgColor(lUInt32 cl) { _bgcolor = cl; }
    /// horizontal image transform
    void setHTransform( ImageTransform t) { _hTransform = t; }
    /// vertical image transform
    void setVTransform( ImageTransform t) { _vTransform = t; }
    /// splitting point of image for split transform
    void setSplitPoint(lvPoint p) { _splitPoint = p ; }
    /// position of image inside destination rectangle
    void setPos(lvPoint p) { _pos = p; }
    /// size of image (percents are relative to destination rectangle)
    void setSize( lvPoint sz) { _size = sz; }
    virtual int getAlign() { return _align; }
    virtual int getVAlign() { return _align & SKIN_VALIGN_MASK; }
    virtual int getHAlign() { return _align & SKIN_HALIGN_MASK; }
    virtual void setAlign( int align ) { _align = align; }
    virtual void setVAlign( int align ) { _align = (_align & ~SKIN_VALIGN_MASK ) | (align & SKIN_VALIGN_MASK); }
    virtual void setHAlign( int align ) { _align = (_align & ~SKIN_HALIGN_MASK ) | (align & SKIN_HALIGN_MASK); }
    virtual void draw( LVDrawBuf & buf, const lvRect & rc );
    CRIconSkin();
    virtual ~CRIconSkin() { }
};
typedef LVRef<CRIconSkin> CRIconSkinRef;

/// list of icons
class CRIconList
{
protected:
    LVRefVec<CRIconSkin> _list;
public:
    void add( CRIconSkinRef icon ) { _list.add( icon ); }
    void add( CRIconList & list ) { _list.add( list._list ); }
    CRIconSkinRef first() { return _list.length()>0 ? _list[0] : CRIconSkinRef(); }
    lUInt32 getBgColor() { CRIconSkinRef r = first(); return r.isNull() ? 0xFFFFFF : r->getBgColor(); }
    int length() { return _list.length(); }
    virtual void draw( LVDrawBuf & buf, const lvRect & rc );
    CRIconList() { }
    virtual ~CRIconList() { }
};
typedef LVRef<CRIconList> CRIconListRef;

/// base skinned item class
class CRSkinnedItem : public LVRefCounter
{
protected:
    lUInt32 _textcolor;
    CRIconListRef _bgicons;
    //lUInt32 _bgcolor;
    //LVImageSourceRef _bgimage;
    //lvPoint _bgimagesplit;
    lString16 _fontFace;
    int _fontSize;
    bool _fontBold;
    bool _fontItalic;
    LVFontRef _font;
    int _textAlign;
public:
    CRSkinnedItem();
    virtual lUInt32 getBackgroundColor() { return _bgicons.isNull() ? 0xFFFFFF : _bgicons->getBgColor(); }
    // set icons to single image, with splitting
    virtual void setBackgroundImage( LVImageSourceRef img )
    {
        CRIconListRef icons( new CRIconList() );
        CRIconSkinRef icon( new CRIconSkin() );
        icon->setImage( img );
        icons->add( icon );
        _bgicons = icons;
    }
    virtual CRIconListRef getBgIcons() { return _bgicons; }
    virtual void setBgIcons( CRIconListRef list ) { _bgicons = list; }
    virtual int getTextAlign() { return _textAlign; }
    virtual int getTextVAlign() { return _textAlign & SKIN_VALIGN_MASK; }
    virtual int getTextHAlign() { return _textAlign & SKIN_HALIGN_MASK; }
    virtual void setTextAlign( int align ) { _textAlign = align; }
    virtual void setTextVAlign( int align ) { _textAlign = (_textAlign & ~SKIN_VALIGN_MASK ) | (align & SKIN_VALIGN_MASK); }
    virtual void setTextHAlign( int align ) { _textAlign = (_textAlign & ~SKIN_HALIGN_MASK ) | (align & SKIN_HALIGN_MASK); }
    virtual void setWordWrap( bool v ) { _textAlign = v ? (_textAlign | SKIN_WORD_WRAP ) : (_textAlign & ~SKIN_WORD_WRAP ); }
    virtual bool getWordWrap() { return (_textAlign & SKIN_WORD_WRAP) ? true : false; }
    virtual lUInt32 getTextColor() { return _textcolor; }
    //virtual lUInt32 getBackgroundColor() { return _bgcolor; }
    //virtual LVImageSourceRef getBackgroundImage() { return _bgimage; }
    //virtual lvPoint getBackgroundImageSplit() { return _bgimagesplit; }
    virtual lString16 getFontFace() { return _fontFace; }
    virtual int getFontSize() { return _fontSize; }
    virtual bool getFontBold() { return _fontBold; }
    virtual bool getFontItalic() { return _fontItalic; }
    virtual void setFontFace( lString16 face );
    virtual void setFontSize( int size );
    virtual void setFontBold( bool bold );
    virtual void setFontItalic( bool italic );
    virtual LVFontRef getFont();
    virtual void setTextColor( lUInt32 color ) { _textcolor = color; }
    //virtual void setBackgroundColor( lUInt32 color ) { _bgcolor = color; }
    //virtual void setBackgroundImage( LVImageSourceRef img ) { _bgimage = img; }
    //virtual void setBackgroundImageSplit( lvPoint pt ) { _bgimagesplit = pt; }
    virtual void setFont( LVFontRef fnt ) { _font = fnt; }
    virtual void draw( LVDrawBuf & buf, const lvRect & rc );
    virtual void drawText( LVDrawBuf & buf, const lvRect & rc, lString16 text, LVFontRef font, lUInt32 textColor, lUInt32 bgColor, int flags );
    virtual void drawText( LVDrawBuf & buf, const lvRect & rc, lString16 text, LVFontRef font )
    {
        drawText(  buf, rc, text, font, getTextColor(), getBackgroundColor(), getTextAlign() );
    }
    virtual void drawText( LVDrawBuf & buf, const lvRect & rc, lString16 text )
    {
        drawText(  buf, rc, text, LVFontRef(), getTextColor(), getBackgroundColor(), getTextAlign() );
    }
    virtual void drawText( LVDrawBuf & buf, const lvRect & rc, lString16 text, lUInt32 color )
    {
        drawText(  buf, rc, text, LVFontRef(), color, getBackgroundColor(), getTextAlign() );
    }
    /// measures text string using current font
    virtual lvPoint measureText( lString16 text );
    virtual ~CRSkinnedItem() { }
};

class CRRectSkin : public CRSkinnedItem
{
protected:
    lvRect _margins;
    lvPoint _minsize;
    lvPoint _maxsize;
    lvPoint _size;
    lvPoint _pos;
    int _align;
public:
    /// same as measureText, but with added margins and minSize applied
    virtual lvPoint measureTextItem( lString16 text );
    CRRectSkin();
    virtual ~CRRectSkin() { }
    /// returns rect based on pos and size
    virtual bool getRect( lvRect & rc, const lvRect & baseRect );
    virtual lvPoint getSize() { return _size; }
    virtual lvPoint getPos() { return _pos; }
    virtual void setSize( lvPoint sz ) { _size = sz; }
    virtual void setPos( lvPoint pos ) { _pos = pos; }
    virtual int getAlign() { return _align; }
    virtual int getVAlign() { return _align & SKIN_VALIGN_MASK; }
    virtual int getHAlign() { return _align & SKIN_HALIGN_MASK; }
    virtual void setAlign( int align ) { _align = align; }
    virtual void setVAlign( int align ) { _align = (_align & ~SKIN_VALIGN_MASK ) | (align & SKIN_VALIGN_MASK); }
    virtual void setHAlign( int align ) { _align = (_align & ~SKIN_HALIGN_MASK ) | (align & SKIN_HALIGN_MASK); }
    virtual lvPoint getMinSize() { return _minsize; }
    virtual lvPoint getMaxSize() { return _maxsize; }
    virtual void setMinSize( lvPoint sz ) { _minsize = sz; }
    virtual void setMaxSize( lvPoint sz ) { _maxsize = sz; }
    virtual void setBorderWidths( const lvRect & rc) { _margins = rc; }
    virtual lvRect getBorderWidths() { return _margins; }
    virtual lvRect getClientRect( const lvRect &windowRect );
    virtual void drawText( LVDrawBuf & buf, const lvRect & rc, lString16 text );
    virtual void drawText( LVDrawBuf & buf, const lvRect & rc, lString16 text, LVFontRef font );
    virtual void drawText( LVDrawBuf & buf, const lvRect & rc, lString16 text, LVFontRef font, lUInt32 textColor, lUInt32 bgColor, int flags )
    {
        CRSkinnedItem::drawText( buf, rc, text, font, textColor, bgColor, flags );
    }
};
typedef LVFastRef<CRRectSkin> CRRectSkinRef;


enum page_skin_type_t {
    PAGE_SKIN_SCROLL,
    PAGE_SKIN_LEFT_PAGE,
    PAGE_SKIN_RIGHT_PAGE,
    PAGE_SKIN_SINGLE_PAGE,
};

class CRPageSkin : public CRSkinnedItem
{
    CRRectSkinRef _scrollSkin;
    CRRectSkinRef _leftPageSkin;
    CRRectSkinRef _rightPageSkin;
    CRRectSkinRef _singlePageSkin;
    lString16     _name;
public:
    const lString16 & getName() { return _name; }
    void setName( const lString16 & newName ) { _name = newName; }
    CRPageSkin();
    CRRectSkinRef getSkin( page_skin_type_t type );
};
typedef LVFastRef<CRPageSkin> CRPageSkinRef;

class CRPageSkinList : public LVArray<CRPageSkinRef>
{
public:
    CRPageSkinRef findByName( const lString16 & name );
};
typedef LVRef<CRPageSkinList> CRPageSkinListRef;

class CRButtonSkin : public CRRectSkin
{
protected:
    LVImageSourceRef _normalimage;
    LVImageSourceRef _disabledimage;
    LVImageSourceRef _pressedimage;
    LVImageSourceRef _selectedimage;
public:
    enum {
        ENABLED = 1,
        PRESSED = 2,
        SELECTED = 4
    };
    LVImageSourceRef getNormalImage() { return _normalimage; }
    LVImageSourceRef getDisabledImage() { return _disabledimage; }
    LVImageSourceRef getPressedImage() { return _pressedimage; }
    LVImageSourceRef getSelectedImage() { return _selectedimage; }
    void setNormalImage( LVImageSourceRef img ) { _normalimage = img; }
    void setDisabledImage( LVImageSourceRef img ) { _disabledimage = img; }
    void setPressedImage( LVImageSourceRef img ) { _pressedimage = img; }
    void setSelectedImage( LVImageSourceRef img ) { _selectedimage = img; }
    virtual void drawButton( LVDrawBuf & buf, const lvRect & rc, int flags = ENABLED );
    LVImageSourceRef getImage(int flags = ENABLED);
    CRButtonSkin();
    virtual ~CRButtonSkin() { 
		CRLog::trace("~CRButtonSkin()");
	}
};
typedef LVFastRef<CRButtonSkin> CRButtonSkinRef;


class CRScrollSkin : public CRRectSkin
{
public:
    enum Location {
        Title,
        Status,
    };

protected:
    CRButtonSkinRef _upButton;
    CRButtonSkinRef _downButton;
    CRButtonSkinRef _leftButton;
    CRButtonSkinRef _rightButton;
    LVImageSourceRef _hBody;
    LVImageSourceRef _hSlider;
    LVImageSourceRef _vBody;
    LVImageSourceRef _vSlider;
    CRRectSkinRef _bottomTabSkin;
    CRRectSkinRef _bottomActiveTabSkin;
    CRRectSkinRef _bottomPageBoundSkin;

    bool _autohide;
    bool _showPageNumbers;
    Location _location;

public:


    Location getLocation() { return _location; }
    void setLocation( Location location ) { _location = location; }
    CRRectSkinRef getBottomTabSkin() { return _bottomTabSkin; }
    CRRectSkinRef getBottomActiveTabSkin() { return _bottomActiveTabSkin; }
    CRRectSkinRef getBottomPageBoundSkin() { return _bottomPageBoundSkin; }
    void setBottomTabSkin( CRRectSkinRef skin ) { _bottomTabSkin = skin; }
    void setBottomActiveTabSkin( CRRectSkinRef skin ) { _bottomActiveTabSkin = skin; }
    void setBottomPageBoundSkin( CRRectSkinRef skin ) { _bottomPageBoundSkin = skin; }
    CRButtonSkinRef getUpButton() { return _upButton; }
    CRButtonSkinRef getDownButton() { return _downButton; }
    CRButtonSkinRef getLeftButton() { return _leftButton; }
    CRButtonSkinRef getRightButton() { return _rightButton; }
    void setUpButton( CRButtonSkinRef btn ) { _upButton = btn; }
    void setDownButton( CRButtonSkinRef btn ) { _downButton = btn; }
    void setLeftButton( CRButtonSkinRef btn ) { _leftButton = btn; }
    void setRightButton( CRButtonSkinRef btn ) { _rightButton = btn; }
    LVImageSourceRef getHBody() { return _hBody; }
    LVImageSourceRef getHSlider() { return _hSlider; }
    LVImageSourceRef getVBody() { return _vBody; }
    LVImageSourceRef getVSlider() { return _vSlider; }
    void setHBody( LVImageSourceRef img ) { _hBody = img; }
    void setHSlider( LVImageSourceRef img ) { _hSlider = img; }
    void setVBody( LVImageSourceRef img ) { _vBody = img; }
    void setVSlider( LVImageSourceRef img ) { _vSlider = img; }
    bool getAutohide() { return _autohide; }
    void setAutohide( bool flgAutoHide ) { _autohide = flgAutoHide; }
    bool getShowPageNumbers() { return _showPageNumbers; }
    void setShowPageNumbers( bool flg ) { _showPageNumbers = flg; }
    virtual void drawScroll( LVDrawBuf & buf, const lvRect & rc, bool vertical, int pos, int maxpos, int pagesize );
    virtual void drawGauge( LVDrawBuf & buf, const lvRect & rc, int percent );
    CRScrollSkin();
    virtual ~CRScrollSkin() { }
};
typedef LVFastRef<CRScrollSkin> CRScrollSkinRef;

class CRButtonList
{
protected:
    LVRefVec<CRButtonSkin> _list;
public:
    void add( LVRef<CRButtonSkin> button ) { _list.add( button ); }
    void add( CRButtonList & list ) { _list.add( list._list ); }
    int length() { return _list.length(); }
    LVRef<CRButtonSkin> get(int index) { return (index >= 0 && index < _list.length()) ? _list[index] : LVRef<CRButtonSkin>(); }
    CRButtonList() { }
    virtual ~CRButtonList() {
		CRLog::trace("~CRButtonList();");
	}
};
typedef LVRef<CRButtonList> CRButtonListRef;

class CRToolBarSkin : public CRRectSkin
{
protected:
    CRButtonListRef _buttons;
public:
    CRToolBarSkin() { }
    virtual ~CRToolBarSkin() { 
		CRLog::trace("~CRToolBarSkin();");
	}    

    CRButtonListRef getButtons() { return _buttons; }
    void setButtons(CRButtonListRef list) { _buttons = list; }
    virtual void drawToolBar( LVDrawBuf & buf, const lvRect & rc, bool enabled, int selectedButton );
    virtual void drawButton(LVDrawBuf & buf, const lvRect & rc, int index, int flags);
};
typedef LVFastRef<CRToolBarSkin> CRToolBarSkinRef;

class CRWindowSkin : public CRRectSkin
{
protected:
    //lvPoint _titleSize;
    CRRectSkinRef _titleSkin;
    CRRectSkinRef _clientSkin;
    CRRectSkinRef _statusSkin;
    CRRectSkinRef _inputSkin;
    CRScrollSkinRef _scrollSkin;
    bool _fullscreen;
public:
    bool getFullScreen() { return _fullscreen; }
    void setFullScreen( bool fs ) { _fullscreen = fs; }
    CRWindowSkin();
    virtual ~CRWindowSkin() { }
    /// returns necessary window size for specified client size
    CRScrollSkinRef getScrollSkin() { return _scrollSkin; }
    void setScrollSkin( CRScrollSkinRef v ) { _scrollSkin = v; }
    virtual lvPoint getWindowSize( const lvPoint & clientSize );
    virtual lvPoint getTitleSize();
    //virtual void setTitleSize( lvPoint sz ) { _titleSize = sz; }
    virtual lvRect getTitleRect( const lvRect &windowRect );
    virtual lvRect getClientRect( const lvRect &windowRect );
    virtual CRRectSkinRef getTitleSkin() { return _titleSkin; }
    virtual void setTitleSkin( CRRectSkinRef skin ) { _titleSkin = skin; }
    virtual CRRectSkinRef getClientSkin() { return _clientSkin; }
    virtual void setClientSkin( CRRectSkinRef skin ) { _clientSkin = skin; }
    virtual CRRectSkinRef getStatusSkin() { return _statusSkin; }
    virtual void setStatusSkin( CRRectSkinRef skin ) { _statusSkin = skin; }
    virtual CRRectSkinRef getInputSkin() { return _inputSkin; }
    virtual void setInputSkin( CRRectSkinRef skin ) { _inputSkin = skin; }
};
typedef LVFastRef<CRWindowSkin> CRWindowSkinRef;

class CRMenuSkin : public CRWindowSkin
{
protected:
    CRRectSkinRef _separatorSkin;
    CRRectSkinRef _valueSkin;
    CRRectSkinRef _itemSkin;
    CRRectSkinRef _itemShortcutSkin;
    CRRectSkinRef _evenItemSkin;
    CRRectSkinRef _evenItemShortcutSkin;
    CRRectSkinRef _selItemSkin;
    CRRectSkinRef _selItemShortcutSkin;
    CRRectSkinRef _evenSelItemSkin;
    CRRectSkinRef _evenSelItemShortcutSkin;
    int _minItemCount;
    int _maxItemCount;
    bool _showShortcuts;
public:
    CRMenuSkin();
    virtual ~CRMenuSkin() { }
    virtual CRRectSkinRef getValueSkin() { return _valueSkin; }
    virtual void setValueSkin( CRRectSkinRef skin ) { _valueSkin = skin; }
    virtual CRRectSkinRef getItemSkin() { return _itemSkin; }
    virtual void setItemSkin( CRRectSkinRef skin ) { _itemSkin = skin; }
    virtual CRRectSkinRef getSeparatorSkin() { return _separatorSkin; }
    virtual void setSeparatorSkin( CRRectSkinRef skin ) { _separatorSkin = skin; }
    virtual CRRectSkinRef getEvenItemSkin() { return _evenItemSkin; }
    virtual void setEvenItemSkin( CRRectSkinRef skin ) { _evenItemSkin = skin; }
    virtual CRRectSkinRef getItemShortcutSkin() { return _itemShortcutSkin; }
    virtual void setItemShortcutSkin( CRRectSkinRef skin ) { _itemShortcutSkin = skin; }
    virtual CRRectSkinRef getEvenItemShortcutSkin() { return _evenItemShortcutSkin; }
    virtual void setEvenItemShortcutSkin( CRRectSkinRef skin ) { _evenItemShortcutSkin = skin; }
    virtual CRRectSkinRef getSelItemSkin() { return _selItemSkin; }
    virtual void setSelItemSkin( CRRectSkinRef skin ) { _selItemSkin = skin; }
    virtual CRRectSkinRef getEvenSelItemSkin() { return _evenSelItemSkin; }
    virtual void setEvenSelItemSkin( CRRectSkinRef skin ) { _evenSelItemSkin = skin; }
    virtual CRRectSkinRef getSelItemShortcutSkin() { return _selItemShortcutSkin; }
    virtual void setSelItemShortcutSkin( CRRectSkinRef skin ) { _selItemShortcutSkin = skin; }
    virtual CRRectSkinRef getEvenSelItemShortcutSkin() { return _evenSelItemShortcutSkin; }
    virtual void setEvenSelItemShortcutSkin( CRRectSkinRef skin ) { _evenSelItemShortcutSkin = skin; }
    int getMinItemCount() { return _minItemCount; }
    int getMaxItemCount() { return _maxItemCount; }
    void setMinItemCount( int v ) { _minItemCount = v; }
    void setMaxItemCount( int v ) { _maxItemCount = v; }
    bool getShowShortcuts() { return _showShortcuts; }
    void setShowShortcuts( bool flgShowShortcuts ) { _showShortcuts = flgShowShortcuts; }
};
typedef LVFastRef<CRMenuSkin> CRMenuSkinRef;



/// Base skin class
class CRSkinContainer : public LVRefCounter
{
protected:
    virtual bool readRectSkin(  const lChar16 * path, CRRectSkin * res );
    virtual bool readIconSkin(  const lChar16 * path, CRIconSkin * res );
    virtual bool readButtonSkin(  const lChar16 * path, CRButtonSkin * res );
    virtual bool readScrollSkin(  const lChar16 * path, CRScrollSkin * res );
    virtual bool readWindowSkin(  const lChar16 * path, CRWindowSkin * res );
    virtual bool readPageSkin(  const lChar16 * path, CRPageSkin * res );
    virtual bool readMenuSkin(  const lChar16 * path, CRMenuSkin * res );
    virtual bool readToolBarSkin( const lChar16 * path, CRToolBarSkin *res );
public:
    /// retuns path to base definition, if attribute base="#nodeid" is specified for element of path
    virtual lString16 getBasePath( const lChar16 * path );
    /// find path by id
    virtual lString16 pathById( const lChar16 * id ) = 0;
    /// gets image from container
    virtual LVImageSourceRef getImage( const lChar16 * filename ) = 0;
    /// gets image from container
    virtual LVImageSourceRef getImage( const lString16 & filename ) { return getImage( filename.c_str() ); }
    /// gets doc pointer by path
    virtual ldomXPointer getXPointer( const lString16 & xPointerStr ) = 0;
    /// gets doc pointer by path
    virtual ldomXPointer getXPointer( const lChar16 * xPointerStr ) { return getXPointer( lString16(xPointerStr) ); }
    /// reads int value from attrname attribute of element specified by path, returns defValue if not found
    virtual int readInt( const lChar16 * path, const lChar16 * attrname, int defValue, bool * res=NULL );
    /// reads boolean value from attrname attribute of element specified by path, returns defValue if not found
    virtual bool readBool( const lChar16 * path, const lChar16 * attrname, bool defValue, bool * res=NULL );
    /// reads image transform value from attrname attribute of element specified by path, returns defValue if not found
    ImageTransform readTransform( const lChar16 * path, const lChar16 * attrname, ImageTransform defValue, bool * res );
    /// reads h align value from attrname attribute of element specified by path, returns defValue if not found
    virtual int readHAlign( const lChar16 * path, const lChar16 * attrname, int defValue, bool * res=NULL );
    /// reads h align value from attrname attribute of element specified by path, returns defValue if not found
    virtual int readVAlign( const lChar16 * path, const lChar16 * attrname, int defValue, bool * res=NULL );
    /// reads string value from attrname attribute of element specified by path, returns empty string if not found
    virtual lString16 readString( const lChar16 * path, const lChar16 * attrname, bool * res=NULL );
    /// reads string value from attrname attribute of element specified by path, returns defValue if not found
    virtual lString16 readString( const lChar16 * path, const lChar16 * attrname, const lString16 & defValue, bool * res=NULL );
    /// reads color value from attrname attribute of element specified by path, returns defValue if not found
    virtual lUInt32 readColor( const lChar16 * path, const lChar16 * attrname, lUInt32 defValue, bool * res=NULL );
    /// reads rect value from attrname attribute of element specified by path, returns defValue if not found
    virtual lvRect readRect( const lChar16 * path, const lChar16 * attrname, lvRect defValue, bool * res=NULL );
    /// reads point(size) value from attrname attribute of element specified by path, returns defValue if not found
    virtual lvPoint readSize( const lChar16 * path, const lChar16 * attrname, lvPoint defValue, bool * res=NULL );
    /// reads rect value from attrname attribute of element specified by path, returns null ref if not found
    virtual LVImageSourceRef readImage( const lChar16 * path, const lChar16 * attrname, bool * res=NULL );
    /// reads list of icons
    virtual CRIconListRef readIcons( const lChar16 * path, bool * res=NULL );
	/// reads list of buttons
    virtual CRButtonListRef readButtons( const lChar16 * path, bool * res=NULL );
    /// returns rect skin by path or #id
    virtual CRRectSkinRef getRectSkin( const lChar16 * path ) = 0;
    /// returns scroll skin by path or #id
    virtual CRScrollSkinRef getScrollSkin( const lChar16 * path ) = 0;
    /// returns window skin by path or #id
    virtual CRWindowSkinRef getWindowSkin( const lChar16 * path ) = 0;
    /// returns menu skin by path or #id
    virtual CRMenuSkinRef getMenuSkin( const lChar16 * path ) = 0;
    /// returns book page skin by path or #id
    virtual CRPageSkinRef getPageSkin( const lChar16 * path ) = 0;
    /// returns book page skin list
    virtual CRPageSkinListRef getPageSkinList() = 0;
    /// returns toolbar skin by path or #id
    virtual CRToolBarSkinRef getToolBarSkin( const lChar16 * path ) = 0;

    /// garbage collection
    virtual void gc() { }

    /// destructor
    virtual ~CRSkinContainer() { }
};

/// skin reference
typedef LVFastRef<CRSkinContainer> CRSkinRef;

class CRSkinListItem
{
    lString16 _name;
    lString16 _baseDir;
    lString16 _fileName;
    lString16Collection _pageSkinList;
    CRSkinListItem() { }
public:
    lString16 getName() { return _name; }
    lString16 getFileName() { return _fileName; }
    lString16 getDirName() { return _baseDir; }
    lString16Collection & getPageSkinList() { return _pageSkinList; }
    static CRSkinListItem * init( lString16 baseDir, lString16 fileName );
    CRSkinRef getSkin();
    virtual ~CRSkinListItem() { }
};
class CRSkinList : public LVPtrVector<CRSkinListItem>
{
public:
    CRSkinListItem * findByName(const lString16 & name);
};


/// opens skin from directory or .zip file
CRSkinRef LVOpenSkin( const lString16 & pathname );
/// open simple skin, without image files, from string
CRSkinRef LVOpenSimpleSkin( const lString8 & xml );



#endif// CR_SKIN_INCLUDED
