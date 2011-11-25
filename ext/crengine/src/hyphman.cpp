/** \file hyphman.cpp
    \brief AlReader hyphenation manager

    (c) Alan, adapted TeX hyphenation dictionaries code: http://alreader.kms.ru/
    (c) Mark Lipsman -- hyphenation algorithm, modified my Mike & SeNS

    Adapted for CREngine by Vadim Lopatin

    This source code is distributed under the terms of
    GNU General Public License.

    See LICENSE file for details.

*/

// set to 0 for old hyphenation, 1 for new algorithm
#define NEW_HYPHENATION 1


#include "../include/crsetup.h"

#include <stdlib.h>
#include <string.h>
#include "../include/lvxml.h"

#if !defined(__SYMBIAN32__)
#include <stdio.h>
#include <wchar.h>
#endif

#include "../include/lvtypes.h"
#include "../include/lvstream.h"
#include "../include/hyphman.h"
#include "../include/lvfnt.h"
#include "../include/lvstring.h"


#ifdef ANDROID

#define _16(x) lString16(x)

#else

#include "../include/cri18n.h"

#endif


HyphDictionary * HyphMan::_selectedDictionary = NULL;

HyphDictionaryList * HyphMan::_dictList = NULL;

#define MAX_PATTERN_SIZE  8
#define PATTERN_HASH_SIZE 16384
class TexPattern;
class TexHyph : public HyphMethod
{
    TexPattern * table[PATTERN_HASH_SIZE];
    lUInt32 _hash;
public:
    bool match( const lChar16 * str, char * mask );
    virtual bool hyphenate( const lChar16 * str, int len, lUInt16 * widths, lUInt8 * flags, lUInt16 hyphCharWidth, lUInt16 maxWidth );
    void addPattern( TexPattern * pattern );
    TexHyph();
    virtual ~TexHyph();
    bool load( LVStreamRef stream );
    bool load( lString16 fileName );
    virtual lUInt32 getHash() { return _hash; }
};

class AlgoHyph : public HyphMethod
{
public:
    virtual bool hyphenate( const lChar16 * str, int len, lUInt16 * widths, lUInt8 * flags, lUInt16 hyphCharWidth, lUInt16 maxWidth );
    virtual ~AlgoHyph();
};

class NoHyph : public HyphMethod
{
public:
    virtual bool hyphenate( const lChar16 * str, int len, lUInt16 * widths, lUInt8 * flags, lUInt16 hyphCharWidth, lUInt16 maxWidth )
    {
        return false;
    }
    virtual ~NoHyph() { }
};

static NoHyph NO_HYPH;
static AlgoHyph ALGO_HYPH;

HyphMethod * HyphMan::_method = &NO_HYPH;

#pragma pack(push, 1)
typedef struct {
    lUInt16         wl;
    lUInt16         wu;
    char            al;
    char            au;

    unsigned char   mask0[2];
    lUInt16         aux[256];

    lUInt16         len;
} thyph;

typedef struct {
    lUInt16 start;
    lUInt16 len;
} hyph_index_item_t;
#pragma pack(pop)

void HyphMan::uninit()
{
	if ( _dictList )
		delete _dictList;
    _dictList = NULL;
	_selectedDictionary = NULL;
    if ( HyphMan::_method != &ALGO_HYPH && HyphMan::_method != &NO_HYPH )
            delete HyphMan::_method;
    _method = &NO_HYPH;
}

bool HyphMan::activateDictionaryFromStream( LVStreamRef stream )
{
    if ( stream.isNull() )
        return false;
    CRLog::trace("remove old hyphenation method");
    if ( HyphMan::_method != &NO_HYPH && HyphMan::_method != &ALGO_HYPH && HyphMan::_method ) {
        delete HyphMan::_method;
        HyphMan::_method = &NO_HYPH;
    }
    CRLog::trace("creating new TexHyph method");
    TexHyph * method = new TexHyph();
    CRLog::trace("loading from file");
    if ( !method->load( stream ) ) {
		CRLog::error("HyphMan::activateDictionaryFromStream: Cannot open hyphenation dictionary from stream" );
        delete method;
        return false;
    }
    CRLog::debug("Dictionary is loaded successfully. Activating.");
    HyphMan::_method = method;
    if ( HyphMan::_dictList->find(lString16(HYPH_DICT_ID_DICTIONARY))==NULL ) {
        HyphDictionary * dict = new HyphDictionary( HDT_DICT_ALAN, lString16("Dictionary"), lString16(HYPH_DICT_ID_DICTIONARY), lString16() );
        HyphMan::_dictList->add(dict);
    	HyphMan::_selectedDictionary = dict;
    }
    CRLog::trace("Activation is done");
    return true;
}

