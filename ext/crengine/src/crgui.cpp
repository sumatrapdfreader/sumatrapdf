/*******************************************************

   CoolReader Engine

   lvxml.cpp:  XML parser implementation

   (c) Vadim Lopatin, 2000-2006
   This source code is distributed under the terms of   GNU General Public License
   See LICENSE file for details

*******************************************************/

#include <stdlib.h>
//include <unistd.h>      /* pause() */
#include "../include/crgui.h"
#include "../include/crtrace.h"

//TODO: place to skin file
#define ITEM_MARGIN 8
#define HOTKEY_SIZE 36
#define MENU_NUMBER_FONT_SIZE 24
#define SCROLL_HEIGHT 34
#define DEF_FONT_SIZE 22
#define DEF_TITLE_FONT_SIZE 28

// if 1, full page (e.g. 8 items) is scrolled even if on next page would be less items (show empty space)
#define FULL_SCROLL 1

const char * cr_default_skin =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<CR3Skin>\n"
"  <menu id=\"main\">\n"
"        <text color=\"#000000\" face=\"Arial\" size=\"25\" bold=\"true\" italic=\"false\" valign=\"center\" halign=\"center\"/>\n"
"        <background color=\"#AAAAAA\"/>\n"
"        <border widths=\"0,8,8,8\"/>\n"
"        <!--icon image=\"filename\" valign=\"\" halign=\"\"/-->\n"
"        <title>\n"
"            <size minvalue=\"32,0\" maxvalue=\"0,0\"/>\n"
"            <text color=\"#000000\" face=\"Arial\" size=\"25\" bold=\"true\" italic=\"false\" valign=\"center\" halign=\"center\"/>\n"
"            <background color=\"#AAAAAA\"/>\n"
"            <border widths=\"4,4,4,4\"/>\n"
"            <!--icon image=\"filename\" valign=\"\" halign=\"\"-->\n"
"        </title>\n"
"        <item>\n"
"            <size minvalue=\"48,48\" maxvalue=\"0,0\"/>\n"
"            <text color=\"#000000\" face=\"Arial\" size=\"24\" bold=\"false\" italic=\"false\" valign=\"center\" halign=\"left\"/>\n"
"            <background image=\"std_menu_item_background.xpm\" color=\"#FFFFFF\"/>\n"
"            <border widths=\"6,6,6,6\"/>\n"
"            <!--icon image=\"filename\" valign=\"\" halign=\"\"-->\n"
"        </item>\n"
"        <shortcut>\n"
"            <size minvalue=\"48,48\" maxvalue=\"0,0\"/>\n"
"            <text color=\"#000000\" face=\"Arial\" size=\"24\" bold=\"false\" italic=\"false\" valign=\"center\" halign=\"center\"/>\n"
"            <background image=\"std_menu_shortcut_background.xpm\" color=\"#FFFFFF\"/>\n"
"            <border widths=\"6,6,6,6\"/>\n"
"            <!--icon image=\"filename\" valign=\"\" halign=\"\"-->\n"
"        </shortcut>\n"
"  </menu>\n"
"  <menu id=\"settings\">\n"
"        <text color=\"#000000\" face=\"Arial\" size=\"25\" bold=\"true\" italic=\"false\" valign=\"center\" halign=\"center\"/>\n"
"        <background color=\"#AAAAAA\"/>\n"
"        <border widths=\"8,8,8,8\"/>\n"
"        <!--icon image=\"filename\" valign=\"\" halign=\"\"/-->\n"
"        <title>\n"
"            <size minvalue=\"0,40\" maxvalue=\"0,0\"/>\n"
"            <text color=\"#000000\" face=\"Arial\" size=\"28\" bold=\"true\" italic=\"false\" valign=\"center\" halign=\"center\"/>\n"
"            <background color=\"#AAAAAA\"/>\n"
"            <border widths=\"4,4,4,4\"/>\n"
"            <!--icon image=\"filename\" valign=\"\" halign=\"\"-->\n"
"        </title>\n"
"        <item>\n"
"            <size minvalue=\"48,48\" maxvalue=\"0,0\"/>\n"
"            <text color=\"#000000\" face=\"Arial\" size=\"24\" bold=\"false\" italic=\"false\" valign=\"center\" halign=\"left\"/>\n"
"            <background image=\"std_menu_item_background.xpm\" color=\"#FFFFFF\"/>\n"
"            <border widths=\"6,6,6,6\"/>\n"
"            <!--icon image=\"filename\" valign=\"\" halign=\"\"-->\n"
"        </item>\n"
"        <shortcut>\n"
"            <size minvalue=\"48,48\" maxvalue=\"0,0\"/>\n"
"            <text color=\"#000000\" face=\"Arial\" size=\"24\" bold=\"false\" italic=\"false\" valign=\"center\" halign=\"center\"/>\n"
"            <background image=\"std_menu_shortcut_background.xpm\" color=\"#FFFFFF\"/>\n"
"            <border widths=\"6,6,6,6\"/>\n"
"            <!--icon image=\"filename\" valign=\"\" halign=\"\"-->\n"
"        </shortcut>\n"
"  </menu>\n"
"</CR3Skin>\n";

bool CRGUIWindowManager::loadSkin( lString16 pathname )
{
    CRSkinRef skin;
    if ( !pathname.empty() )
        skin = LVOpenSkin( pathname );
    if ( skin.isNull() ) {
        skin = LVOpenSimpleSkin( lString8( cr_default_skin ) );
        setSkin( skin );
        return false;
    }
    setSkin( skin );
    return true;
}


/// add all items from another table
void CRGUIAcceleratorTable::addAll( const CRGUIAcceleratorTable & v )
{
	for ( int i=0; i<v._items.length(); i++ ) {
		CRGUIAccelerator * item = v._items.get( i );
		add( item->keyCode, item->keyFlags, item->commandId, item->commandParam );
	}
}

/// add accelerator to table or change existing
bool CRGUIAcceleratorTable::add( int keyCode, int keyFlags, int commandId, int commandParam )
{
    int index = indexOf( keyCode, keyFlags );
    if ( index >= 0 ) {
        // just update
        CRGUIAccelerator * item = _items[index];
        item->commandId = commandId;
        item->commandParam = commandParam;
        return false;
    }
    CRGUIAccelerator * item = new CRGUIAccelerator();
    item->keyCode = keyCode;
    item->keyFlags = keyFlags;
    item->commandId = commandId;
    item->commandParam = commandParam;
    _items.add(item);
    return true;
}

CRGUIAcceleratorTableRef CRGUIAcceleratorTableList::get( const lString16 & name, CRPropRef keyRemappingOptions )
{
    CRGUIAcceleratorTableRef prev = get(name);
    if ( !prev )
        return prev;
    CRPropRef keymaps = keyRemappingOptions->getSubProps(LCSTR(lString16("keymap.") + name + L"."));
    if ( keymaps.isNull() || keymaps->getCount()==0 )
        return prev;
    CRGUIAcceleratorTableRef acc( new CRGUIAcceleratorTable( *prev ));
    for ( int i=0; i<keymaps->getCount(); i++ ) {
        lString16 name( keymaps->getName(i) );
        lString16 value = keymaps->getValue(i);
//        CRLog::trace("Override key map: %s -> %s", LCSTR(name), LCSTR(value) );
        int key, flags;
        int cmd, params;
        if ( !splitIntegerList( name, lString16("."), key, flags ))
            continue;
        if ( !splitIntegerList( value, lString16(","), cmd, params ))
            continue;
        acc->add(key, flags, cmd, params);
    }
    return acc;
}

/// add all tables
void CRGUIAcceleratorTableList::addAll( const CRGUIAcceleratorTableList & v )
{
	LVHashTable<lString16, CRGUIAcceleratorTableRef>::iterator i( v._table );
	for ( ;; ) {
		LVHashTable<lString16, CRGUIAcceleratorTableRef>::pair * p = i.next();
		if ( !p )
			break;
		CRGUIAcceleratorTableRef t = _table.get( p->key );
		if ( t.isNull() ) {
			t = CRGUIAcceleratorTableRef( new CRGUIAcceleratorTable() );
			_table.set( p->key, t );
		}
		crtrace trace;
		trace << "Merging accelerators for '"  << p->key << "'";
		t->addAll( *p->value );
	}
}

/// returns true if key is processed
bool CRGUIWindowManager::onKeyPressed( int key, int flags )
{
    // orientation related key substitution
    const char * orientation_mapping_name = NULL;
    if ( _orientation==CR_ROTATE_ANGLE_90 )
        orientation_mapping_name = "key-substitution-90";
    else if ( _orientation==CR_ROTATE_ANGLE_180 )
        orientation_mapping_name = "key-substitution-180";
    else if ( _orientation==CR_ROTATE_ANGLE_270 )
        orientation_mapping_name = "key-substitution-270";
    if ( orientation_mapping_name ) {
        CRGUIAcceleratorTableRef subst = _accTables.get(orientation_mapping_name);
        if ( !subst.isNull() ) {
            const CRGUIAccelerator * acc = subst->findKeyAccelerator(key, flags);
            if ( acc ) {
                CRLog::debug("Translation using %s : %d, %d -> %d, %d", orientation_mapping_name,
                             acc->keyCode, acc->keyFlags, acc->commandId, acc->commandParam);
                key = acc->commandId;
                flags = acc->commandParam;
            }
        }
    }
    CRLog::trace("CRGUIWindowManager::onKeyPressed( %d, %d)", key, flags );
    for ( int i=_windows.length()-1; i>=0; i-- ) {
        if ( _windows[i]->isVisible() ) {
            if ( _windows[i]->onKeyPressed( key, flags ) ) {
                CRLog::trace("CRGUIWindowManager::onKeyPressed() -- window %d has processed key, exiting", i );
                return true;
            } else {
                CRLog::trace("CRGUIWindowManager::onKeyPressed() -- window %d cannot process key, continue", i );
            }
        } else {
            CRLog::trace("CRGUIWindowManager::onKeyPressed() -- window %d is invisible, continue", i );
        }
    }
    return false;
}

