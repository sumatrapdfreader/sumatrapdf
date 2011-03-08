/* Copyright Krzysztof Kowalczyk 2006-2011
   License: GPLv3 */
#ifndef FileHistory_h
#define FileHistory_h

#include "DisplayState.h"

#define INVALID_MENU_ID (unsigned int)-1

class FileHistoryNode {
public:
    FileHistoryNode(const TCHAR *filePath=NULL);
    ~FileHistoryNode() { delete next; }

    FileHistoryNode *   next;
    FileHistoryNode *   prev;
    unsigned int        menuId;
    DisplayState        state;
};

class FileHistoryList {
public:
    FileHistoryList() : first(NULL) { }
    ~FileHistoryList() { delete first; }

    void                Prepend(FileHistoryNode *node);
    void                Append(FileHistoryNode *node);
    FileHistoryNode *   Find(const TCHAR *filePath);
    FileHistoryNode *   Find(unsigned int menuId);
    void                Remove(FileHistoryNode *node);

    FileHistoryNode *   first;
};

#endif
