/** \file props.cpp
    \brief properties container

    CoolReader Engine

    (c) Vadim Lopatin, 2000-2007
    This source code is distributed under the terms of
    GNU General Public License
    See LICENSE file for details
*/

#include "../include/props.h"
#include <stdio.h>


//============================================================================
// CRPropContainer declarations
//============================================================================

class CRPropItem
{
private:
    lString8 _name;
    lString16 _value;
public:
    CRPropItem( const char * name, const lString16 value )
    : _name(name), _value(value)
    { }
    CRPropItem( const CRPropItem& v )
    : _name( v._name )
    , _value( v._value )
    { }
    CRPropItem & operator = ( const CRPropItem& v )
    {
      _name = v._name;
      _value = v._value;
      return *this;
    }
    const char * getName() const { return _name.c_str(); }
    const lString16 & getValue() const { return _value; }
    void setValue(const lString16 &v) { _value = v; }
};

/// set contents from specified properties
void CRPropAccessor::set( const CRPropRef & v )
{
    clear();
    int sz = v->getCount();
    for ( int i=0; i<sz; i++ )
        setString( v->getName(i), v->getValue(i) );
}

/// calc props1 - props2
CRPropRef operator - ( CRPropRef props1, CRPropRef props2 )
{
    CRPropRef v = LVCreatePropsContainer();
    int cnt1 = props1->getCount();
    int cnt2 = props2->getCount();
    int p1 = 0;
    int p2 = 0;
    while ( (p1<=cnt1 && p2<=cnt2) && (p1<cnt1 || p2<cnt2) ) {
        if ( p1==cnt1 ) {
            break;
        } else if ( p2==cnt2 ) {
            v->setString( props1->getName( p1 ), props1->getValue( p1 ) );
            p1++;
        } else {
            int res = lStr_cmp( props1->getName( p1 ), props2->getName( p2 ) );
            if ( res<0 ) {
                v->setString( props1->getName( p1 ), props1->getValue( p1 ) );
                p1++;
            } else if ( res==0 ) {
                p1++;
                p2++;
            } else { // ( res>0 )
                p2++;
            }
        }
    }
    return v;
}
/// calc props1 | props2
CRPropRef operator | ( CRPropRef props1, CRPropRef props2 )
{
    CRPropRef v = LVCreatePropsContainer();
    int cnt1 = props1->getCount();
    int cnt2 = props2->getCount();
    int p1 = 0;
    int p2 = 0;
    while ( (p1<=cnt1 && p2<=cnt2) && (p1<cnt1 || p2<cnt2) ) {
        if ( p1==cnt1 ) {
            v->setString( props2->getName( p2 ), props2->getValue( p2 ) );
            p2++;
        } else if ( p2==cnt2 ) {
            v->setString( props1->getName( p1 ), props1->getValue( p1 ) );
            p1++;
        } else {
            int res = lStr_cmp( props1->getName( p1 ), props2->getName( p2 ) );
            if ( res<0 ) {
                v->setString( props1->getName( p1 ), props1->getValue( p1 ) );
                p1++;
            } else if ( res==0 ) {
                v->setString( props1->getName( p1 ), props1->getValue( p1 ) );
                p1++;
                p2++;
            } else { // ( res>0 )
                v->setString( props2->getName( p2 ), props2->getValue( p2 ) );
                p2++;
            }
        }
    }
    return v;
}
/// calc props1 & props2
CRPropRef operator & ( CRPropRef props1, CRPropRef props2 )
{
    CRPropRef v = LVCreatePropsContainer();
    int cnt1 = props1->getCount();
    int cnt2 = props2->getCount();
    int p1 = 0;
    int p2 = 0;
    while ( (p1<=cnt1 && p2<=cnt2) && (p1<cnt1 || p2<cnt2) ) {
        if ( p1==cnt1 ) {
            break;
        } else if ( p2==cnt2 ) {
            break;
        } else {
            int res = lStr_cmp( props1->getName( p1 ), props2->getName( p2 ) );
            if ( res<0 ) {
                p1++;
            } else if ( res==0 ) {
                v->setString( props1->getName( p1 ), props1->getValue( p1 ) );
                p1++;
                p2++;
            } else { // ( res>0 )
                p2++;
            }
        }
    }
    return v;
}