/// changes screen size and orientation
void CRGUIWindowManager::reconfigure( int dx, int dy, cr_rotate_angle_t orientation )
{
    CRLog::info("CRGuiWindowManager::reconfigure( dx=%d, dy=%d, angle=%d", dx, dy, (int)orientation);
    int flags = 0;
    lvRect fullRect = _screen->getRect();
    if ( fullRect.width()!=dx || fullRect.height()!=dy )
        flags |= CRGUI_CONFIGURE_FLAG_SCREEN_SIZE;
    if ( orientation!=_orientation ) {
        flags |= CRGUI_CONFIGURE_FLAG_SCREEN_ORIENTATION;
        _orientation = orientation;
    }
    if ( !flags )
        return;
    if ( _screen->setSize( dx, dy ) ) {
        fullRect = _screen->getRect();
        for ( int i=_windows.length()-1; i>=0; i-- ) {
            _windows[i]->reconfigure( flags );
        }
        postEvent( new CRGUIUpdateEvent(true) );
    }
}

/// redraw one window
void CRGUIWindowManager::updateWindow( CRGUIWindow * window )
{
    int index = _windows.indexOf( window );
    if ( index < 0 )
        return;
    lvRect coverBox;
    if  ( _windows.empty() )
        return;
    LVPtrVector<CRGUIWindow, false> drawList;
    for ( int i=_windows.length()-1; i>=index; i-- ) {
        if ( !_windows[i]->isVisible() )
            continue;
        lvRect rc = _windows[i]->getRect();
        if ( coverBox.isRectInside( rc ) )
            continue; // fully covered by top window
        if ( !rc.isEmpty() )
            drawList.add( _windows[i] );
        if ( !rc.isRectInside( coverBox ) )
            coverBox = rc;
    }
    while ( !drawList.empty()  ) {
        CRGUIWindow * w = drawList.pop();
        if ( w->isDirty() ) {
            if ( w->isVisible() )
                w->flush();
            _screen->invalidateRect( w->getRect() );
        }
    }
/// invalidates rectangle: add it to bounding box of next partial update
    _screen->flush( false );
}

void CRGUIWindowManager::update( bool fullScreenUpdate, bool forceFlushScreen )
{
    lvRect coverBox;
    if  ( _windows.empty() )
        return;
    LVPtrVector<CRGUIWindow, false> drawList;
    for ( int i=_windows.length()-1; i>=0; i-- ) {
        if ( !_windows[i]->isVisible() )
            continue;
        lvRect rc = _windows[i]->getRect();
        if ( coverBox.isRectInside( rc ) )
            continue; // fully covered by top window
        if ( !rc.isEmpty() )
            drawList.add( _windows[i] );
        if ( !rc.isRectInside( coverBox ) )
            coverBox = rc;
    }
    while ( !drawList.empty()  ) {
        CRGUIWindow * w = drawList.pop();
        if ( w->isDirty() || fullScreenUpdate ) {
            if ( w->isVisible() )
                w->flush();
            _screen->invalidateRect( w->getRect() );
        }
    }
    _lastProgressPercent = -1;
    if ( !forceFlushScreen ) {
#if 0
        // simulate slow rendering
        sleep(1);
#endif
        forwardSystemEvents(false);
        if ( !_events.empty() ) {
            // postpone screen update
            postEvent( new CRGUIUpdateEvent(fullScreenUpdate) );
            return;
        }
    }
    _screen->flush( fullScreenUpdate );
}

/// closes window, removes from stack, destroys object
void CRGUIWindowManager::closeWindow( CRGUIWindow * window )
{
    int index = _windows.indexOf( window );
    if ( index >= 0 ) {
        if ( window == _windows.peek() )
            window->covered(); // send cover before close
        _windows.remove( index );
    }
    window->closing();
    delete window;
    for ( int i=0; i<_windows.length() && (index<0 || i<index); i++ )
        _windows[i]->setDirty();
    CRGUIWindow * gotFocus = getTopVisibleWindow();
    if (gotFocus)
        gotFocus->reactivated();
    fontMan->gc();
}

/// activates window, brings it on top; add to stack if not added
void CRGUIWindowManager::activateWindow( CRGUIWindow * window )
{
    int index = _windows.indexOf( window );
    CRGUIWindow * lostFocus = getTopVisibleWindow();
    window->setVisible( true );
    if ( index < 0 ) {
        _windows.push( window );
    } else if ( index < _windows.length() - 1 ) {
        _windows.push( _windows.remove( index ) );
    }
    if ( window != lostFocus )
    {
        if ( lostFocus )
            lostFocus->covered();
        window->activated();
    }
}

/// runs event loop
int CRGUIWindowManager::runEventLoop()
{
    while ( !_stopFlag ) {
        if ( !getWindowCount() )
            _stopFlag = true;
        handleAllEvents(!_stopFlag);
    }
    return 0;
}


/// handle all events from queue
bool CRGUIWindowManager::handleAllEvents( bool waitForEvent )
{
    bool handled = false;
    if ( _events.empty() && waitForEvent ) {
        idle();
        forwardSystemEvents( true );
    }
    for (CRGUIEvent * event=getEvent(); event; event=getEvent() ) {
        handleEvent( event );
        delete event;
        handled = true;
    }
    return handled;
}

void CRGUIWindowManager::postCommand( int command, int params )
{
    postEvent(new CRGUICommandEvent(command, params) );
}

void CRGUIWindowManager::postEvent( CRGUIEvent * event )
{
    int evt = event->getType();
    if ( evt==CREV_KEYDOWN || evt==CREV_KEYUP || evt==CREV_COMMAND ) {
        // for window events, like keyPress or Command, post them before Update/Resize
        int i=_events.length()-1;
        for ( ; i>=0; i-- ) {
            int t = _events[i]->getType();
            if ( t!=CREV_UPDATE && t!=CREV_RESIZE )
                break;
        }
        _events.insert(i+1, event);
        return;
    } else if ( evt==CREV_UPDATE || evt==CREV_RESIZE ) {
        for ( int i=_events.length()-1; i>=0; i-- ) {
            int t = _events[i]->getType();
            if ( t==evt || t==CREV_UPDATE ) { // UPDATE is invalidated by Resize
                if ( t==CREV_UPDATE && evt==CREV_UPDATE && _events[i]->getParam1()!=0 )
                    event->setParam1(1); // don't miss fullScreen flag from removed events
                // remove duplicates
                delete _events.remove(i);
            }
        }
        _events.push(event);
        return;
    }
    _events.push(event);
}

/// override to handle
bool CRGUIWindowManager::handleEvent( CRGUIEvent * event )
{
    // by default, allow event to do something with window
    if ( event->isWMEvent() )
        return event->handle( this );
    if ( event->isWindowEvent() ) {
        CRLog::trace("CRGUIWindowManager::handleEvent( %d, %d, %d)", event->getType(), event->getParam1(), event->getParam2() );
        for ( int i=_windows.length()-1; i>=0; i-- ) {
            if ( !event->isForVisibleOnly() || _windows[i]->isVisible() ) {
                if ( _windows[i]->handleEvent(event) ) {
                    CRLog::trace("CRGUIWindowManager::handleEvent() -- window %d has processed event, exiting", i );
                    return true;
                } else {
                    CRLog::trace("CRGUIWindowManager::handleEvent() -- window %d cannot process event, continue", i );
                    if ( event->isForModalOnly() )
                        return false;
                }
            } else {
                CRLog::trace("CRGUIWindowManager::handleEvent() -- window %d is invisible, continue", i );
            }
        }
        return false;
    }
	return false;
}


static bool firstWaitUpdate = true;

/// draws icon at center of screen
void CRGUIWindowManager::showWaitIcon( lString16 filename, int progressPercent )
{
    LVImageSourceRef img = _skin->getImage( filename );
    if ( !img.isNull() ) {
        int dx = img->GetWidth();
        int dy = img->GetHeight();
        int x = (_screen->getWidth() - dx) / 2;
        int y = (_screen->getHeight() - dy) / 2;
        CRLog::debug("Drawing wait image %s %dx%d  progress=%d%%", UnicodeToUtf8(filename).c_str(), dx, dy, progressPercent );
        _screen->getCanvas()->Draw( img, x, y, dx, dy, true );
        int gaugeH = 0;
        if ( progressPercent>=0 && progressPercent<=100 ) {
            CRScrollSkinRef skin = _skin->getScrollSkin(L"#progress");
            if ( !skin.isNull() ) {
                CRLog::trace("Drawing gauge %d%%", progressPercent);
                gaugeH = 16;
                lvRect gaugeRect( x, y+dy, x+dx, y+dy+gaugeH );
                skin->drawGauge( *(_screen->getCanvas()), gaugeRect, progressPercent );
            }
        }
        _screen->invalidateRect( lvRect(x, y, x+dx, y+dy+gaugeH) );
        _screen->flush(firstWaitUpdate);
        firstWaitUpdate = false;
    } else {
        CRLog::error("CRGUIWindowManager::showWaitIcon(%s): image not found in current skin", UnicodeToUtf8(filename).c_str() );
    }
}