bool HyphMan::initDictionaries( lString16 dir )
{
	_dictList = new HyphDictionaryList();
	if ( _dictList->open( dir ) ) {
		if ( !_dictList->activate( lString16(DEF_HYPHENATION_DICT) ) )
	    	if ( !_dictList->activate( lString16(DEF_HYPHENATION_DICT2) ) )
    			_dictList->activate( lString16(HYPH_DICT_ID_ALGORITHM) );
		return true;
	} else {
		_dictList->activate( lString16(HYPH_DICT_ID_ALGORITHM) );
		return false;
	}
}

bool HyphDictionary::activate()
{
	if ( getType() == HDT_ALGORITHM ) {
		CRLog::info("Turn on algorythmic hyphenation" );
        if ( HyphMan::_method != &ALGO_HYPH ) {
            if ( HyphMan::_method != &NO_HYPH )
                delete HyphMan::_method;
            HyphMan::_method = &ALGO_HYPH;
        }
	} else if ( getType() == HDT_NONE ) {
		CRLog::info("Disabling hyphenation" );
        if ( HyphMan::_method != &NO_HYPH ) {
            if ( HyphMan::_method != &ALGO_HYPH )
                delete HyphMan::_method;
            HyphMan::_method = &NO_HYPH;
        }
	} else if ( getType() == HDT_DICT_ALAN || getType() == HDT_DICT_TEX ) {
        if ( HyphMan::_method != &NO_HYPH && HyphMan::_method != &ALGO_HYPH ) {
            delete HyphMan::_method;
            HyphMan::_method = &NO_HYPH;
        }
		CRLog::info("Selecting hyphenation dictionary %s", UnicodeToUtf8(_filename).c_str() );
		LVStreamRef stream = LVOpenFileStream( getFilename().c_str(), LVOM_READ );
		if ( stream.isNull() ) {
			CRLog::error("Cannot open hyphenation dictionary %s", UnicodeToUtf8(_filename).c_str() );
			return false;
		}
        TexHyph * method = new TexHyph();
        if ( !method->load( stream ) ) {
			CRLog::error("Cannot open hyphenation dictionary %s", UnicodeToUtf8(_filename).c_str() );
            delete method;
            return false;
        }
        HyphMan::_method = method;
	}
	HyphMan::_selectedDictionary = this;
	return true;
}

bool HyphDictionaryList::activate( lString16 id )
{
	HyphDictionary * p = find(id); 
	if ( p ) 
		return p->activate(); 
	else 
		return false;
}

void HyphDictionaryList::addDefault()
{
	if ( !find( lString16( HYPH_DICT_ID_NONE ) ) ) {
		_list.add( new HyphDictionary( HDT_NONE, _16("[No Hyphenation]"), lString16(HYPH_DICT_ID_NONE), lString16(HYPH_DICT_ID_NONE) ) );
	}
	if ( !find( lString16( HYPH_DICT_ID_ALGORITHM ) ) ) {
		_list.add( new HyphDictionary( HDT_ALGORITHM, _16("[Algorythmic Hyphenation]"), lString16(HYPH_DICT_ID_ALGORITHM), lString16(HYPH_DICT_ID_ALGORITHM) ) );
	}
		
}

HyphDictionary * HyphDictionaryList::find( lString16 id )
{
	for ( int i=0; i<_list.length(); i++ ) {
		if ( _list[i]->getId() == id )
			return _list[i];
	}
	return NULL;
}