/// returns added or changed items of props2 compared to props1
CRPropRef operator ^ ( CRPropRef props1, CRPropRef props2 )
{
    CRPropRef v = LVCreatePropsContainer();
    int cnt1 = props1->getCount();
    int cnt2 = props2->getCount();
    int p1 = 0;
    int p2 = 0;
    while ( (p1<=cnt1 && p2<=cnt2) && (p1<cnt1 || p2<cnt2) ) {
        if ( p1==cnt1 ) {
            v->setString( props2->getName( p2 ), props2->getValue( p2 ) );
            p2++;
        } else if ( p2==cnt2 ) {
            break;
        } else {
            int res = lStr_cmp( props1->getName( p1 ), props2->getName( p2 ) );
            if ( res<0 ) {
                p1++;
            } else if ( res==0 ) {
                lString16 v1 = props1->getValue( p1 );
                lString16 v2 = props2->getValue( p2 );
                if ( v1!=v2 )
                    v->setString( props2->getName( p2 ), v2 );
                p1++;
                p2++;
            } else { // ( res>0 )
                v->setString( props2->getName( p2 ), props2->getValue( p2 ) );
                p2++;
            }
        }
    }
    return v;
}

class CRPropContainer : public CRPropAccessor
{
    friend class CRPropSubContainer;
private:
    LVPtrVector<CRPropItem> _list;
protected:
    lInt64 _revision;
    bool findItem( const char * name, int nameoffset, int start, int end, int & pos ) const;
    bool findItem( const char * name, int & pos ) const;
    void clear( int start, int end );
    CRPropContainer( const CRPropContainer & v )
    : _list( v._list )
    {
    }
public:
    /// get copy of property list
    virtual CRPropRef clone() const
    {
        return CRPropRef( new CRPropContainer(*this) );
    }
    /// returns true if specified property exists
    virtual bool hasProperty( const char * propName ) const
    {
        int pos;
        return findItem( propName, pos );
    }
    /// clear all items
    virtual void clear();
    /// returns property path in root container
    virtual const lString8 & getPath() const;
    /// returns property item count in container
    virtual int getCount() const;
    /// returns property name by index
    virtual const char * getName( int index ) const;
    /// returns property value by index
    virtual const lString16 & getValue( int index ) const;
    /// sets property value by index
    virtual void setValue( int index, const lString16 &value );
    /// get string property by name, returns false if not found
    virtual bool getString( const char * propName, lString16 &result ) const;
    /// set string property by name
    virtual void setString( const char * propName, const lString16 &value );
    /// get subpath container
    virtual CRPropRef getSubProps( const char * path );
    /// constructor
    CRPropContainer();
    /// virtual destructor
    virtual ~CRPropContainer();
};

/// set string property by name, if it's not set already
void CRPropAccessor::setStringDef( const char * propName, const char * defValue )
{
    if ( !hasProperty( propName ) )
        setString( propName, Utf8ToUnicode( lString8( defValue ) ) );
}

/// set int property by name, if it's not set already
void CRPropAccessor::setIntDef( const char * propName, int value )
{
    if ( !hasProperty( propName ) )
        setInt( propName, value );
}

void CRPropAccessor::limitValueList( const char * propName, const char * values[] )
{
    lString16 defValue = Utf8ToUnicode( lString8( values[0] ) );
    lString16 value;
    if ( getString( propName, value ) ) {
        for ( int i=0; values[i]; i++ ) {
            lString16 v = Utf8ToUnicode( lString8( values[i] ) );
            if ( v==value )
                return;
        }
    }
    setString( propName, defValue );
}

void CRPropAccessor::limitValueList( const char * propName, int values[], int value_count )
{
    lString16 defValue = lString16::itoa( values[0] );
    lString16 value;
    if ( getString( propName, value ) ) {
        for ( int i=0; i < value_count; i++ ) {
            lString16 v = lString16::itoa( values[i] );
            if ( v==value )
                return;
        }
    }
    setString( propName, defValue );
}