#define PROGRESS_UPDATE_INTERVAL 5
/// draws icon at center of screen, with optional progress gauge
void CRGUIWindowManager::showProgress( lString16 filename, int progressPercent )
{
    time_t t = (time_t)time((time_t)0);
    if ( t<_lastProgressUpdate+PROGRESS_UPDATE_INTERVAL || progressPercent==_lastProgressPercent )
        return;
    showWaitIcon( filename, progressPercent );
    _lastProgressUpdate = t;
    _lastProgressPercent = progressPercent;
}

void CRGUIScreenBase::flush( bool full )
{
    if ( _updateRect.isEmpty() && !full && !getTurboUpdateEnabled() ) {
        CRLog::trace("CRGUIScreenBase::flush() - update rectangle is empty");
        return;
    }
    if ( !_front.isNull() && !_updateRect.isEmpty() && !full ) {
        // calculate really changed area
        lvRect rc;
        lvRect lineRect(_updateRect);
        int sz = _canvas->GetRowSize();
        for ( int y = _updateRect.top; y < _updateRect.bottom; y++ ) {
            if ( y>=0 && y<_height ) {
                void * line1 = _canvas->GetScanLine( y );
                void * line2 = _front->GetScanLine( y );
                if (memcmp( line1, line2, sz ) != 0) {
                    // line content is different
                    lineRect.top = y;
                    lineRect.bottom = y+1;
                    rc.extend( lineRect );
                    // copy line to front buffer
                    memcpy( line2, line1, sz );
                }
            }
        }
        if ( rc.isEmpty() ) {
            // no actual changes
            _updateRect.clear();
            return;
        }
        _updateRect.top = rc.top;
        _updateRect.bottom = rc.bottom;
    }
    //if ( !full && !checkFullUpdateCounter() )
    //    full = false;
    if ( full && !_front.isNull() ) {
        // copy full screen to front buffer
        _canvas->DrawTo( _front.get(), 0, 0, 0, NULL );
    }
    if ( full )
        _updateRect = getRect();
    update( _updateRect, full );
    _updateRect.clear();
}

/// calculates title rectangle for specified window rectangle
bool CRGUIWindowBase::getTitleRect( lvRect & rc )
{
    rc = _rect;
    if ( _skinName.empty() ) {
        rc.bottom = rc.top;
        return false;
    }
    CRWindowSkinRef skin( _wm->getSkin()->getWindowSkin(_skinName.c_str()) );
    rc.shrinkBy(skin->getBorderWidths());
    rc.bottom = rc.top;
    CRRectSkinRef clientSkin = skin->getClientSkin();
    CRRectSkinRef titleSkin = skin->getTitleSkin();
    CRRectSkinRef statusSkin = skin->getStatusSkin();
    CRScrollSkinRef sskin = skin->getScrollSkin();
    if ( !titleSkin.isNull() ) {
        rc.bottom += titleSkin->getMinSize().y;
    }
    return !rc.isEmpty();
}

/// calculates status rectangle for specified window rectangle
bool CRGUIWindowBase::getStatusRect( lvRect & rc )
{
    rc = _rect;
    if ( _skinName.empty() ) {
        rc.bottom = rc.top;
        return false;
    }
    CRWindowSkinRef skin( _wm->getSkin()->getWindowSkin(_skinName.c_str()) );
    rc.shrinkBy(skin->getBorderWidths());
    rc.top = rc.bottom;
    lvPoint scrollSize = getMinScrollSize( _page, _pages );
    int h = scrollSize.y;
    CRRectSkinRef statusSkin = skin->getStatusSkin();
    CRScrollSkinRef sskin = skin->getScrollSkin();
    bool scroll = ( !sskin.isNull() && sskin->getLocation()==CRScrollSkin::Status );
    if ( !statusSkin.isNull() ) {
        rc.top -= statusSkin->getMinSize().y;
    }
    if ( scroll && rc.height()<h ) {
        rc.top = rc.bottom - h;
    }
    return !rc.isEmpty();
}

/// calculates client rectangle for specified window rectangle
bool CRGUIWindowBase::getClientRect( lvRect & rc )
{
    rc = _rect;
    if ( _skinName.empty() )
        return true;
    CRWindowSkinRef skin( _wm->getSkin()->getWindowSkin(_skinName.c_str()) );
    rc.shrinkBy(skin->getBorderWidths());
    rc.bottom = rc.top;
    lvRect titleRect;
    getTitleRect( titleRect );
    lvRect statusRect;
    getStatusRect( statusRect );
    rc.top = titleRect.bottom;
    rc.bottom = statusRect.top;
    return !rc.isEmpty();
}

/// formats scroll label (like "1 of 2")
lString16 CRGUIWindowBase::getScrollLabel( int page, int pages )
{
    return lString16::itoa(page) + L" of " + lString16::itoa(pages);
}

/// calculates minimum scroll size
lvPoint CRGUIWindowBase::getMinScrollSize( int page, int pages )
{
    lvPoint sz(0,0);
    //if ( pages<=1 )
    //    return sz; // can hide scrollbar
    CRWindowSkinRef skin( _wm->getSkin()->getWindowSkin(_skinName.c_str()) );
    CRRectSkinRef statusSkin = skin->getStatusSkin();
    CRScrollSkinRef sskin = skin->getScrollSkin();
    if ( !sskin.isNull() ) {
		LVFontRef sf = sskin->getFont();
        int h = sf.isNull() ? sskin->getFontSize() : sf->getHeight();
        int w = 0;
        bool noData = sskin->getAutohide() && pages<=1;
        lString16 label = getScrollLabel( page, pages );
        if ( !label.empty() )
            w = sskin->getFont()->getTextWidth(label.c_str(), label.length());
        if ( !sskin->getBottomTabSkin().isNull() ) {
            if ( h < sskin->getBottomTabSkin()->getMinSize().y )
                h = sskin->getBottomTabSkin()->getMinSize().y;
            if ( !noData && w < _rect.width()/4 )
                w = _rect.width()/4;
        }
        if ( !noData && h < sskin->getMinSize().y )
            h = sskin->getMinSize().y;
        if ( !sskin->getHBody().isNull() ) {
            if ( h < sskin->getHBody()->GetHeight() )
                h = sskin->getHBody()->GetHeight();
            if ( !noData && w < _rect.width()/4 )
                w = _rect.width()/4;
        }
        if ( h && w ) {
            sz.y = h;
            sz.x = w;
        }
    }
    return sz;
}

#define BATTERY_ICON_SIZE 60
/// calculates scroll rectangle for specified window rectangle
bool CRGUIWindowBase::getScrollRect( lvRect & rc )
{
    CRWindowSkinRef skin( _wm->getSkin()->getWindowSkin(_skinName.c_str()) );
    rc = _rect;
    rc.shrinkBy(skin->getBorderWidths());
    CRScrollSkinRef sskin = skin->getScrollSkin();
    if ( sskin.isNull() )
        return false;
    lvPoint scrollSize = getMinScrollSize( _page, _pages );
    int h = scrollSize.y;
    if ( sskin->getLocation()==CRScrollSkin::Title ) {
        rc.bottom = rc.top;
        CRRectSkinRef titleSkin = skin->getTitleSkin();
        if ( !titleSkin.isNull() ) {
            getTitleRect( rc );
            lvRect rc2;
            sskin->getRect( rc2, rc );
            if ( rc2.width() < rc.width()/2 )
                rc = rc2;
            else
                rc.left = rc.right - rc.width()/4;
            rc.left -= BATTERY_ICON_SIZE;
            rc.right -= BATTERY_ICON_SIZE;
        } else if ( !sskin.isNull() ) {
            rc.bottom += h;
        }
    } else {
        rc.top = rc.bottom;
        CRRectSkinRef statusSkin = skin->getStatusSkin();
        if ( !statusSkin.isNull() ) {
            if ( statusSkin->getMinSize().y > h )
                h = statusSkin->getMinSize().y;
        }
        rc.top -= h;
    }
    return !rc.isEmpty();
}

/// calculates input box rectangle for window rectangle
bool CRGUIWindowBase::getInputRect( lvRect & rc )
{
    CRWindowSkinRef skin( _wm->getSkin()->getWindowSkin(_skinName.c_str()) );
    rc = _rect;
    //rc.shrinkBy(skin->getBorderWidths());
    CRRectSkinRef inputSkin = skin->getInputSkin();
    CRRectSkinRef statusSkin = skin->getStatusSkin();
    if ( inputSkin.isNull() || statusSkin.isNull() )
        return false;
    lvRect rc2;
    if ( !getStatusRect( rc2 ) )
        return false;
    inputSkin->getRect( rc, rc2 );
    return !rc.isEmpty();
}

/// draw input box, if any
void CRGUIWindowBase::drawInputBox()
{
    LVDrawBuf & buf = *_wm->getScreen()->getCanvas();
    CRWindowSkinRef skin( _wm->getSkin()->getWindowSkin(_skinName.c_str()) );
    CRRectSkinRef inputSkin = skin->getInputSkin();
    CRRectSkinRef statusSkin = skin->getStatusSkin();
    if ( inputSkin.isNull() || statusSkin.isNull() )
        return;
    lvRect rc2;
    if ( !getInputRect( rc2 ) )
        return;
    inputSkin->draw( buf, rc2 );
    if ( !_inputText.empty() )
        inputSkin->drawText(buf, rc2, _inputText );
}

/// draw status text
void CRGUIWindowBase::drawStatusText( LVDrawBuf & buf, const lvRect & rc, CRRectSkinRef skin )
{
    lvRect statusRc( rc );
    lvRect b = skin->getBorderWidths();
    statusRc.shrinkBy( b );
    if ( statusRc.width() > 100 ) {
        // draw status text
        skin->drawText( buf, statusRc, getStatusText() );
    }
}