bool HyphDictionaryList::open( lString16 hyphDirectory )
{
    CRLog::info("HyphDictionaryList::open(%s)", LCSTR(hyphDirectory) );
    _list.clear();
    addDefault();
    if ( hyphDirectory.empty() )
	return true;
    //LVAppendPathDelimiter( hyphDirectory );
    LVContainerRef container;
    LVStreamRef stream;
    if ( (hyphDirectory.endsWith(lString16(L"/")) || hyphDirectory.endsWith(lString16(L"\\"))) && LVDirectoryExists(hyphDirectory) ) {
        container = LVOpenDirectory( hyphDirectory.c_str(), L"*.*" );
    } else if ( LVFileExists(hyphDirectory) ) {
        stream = LVOpenFileStream( hyphDirectory.c_str(), LVOM_READ );
        if ( !stream.isNull() )
            container = LVOpenArchieve( stream );
    }

	if ( !container.isNull() ) {
		int len = container->GetObjectCount();
        int count = 0;
        CRLog::info("%d items found in hyph directory");
		for ( int i=0; i<len; i++ ) {
			const LVContainerItemInfo * item = container->GetObjectInfo( i );
			lString16 name = item->GetName();
            lString16 suffix;
            HyphDictType t = HDT_NONE;
            if ( name.endsWith(lString16(".pdb")) ) {
                suffix = L"_hyphen_(Alan).pdb";
                t = HDT_DICT_ALAN;
            } else if ( name.endsWith(lString16(".pattern")) ) {
                suffix = L".pattern";
                t = HDT_DICT_TEX;
            } else
                continue;
            


			lString16 filename = hyphDirectory + name;
			lString16 id = name;
			lString16 title = name;
			if ( title.endsWith( suffix ) )
				title.erase( title.length() - suffix.length(), suffix.length() );
            
			_list.add( new HyphDictionary( t, title, id, filename ) );
            count++;
		}
        CRLog::info("%d dictionaries added to list");
		return true;
	}
	return false;
}

HyphMan::HyphMan()
{
}

HyphMan::~HyphMan()
{
}

struct tPDBHdr
{
    char filename[36];
    lUInt32 dw1;
    lUInt32 dw2;
    lUInt32 dw4[4];
    char type[8];
    lUInt32 dw44;
    lUInt32 dw48;
    lUInt16 numrec;
};

static int isCorrectHyphFile(LVStream * stream)
{
    if (!stream)
        return false;
    lvsize_t   dw;
    int    w = 0;
    tPDBHdr    HDR;
    stream->SetPos(0);
    stream->Read( &HDR, 78, &dw);
    stream->SetPos(0);
    lvByteOrderConv cnv;
    w=cnv.msf(HDR.numrec);
    if (dw!=78 || w>0xff) 
        w = 0;

    if (strncmp((const char*)&HDR.type, "HypHAlR4", 8)) 
        w = 0;
        
    return w;
}

class TexPattern {
public:
    lChar16 word[MAX_PATTERN_SIZE];
    char attr[MAX_PATTERN_SIZE+1];
    TexPattern * next;

    int cmp( TexPattern * v )
    {
        return lStr_cmp( word, v->word );
    }

    static int hash( const lChar16 * s )
    {
        return (((s[0] *31 + s[1])*31 + s[2]) * 31 + s[3]) % PATTERN_HASH_SIZE;
    }

    static int hash3( const lChar16 * s )
    {
        return (((s[0] *31 + s[1])*31 + s[2]) * 31 + 0) % PATTERN_HASH_SIZE;
    }

    static int hash2( const lChar16 * s )
    {
        return (((s[0] *31 + s[1])*31 + 0) * 31 + 0) % PATTERN_HASH_SIZE;
    }

    int hash()
    {
        return (((word[0] *31 + word[1])*31 + word[2]) * 31 + word[3]) % PATTERN_HASH_SIZE;
    }

    bool match( const lChar16 * s, char * mask )
    {
        TexPattern * p = this;
        bool found = false;
        while ( p ) {
            bool res = true;
            for ( int i=0; p->word[i]; i++ )
                if ( p->word[i]!=s[i] ) {
                    res = false;
                    break;
                }
            if ( res ) {
                apply(mask);
                found = true;
            }
            p = p->next;
        }
        return found;
    }

    void apply( char * mask )
    {
        ;
        for ( char * p = attr; *p && *mask; p++, mask++ ) {
            if ( *mask < *p )
                *mask = *p;
        }
    }

    TexPattern( const lString16 &s ) : next( NULL )
    {
        memset( word, 0, sizeof(word) );
        memset( attr, 0, sizeof(attr) );
        int n = 0;
        for ( int i=0; i<(int)s.length() && n<MAX_PATTERN_SIZE; i++ ) {
            lChar16 ch = s[i];
            if ( ch>='0' && ch<='9' ) {
                attr[n] = (char)ch;
            } else {
                word[n++] = ch;
            }
        }
    }

    TexPattern( const unsigned char * s, int sz, const lChar16 * charMap )
    {
        if ( sz >= MAX_PATTERN_SIZE )
            sz = MAX_PATTERN_SIZE - 1;
        memset( word, 0, sizeof(word) );
        memset( attr, 0, sizeof(attr) );
        for ( int i=0; i<sz; i++ )
            word[i] = charMap[ s[i] ];
        memcpy( attr, s+sz, sz+1 );
    }
};

