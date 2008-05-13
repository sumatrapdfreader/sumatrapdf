// Copyright William Blum 2008 http://william.famille-blum.org/
// PDF-source syncronizer based on .pdfsync file
// License: GPLv2

#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <hash_map>
#include <stack>
#include <stddef.h>
#include "base_util.h"
#include "str_util.h"
using namespace std;
using namespace stdext;

// Minimal error distance^2 between a point clicked by the user and a PDF mark
#define PDFSYNC_EPSILON_SQUARE          800

// Minimal vertical distance
#define PDFSYNC_EPSILON_Y               20

#define PDFSYNCERR_SUCCESS                   0
#define PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED 1
#define PDFSYNCERR_INVALID_PAGE_NUMBER       2 
#define PDFSYNCERR_NO_SYNC_AT_LOCATION       3

typedef struct {
 fpos_t openline_pos;
 fpos_t closeline_pos;
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
 hash_map<string, src_scope>::const_iterator srcfile; // `scoping' source file 
 fpos_t startpos;     // start position in the sync file
 UINT firstrecord;    // number of the first record in the section
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
    Pdfsync(string filename)
    {
        size_t n = filename.size();
        size_t u = _countof(PDF_EXTENSION)-1;
        if(n>u && _stricmp(filename.c_str()+(n-u),PDF_EXTENSION) == 0 )
          this->syncfilename = filename.replace(n-u, u, PDFSYNC_EXTENSION);
        else {
          size_t u = _countof(PDFSYNC_EXTENSION)-1;
          _ASSERT(n>u && strcmp(filename.c_str()+(n-u),PDFSYNC_EXTENSION) == 0 );
        }
        this->record2src_decitree = NULL;
        this->index_lost = true;
    }
    
    int rebuild_index();
    UINT pdf_to_source(UINT sheet, UINT x, UINT y, PSTR filename, UINT cchFilename, UINT *line, UINT *col);
    UINT source_to_pdf(PCTSTR srcfilename, UINT line, UINT col, UINT *page, UINT *x, UINT *y);

    void lose_index() { this->index_lost = true;}
    bool is_index_lost() { return this->index_lost; }

private:
    record2srcfile_node *build_decision_tree(int leftrecord, int rightrecord);
    void delete_decision_tree(record2srcfile_node *root);
    int get_record_section(int record_index);
    int scan_and_build_index(FILE *fp);

private:
    hash_map<int, pdfsheet_indexentry, hash_compare<int, less<int> >> pdfsheet_index;
    hash_map<string, src_scope> src_scopes;
    vector<record_section> record_sections;
    record2srcfile_node *record2src_decitree;
    string syncfilename;
    bool index_lost; // true if the index needs to be recomputed (it needs to be set to true when a change to the pdfsync file is detected)
};