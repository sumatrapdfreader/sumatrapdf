/** \file hyphman.h
    \brief AlReader hyphenation manager

    (c) Alan, http://alreader.kms.ru/

    Adapted for CREngine by Vadim Lopatin

    This source code is distributed under the terms of
    GNU General Public License.

    See LICENSE file for details.

*/

#ifndef _HYPHEN_
#define _HYPHEN_

#include "lvtypes.h"
#include "lvstream.h"

class HyphMethod
{
public:
    virtual bool hyphenate( const lChar16 * str, int len, lUInt16 * widths, lUInt8 * flags, lUInt16 hyphCharWidth, lUInt16 maxWidth ) = 0;
    virtual ~HyphMethod() { }
};


#define WORD_LENGTH   64
#define MAX_REAL_WORD 24


enum HyphDictType
{
	HDT_NONE,      // disable hyphenation
	HDT_ALGORITHM, // universal
	HDT_DICT_ALAN, // tex/alreader
	HDT_DICT_TEX,  // tex/fbreader
};

class HyphDictionary
{
	HyphDictType _type;
	lString16 _title;
	lString16 _id;
	lString16 _filename;
public:
	HyphDictionary( HyphDictType type, lString16 title, lString16 id, lString16 filename )
		: _type(type), _title(title), _id( id ), _filename( filename ) { }
	HyphDictType getType() { return _type; }
	lString16 getTitle() { return _title; }
	lString16 getId() { return _id; }
	lString16 getFilename() { return _filename; }
	bool activate();
	virtual lUInt32 getHash() { return getTitle().getHash(); }
};

#define HYPH_DICT_ID_NONE L"@none"
#define HYPH_DICT_ID_ALGORITHM L"@algorithm"
#define HYPH_DICT_ID_DICTIONARY L"@dictionary"


class HyphDictionaryList
{
	LVPtrVector<HyphDictionary> _list;
	void addDefault();
public:
    void add(HyphDictionary * dict) { _list.add(dict); }
	int length() { return _list.length(); }
	HyphDictionary * get( int index ) { return (index>=0 && index<+_list.length()) ? _list[index] : NULL; }
	HyphDictionaryList() { addDefault(); }
	bool open( lString16 hyphDirectory );
	HyphDictionary * find( lString16 id );
	bool activate( lString16 id );
};

#define DEF_HYPHENATION_DICT "Russian_EnUS_hyphen_(Alan).pdb"
#define DEF_HYPHENATION_DICT2 "ru.pattern"

class HyphDictionary;
class HyphDictionaryList;

/// hyphenation manager
class HyphMan
{
	friend class HyphDictionary;
    static HyphMethod * _method;
	static HyphDictionary * _selectedDictionary;
	static HyphDictionaryList * _dictList;
public:
	static void uninit();
    static bool activateDictionaryFromStream( LVStreamRef stream );
	static HyphDictionaryList * getDictList() { return _dictList; }
    static bool activateDictionary( lString16 id ) { return _dictList->activate(id); }
	static bool initDictionaries( lString16 dir );
	static HyphDictionary * getSelectedDictionary() { return _selectedDictionary; }

    HyphMan();
    ~HyphMan();

    inline static bool hyphenate( const lChar16 * str, int len, lUInt16 * widths, lUInt8 * flags, lUInt16 hyphCharWidth, lUInt16 maxWidth )
    {
        return _method->hyphenate( str, len, widths, flags, hyphCharWidth, maxWidth );
    }
};



#endif