/// draw status bar using current skin, with optional status text and scroll/tab/page indicator
void CRGUIWindowBase::drawStatusBar()
{
    LVDrawBuf & buf = *_wm->getScreen()->getCanvas();
    CRWindowSkinRef skin( _wm->getSkin()->getWindowSkin(_skinName.c_str()) );
    CRRectSkinRef statusSkin = skin->getStatusSkin();
    CRScrollSkinRef sskin = skin->getScrollSkin();
    lvRect statusRc;
    lvRect scrollRc;
    lvRect inputRc;
    if ( !getStatusRect( statusRc ) )
        return;
    getScrollRect( scrollRc );
    bool showInput = getInputRect( inputRc );
    if ( !statusSkin.isNull() ) {
        statusSkin->draw( buf, statusRc );
    }
    bool scroll = ( !sskin.isNull() && sskin->getLocation()==CRScrollSkin::Status && !scrollRc.isEmpty() );
    if ( scroll ) {
        sskin->drawScroll( buf, scrollRc, false, _page-1, _pages, 1 );
    }
    if ( !statusSkin.isNull() && !statusRc.isEmpty() && !getStatusText().empty() ) {
        if ( scroll ) {
            if ( scrollRc.left - statusRc.left > statusRc.right - scrollRc.right )
                statusRc.right = scrollRc.left;
            else
                statusRc.left = scrollRc.right;
        }
        if ( showInput ) {
            if ( inputRc.left - statusRc.left > statusRc.right - inputRc.right )
                statusRc.right = inputRc.left;
            else
                statusRc.left = inputRc.right;
        }
        drawStatusText( buf, statusRc, statusSkin );
    }
    drawInputBox();
}

// draws frame, title, status and client
void CRGUIWindowBase::draw()
{
    CRLog::trace("enter CRGUIWindowBase::draw()");
    LVDrawBuf & buf = *_wm->getScreen()->getCanvas();
    CRLog::trace("getting skin at CRGUIWindowBase::draw()");
    CRWindowSkinRef skin( _wm->getSkin()->getWindowSkin(_skinName.c_str()) );
    CRLog::trace("drawing window skin background at CRGUIWindowBase::draw()");
    skin->draw( buf, _rect );
    CRLog::trace("start drawing at CRGUIWindowBase::draw()");
    drawTitleBar();
    drawStatusBar();
    drawClient();
    CRLog::trace("exit CRGUIWindowBase::draw()");
}

/// draw title bar using current skin, with optional scroll/tab/page indicator
void CRGUIWindowBase::drawClient()
{
    LVDrawBuf & buf = *_wm->getScreen()->getCanvas();
    CRWindowSkinRef skin( _wm->getSkin()->getWindowSkin(_skinName.c_str()) );
    CRRectSkinRef clientSkin = skin->getClientSkin();
    if ( clientSkin.isNull() )
        return;
    lvRect rc;
    if ( !getClientRect( rc ) )
        return;
    clientSkin->draw( buf, rc );
}

/// draw title bar using current skin, with optional scroll/tab/page indicator
void CRGUIWindowBase::drawTitleBar()
{
    LVDrawBuf & buf = *_wm->getScreen()->getCanvas();
    CRWindowSkinRef skin( _wm->getSkin()->getWindowSkin(_skinName.c_str()) );
    CRRectSkinRef titleSkin = skin->getTitleSkin();
    lvRect titleRc;
    if ( !getTitleRect( titleRc ) )
        return;
    titleSkin->draw( buf, titleRc );

    lvRect batteryRc( titleRc );
    batteryRc.left = batteryRc.right - BATTERY_ICON_SIZE;
    batteryRc.shrinkBy( titleSkin->getBorderWidths() );
    _wm->drawBattery( buf, batteryRc );

    CRScrollSkinRef sskin = skin->getScrollSkin();
    lvRect scrollRc;
    getScrollRect( scrollRc );
    bool scroll = ( !sskin.isNull() && sskin->getLocation()==CRScrollSkin::Title && !scrollRc.isEmpty() );
    if ( scroll ) {
        sskin->drawScroll( buf, scrollRc, false, _page-1, _pages, 1 );
        titleRc.right = scrollRc.left;
    }

    //lvRect b = titleSkin->getBorderWidths();
    buf.SetTextColor( skin->getTextColor() );
    buf.SetBackgroundColor( skin->getBackgroundColor() );
    int imgWidth = 0;
    //titleRc.shrinkBy( b );
    int hh = titleRc.bottom - titleRc.top;
    if ( !_icon.isNull() ) {
        int w = _icon->GetWidth();
        int h = _icon->GetHeight();
        buf.Draw( _icon, titleRc.left + hh/2-w/2, titleRc.top + hh/2 - h/2, w, h );
        imgWidth = w + ITEM_MARGIN;
    }
    lvRect textRect = titleRc;
    textRect.left += imgWidth;
    titleSkin->drawText( buf, textRect, _caption );
}

/// called on system configuration change: screen size and orientation
void CRGUIWindowBase::reconfigure( int flags )
{
    lvRect fs = _wm->getScreen()->getRect();
    if ( _fullscreen ) {
        setRect( fs );
    } else {
        lvRect rc = getRect();
        int dx = fs.width();
        //int dy = fs.height();
        if ( rc.right > dx ) {
            rc.left -= rc.right - dx;
            rc.right = dx;
            if ( rc.left < 0 )
                rc.left = 0;
        }
        if ( rc.right > dx ) {
            rc.left -= rc.right - dx;
            rc.right = dx;
            if ( rc.left < 0 )
                rc.left = 0;
        }
        setRect( rc );
    }
    setDirty();
}

/// returns true if key is processed (by default, let's translate key to command using accelerator table)
bool CRGUIWindowBase::onKeyPressed( int key, int flags )
{
    if ( _acceleratorTable.isNull() ) {
        CRLog::trace("CRGUIWindowBase::onKeyPressed( %d, %d) - no accelerator table specified!", key, flags );
        return !_passKeysToParent;
    }
    int cmd, param;
    if ( _acceleratorTable->translate( key, flags, cmd, param ) ) {
        CRLog::trace("Accelerator applied: key %d(%d) -> command(%d,%d)", key, flags, cmd, param );
		if ( cmd == GCMD_PASS_TO_PARENT ) {
			return false;
		}
        CRGUIEvent * event = new CRGUICommandEvent( cmd, param );
        event->setTargetWindow(this);
        _wm->postEvent( event );
        return true;
        //return onCommand( cmd, param );
    } else {
        CRLog::trace("Accelerator not found for key %d(%d)", key, flags );
        _acceleratorTable->dump();
    }
    return !_passKeysToParent;
}

void CRDocViewWindow::draw()
{
    lvRect clientRect = _rect;
    if ( !_skin.isNull() ) {
        if ( getClientRect( clientRect ) ) {
            _skin->draw( *_wm->getScreen()->getCanvas(), _rect );
            drawTitleBar();
            drawStatusBar();
        }
    }
    LVDocImageRef pageImage = _docview->getPageImage(0);
    LVDrawBuf * drawbuf = pageImage->getDrawBuf();
    _wm->getScreen()->draw( drawbuf, clientRect.left, clientRect.top );
}

void CRDocViewWindow::setRect( const lvRect & rc )
{
    if ( rc == _rect )
        return;
    _rect = rc;
    lvRect clientRect = _rect;
    if ( !_skin.isNull() )
        clientRect = _skin->getClientRect( rc );
    _docview->Resize( clientRect.width(), clientRect.height() );
    setDirty();
}


void CRMenuItem::Draw( LVDrawBuf & buf, lvRect & rc, CRRectSkinRef skin, CRRectSkinRef valueSkin, bool selected )
{
    _itemDirty = false;
    lvRect itemBorders = skin->getBorderWidths();
    skin->draw( buf, rc );
    buf.SetTextColor( skin->getTextColor() );
    buf.SetBackgroundColor( skin->getBackgroundColor() );
    int imgWidth = 0;
    int hh = rc.bottom - rc.top - itemBorders.top - itemBorders.bottom;
    if ( !_image.isNull() ) {
        int w = _image->GetWidth();
        int h = _image->GetHeight();
        buf.Draw( _image, rc.left + hh/2-w/2 + itemBorders.left, rc.top + hh/2 - h/2 + itemBorders.top, w, h );
        imgWidth = w + ITEM_MARGIN;
    }

    lvRect textRect = rc;
    textRect.left += imgWidth;

    lString16 s1;
    lString16 s2;
    lvRect valueRect = textRect;
    if ( _label.split2(lString16("\t"), s1, s2 ) ) {
        //valueSkin->drawText( buf, textRect, s2 );
    } else {
        s1 = _label;
    }

    LVFontRef font = getFont();
    if ( font.isNull() )
        font = skin->getFont();
    if ( s2.empty() ) {
        textRect.top += (textRect.height() - font->getHeight() - itemBorders.top - itemBorders.bottom) / 2;
        textRect.bottom = textRect.top + font->getHeight() + itemBorders.top + itemBorders.bottom;
    }
    skin->drawText( buf, textRect, s1, font );
    if ( !s2.empty() ) {
        valueSkin->drawText( buf, valueRect, s2 );
    }
}

int CRMenu::getPageCount()
{
    return (_items.length() + _pageItems - 1) / _pageItems;
}

bool CRMenu::setCurPage( int nPage )
{
    int oldTop = _topItem;
    _topItem = _pageItems * nPage;
#if FULL_SCROLL==1
    if ( _topItem >= (int)_items.length() )
        _topItem = ((int)_items.length() - 1) / _pageItems * _pageItems;
#else
    if ( _topItem + _pageItems >= (int)_items.length() )
        _topItem = (int)_items.length() - _pageItems;
#endif
    if ( _topItem < 0 )
        _topItem = 0;
    if ( _topItem != oldTop ) {
		_pageUpdate = true;
        setDirty();
        return true;
	}
	return false;
}

