/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. 
   Take all the code you want, we'll just write more.
*/

#include "base_util.h"
#include "strlist_util.h"
#include "str_util.h"
#include "wstr_util.h"

int StrList_Len(StrList **root)
{
    int         len = 0;
    StrList *   cur;
    assert(root);
    if (!root)
        return 0;
    cur = *root;
    while (cur) {
        ++len;
        cur = cur->next;
    }
    return len;
}

int WStrList_Len(WStrList **root)
{
    return StrList_Len((StrList**)root);
}

BOOL StrList_InsertAndOwn(StrList **root, char *txt)
{
    StrList *   el;
    assert(root && txt);
    if (!root || !txt)
        return FALSE;

    el = SA(StrList);
    if (!el)
        return FALSE;
    el->str = txt;
    el->next = *root;
    *root = el;
    return TRUE;
}

BOOL WStrList_InsertAndOwn(WStrList **root, WCHAR *txt)
{
    return StrList_InsertAndOwn((StrList**)root, (char*)txt);
}

BOOL StrList_Insert(StrList **root, char *txt)
{
    char *txtDup;

    assert(root && txt);
    if (!root || !txt)
        return FALSE;
    txtDup = str_dup(txt);
    if (!txtDup)
        return FALSE;

    if (!StrList_InsertAndOwn(root, txtDup)) {
        free((void*)txtDup);
        return FALSE;
    }
    return TRUE;
}

BOOL WStrList_Insert(WStrList **root, WCHAR *txt)
{
    WCHAR *txtDup;

    assert(root && txt);
    if (!root || !txt)
        return FALSE;
    txtDup = wstr_dup(txt);
    if (!txtDup)
        return FALSE;

    if (!WStrList_InsertAndOwn(root, txtDup)) {
        free((void*)txtDup);
        return FALSE;
    }
    return TRUE;
}

void StrList_Destroy(StrList **root)
{
    StrList *   cur;
    StrList *   next;

    if (!root)
        return;
    cur = *root;
    while (cur) {
        next = cur->next;
        free((void*)cur->str);
        free((void*)cur);
        cur = next;
    }
    *root = NULL;
}

void WStrList_Destroy(WStrList **root)
{
    StrList_Destroy((StrList**)root);

}

void StrList_Reverse(StrList **strListRoot)
{
    StrList *newRoot = NULL;
    StrList *cur, *next;
    if (!strListRoot) 
        return;
    cur = *strListRoot;
    while (cur) {
        next = cur->next;
        cur->next = newRoot;
        newRoot = cur;
        cur = next;
    }
    *strListRoot = newRoot;
}

void WStrList_Reverse(WStrList **strListRoot)
{
    StrList_Reverse((StrList **)strListRoot);
}
