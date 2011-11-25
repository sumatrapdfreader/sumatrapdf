/***************************************************************************
 *   Copyright (C) 2007 by Vadim Lopatin   *
 *   vadim.lopatin@coolreader.org   *
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
#ifndef CR_GUI_INCLUDED
#define CR_GUI_INCLUDED

#include "lvtypes.h"
#include "lvstring.h"
#include "lvptrvec.h"
#include "lvdrawbuf.h"
#include "lvdocview.h"
#include "crskin.h"

#ifdef CR_WX_SUPPORT
#include <wx/wx.h>
#endif

class CRGUIWindowManager;

#define KEY_FLAG_LONG_PRESS 1

/// Accelerator table entry: to convert keypress to command
class CRGUIAccelerator
{
    public:
        int keyCode;
        int keyFlags;
        int commandId;
        int commandParam;
};

/// accelerator table
class CRGUIAcceleratorTable
{
protected:
    LVPtrVector<CRGUIAccelerator> _items;
    int indexOf( int keyCode, int keyFlags )
    {
        //CRLog::trace( "indexOf( %d, %d )", (int)keyCode, (int)keyFlags );
        for ( int i=0; i<_items.length(); i++ ) {
            //CRLog::trace( " compare( %d, %d )", (int)_items[i]->keyCode, (int)_items[i]->keyFlags );
            if ( _items[i]->keyCode==keyCode && _items[i]->keyFlags==keyFlags ) {
                //CRLog::trace(" found index = %d", i);
                return i;
            }
        }
        return -1;
    }
public:
	/// find key by command
    bool findCommandKey( int cmd, int param, int & keyCode, int & keyFlags )
    {
        //CRLog::trace( "indexOf( %d, %d )", (int)keyCode, (int)keyFlags );
        for ( int i=0; i<_items.length(); i++ ) {
            //CRLog::trace( " compare( %d, %d )", (int)_items[i]->keyCode, (int)_items[i]->keyFlags );
			if ( _items[i]->commandId==cmd && _items[i]->commandParam==param ) {
                //CRLog::trace(" found index = %d", i);
				keyCode = _items[i]->keyCode;
				keyFlags = _items[i]->keyFlags;
				return true;
            }
        }
        return false;
    }
	/// get item by index
	const CRGUIAccelerator * get( int index ) const
	{
		return _items[ index ];
	}

	/// debug dump of table
    void dump()
    {
        if ( CRLog::isTraceEnabled() ) {
#if 0
            CRLog::trace("Accelerator table:");
            for ( int i=0; i<_items.length(); i++ ) {
                CRGUIAccelerator * p = _items[i];
                CRLog::trace("%d, %d => %d, %d\n", p->keyCode, p->keyFlags, p->commandId, p->commandParam);
            }
#endif
        }
    }
	/// returns number of entries
	unsigned length() { return _items.length(); }
    /// remove accelerator from table
    bool remove( int keyCode, int keyFlags )
    {
        int index = indexOf( keyCode, keyFlags );
        if ( index >= 0 ) {
            _items.erase( index, 1 );
            return true;
        }
        return false;
    }

    /// add accelerator to table or change existing
    bool add( int keyCode, int keyFlags, int commandId, int commandParam );

	/// add all items from another table
	void addAll( const CRGUIAcceleratorTable & v );

    /// translate keycode to command, returns true if translated
    bool translate( int keyCode, int keyFlags, int & commandId, int & commandParam )
    {
        int index = indexOf( keyCode, keyFlags );
        if ( index<0 )
            return false;
        commandId = _items[index]->commandId;
        commandParam = _items[index]->commandParam;
        return true;
    }
    /// translate keycode to command, returns true if translated
    const CRGUIAccelerator * findKeyAccelerator( int keyCode, int keyFlags )
    {
        int index = indexOf( keyCode, keyFlags );
        if ( index<0 )
            return NULL;
        return get(index);
    }
    /// empty table constructor
    CRGUIAcceleratorTable() { }
    /// copy constructor
    CRGUIAcceleratorTable( const CRGUIAcceleratorTable& v) 
	{
		for ( int i=0; i<v._items.length(); i++ ) {
			_items.add( new CRGUIAccelerator(*v._items[i]) );
		}
	}
    /// constructor from int array: 4 ints per entry (keyCode, keyFlags, commandId, commandParam), keyCode==0 indicates end of list 
    CRGUIAcceleratorTable( const int * tableQuadsArray )
    {
        while( *tableQuadsArray ) {
            CRGUIAccelerator * item = new CRGUIAccelerator();
            item->keyCode = *tableQuadsArray++;
            item->keyFlags = *tableQuadsArray++;
            item->commandId = *tableQuadsArray++;
            item->commandParam = *tableQuadsArray++;
            _items.add(item);
        }
    }
};

/// accelerator table reference
typedef LVRef<CRGUIAcceleratorTable> CRGUIAcceleratorTableRef;

/**
 * \brief Container for list of named accelerator tables read from files
 *
 * File format:
 * there are two files: 
 *   1) definition file which has key code and command id definitions
 *   2) keymap file which has several key->command tables
 *
 * definition file format:
 *   # there may be comment lines started with # character
 *   other lines should be in format
 *   identifier=value
 *   where identifier is alphanumeric identifier to name value
 *   value is either decimal number, hex number with 0x prefix, or character in ''
 * example: 
 *
 * # this is sample definition file
 * XK_Return=0xFF01
 * KEY_1='1'
 * KEY_SPACE=32
 * CMD_CLOSE=100
 *
 * keymap file format:
 *   # there may be comment lines started with # character
 *   file should be divided into sections using lines like
 *   [sectionname]
 *   other lines should be in format
 *   key[,keyflags]=cmd[,cmdparam]
 *   (keyflags and cmdparam are optional)
 *   key, keyflags, cmd, and cmdparam should be either
 *   identifier defined in definition file, decimal number, hex number with 0x prefix, or character in ''
 *   it's better to avoid constants in this file, instead just place them to definition file and use by identifiers
 *
 * example:
 *
 * # this is sample keymap file
 * [mainwindow]
 * #this is keymap table for main window, accessible by name "mainwindow"
 * XK_Return=CMD_SHOW_MENU, 0
 * XK_Return,LONG=CMD_SHOW_MENU_SETTINGS
 * XK_Down,LONG=CMD_MOVE_FORWARD, 10
 * '1'=CMD_GO_TO_PAGE
 *
 */
