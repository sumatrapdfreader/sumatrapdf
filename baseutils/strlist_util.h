/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. */
#ifndef STRLIST_UTIL_H_
#define STRLIST_UTIL_H_

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct StrList {
    struct StrList *    next;
    char *              str;
} StrList;

typedef struct WStrList {
    struct WStrList *    next;
    WCHAR *              str;
} WStrList;

int     StrList_Len(StrList **root);
BOOL    StrList_InsertAndOwn(StrList **root, char *txt);
BOOL    StrList_Insert(StrList **root, char *txt);
void    StrList_Destroy(StrList **root);
void    StrList_Reverse(StrList **strListRoot);

int     WStrList_Len(WStrList **root);
BOOL    WStrList_InsertAndOwn(WStrList **root, WCHAR *txt);
BOOL    WStrList_Insert(WStrList **root, WCHAR *txt);
void    WStrList_Destroy(WStrList **root);
void    WStrList_Reverse(WStrList **strListRoot);

#ifdef __cplusplus
}
#endif

#endif