int CRMenu::getCurPage( )
{
    return (_topItem + (_pageItems-1)) / _pageItems;
}

int CRMenu::getTopItem()
{
    return _topItem;
}

void CRMenu::Draw( LVDrawBuf & buf, lvRect & rc, CRRectSkinRef skin, CRRectSkinRef valueSkin, bool selected )
{
    CRLog::trace("enter CRMenu::Draw()");
    CRMenuSkinRef menuSkin = _skin; //getSkin();
    //CRRectSkinRef valueSkin = menuSkin->getValueSkin();

    lvRect rc2 = rc;

    lvRect itemBorders = skin->getBorderWidths();
    skin->draw( buf, rc );
    buf.SetTextColor( skin->getTextColor() );
    buf.SetBackgroundColor( skin->getBackgroundColor() );
    int imgWidth = 0;
    int hh = rc.bottom - rc.top - itemBorders.top - itemBorders.bottom;
    if ( !_image.isNull() ) {
        int w = _image->GetWidth();
        int h = _image->GetHeight();
        buf.Draw( _image, rc.left + hh/2-w/2 + itemBorders.left, rc.top + hh/2 - h/2 + itemBorders.top, w, h );
        imgWidth = w + ITEM_MARGIN;
    }
    lvRect textRect = rc;
    textRect.left += imgWidth;
    //textRect.shrinkBy( itemBorders );

    lString16 s = getSubmenuValue();
    lvRect valueRect = textRect;
    if ( !s.empty() ) {
        if ( valueSkin.isNull() ) {
            textRect.bottom -= textRect.height()*2/5;
        } else {
            valueSkin->drawText( buf, textRect, s );
        }
    } else {
		LVFontRef skinFont = skin->getFont();
		int fh = skinFont.isNull() ? skin->getFontSize() : skinFont->getHeight();
        textRect.top += (textRect.height() - fh - itemBorders.top - itemBorders.bottom) / 2;
        textRect.bottom = textRect.top + fh + itemBorders.top + itemBorders.bottom;
    }
    skin->drawText( buf, textRect, _label );
    if ( !s.empty() ) {
        if ( valueSkin.isNull() ) {
            // old implementation: no value skin
            int w = _valueFont->getTextWidth( s.c_str(), s.length() );
            rc2 = valueRect;
            rc2.top += rc2.height()*3/8;
            int hh = rc2.height();
            buf.SetTextColor( skin->getTextColor() );
            _valueFont->DrawTextString( &buf, rc2.right - w - ITEM_MARGIN, rc2.top + hh/2 - _valueFont->getHeight()/2, s.c_str(), s.length(), L'?', NULL, false, 0 );
        } else {
            valueSkin->drawText( buf, valueRect, s );
        }
    }
    _itemDirty = false;
    CRLog::trace("exit CRMenu::Draw()");
}

lvPoint CRMenuItem::getItemSize( CRRectSkinRef skin )
{
    LVFontRef font = _defFont;
    if ( font.isNull() )
        font = skin->getFont();
    lvRect borders = skin->getBorderWidths();
    int h = font->getHeight() * 7/4;

    int w = font->getTextWidth( _label.c_str(), _label.length() );
    w += ITEM_MARGIN * 2;
    if ( !_image.isNull() ) {
        int hi = 0;
        if ( _image->GetHeight()>h )
            hi = _image->GetHeight() * 8 / 7;
        if ( h < hi )
            h = hi;
        w += hi;
    }

    lvPoint minsize = skin->getMinSize();
    if ( minsize.y>0 && h < minsize.y )
        h = minsize.y;
    if ( minsize.x>0 && w < minsize.x )
        w = minsize.x;
    h += borders.bottom + borders.top;
    w += borders.left + borders.right;
    return lvPoint( w, h );
}

CRMenuSkinRef CRMenu::getSkin()
{
    if ( !_skin.isNull() )
        return _skin;
    lString16 path = getSkinName();
    lString16 path2;
    if ( !path.startsWith( L"#" ) )
        path = lString16(L"/CR3Skin/") + path;
    else if ( _wm->getScreenOrientation()&1 )
        _skin = _wm->getSkin()->getMenuSkin( (path + L"-rotated").c_str() );
    if ( !_skin )
        _skin = _wm->getSkin()->getMenuSkin( path.c_str() );
    return _skin;
}

lvPoint CRMenu::getItemSize()
{
    CRMenuSkinRef skin = getSkin();
    CRRectSkinRef itemSkin = skin->getItemSkin();
    lvRect itemBorders = itemSkin->getBorderWidths();
    lvPoint sz = CRMenuItem::getItemSize( itemSkin );
    if ( !isSubmenu() || _propName.empty() || _props.isNull() )
        return sz;
    int maxw = 0;
    for ( int i=0; i<_items.length(); i++ ) {
        lString16 s = _items[i]->getLabel();
        int w = _valueFont->getTextWidth( s.c_str(), s.length() );
        if ( w > maxw )
            maxw = w;
    }
    if ( maxw>0 )
        sz.x = sz.x + itemBorders.left + itemBorders.right + maxw;
    return sz;
}

int CRMenu::getItemHeight()
{
    CRMenuSkinRef skin = getSkin();
    CRRectSkinRef itemSkin = skin->getItemSkin();
    CRRectSkinRef separatorSkin = skin->getSeparatorSkin();
    int separatorHeight = 0;
    if ( !separatorSkin.isNull() )
        separatorHeight = separatorSkin->getMinSize().y;
    int h = itemSkin->getFont()->getHeight() * 5/4;
    lvPoint minsize = skin->getMinSize();
    if ( minsize.y>0 && h < minsize.y )
        h = minsize.y;
    if ( _fullscreen ) {
        int nItems = _pageItems; // _items.length();
        //int scrollHeight = 0;
        //lvRect statusRect;
        //if ( getStatusRect(  statusRect ) )
        //    scrollHeight = statusRect.height();
        //lvRect rc(0,0,_wm->getScreen()->getWidth(), _wm->getScreen()->getHeight() );
        lvRect client;
        getClientRect( client );
        h = client.height() - separatorHeight*(nItems-1);
        if ( nItems > 0 )
            h /= nItems;
    }

    return h;
}

lvPoint CRMenu::getMaxItemSize()
{
    CRMenuSkinRef skin = getSkin();
    CRRectSkinRef itemSkin = skin->getItemSkin();
    lvPoint mySize = getItemSize();
    //int itemHeight = getItemHeight();
    int maxx = 0;
    int maxy = 0;
    for ( int i=0; i<_items.length(); i++ ) {
        lvPoint sz = _items[i]->getItemSize( itemSkin );
        if ( maxx < sz.x )
            maxx = sz.x;
        if ( maxy < sz.y )
            maxy = sz.y;
    }
    if ( maxx < mySize.x )
        maxx = mySize.x;
    if ( maxy < mySize.y )
        maxy = mySize.y;
    if ( _fullscreen )
        maxy = getItemHeight();
    return lvPoint( maxx, maxy );
}

lvPoint CRMenu::getSize()
{
    if ( _fullscreen )
        return lvPoint( _wm->getScreen()->getWidth(), _wm->getScreen()->getHeight() );
    lvPoint itemSize = getMaxItemSize();
    int nItems = _items.length();
    int scrollHeight = 0;
    if ( nItems > _pageItems ) {
        nItems = _pageItems;
        scrollHeight = SCROLL_HEIGHT;
    }
    int h = nItems * (itemSize.y) + scrollHeight;
    int w = itemSize.x + 3 * ITEM_MARGIN + HOTKEY_SIZE;
    CRMenuSkinRef skin = getSkin();
    CRRectSkinRef sskin = skin->getItemShortcutSkin();
    CRRectSkinRef iskin = skin->getItemSkin();
    if ( !sskin.isNull() ) {
        lvPoint ssz = sskin->getMinSize();
        lvRect borders = sskin->getBorderWidths();
        w += ssz.x + borders.left + borders.right;
    }
    if ( !iskin.isNull() ) {
        lvRect borders = iskin->getBorderWidths();
        w += borders.left + borders.right;
    }
    if ( w>600 )
        w = 600;
    lvPoint res = skin->getWindowSize( lvPoint( w, h ) );
    if ( res.x > _wm->getScreen()->getWidth() )
        res.x = _wm->getScreen()->getWidth();
    if ( res.y > _wm->getScreen()->getHeight() )
        res.y = _wm->getScreen()->getHeight();
    return res;
}

lString16 CRMenu::getSubmenuValue()
{
    if ( !isSubmenu() || _propName.empty() || _props.isNull() )
        return lString16();
    lString16 value = getProps()->getStringDef(
                               UnicodeToUtf8(getPropName()).c_str(), "");
    for ( int i=0; i<_items.length(); i++ ) {
        if ( !_items[i]->getPropValue().empty() &&
                value==(_items[i]->getPropValue()) )
            return _items[i]->getLabel();
    }
    return lString16();
}

void CRMenu::toggleSubmenuValue()
{
    if ( !isSubmenu() || _propName.empty() || _props.isNull() )
        return;
    lString16 value = getProps()->getStringDef(
                               UnicodeToUtf8(getPropName()).c_str(), "");
    for ( int i=0; i<_items.length(); i++ ) {
        if ( !_items[i]->getPropValue().empty() &&
              value==(_items[i]->getPropValue()) ) {
            int n = (i + 1) % _items.length();
            getProps()->setString(UnicodeToUtf8(getPropName()).c_str(), _items[n]->getPropValue() );
            //return _items[i]->getLabel();
            return;
        }
    }
}

