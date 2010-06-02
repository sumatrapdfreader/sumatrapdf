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

char *StrList_Join(StrList *strListRoot, char *joint)
{
    StrList *next;
    int len = 0;
    int jointLen = joint ? str_len(joint) : 0;
    char *result, *tmp;

    for (next = strListRoot; next; next = next->next)
        len += str_len(next->str) + jointLen;
    len -= jointLen;
    if (len <= 0)
        return str_dup("");

    result = malloc(len + 1);
    if (!result)
        return NULL;

    for (next = strListRoot, tmp = result; next; next = next->next)
    {
        strcpy(tmp, next->str);
        tmp += str_len(next->str);
        if (jointLen > 0 && next->next) {
            strcpy(tmp, joint);
            tmp += jointLen;
        }
    }

    return result;
}

WCHAR *WStrList_Join(WStrList *strListRoot, WCHAR *joint)
{
    WStrList *next;
    int len = 0;
    int jointLen = joint ? wstr_len(joint) : 0;
    WCHAR *result, *tmp;

    for (next = strListRoot; next; next = next->next)
        len += wstr_len(next->str) + jointLen;
    len -= jointLen;
    if (len <= 0)
        return wstr_dup(L"");

    result = malloc((len + 1) * sizeof(WCHAR));
    if (!result)
        return NULL;

    for (next = strListRoot, tmp = result; next; next = next->next)
    {
        lstrcpyW(tmp, next->str);
        tmp += wstr_len(next->str);
        if (jointLen > 0 && next->next) {
            lstrcpyW(tmp, joint);
            tmp += jointLen;
        }
    }

    return result;
}
