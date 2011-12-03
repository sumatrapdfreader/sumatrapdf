/*******************************************************

   CoolReader Engine

   lvrefcache.h:  Referenced objects cache
      allows to reuse objects with the same concents

   (c) Vadim Lopatin, 2000-2006
   This source code is distributed under the terms of
   GNU General Public License
   See LICENSE file for details

*******************************************************/

#if !defined(__LV_REF_CACHE_H_INCLUDED__)
#define __LV_REF_CACHE_H_INCLUDED__

#include "lvref.h"
#include "lvarray.h"

/*
    Object cache

    Requirements: 
       sz parameter of constructor should be power of 2
       bool operator == (LVRef<T> & r1, LVRef<T> & r2 ) should be defined
       lUInt32 calcHash( LVRef<T> & r1 ) should be defined
*/

template <class ref_t> 
class LVRefCache {

    class LVRefCacheRec {
        ref_t style;
        lUInt32 hash;
        LVRefCacheRec * next;
        LVRefCacheRec(ref_t & s, lUInt32 h)
            : style(s), hash(h), next(NULL) { }
        friend class LVRefCache< ref_t >;
    };

private:
    int size;
    LVRefCacheRec ** table;

public:
    // check whether equal object already exists if cache
    // if found, replace reference with cached value
    void cacheIt(ref_t & style)
    {
        lUInt32 hash = calcHash( style );
        lUInt32 index = hash & (size - 1);
        LVRefCacheRec **rr;
        rr = &table[index];
        while ( *rr != NULL )
        {
            if ( *(*rr)->style.get() == *style.get() )
            {
                style = (*rr)->style;
                return;
            }
            rr = &(*rr)->next;
        }
        *rr = new LVRefCacheRec( style, hash );
    }
    // garbage collector: remove unused entries
    void gc()
    {
        for (int index = 0; index < size; index++)
        {
            LVRefCacheRec **rr;
            rr = &table[index];
            while ( *rr != NULL )
            {
                if ( (*rr)->style.getRefCount() == 1 )
                {
                    LVRefCacheRec * r = (*rr);
                    *rr = r->next;
                    delete r;
                }
                else
                {
                    rr = &(*rr)->next;
                }
            }
        }
    }
    LVRefCache( int sz )
    {
        size = sz;
        table = new LVRefCacheRec * [ sz ];
        for( int i=0; i<sz; i++ )
            table[i] = NULL;
    }
    ~LVRefCache()
    {
        LVRefCacheRec *r, *r2;
        for ( int i=0; i < size; i++ )
        {
            for ( r = table[ i ]; r;  )
            {
                r2 = r;
                r = r->next;
                delete r2;
            }
        }
        delete[] table;
    }
};

template <class ref_t>
class LVIndexedRefCache {

    // hash table item
    struct LVRefCacheRec {
        int index;
        ref_t style;
        lUInt32 hash;
        LVRefCacheRec * next;
        LVRefCacheRec(ref_t & s, lUInt32 h)
            : style(s), hash(h), next(NULL) { }
    };

    // index item
    struct LVRefCacheIndexRec {
        LVRefCacheRec * item;
        int refcount; // refcount, or next free index if item==NULL
    };

private:
    int size;
    LVRefCacheRec ** table;

    LVRefCacheIndexRec * index;
    int indexsize;
    int nextindex;
    int freeindex;
    int numitems;

    int indexItem( LVRefCacheRec * rec )
    {
        int n;
        if ( freeindex ) {
            n = freeindex;
            freeindex = index[freeindex].refcount; // next free index
        } else {
            n = ++nextindex;
        }
        if ( n>=indexsize ) {
            // resize
            if ( indexsize==0 )
                indexsize = size/2;
            else
                indexsize *= 2;
            index = (LVRefCacheIndexRec*)realloc( index, sizeof(LVRefCacheIndexRec)*indexsize );
            for ( int i=nextindex+1; i<indexsize; i++ ) {
                index[i].item = NULL;
                index[i].refcount = 0;
            }
        }
        rec->index = n;
        index[n].item = rec;
        index[n].refcount = 1;
        return n;
    }

    // remove item from hash table
    void removeItem( LVRefCacheRec * item )
    {
        lUInt32 hash = item->hash;
        lUInt32 tindex = hash & (size - 1);
        LVRefCacheRec **rr = &table[tindex];
        for ( ; *rr; rr = &(*rr)->next ) {
            if ( *rr == item ) {
                LVRefCacheRec * tmp = *rr;
                *rr = (*rr)->next;
                delete tmp;
                numitems--;
                return;
            }
        }
        // not found!
    }

public:

    LVArray<ref_t> * getIndex()
    {
        LVArray<ref_t> * list = new LVArray<ref_t>(indexsize, ref_t());
        for ( int i=1; i<indexsize; i++ ) {
            if ( index[i].item )
                list->set(i, index[i].item->style);
        }
        return list;
    }

    int length()
    {
        return numitems;
    }

    void release( ref_t r )
    {
        int i = find(r);
        if (  i>0 )
            release(i);
    }

    void release( int n )
    {
        if ( n<1 || n>nextindex )
            return;
        if ( index[n].item ) {
            if ( (--index[n].refcount)<=0 ) {
                removeItem( index[n].item );
                // next free
                index[n].refcount = freeindex;
                index[n].item = NULL;
                freeindex = n;
            }
        }
    }

    // get by index
    ref_t get( int n )
    {
        if ( n>0 && n<=nextindex && index[n].item )
            return index[n].item->style;
        return ref_t();
    }

    // check whether equal object already exists if cache
    // if found, replace reference with cached value
    // returns index of item - use it to release reference
    bool cache( lUInt16 &indexholder, ref_t & style)
    {
        int newindex = cache( style );
        if ( indexholder != newindex ) {
            release( indexholder );
            indexholder = (lUInt16)newindex;
            return true;
        } else {
            release( indexholder );
            return false;
        }
    }

    bool addIndexRef( lUInt16 n )
    {
        if ( n>0 && n<=nextindex && index[n].item ) {
            index[n].refcount++;
            return true;
        } else
            return false;
    }

    // check whether equal object already exists if cache
    // if found, replace reference with cached value
    // returns index of item - use it to release reference
    int cache(ref_t & style)
    {
        lUInt32 hash = calcHash( style );
        lUInt32 index = hash & (size - 1);
        LVRefCacheRec **rr;
        rr = &table[index];
        while ( *rr != NULL )
        {
            if ( (*rr)->hash==hash && *(*rr)->style.get() == *style.get() )
            {
                style = (*rr)->style;
                int n = (*rr)->index;
                this->index[n].refcount++;
                return n;
            }
            rr = &(*rr)->next;
        }
        *rr = new LVRefCacheRec( style, hash );
        numitems++;
        return indexItem( *rr );
    }

    // check whether equal object already exists if cache
    // if found, replace reference with cached value
    // returns index of item - use it to release reference
    int find(ref_t & style)
    {
        lUInt32 hash = calcHash( style );
        lUInt32 index = hash & (size - 1);
        LVRefCacheRec **rr;
        rr = &table[index];
        while ( *rr != NULL )
        {
            if ( (*rr)->hash==hash && *(*rr)->style.get() == *style.get() )
            {
                int n = (*rr)->index;
                return n;
            }
            rr = &(*rr)->next;
        }
        return 0;
    }

    /// from index array
    LVIndexedRefCache( LVArray<ref_t> &list )
    : index(NULL)
    , indexsize(0)
    , nextindex(0)
    , freeindex(0)
    , numitems(0)
    {
        setIndex(list);
    }

    int nearestPowerOf2( int n )
    {
        int res;
        for ( res = 1; res<n; res<<=1 )
            ;
        return res;
    }

    /// init from index array
    void setIndex( LVArray<ref_t> &list )
    {
        clear();
        size = nearestPowerOf2(list.length()>0 ? list.length()*4 : 32);
        if ( table )
            delete[] table;
        table = new LVRefCacheRec * [ size ];
        for( int i=0; i<size; i++ )
            table[i] = NULL;
        indexsize = list.length();
        nextindex = indexsize > 0 ? indexsize-1 : 0;
        if ( indexsize ) {
            index = (LVRefCacheIndexRec*)realloc( index, sizeof(LVRefCacheIndexRec)*indexsize );
            index[0].item = NULL;
            index[0].refcount=0;
            for ( int i=1; i<indexsize; i++ ) {
                if ( list[i].isNull() ) {
                    // add free node
                    index[i].item = NULL;
                    index[i].refcount = freeindex;
                    freeindex = i;
                } else {
                    // add item
                    lUInt32 hash = calcHash( list[i] );
                    lUInt32 hindex = hash & (size - 1);
                    LVRefCacheRec * rec = new LVRefCacheRec(list[i], hash);
                    rec->index = i;
                    rec->next = table[hindex];
                    table[hindex] = rec;
                    index[i].item = rec;
                    index[i].refcount = 1;
                    numitems++;
                }
            }
        }
    }