//============================================================================
// CRPropAccessor methods
//============================================================================

lString16 CRPropAccessor::getStringDef( const char * propName, const char * defValue ) const
{
    lString16 value;
    if ( !getString( propName, value ) )
        return lString16( defValue );
    else
        return value;
}

bool CRPropAccessor::getInt( const char * propName, int &result ) const
{
    lString16 value;
    if ( !getString( propName, value ) )
        return false;
    return value.atoi(result);
}

int CRPropAccessor::getIntDef( const char * propName, int defValue ) const
{
    int v = 0;
    if ( !getInt( propName, v ) )
        return defValue;
    else
        return v;
}

/// set int property as hex
void CRPropAccessor::setHex( const char * propName, int value )
{
    char s[16];
    sprintf(s, "0x%08X", value);
    setString( propName, Utf8ToUnicode(lString8(s)) );
}

void CRPropAccessor::setInt( const char * propName, int value )
{
    setString( propName, lString16::itoa( value ) );
}

/// get color (#xxxxxx) property by name, returns false if not found
bool CRPropAccessor::getColor( const char * propName, lUInt32 &result ) const
{
    unsigned n = 0;
    lString16 value;
    if ( !getString( propName, value ) || value.empty() || value[0]!='#' )
        return false;
    for ( unsigned i=1; i<value.length(); i++ ) {
        lChar16 ch = value[i];
        if ( ch>='0' && ch<='9' )
            n = (n << 4) | (ch - '0');
        else if ( ch>='a' && ch<='F' )
            n = (n << 4) | (ch - 'a' + 10);
        else if ( ch>='A' && ch<='F' )
            n = (n << 4) | (ch - 'A' + 10);
        else
            return false;
    }
    result = (lUInt32)n;
    return true;
}

/// get color (#xxxxxx) property by name, returns default value if not found
lUInt32 CRPropAccessor::getColorDef( const char * propName, lUInt32 defValue ) const
{
    lUInt32 v = 0;
    if ( !getColor( propName, v ) )
        return defValue;
    else
        return v;
}

/// set color (#xxxxxx) property by name
void CRPropAccessor::setColor( const char * propName, lUInt32 value )
{
    char s[12];
    sprintf( s, "#%06x", (int)value );
    setString( propName, lString16( s ) );
}

/// get rect property by name, returns false if not found
bool CRPropAccessor::getRect( const char * propName, lvRect &result ) const
{
    lString16 value;
    if ( !getString( propName, value ) )
        return false;
    lString8 s8 = UnicodeToUtf8( value );
    int n[4];
    if ( sscanf( s8.c_str(), "{%d,%d,%d,%d}", n, n+1, n+2, n+3 )!=4 )
        return false;
    result.left = n[0];
    result.top = n[1];
    result.right = n[2];
    result.bottom = n[3];
    return true;
}

/// get rect property by name, returns default value if not found
lvRect CRPropAccessor::getRectDef( const char * propName, const lvRect & defValue ) const
{
    lvRect v;
    if ( !getRect( propName, v ) )
        return defValue;
    else
        return v;
}

/// set rect property by name
void CRPropAccessor::setRect( const char * propName, const lvRect & value )
{
    char s[64];
    sprintf( s, "{%d,%d,%d,%d}", value.left, value.top, value.right, value.bottom );
    setString( propName, lString16( s ) );
}

/// get point property by name, returns false if not found
bool CRPropAccessor::getPoint( const char * propName, lvPoint &result ) const
{
    lString16 value;
    if ( !getString( propName, value ) )
        return false;
    lString8 s8 = UnicodeToUtf8( value );
    int n[2];
    if ( sscanf( s8.c_str(), "{%d,%d}", n, n+1)!=2 )
        return false;
    result.x = n[0];
    result.y = n[1];
    return true;
}

/// get point property by name, returns default value if not found
lvPoint CRPropAccessor::getPointDef( const char * propName, const lvPoint & defValue ) const
{
    lvPoint v;
    if ( !getPoint( propName, v ) )
        return defValue;
    else
        return v;
}