class CRGUIAcceleratorTableList
{
private:
    LVHashTable<lString16, CRGUIAcceleratorTableRef> _table;
public:
	/// add all tables
	void addAll( const CRGUIAcceleratorTableList & v );
    /// remove all tables
    void clear() { _table.clear(); }
    /// add accelerator table definition from array
    void add( const char * name, const int * defs )
    {
        add( lString16( name ), defs );
    }
    /// add accelerator table definition from array
    void add( const lString16 & name, const int * defs )
    {
        _table.set( name, CRGUIAcceleratorTableRef( new CRGUIAcceleratorTable( defs ) ) );
    }
    /// find accelerator table by name
    CRGUIAcceleratorTableRef get( const lString16 & name ) { return _table.get( name ); }
    CRGUIAcceleratorTableRef get( const lString16 & name, CRPropRef keyRemappingOptions );
    /// find accelerator table by name
    CRGUIAcceleratorTableRef get( const char * name ) { return _table.get( lString16( name ) ); }
    /// returns true if there are no no tables in list
    bool empty() { return _table.length()==0; }
    /// constructs empty list, then it should be filled from file using openFromFile()
    CRGUIAcceleratorTableList() : _table( 32 )
    {
    }
    ~CRGUIAcceleratorTableList() { }
    /// reads definitions from files
    bool openFromFile( const char  * defFile, const char * mapFile );
};

class CRKeyboardLayout
{
	lString16Collection _items;
public:
	CRKeyboardLayout() { }
	const lString16Collection & getItems() { return _items; }
	lString16 get( int i )
	{
		if ( i<0 || i>= (int)_items.length() )
			return lString16();
		return _items[i];
	}
	void set( int index, lString16 chars )
	{
		if ( index<0 || index>20 )
			return;
		while ( (int)_items.length() <= index )
			_items.add(lString16());
		_items[ index ] = chars;
	}
};

class CRKeyboardLayoutSet
{
public:
	lString16 name;
	LVRef<CRKeyboardLayout> vKeyboard;
	LVRef<CRKeyboardLayout> tXKeyboard;
	CRKeyboardLayoutSet()
		: vKeyboard( new CRKeyboardLayout() ), tXKeyboard( new CRKeyboardLayout() )
	{
	}
	CRKeyboardLayoutSet( const CRKeyboardLayoutSet & v )
		: name( v.name), vKeyboard( v.vKeyboard ), tXKeyboard( v.tXKeyboard )
	{
	}
	CRKeyboardLayoutSet & operator = ( const CRKeyboardLayoutSet & v )
	{
		name = v.name;
		vKeyboard = v.vKeyboard;
		tXKeyboard = v.tXKeyboard;
        return *this;
	}
};

typedef LVRef<CRKeyboardLayoutSet> CRKeyboardLayoutRef;

class CRKeyboardLayoutList
{
    LVHashTable<lString16, CRKeyboardLayoutRef> _table;
	CRKeyboardLayoutRef _current;
public:
	// get currently set layout
	CRKeyboardLayoutRef getCurrentLayout();
	// get next layout
	CRKeyboardLayoutRef nextLayout();
	// get previous layout
	CRKeyboardLayoutRef prevLayout();

	CRKeyboardLayoutRef get( lString16 name ) { return _table.get( name ); }
	void set( lString16 name, CRKeyboardLayoutRef v ) { _table.set( name, v ); }
	CRKeyboardLayoutList() : _table(16) { }
    /// reads definitions from files
    bool openFromFile( const char  * layoutFile );
};

/// i18n support interface
class CRGUIStringTranslator
{
public:
    /// translate string by key, return default value if not found
    virtual lString16 translateString( const char *, const char * defValue )
    {
        return Utf8ToUnicode( lString8(defValue) );
    }
    virtual ~CRGUIStringTranslator() { }
};

enum CRGUIEventType {
    CREV_WINDOW_EVENTS_START=1,
    CREV_KEYDOWN = 1,
    CREV_KEYUP,
    CREV_COMMAND,

    CREV_WM_EVENTS_START=100,
    CREV_UPDATE = 100,
    CREV_RESIZE,

};

class CRGUIWindow;
class CRGUIWindowManager;

