/** \file props.h
    \brief properties container

    CoolReader Engine

    (c) Vadim Lopatin, 2000-2007
    This source code is distributed under the terms of
    GNU General Public License
    See LICENSE file for details
*/

#ifndef PROPS_H_INCLUDED
#define PROPS_H_INCLUDED

#include "lvstring.h"
#include "lvptrvec.h"
#include "lvref.h"
#include "lvstream.h"

class CRPropAccessor;
typedef LVFastRef<CRPropAccessor> CRPropRef;

/// interface to get/set properties
class CRPropAccessor : public LVRefCounter {
public:
    /// returns property path in root container
    virtual const lString8 & getPath() const = 0;
    /// clear all items
    virtual void clear() = 0;
    /// returns property item count in container
    virtual int getCount() const = 0;
    /// returns property name by index
    virtual const char * getName( int index ) const = 0;
    /// returns property value by index
    virtual const lString16 & getValue( int index ) const = 0;
    /// sets property value by index
    virtual void setValue( int index, const lString16 &value ) = 0;

    /// returns true if specified property exists
    virtual bool hasProperty( const char * propName ) const = 0;
    /// get string property by name, returns false if not found
    virtual bool getString( const char * propName, lString16 &result ) const = 0;
    /// get string property by name, returns default value if not found
    virtual lString16 getStringDef( const char * propName, const char * defValue = NULL ) const;
    /// set string property by name, if it's not set already
    virtual void setStringDef( const char * propName, const char * defValue );
    /// set string property by name, if it's not set already
    virtual void setStringDef( const char * propName, lString16 defValue )
    {
        if ( !hasProperty(propName) )
            setString( propName, defValue );
    }
    /// set string property by name
    virtual void setString( const char * propName, const lString16 &value ) = 0;
    /// set string property by name
    virtual void setString( const char * propName, const lString8 &value )
    {
        setString( propName, Utf8ToUnicode( value ) );
    }
    /// set string property by name
    virtual void setString( const char * propName, const char * value )
    {
        setString( propName, lString8( value ) );
    }
    /// do validation and corrections
    virtual void limitValueList( const char * propName, const char * values[] );
    /// do validation and corrections
    virtual void limitValueList( const char * propName, int values[], int value_count );

    /// get int property by name, returns false if not found
    virtual bool getInt( const char * propName, int &result ) const;
    /// get int property by name, returns default value if not found
    virtual int getIntDef( const char * propName, int defValue=0 ) const;
    /// set int property by name
    virtual void setInt( const char * propName, int value );
    /// set int property by name, if it's not set already
    virtual void setIntDef( const char * propName, int value );
    /// set int property as hex
    virtual void setHex( const char * propName, int value );
    /// set int property as hex, if not exist
    virtual void setHexDef( const char * propName, int value )
    {
        if ( !hasProperty( propName ) )
            setHex( propName, value );
    }

    /// get bool property by name, returns false if not found
    virtual bool getBool( const char * propName, bool &result ) const;
    /// get bool property by name, returns default value if not found
    virtual bool getBoolDef( const char * propName, bool defValue=false ) const;
    /// set bool property by name
    virtual void setBool( const char * propName, bool value );

    /// get lInt64 property by name, returns false if not found
    virtual bool getInt64( const char * propName, lInt64 &result ) const;
    /// get lInt64 property by name, returns default value if not found
    virtual lInt64 getInt64Def( const char * propName, lInt64 defValue=0 ) const;
    /// set lInt64 property by name
    virtual void setInt64( const char * propName, lInt64 value );

    /// get argb color (#xxxxxx) property by name, returns false if not found
    virtual bool getColor( const char * propName, lUInt32 &result ) const;
    /// get argb color (#xxxxxx) property by name, returns default value if not found
    virtual lUInt32 getColorDef( const char * propName, lUInt32 defValue=0 ) const;
    /// set argb color (#xxxxxx) property by name
    virtual void setColor( const char * propName, lUInt32 value );

    /// get rect property by name, returns false if not found
    virtual bool getRect( const char * propName, lvRect &result ) const;
    /// get rect property by name, returns default value if not found
    virtual lvRect getRectDef( const char * propName, const lvRect & defValue ) const;
    /// set rect property by name
    virtual void setRect( const char * propName, const lvRect & value );

    /// get point property by name, returns false if not found
    virtual bool getPoint( const char * propName, lvPoint &result ) const;
    /// get point property by name, returns default value if not found
    virtual lvPoint getPointDef( const char * propName, const lvPoint & defValue ) const;
    /// set point property by name
    virtual void setPoint( const char * propName, const lvPoint & value );


    /// get subpath container (only items with names started with path)
    virtual CRPropRef getSubProps( const char * path ) = 0;
    /// get copy of property list
    virtual CRPropRef clone() const = 0;
    /// set contents from specified properties
    virtual void set( const CRPropRef & v );
    /// read from stream
    virtual bool loadFromStream( LVStream * stream );
    /// save to stream
    virtual bool saveToStream( LVStream * stream );
    /// virtual destructor
    virtual ~CRPropAccessor();
    /// serialize to byte buffer
    virtual void serialize( SerialBuf & buf );
    /// deserialize from byte buffer
    virtual bool deserialize( SerialBuf & buf );
};


/// returns common items from props1 not containing in props2
CRPropRef operator - ( CRPropRef props1, CRPropRef props2 );
/// returns common items containing in props1 or props2
CRPropRef operator | ( CRPropRef props1, CRPropRef props2 );
/// returns common items of props1 and props2
CRPropRef operator & ( CRPropRef props1, CRPropRef props2 );
/// returns added or changed items of props2 compared to props1
CRPropRef operator ^ ( CRPropRef props1, CRPropRef props2 );

/// factory function creates empty property container
CRPropRef LVCreatePropsContainer();
/// deep copy properties
inline CRPropRef LVClonePropsContainer( CRPropRef props )
{
    CRPropRef result = LVCreatePropsContainer();
    result->set( props );
    return result;
}


#endif //PROPS_H_INCLUDED
