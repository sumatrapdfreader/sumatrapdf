/** \file lvdocview.h
    \brief XML/CSS document view

    CoolReader Engine

    (c) Vadim Lopatin, 2000-2009

    This source code is distributed under the terms of
    GNU General Public License.
    See LICENSE file for details.
*/

#ifndef __LV_TEXT_VIEW_H_INCLUDED__
#define __LV_TEXT_VIEW_H_INCLUDED__

#include "crsetup.h"
#include "crskin.h"
#include "lvtinydom.h"
#include "lvpagesplitter.h"
#include "lvdrawbuf.h"
#include "hist.h"
#include "lvthread.h"

// standard properties supported by LVDocView
#define PROP_FONT_GAMMA              "font.gamma" // currently supported: 0.65 .. 1.35, see gammatbl.h
#define PROP_FONT_ANTIALIASING       "font.antialiasing.mode"
#define PROP_FONT_COLOR              "font.color.default"
#define PROP_FONT_FACE               "font.face.default"
#define PROP_FONT_WEIGHT_EMBOLDEN    "font.face.weight.embolden"
#define PROP_BACKGROUND_COLOR        "background.color.default"
#define PROP_BACKGROUND_IMAGE        "background.image.filename"
#define PROP_TXT_OPTION_PREFORMATTED "crengine.file.txt.preformatted"
#define PROP_LOG_FILENAME            "crengine.log.filename"
#define PROP_LOG_LEVEL               "crengine.log.level"
#define PROP_LOG_AUTOFLUSH           "crengine.log.autoflush"
#define PROP_FONT_SIZE               "crengine.font.size"
#define PROP_FALLBACK_FONT_FACE      "crengine.font.fallback.face"
#define PROP_STATUS_FONT_COLOR       "crengine.page.header.font.color"
#define PROP_STATUS_FONT_FACE        "crengine.page.header.font.face"
#define PROP_STATUS_FONT_SIZE        "crengine.page.header.font.size"
#define PROP_PAGE_MARGIN_TOP         "crengine.page.margin.top"
#define PROP_PAGE_MARGIN_BOTTOM      "crengine.page.margin.bottom"
#define PROP_PAGE_MARGIN_LEFT        "crengine.page.margin.left"
#define PROP_PAGE_MARGIN_RIGHT       "crengine.page.margin.right"
#define PROP_PAGE_VIEW_MODE          "crengine.page.view.mode" // pages/scroll
#define PROP_INTERLINE_SPACE         "crengine.interline.space"
#if CR_INTERNAL_PAGE_ORIENTATION==1
#define PROP_ROTATE_ANGLE            "window.rotate.angle"
#endif
#define PROP_EMBEDDED_STYLES         "crengine.doc.embedded.styles.enabled"
#define PROP_DISPLAY_INVERSE         "crengine.display.inverse"
#define PROP_DISPLAY_FULL_UPDATE_INTERVAL "crengine.display.full.update.interval"
#define PROP_DISPLAY_TURBO_UPDATE_MODE "crengine.display.turbo.update"
#define PROP_STATUS_LINE             "window.status.line"
#define PROP_BOOKMARK_ICONS          "crengine.bookmarks.icons"
#define PROP_FOOTNOTES               "crengine.footnotes"
#define PROP_SHOW_TIME               "window.status.clock"
#define PROP_SHOW_TITLE              "window.status.title"
#define PROP_STATUS_CHAPTER_MARKS    "crengine.page.header.chapter.marks"
#define PROP_SHOW_BATTERY            "window.status.battery"
#define PROP_SHOW_POS_PERCENT        "window.status.pos.percent"
#define PROP_SHOW_PAGE_COUNT         "window.status.pos.page.count"
#define PROP_SHOW_PAGE_NUMBER        "window.status.pos.page.number"
#define PROP_SHOW_BATTERY_PERCENT    "window.status.battery.percent"
#define PROP_FONT_KERNING_ENABLED    "font.kerning.enabled"
#define PROP_LANDSCAPE_PAGES         "window.landscape.pages"
#define PROP_HYPHENATION_DICT        "crengine.hyphenation.directory"
#define PROP_AUTOSAVE_BOOKMARKS      "crengine.autosave.bookmarks"

#define PROP_FLOATING_PUNCTUATION    "crengine.style.floating.punctuation.enabled"
#define PROP_FORMAT_MIN_SPACE_CONDENSING_PERCENT "crengine.style.space.condensing.percent"

#define PROP_FILE_PROPS_FONT_SIZE    "cr3.file.props.font.size"