    LVIndexedRefCache( int sz )
    : index(NULL)
    , indexsize(0)
    , nextindex(0)
    , freeindex(0)
    , numitems(0)
    {
        size = sz;
        table = new LVRefCacheRec * [ sz ];
        for( int i=0; i<sz; i++ )
            table[i] = NULL;
    }
    void clear( int sz = 0 )
    {
        if ( sz==-1 )
            sz = size;
        LVRefCacheRec *r, *r2;
        for ( int i=0; i < size; i++ )
        {
            for ( r = table[ i ]; r;  )
            {
                r2 = r;
                r = r->next;
                delete r2;
            }
            table[i] = NULL;
        }
        if (index) {
            free( index );
            index = NULL;
            indexsize = 0;
            nextindex = 0;
            freeindex = 0;
        }
        numitems = 0;
        if ( sz ) {
            size = sz;
            if ( table )
                delete[] table;
            table = new LVRefCacheRec * [ sz ];
            for( int i=0; i<sz; i++ )
                table[i] = NULL;
        }
    }
    ~LVIndexedRefCache()
    {
        clear();
        delete[] table;
    }
};

template <typename keyT, class dataT> class LVCacheMap
{
private:
    class Pair {
    public: 
        keyT key;
        dataT data;
        int lastAccess;
    };
    Pair * buf;
    int size;
    int numitems;
    int lastAccess;
    void checkOverflow( int oldestAccessTime )
    {
        int i;
        if ( oldestAccessTime==-1 ) {
            for ( i=0; i<size; i++ )
                if ( oldestAccessTime==-1 || buf[i].lastAccess>oldestAccessTime )
                    oldestAccessTime = buf[i].lastAccess;
        }
        if ( oldestAccessTime>1000000000 ) {
            int maxLastAccess = 0;
            for ( i=0; i<size; i++ ) {
                buf[i].lastAccess -= 1000000000;
                if ( maxLastAccess==0 || buf[i].lastAccess>maxLastAccess )
                    maxLastAccess = buf[i].lastAccess;
            }
            lastAccess = maxLastAccess+1;
        }
    }
public:
    int length()
    {
        return numitems;
    }
    LVCacheMap( int maxSize )
    : size(maxSize), numitems(0), lastAccess(1)
    {
        buf = new Pair[ size ];
        clear();
    }
    void clear()
    {
        for ( int i=0; i<size; i++ )
        {
            buf[i].key = keyT();
            buf[i].data = dataT();
            buf[i].lastAccess = 0;
        }
        numitems = 0;
    }
    bool get( keyT key, dataT & data )
    {
        for ( int i=0; i<size; i++ ) {
            if ( buf[i].key == key ) {
                data = buf[i].data;
                buf[i].lastAccess = ++lastAccess;
                if ( lastAccess>1000000000 )
                    checkOverflow(-1);
                return true;
            }
        }
        return false;
    }
    bool remove( keyT key )
    {
        for ( int i=0; i<size; i++ ) {
            if ( buf[i].key == key ) {
                buf[i].key = keyT();
                buf[i].data = dataT();
                buf[i].lastAccess = 0;
                numitems--;
                return true;
            }
        }
        return false;
    }
    void set( keyT key, dataT data )
    {
        int oldestAccessTime = -1;
        int oldestIndex = 0;
        for ( int i=0; i<size; i++ ) {
            if ( buf[i].key == key ) {
                buf[i].data = data;
                buf[i].lastAccess = ++lastAccess;
                return;
            }
            int at = buf[i].lastAccess;
            if ( at < oldestAccessTime || oldestAccessTime==-1 ) {
                oldestAccessTime = at;
                oldestIndex = i;
            }
        }
        checkOverflow(oldestAccessTime);
        if ( buf[oldestIndex].key==keyT() )
            numitems++;
        buf[oldestIndex].key = key;
        buf[oldestIndex].data = data;
        buf[oldestIndex].lastAccess = ++lastAccess;
        return;
    }
    ~LVCacheMap()
    {
        delete[] buf;
    }
};

#endif // __LV_REF_CACHE_H_INCLUDED__
