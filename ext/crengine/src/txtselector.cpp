/*******************************************************

   CoolReader Engine

   txtselector.cpp:  DOM document selection navigator

   (c) Vadim Lopatin, 2000-2010
   This source code is distributed under the terms of
   GNU General Public License
   See LICENSE file for details

*******************************************************/

#include "../include/txtselector.h"

/// create selection tool for specified initial range (usually current page)
ldomTextSelectionTool::ldomTextSelectionTool( ldomXRange & initialRange, ldomTextSelectionTool::interval_t initialInterval, ldomTextSelectionTool::origin_t initialOrigin )
:  _initialRange(initialRange)
, _currRange(initialRange)
, _currInterval(initialInterval)
, _currOrigin(initialOrigin)
{
}

/// moves selection, returns true if selection is moved, and it's necessary to ensure positionToShow is visible on screen
bool ldomTextSelectionTool::moveBy( ldomTextSelectionTool::interval_t interval, ldomTextSelectionTool::direction_t dir, ldomTextSelectionTool::origin_t origin, int count, ldomXPointer &positionToShow )
{
    return false;
}

/// selects middle interval of specified type
bool ldomTextSelectionTool::selectMiddleInterval( interval_t interval, ldomXPointer &positionToShow )
{
    return false;
}

/// moves selection, returns true if selection is moved, and it's necessary to ensure positionToShow is visible on screen
bool ldomTextSelectionTool::doCommand( int cmd, int param, ldomXPointer &positionToShow )
{
    int count = 1;
    if ( cmd>1 && cmd<20 )
        count = cmd;
    switch (cmd) {
    case CMD_SEL_MIDDLE_WORD:           // select middle word for initial interval
        return selectMiddleInterval( WORD, positionToShow );
    case CMD_SEL_MIDDLE_SENTENCE:       // select middle sentence for initial interval
        return selectMiddleInterval( SENTENCE, positionToShow );
    case CMD_SEL_MIDDLE_PARA:            // select middle paragraph for initial interval
        return selectMiddleInterval( PARA, positionToShow );
    case CMD_SEL_MIDDLE_INTERVAL:        // select middle interval (word/sentence/paragraph depending on current setting) for initial interval
        return selectMiddleInterval( _currInterval, positionToShow );
    case CMD_SEL_CUR_MODE_TOGGLE:        // toggle current mode: start/end/both
        _currOrigin = (origin_t)(((int)_currOrigin+1)%3);
        return false;
    case CMD_SEL_CUR_MODE_START:         // set current mode to start
        _currOrigin = START;
        return false;
    case CMD_SEL_CUR_MODE_END:           // set current mode to end
        _currOrigin = END;
        return false;
    case CMD_SEL_CUR_MODE_ALL:           // set current mode to start+end
        _currOrigin = ALL;
        return false;
    case CMD_SEL_CUR_INTERVAL_TOGGLE:    // toggle current interval: paragraph/sentence/word
        _currInterval = (interval_t)(((int)_currInterval+1)%3);
        return false;
    case CMD_SEL_CUR_INTERVAL_WORD:      // set current interval to word
        _currInterval = WORD;
        return false;
    case CMD_SEL_CUR_INTERVAL_SENTENCE:  // set current interval to sentence
        _currInterval = SENTENCE;
        return false;
    case CMD_SEL_CUR_INTERVAL_PARA:      // set current interval to paragraph
        _currInterval = PARA;
        return false;
    case CMD_SEL_START_FWD_BY_WORD:      // move selection start - forward by word
        return moveBy( WORD, FORWARD, START, count, positionToShow );
    case CMD_SEL_START_FWD_BY_SENTENCE:  // move selection start - forward by sentence
        return moveBy( SENTENCE, FORWARD, START, count, positionToShow );
    case CMD_SEL_START_FWD_BY_PARA:      // move selection start - forward by paragraph
        return moveBy( PARA, FORWARD, START, count, positionToShow );
    case CMD_SEL_START_FWD_BY_INTERVAL:  // move selection start - forward by current interval (word/sentence/para)
        return moveBy( _currInterval, FORWARD, START, count, positionToShow );
    case CMD_SEL_START_BACK_BY_WORD:     // move selection start - back by word
        return moveBy( WORD, BACK, START, count, positionToShow );
    case CMD_SEL_START_BACK_BY_SENTENCE: // move selection start - back by sentence
        return moveBy( WORD, BACK, START, count, positionToShow );
    case CMD_SEL_START_BACK_BY_PARA:     // move selection start - back by paragraph
        return moveBy( WORD, BACK, START, count, positionToShow );
    case CMD_SEL_START_BACK_BY_INTERVAL: // move selection start - back by interval (word/sentence/para)
        return moveBy( _currInterval, BACK, START, count, positionToShow );
    case CMD_SEL_END_FWD_BY_WORD:        // move selection end - forward by word
        return moveBy( WORD, FORWARD, END, count, positionToShow );
    case CMD_SEL_END_FWD_BY_SENTENCE:    // move selection end - forward by sentence
        return moveBy( SENTENCE, FORWARD, END, count, positionToShow );
    case CMD_SEL_END_FWD_BY_PARA:        // move selection end - forward by paragraph
        return moveBy( PARA, FORWARD, END, count, positionToShow );
    case CMD_SEL_END_FWD_BY_INTERVAL:    // move selection end - forward by interval (word/sentence/para)
        return moveBy( _currInterval, FORWARD, END, count, positionToShow );
    case CMD_SEL_END_BACK_BY_WORD:       // move selection end - back by word
        return moveBy( WORD, BACK, END, count, positionToShow );
    case CMD_SEL_END_BACK_BY_SENTENCE:   // move selection end - back by sentence
        return moveBy( SENTENCE, BACK, END, count, positionToShow );
    case CMD_SEL_END_BACK_BY_PARA:       // move selection end - back by paragraph
        return moveBy( PARA, BACK, END, count, positionToShow );
    case CMD_SEL_END_BACK_BY_INTERVAL:   // move selection end - back by interval (word/sentence/para)
        return moveBy( _currInterval, BACK, END, count, positionToShow );
    case CMD_SEL_ALL_FWD_BY_WORD:        // move selection start+end - forward by word
        return moveBy( WORD, FORWARD, ALL, count, positionToShow );
    case CMD_SEL_ALL_FWD_BY_SENTENCE:    // move selection start+end - forward by sentence
        return moveBy( SENTENCE, FORWARD, ALL, count, positionToShow );
    case CMD_SEL_ALL_FWD_BY_PARA:        // move selection start+end - forward by paragraph
        return moveBy( PARA, FORWARD, ALL, count, positionToShow );
    case CMD_SEL_ALL_FWD_BY_INTERVAL:    // move selection start+end - forward by interval (word/sentence/para)
        return moveBy( _currInterval, FORWARD, ALL, count, positionToShow );
    case CMD_SEL_ALL_BACK_BY_WORD:       // move selection start+end - back by word
        return moveBy( WORD, BACK, ALL, count, positionToShow );
    case CMD_SEL_ALL_BACK_BY_SENTENCE:   // move selection start+end - back by sentence
        return moveBy( SENTENCE, BACK, ALL, count, positionToShow );
    case CMD_SEL_ALL_BACK_BY_PARA:       // move selection start+end - back by paragraph
        return moveBy( PARA, BACK, ALL, count, positionToShow );
    case CMD_SEL_ALL_BACK_BY_INTERVAL:   // move selection start+end - back by interval (word/sentence/para)
        return moveBy( _currInterval, BACK, ALL, count, positionToShow );
    case CMD_SEL_CUR_FWD_BY_WORD:        // move selection depending on current mode (start/end/both) - forward by word
        return moveBy( WORD, FORWARD, _currOrigin, count, positionToShow );
    case CMD_SEL_CUR_FWD_BY_SENTENCE:    // move selection depending on current mode (start/end/both) - forward by sentence
        return moveBy( SENTENCE, FORWARD, _currOrigin, count, positionToShow );
    case CMD_SEL_CUR_FWD_BY_PARA:        // move selection depending on current mode (start/end/both) - forward by paragraph
        return moveBy( PARA, FORWARD, _currOrigin, count, positionToShow );
    case CMD_SEL_CUR_FWD_BY_INTERVAL:    // move selection depending on current mode (start/end/both) - forward by interval (word/sentence/para)
        return moveBy( _currInterval, FORWARD, _currOrigin, count, positionToShow );
    case CMD_SEL_CUR_BACK_BY_WORD:       // move selection depending on current mode (start/end/both) - back by word
        return moveBy( WORD, BACK, _currOrigin, count, positionToShow );
    case CMD_SEL_CUR_BACK_BY_SENTENCE:   // move selection depending on current mode (start/end/both) - back by sentence
        return moveBy( SENTENCE, BACK, _currOrigin, count, positionToShow );
    case CMD_SEL_CUR_BACK_BY_PARA:       // move selection depending on current mode (start/end/both) - back by paragraph
        return moveBy( PARA, BACK, _currOrigin, count, positionToShow );
    case CMD_SEL_CUR_BACK_BY_INTERVAL:   // move selection depending on current mode (start/end/both) - back by interval (word/sentence/para)
        return moveBy( _currInterval, BACK, _currOrigin, count, positionToShow );
    }
    return false;
}

#ifdef _DEBUG
void testTxtSelector()
{
    CRLog::info("testTxtSelector()");

}
#endif