#define PROP_MIN_FILE_SIZE_TO_CACHE  "crengine.cache.filesize.min"
#define PROP_FORCED_MIN_FILE_SIZE_TO_CACHE  "crengine.cache.forced.filesize.min"
#define PROP_PROGRESS_SHOW_FIRST_PAGE  "crengine.progress.show.first.page"
#define PROP_HIGHLIGHT_COMMENT_BOOKMARKS "crengine.highlight.bookmarks"
// image scaling settings
// mode: 0=disabled, 1=integer scaling factors, 2=free scaling
// scale: 0=auto based on font size, 1=no zoom, 2=scale up to *2, 3=scale up to *3
#define PROP_IMG_SCALING_ZOOMIN_INLINE_MODE  "crengine.image.scaling.zoomin.inline.mode"
#define PROP_IMG_SCALING_ZOOMIN_INLINE_SCALE  "crengine.image.scaling.zoomin.inline.scale"
#define PROP_IMG_SCALING_ZOOMOUT_INLINE_MODE "crengine.image.scaling.zoomout.inline.mode"
#define PROP_IMG_SCALING_ZOOMOUT_INLINE_SCALE "crengine.image.scaling.zoomout.inline.scale"
#define PROP_IMG_SCALING_ZOOMIN_BLOCK_MODE  "crengine.image.scaling.zoomin.block.mode"
#define PROP_IMG_SCALING_ZOOMIN_BLOCK_SCALE  "crengine.image.scaling.zoomin.block.scale"
#define PROP_IMG_SCALING_ZOOMOUT_BLOCK_MODE "crengine.image.scaling.zoomout.block.mode"
#define PROP_IMG_SCALING_ZOOMOUT_BLOCK_SCALE "crengine.image.scaling.zoomout.block.scale"

const lChar16 * getDocFormatName( doc_format_t fmt );

/// text format import options
typedef enum {
    txt_format_pre,  // no formatting, leave lines as is
    txt_format_auto  // autodetect format
} txt_format_t;

/// no battery
#define CR_BATTERY_STATE_NO_BATTERY -2
/// battery is charging
#define CR_BATTERY_STATE_CHARGING -1
// values 0..100 -- battery life percent

#ifndef CR_ENABLE_PAGE_IMAGE_CACHE
#ifdef ANDROID
#define CR_ENABLE_PAGE_IMAGE_CACHE 0
#else
#define CR_ENABLE_PAGE_IMAGE_CACHE 1
#endif
#endif//#ifndef CR_ENABLE_PAGE_IMAGE_CACHE

#if CR_ENABLE_PAGE_IMAGE_CACHE==1
/// Page imege holder which allows to unlock mutex after destruction
class LVDocImageHolder
{
private:
    LVRef<LVDrawBuf> _drawbuf;
    LVMutex & _mutex;
public:
    LVDrawBuf * getDrawBuf() { return _drawbuf.get(); }
    LVRef<LVDrawBuf> getDrawBufRef() { return _drawbuf; }
    LVDocImageHolder( LVRef<LVDrawBuf> drawbuf, LVMutex & mutex )
    : _drawbuf(drawbuf), _mutex(mutex)
    {
    }
    ~LVDocImageHolder()
    {
        _drawbuf = NULL;
        _mutex.unlock();
    }
};

typedef LVRef<LVDocImageHolder> LVDocImageRef;

/// page image cache
class LVDocViewImageCache
{
    private:
        LVMutex _mutex;
        class Item {
            public:
                LVRef<LVDrawBuf> _drawbuf;
                LVRef<LVThread> _thread;
                int _offset;
                int _page;
                bool _ready;
                bool _valid;
        };
        Item _items[2];
        int _last;
    public:
        /// return mutex
        LVMutex & getMutex() { return _mutex; }
        /// set page to cache
        void set( int offset, int page, LVRef<LVDrawBuf> drawbuf, LVRef<LVThread> thread )
        {
            LVLock lock( _mutex );
            _last = (_last + 1) & 1;
            _items[_last]._ready = false;
            _items[_last]._thread = thread;
            _items[_last]._drawbuf = drawbuf;
            _items[_last]._offset = offset;
            _items[_last]._page = page;
            _items[_last]._valid = true;
        }
        /// return page image, wait until ready
        LVRef<LVDrawBuf> getWithoutLock( int offset, int page )
        {
            for ( int i=0; i<2; i++ ) {
                if ( _items[i]._valid &&
                     ( (_items[i]._offset == offset && offset!=-1)
                      || (_items[i]._page==page && page!=-1)) ) {
                    if ( !_items[i]._ready ) {
                        _items[i]._thread->join();
                        _items[i]._thread = NULL;
                        _items[i]._ready = true;
                    }
                    _last = i;
                    return _items[i]._drawbuf;
                }
            }
            return LVRef<LVDrawBuf>();
        }
        /// return page image, wait until ready
        LVDocImageRef get( int offset, int page )
        {
            _mutex.lock();
            LVRef<LVDrawBuf> buf = getWithoutLock( offset, page );
            if ( !buf.isNull() )
                return LVDocImageRef( new LVDocImageHolder(getWithoutLock( offset, page ), _mutex) );
            return LVDocImageRef( NULL );
        }
        bool has( int offset, int page )
        {
            _mutex.lock();
            for ( int i=0; i<2; i++ ) {
                if ( _items[i]._valid && ( (_items[i]._offset == offset && offset!=-1)
                      || (_items[i]._page==page && page!=-1)) ) {
                    return true;
                }
            }
            return false;
        }
        void clear()
        {
            LVLock lock( _mutex );
            for ( int i=0; i<2; i++ ) {
                if ( _items[i]._valid && !_items[i]._ready ) {
                    _items[i]._thread->join();
                }
                _items[i]._thread.Clear();
                _items[i]._valid = false;
                _items[i]._drawbuf.Clear();
                _items[i]._offset = -1;
                _items[i]._page = -1;
            }
        }
        LVDocViewImageCache()
        : _last(0)
        {
            for ( int i=0; i<2; i++ )
                _items[i]._valid = false;
        }
        ~LVDocViewImageCache()
        {
            clear();
        }
};
#endif

