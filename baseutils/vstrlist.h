/* Copyright 2006-2010 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef _VSTRLIST_H_
#define _VSTRLIST_H_

#include "tstr_util.h"

#ifdef USE_STL
#include <vector>
#include <stack>
using namespace std;
#else
#define ALLOC_INCREMENT  10
template <class _Ty>
class vector {
public:
    _Ty &operator[](size_t i) const
    {
        assert(i<m_size);
        return m_data[i];
    }
    _Ty at(size_t i) const
    {
        assert(i<m_size);
        return m_data[i];
    }
    void clear()
    {
        m_size = 0;
    }
    void push_back(_Ty v)
    {
        if (m_size>=m_allocsize) {
            m_allocsize += ALLOC_INCREMENT;
            m_data = (_Ty *)realloc(m_data, sizeof(_Ty) * m_allocsize); 
        }
        m_data[m_size] = v;
        m_size++;
    }
    void resize(size_t s)
    {
        if (s>m_allocsize) {
            m_allocsize = s+ALLOC_INCREMENT-s%ALLOC_INCREMENT;
            m_data = (_Ty *)realloc(m_data, sizeof(_Ty) * m_allocsize); 
        }
        m_size = s;
    }
    void erase(size_t i)
    {
        if (i < m_size) {
            memcpy(m_data + i, m_data + i + 1, sizeof(_Ty) * (m_size - i - 1));
            m_size--;
        }
    }
    size_t size() const 
    {
        return m_size;
    }
    vector()
    {
        m_allocsize = ALLOC_INCREMENT;
        m_size = 0;
        m_data = (_Ty *)malloc(sizeof(_Ty) * m_allocsize); 
    }
    ~vector()
    {
        free(m_data);
    }
private:
    _Ty *m_data;
    size_t m_allocsize, m_size;
};

template <class _Ty>
class stack : public vector<_Ty> {
public:
    void push(_Ty v) {
        push_back(v);
    }
    void pop() {
        assert(this->size()>0);
        erase(this->size()-1);
    }
    _Ty &top() {
        assert(this->size()>0);
        return (*this)[this->size()-1];
    }
};

#endif

class VStrList : public vector<TCHAR *>
{
public:
    ~VStrList() { clearFree(); }

    TCHAR *join(TCHAR *joint=NULL)
    {
        int len = 0;
        int jointLen = joint ? lstrlen(joint) : 0;
        TCHAR *result, *tmp;

        for (size_t i = size(); i > 0; i--)
            len += lstrlen((*this)[i - 1]) + jointLen;
        len -= jointLen;
        if (len <= 0)
            return (TCHAR *)calloc(1, sizeof(TCHAR));

        result = (TCHAR *)malloc((len + 1) * sizeof(TCHAR));
        if (!result)
            return NULL;

        tmp = result;
        for (size_t i = 0; i < size(); i++) {
            if (jointLen > 0 && i > 0) {
                lstrcpy(tmp, joint);
                tmp += jointLen;
            }
            lstrcpy(tmp, (*this)[i]);
            tmp += lstrlen((*this)[i]);
        }

        return result;
    }

    int find(TCHAR *s)
    {
        TCHAR *s2;
        for (size_t i = 0; i<size(); i++) {
            s2 = at(i);
            if (tstr_eq(s, s2))
                return (int)i;
        }
        return -1;
    }

    void clearFree()
    {
        for (size_t i = 0; i < size(); i++)
            free((*this)[i]);
        clear();
    }
};

#endif