class CRGUIEvent
{
protected:
    int _type;
    CRGUIWindow * _targetWindow;
    int _param1;
    int _param2;
public:
    virtual bool isForVisibleOnly() { return false; }
    virtual bool isForModalOnly() { return false; }
    virtual bool isWindowEvent() { return _type<CREV_WM_EVENTS_START; }
    virtual bool isWMEvent() { return _type>=CREV_WM_EVENTS_START; }
    int getType() { return _type; }
    virtual bool handle( CRGUIWindow * window ) { return false; }
    virtual bool handle( CRGUIWindowManager * wm ) { return false; }
    CRGUIEvent & setParam1( int v ) { _param1=v; return *this; }
    CRGUIEvent & setParam2( int v ) { _param2=v; return *this; }
    CRGUIEvent & setTargetWindow( CRGUIWindow * targetWindow ) { _targetWindow = targetWindow; return *this; }
    int getParam1() { return _param1; }
    int getParam2() { return _param2; }
    CRGUIEvent( int type ) : _type(type), _targetWindow(NULL)
    {

    }
    virtual ~CRGUIEvent() { }
};


/// Screen object - provides canvas and interface to device screen
class CRGUIScreen
{
    public:
        // for turbo updates
        enum UpdateMode {
            NormalMode,
            PrepareMode,
        };
        virtual void setTurboUpdateEnabled( bool flg ) { }
        virtual bool getTurboUpdateEnabled() {  return false; }
        virtual bool getTurboUpdateSupported() {  return false; }
        virtual void setTurboUpdateMode( UpdateMode mode ) { }
        /// fast update feature parameter setting
        virtual void setFullUpdateInterval( int pagesBeforeFullupdate=1 ) = 0;
        /// creates compatible canvas of specified size
        virtual LVDrawBuf * createCanvas( int dx, int dy ) = 0;
        /// sets new screen size, returns true if size is changed
        virtual bool setSize( int dx, int dy ) = 0;
        /// returns screen width
        virtual int getWidth() = 0;
        /// returns screen height
        virtual int getHeight() = 0;
        /// returns screen dimension
        virtual lvRect getRect() { return lvRect(0, 0, getWidth(), getHeight() ); }
        /// return pointer to screen canvas
        virtual LVRef<LVDrawBuf> getCanvas() = 0;
        /// draw image on screen canvas
        virtual void draw( LVDrawBuf * img, int x = 0, int y = 0) = 0;
        /// transfers contents of buffer to device, if full==true, redraws whole screen, otherwise only changed area
        virtual void flush( bool full ) = 0;
        /// invalidates rectangle: add it to bounding box of next partial update
        virtual void invalidateRect( const lvRect & rc ) { }
        virtual ~CRGUIScreen() { }
};

/// window configure flag, on screen size change
#define CRGUI_CONFIGURE_FLAG_SCREEN_SIZE 1
/// window configure flag, on screen orientation change
#define CRGUI_CONFIGURE_FLAG_SCREEN_ORIENTATION 2

/// Window interface
class CRGUIWindow
{
    public:
        /// override to handle
        virtual bool handleEvent( CRGUIEvent * event )
        {
            if ( !event->isWindowEvent() )
                return false;
            // by default, allow event to do something with window
            return event->handle( this );
        }
        /// sets scroll label (e.g. "Page $1 of $2" or "$1 / $2")
        virtual void setScrollLabelTemplate( lString16 text ) = 0;
        /// returns scroll label (e.g. "$1 of $2")
        virtual lString16 getScrollLabelTemplate() = 0;
        /// sets skin name for window
        virtual void setSkinName( const lString16  & skin ) = 0;
        /// returns skin name for window
        virtual lString16 getSkinName() = 0;
        /// set accelerator table for window
        virtual void setAccelerators( CRGUIAcceleratorTableRef ) { }
        /// get window accelerator table
        virtual CRGUIAcceleratorTableRef getAccelerators() { return CRGUIAcceleratorTableRef(); }
        /// returns true if key is processed
        virtual bool onKeyPressed( int key, int flags = 0 ) = 0;
        /// returns true if command is processed
        virtual bool onCommand( int command, int params = 0 ) = 0;
        /// returns true if window is visible
        virtual bool isVisible() const = 0;
        /// returns true if window is fullscreen
        virtual bool isFullscreen() = 0;
        /// returns true if window is changed but now drawn
        virtual bool isDirty() = 0;
        /// sets dirty flag
        virtual void setDirty() = 0;
        /// shows or hides window
        virtual void setVisible( bool visible ) = 0;
        /// called on system configuration change: screen size and orientation
        virtual void reconfigure( int flags ) = 0;
        /// returns window rectangle
        virtual const lvRect & getRect() = 0;
        /// sets window rectangle
        virtual void setRect( const lvRect & rc ) = 0;
        /// draws content of window to screen
        virtual void flush() = 0;
        /// called if window gets focus
        virtual void activated() { setDirty(); }
        /// called if window loss focus
        virtual void covered() { }
        /// called if window is being closed
        virtual void closing() { }
        /// returns window manager
        virtual CRGUIWindowManager * getWindowManager() = 0;
        /// destroys window
        virtual ~CRGUIWindow() { }
};