#if 0
static void DrawArrow( LVDrawBuf & buf, int x, int y, int dx, int dy, lvColor cl, int direction )
{
    int x0 = x + dx/2;
    int y0 = y + dy/2;
    dx -= 4;
    dy -= 4;
    dx /= 2;
    dy /= 2;
    int deltax = direction?-1:1;
    x0 -= deltax * dx/2;
    for ( int k=0; k<dx; k++ ) {
        buf.FillRect( x0+k*deltax, y0-k, x0+k*deltax+1, y0+k+1, cl );
    }
}
#endif

int CRMenu::getScrollHeight()
{
    CRMenuSkinRef skin = getSkin();
    int nItems = _items.length();
    int scrollHeight = 0;
    CRScrollSkinRef sskin = skin->getScrollSkin();
    if ( nItems > _pageItems || !sskin->getAutohide() ) {
        nItems = _pageItems;
        scrollHeight = SCROLL_HEIGHT;
        if ( sskin->getMinSize().y>0 )
            scrollHeight = sskin->getMinSize().y;
    }
    return scrollHeight;
}

/// returns index of selected item, -1 if no item selected
int CRMenu::getSelectedItemIndex()
{
    if ( _cmdToHighlight>=0 ) {
        // highlighted command
        for ( int i=0; i<_items.length(); i++ ) {
            if ( _items[i]->getId()==_cmdToHighlight ) {
                return i;
            }
        }
        return -1;
    }
    if ( getProps().isNull() )
        return -1;
    for ( int i=0; i<_items.length(); i++ ) {
        if ( !_items[i]->getPropValue().empty() &&
              getProps()->getStringDef(
                       UnicodeToUtf8(getPropName()).c_str()
                       , "")==(_items[i]->getPropValue()) )
            return i;
    }
    return -1;
}

void CRMenu::highlightCommandItem( int cmd )
{
    CRLog::debug("Highlighting menu item");
    _cmdToHighlight = cmd;
    _selectedItem = getSelectedItemIndex();
    _cmdToHighlight = -1;
    setDirty();
    _wm->updateWindow(this);
}

void CRMenu::drawClient()
{
    CRLog::trace("enter CRMenu::drawClient()");
    LVDrawBuf & buf = *_wm->getScreen()->getCanvas();
    CRMenuSkinRef skin = getSkin();
    CRRectSkinRef clientSkin = skin->getClientSkin();
    //CRRectSkinRef titleSkin = skin->getTitleSkin();
    //CRScrollSkinRef sskin = skin->getScrollSkin();
    CRRectSkinRef valueSkin = skin->getValueSkin();
    CRRectSkinRef itemSkin = skin->getItemSkin();
    CRRectSkinRef itemShortcutSkin = skin->getItemShortcutSkin();
    CRRectSkinRef itemSelSkin = skin->getSelItemSkin();
    CRRectSkinRef itemSelShortcutSkin = skin->getSelItemShortcutSkin();
    CRRectSkinRef evenitemSkin = skin->getEvenItemSkin();
    CRRectSkinRef evenitemShortcutSkin = skin->getEvenItemShortcutSkin();
    CRRectSkinRef evenitemSelSkin = skin->getEvenSelItemSkin();
    CRRectSkinRef evenitemSelShortcutSkin = skin->getEvenSelItemShortcutSkin();
    if ( evenitemSkin.isNull() )
        evenitemSkin = itemSkin;
    if ( evenitemShortcutSkin.isNull() )
        evenitemShortcutSkin = itemShortcutSkin;
    if ( evenitemSelSkin.isNull() )
        evenitemSelSkin = itemSelSkin;
    if ( evenitemSelShortcutSkin.isNull() )
        evenitemSelShortcutSkin = itemSelShortcutSkin;

    CRRectSkinRef separatorSkin = skin->getSeparatorSkin();
    int separatorHeight = 0;
    if ( !separatorSkin.isNull() )
        separatorHeight = separatorSkin->getMinSize().y;

    lvRect itemBorders = itemSkin->getBorderWidths();

    bool showShortcuts = skin->getShowShortcuts();

    buf.SetTextColor( 0x000000 );
    buf.SetBackgroundColor( 0xFFFFFF );

    lvRect clientRect;
    getClientRect(clientRect);
    if ( !clientSkin.isNull() && _pageUpdate)
        clientSkin->draw( buf, clientRect );

    lvPoint itemSize = getMaxItemSize();
    //int hdrHeight = itemSize.y; // + ITEM_MARGIN + ITEM_MARGIN;
    lvPoint sz = getSize();

    //int nItems = _items.length();
    //int scrollHeight = getScrollHeight();

    lvRect itemsRc( clientRect );
    lvRect rc( itemsRc );
    rc.top += 0; //ITEM_MARGIN;
    //rc.left += ITEM_MARGIN;
    //rc.right -= ITEM_MARGIN;
    LVFontRef numberFont( fontMan->GetFont( MENU_NUMBER_FONT_SIZE, 600, true, css_ff_sans_serif, lString8("Arial")) );
    for ( int index=0; index<_pageItems; index++ ) {
        int i = _topItem + index;
        if ( i >= _items.length() )
            break;
        CRMenuItem * item = _items[i];

        bool selected = (i == _selectedItem);

        rc.bottom = rc.top + itemSize.y;
		if (!(_pageUpdate || item->isItemDirty())) {
			rc.top = rc.bottom + separatorHeight;
			continue;
		}

		if (!_pageUpdate && !clientSkin.isNull()) {
			lvRect oldcr;
			buf.GetClipRect(&oldcr);
			buf.SetClipRect(&rc);
			clientSkin->draw( buf, clientRect );
			buf.SetClipRect(&oldcr);
		}

        bool even = (i & 1);
        CRRectSkinRef is = selected
                           ? (even ? evenitemSelSkin : itemSelSkin)
                           : (even ? evenitemSkin : itemSkin);
        CRRectSkinRef ss = selected
                           ? (even ? evenitemSelShortcutSkin : itemSelShortcutSkin)
                           : (even ? evenitemShortcutSkin : itemShortcutSkin);
        if ( selected ) {
            lvRect sel = rc;
            sel.extend( 4 );
            //buf.FillRect(sel, itemSelSkin->getBackgroundColor() );
        }

        lvRect itemRc( rc );

        if ( showShortcuts ) {
            int shortcutSize = HOTKEY_SIZE;
            if ( shortcutSize < ss->getMinSize().x )
                shortcutSize = ss->getMinSize().x;
            // number
            lvRect numberRc( rc );
            //numberRc.extend(ITEM_MARGIN/4); //ITEM_MARGIN/8-2);
            numberRc.right = numberRc.left + shortcutSize;

            ss->draw( buf, numberRc );
            lString16 number = index<9 ? lString16::itoa( index+1 ) : L"0";
            buf.SetTextColor( ss->getTextColor() );
            buf.SetBackgroundColor( ss->getBackgroundColor() );
            ss->drawText( buf, numberRc, number );
            // item
            itemRc.left = numberRc.right;
        }

        is->setTextAlign( is->getTextAlign() | SKIN_EXTEND_TAB);
        item->Draw( buf, itemRc, is, valueSkin, selected );

        // draw separator
        if ( separatorHeight>0 && index<_pageItems-1 ) {
            lvRect r(rc);
            r.top += itemSize.y;
            r.bottom = r.top + separatorHeight;
            separatorSkin->draw(buf, r);
        }
        rc.top += itemSize.y + separatorHeight;
    }
    CRLog::trace("exit CRMenu::drawClient()");
    _pageUpdate = false;
}

/// draw battery state to specified rectangle of screen
void CRGUIWindowManager::drawBattery( LVDrawBuf & buf, const lvRect & rc )
{
    int percent;
    bool charge;
    if ( !getBatteryStatus( percent, charge ) )
        return;
    LVDrawStateSaver saver( buf );
    buf.SetTextColor(0xFFFFFF);
    buf.SetBackgroundColor(0x000000);

    LVRefVec<LVImageSource> icons;
    bool drawPercent = false; //m_props->getBoolDef(PROP_SHOW_BATTERY_PERCENT, true) || m_batteryIcons.size()<=2;
    if ( m_batteryIcons.size()>1 ) {
        icons.add(getBatteryIcons()[0]);
        if ( drawPercent ) {
//            m_batteryFont = fontMan->GetFont(getBatteryIcons()[0]->GetHeight()-1, 900, false,
//                    DEFAULT_FONT_FAMILY, m_statusFontFace);
            icons.add(getBatteryIcons()[getBatteryIcons().length()-1]);
        } else {
            for ( int i=1; i<getBatteryIcons().length()-1; i++ )
                icons.add(getBatteryIcons()[i]);
        }
    } else {
        if ( m_batteryIcons.size()==1 )
            icons.add(getBatteryIcons()[0]);
    }

    //bool drawPercent = m_props->getBoolDef( PROP_SHOW_BATTERY_PERCENT, true );
    LVDrawBatteryIcon( &buf, rc,
                       percent, charge,
                       icons,
                       NULL );
}

/// closes menu and its submenus
void CRMenu::destroyMenu()
{
    for ( int i=_items.length()-1; i>=0; i-- )
        if ( _items[i]->isSubmenu() ) {
            ((CRMenu*)_items[i])->destroyMenu();
            _items.remove( i );
        }
    _wm->closeWindow( this ); // close, for root menu
}

/// closes menu and its submenus
void CRMenu::closeMenu( int command, int params )
{
    for ( int i=0; i<_items.length(); i++ )
        if ( _items[i]->isSubmenu() )
            ((CRMenu*)_items[i])->closeMenu( 0, 0 );
    if ( _menu != NULL )
        _wm->showWindow( this, false ); // just hide, for submenus
    else {
        if ( command )
            _wm->postCommand( command, params );
        destroyMenu();
    }
}