class HyphPatternReader : public LVXMLParserCallback
{
protected:
    bool insidePatternTag;
    lString16Collection & data;
public:
    HyphPatternReader(lString16Collection & result) : insidePatternTag(false), data(result)
    {
        result.clear();
    }
    /// called on parsing end
    virtual void OnStop() { }
    /// called on opening tag end
    virtual void OnTagBody() {}
    /// called on opening tag
    virtual ldomNode * OnTagOpen( const lChar16 * nsname, const lChar16 * tagname)
    {
        if ( !lStr_cmp(tagname, L"pattern") ) {
            insidePatternTag = true;
        }
        return NULL;
    }
    /// called on closing
    virtual void OnTagClose( const lChar16 * nsname, const lChar16 * tagname )
    {
        insidePatternTag = false;
    }
    /// called on element attribute
    virtual void OnAttribute( const lChar16 * nsname, const lChar16 * attrname, const lChar16 * attrvalue )
    {
    }
    /// called on text
    virtual void OnText( const lChar16 * text, int len, lUInt32 flags )
    {
        if ( insidePatternTag )
            data.add( lString16(text, len) );
    }
};

TexHyph::TexHyph()
{
    memset( table, 0, sizeof(table) );
    _hash = 123456;
}

TexHyph::~TexHyph()
{
    for ( int i=0; i<PATTERN_HASH_SIZE; i++ ) {
        TexPattern * p = table[i];
        while (p) {
            TexPattern * tmp = p;
            p = p->next;
            delete tmp;
        }
    }
}

void TexHyph::addPattern( TexPattern * pattern )
{
    int h = pattern->hash();
    TexPattern * * p = &table[h];
    while ( *p && pattern->cmp(*p)<0 )
        p = &((*p)->next);
    pattern->next = *p;
    *p = pattern;
}

bool TexHyph::load( LVStreamRef stream )
{
    int w = isCorrectHyphFile(stream.get());
    int patternCount = 0;
    if (w) {
        _hash = stream->crc32();
        int        i;
        lvsize_t   dw;

        lvByteOrderConv cnv;

        int hyph_count = w;
        thyph hyph;

        lvpos_t p = 78 + (hyph_count * 8 + 2);
        stream->SetPos(p);
        if ( stream->SetPos(p)!=p )
            return false;
        lChar16 charMap[256];
        unsigned char buf[0x10000];
        memset( charMap, 0, sizeof( charMap ) );
        // make char map table
        for (i=0; i<hyph_count; i++)
        {
            if ( stream->Read( &hyph, 522, &dw )!=LVERR_OK || dw!=522 ) 
                return false;
            cnv.msf( &hyph.len ); //rword(_main_hyph[i].len);
            lvpos_t newPos;
            if ( stream->Seek( hyph.len, LVSEEK_CUR, &newPos )!=LVERR_OK )
                return false;

            cnv.msf( hyph.wl );
            cnv.msf( hyph.wu );
            charMap[ (unsigned char)hyph.al ] = hyph.wl;
            charMap[ (unsigned char)hyph.au ] = hyph.wu;
        }

        if ( stream->SetPos(p)!=p )
            return false;

        for (i=0; i<hyph_count; i++)
        {
            stream->Read( &hyph, 522, &dw );
            if (dw!=522) 
                return false;
            cnv.msf( &hyph.len );

            stream->Read(buf, hyph.len, &dw); 
            if (dw!=hyph.len)
                return false;

            unsigned char * p = buf;
            unsigned char * end_p = p + hyph.len;
            while ( p < end_p ) {
                lUInt8 sz = *p++;
                if ( p + sz > end_p )
                    break;
                TexPattern * pattern = new TexPattern( p, sz, charMap );
                //CRLog::debug("Pattern: '%s' - %s", LCSTR(lString16(pattern->word)), pattern->attr );
                addPattern( pattern );
                patternCount++;
                p += sz + sz + 1;
            }
        }

        return patternCount>0;
    } else {
        // tex xml format as for FBReader
        lString16Collection data;
        HyphPatternReader reader( data );
        LVXMLParser parser( stream, &reader );
        if ( !parser.CheckFormat() )
            return false;
        if ( !parser.Parse() )
            return false;
        if ( !data.length() )
            return false;
        for ( int i=0; i<(int)data.length(); i++ ) {
            TexPattern * pattern = new TexPattern( data[i] );
            //CRLog::debug("Pattern: '%s' - %s", LCSTR(lString16(pattern->word)), pattern->attr );
            addPattern( pattern );
            patternCount++;
        }
        return patternCount>0;
    }
}

