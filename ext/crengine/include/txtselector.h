/** \file txtselector.h
    \brief DOM document selection navigator

   CoolReader Engine

   (c) Vadim Lopatin, 2000-2010

   This source code is distributed under the terms of
   GNU General Public License.

   See LICENSE file for details.

*/

#ifndef TXTSELECTOR_H
#define TXTSELECTOR_H

#include "lvtinydom.h"

// TEXT SELECTION TOOL

/// text selection tool commands
enum text_selection_cmd_t {
    CMD_SEL_MIDDLE_WORD = 4500,     // select middle word for initial interval
    CMD_SEL_MIDDLE_SENTENCE,        // select middle sentence for initial interval
    CMD_SEL_MIDDLE_PARA,            // select middle paragraph for initial interval
    CMD_SEL_MIDDLE_INTERVAL,        // select middle interval (word/sentence/paragraph depending on current setting) for initial interval
    CMD_SEL_CUR_MODE_TOGGLE,        // toggle current mode: start/end/both
    CMD_SEL_CUR_MODE_START,         // set current mode to start
    CMD_SEL_CUR_MODE_END,           // set current mode to end
    CMD_SEL_CUR_MODE_ALL,           // set current mode to start+end
    CMD_SEL_CUR_INTERVAL_TOGGLE,    // toggle current interval: paragraph/sentence/word
    CMD_SEL_CUR_INTERVAL_WORD,      // set current interval to word
    CMD_SEL_CUR_INTERVAL_SENTENCE,  // set current interval to sentence
    CMD_SEL_CUR_INTERVAL_PARA,      // set current interval to paragraph
    CMD_SEL_START_FWD_BY_WORD,      // move selection start - forward by word
    CMD_SEL_START_FWD_BY_SENTENCE,  // move selection start - forward by sentence
    CMD_SEL_START_FWD_BY_PARA,      // move selection start - forward by paragraph
    CMD_SEL_START_FWD_BY_INTERVAL,  // move selection start - forward by current interval (word/sentence/para)
    CMD_SEL_START_BACK_BY_WORD,     // move selection start - back by word
    CMD_SEL_START_BACK_BY_SENTENCE, // move selection start - back by sentence
    CMD_SEL_START_BACK_BY_PARA,     // move selection start - back by paragraph
    CMD_SEL_START_BACK_BY_INTERVAL, // move selection start - back by interval (word/sentence/para)
    CMD_SEL_END_FWD_BY_WORD,        // move selection end - forward by word
    CMD_SEL_END_FWD_BY_SENTENCE,    // move selection end - forward by sentence
    CMD_SEL_END_FWD_BY_PARA,        // move selection end - forward by paragraph
    CMD_SEL_END_FWD_BY_INTERVAL,    // move selection end - forward by interval (word/sentence/para)
    CMD_SEL_END_BACK_BY_WORD,       // move selection end - back by word
    CMD_SEL_END_BACK_BY_SENTENCE,   // move selection end - back by sentence
    CMD_SEL_END_BACK_BY_PARA,       // move selection end - back by paragraph
    CMD_SEL_END_BACK_BY_INTERVAL,   // move selection end - back by interval (word/sentence/para)
    CMD_SEL_ALL_FWD_BY_WORD,        // move selection start+end - forward by word
    CMD_SEL_ALL_FWD_BY_SENTENCE,    // move selection start+end - forward by sentence
    CMD_SEL_ALL_FWD_BY_PARA,        // move selection start+end - forward by paragraph
    CMD_SEL_ALL_FWD_BY_INTERVAL,    // move selection start+end - forward by interval (word/sentence/para)
    CMD_SEL_ALL_BACK_BY_WORD,       // move selection start+end - back by word
    CMD_SEL_ALL_BACK_BY_SENTENCE,   // move selection start+end - back by sentence
    CMD_SEL_ALL_BACK_BY_PARA,       // move selection start+end - back by paragraph
    CMD_SEL_ALL_BACK_BY_INTERVAL,   // move selection start+end - back by interval (word/sentence/para)
    CMD_SEL_CUR_FWD_BY_WORD,        // move selection depending on current mode (start/end/both) - forward by word
    CMD_SEL_CUR_FWD_BY_SENTENCE,    // move selection depending on current mode (start/end/both) - forward by sentence
    CMD_SEL_CUR_FWD_BY_PARA,        // move selection depending on current mode (start/end/both) - forward by paragraph
    CMD_SEL_CUR_FWD_BY_INTERVAL,    // move selection depending on current mode (start/end/both) - forward by interval (word/sentence/para)
    CMD_SEL_CUR_BACK_BY_WORD,       // move selection depending on current mode (start/end/both) - back by word
    CMD_SEL_CUR_BACK_BY_SENTENCE,   // move selection depending on current mode (start/end/both) - back by sentence
    CMD_SEL_CUR_BACK_BY_PARA,       // move selection depending on current mode (start/end/both) - back by paragraph
    CMD_SEL_CUR_BACK_BY_INTERVAL,   // move selection depending on current mode (start/end/both) - back by interval (word/sentence/para)
};

/// text selection tool
class ldomTextSelectionTool {
public:
    enum interval_t {
        WORD,
        SENTENCE,
        PARA,
    };
    enum direction_t {
        FORWARD,
        BACK,
    };
    enum origin_t {
        START,
        END,
        ALL,
    };
private:
    ldomXRange _initialRange;
    ldomXRange _currRange;
    interval_t _currInterval;
    origin_t   _currOrigin;


public:

    /// create selection tool for specified initial range (usually current page)
    ldomTextSelectionTool( ldomXRange & initialRange, interval_t initialInterval, origin_t initialOrigin );

    /// selects middle interval of specified type
    bool selectMiddleInterval( interval_t interval, ldomXPointer &positionToShow );
    /// moves selection, returns true if selection is moved, and it's necessary to ensure positionToShow is visible on screen
    bool moveBy( interval_t interval, direction_t dir, origin_t origin, int count, ldomXPointer &positionToShow );
    /// moves selection, returns true if selection is moved, and it's necessary to ensure positionToShow is visible on screen
    bool doCommand( int cmd, int param, ldomXPointer &positionToShow );
};

#endif // TXTSELECTOR_H