/// set point property by name
void CRPropAccessor::setPoint( const char * propName, const lvPoint & value )
{
    char s[64];
    sprintf( s, "{%d,%d}", value.x, value.y );
    setString( propName, lString16( s ) );
}

bool CRPropAccessor::getBool( const char * propName, bool &result ) const
{
    lString16 value;
    if ( !getString( propName, value ) )
        return false;
    if ( value == L"true" || value == L"TRUE" || value == L"yes" || value == L"YES" || value == L"1" ) {
        result = true;
        return true;
    }
    if ( value == L"false" || value == L"FALSE" || value == L"no" || value == L"NO" || value == L"0" ) {
        result = false;
        return true;
    }
    return false;
}

bool CRPropAccessor::getBoolDef( const char * propName, bool defValue ) const
{
    bool v = 0;
    if ( !getBool( propName, v ) )
        return defValue;
    else
        return v;
}

void CRPropAccessor::setBool( const char * propName, bool value )
{
    setString( propName, lString16( value ? L"1" : L"0" ) );
}

bool CRPropAccessor::getInt64( const char * propName, lInt64 &result ) const
{
    lString16 value;
    if ( !getString( propName, value ) )
        return false;
    return value.atoi(result);
}

lInt64 CRPropAccessor::getInt64Def( const char * propName, lInt64 defValue ) const
{
    lInt64 v = 0;
    if ( !getInt64( propName, v ) )
        return defValue;
    else
        return v;
}

void CRPropAccessor::setInt64( const char * propName, lInt64 value )
{
    setString( propName, lString16::itoa( value ) );
}

CRPropAccessor::~CRPropAccessor()
{
}

static lString8 addBackslashChars( lString8 str )
{
    unsigned i;
    bool found = false;
    for ( i=0; i<str.length(); i++ ) {
        char ch = str[i];
        if ( ch =='\\' || ch=='\r' || ch=='\n' || ch=='\0' ) {
            found = true;
            break;
        }
    }
    if ( !found )
        return str;
    lString8 out;
    out.reserve( str.length() + 1 );
    for ( i=0; i<str.length(); i++ ) {
        char ch = str[i];
        switch ( ch ) {
        case '\\':
            out << "\\\\";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\0':
            out << "\\0";
            break;
        default:
            out << ch;
        }
    }
    return out;
}

static lString8 removeBackslashChars( lString8 str )
{
    unsigned i;
    bool found = false;
    for ( i=0; i<str.length(); i++ ) {
        char ch = str[i];
        if ( ch =='\\' ) {
            found = true;
            break;
        }
    }
    if ( !found )
        return str;
    lString8 out;
    out.reserve( str.length() + 1 );
    for ( i=0; i<str.length(); i++ ) {
        char ch = str[i];
        if ( ch=='\\' ) {
            ch = str[++i];
            switch ( ch ) {
            case 'r':
                out << '\r';
                break;
            case 'n':
                out << '\n';
                break;
            case '0':
                out << '\0';
                break;
            default:
                out << ch;
            }
        } else {
            out << ch;
        }
    }
    return out;
}

/// read from stream
bool CRPropAccessor::loadFromStream( LVStream * stream )
{
    if ( !stream || stream->GetMode()!=LVOM_READ )
        return false;
    lvsize_t sz = stream->GetSize() - stream->GetPos();
    if ( sz<=0 )
        return false;
    char * buf = new char[sz + 3];
    lvsize_t bytesRead = 0;
    if ( stream->Read( buf, sz, &bytesRead )!=LVERR_OK ) {
        delete[] buf;
        return false;
    }
    buf[sz] = 0;
    char * p = buf;
    if( (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1]==0xBB && (unsigned char)buf[2]==0xBF )
        p += 3;
    // read lines from buffer
    while (*p) {
        char * elp = p;
        char * eqpos = NULL;
        while ( *elp && !(elp[0]=='\r' && elp[1]=='\n')  && !(elp[0]=='\n') ) {
            if ( *elp == '=' && eqpos==NULL )
                eqpos = elp;
            elp++;
        }
        if ( eqpos!=NULL && eqpos>p && *elp!='#' ) {
            lString8 name( p, eqpos-p );
            lString8 value( eqpos+1, elp - eqpos - 1);
            setString( name.c_str(), Utf8ToUnicode(removeBackslashChars(value)) );
        }
        for ( p=elp; *elp && *elp!='\r' && *elp!='\n'; elp++)
            ;
        p = elp;
		while ( *p=='\r' || *p=='\n' )
			p++;
    }
    // cleanup
    delete[] buf;
    return true;
}

