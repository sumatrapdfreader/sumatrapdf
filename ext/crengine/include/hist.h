/** \file hist.h
    \brief file history and bookmarks container

    CoolReader Engine

    (c) Vadim Lopatin, 2000-2007
    This source code is distributed under the terms of
    GNU General Public License
    See LICENSE file for details
*/

#ifndef HIST_H_INCLUDED
#define HIST_H_INCLUDED

#include "lvptrvec.h"
#include <time.h>

enum bmk_type {
    bmkt_lastpos,
    bmkt_pos,
    bmkt_comment,
    bmkt_correction,
};

class CRBookmark {
private:
    lString16 _startpos;
    lString16 _endpos;
    int       _percent;
    int       _type;
	int       _shortcut;
    lString16 _postext;
    lString16 _titletext;
    lString16 _commenttext;
    time_t    _timestamp;
public:
	static lString16 getChapterName( ldomXPointer p );
    CRBookmark(const CRBookmark & v )
    : _startpos(v._startpos)
    , _endpos(v._endpos)
    , _percent(v._percent)
    , _type(v._type)
	, _shortcut(v._shortcut)
    , _postext(v._postext)
    , _titletext(v._titletext)
    , _commenttext(v._commenttext)
    , _timestamp(v._timestamp)
    {
    }
    CRBookmark & operator = (const CRBookmark & v )
    {
        _startpos = v._startpos;
        _endpos = v._endpos;
        _percent = v._percent;
        _type = v._type;
		_shortcut = v._shortcut;
        _postext = v._postext;
        _titletext = v._titletext;
        _commenttext = v._commenttext;
        _timestamp = v._timestamp;
        return *this;
    }
    CRBookmark() : _percent(0), _type(0), _shortcut(0), _timestamp(0) { }
    CRBookmark ( ldomXPointer ptr );
    lString16 getStartPos() { return _startpos; }
    lString16 getEndPos() { return _endpos; }
    lString16 getPosText() { return _postext; }
    lString16 getTitleText() { return _titletext; }
    lString16 getCommentText() { return _commenttext; }
	int getShortcut() { return _shortcut; }
    int getType() { return _type; }
    int getPercent() { return _percent; }
    time_t getTimestamp() { return _timestamp; }
    void setStartPos(const lString16 & s ) { _startpos = s; }
    void setEndPos(const lString16 & s ) { _endpos = s; }
    void setPosText(const lString16 & s ) { _postext= s; }
    void setTitleText(const lString16 & s ) { _titletext = s; }
    void setCommentText(const lString16 & s ) { _commenttext = s; }
    void setType( int n ) { _type = n; }
	void setShortcut( int n ) { _shortcut = n; }
    void setPercent( int n ) { _percent = n; }
    void setTimestamp( time_t t ) { _timestamp = t; }
};

class CRFileHistRecord {
private:
    lString16 _fname;
    lString16 _fpath;
    lString16 _title;
    lString16 _author;
    lString16 _series;
    lvpos_t   _size;
    LVPtrVector<CRBookmark> _bookmarks;
    CRBookmark _lastpos;
public:
    /// returns first available placeholder for new bookmark, -1 if no more space
    int getLastShortcutBookmark();
    /// returns first available placeholder for new bookmark, -1 if no more space
    int getFirstFreeShortcutBookmark();
    CRBookmark * setShortcutBookmark( int shortcut, ldomXPointer ptr );
    CRBookmark * getShortcutBookmark( int shortcut );
    time_t getLastTime() { return _lastpos.getTimestamp(); }
    lString16 getLastTimeString( bool longFormat=false );
    void setLastTime( time_t t ) { _lastpos.setTimestamp(t); }
    LVPtrVector<CRBookmark>  & getBookmarks() { return _bookmarks; }
    CRBookmark * getLastPos() { return &_lastpos; }
    void setLastPos( CRBookmark * bmk );
    lString16 getTitle() { return _title; }
    lString16 getAuthor() { return _author; }
    lString16 getSeries() { return _series; }
    lString16 getFileName() { return _fname; }
    lString16 getFilePath() { return _fpath; }
    lString16 getFilePathName() { return _fpath + _fname; }
    lvpos_t   getFileSize() { return _size; }
    void setTitle( const lString16 & s ) { _title = s; }
    void setAuthor( const lString16 & s ) { _author = s; }
    void setSeries( const lString16 & s ) { _series = s; }
    void setFileName( const lString16 & s ) { _fname = s; }
    void setFilePath( const lString16 & s ) { _fpath = s; }
    void setFileSize( lvsize_t sz ) { _size = sz; }
    CRFileHistRecord()
        : _size(0)
    {
    }
    CRFileHistRecord( const CRFileHistRecord & v)
        : _fname(v._fname)
        , _fpath(v._fpath)
        , _title(v._title)
        , _author(v._author)
        , _series(v._series)
        , _size(v._size)
        , _bookmarks(v._bookmarks)
        , _lastpos(v._lastpos)
    {
    }
    ~CRFileHistRecord()
    {
    }
};


class CRFileHist {
private:
    LVPtrVector<CRFileHistRecord> _records;
    int findEntry( const lString16 & fname, const lString16 & fpath, lvsize_t sz );
    void makeTop( int index );
public:
    void limit( int maxItems )
    {
        for ( int i=_records.length()-1; i>maxItems; i-- ) {
            _records.erase( i, 1 );
        }
    }
    LVPtrVector<CRFileHistRecord> & getRecords() { return _records; }
    bool loadFromStream( LVStreamRef stream );
    bool saveToStream( LVStream * stream );
    CRFileHistRecord * savePosition( lString16 fpathname, size_t sz, 
        const lString16 & title,
        const lString16 & author,
        const lString16 & series,
        ldomXPointer ptr );
    ldomXPointer restorePosition(  ldomDocument * doc, lString16 fpathname, size_t sz );
    CRFileHist()
    {
    }
    ~CRFileHist()
    {
        clear();
    }
    void clear();
};

#endif //HIST_H_INCLUDED