class LVPageWordSelector {
    LVDocView * _docview;
    ldomWordExList _words;
    void updateSelection();
public:
    // selects middle word of current page
    LVPageWordSelector( LVDocView * docview );
    // clears selection
    ~LVPageWordSelector();
    // move current word selection in specified direction, (distance) times
    void moveBy( MoveDirection dir, int distance = 1 );
    // returns currently selected word
    ldomWordEx * getSelectedWord() { return _words.getSelWord(); }
    // access to words
    ldomWordExList & getWords() { return _words; }
    // append chars to search pattern
    ldomWordEx * appendPattern( lString16 chars );
    // remove last item from pattern
    ldomWordEx * reducePattern();
    // selects word of current page with specified coords;
    void selectWord(int x, int y);
};


#define LVDOCVIEW_COMMANDS_START 100
/// LVDocView commands
enum LVDocCmd
{
    DCMD_BEGIN = LVDOCVIEW_COMMANDS_START,
    DCMD_LINEUP,
    DCMD_PAGEUP,
    DCMD_PAGEDOWN,
    DCMD_LINEDOWN,
    DCMD_LINK_FORWARD,
    DCMD_LINK_BACK,
    DCMD_LINK_NEXT,
    DCMD_LINK_PREV,
    DCMD_LINK_GO,
    DCMD_END,
    DCMD_GO_POS,
    DCMD_GO_PAGE,
    DCMD_ZOOM_IN,
    DCMD_ZOOM_OUT,
    DCMD_TOGGLE_TEXT_FORMAT,
    DCMD_BOOKMARK_SAVE_N, // save current page bookmark under spicified number
    DCMD_BOOKMARK_GO_N,  // go to bookmark with specified number
    DCMD_MOVE_BY_CHAPTER, // param=-1 - previous chapter, 1 = next chapter
    DCMD_GO_SCROLL_POS,  // param=position of scroll bar slider
    DCMD_TOGGLE_PAGE_SCROLL_VIEW,  // toggle paged/scroll view mode
    DCMD_LINK_FIRST, // select first link on page
    DCMD_ROTATE_BY, // rotate view, param =  +1 - clockwise, -1 - counter-clockwise
    DCMD_ROTATE_SET, // rotate viewm param = 0..3 (0=normal, 1=90`, ...)
    DCMD_SAVE_HISTORY, // save history and bookmarks
    DCMD_SAVE_TO_CACHE, // save document to cache for fast opening
    DCMD_TOGGLE_BOLD, // togle font bolder attribute
    DCMD_SCROLL_BY, // scroll by N pixels, for Scroll view mode only
    DCMD_REQUEST_RENDER, // invalidate rendered data
    DCMD_GO_PAGE_DONT_SAVE_HISTORY,
    DCMD_SET_INTERNAL_STYLES, // set internal styles option

    // selection by sentences
    DCMD_SELECT_FIRST_SENTENCE, // select first sentence on page
    DCMD_SELECT_NEXT_SENTENCE, // nove selection to next sentence
    DCMD_SELECT_PREV_SENTENCE, // nove selection to next sentence
    DCMD_SELECT_MOVE_LEFT_BOUND_BY_WORDS, // move selection start by words
    DCMD_SELECT_MOVE_RIGHT_BOUND_BY_WORDS, // move selection end by words

    //=======================================
    DCMD_EDIT_CURSOR_LEFT,
    DCMD_EDIT_CURSOR_RIGHT,
    DCMD_EDIT_CURSOR_UP,
    DCMD_EDIT_CURSOR_DOWN,
    DCMD_EDIT_PAGE_UP,
    DCMD_EDIT_PAGE_DOWN,
    DCMD_EDIT_HOME,
    DCMD_EDIT_END,
    DCMD_EDIT_INSERT_CHAR,
    DCMD_EDIT_REPLACE_CHAR,
};
#define LVDOCVIEW_COMMANDS_END DCMD_TOGGLE_BOLD

/// document view mode: pages/scroll
enum LVDocViewMode
{
    DVM_SCROLL,
    DVM_PAGES,
};

