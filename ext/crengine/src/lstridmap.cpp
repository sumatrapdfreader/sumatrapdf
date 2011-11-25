/*******************************************************

   CoolReader Engine DOM Tree 

   LDOMNodeIdMap.cpp:  Name to Id map

   (c) Vadim Lopatin, 2000-2006
   This source code is distributed under the terms of
   GNU General Public License
   See LICENSE file for details

*******************************************************/

#include "../include/lstridmap.h"
#include "../include/dtddef.h"
#include "../include/lvtinydom.h"
#include <string.h>

LDOMNameIdMapItem::LDOMNameIdMapItem(lUInt16 _id, const lString16 & _value, const css_elem_def_props_t * _data)
    : id(_id), value(_value)
{
	if ( _data ) {
        data = new css_elem_def_props_t();
		*data = *_data;
	} else
		data = NULL;
}

LDOMNameIdMapItem::LDOMNameIdMapItem(LDOMNameIdMapItem & item)
    : id(item.id), value(item.value)
{
	if ( item.data ) {
		data = new css_elem_def_props_t();
		*data = *item.data;
	} else {
		data = NULL;
	}
}


static const char id_map_item_magic[] = "IDMI";

/// serialize to byte array
void LDOMNameIdMapItem::serialize( SerialBuf & buf )
{
    if ( buf.error() )
        return;
	buf.putMagic( id_map_item_magic );
	buf << id;
	buf << value;
	if ( data ) {
		buf << (lUInt8)1;
		buf << (lUInt8)data->display;
		buf << (lUInt8)data->white_space;
		buf << data->allow_text;
		buf << data->is_object;
	} else {
		buf << (lUInt8)0;
	}
}

/// deserialize from byte array
LDOMNameIdMapItem * LDOMNameIdMapItem::deserialize( SerialBuf & buf )
{
    if ( buf.error() )
        return NULL;
	if ( !buf.checkMagic( id_map_item_magic ) )
        return NULL;
	lUInt16 id;
	lString16 value;
	lUInt8 flgData;
    buf >> id >> value >> flgData;
    if ( id>=MAX_TYPE_ID )
        return NULL;
    if ( flgData ) {
        css_elem_def_props_t props;
        lUInt8 display;
        lUInt8 white_space;
        buf >> display >> white_space >> props.allow_text >> props.is_object;
        if ( display > css_d_none || white_space > css_ws_nowrap )
            return NULL;
        props.display = (css_display_t)display;
        props.white_space = (css_white_space_t)white_space;
    	return new LDOMNameIdMapItem(id, value, &props);
    }
   	return new LDOMNameIdMapItem(id, value, NULL);
}

LDOMNameIdMapItem::~LDOMNameIdMapItem()
{
	if ( data )
		delete data;
}

static const char id_map_magic[] = "IMAP";

/// serialize to byte array (pointer will be incremented by number of bytes written)
void LDOMNameIdMap::serialize( SerialBuf & buf )
{
    if ( buf.error() )
        return;
    int start = buf.pos();
	buf.putMagic( id_map_magic );
    buf << m_count;
    for ( int i=0; i<m_size; i++ ) {
        if ( m_by_id[i] )
            m_by_id[i]->serialize( buf );
    }
    buf.putCRC( buf.pos() - start );
}

/// deserialize from byte array (pointer will be incremented by number of bytes read)
bool LDOMNameIdMap::deserialize( SerialBuf & buf )
{
    if ( buf.error() )
        return false;
    int start = buf.pos();
    if ( !buf.checkMagic( id_map_magic ) ) {
        buf.seterror();
        return false;
    }
    Clear();
    lUInt16 count;
    buf >> count;
    if ( count>m_size ) {
        buf.seterror();
        return false;
    }
    for ( int i=0; i<count; i++ ) {
        LDOMNameIdMapItem * item = LDOMNameIdMapItem::deserialize(buf);
        if ( !item || (item->id<m_size && m_by_id[item->id]!=NULL ) ) { // invalid entry
            if ( item )
                delete item;
            buf.seterror();
            return false;
        }
        AddItem( item );
    }
    m_sorted = false;
    buf.checkCRC( buf.pos() - start );
    return !buf.error();
}


LDOMNameIdMap::LDOMNameIdMap(lUInt16 maxId)
{
    m_size = maxId+1;
    m_count = 0;
    m_by_id   = new LDOMNameIdMapItem * [m_size];
    memset( m_by_id, 0, sizeof(LDOMNameIdMapItem *)*m_size );  
    m_by_name = new LDOMNameIdMapItem * [m_size];
    memset( m_by_name, 0, sizeof(LDOMNameIdMapItem *)*m_size );  
    m_sorted = true;
}