bool TexHyph::load( lString16 fileName )
{
    LVStreamRef stream = LVOpenFileStream( fileName.c_str(), LVOM_READ );
    if ( stream.isNull() )
        return false;
    return load( stream );
}


bool TexHyph::match( const lChar16 * str, char * mask )
{
    bool found = false;
    TexPattern * res = table[ TexPattern::hash( str ) ];
    if ( res ) {
        found = res->match( str, mask ) || found;
    }
    res = table[ TexPattern::hash3( str ) ];
    if ( res ) {
        found = res->match( str, mask ) || found;
    }
    res = table[ TexPattern::hash2( str ) ];
    if ( res ) {
        found = res->match( str, mask ) || found;
    }
    return found;
}

bool TexHyph::hyphenate( const lChar16 * str, int len, lUInt16 * widths, lUInt8 * flags, lUInt16 hyphCharWidth, lUInt16 maxWidth )
{
    if ( len<=3 )
        return false;
    if ( len>WORD_LENGTH )
        len = WORD_LENGTH - 2;
    lChar16 word[WORD_LENGTH+3];
    char mask[WORD_LENGTH+3];
    word[0] = ' ';
    lStr_memcpy( word+1, str, len );
    lStr_lowercase( word+1, len );
    word[len+1] = ' ';
    word[len+2] = 0;
    word[len+3] = 0;
    word[len+4] = 0;
    memset( mask, '0', len+3 );
    mask[len+3] = 0;
    bool found = false;
    for ( int i=0; i<len-1; i++ ) {
        found = match( word + i, mask + i ) | found;
    }
    if ( !found )
        return false;
    int p=0;
    for ( p=len-3; p>=1; p-- ) {
        // hyphenate
        //00010030100
        int nw = widths[p]+hyphCharWidth;
        int bestp = -1;
        int bestm = '0';
        if ( (mask[p+2]&1) && nw <= maxWidth ) {
            if ( bestp<0 || mask[p+2]>bestm ) {
                bestp = p;
                bestm = mask[p+2];
            }
        }
        if ( bestp>=0 ) {
            widths[bestp] = nw;
            flags[bestp] |= LCHAR_ALLOW_HYPH_WRAP_AFTER;
            return true;
        }
    }
    return false;
}

bool AlgoHyph::hyphenate( const lChar16 * str, int len, lUInt16 * widths, lUInt8 * flags, lUInt16 hyphCharWidth, lUInt16 maxWidth )
{
    lUInt16 chprops[WORD_LENGTH];
    lStr_getCharProps( str, len, chprops );
    int start, end, i, j;
    #define MIN_WORD_LEN_TO_HYPHEN 2
    for ( start = 0; start<len; ) {
        // find start of word
        while (start<len && !(chprops[start] & CH_PROP_ALPHA) )
            ++start;
        // find end of word
        for ( end=start+1; end<len && (chprops[start] & CH_PROP_ALPHA); ++end )
            ;
        // now look over word, placing hyphens
        if ( end-start > MIN_WORD_LEN_TO_HYPHEN ) { // word must be long enough
            for (i=start;i<end-MIN_WORD_LEN_TO_HYPHEN;++i) {
                if ( widths[i] > maxWidth )
                    break;
                if ( chprops[i] & CH_PROP_VOWEL ) {
                    for ( j=i+1; j<end; ++j ) {
                        if ( chprops[j] & CH_PROP_VOWEL ) {
                            if ( (chprops[i+1] & CH_PROP_CONSONANT) && (chprops[i+2] & CH_PROP_CONSONANT) )
                                ++i;
                            else if ( (chprops[i+1] & CH_PROP_CONSONANT) && ( chprops[i+2] & CH_PROP_ALPHA_SIGN ) )
                                i += 2;
                            if ( i-start>=1 && end-i>2 ) {
                                // insert hyphenation mark
                                lUInt16 nw = widths[i] + hyphCharWidth;
                                if ( nw<maxWidth )
                                {
                                    flags[i] |= LCHAR_ALLOW_HYPH_WRAP_AFTER;
                                    widths[i] = nw;
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
        start=end;
    }
    return true;
}

AlgoHyph::~AlgoHyph()
{
}