/// document scroll position info
class LVScrollInfo
{
public:
    int pos;
    int maxpos;
    int pagesize;
    int scale;
    lString16 posText;
    LVScrollInfo()
    : pos(0), maxpos(0), pagesize(0), scale(1)
    {
    }
};

/// page header flags
enum {
    PGHDR_NONE=0,
    PGHDR_PAGE_NUMBER=1,
    PGHDR_PAGE_COUNT=2,
    PGHDR_AUTHOR=4,
    PGHDR_TITLE=8,
    PGHDR_CLOCK=16,
    PGHDR_BATTERY=32,
    PGHDR_CHAPTER_MARKS=64,
    PGHDR_PERCENT=128,
};


//typedef lUInt64 LVPosBookmark;

typedef LVArray<int> LVBookMarkPercentInfo;

#define DEF_COLOR_BUFFER_BPP 32

/**
    \brief XML document view

    Platform independant document view implementation.

    Supports scroll view of document.
*/
class LVDocView : public CacheLoadingCallback
{
    friend class LVDrawThread;
private:
    int m_bitsPerPixel;
    int m_dx;
    int m_dy;

    // current position
    int _pos;  // >=0 is correct vertical offset inside document, <0 - get based on m_page
    int _page; // >=0 is correct page number, <0 - get based on _pos
    bool _posIsSet;
    ldomXPointer _posBookmark; // bookmark for current position

    int m_battery_state;
    int m_font_size;
    int m_status_font_size;
    int m_def_interline_space;
    LVArray<int> m_font_sizes;
    bool m_font_sizes_cyclic;
    bool m_is_rendered;

    LVDocViewMode m_view_mode; // DVM_SCROLL, DVM_PAGES
    inline bool isPageMode() { return m_view_mode==DVM_PAGES; }
    inline bool isScrollMode() { return m_view_mode==DVM_SCROLL; }

    /*
#if (COLOR_BACKBUFFER==1)
    LVColorDrawBuf m_drawbuf;
#else
    LVGrayDrawBuf  m_drawbuf;
#endif
    */
    lUInt32 m_backgroundColor;
    lUInt32 m_textColor;
    lUInt32 m_statusColor;
    font_ref_t     m_font;
    font_ref_t     m_infoFont;
    LVFontRef      m_batteryFont;
    LVContainerRef m_container;
    LVStreamRef    m_stream;
    LVContainerRef m_arc;
    ldomDocument * m_doc;
    lString8 m_stylesheet;
    LVRendPageList m_pages;
    LVScrollInfo m_scrollinfo;
    LVImageSourceRef m_defaultCover;
    LVImageSourceRef m_backgroundImage;
    LVRef<LVColorDrawBuf> m_backgroundImageScaled;
    bool m_backgroundTiled;
    bool m_highlightBookmarks;
    LVPtrVector<LVBookMarkPercentInfo> m_bookmarksPercents;

protected:
    lString16 m_last_clock;

    ldomMarkedRangeList m_markRanges;
    ldomMarkedRangeList m_bmkRanges;

private:
    lString16 m_filename;
#define ORIGINAL_FILENAME_PATCH
#ifdef ORIGINAL_FILENAME_PATCH
    lString16 m_originalFilename;
#endif
    lvsize_t  m_filesize;


    lvRect m_pageMargins;
    lvRect m_pageRects[2];
    int    m_pagesVisible;
    int m_pageHeaderInfo;
    bool m_showCover;
    LVRefVec<LVImageSource> m_headerIcons;
    LVRefVec<LVImageSource> m_batteryIcons;

#if CR_INTERNAL_PAGE_ORIENTATION==1
    cr_rotate_angle_t m_rotateAngle;
#endif

    CRFileHist m_hist;

    LVArray<int> m_section_bounds;
    bool m_section_bounds_valid;

    LVMutex _mutex;
#if CR_ENABLE_PAGE_IMAGE_CACHE==1
    LVDocViewImageCache m_imageCache;
#endif


    lString8 m_defaultFontFace;
	lString8 m_statusFontFace;
    ldomNavigationHistory _navigationHistory;

    doc_format_t m_doc_format;

    LVDocViewCallback * m_callback;

    // options
    CRPropRef m_props;
    // document properties
    CRPropRef m_doc_props;

    bool m_swapDone;

    /// edit cursor position
    ldomXPointer m_cursorPos;

    lString16 m_pageHeaderOverride;

    int m_drawBufferBits;

    CRPageSkinRef _pageSkin;

    /// sets current document format
    void setDocFormat( doc_format_t fmt );


    // private functions
    void updateScroll();
    /// makes table of contents for current document
    void makeToc();
    /// updates page layout
    void updateLayout();
    /// parse document from m_stream
    bool ParseDocument( );
    /// format of document from cache is known
    virtual void OnCacheFileFormatDetected( doc_format_t fmt );
    void insertBookmarkPercentInfo(int start_page, int end_y, int percent);