/// Window manager
class CRGUIWindowManager : public CRGUIStringTranslator
{
    protected:
        LVPtrVector<CRGUIWindow, true> _windows;
        LVPtrVector<CRGUIEvent, true> _events;
        CRGUIScreen * _screen;
        /// if true, we should delete screen in destructor
        bool _ownScreen;
        LVRef<CRGUIStringTranslator> _i18n;
        int _postedCommand;
        int _postedCommandParam;
        time_t _lastProgressUpdate;
        int _lastProgressPercent;
        CRSkinRef _skin;
        CRGUIAcceleratorTableList _accTables;
		CRKeyboardLayoutList _kbLayouts;
        cr_rotate_angle_t _orientation;
        LVRefVec<LVImageSource> m_batteryIcons;
        bool _stopFlag;
    public:
        /// forward events from system queue to application queue
        virtual void forwardSystemEvents( bool waitForEvent ) { }
        /// post application event to message queue
        virtual void postEvent( CRGUIEvent * event );
        /// peeks head of application message queue, w/o removing from queue (returns NULL if no events in queue)
        virtual CRGUIEvent * peekEvent()
        {
            forwardSystemEvents( false );
            return _events.peekHead();
        }
        /// returns head of application message queue, removing from queue (returns NULL if no events in queue)
        virtual CRGUIEvent * getEvent()
        {
            forwardSystemEvents( false );
            return _events.popHead();
        }
        /// handle all events from queue
        virtual bool handleAllEvents( bool waitForEvent );
        /// override to handle
        virtual bool handleEvent( CRGUIEvent * event );
        /// called when message queue is empty and application is going to wait for event
        virtual void idle() { }

        /// returns list of battery icons
        virtual LVRefVec<LVImageSource> & getBatteryIcons() { return m_batteryIcons; }
        /// set list of battery icons to display battery state
        virtual void setBatteryIcons( LVRefVec<LVImageSource> icons )
        {
            m_batteryIcons.clear();
            m_batteryIcons.add(icons);
        }
        /// draw battery state to specified rectangle of screen
        virtual void drawBattery( LVDrawBuf & buf, const lvRect & rc );
        /// sets screen orientation value, to be red by corresponding getter. Doesn't rotate screen actually.
        virtual void setScreenOrientation( cr_rotate_angle_t angle ) { _orientation = angle; }
        /// returns current screen orientation
        virtual cr_rotate_angle_t getScreenOrientation() { return _orientation; }
        /// draws icon at center of screen, with optional progress gauge
        virtual void showWaitIcon( lString16 filename, int progressPercent=-1 );
        /// draws icon with gauge at center of screen, skipping too frequent updates
        virtual void showProgress( lString16 filename, int progressPercent );
		/// loads skin from file
	    virtual bool loadSkin( lString16 pathname );
		/// returns keyboard layouts
		virtual CRKeyboardLayoutList & getKeyboardLayouts() { return _kbLayouts; }
        /// returns accelerator table list
        virtual CRGUIAcceleratorTableList & getAccTables() { return _accTables; }
        /// return battery status
        virtual bool getBatteryStatus( int & percent, bool & charging )
        {
            // stub
            percent = 0; charging = false; return false;
        }
        /// set skin
        virtual void setSkin( CRSkinRef skin ) { _skin = skin; }
        /// returns currently selected skin
        virtual CRSkinRef getSkin() { return _skin; }
        /// sets another i18n translator
        virtual void setTranslator( LVRef<CRGUIStringTranslator> i18n )
        {
            _i18n = i18n;
        }
        /// translate string by key, return default value if not found
        virtual lString16 translateString( const char * key, const char * defValue )
        {
            if ( _i18n.isNull() )
                return Utf8ToUnicode( lString8(defValue) );
            return _i18n->translateString( key, defValue );
        }
        /// returns count of windows
        virtual int getWindowCount() { return _windows.length(); }
        /// changes screen size and orientation
        virtual void reconfigure( int dx, int dy, cr_rotate_angle_t orientation );
        /// adds command to message queue
        virtual void postCommand( int command, int params = 0 );
        /// runs posted events (commands)
        virtual bool processPostedEvents()
        {
            return handleAllEvents( false );
        }
        /// returns true if command is processed
        virtual bool onCommand( int command, int params = 0 )
        {
            for ( int i=_windows.length()-1; i>=0; i-- ) {
                if ( _windows[i]->isVisible() && _windows[i]->onCommand( command, params ) )
                    return true;
            }
            return false;
        }
        /// returns true if key is processed
        virtual bool onKeyPressed( int key, int flags = 0 );
        /// returns top visible window
        CRGUIWindow * getTopVisibleWindow()
        {
            for ( int i=_windows.length()-1; i>=0; i-- ) {
                if ( !_windows[i]->isVisible() )
                    continue;
                return _windows[i];
            }
            return NULL;
        }
        /// shows or hides window
        void showWindow( CRGUIWindow * window, bool visible )
        {
            int index = _windows.indexOf( window );
            if ( index >= 0  ) { //&& window->isVisible()!=visible
                window->setVisible( visible );
                if ( !visible ) {
                    window->covered();
                    CRGUIWindow * wnd = getTopVisibleWindow();
                    if ( wnd )
                        activateWindow( wnd );
                } else
                    activateWindow( window );
            }
        }
        /// activates window, brings it on top; add to stack if not added
        void activateWindow( CRGUIWindow * window );
        /// closes window, removes from stack, destroys object
        void closeWindow( CRGUIWindow * window );
        /// redraw one window
        virtual void updateWindow( CRGUIWindow * window );
        /// full redraw of all windows
        void update( bool fullScreenUpdate, bool forceFlushScreen=true );
        /// returns screen associated with window manager
        virtual CRGUIScreen * getScreen()
        {
            return _screen;
        }
        /// runs event loop
        virtual int runEventLoop();
        /// constructor
        CRGUIWindowManager(CRGUIScreen * screen)
        : _screen( screen ), _ownScreen(false)
        , _postedCommand(0)
        , _postedCommandParam(0)
        ,_lastProgressUpdate(0)
        ,_lastProgressPercent(-1)
        ,_orientation(CR_ROTATE_ANGLE_0)
        ,_stopFlag(false)
        {
        }
        virtual void closeAllWindows()
        {
            for ( int i=_windows.length()-1; i>=0; i-- ) {
                closeWindow(_windows[i]);
            }
        }
        /// destroy all windows on close
        virtual ~CRGUIWindowManager()
        {
            closeAllWindows();
            if ( _ownScreen )
                delete _screen;
        }
};

