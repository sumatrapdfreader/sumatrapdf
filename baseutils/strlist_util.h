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

#ifdef _UNICODE
#define TStrList                WStrList
#define TStrList_Len            WStrList_Len
#define TStrList_InsertAndOwn   WStrList_InsertAndOwn
#define TStrList_Insert         WStrList_Insert
#define TStrList_Destroy        WStrList_Destroy
#define TStrList_Reverse        WStrList_Reverse
#else
#define TStrList                StrList
#define TStrList_Len            StrList_Len
#define TStrList_InsertAndOwn   StrList_InsertAndOwn
#define TStrList_Insert         StrList_Insert
#define TStrList_Destroy        StrList_Destroy
#define TStrList_Reverse        StrList_Reverse
#endif


#ifdef __cplusplus
}
#endif

#endif
