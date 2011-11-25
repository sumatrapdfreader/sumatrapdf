/** \file lvpagesplitter.cpp
    \brief page splitter implementation

    CoolReader Engine

    (c) Vadim Lopatin, 2000-2006
    This source code is distributed under the terms of
    GNU General Public License.

    See LICENSE file for details.
*/

#include "../include/lvpagesplitter.h"
#include "../include/lvtinydom.h"
#include <time.h>


int LVRendPageList::FindNearestPage( int y, int direction )
{
    if (!length())
        return 0;
    for (int i=0; i<length(); i++)
    {
        const LVRendPageInfo * pi = ((*this)[i]);
        if (y<pi->start) {
            if (i==0 || direction>=0)
                return i;
            else
                return i-1;
        } else if (y<pi->start+pi->height) {
            if (i<length()-1 && direction>0)
                return i+1;
            else if (i==0 || direction>=0)
                return i;
            else
                return i-1;
        }
    }
    return length()-1;
}

LVRendPageContext::LVRendPageContext(LVRendPageList * pageList, int pageHeight)
    : callback(NULL), totalFinalBlocks(0)
    , renderedFinalBlocks(0), lastSentProgress(0), lastPercent(-1), page_list(pageList), page_h(pageHeight), footNotes(64), curr_note(NULL)
{
    if ( callback ) {
        callback->OnFormatStart();
    }
}

#define RENDER_PROGRESS_INTERVAL_SECONDS 2
#define RENDER_PROGRESS_INTERVAL_PERCENT 2
bool LVRendPageContext::updateRenderProgress( int numFinalBlocksRendered )
{
    renderedFinalBlocks += numFinalBlocksRendered;
    int percent = totalFinalBlocks>0 ? renderedFinalBlocks * 100 / totalFinalBlocks : 0;
    if ( percent<0 )
        percent = 0;
    if ( percent>100 )
        percent = 100;
    if ( callback && percent>lastPercent+RENDER_PROGRESS_INTERVAL_PERCENT ) {
        time_t t = time((time_t)0);
        if ( t>lastSentProgress+RENDER_PROGRESS_INTERVAL_SECONDS ) {
            callback->OnFormatProgress(percent);
            lastSentProgress = t;
            lastPercent = percent;
            return true;
        }
    }
    return false;
}

/// append footnote link to last added line
void LVRendPageContext::addLink( lString16 id )
{
    if ( !page_list )
        return;
    if ( lines.empty() )
        return;
    LVFootNote * note = getOrCreateFootNote( id );
    lines.last()->addLink(note);
}

/// mark start of foot note
void LVRendPageContext::enterFootNote( lString16 id )
{
    if ( !page_list )
        return;
    //CRLog::trace("enterFootNote( %s )", LCSTR(id) );
    if ( curr_note != NULL ) {
        CRLog::error("Nested entering note" );
        return;
    }
    curr_note = getOrCreateFootNote( id );
}

/// mark end of foot note
void LVRendPageContext::leaveFootNote()
{
    if ( !page_list )
        return;
    //CRLog::trace("leaveFootNote()" );
    if ( !curr_note ) {
        CRLog::error("leaveFootNote() w/o current note set");
    }
    curr_note = NULL;
}


void LVRendPageContext::AddLine( int starty, int endy, int flags )
{
    if ( curr_note!=NULL )
        flags |= RN_SPLIT_FOOT_NOTE;
    LVRendLineInfo * line = new LVRendLineInfo(starty, endy, flags);
    lines.add( line );
    if ( curr_note != NULL ) {
        //CRLog::trace("adding line to note (%d)", line->start);
        curr_note->addLine( line );
    }
}

#define FOOTNOTE_MARGIN 12


// helper class
struct PageSplitState {
public:
    int page_h;
    LVRendPageList * page_list;
    const LVRendLineInfo * pagestart;
    const LVRendLineInfo * pageend;
    const LVRendLineInfo * next;
    const LVRendLineInfo * last;
    int   footheight;
    LVFootNote * footnote;
    const LVRendLineInfo * footstart;
    const LVRendLineInfo * footend;
    const LVRendLineInfo * footlast;
    LVArray<LVPageFootNoteInfo> footnotes;
    int lastpageend;

    PageSplitState(LVRendPageList * pl, int pageHeight)
        : page_h(pageHeight)
        , page_list(pl)
        , pagestart(NULL)
        , pageend(NULL)
        , next(NULL)
        , last(NULL)
        , footheight(0)
        , footnote(NULL)
        , footstart(NULL)
        , footend(NULL)
        , footlast(NULL)
        , lastpageend(0)
    {
    }

    unsigned CalcSplitFlag( int flg1, int flg2 )
    {
        if (flg1==RN_SPLIT_AVOID || flg2==RN_SPLIT_AVOID)
            return RN_SPLIT_AVOID;
        if (flg1==RN_SPLIT_ALWAYS || flg2==RN_SPLIT_ALWAYS)
            return RN_SPLIT_ALWAYS;
        return RN_SPLIT_AUTO;
    }