/// Window base implementation
class CRGUIWindowBase : public CRGUIWindow
{
    protected:
        CRGUIWindowManager * _wm;
        lvRect _rect;
        bool _visible;
        bool _fullscreen;
        bool _dirty;
        bool _passKeysToParent;
        bool _passCommandsToParent;
        int _page;
        int _pages;
        CRGUIAcceleratorTableRef _acceleratorTable;
        lString16 _skinName;
        lString16 _scrollLabel;
        lString16 _caption;
        lString16 _statusText;
        lString16 _inputText;
        LVImageSourceRef _icon; // window title icon
        // draws frame, title, status and client
        virtual void draw();

        /// use to override status text
        virtual lString16 getStatusText() { return _statusText; }

        /// draw status bar using current skin, with optional status text and scroll/tab/page indicator
        virtual void drawStatusBar();
        /// draw status text
        virtual void drawStatusText( LVDrawBuf & buf, const lvRect & rc, CRRectSkinRef skin );
        /// draw title bar using current skin, with optional scroll/tab/page indicator
        virtual void drawTitleBar();
        /// draw input box, if any
        virtual void drawInputBox();
        /// draw title bar using current skin, with optional scroll/tab/page indicator
        virtual void drawClient();

        /// calculates title rectangle for window rectangle
        virtual bool getTitleRect( lvRect & rc );
        /// calculates status rectangle for window rectangle
        virtual bool getStatusRect( lvRect & rc );
        /// calculates client rectangle for window rectangle
        virtual bool getClientRect( lvRect & rc );
        /// calculates input box rectangle for window rectangle
        virtual bool getInputRect( lvRect & rc );
        /// calculates scroll rectangle for window rectangle
        virtual bool getScrollRect( lvRect & rc );
    public:
        /// use to override status text
        virtual void setStatusText( lString16 s ) { _statusText = s; }
        /// formats scroll label (like "1 of 2")
        virtual lString16 getScrollLabel( int page, int pages );
        /// calculates minimum scroll size
        virtual lvPoint getMinScrollSize( int page, int pages );
        /// sets scroll label (e.g. "Page $1 of $2" or "$1 / $2")
        virtual void setScrollLabelTemplate( lString16 text ) { _scrollLabel=text; }
        /// returns scroll label (e.g. "$1 of $2")
        virtual lString16 getScrollLabelTemplate() { return _scrollLabel; }
        /// called on system configuration change: screen size and orientation
        virtual void reconfigure( int flags );
        /// sets skin name for window
        virtual void setSkinName( const lString16  & skin ) { _skinName = skin; }
        /// returns skin name for window
        virtual lString16 getSkinName() { return _skinName; }
        /// returns true if command is processed
        virtual bool onCommand( int command, int params = 0 ) { return !_passCommandsToParent; }
        /// returns true if key is processed (by default, let's translate key to command using accelerator table)
        virtual bool onKeyPressed( int key, int flags = 0 );
        /// set accelerator table for window
        virtual void setAccelerators( CRGUIAcceleratorTableRef table ) { _acceleratorTable = table; }
        /// get window accelerator table
        virtual CRGUIAcceleratorTableRef getAccelerators() { return _acceleratorTable; }
        /// returns window width
        inline int getWidth() { return getRect().width(); }
        /// returns window height
        inline int getHeight() { return getRect().height(); }
        /// sets dirty flag
        virtual void setDirty() { _dirty = true; }
        /// returns true if window is changed but now drawn
        virtual bool isDirty() { return _dirty; }
        /// shows or hides window
        virtual void setVisible( bool visible ) { _visible = visible; setDirty(); }
        virtual bool isVisible() const { return _visible; }
        virtual const lvRect & getRect() { return _rect; }
        virtual void setRect( const lvRect & rc ) { _rect = rc; setDirty(); }
        virtual void flush() { draw(); _dirty = false; }
        /// returns true if window is fullscreen
        virtual bool isFullscreen() { return _fullscreen; }
        /// set fullscreen state for window
        virtual void setFullscreen( bool fullscreen ) { _fullscreen = fullscreen; }
        virtual CRGUIWindowManager * getWindowManager() { return _wm; }
        CRGUIWindowBase( CRGUIWindowManager * wm )
        : _wm(wm), _visible(true), _fullscreen(true), _dirty(true), _passKeysToParent(false), _passCommandsToParent(false)
        , _page(0), _pages(0)
        {
            // fullscreen visible by default
            _rect = _wm->getScreen()->getRect();
            //_statusText = L"Sample status text";
        }
        virtual ~CRGUIWindowBase() { }
};