    void updateDocStyleSheet();

protected:
    /// draw to specified buffer by either Y pos or page number (unused param should be -1)
    void Draw( LVDrawBuf & drawbuf, int pageTopPosition, int pageNumber, bool rotate );


    virtual void drawNavigationBar( LVDrawBuf * drawbuf, int pageIndex, int percent );

    virtual void getNavigationBarRectangle( lvRect & rc );

    virtual void getNavigationBarRectangle( int pageIndex, lvRect & rc );

    virtual void getPageRectangle( int pageIndex, lvRect & pageRect );
    /// returns document offset for next page
    int getNextPageOffset();
    /// returns document offset for previous page
    int getPrevPageOffset();
    /// ensure current position is set to current bookmark value
    void checkPos();
    /// selects link on page, if any (delta==0 - current, 1-next, -1-previous). returns selected link range, null if no links.
    virtual ldomXRange * selectPageLink( int delta, bool wrapAround);
    /// set status bar and clock mode
    void setStatusMode( int newMode, bool showClock, bool showTitle, bool showBattery, bool showChapterMarks, bool showPercent, bool showPageNumber, bool showPageCount );
    /// create document and set flags
    void createEmptyDocument();
    /// get document rectangle for specified cursor position, returns false if not visible
    bool getCursorDocRect( ldomXPointer ptr, lvRect & rc );
    /// get screen rectangle for specified cursor position, returns false if not visible
    bool getCursorRect( ldomXPointer ptr, lvRect & rc, bool scrollToCursor = false );
public:
    LVFontRef getBatteryFont() { return m_batteryFont; }
    void setBatteryFont( LVFontRef font ) { m_batteryFont=font; }

    /// draw current page to specified buffer
    void Draw( LVDrawBuf & drawbuf );
    
    /// close document
    void close();
    /// set buffer format
    void setDrawBufferBits( int bits ) { m_drawBufferBits = bits; }
    /// substitute page header with custom text (e.g. to be used while loading)
    void setPageHeaderOverride( lString16 s );
    /// get screen rectangle for current cursor position, returns false if not visible
    bool getCursorRect( lvRect & rc, bool scrollToCursor = false )
    {
        return getCursorRect( m_cursorPos, rc, scrollToCursor );
    }
    /// returns cursor position
    ldomXPointer getCursorPos() { return m_cursorPos; }
    /// set cursor position
    void setCursorPos( ldomXPointer ptr ) { m_cursorPos = ptr; }
    /// try swappping of document to cache, if size is big enough, and no swapping attempt yet done
    void swapToCache();
    /// save document to cache file, with timeout option
    ContinuousOperationResult swapToCache(CRTimerUtil & maxTime);
    /// save unsaved data to cache file (if one is created), with timeout option
    ContinuousOperationResult updateCache(CRTimerUtil & maxTime);
    /// save unsaved data to cache file (if one is created), w/o timeout
    ContinuousOperationResult updateCache();

    /// returns selected (marked) ranges
    ldomMarkedRangeList * getMarkedRanges() { return &m_markRanges; }

    /// returns XPointer to middle paragraph of current page
    ldomXPointer getCurrentPageMiddleParagraph();
    /// render document, if not rendered
    void checkRender();
    /// saves current position to navigation history, to be able return back
    bool savePosToNavigationHistory();
    /// navigate to history path URL
    bool navigateTo( lString16 historyPath );
    /// packs current file path and name
    lString16 getNavigationPath();
    /// returns pointer to bookmark/last position containter of currently opened file
    CRFileHistRecord * getCurrentFileHistRecord();
	/// -1 moveto previous chapter, 0 to current chaoter first pae, 1 to next chapter
	bool moveByChapter( int delta );
	/// -1 moveto previous page, 1 to next page
	bool moveByPage( int delta );
	/// saves new bookmark
    CRBookmark * saveRangeBookmark( ldomXRange & range, bmk_type type, lString16 comment );
	/// export bookmarks to text file
	bool exportBookmarks( lString16 filename );
	/// saves current page bookmark under numbered shortcut
    CRBookmark * saveCurrentPageShortcutBookmark( int number );
    /// saves current page bookmark under numbered shortcut
    CRBookmark * saveCurrentPageBookmark( lString16 comment );
    /// removes bookmark from list, and deletes it, false if not found
    bool removeBookmark( CRBookmark * bm );
    /// sets new list of bookmarks, removes old values
    void setBookmarkList(LVPtrVector<CRBookmark> & bookmarks);
    /// restores page using bookmark by numbered shortcut
	bool goToPageShortcutBookmark( int number );
    /// find bookmark by window point, return NULL if point doesn't belong to any bookmark
    CRBookmark * findBookmarkByPoint(lvPoint pt);
    /// returns true if coverpage display is on
    bool getShowCover() { return  m_showCover; }
    /// sets coverpage display flag
    void setShowCover( bool show ) { m_showCover = show; }
    /// returns true if page image is available (0=current, -1=prev, 1=next)
    bool isPageImageReady( int delta );

