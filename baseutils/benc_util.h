/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. */
#ifndef BENC_UTIL_H_
#define BENC_UTIL_H_

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum benc_type {
    BOT_INT64 = 1,
    BOT_STRING,
    BOT_ARRAY,
    BOT_DICT
} benc_type;

typedef struct benc_obj {
    benc_type m_type;
} benc_obj;

typedef struct benc_int64 {
    benc_type  m_type;
    int64_t    m_val;
} benc_int64;

typedef struct benc_str {
    benc_type  m_type;
    size_t     m_len;
    char *     m_str;
} benc_str;

/* Note: it's important that the layout of m_next and m_used is the same in
   benc_array_data and benc_dict_data, as code relies on that */
typedef struct benc_array_data {
    struct benc_array_data * m_next;
    size_t                   m_used;
    size_t                   m_allocated;    
    benc_obj **              m_data;
} benc_array_data;

typedef struct benc_dict_data {
    struct benc_dict_data * m_next;
    size_t                  m_used;
    size_t                  m_allocated;
    char **                 m_keys;
    benc_obj **             m_values;
} benc_dict_data;

typedef struct benc_array {
    benc_type       m_type;
    benc_array_data m_data;
} benc_array;

typedef struct benc_dict {
    benc_type       m_type;
    benc_dict_data  m_data;
} benc_dict;

BOOL        int64_to_string(int64_t val, char* data, size_t dataLen);
BOOL        int64_to_string_zero_pad(int64_t val, size_t pad, char* data, size_t dataLen);

benc_int64* benc_int64_new(int64_t val);
benc_str *  benc_str_new(const char* data, size_t len);
benc_array* benc_array_new(void);
benc_dict * benc_dict_new(void);

benc_obj *  benc_obj_from_data(const char *data, size_t len);
char *      benc_obj_to_data(benc_obj *bobj, size_t* lenOut);
void        benc_obj_delete(benc_obj *);    

size_t      benc_obj_len(benc_obj* );

benc_int64* benc_obj_as_int64(benc_obj *);
benc_str*   benc_obj_as_str(benc_obj *);
benc_array* benc_obj_as_array(benc_obj *);
benc_dict * benc_obj_as_dict(benc_obj *);

size_t      benc_array_len(benc_array *);
BOOL        benc_array_append(benc_array* arr, benc_obj* bobj);
void        benc_array_delete(benc_array *);
benc_obj *  benc_array_get(benc_array *bobj, size_t idx);
BOOL        benc_array_get_int(benc_array *bobj, size_t idx, int *valOut);

size_t      benc_dict_len(benc_dict *bobj);
BOOL        benc_dict_insert(benc_dict* dict, const char* key, size_t keyLen, benc_obj* val);
BOOL        benc_dict_insert2(benc_dict* dict, const char* key, benc_obj* val);
BOOL        benc_dict_insert_str(benc_dict* dict, const char* key, const char *str);
BOOL        benc_dict_insert_wstr(benc_dict* dict, const char* key, const WCHAR *str);
BOOL        benc_dict_insert_tstr(benc_dict* dict, const char* key, const TCHAR *str);
BOOL        benc_dict_insert_int64(benc_dict* dict, const char* key, int64_t val);
benc_obj*   benc_dict_find(benc_dict* dict, const char* key, size_t keyLen);
benc_obj*   benc_dict_find2(benc_dict* dict, const char* key);
BOOL        dict_get_bool(benc_dict* dict, const char* key, BOOL* valOut);
BOOL        dict_get_int(benc_dict* dict, const char* key, int* valOut);
const char* dict_get_str(benc_dict* dict, const char* key);
BOOL        dict_get_float_from_str(benc_dict* dict, const char* key, float* valOut);
BOOL        dict_get_double_from_str(benc_dict* dict, const char* key, double* valOut);

void        u_benc_all(void);


#ifdef __cplusplus
}
#endif

#endif