/// Base Screen class implementation
class CRGUIScreenBase : public CRGUIScreen
{
    protected:
        int _width;
        int _height;
        lvRect _updateRect;
        LVRef<LVDrawBuf> _canvas;
        LVRef<LVDrawBuf> _front;
        int _fullUpdateInterval;
        int _fullUpdateCounter;
        /// override in ancessor to transfer image to device
        virtual void update( const lvRect & rc, bool full ) = 0;
    public:
        /// fast update feature parameter setting
        virtual void setFullUpdateInterval( int pagesBeforeFullupdate=1 )
        {
            _fullUpdateInterval = pagesBeforeFullupdate;
            if ( _fullUpdateInterval>0 )
                _fullUpdateCounter = _fullUpdateInterval;
        }
        virtual bool checkFullUpdateCounter()
        {
            if ( _fullUpdateInterval<=0 )
                return false; // always partial update
            if ( _fullUpdateInterval==1 )
                return true;  // always full update
            _fullUpdateCounter--;
            if ( _fullUpdateCounter<=0 ) {
                _fullUpdateCounter = _fullUpdateInterval;
                return true; // full update
            }
            return false; // partial update
        }

        /// creates compatible canvas of specified size
        virtual LVDrawBuf * createCanvas( int dx, int dy )
        {
#if (COLOR_BACKBUFFER==1)
            LVDrawBuf * buf = new LVColorDrawBuf( dx, dy );
#else
            LVDrawBuf * buf = new LVGrayDrawBuf( dx, dy, GRAY_BACKBUFFER_BITS );
#endif
            return buf;
        }
        /// sets new screen size
        virtual bool setSize( int dx, int dy )
        {
            if ( _width!=dx || _height != dy ) {
                _width = dx;
                _height = dy;
                _canvas = LVRef<LVDrawBuf>( createCanvas( dx, dy ) );
                if ( !_front.isNull() )
                    _front = LVRef<LVDrawBuf>( createCanvas( dx, dy ) );
                return true;
            }
            return false;
        }

        /// returns screen width
        virtual int getWidth() { return _width; }
        /// returns screen height
        virtual int getHeight() { return _height; }
        /// return pointer to screen canvas
        virtual LVRef<LVDrawBuf> getCanvas() { return _canvas; }
        /// draw image on screen canvas
        virtual void draw( LVDrawBuf * img, int x = 0, int y = 0)
        {
            img->DrawTo( _canvas.get(), x, y, 0, NULL );
        }
        /// transfers contents of buffer to device, if full==true, redraws whole screen, otherwise only changed area
        virtual void flush( bool full );
        /// invalidates rectangle: add it to bounding box of next partial update
        virtual void invalidateRect( const lvRect & rc )
        {
            _updateRect.extend( rc );
        }
        CRGUIScreenBase( int width, int height, bool doublebuffer  )
        : _width( width ), _height( height ), _canvas(NULL), _front(NULL)
        , _fullUpdateInterval(1)
        , _fullUpdateCounter(1)
        {
            if ( width && height ) {
                _canvas = LVRef<LVDrawBuf>( createCanvas( width, height ) );
                if ( doublebuffer )
                    _front = LVRef<LVDrawBuf>( createCanvas( width, height ) );
            }
        }
        virtual ~CRGUIScreenBase()
        {
        }
};

#ifdef CR_WX_SUPPORT
/// WXWidget support: draw to wxImage
class CRWxScreen : public CRGUIScreenBase
{
    protected:
        wxBitmap _wxbitmap;
        virtual void update( const lvRect & rc, bool full )
        {
            wxImage img;
            int dyy = _canvas->GetHeight();
            int dxx = _canvas->GetWidth();
            int dx = dxx;
            int dy = dyy;
            img.Create(dx, dy, true);
            unsigned char * bits = img.GetData();
            for ( int y=0; y<dy && y<dyy; y++ ) {
                int bpp = _canvas->GetBitsPerPixel();
                if ( bpp==32 ) {
                    const lUInt32* src = (const lUInt32*) _canvas->GetScanLine( y );
                    unsigned char * dst = bits + y*dx*3;
                    for ( int x=0; x<dx && x<dxx; x++ )
                    {
                        lUInt32 c = *src++;
                        *dst++ = (c>>16) & 255;
                        *dst++ = (c>>8) & 255;
                        *dst++ = (c>>0) & 255;
                    }
                } else if ( bpp==2 ) {
                    //
                    static const unsigned char palette[4][3] = {
                        { 0xff, 0xff, 0xff },
                        { 0xaa, 0xaa, 0xaa },
                        { 0x55, 0x55, 0x55 },
                        { 0x00, 0x00, 0x00 },
                    };
                    const lUInt8* src = (const lUInt8*) _canvas->GetScanLine( y );
                    unsigned char * dst = bits + y*dx*3;
                    for ( int x=0; x<dx && x<dxx; x++ )
                    {
                        lUInt32 c = (( src[x>>2] >> ((3-(x&3))<<1) ))&3;
                        *dst++ = palette[c][0];
                        *dst++ = palette[c][1];
                        *dst++ = palette[c][2];
                    }
                } else if ( bpp==1 ) {
                    //
                    static const unsigned char palette[2][3] = {
                        { 0xff, 0xff, 0xff },
                        { 0x00, 0x00, 0x00 },
                    };
                    const lUInt8* src = (const lUInt8*) _canvas->GetScanLine( y );
                    unsigned char * dst = bits + y*dx*3;
                    for ( int x=0; x<dx && x<dxx; x++ )
                    {
                        lUInt32 c = (( src[x>>3] >> ((7-(x&7))) ))&1;
                        *dst++ = palette[c][0];
                        *dst++ = palette[c][1];
                        *dst++ = palette[c][2];
                    }
                }
            }

            // copy to bitmap
            wxBitmap bmp( img );
            _wxbitmap = bmp;
        }
    public:
        CRWxScreen( int width, int height )
        :  CRGUIScreenBase( width, height, true ) { }
        wxBitmap getWxBitmap() { return _wxbitmap; }
};
#endif