    void StartPage( const LVRendLineInfo * line )
    {
#ifdef DEBUG_FOOTNOTES
        if ( !line ) {
            CRLog::trace("StartPage(NULL)");
        }
        if ( CRLog::isTraceEnabled() )
            CRLog::trace("StartPage(%d)", line ? line->start : -111111111);
#endif
        last = pagestart = line;
        pageend = NULL;
        next = NULL;
    }
    void AddToList()
    {
        bool hasFootnotes = footnotes.length() > 0;
        if ( !pageend )
            pageend = pagestart;
        if ( !pagestart && !hasFootnotes )
            return;
        int start = (pagestart && pageend) ? pagestart->getStart() : lastpageend;
        int h = (pagestart && pageend) ? pageend->getEnd()-pagestart->getStart() : 0;
#ifdef DEBUG_FOOTNOTES
        if ( CRLog::isTraceEnabled() ) {
            if ( pagestart && pageend )
                CRLog::trace("AddToList(%d, %d) footnotes: %d  pageHeight=%d", pagestart->start, pageend->start+pageend->height, footnotes.length(), h);
            else
                CRLog::trace("AddToList(Only footnote: %d) footnotes: %d  pageHeight=%d", lastpageend, footnotes.length(), h);
        }
#endif
        LVRendPageInfo * page = new LVRendPageInfo(start, h, page_list->length());
        lastpageend = start + h;
        if ( footnotes.length()>0 ) {
            page->footnotes.add( footnotes );
            footnotes.clear();
            footheight = 0;
        }
        page_list->add(page);
    }
    int currentFootnoteHeight()
    {
        if ( !footstart )
            return 0;
        int h = 0;
        h = (footlast?footlast:footstart)->getEnd() - footstart->getStart();
        return h;
    }
    int currentHeight( const LVRendLineInfo * line = NULL )
    {
        if ( line == NULL )
            line = last;
        int h = 0;
        if ( line && pagestart )
            h += line->getEnd() - pagestart->getStart();
        int footh = 0 /*currentFootnoteHeight()*/ + footheight;
        if ( footh )
            h += FOOTNOTE_MARGIN + footh;
        return h;
    }
    void AddLine( LVRendLineInfo * line )
    {
        if (pagestart==NULL )
        {
            StartPage( line );
        }
        else 
        {
            if (line->getStart()<last->getEnd())
                return; // for table cells
            unsigned flgSplit = CalcSplitFlag( last->getSplitAfter(), line->getSplitBefore() );
            bool flgFit = currentHeight( line ) <= page_h;
            if (!flgFit)
            {
            // doesn't fit
            // split
                next = line;
                pageend = last;
                AddToList();
                StartPage(next);
            }
            else if (flgSplit==RN_SPLIT_ALWAYS)
            {
            //fits, but split is mandatory
                if (next==NULL)
                {
                    next = line;
                }
                pageend = last;
                AddToList();
                StartPage(line);
            }
            else if (flgSplit==RN_SPLIT_AUTO)
            {
            //fits, split is allowed
            //update split candidate
                pageend = last;
                next = line;
            }
        }
        last = line;
    }
    void Finalize()
    {
        if (last==NULL)
            return;
        pageend = last;
        AddToList();
    }
    void StartFootNote( LVFootNote * note )
    {
#ifdef DEBUG_FOOTNOTES
        CRLog::trace( "StartFootNote(%d)", note->getLines().length() );
#endif
        if ( !note || note->getLines().length()==0 )
            return;
        footnote = note;
        //footstart = footnote->getLines()[0];
        //footlast = footnote->getLines()[0];
        footend = NULL;
    }
    void AddFootnoteFragmentToList()
    {
        if ( footstart==NULL )
            return; // no data
        if ( footend==NULL )
            footend = footstart;
        //CRLog::trace("AddFootnoteFragmentToList(%d, %d)", footstart->start, footend->end );
        int h = footend->getEnd() - footstart->getStart(); // currentFootnoteHeight();
        if ( h>0 && h<page_h ) {
            footheight += h;
#ifdef DEBUG_FOOTNOTES
            CRLog::trace("AddFootnoteFragmentToList(%d, %d)", footstart->getStart(), h);
#endif
            footnotes.add( LVPageFootNoteInfo( footstart->getStart(), h ) );
        }
        footstart = footend = NULL;
    }
    /// footnote is finished
    void EndFootNote()
    {
#ifdef DEBUG_FOOTNOTES
        CRLog::trace("EndFootNote()");
#endif
        footend = footlast;
        AddFootnoteFragmentToList();
        footnote = NULL;
        footstart = footend = footlast = NULL;
    }
    void AddFootnoteLine( LVRendLineInfo * line )
    {
        int dh = line->getEnd()
            - (footstart ? footstart->getStart() : line->getStart())
            + (footheight==0?FOOTNOTE_MARGIN:0);
        int h = currentHeight(NULL); //next
#ifdef DEBUG_FOOTNOTES
        CRLog::trace("Add footnote line %d  footheight=%d  h=%d  dh=%d  page_h=%d", line->start, footheight, h, dh, page_h);
#endif
        if ( h + dh > page_h ) {
#ifdef DEBUG_FOOTNOTES
            CRLog::trace("No current page space for this line, %s", (footstart?"footstart is not null":"footstart is null"));
#endif
            if ( footstart==NULL ) {
                //CRLog::trace("Starting new footnote fragment");
                // no footnote lines fit
                //pageend = last;
                AddToList();
                //StartPage( last );
                StartPage( last );
            } else {
                AddFootnoteFragmentToList();
                //const LVRendLineInfo * save = ?:last;
                // = NULL;
                // LVE-TODO-TEST
                //if ( next != NULL ) {
                    pageend = last;
                    AddToList();
                    StartPage( NULL );
                    //StartPage( next );
                //}
            }
            footstart = footlast = line;
            footend = NULL;
            return;
        }
        if ( footstart==NULL ) {
            footstart = footlast = line;
            footend = line;
        } else {
            footend = line;
            footlast = line;
        }
    }
};