    // property support methods
    /// sets default property values if properties not found, checks ranges
    void propsUpdateDefaults( CRPropRef props );
    /// applies properties, returns list of not recognized properties
    CRPropRef propsApply( CRPropRef props );
    /// returns current values of supported properties
    CRPropRef propsGetCurrent();

    /// get current default cover image
    LVImageSourceRef getDefaultCover() const { return m_defaultCover; }
    /// set default cover image (for books w/o cover)
    void setDefaultCover(LVImageSourceRef cover) { m_defaultCover = cover; clearImageCache(); }

    /// get background image
    LVImageSourceRef getBackgroundImage() const { return m_backgroundImage; }
    /// set background image
    void setBackgroundImage(LVImageSourceRef bgImage, bool tiled=true) { m_backgroundImage = bgImage; m_backgroundTiled=tiled; m_backgroundImageScaled.Clear(); clearImageCache(); }
    /// clears page background
    void drawPageBackground( LVDrawBuf & drawbuf, int offsetX, int offsetY );

    // callback functions
    /// set callback
    LVDocViewCallback * setCallback( LVDocViewCallback * callback ) { LVDocViewCallback * old = m_callback; m_callback = callback; return old; }
    /// get callback
    LVDocViewCallback * getCallback( ) { return m_callback; }

    // doc format functions
    /// set text format options
    void setTextFormatOptions( txt_format_t fmt );
    /// get text format options
    txt_format_t getTextFormatOptions();
    /// get current document format
    doc_format_t getDocFormat() { return m_doc_format; }

    // Links and selections functions
    /// sets selection for whole element, clears previous selection
    virtual void selectElement( ldomNode * elem );
    /// sets selection for range, clears previous selection
    virtual void selectRange( const ldomXRange & range );
    /// sets selection for list of words, clears previous selection
    virtual void selectWords( const LVArray<ldomWord> & words );
    /// sets selections for ranges, clears previous selections
    virtual void selectRanges(ldomXRangeList & ranges);
    /// clears selection
    virtual void clearSelection();
    /// update selection -- command handler
    int onSelectionCommand( int cmd, int param );


    /// navigation history
    ldomNavigationHistory & getNavigationHistory() { return _navigationHistory; }
    /// get list of links
    virtual void getCurrentPageLinks( ldomXRangeList & list );
    /// selects first link on page, if any. returns selected link range, null if no links.
    virtual ldomXRange * selectFirstPageLink();
    /// selects next link on page, if any. returns selected link range, null if no links.
    virtual ldomXRange * selectNextPageLink( bool wrapAround);
    /// selects previous link on page, if any. returns selected link range, null if no links.
    virtual ldomXRange * selectPrevPageLink( bool wrapAround );
    /// returns selected link on page, if any. null if no links.
    virtual ldomXRange * getCurrentPageSelectedLink();
    /// follow link, returns true if navigation was successful
    virtual bool goLink( lString16 href, bool savePos=true );
    /// follow selected link, returns true if navigation was successful
    virtual bool goSelectedLink();
    /// go back. returns true if navigation was successful
    virtual bool goBack();
    /// go forward. returns true if navigation was successful
    virtual bool goForward();


    /// create empty document with specified message (to show errors)
    virtual void createDefaultDocument( lString16 title, lString16 message );