/// Window to show LVDocView contents
class CRDocViewWindow : public CRGUIWindowBase
{
    protected:
        LVDocView * _docview;
	    CRWindowSkinRef _skin;
        virtual void draw();
    public:
        LVDocView * getDocView()
        {
            return _docview;
        }
        CRDocViewWindow( CRGUIWindowManager * wm )
        : CRGUIWindowBase( wm )
        {
            CRLog::trace("CRDocViewWindow()");
            _docview = new LVDocView( wm->getScreen()->getCanvas()->GetBitsPerPixel() );
            CRLog::trace("resizing...");
            _docview->Resize( getWidth(), getHeight() );
            _docview->setPageMargins( lvRect(10, 10, 10, 10) );
            CRLog::trace("CRDocViewWindow() finished");
        }
        virtual ~CRDocViewWindow()
        {
            delete _docview;
            _docview = NULL;
        }

        virtual void setRect( const lvRect & rc );

        /// returns true if command is processed
        virtual bool onCommand( int command, int params );

		/// returns true if window is changed but now drawn
        virtual bool isDirty()
        {
            return _dirty || !_docview->isPageImageReady( 0 );
        }
};





//===========================================================================================
// MENU SUPPORT

enum CRMenuControlCmd {
    MCMD_CANCEL=500,
    MCMD_OK,
    MCMD_SCROLL_FORWARD,
    MCMD_SCROLL_BACK,
    MCMD_SELECT_0,
    MCMD_SELECT_1,
    MCMD_SELECT_2,
    MCMD_SELECT_3,
    MCMD_SELECT_4,
    MCMD_SELECT_5,
    MCMD_SELECT_6,
    MCMD_SELECT_7,
    MCMD_SELECT_8,
    MCMD_SELECT_9,
    MCMD_SELECT_0_LONG,
    MCMD_SELECT_1_LONG,
    MCMD_SELECT_2_LONG,
    MCMD_SELECT_3_LONG,
    MCMD_SELECT_4_LONG,
    MCMD_SELECT_5_LONG,
    MCMD_SELECT_6_LONG,
    MCMD_SELECT_7_LONG,
    MCMD_SELECT_8_LONG,
    MCMD_SELECT_9_LONG,
    MCMD_SCROLL_FORWARD_LONG,
    MCMD_SCROLL_BACK_LONG,
    MCMD_CLEAR,
};

enum CRGUICmd {
	GCMD_PASS_TO_PARENT = 550,
};

class CRMenu;

/// CRGUI menu item base class
class CRMenuItem
{
    protected:
        CRMenu * _menu;
        int _id;
        lString16 _label;
        LVImageSourceRef _image;
        LVFontRef _defFont;
        lString16 _propValue;
    public:
        /// id of item
        int getId() { return _id; }
        /// set id of item
        void setId( int id ) { _id = id; }
        /// item label
        lString16 getLabel() { return _label; }
        /// item icon
        LVImageSourceRef getImage() { return _image; }
        /// item label font
        virtual LVFontRef getFont() { return _defFont; }
        /// constructor
        CRMenuItem( CRMenu * menu, int id, lString16 label, LVImageSourceRef image, LVFontRef defFont, const lChar16 * propValue=NULL  )
    : _menu(menu), _id(id), _label(label), _image(image), _defFont(defFont), _propValue(propValue) { }
        /// constructor
        CRMenuItem( CRMenu * menu, int id, const char * label, LVImageSourceRef image, LVFontRef defFont, const lChar16 * propValue=NULL  )
    : _menu(menu), _id(id), _label(label), _image(image), _defFont(defFont), _propValue(propValue) { }
        /// measures item size
        virtual lvPoint getItemSize( CRRectSkinRef skin );
        /// draws item
        virtual void Draw( LVDrawBuf & buf, lvRect & rc, CRRectSkinRef skin, CRRectSkinRef valueSkin, bool selected );
        /// returns true if submenu
        virtual bool isSubmenu() const { return false; }
        /// called on item selection
        virtual int onSelect() { return 0; }
        virtual ~CRMenuItem() { }
        /// submenu for options dialog support
        virtual lString16 getSubmenuValue() { return lString16(); }
        /// property value, for options editor support
        virtual lString16 getPropValue() { return _propValue; }
};

