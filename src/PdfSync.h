// Copyright William Blum 2008 http://william.famille-blum.org/
// PDF-source syncronizer based on .pdfsync file
// License: GPLv2

#pragma once
#include <windows.h>
#include <crtdbg.h>

#include "base_util.h"
#include "str_util.h"


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
        _ASSERT(i<m_size);
        return m_data[i];
    }
    void clear()
    {
        m_size = 0;
    }
    void push_back(_Ty v)
    {
        if(m_size>=m_allocsize) {
            m_allocsize += ALLOC_INCREMENT;
            m_data = (_Ty *)realloc(m_data, sizeof(_Ty) * m_allocsize); 
        }
        m_data[m_size] = v;
        m_size++;
    }
    void resize(size_t s)
    {
        if(s>m_allocsize) {
            m_allocsize = s+ALLOC_INCREMENT-s%ALLOC_INCREMENT;
            m_data = (_Ty *)realloc(m_data, sizeof(_Ty) * m_allocsize); 
        }
        m_size = s;
    }
    size_t size()
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
        delete m_data;
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
        _ASSERT(size()>0);
        resize(size()-1);
    }
    _Ty &top() {
        _ASSERT(size()>0);
        return (*this)[size()-1];
    }
};

#endif


// Minimal error distance^2 between a point clicked by the user and a PDF mark
#define PDFSYNC_EPSILON_SQUARE          800

// Minimal vertical distance
#define PDFSYNC_EPSILON_Y               20

#define PDFSYNCERR_SUCCESS                   0
#define PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED 1
#define PDFSYNCERR_INVALID_PAGE_NUMBER       2 
#define PDFSYNCERR_NO_SYNC_AT_LOCATION       3

typedef struct {
    TCHAR filename[_MAX_PATH]; // source file name
    fpos_t openline_pos;    // start of the scope in the sync file
    fpos_t closeline_pos;   // end of the scope
} src_scope;


// a pdfsheet entry gives the starting position in the pdfsync file of a section starting with "s ..." followed by declaration lines of the form "p ...2
typedef struct {
    fpos_t startpos;
#if _DEBUG
    fpos_t endpos;
#endif
} pdfsheet_indexentry;


// a section of consecutive records declarations in the syncfile ('l' lines)
typedef struct {
    int srcfile;           // index of the `scoping' source file 
    fpos_t startpos;       // start position in the sync file
    UINT firstrecord;      // number of the first record in the section
#if _DEBUG
    int highestrecord; // highest record #
#endif
} record_section;


// Binary decision tree used to quickly find the source filename containing a given record
// from the record_sections structure.
enum nodetype { Leaf, Internal };
typedef struct {
    nodetype type; // leaf or internal node?
} record2srcfile_node;

typedef struct {
    record2srcfile_node header;
    int section; // record section number in table record_sections[].
} record2srcfile_leaf;

typedef struct {
    record2srcfile_node header;
    int splitvalue; // value to compare to in order to decide whether we go right or left in the tree
    record2srcfile_node *left;
    record2srcfile_node *right;
} record2srcfile_internal;

#define PDF_EXTENSION     ".PDF"
#define PDFSYNC_EXTENSION ".PDFSYNC"

class Pdfsync
{
public:
    Pdfsync(PCTSTR filename)
    {
        size_t n = _tcslen(filename);
        size_t u = _countof(PDF_EXTENSION)-1;
        if(n>u && _tcsicmp(filename+(n-u),PDF_EXTENSION) == 0 ) {
            _tcsncpy_s(this->syncfilename, filename, n-u);
            _tcscat_s(this->syncfilename, PDFSYNC_EXTENSION);
        }
        else {
            size_t u = _countof(PDFSYNC_EXTENSION)-1;
            _ASSERT(n>u && _tcsicmp(filename+(n-u),PDFSYNC_EXTENSION) == 0 );
        }
        this->record2src_decitree = NULL;
        this->index_discarded = true;
    }

    ~Pdfsync() {
        if(this->record2src_decitree)
            delete_decision_tree(this->record2src_decitree);
    }


    int rebuild_index();
    UINT pdf_to_source(UINT sheet, UINT x, UINT y, PTSTR filename, UINT cchFilename, UINT *line, UINT *col);
    UINT source_to_pdf(PCTSTR srcfilename, UINT line, UINT col, UINT *page, UINT *x, UINT *y);

    void discard_index() { this->index_discarded = true;}
    bool is_index_discarded() { return this->index_discarded; }

    UINT prepare_commandline(PCTSTR pattern, PCTSTR filename, UINT line, UINT col, PTSTR cmdline, UINT cchCmdline);

private:
    record2srcfile_node *build_decision_tree(int leftrecord, int rightrecord);
    void delete_decision_tree(record2srcfile_node *root);
    int get_record_section(int record_index);
    int scan_and_build_index(FILE *fp);

private:
    vector<pdfsheet_indexentry> pdfsheet_index;
    vector<record_section> record_sections;
    vector<src_scope> srcfiles;
    record2srcfile_node *record2src_decitree;
    char syncfilename[_MAX_PATH];
    bool index_discarded; // true if the index needs to be recomputed (needs to be set to true when a change to the pdfsync file is detected)
};