/// save to stream
bool CRPropAccessor::saveToStream( LVStream * targetStream )
{
    if ( !targetStream || targetStream->GetMode()!=LVOM_WRITE )
        return false;
    LVStreamRef streamref = LVCreateMemoryStream(NULL, 0, false, LVOM_WRITE);
    LVStream * stream = streamref.get();

    *stream << "\xEF\xBB\xBF";
    for ( int i=0; i<getCount(); i++ ) {
        *stream << getPath() << getName(i) << "=" << addBackslashChars(UnicodeToUtf8(getValue(i))) << "\r\n";
    }
    LVPumpStream( targetStream, stream );
    return true;
}

//============================================================================
// CRPropContainer methods
//============================================================================

CRPropContainer::CRPropContainer()
: _revision(0)
{
}

/// returns property path in root container
const lString8 & CRPropContainer::getPath() const
{
    return lString8::empty_str;
}

/// returns property item count in container
int CRPropContainer::getCount() const
{
    return _list.length();
}

/// returns property name by index
const char * CRPropContainer::getName( int index ) const
{
    return _list[index]->getName();
}

/// returns property value by index
const lString16 & CRPropContainer::getValue( int index ) const
{
    return _list[index]->getValue();
}

/// sets property value by index
void CRPropContainer::setValue( int index, const lString16 &value )
{
    _list[index]->setValue( value );
}

/// binary search
bool CRPropContainer::findItem( const char * name, int nameoffset, int start, int end, int & pos ) const
{
    int a = start;
    int b = end;
    while ( a < b ) {
        int c = (a + b) / 2;
        int res = lStr_cmp( name, _list[c]->getName() + nameoffset );
        if ( res == 0 ) {
            pos = c;
            return true;
        } else if ( res<0 ) {
            b = c;
        } else {
            a = c + 1;
        }
    }
    pos = a;
    return false;
}

/// binary search
bool CRPropContainer::findItem( const char * name, int & pos ) const
{
    return findItem( name, 0, 0, _list.length(), pos );
}

/// get string property by name, returns false if not found
bool CRPropContainer::getString( const char * propName, lString16 &result ) const
{
    int pos = 0;
    if ( !findItem( propName, pos ) )
        return false;
    result = _list[pos]->getValue();
    return true;
}

/// clear all items
void CRPropContainer::clear()
{
    _list.clear();
    _revision++;
}

/// set string property by name
void CRPropContainer::setString( const char * propName, const lString16 &value )
{
    int pos = 0;
    if ( !findItem( propName, pos ) ) {
        _list.insert( pos, new CRPropItem( propName, value ) );
        _revision++;
    } else {
        _list[pos]->setValue( value );
    }
}

/// virtual destructor
CRPropContainer::~CRPropContainer()
{
}

void CRPropContainer::clear( int start, int end )
{
    _list.erase( start, end-start );
    _revision++;
}

//============================================================================
// CRPropSubContainer methods
//============================================================================