/// closes top level menu and its submenus, posts command
void CRMenu::closeAllMenu( int command, int params )
{
    CRMenu* p = this;
    while ( p->_menu )
        p = p->_menu;
    if ( command )
        _wm->postCommand( command, params );
    p->destroyMenu();
}

/// called on system configuration change: screen size and orientation
void CRMenu::reconfigure( int flags )
{
    _skin.Clear();
    getSkin();
    _fullscreen = _fullscreen || _skin->getFullScreen();
    CRGUIWindowBase::reconfigure( flags );
    int pageItems = _pageItems;
    if ( _skin->getMinItemCount()>0 && pageItems<_skin->getMinItemCount() )
        pageItems = _skin->getMinItemCount();
    if ( _skin->getMaxItemCount()>0 && pageItems>_skin->getMaxItemCount() )
        pageItems = _skin->getMaxItemCount();
    if ( pageItems!=_pageItems ) {
        // change items per page
        _pageItems = pageItems;
        _topItem = _topItem / pageItems * pageItems;
    }
}

bool CRMenu::onKeyPressed( int key, int flags )
{
    CRGUIWindowBase::onKeyPressed( key, flags );
    return true; // don't allow key processing by parent window
}

/// called if window gets focus
void CRMenu::activated()
{
    int curItem = getSelectedItemIndex();
    if ( curItem>=0 ) {
        _topItem = curItem / _pageItems * _pageItems;
        _selectedItem = curItem;
    } else
		_selectedItem = getDefaultSelectionIndex();
    _pageUpdate = true;
    setDirty();
}

void CRMenu::doCloseMenu(int command, bool highlight, int param)
{
	if ( _menu == NULL ) {
		if (highlight)
			highlightCommandItem( command );
		closeMenu(command, param); // close, for root menu	
    } else
		closeMenu(0);
}

int CRMenu::getLastOnPage()
{
	int lastOnPage = _topItem + _pageItems;
	if (lastOnPage >= (int)_items.length())
		lastOnPage = (int)_items.length();
	return lastOnPage;
}

void CRMenu::setCurItem(int nItem)
{
	int oldItem = _selectedItem;

	if (_selectedItem >= 0) {
		_items[oldItem]->onLeave();
	}
	int lastOnPage = getLastOnPage();
	_selectedItem = nItem;
	if (_selectedItem < _topItem)
		_selectedItem = lastOnPage -1;
	else if (_selectedItem > lastOnPage -1)
		_selectedItem = _topItem;
	_items[_selectedItem]->onEnter();
	if (oldItem != _selectedItem) {
		setDirty();
		_wm->updateWindow(this);
	}
}

/// returns true if command is processed
bool CRMenu::onCommand( int command, int params )
{
	CRLog::trace( "CRMenu::onCommand(%d, %d)", command, params );
    switch (command) {
	case MCMD_CANCEL:
		closeMenu( 0 );
		return true;
	case MCMD_OK:
		doCloseMenu(getId(), false, params);
		return true;
	case MCMD_SCROLL_FORWARD_LONG:
        setCurPage( getCurPage()+10 );
        return true;
    case MCMD_SCROLL_BACK_LONG:
		setCurPage( getCurPage()-10 );
		return true;
	case MCMD_SCROLL_FORWARD:
		if ( params==0 )
			params = 1;
        setCurPage( getCurPage()+params );
        return true;
    case MCMD_SCROLL_BACK:
		if ( params==0 )
			params = 1;
        setCurPage( getCurPage()-params );
        return true;
    case MCMD_NEXT_ITEM:
		setCurItem(_selectedItem + 1);
		return true;
    case MCMD_PREV_ITEM:
		setCurItem(_selectedItem < 0 ? getLastOnPage() -1 : _selectedItem - 1);
		return true;
    case MCMD_NEXT_PAGE:
		if (setCurPage(getCurPage() + 1))
			if (_selectedItem >= 0)	
				setCurItem(_selectedItem + _pageItems);
		return true;
    case MCMD_PREV_PAGE:
		if (_topItem == 0) {
			doCloseMenu(getId(), false, params);
		} else if (setCurPage(getCurPage() - 1) && _selectedItem >= 0)
			setCurItem(_selectedItem - _pageItems);
		return true;
    case MCMD_SELECT:
		onItemSelect(_selectedItem, params);
		return true;
	}

    int option = -1;
    int longPress = 0;
    if ( command>=MCMD_SELECT_0 && command<=MCMD_SELECT_9 )
        option = (command==MCMD_SELECT_0) ? 9 : command - MCMD_SELECT_1;
    if ( command>=MCMD_SELECT_0_LONG && command<=MCMD_SELECT_9_LONG ) {
        option = (command==MCMD_SELECT_0_LONG) ? 9 : command - MCMD_SELECT_1_LONG;
        longPress = 1;
    }
    if ( option < 0 ) {
        CRLog::error( "CRMenu::onCommand() - unsupported command %d, %d", command, params );
        return true;
    }
	onItemSelect(option += getTopItem(), longPress);
    return true;
}

bool CRMenu::onItemSelect(int itemId, int param)
{
	if (itemId < 0 || itemId >= _items.length()) {
		CRLog::error( "CRMenu::onItemSelect() - invalid selection: %d", itemId);
		return true;
	}

    CRMenuItem * item = _items[itemId];

    if (item->onSelect() > 0)
        return true;

    if (item->isSubmenu()) {
        CRMenu * menu = (CRMenu *)item;
        if ( menu->getItems().length() <= 3 ) {
            // toggle 2 and 3 choices w/o menu
            menu->toggleSubmenuValue();
            item->setItemDirty();
            setDirty();
        } else {
            // show menu
            _wm->activateWindow( menu );
        }
        return true;
    } else {
        // command menu item
        if ( !item->getPropValue().empty() ) {
                // set property
            CRLog::trace("Setting property value");
            _props->setString( UnicodeToUtf8(getPropName()).c_str(), item->getPropValue() );
            doCloseMenu(getId());
            return true;
        }
        doCloseMenu(item->getId(), true, param);
        return true;
	}
}

const lvRect & CRMenu::getRect()
{
    lvPoint sz = getSize();
    lvRect rc = _wm->getScreen()->getRect();
    _rect = rc;
    _rect.top = _rect.bottom - sz.y;
    _rect.right = _rect.left + sz.x;
    return _rect;
}

void CRMenu::draw()
{
	_pages = _pageItems>0 ? (_items.length()+_pageItems-1)/_pageItems : 0;
	_page = _pages>0 ? _topItem/_pageItems+1 : 0;
	_caption = _label;
	_icon = _image;

	if (_pageUpdate) {
		CRGUIWindowBase::draw();
	} else {
	    drawTitleBar();
		drawClient();
		drawStatusBar();
	}
}

static bool readNextLine( const LVStreamRef & stream, lString16 & dst )
{
    lString8 line;
    bool flgComment = false;
    for ( ; ; ) {
        int ch = stream->ReadByte();
        if ( ch<0 )
            break;
        if ( ch=='#' && line.empty() )
            flgComment = true;
        if ( ch=='\r' || ch=='\n' ) {
            if ( flgComment ) {
                flgComment = false;
                line.clear();
            } else {
                                if ( line[0]==(lChar8)0xEf && line[1]==(lChar8)0xBB && line[2]==(lChar8)0xBF )
					line.erase( 0, 3 );
                if ( !line.empty() ) {
                    dst = Utf8ToUnicode( line );
                    return true;
                }
            }
        } else {
            line << ch;
        }
    }
    return false;
}

static bool splitLine( lString16 line, const lString16 & delimiter, lString16 & key, lString16 & value )
{
    if ( !line.empty() ) {
        unsigned n = line.pos(delimiter);
        value.clear();
        key = line;
        if ( n>0 && n <line.length()-1 ) {
            value = line.substr( n+1, line.length() - n - 1 );
            key = line.substr( 0, n );
            key.trim();
            value.trim();
            return key.length()!=0 && value.length()!=0;
        }
    }
    return false;
}

static int decodeKey( lString16 name )
{
    name.trim();
    if ( name.empty() )
        return 0;
    int key = 0;
    lChar16 ch0 = name[0];
    if ( ch0 >= '0' && ch0 <= '9' )
        return name.atoi();
    if ( ch0=='-' && name.length()>=2 && name[1] >= '0' && name[1] <= '9' )
        return name.atoi();
    if ( name.length()==3 && name[0]=='\'' && name[2]=='\'' )
        key = name[1];
    if ( name.length() == 1 )
        key = name[0];
    if ( key == 0 && name.length()>=4 && name[0]=='0' && name[1]=='x' ) {
        for ( unsigned i=2; i<name.length(); i++ ) {
            lChar16 ch = name[i];
            if ( ch>='0' && ch<='9' )
                key = key*16 + (ch-'0');
            else if ( ch>='a' && ch<='f' )
                key = key*16 + (ch-'a') + 10;
            else if ( ch>='A' && ch<='F' )
                key = key*16 + (ch-'A') + 10;
            else
                break;
        }
    }
    return key;
}