void LVRendPageContext::split()
{
    if ( !page_list )
        return;
    PageSplitState s(page_list, page_h);

    int lineCount = lines.length();


    LVRendLineInfo * line = NULL;
    for ( int lindex=0; lindex<lineCount; lindex++ ) {
        line = lines[lindex];
        s.AddLine( line );
        // add footnotes for line, if any...
        if ( line->getLinks() ) {
            s.last = line;
            s.next = lindex<lineCount-1?lines[lindex+1]:line;
            bool foundFootNote = false;
            //if ( CRLog::isTraceEnabled() && line->getLinks()->length()>0 ) {
            //    CRLog::trace("LVRendPageContext::split() line %d: found %d links", lindex, line->getLinks()->length() );
           // }
            for ( int j=0; j<line->getLinks()->length(); j++ ) {
                LVFootNote* note = line->getLinks()->get(j);
                if ( note->getLines().length() ) {
                    foundFootNote = true;
                    s.StartFootNote( note );
                    for ( int k=0; k<note->getLines().length(); k++ ) {
                        s.AddFootnoteLine( note->getLines()[k] );
                    }
                    s.EndFootNote();
                }
            }
            if ( !foundFootNote )
                line->flags = line->flags & ~RN_SPLIT_FOOT_LINK;
        }
    }
    s.Finalize();
}

void LVRendPageContext::Finalize()
{
    split();
    if ( callback ) {
        callback->OnFormatEnd();
    }
    lines.clear();
    footNotes.clear();
}

static const char * pagelist_magic = "PageList";

bool LVRendPageList::serialize( SerialBuf & buf )
{
    if ( buf.error() )
        return false;
    buf.putMagic( pagelist_magic );
    int pos = buf.pos();
    buf << (lUInt32)length();
    for ( int i=0; i<length(); i++ ) {
        get(i)->serialize( buf );
    }
    buf.putMagic( pagelist_magic );
    buf.putCRC( buf.pos() - pos );
    return !buf.error();
}

bool LVRendPageList::deserialize( SerialBuf & buf )
{
    if ( buf.error() )
        return false;
    if ( !buf.checkMagic( pagelist_magic ) )
        return false;
    clear();
    int pos = buf.pos();
    lUInt32 len;
    buf >> len;
    clear();
    reserve(len);
    for ( unsigned i=0; i<len; i++ ) {
        LVRendPageInfo * item = new LVRendPageInfo();
        item->deserialize( buf );
        item->index = i;
        add( item );
    }
    if ( !buf.checkMagic( pagelist_magic ) )
        return false;
    buf.checkCRC( buf.pos() - pos );
    return !buf.error();
}

bool LVRendPageInfo::serialize( SerialBuf & buf )
{
    if ( buf.error() )
        return false;
    buf << (lUInt32)start; /// start of page
    buf << (lUInt16)height; /// height of page, does not include footnotes
    buf << (lUInt8) type;   /// type: PAGE_TYPE_NORMAL, PAGE_TYPE_COVER
    lUInt16 len = footnotes.length();
    buf << len;
    for ( int i=0; i<len; i++ ) {
        buf << (lUInt32)footnotes[i].start;
        buf << (lUInt32)footnotes[i].height;
    }
    return !buf.error();
}

bool LVRendPageInfo::deserialize( SerialBuf & buf )
{
    if ( buf.error() )
        return false;
    lUInt32 n1;
	lUInt16 n2;
    lUInt8 n3;

    buf >> n1 >> n2 >> n3; /// start of page

    start = n1;
    height = n2;
    type = n3;

    lUInt16 len;
    buf >> len;
    footnotes.clear();
    if ( len ) {
        footnotes.reserve(len);
        for ( int i=0; i<len; i++ ) {
            lUInt32 n1;
            lUInt32 n2;
            buf >> n1;
            buf >> n2;
            footnotes.add( LVPageFootNoteInfo( n1, n2 ) );
        }
    }
    return !buf.error();
}