class CRPropSubContainer : public CRPropAccessor
{
private:
    CRPropContainer * _root;
    lString8 _path;
    int _start;
    int _end;
    lInt64 _revision;
protected:
    void sync() const
    {
        CRPropSubContainer * ptr = (CRPropSubContainer*) this;
        if ( ptr->_revision != ptr->_root->_revision ) {
            ptr->_root->findItem( ptr->_path.c_str(), ptr->_start );
            ptr->_root->findItem( (ptr->_path + "\x7F").c_str(), ptr->_end );
            ptr->_revision = ptr->_root->_revision;
        }
    }
public:
    /// get copy of property list
    virtual CRPropRef clone() const
    {
        CRPropContainer * v = new CRPropContainer();
        int cnt = getCount();
        v->_list.reserve(cnt);
        for ( int i=0; i<cnt; i++ ) {
            v->_list.add( new CRPropItem( getName(i), getValue(i) ) );
        }
        return CRPropRef( v );
    }
    /// returns true if specified property exists
    virtual bool hasProperty( const char * propName ) const
    {
        lString16 str;
        return getString( propName, str );
    }
    CRPropSubContainer(CRPropContainer * root, lString8 path)
    : _root(root), _path(path), _start(0), _end(0), _revision(0)
    {
        sync();
    }
    /// clear all items
    virtual void clear()
    {
        sync();
        _root->clear( _start, _end );
    }
    /// returns property path in root container
    virtual const lString8 & getPath() const
    {
        return _path;
    }
    /// returns property item count in container
    virtual int getCount() const
    {
        sync();
        return _end - _start;
    }
    /// returns property name by index
    virtual const char * getName( int index ) const
    {
        sync();
        return _root->getName( index + _start ) + _path.length();
    }
    /// returns property value by index
    virtual const lString16 & getValue( int index ) const
    {
        sync();
        return _root->getValue( index + _start );
    }
    /// sets property value by index
    virtual void setValue( int index, const lString16 &value )
    {
        sync();
        _root->setValue( index + _start, value );
    }
    /// get string property by name, returns false if not found
    virtual bool getString( const char * propName, lString16 &result ) const
    {
        sync();
        int pos = 0;
        if ( !_root->findItem( propName, _path.length(), _start, _end, pos ) )
            return false;
        result = _root->getValue( pos );
        return true;
    }
    /// set string property by name
    virtual void setString( const char * propName, const lString16 &value )
    {
        sync();
        int pos = 0;
        if ( !_root->findItem( propName, _path.length(), _start, _end, pos ) ) {
            _root->_list.insert( pos, new CRPropItem( (_path + propName).c_str(), value ) );
            _root->_revision++;
            sync();
        } else {
            _root->_list[pos]->setValue( value );
        }
    }
    /// get subpath container
    virtual CRPropRef getSubProps( const char * path )
    {
        return _root->getSubProps( (_path + path).c_str() );
    }
    /// virtual destructor
    virtual ~CRPropSubContainer()
    {
    }
};

/// get subpath container
CRPropRef CRPropContainer::getSubProps( const char * path )
{
    return CRPropRef(new CRPropSubContainer(this, lString8(path)));
}

/// factory function
CRPropRef LVCreatePropsContainer()
{
    return CRPropRef(new CRPropContainer());
}

const char * props_magic = "PRPS";
const char * props_name_magic = "n=";
const char * props_value_magic = "v=";
/// serialize to byte buffer
void CRPropAccessor::serialize( SerialBuf & buf )
{
    if ( buf.error() )
        return;
    int pos = buf.pos();
    buf.putMagic( props_magic );
    lInt32 sz = getCount();
    buf << sz;
    for ( int i=0; i<sz; i++ ) {
        buf.putMagic( props_name_magic );
        buf << lString8(getName(i));
        buf.putMagic( props_value_magic );
        buf << getValue(i);
    }
    buf.putCRC( buf.pos() - pos );
}

/// deserialize from byte buffer
bool CRPropAccessor::deserialize( SerialBuf & buf )
{
    clear();
    if ( buf.error() )
        return false;
    int pos = buf.pos();
    if ( !buf.checkMagic( props_magic ) )
        return false;
    lInt32 sz;
    buf >> sz;
    for ( int i=0; i<sz; i++ ) {
        lString8 nm;
        lString16 val;
        if ( !buf.checkMagic( props_name_magic ) )
            return false;
        buf >> nm;
        if ( !buf.checkMagic( props_value_magic ) )
            return false;
        buf >> val;
        setString( nm.c_str(), val );
    }
    buf.checkCRC( buf.pos() - pos );
    return !buf.error();
}

