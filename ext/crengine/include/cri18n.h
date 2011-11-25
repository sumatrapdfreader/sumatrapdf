/** \file cri18n.h
    \brief internationalization support, gettext wrapper

    CoolReader Engine

    (c) Vadim Lopatin, 2000-2006
    This source code is distributed under the terms of
    GNU General Public License.
    See LICENSE file for details.

*/

#ifndef __CRI18N_H_INCLUDED__
#define __CRI18N_H_INCLUDED__

#if CR_EMULATE_GETTEXT!=1 && !defined(_WIN32)
#include <libintl.h>
#endif

#include "lvstring.h"
#include "lvptrvec.h"

/// i18n interface
class CRI18NTranslator
{
protected:
	static CRI18NTranslator * _translator;
	virtual const char * getText( const char * src ) = 0;
public:
	virtual ~CRI18NTranslator() { }
	static void setTranslator( CRI18NTranslator * translator );
    static const char * translate( const char * src );
    static const lString8 translate8( const char * src );
    static const lString16 translate16( const char * src );
};

class CRMoFileTranslator : public CRI18NTranslator
{
public:
	class Item {
	public:
		lString8 src;
		lString8 dst;
		Item( lString8 srcText, lString8 dstText )
			: src(srcText), dst(dstText)
		{
		}
	protected:
		// no copy
                Item( const Item & ) { }
                Item & operator = ( const Item & ) { return *this; }
	};
protected:
	LVPtrVector<Item> _list;
	// call in src sort order only!
	virtual void add( lString8 src, lString8 dst );
	virtual const char * getText( const char * src );
	virtual void sort();
public:
	CRMoFileTranslator();
	bool openMoFile( lString16 fileName );
	virtual ~CRMoFileTranslator();
};

#ifdef _
#undef _
#endif
#ifdef _8
#undef _8
#endif
#ifdef _16
#undef _16
#endif
#define _(String) CRI18NTranslator::translate(String)
#define _8(String) CRI18NTranslator::translate8(String)
#define _16(String) CRI18NTranslator::translate16(String)


#endif