/// Copy constructor
LDOMNameIdMap::LDOMNameIdMap( LDOMNameIdMap & map )
{
    m_size = map.m_size;
    m_count = map.m_count;
    m_by_id   = new LDOMNameIdMapItem * [m_size];
    int i;
    for ( i=0; i<m_size; i++ ) {
        if ( map.m_by_id[i] )
            m_by_id[i] = new LDOMNameIdMapItem( *map.m_by_id[i] );
        else
            m_by_id[i] = NULL;
    }
    m_by_name = new LDOMNameIdMapItem * [m_size];
    for ( i=0; i<m_size; i++ ) {
        if ( map.m_by_name[i] )
            m_by_name[i] = new LDOMNameIdMapItem( *map.m_by_name[i] );
        else
            m_by_name[i] = NULL;
    }
    m_sorted = map.m_sorted;
}

LDOMNameIdMap::~LDOMNameIdMap()
{
    Clear();
    delete[] m_by_name;
    delete[] m_by_id;
}

static int compare_items( const void * item1, const void * item2 )
{
    return (*((LDOMNameIdMapItem **)item1))->value.compare( (*((LDOMNameIdMapItem **)item2))->value );
}

void LDOMNameIdMap::Sort()
{
    if (m_count>1)
        qsort( m_by_name, m_count, sizeof(LDOMNameIdMapItem*), compare_items );
    m_sorted = true;
}

const LDOMNameIdMapItem * LDOMNameIdMap::findItem( const lChar16 * name )
{
    if (m_count==0 || !name || !*name)
        return NULL;
    if (!m_sorted)
        Sort();
    lUInt16 a, b, c;
    int r;
    a = 0;
    b = m_count;
    while (1)
    {
        c = (a + b)>>1;
        r = lStr_cmp( name, m_by_name[c]->value.c_str() );
        if (r == 0)
            return m_by_name[c]; // found
        if (b==a+1)
            return NULL; // not found
        if (r>0)
        {
            a = c;
        }
        else
        {
            b = c;
        }
    }
}

const LDOMNameIdMapItem * LDOMNameIdMap::findItem( const lChar8 * name )
{
    if (m_count==0 || !name || !*name)
        return NULL;
    if (!m_sorted)
        Sort();
    lUInt16 a, b, c;
    int r;
    a = 0;
    b = m_count;
    while (1)
    {
        c = (a + b)>>1;
        r = lStr_cmp( name, m_by_name[c]->value.c_str() );
        if (r == 0)
            return m_by_name[c]; // found
        if (b==a+1)
            return NULL; // not found
        if (r>0)
        {
            a = c;
        }
        else
        {
            b = c;
        }
    }
}

void LDOMNameIdMap::AddItem( LDOMNameIdMapItem * item )
{
    if ( item==NULL )
        return;
    if ( item->id==0 ) {
        delete item;
        return;
    }
    if (item->id>=m_size)
    {
        // reallocate storage
        lUInt16 newsize = item->id+16;
        m_by_id = (LDOMNameIdMapItem **)realloc( m_by_id, sizeof(LDOMNameIdMapItem *)*newsize );
        m_by_name = (LDOMNameIdMapItem **)realloc( m_by_name, sizeof(LDOMNameIdMapItem *)*newsize );
        for (lUInt16 i = m_size; i<newsize; i++)
        {
            m_by_id[i] = NULL;
            m_by_name[i] = NULL;
        }
        m_size = newsize;
    }
    if (m_by_id[item->id] != NULL)
    {
        delete item;
        return; // already exists
    }
    m_by_id[item->id] = item;
    m_by_name[m_count++] = item;
    m_sorted = false;
}

void LDOMNameIdMap::AddItem( lUInt16 id, const lString16 & value, const css_elem_def_props_t * data )
{
    if (id==0)
        return;
    LDOMNameIdMapItem * item = new LDOMNameIdMapItem( id, value, data );
    AddItem( item );
}


void LDOMNameIdMap::Clear()
{
    for (lUInt16 i = 0; i<m_count; i++)
    {
        if (m_by_name[i])
            delete m_by_name[i];
    }
    memset( m_by_id, 0, sizeof(LDOMNameIdMapItem *)*m_size);
    m_count = 0;
}

void LDOMNameIdMap::dumpUnknownItems( FILE * f, int start_id )
{
    for (int i=start_id; i<m_size; i++)
    {
        if (m_by_id[i] != NULL)
        {
            lString8 s8( m_by_id[i]->value.c_str() );
            fprintf( f, "%d %s\n", m_by_id[i]->id, s8.c_str() );
        }
    }
}