    /// returns default font face
    lString8 getDefaultFontFace() { return m_defaultFontFace; }
    /// set default font face
    void setDefaultFontFace( const lString8 & newFace );
    /// returns status bar font face
    lString8 getStatusFontFace() { return m_statusFontFace; }
    /// set status bar font face
    void setStatusFontFace( const lString8 & newFace );
    /// invalidate formatted data, request render
    void requestRender();
    /// invalidate document data, request reload
    void requestReload();
    /// invalidate image cache, request redraw
    void clearImageCache();
#if CR_ENABLE_PAGE_IMAGE_CACHE==1
    /// get page image (0=current, -1=prev, 1=next)
    LVDocImageRef getPageImage( int delta );
    /// returns true if current page image is ready
    bool IsDrawed();
    /// cache page image (render in background if necessary) (0=current, -1=prev, 1=next)
    void cachePageImage( int delta );
#endif
    /// return view mutex
    LVMutex & getMutex() { return _mutex; }
    /// update selection ranges
    void updateSelections();
    void updateBookMarksRanges();
    /// get page document range, -1 for current page
    LVRef<ldomXRange> getPageDocumentRange( int pageIndex=-1 );
    /// get page text, -1 for current page
    lString16 getPageText( bool wrapWords, int pageIndex=-1 );
    /// returns number of non-space characters on current page
    int getCurrentPageCharCount();
    /// returns number of images on current page
    int getCurrentPageImageCount();
    /// calculate page header rectangle
    virtual void getPageHeaderRectangle( int pageIndex, lvRect & headerRc );
    /// calculate page header height
    virtual int getPageHeaderHeight( );
    /// set list of icons to display at left side of header
    void setHeaderIcons( LVRefVec<LVImageSource> icons );
    /// set list of battery icons to display battery state
    void setBatteryIcons( LVRefVec<LVImageSource> icons );
    /// sets page margins
    void setPageMargins( const lvRect & rc );
    /// returns page margins
    lvRect getPageMargins() const { return m_pageMargins; }
#if CR_INTERNAL_PAGE_ORIENTATION==1
    /// sets rotate angle
    void SetRotateAngle( cr_rotate_angle_t angle );
#endif
    /// rotate rectangle by current angle, winToDoc==false for doc->window translation, true==ccw
    lvRect rotateRect( lvRect & rc, bool winToDoc );
    /// rotate point by current angle, winToDoc==false for doc->window translation, true==ccw
    lvPoint rotatePoint( lvPoint & pt, bool winToDoc );
#if CR_INTERNAL_PAGE_ORIENTATION==1
    /// returns rotate angle
    cr_rotate_angle_t GetRotateAngle() { return m_rotateAngle; }
#endif
    /// returns true if document is opened
    bool isDocumentOpened();
    /// returns section bounds, in 1/100 of percent
    LVArray<int> & getSectionBounds( );
    /// sets battery state
    virtual bool setBatteryState( int newState );
    /// returns battery state
    int getBatteryState( ) { return m_battery_state; }
    /// returns current time representation string
    virtual lString16 getTimeString();
    /// returns true if time changed since clock has been last drawed
    bool isTimeChanged();
    /// returns if Render has been called
    bool IsRendered() { return m_is_rendered; }
    /// returns file list with positions/bookmarks
    CRFileHist * getHistory() { return &m_hist; }
    /// returns formatted page list
    LVRendPageList * getPageList() { return &m_pages; }
    /// returns pointer to TOC root node
    LVTocItem * getToc();
    /// returns pointer to TOC root node
    bool getFlatToc( LVPtrVector<LVTocItem, false> & items );
    /// update page numbers for items
    void updatePageNumbers( LVTocItem * item );
    /// set view mode (pages/scroll)
    void setViewMode( LVDocViewMode view_mode, int visiblePageCount=-1 );
    /// get view mode (pages/scroll)
    LVDocViewMode getViewMode();
    /// toggle pages/scroll view mode
    void toggleViewMode();
    /// get window visible page count (1 or 2)
    int getVisiblePageCount();
    /// set window visible page count (1 or 2)
    void setVisiblePageCount( int n );

    /// get page header info mask
    int getPageHeaderInfo() { return m_pageHeaderInfo; }
    /// set page header info mask
    void setPageHeaderInfo( int hdrFlags );
    /// get info line font
    font_ref_t getInfoFont() { return m_infoFont; }
    /// set info line font
    void setInfoFont( font_ref_t font ) { m_infoFont = font; }
    /// draw page header to buffer
    virtual void drawPageHeader( LVDrawBuf * drawBuf, const lvRect & headerRc, int pageIndex, int headerInfoFlags, int pageCount );
    /// draw battery state to buffer
    virtual void drawBatteryState( LVDrawBuf * drawBuf, const lvRect & rc, bool isVertical );

    /// returns background color
    lUInt32 getBackgroundColor()
    {
        return m_backgroundColor;
    }
    /// sets background color
    void setBackgroundColor( lUInt32 cl )
    {
        m_backgroundColor = cl;
        clearImageCache();
    }
    /// returns text color
    lUInt32 getTextColor()
    {
        return m_textColor;
    }
    /// sets text color
    void setTextColor( lUInt32 cl )
    {
        m_textColor = cl;
        clearImageCache();
    }

    /// returns text color
    lUInt32 getStatusColor()
    {
        return m_statusColor;
    }
    /// sets text color
    void setStatusColor( lUInt32 cl )
    {
        m_statusColor = cl;
        clearImageCache();
    }

    CRPageSkinRef getPageSkin();
    void setPageSkin( CRPageSkinRef skin );

    /// returns xpointer for specified window point
    ldomXPointer getNodeByPoint( lvPoint pt );
    /// returns image source for specified window point, if point is inside image
    LVImageSourceRef getImageByPoint(lvPoint pt);
    /// draws scaled image into buffer, clear background according to current settings
    bool drawImage(LVDrawBuf * buf, LVImageSourceRef img, int x, int y, int dx, int dy);
    /// converts point from window to document coordinates, returns true if success
    bool windowToDocPoint( lvPoint & pt );
    /// converts point from documsnt to window coordinates, returns true if success
    bool docToWindowPoint( lvPoint & pt );