bool CRGUIAcceleratorTableList::openFromFile( const char  * defFile, const char * mapFile )
{
    _table.clear();
    LVHashTable<lString16, int> defs( 256 );
    LVStreamRef defStream = LVOpenFileStream( defFile, LVOM_READ );
    if ( defStream.isNull() ) {
        CRLog::error( "cannot open keymap def file %s", defFile );
        return false;
    }
    LVStreamRef mapStream = LVOpenFileStream( mapFile, LVOM_READ );
    if ( mapStream.isNull() ) {
        CRLog::error( "cannot open keymap file %s", defFile );
        return false;
    }
    lString16 line;
    CRPropRef props = LVCreatePropsContainer();
    while ( readNextLine(defStream, line) ) {
        lString16 name;
        lString16 value;
        if ( splitLine( line, lString16(L"="), name, value ) )  {
            int key = decodeKey( value );
            if ( key!=0 )
                defs.set( name, key );
            else
                CRLog::error("Unknown key definition in line %s", UnicodeToUtf8(line).c_str() );
        } else if ( !line.empty() )
            CRLog::error("Invalid definition in line %s", UnicodeToUtf8(line).c_str() );
    }
    if ( !defs.length() ) {
        CRLog::error("No definitions read from %s", defFile);
        return false;

    }
    lString16 section;
    CRGUIAcceleratorTableRef table( new CRGUIAcceleratorTable() );
    bool eof = false;
    do {
        eof = !readNextLine(mapStream, line);
        if ( eof || (!line.empty() && line[0]=='[') ) {
            // eof or [section] found
            // save old section
            if ( !section.empty() ) {
                if ( table->length() ) {
                    _table.set( section, table );
                }
                section.clear();
            }
            // begin new section
            if ( !eof ) {
                table = CRGUIAcceleratorTableRef( new CRGUIAcceleratorTable() );
                int endbracket = line.pos( lString16(L"]") );
                if ( endbracket<=0 )
                    endbracket = line.length();
                if ( endbracket >= 2 )
                    section = line.substr( 1, endbracket - 1 );
                else
                    section.clear(); // wrong sectino
            }
        } else if ( !section.empty() ) {
            // read definition
            lString16 name;
            lString16 value;
            if ( splitLine( line, lString16(L"="), name, value ) ) {
                int flag = 0;
                int key = 0;
                lString16 keyName;
                lString16 flagName;
                splitLine( name, lString16(L","), keyName, flagName );
                if ( !flagName.empty() ) {
                    flag = decodeKey( flagName );
                    if ( !flag )
                        flag = defs.get( flagName );
                }
                // decoding key name
                key = decodeKey( keyName );
                if ( !key )
                    key = defs.get( keyName );
                if ( !key ) {
                    CRLog::error( "unknown key definition %s in line %s", UnicodeToUtf8(keyName).c_str(), UnicodeToUtf8(line).c_str() );
                    continue;
                }
                int cmd = 0;
                int cmdParam = 0;
                lString16 cmdName;
                lString16 paramName;
                splitLine( value, lString16(L","), cmdName, paramName );
                if ( !paramName.empty() ) {
                    cmdParam = decodeKey( paramName );
                    if ( !cmdParam )
                        cmdParam = defs.get( paramName );
                }
                cmd = decodeKey( cmdName );
                if ( !cmd )
                    cmd = defs.get( cmdName );
                if ( key != 0 && cmd != 0 ) {
                    // found valid key cmd definition
                    table->add( key, flag, cmd, cmdParam );
                    //CRLog::trace("Acc: %d, %d => %d, %d", key, flag, cmd, cmdParam);
                } else {
                    CRLog::error( "unknown command definition %s in line %s", UnicodeToUtf8(cmdName).c_str(), UnicodeToUtf8(line).c_str() );
                    continue;
                }
            }
        }
    } while ( !eof );
    return !empty();
}

// get currently set layout
CRKeyboardLayoutRef CRKeyboardLayoutList::getCurrentLayout()
{
	if ( !_current.isNull() )
		return _current;
	_current = get( lString16( L"english" ) );
	if ( !_current )
		nextLayout();
	return _current;
}

// get next layout
CRKeyboardLayoutRef CRKeyboardLayoutList::prevLayout()
{
	bool found = false;
	CRKeyboardLayoutRef prev;
	CRKeyboardLayoutRef next;
	CRKeyboardLayoutRef first;
	CRKeyboardLayoutRef last;
	LVHashTable<lString16, CRKeyboardLayoutRef>::iterator i( _table );
	for ( ;; ) {
		LVHashTable<lString16, CRKeyboardLayoutRef>::pair * item = i.next();
		if ( !item )
			break;
		if ( first.isNull() )
			first = item->value;
		last = item->value;
		if ( item->value.get() == _current.get() ) {
			found = true;
		} else {
			if ( !found )
				prev = item->value;
			if ( !found && !next.isNull() )
				next = item->value;

		}
	}
	if ( prev.isNull() )
		_current = last;
	else
		_current = prev;
	return _current;
}

// get next layout
CRKeyboardLayoutRef CRKeyboardLayoutList::nextLayout()
{
	bool found = false;
	CRKeyboardLayoutRef prev;
	CRKeyboardLayoutRef next;
	CRKeyboardLayoutRef first;
	CRKeyboardLayoutRef last;
	LVHashTable<lString16, CRKeyboardLayoutRef>::iterator i( _table );
	for ( ;; ) {
		LVHashTable<lString16, CRKeyboardLayoutRef>::pair * item = i.next();
		if ( !item )
			break;
		if ( first.isNull() )
			first = item->value;
		last = item->value;
		if ( item->value.get() == _current.get() ) {
			found = true;
		} else {
			if ( !found )
				prev = item->value;
			else if ( next.isNull() )
				next = item->value;

		}
	}
	if ( next.isNull() )
		_current = first;
	else
		_current = next;
	return _current;
}

bool CRKeyboardLayoutList::openFromFile( const char  * layoutFile )
{
    //_table.clear();
    LVStreamRef stream = LVOpenFileStream( layoutFile, LVOM_READ );
    if ( stream.isNull() ) {
        CRLog::error( "cannot open keyboard layout file %s", layoutFile );
        return false;
    }
    lString16 line;
    lString16 section;
	CRKeyboardLayoutRef table;
	LVRef<CRKeyboardLayout> layout;
    bool eof = false;
    do {
        eof = !readNextLine(stream, line);
        if ( eof || (!line.empty() && line[0]=='[') ) {
            // eof or [section] found
            // save old section
            if ( !section.empty() ) {
                if ( layout->getItems().length() ) {
                    _table.set( section, table );
                }
                section.clear();
            }
            // begin new section
            if ( !eof ) {
                int endbracket = line.pos( lString16(L"]") );
                if ( endbracket<=0 )
                    endbracket = line.length();
                if ( endbracket >= 2 )
                    section = line.substr( 1, endbracket - 1 );
                else
                    section.clear(); // wrong sectino
				lString16 langname;
				lString16 layouttype;
				if ( !section.empty() && splitLine( section, lString16(L"."), langname, layouttype ) ) {
					table = _table.get( langname );
					if ( table.isNull() ) {
						table = CRKeyboardLayoutRef( new CRKeyboardLayoutSet() );
						_table.set( langname, table );
					}
					if ( layouttype == L"tx" )
						layout = table->tXKeyboard;
					else
						layout = table->vKeyboard;
				} else
					section.clear();
            }
        } else if ( !section.empty() ) {
            // read definition
            lString16 name;
            lString16 value;
            if ( splitLine( line, lString16(L"="), name, value ) ) {
				if ( name == L"enabled" ) {
					//if ( value == L"0" )
					//	; //TODO:set disabled flag
					continue;
				}
                int item;
				if ( !name.atoi(item) )
					continue;
				layout->set( item, value );
            }
        }
    } while ( !eof );
	return _table.length()>0;
}



static int inv_control_table[] = {
    // old cmd, new cmd, param multiplier
    DCMD_LINEUP, DCMD_LINEDOWN, 1,
    DCMD_LINEDOWN, DCMD_LINEUP, 1,
    DCMD_PAGEUP, DCMD_PAGEDOWN, 1,
    DCMD_PAGEDOWN, DCMD_PAGEUP, 1,
    DCMD_MOVE_BY_CHAPTER, DCMD_MOVE_BY_CHAPTER, -1,
    0, 0, 0, 0,
};

/// returns true if command is processed
bool CRDocViewWindow::onCommand( int command, int params )
{
    if ( command >= LVDOCVIEW_COMMANDS_START && command <= LVDOCVIEW_COMMANDS_END ) {
        // TODO: rework controls inversion
#if CR_INTERNAL_PAGE_ORIENTATION==1
        cr_rotate_angle_t a = _docview->GetRotateAngle();
		if ( a==CR_ROTATE_ANGLE_90 || a==CR_ROTATE_ANGLE_180 ) {
			// inverse controls
			for ( int i=0; inv_control_table[i]; i+=3 ) {
				if ( command == inv_control_table[i] ) {
					command = inv_control_table[i+1];
					params *= inv_control_table[i+2];
					break;
				}
			}
		}
#endif
        _docview->doCommand( (LVDocCmd)command, params );
        _dirty = true;
        return true;
    }
    return !_passCommandsToParent;
}


bool CRGUIKeyDownEvent::handle( CRGUIWindow * window )
{
    if ( _targetWindow!=NULL ) {
        if ( window!=_targetWindow )
            return false;
    }
    CRGUIWindowManager * wm = window->getWindowManager();
    bool res = window->onKeyPressed( _param1, _param2 );
    if ( res )
        wm->postEvent( new CRGUIUpdateEvent(false) );
    return res;
}

bool CRGUICommandEvent::handle( CRGUIWindow * window )
{
    if ( _targetWindow!=NULL ) {
        if ( window!=_targetWindow )
            return false;
    }
    CRGUIWindowManager * wm = window->getWindowManager();
    wm->forwardSystemEvents(false);
    bool res = window->onCommand( _param1, _param2 );
    wm->forwardSystemEvents(false);
    if ( res )
        wm->postEvent( new CRGUIUpdateEvent(false) );
    return res;
}
