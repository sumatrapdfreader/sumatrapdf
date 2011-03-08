/* Copyright Krzysztof Kowalczyk 2006-2011
   License: GPLv3 */

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "FileHistory.h"
#include "TStrUtil.h"

/* Handling of file history list.

   We keep an infinite list of all (still existing in the file system) PDF
   files that a user has ever opened. For each file we also keep a bunch of
   attributes describing the display state at the time the file was closed.

   We persist this list as serialized text inside preferences file.
   Serialized history list looks like this:

File History:
  DisplayMode: single page
  File: /home/test.pdf
  PageNo: 5
  ZoomLevel: 123.3434
  FullScreen: 1
  LastAccessTimeInSecsSinceEpoch: 12341234124314

File History:

etc...

    We deserialize this info at startup and serialize when the application
    quits.
*/

FileHistoryNode::FileHistoryNode(const TCHAR *filePath) :
    next(NULL), prev(NULL), menuId(INVALID_MENU_ID)
{
    if (filePath) {
        DBG_OUT_T("FileHistoryNode(filePath='%s')\n", filePath);
        this->state.filePath = (const TCHAR*)StrCopy(filePath);
    }
}

void FileHistoryList::Prepend(FileHistoryNode *node)
{
    assert(node && !node->next && !node->prev);
    if (!node) return;
    node->next = first;
    if (node->next)
        node->next->prev = node;
    first = node;
}

void FileHistoryList::Append(FileHistoryNode *node)
{
    assert(node && !node->next && !node->prev);

    if (!first) {
        first = node;
        return;
    }

    FileHistoryNode *last = first;
    for (; last->next; last = last->next);
    last->next = node;
    node->prev = last;
}

FileHistoryNode *FileHistoryList::Find(const TCHAR *filePath)
{
    for (FileHistoryNode *node = first; node; node = node->next)
        if (tstr_ieq(node->state.filePath, filePath))
            return node;
    return NULL;
}

FileHistoryNode *FileHistoryList::Find(unsigned int menuId)
{
    for (FileHistoryNode *node = first; node; node = node->next)
        if (node->menuId == menuId)
            return node;
    return NULL;
}

void FileHistoryList::Remove(FileHistoryNode *node)
{
    if (node->next)
        node->next->prev = node->prev;

    if (first == node)
        first = node->next;
    else
        node->prev->next = node->next;

    node->next = node->prev = NULL;
    // note: the caller must delete the node, if it's no longer needed
}