/// CRGUI menu base class
class CRMenu : public CRGUIWindowBase, public CRMenuItem {
    protected:
        LVPtrVector<CRMenuItem> _items;
        CRPropRef _props;
        lString16 _propName;
        LVFontRef _valueFont;
        int _topItem;
        int _pageItems;
        int _helpHeight;
        int _cmdToHighlight;
        CRMenuSkinRef _skin;// = _wm->getSkin()->getMenuSkin( path.c_str() );
        // override for CRGUIWindow method
        virtual void draw();
        virtual void Draw( LVDrawBuf & buf, lvRect & rc, CRRectSkinRef skin, CRRectSkinRef valueSkin, bool selected );
        //virtual void Draw( LVDrawBuf & buf, int x, int y );
        virtual void highlightCommandItem( int cmd );
    public:
        /// returns index of selected item, -1 if no item selected
        virtual int getSelectedItemIndex();
        virtual void activated();
        virtual void drawClient();
        virtual int getScrollHeight();
        CRMenuSkinRef getSkin();
        CRMenu( CRGUIWindowManager * wm, CRMenu * parentMenu, int id, lString16 label, LVImageSourceRef image, LVFontRef defFont, LVFontRef valueFont, CRPropRef props=CRPropRef(), const char * propName=NULL, int pageItems=8 )
        : CRGUIWindowBase( wm ), CRMenuItem( parentMenu, id, label, image, defFont ), _props(props), _propName(Utf8ToUnicode(lString8(propName))), _valueFont(valueFont), _topItem(0), _pageItems(pageItems),
          _cmdToHighlight(-1)
        { _fullscreen = false; _helpHeight=0; }
        CRMenu( CRGUIWindowManager * wm, CRMenu * parentMenu, int id, const char * label, LVImageSourceRef image, LVFontRef defFont, LVFontRef valueFont, CRPropRef props=CRPropRef(), const char * propName=NULL, int pageItems=8 )
        : CRGUIWindowBase( wm ), CRMenuItem( parentMenu, id, label, image, defFont ), _props(props), _propName(Utf8ToUnicode(lString8(propName))), _valueFont(valueFont), _topItem(0), _pageItems(pageItems),
        _cmdToHighlight(-1)
        { _fullscreen = false; _helpHeight=0; }
        virtual bool isSubmenu() const { return true; }
        LVPtrVector<CRMenuItem> & getItems() { return _items; }
        CRPropRef getProps() const { return _props; }
        lString16 getPropName() const { return _propName; }
        LVFontRef getValueFont() const { return _valueFont; }
        void setValueFont( LVFontRef font ) { _valueFont = font; }
        void addItem( CRMenuItem * item ) { _items.add( item ); }
        CRMenuItem * findItem( int id ) {
            for ( int i=0; i<_items.length(); i++ )
                if ( _items[i]->getId()==id )
                    return _items[i];
            return NULL;
        }
        CRMenu * findSubmenu( int id ) {
            for ( int i=0; i<_items.length(); i++ )
                if ( _items[i]->getId()==id && _items[i]->isSubmenu() )
                    return (CRMenu*)_items[i];
            return NULL;
        }
        /// called on system configuration change: screen size and orientation
        virtual void reconfigure( int flags );
        virtual int getPageCount();
        virtual void setCurPage( int nPage );
        virtual int getCurPage( );
        virtual int getTopItem();
        virtual lString16 getSubmenuValue();
        virtual void toggleSubmenuValue();
        virtual int getItemHeight();
        virtual lvPoint getMaxItemSize();
        virtual lvPoint getItemSize();
        virtual lvPoint getSize();
        virtual ~CRMenu() { }
        // CRGUIWindow
        virtual const lvRect & getRect();
        /// overriden to disable passing key to parent windows
        virtual bool onKeyPressed( int key, int flags );
        /// returns true if command is processed
        virtual bool onCommand( int command, int params = 0 );
        /// closes menu and its submenus, posts command
        virtual void closeMenu( int command, int params = 0 );
        /// closes top level menu and its submenus, posts command
        virtual void closeAllMenu( int command, int params = 0 );
        /// closes menu and its submenus
        virtual void destroyMenu();
};

class CRGUIUpdateEvent : public CRGUIEvent
{
public:
    CRGUIUpdateEvent( bool fullScreen=false )
    : CRGUIEvent( CREV_UPDATE )
    {
        _param1 = fullScreen ? 1 : 0;
    }
    virtual bool handle( CRGUIWindow * window )
    {
        return false;
    }
    virtual bool handle( CRGUIWindowManager * wm )
    {
        wm->update( _param1!=0, false );
        return true;
    }
};

class CRGUIResizeEvent : public CRGUIEvent
{
    cr_rotate_angle_t _angle;
public:
    CRGUIResizeEvent( int dx, int dy, cr_rotate_angle_t angle )
    : CRGUIEvent( CREV_RESIZE )
    {
        _param1 = dx;
        _param2 = dy;
        _angle = angle;
    }
    virtual bool handle( CRGUIWindow * window )
    {
        return false;
    }
    virtual bool handle( CRGUIWindowManager * wm )
    {
        wm->reconfigure(_param1, _param2, _angle);
        return true;
    }
};

class CRGUIKeyDownEvent : public CRGUIEvent
{
public:
    virtual bool isForVisibleOnly() { return true; }
    virtual bool isForModalOnly() { return true; }
    CRGUIKeyDownEvent( int key, int params )
    : CRGUIEvent( CREV_KEYDOWN )
    {
        _param1 = key;
        _param2 = params;
    }
    virtual bool handle( CRGUIWindow * window );
    virtual bool handle( CRGUIWindowManager * wm ) { return false; }
};

class CRGUICommandEvent : public CRGUIEvent
{
public:
    CRGUICommandEvent( int cmd, int params )
    : CRGUIEvent( CREV_COMMAND )
    {
        _param1 = cmd;
        _param2 = params;
    }
    virtual bool handle( CRGUIWindow * window );
    virtual bool handle( CRGUIWindowManager * wm ) { return false; }
    virtual bool isForModalOnly() { return false; }
    virtual bool isForVisibleOnly() { return true; }
};


#endif// CR_GUI_INCLUDED