    /// returns document
    ldomDocument * getDocument() { return m_doc; }
    /// return document properties
    CRPropRef getDocProps() { return m_doc_props; }
    /// returns book title
    lString16 getTitle() { return m_doc_props->getStringDef(DOC_PROP_TITLE); }
    /// returns book author(s)
    lString16 getAuthors() { return m_doc_props->getStringDef(DOC_PROP_AUTHORS); }
    /// returns book series name and number (series name #1)
    lString16 getSeries()
    {
        lString16 name = m_doc_props->getStringDef(DOC_PROP_SERIES_NAME);
        lString16 number = m_doc_props->getStringDef(DOC_PROP_SERIES_NUMBER);
        if ( !name.empty() && !number.empty() )
            name << L" #" << number;
        return name;
    }

    /// export to WOL format
    bool exportWolFile( const char * fname, bool flgGray, int levels );
    /// export to WOL format
    bool exportWolFile( const wchar_t * fname, bool flgGray, int levels );
    /// export to WOL format
    bool exportWolFile( LVStream * stream, bool flgGray, int levels );

    /// draws page to image buffer
    void drawPageTo( LVDrawBuf * drawBuf, LVRendPageInfo & page, lvRect * pageRect, int pageCount, int basePage);
    /// draws coverpage to image buffer
    void drawCoverTo( LVDrawBuf * drawBuf, lvRect & rc );
    /// returns cover page image source, if any
    LVImageSourceRef getCoverPageImage();
    /// returns cover page image stream, if any
    LVStreamRef getCoverPageImageStream();

    /// returns bookmark
    ldomXPointer getBookmark();
    /// returns bookmark for specified page
    ldomXPointer getPageBookmark( int page );
    /// sets current bookmark
    void setBookmark( ldomXPointer bm );
    /// moves position to bookmark
    void goToBookmark( ldomXPointer bm );
    /// get page number by bookmark
    int getBookmarkPage(ldomXPointer bm);
    /// get bookmark position text
    bool getBookmarkPosText( ldomXPointer bm, lString16 & titleText, lString16 & posText );

    /// returns scrollbar control info
    const LVScrollInfo * getScrollInfo() { updateScroll(); return &m_scrollinfo; }
    /// move to position specified by scrollbar
    bool goToScrollPos( int pos );
    /// converts scrollbar pos to doc pos
    int scrollPosToDocPos( int scrollpos );
    /// returns position in 1/100 of percents
    int getPosPercent();

    /// execute command
    int doCommand( LVDocCmd cmd, int param=0 );

    /// set document stylesheet text
    void setStyleSheet( lString8 css_text );

    /// set default interline space, percent (100..200)
    void setDefaultInterlineSpace( int percent );

    /// change font size, if rollCyclic is true, largest font is followed by smallest and vice versa
    void ZoomFont( int delta );
    /// retrieves current base font size
    int  getFontSize() { return m_font_size; }
    /// sets new base font size
    void setFontSize( int newSize );
    /// retrieves current status bar font size
    int  getStatusFontSize() { return m_status_font_size; }
    /// sets new status bar font size
    void setStatusFontSize( int newSize );

    /// sets posible base font sizes (for ZoomFont)
    void setFontSizes( LVArray<int> & sizes, bool cyclic );

    /// get drawing buffer
    //LVDrawBuf * GetDrawBuf() { return &m_drawbuf; }
    /// draw document into buffer
    //void Draw();

    /// resize view
    void Resize( int dx, int dy );
    /// get view height
    int GetHeight();
    /// get view width
    int GetWidth();

    /// get full document height
    int GetFullHeight();

    /// get vertical position of view inside document
    int GetPos();
    /// get position of view inside document
    void GetPos( lvRect & rc );
    /// set vertical position of view inside document
    int SetPos( int pos, bool savePos=true );

	int getPageHeight(int pageIndex);

    /// get number of current page
    int getCurPage();
    /// move to specified page
    bool goToPage( int page );
    /// returns page count
    int getPageCount();

    /// clear view
    void Clear();
    /// load document from file
    bool LoadDocument( const char * fname );
    /// load document from file
    bool LoadDocument( const lChar16 * fname );
    /// load document from stream
    bool LoadDocument( LVStreamRef stream );

    /// save last file position
    void savePosition();
    /// restore last file position
    void restorePosition();

#ifdef ORIGINAL_FILENAME_PATCH
    void setOriginalFilename( const lString16 & fn ) {
        m_originalFilename = fn;
    }
    const lString16 & getOriginalFilename() {
        return m_originalFilename;
    }
    void setMinFileSizeToCache( int size ) {
        m_props->setInt(PROP_MIN_FILE_SIZE_TO_CACHE, size);
    }
#endif

    /// render (format) document
    void Render( int dx=0, int dy=0, LVRendPageList * pages=NULL );
    /// set properties before rendering
    void setRenderProps( int dx, int dy );

    /// Constructor
    LVDocView( int bitsPerPixel=-1 );
    /// Destructor
    virtual ~LVDocView();
};


#endif
