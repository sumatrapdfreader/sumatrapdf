/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code.

   Handling of bencoded format. See:
   http://www.bittorrent.org/protocol.html or 
   http://en.wikipedia.org/wiki/Bencode or
   http://wiki.theory.org/BitTorrentSpecification
*/
#include "base_util.h"
#include "benc_util.h"
#include "tstr_util.h"

#define IVALID_LEN (size_t)-1

typedef enum phase_t {
 PHASE_CALC_LEN = 1,
 PHASE_FORM_DATA = 2
} phase_t;

static size_t _benc_obj_to_data(benc_obj* bobj, phase_t phase, char* data);
static benc_obj *_benc_obj_from_data(const char** dataInOut, size_t* lenInOut, BOOL must_be_last);

#define IS_DIGIT(c) (((c) >= '0') && ((c) <= '9'))

benc_int64 *benc_int64_new(int64_t val)
{
    benc_int64 *bobj = SAZ(benc_int64);
    if (!bobj) return NULL;
    bobj->m_type = BOT_INT64;
    bobj->m_val = val;
    return bobj;
}

benc_str *benc_str_new(const char* data, size_t len)
{
    benc_str *bobj = SAZ(benc_str);
    if (!bobj) return NULL;
    bobj->m_type = BOT_STRING;
    bobj->m_len = len;
    bobj->m_str = str_dupn(data, len);
    return bobj;
}


benc_array *benc_array_new(void)
{
    benc_array *bobj = SAZ(benc_array);
    if (!bobj) return NULL;
    bobj->m_type = BOT_ARRAY;
    bobj->m_data.m_next = NULL;
    bobj->m_data.m_used = 0;
    bobj->m_data.m_allocated = 0;
    bobj->m_data.m_data = 0;
    return bobj;
}

benc_dict *benc_dict_new(void)
{
    benc_dict *bobj = SAZ(benc_dict);
    if (!bobj) return NULL;
    bobj->m_type = BOT_DICT;
    bobj->m_data.m_next = NULL;
    bobj->m_data.m_used = 0;
    bobj->m_data.m_allocated = 0;
    bobj->m_data.m_keys = NULL;
    bobj->m_data.m_values = NULL;
    return bobj;
}

static BOOL _is_valid_benc_int(char c)
{
    if (c == 'e') return TRUE;
    if (IS_DIGIT(c)) return TRUE;
    return FALSE;
}

static BOOL _parse_int64(const char** dataInOut, size_t* lenInOut, int64_t* valOut)
{
    const char* data = *dataInOut;
    size_t      len = *lenInOut;
    BOOL        wasMinus = FALSE;
    BOOL        validNumber = FALSE;
    int64_t     val = 0;
    int64_t     digit;
    char        c;

    if ((len > 0) && ('-' == *data)) {
        ++data;
        --len;
        wasMinus = TRUE;
    }

    for (;;) {
        if (len <= 0)
            return FALSE;
        c = *data++;
        len--;
        if (!_is_valid_benc_int(c))
            return FALSE;
        if ('e' == c)
            break;
        digit = (int64_t) (c - '0');
        assert( (digit >= 0) && (digit <=9) );
        validNumber = TRUE;
        val = (val * 10) + digit;
    }
    *dataInOut = data;
    *lenInOut = len;
    if (wasMinus) {
        val = -val;
        if (0 == val) /* i-0e is not valid */
            validNumber = FALSE;
    }
    *valOut = val;
    return validNumber;
}

/* Parse a lenght of a string in form "\d+:".
   Return a lenght of a string or IVALID_LEN if there was an error */
static size_t _parse_str_len(const char** dataInOut, size_t* lenInOut)
{
    const char *    data = *dataInOut;
    size_t          len = *lenInOut;
    size_t          strLen = 0;
    BOOL            validNumber = FALSE;
    BOOL            endsWithColon = FALSE;
    char            c;
    size_t          digit;

    if (!data)
        return IVALID_LEN;

    while (len > 0) {
        c = *data++;
        --len;
        if (IS_DIGIT(c)) {
            validNumber = TRUE;
            digit = (size_t)(c - '0');
            assert(digit <= 9);
            strLen = (strLen * 10) + digit;
        } else if (':' == c) {
            endsWithColon = TRUE;
            break;
        } else {
            /* not valid */
            return IVALID_LEN;
        }
    }

    if (!validNumber || !endsWithColon)
        return IVALID_LEN;
    *dataInOut = data;
    *lenInOut = len;
    return strLen;
}

static size_t _parse_str_help(const char** dataInOut, size_t* lenInOut)
{
    size_t strLen = _parse_str_len(dataInOut, lenInOut);
    if (IVALID_LEN == strLen)
        return IVALID_LEN;
    if (strLen > *lenInOut)
        return IVALID_LEN;
    return strLen;
}

static benc_str* _parse_str(const char **dataInOut, size_t* lenInOut)
{
    benc_str* bobj;
    size_t strLen = _parse_str_help(dataInOut, lenInOut);
    if (IVALID_LEN == strLen)
        return NULL;
    bobj = benc_str_new(*dataInOut, strLen);
    *dataInOut += strLen;
    *lenInOut -= strLen;
    return bobj;
}

/* Given a current size of the array, decide how many new 
   (used when growing array to make space for new items).
   Each array item is a pointer, so uses 4 bytes. We want to strike a balance
   between not allocating too much (don't waste memory for small arrays) and
   creating too many segments (for large arrays). Those numbers should be
   based on profiling, but for now I just made them up. */
static size_t _array_grow_by(size_t currSize)
{
    if (currSize < 8)
        return 8;
    else if (currSize < 32)
        return 32;
    else if (currSize < 128)
        return 128;
    else if (currSize < 1024)
        return 1024;

    return currSize; /* grow by 2.0 */
}

static benc_array_data* _array_get_free_slot(benc_array_data* curr, size_t *slotIdxOut)
{
    size_t curr_size = curr->m_used;
    while (curr->m_next) {
        curr = curr->m_next;
        curr_size += curr->m_used;
    }
    if (curr->m_used == curr->m_allocated) {
        /* no space left, need to allocate next segment */
        size_t grow_by = _array_grow_by(curr_size);
        benc_obj** new_data = SAZA(benc_obj*, grow_by);
        if (!new_data)
            return NULL;

        if (0 != curr->m_allocated) {
            assert(curr->m_data);
            assert(!curr->m_next);
            curr->m_next = SAZ(benc_array_data);
            if (!curr->m_next) {
                free(new_data);
                return NULL;
            }
            curr = curr->m_next;
        } else {
            assert(!curr->m_data);
        }
        assert(!curr->m_data);
        curr->m_data = new_data;
        curr->m_allocated = grow_by;
        curr->m_used = 0;
        curr->m_next = NULL;
    }
    assert(curr->m_allocated > 0);
    assert(curr->m_data);
    assert(curr->m_used < curr->m_allocated);
    *slotIdxOut = curr->m_used;
    return curr;    
}

BOOL benc_array_append(benc_array* arr, benc_obj* bobj)
{
    benc_array_data *   data;
    size_t              slotIdx;

    assert(arr);
    assert(bobj);

    if (!arr || !bobj)
        return FALSE;

    data = _array_get_free_slot(&(arr->m_data), &slotIdx);
    if (!data)
        return FALSE;

    assert(slotIdx == data->m_used);
    data->m_data[slotIdx] = bobj;
    data->m_used++;
    assert(data->m_used <= data->m_allocated);
    return TRUE;
}

/* When we need to grow the dict, returns the size of new dictionary based on
   current size.
   We need to strike a balance between memory usage (don't use too much memory)
   and speed (don't reallocate too often).
   Those numbers should come from profiling, but for now I'm just guessing.
*/
size_t _dict_calc_new_size(size_t currSize)
{
    if (currSize < 8)
        return 8;
    else if (currSize < 32)
        return 32;
    else if (currSize < 128)
        return 128;
    else if (currSize < 1024)
        return 1024;
    return currSize * 2;
}

/* Return 1 if val1 > val2
          0 if val1 == val2
         -1 if val1 < val2
   Compares raw bytes
*/
int _compare_dict_keys(const unsigned char* val1, const unsigned char* val2, size_t val2Len)
{
    unsigned char c1;
    unsigned char c2;
    for (;;) {
        if (0 == val2Len) {
            if (0 == *val1)
                return 0;
            else
                return 1;
        }
        c1 = *val1;
        c2 = *val2;
        if (c1 > c2)
            return 1;
        if (c1 < c2)
            return -1;
        ++val1;
        ++val2;
        --val2Len;
    }
}

/* Find a place to insert a value with a given key. If needed, allocates
   additional memory for data.
   Per bencoding spec, keys must be ordered alphabetically when serialized,
   so we insert them in sorted order. This might be expensive due to lots of
   memory copying for lots of insertions in random order (more efficient would
   probably be: append at the end and sort when insertions are done).
   TODO: This is a naive implementation that doesn't use segments but always
   reallocates data so that keys and values are always in one array.
   This simplifies insertion logic.
*/
static benc_dict_data* 
_dict_get_insert_slot(benc_dict_data* curr, const char* key, size_t keyLen, size_t* slotIdxOut)
{
    size_t slotIdx, used;
    int cmp;
    const unsigned char* currKey;
    assert(curr && key && slotIdxOut);
    assert(NULL == curr->m_next); /* not using segments yet */

    used = curr->m_used;
    if (used == curr->m_allocated) {
        /* need more space */
        benc_obj **values;
        char **keys;
        size_t new_size = _dict_calc_new_size(used);
        keys = SAZA(char*, new_size);
        if (!keys)
            return NULL;
        values = SAZA(benc_obj*, new_size);
        if (!values) {
            free(keys);
            return NULL;
        }
        if (used > 0) {
            memcpy(keys, curr->m_keys, used * sizeof(char**));
            memcpy(values, curr->m_values, used * sizeof(benc_obj*));
            assert(curr->m_keys);
            free(curr->m_keys);
            assert(curr->m_values);
            free(curr->m_values);
        }
        curr->m_keys = keys;
        curr->m_values = values;
        curr->m_allocated = new_size;
    }
    assert(curr->m_used < curr->m_allocated);

    /* TODO: brain dead linear search. Replace with binary search */
    slotIdx = 0;
    while (slotIdx < used) {
        currKey = (const unsigned char*)curr->m_values[slotIdx];
        cmp = _compare_dict_keys(currKey, key, keyLen);
        if (0 == cmp) {
            /* same as existing key: insert at exactly this position */
            break;
        }
        if (1 == cmp) {
            /* currKey > key: make space for data at this position */
            size_t toMove = used - slotIdx;
            char ** keys_start = curr->m_keys + slotIdx;
            benc_obj** vals_start = curr->m_values + slotIdx;
            memmove(keys_start+1, keys_start, toMove * sizeof(char*));
            memmove(vals_start+1, vals_start, toMove * sizeof(benc_obj*));
            *keys_start = NULL;
            *vals_start = NULL;
            break;
        }
        ++slotIdx;
    }
    *slotIdxOut = slotIdx;
    return curr;
}

BOOL benc_dict_insert2(benc_dict* dict, const char* key, benc_obj* val)
{
    return benc_dict_insert(dict, key, strlen(key), val);
}

BOOL benc_dict_insert_int64(benc_dict* dict, const char* key, int64_t val)
{
    benc_obj * bobj = (benc_obj*)benc_int64_new(val);
    if (!bobj)
        return FALSE;
    if (!benc_dict_insert2(dict, key, bobj)) {
        benc_obj_delete(bobj);
        return FALSE;
    }
    return TRUE;
}

BOOL benc_dict_insert_str(benc_dict* dict, const char* key, const char *str)
{
    benc_obj * bobj = (benc_obj*)benc_str_new(str, strlen(str));
    if (!bobj)
        return FALSE;
    if (!benc_dict_insert2(dict, key, bobj)) {
        benc_obj_delete(bobj);
        return FALSE;
    }
    return TRUE;
}

BOOL benc_dict_insert_wstr(benc_dict* dict, const char* key, const WCHAR *str)
{
    BOOL ret;
    char *utf8_str = wstr_to_utf8(str);
    if (!utf8_str)
        return FALSE;
    ret = benc_dict_insert_str(dict, key, utf8_str);
    free(utf8_str);
    return ret;
}

BOOL benc_dict_insert_tstr(benc_dict* dict, const char* key, const TCHAR *str)
{
    BOOL ret;
    char *utf8_str = tstr_to_utf8(str);
    if (!utf8_str)
        return FALSE;
    ret = benc_dict_insert_str(dict, key, utf8_str);
    free(utf8_str);
    return ret;
}

BOOL benc_dict_insert(benc_dict* dict, const char* key, size_t keyLen, benc_obj* val)
{
    benc_dict_data *    data;
    size_t              slotIdx;

    if (!dict || !key || !val)
        return FALSE;

    data = _dict_get_insert_slot(&(dict->m_data), key, keyLen, &slotIdx);
    if (!data)
        return FALSE;

    /* TODO: handle case where key already exists */
    data->m_values[slotIdx] = val;
    data->m_keys[slotIdx] = str_dupn(key, keyLen);
    data->m_used++;
    assert(data->m_used <= data->m_allocated);
    return TRUE;
}

/* Return value for a given 'key' of 'keyLen' length.
   Returns NULL if 'key' not found.
*/
benc_obj * benc_dict_find(benc_dict* dict, const char* key, size_t keyLen)
{
    size_t          slotIdx;
    benc_dict_data* curr;
    size_t          used;

    if (!dict || !key)
        return NULL;

    curr = &(dict->m_data);
    assert(!curr->m_next); /* not using segments yet */
    used = curr->m_used;
    /* TODO: brain dead linear search. Replace with binary search */
    slotIdx = 0;
    while (slotIdx < used) {
        const unsigned char* currKey = (const unsigned char*)curr->m_keys[slotIdx];
        int cmp = _compare_dict_keys(currKey, key, keyLen);
        if (0 == cmp)
            return curr->m_values[slotIdx];
        ++slotIdx;
    }
    return NULL;
}

benc_obj* benc_dict_find2(benc_dict* dict, const char* key)
{
    assert(key);
    if (!key) return NULL;
    return benc_dict_find(dict, key, strlen(key));
}

BOOL dict_get_bool(benc_dict* dict, const char* key, BOOL* valOut)
{
    benc_obj* obj = benc_dict_find2(dict, key);
    benc_int64* val = benc_obj_as_int64(obj);
    if (!val) return FALSE;
    *valOut = (BOOL) val->m_val;
    return TRUE;
}

BOOL dict_get_int(benc_dict* dict, const char* key, int* valOut)
{
    benc_obj* obj = benc_dict_find2(dict, key);
    benc_int64* val = benc_obj_as_int64(obj);
    if (!val) return FALSE;
    *valOut = (int) val->m_val;
    return TRUE;
}

const char* dict_get_str(benc_dict* dict, const char* key)
{
    benc_obj* obj = benc_dict_find2(dict, key);
    benc_str* val = benc_obj_as_str(obj);
    if (!val) return NULL;
    return (const char*)val->m_str;
}

BOOL dict_get_float_from_str(benc_dict* dict, const char* key, float* valOut)
{
    const char *str = dict_get_str(dict, key);
    if (!str) return FALSE;
    return sscanf(str, "%f", valOut) == 1;
}

BOOL dict_get_double_from_str(benc_dict* dict, const char* key, double* valOut)
{
    const char *str = dict_get_str(dict, key);
    if (!str) return FALSE;
    return sscanf(str, "%lf", valOut) == 1;
}

static benc_array * _parse_array(const char** dataInOut, size_t* lenInOut)
{
    char            c;
    const char *    data = *dataInOut;
    size_t          len = *lenInOut;
    benc_obj *      bobj;

    benc_array *bobj_arr = benc_array_new();
    if (!bobj_arr)
        return NULL;
    while (len > 0) {
        c = *data;
        if ('e' == c) {
            ++data;
            --len;
            goto FoundArrayEnd;
        }
        bobj = _benc_obj_from_data(&data, &len, FALSE);
        if (!bobj)
            break;
        if (!benc_array_append(bobj_arr, bobj))
            break;
    }
    // error happened
    benc_obj_delete((benc_obj*) bobj_arr);
    return NULL;
FoundArrayEnd:
    *dataInOut = data;
    *lenInOut = len;
    return bobj_arr;
}

static benc_dict *_parse_dict(const char** dataInOut, size_t* lenInOut)
{
    char            c;
    const char *    data = *dataInOut;
    size_t          len = *lenInOut;
    const char *    key;
    size_t          keyLen;
    benc_obj *      val = NULL;

    benc_dict * dict = benc_dict_new();
    if (!dict)
        return NULL;

    while (len > 0) {
        c = *data;
        if ('e' == c) {
            ++data;
            --len;
            goto FoundDictEnd;
        }
        keyLen = _parse_str_help(&data, &len);
        if (IVALID_LEN == keyLen)
            break;
        key = data;
        data += keyLen;
        len -= keyLen;
        assert(len > 0);

        val = _benc_obj_from_data(&data, &len, FALSE);
        if (!val)
            break;

        if (!benc_dict_insert(dict, key, keyLen, val))
            break;
    }
    // error happened
    if (val)
        benc_obj_delete(val);
    benc_obj_delete((benc_obj*) dict);
    return NULL;
FoundDictEnd:
    *dataInOut = data;
    *lenInOut = len;
    return dict;

}

static benc_obj *_benc_obj_from_data(const char** dataInOut, size_t* lenInOut, BOOL must_be_last)
{
    char            c;
    BOOL            ok;
    int64_t         int64val;
    benc_obj *      bobj;
    const char *    data = *dataInOut;
    size_t          len = *lenInOut;

    /*assert(data);*/
    /*assert(len > 0);*/
    if (!data || (len <= 0)) return NULL;

    while (len >= 0) {
        c = *data++;
        --len;
        if ('i' == c) {
            ok = _parse_int64(&data, &len, &int64val);
            if (!ok)
                return NULL;
            bobj = (benc_obj*) benc_int64_new(int64val);
            break;
        } else if ('d' == c) {
            bobj = (benc_obj*) _parse_dict(&data, &len);
            break;
        } else if ('l' == c) {
            bobj = (benc_obj*) _parse_array(&data, &len);
            break;
        } else {
            /* must be string */
            --data;
            ++len;
            bobj = (benc_obj*)_parse_str(&data, &len);
            break;
        }
    }

    if (bobj && must_be_last && (len > 0)) {
        benc_obj_delete(bobj);
        return NULL;
    }
    *dataInOut = data;
    *lenInOut = len;
    return bobj;
}

benc_obj *benc_obj_from_data(const char *data, size_t len)
{
    return _benc_obj_from_data(&data, &len, TRUE);
}

static size_t _len_for_int64(benc_int64* bobj)
{
    /* 2 is for "i" and "e" */
    return 2 + digits_for_number(bobj->m_val);
}

static size_t _len_for_txt(const char* txt)
{
    size_t txt_len = strlen(txt);
    return 1 + txt_len + digits_for_number(txt_len);
}

static size_t _len_for_str(benc_str* bobj)
{
    /* 1 is for ":" */
    return 1 + bobj->m_len + digits_for_number(bobj->m_len);
}

static size_t _len_for_array(benc_array* bobj)
{
    size_t len = 2;
    benc_array_data *data = &(bobj->m_data);
    assert(BOT_ARRAY == bobj->m_type);

    while (data) {
        size_t i;
        benc_array_data *next = data->m_next;
        for (i=0; i < data->m_used; i++) {
            len += _benc_obj_to_data(data->m_data[i], PHASE_CALC_LEN, NULL);
        }
        data = next;
    }
    return len;
}

static size_t _len_for_dict(benc_dict* bobj)
{
    size_t len = 2;
    benc_dict_data *data = &(bobj->m_data);
    assert(BOT_DICT == bobj->m_type);
    while (data) {
        size_t i;
        benc_dict_data *next = data->m_next;
        for (i=0; i < data->m_used; i++) {
            len += _len_for_txt(data->m_keys[i]);
            len += _benc_obj_to_data(data->m_values[i], PHASE_CALC_LEN, NULL);
        }
        data = next;
    }
    return len;
}

static void _str_reverse(char *start, char *end)
{
    while (start < end) {
        char tmp = *start;
        *start++ = *end;
        *end-- = tmp;
    }
}

/* Convert 'val' to string in a buffer 'data' of size 'dataLen'. NULL-terminates
   the string.
   Returns FALSE if not enough space in the buffer.
   TODO: should return the size of needed buffer.
   TODO: for simplicity of code, if buffer is not big enough, will not use
         the last byte of the buffer
   TODO: move it to some other place like str_util.[h|c]?
*/
BOOL int64_to_string(int64_t val, char* data, size_t dataLen)
{
    char *  numStart;
    size_t  dataLenRest = dataLen;

    if (val < 0) {
        if (dataLenRest < 2) {
            *data = 0;
            return FALSE;
        }
        *data++ = '-';            
        --dataLenRest;
        val = -val;
    }
    numStart = data;
    while (val > 9) {
        int digit = (int)(val % 10);
        if (dataLenRest < 2) {
            *data = 0;
            return FALSE;
        }
        *data++ = (char)(digit + '0');
        --dataLenRest;
        val = val / 10;
    }
    if (dataLenRest < 2) {
        *data = 0;
        return FALSE;
    }
    data[0] = (char)(val + '0');
    data[1] = 0;
    _str_reverse(numStart, data);
    return TRUE;
}

BOOL int64_to_string_zero_pad(int64_t val, size_t pad, char* data, size_t dataLen)
{
    size_t len;
    assert(dataLen >= pad + 1);
    if (dataLen < pad + 1)
        return FALSE;
    if (!int64_to_string(val, data, dataLen))
        return FALSE;
    len = strlen(data);
    if (len < pad) {
        size_t toPad = pad - len;
        size_t toMove = len + 1;
        memmove(data + toPad, data, toMove);
        while (toPad != 0) {
            data[toPad-1] = '0';
            --toPad;
        }
    }
    return TRUE;
}

static size_t _serialize_int64_num(int64_t val, char* data)
{
    char *  numStart;
    size_t  len;
    char *  start = data;
    if (val < 0) {
        *data++ = '-';
        val = -val;
    }
    numStart = data;
    while (val > 9) {
        int digit = (int)(val % 10);
        *data++ = (char)(digit + '0');
        val = val / 10;
    }
    *data = (char)(val + '0');
    _str_reverse(numStart, data);
    len = data + 1 - start;
    return len;
}

static size_t _serialize_int64(benc_int64* bobj, char* data)
{
    size_t len;
    *data++ = 'i';
    len = _serialize_int64_num(bobj->m_val, data);
    data += len;
    *data++ = 'e';
    len += 2;
    assert(len == _len_for_int64(bobj));
    return len;
}

static size_t _serialize_str_help(char *str, size_t strLen, char* data)
{
    size_t len = _serialize_int64_num(strLen, data);
    data += len;
    *data++ = ':';
    memcpy(data, str, strLen);
    return len + 1 + strLen;
}

static size_t _serialize_str(benc_str* bobj, char* data)
{
    size_t len = _serialize_str_help(bobj->m_str, bobj->m_len, data);
    assert(len == _len_for_str(bobj));
    return len;
}

static size_t _serialize_array(benc_array* arr, char* data)
{
    size_t i;
    size_t len;
    size_t arr_len = benc_array_len(arr);
    char *start = data;
    *data++ = 'l';
    for (i = 0; i < arr_len; i++) {
        benc_obj *bobj = benc_array_get(arr, i);
        size_t len_tmp = _benc_obj_to_data(bobj, PHASE_FORM_DATA, data);
        data += len_tmp;
    }
    *data++ = 'e';
    len = data - start;
    assert(len == _len_for_array(arr));
    return len;
}

static size_t _serialize_dict(benc_dict* dict, char* data)
{
    benc_dict_data * curr;
    size_t len = 2; /* for 'd' and 'e' */
    size_t i, lenTmp, used;

    *data++ = 'd';
    curr = &(dict->m_data);
    assert(!curr->m_next); /* not using segments yet */
    while (curr) {
        used = curr->m_used;
        for (i = 0; i < used; i++) {
            char *key = curr->m_keys[i];
            benc_obj *val = curr->m_values[i];
            lenTmp = _serialize_str_help(key, strlen(key), data);
            data += lenTmp;
            len += lenTmp;
            lenTmp = _benc_obj_to_data(val, PHASE_FORM_DATA, data);
            data += lenTmp;
            len += lenTmp;
        }
        curr = curr->m_next;
    }
    *data++ = 'e';
    assert(len == _len_for_dict(dict));
    return len;
}

static size_t _benc_obj_to_data(benc_obj* bobj, phase_t phase, char* data)
{
    size_t len;
    assert((PHASE_CALC_LEN == phase) || (PHASE_FORM_DATA == phase));
    assert((PHASE_CALC_LEN == phase) || data);

    if (BOT_INT64 == bobj->m_type) {
        if (PHASE_CALC_LEN == phase)
            len = _len_for_int64((benc_int64*)bobj);
        else
            len = _serialize_int64((benc_int64*)bobj, data);
    } else if (BOT_STRING == bobj->m_type) {
        if (PHASE_CALC_LEN == phase)
            len = _len_for_str((benc_str*)bobj);
        else
            len = _serialize_str((benc_str*)bobj, data);
    } else if (BOT_ARRAY == bobj->m_type) {
        if (PHASE_CALC_LEN == phase)
            len = _len_for_array((benc_array*)bobj);
        else
            len = _serialize_array((benc_array*)bobj, data);        
    } else if (BOT_DICT == bobj->m_type) {
        if (PHASE_CALC_LEN == phase)
            len = _len_for_dict((benc_dict*)bobj);
        else
            len = _serialize_dict((benc_dict*)bobj, data);    
    } else {
        assert(0);
        len = 0;
    }
    return len;
}

char * benc_obj_to_data(benc_obj *bobj, size_t* lenOut)
{
    size_t len;
    char *data;
    assert(bobj);
    if (!bobj)
        return NULL;
    len = _benc_obj_to_data(bobj, PHASE_CALC_LEN, NULL);
    assert(len > 0);
    data = (char*)malloc(len+1);
    if (!data)
        return NULL;
    data[len] = 0; /* NULL-terminate to make life easier */
    _benc_obj_to_data(bobj, PHASE_FORM_DATA, data);
    *lenOut = len;
    return data;
}

size_t benc_array_len(benc_array *bobj)
{
    benc_array_data *data = (benc_array_data*) &(bobj->m_data);
    size_t len = data->m_used;
    while (data->m_next) {
        data = data->m_next;
        len += data->m_used;
    }
    return len;        
}

benc_obj *benc_array_get(benc_array *bobj, size_t idx)
{
    benc_array_data *data = (benc_array_data*) &(bobj->m_data);

    while (data) {
        if (idx < data->m_used) {
            return data->m_data[idx];
        }
        idx -= data->m_used;
        data = data->m_next;
    }
    assert(0);  /* asked for an non-existing item */
    return NULL;
}

benc_int64* benc_obj_as_int64(benc_obj *bobj)
{
    if (!bobj)
        return NULL;
    if (BOT_INT64 == bobj->m_type)
        return (benc_int64 *)bobj;
    return NULL;
}

benc_str* benc_obj_as_str(benc_obj *bobj)
{
    if (!bobj)
        return NULL;
    if (BOT_STRING == bobj->m_type)
        return (benc_str *)bobj;
    return NULL;
}

benc_dict* benc_obj_as_dict(benc_obj *bobj)
{
    if (!bobj)
        return NULL;
    if (BOT_DICT == bobj->m_type)
        return (benc_dict *)bobj;
    return NULL;
}

benc_array* benc_obj_as_array(benc_obj *bobj)
{
    if (!bobj)
        return NULL;
    if (BOT_ARRAY == bobj->m_type)
        return (benc_array *)bobj;
    return NULL;
}

size_t benc_dict_len(benc_dict *bobj)
{
    /* A hack, but works because benc_array and benc_dict share enough of
       common data layout */
    return benc_array_len((benc_array*)bobj);
}

size_t benc_obj_len(benc_obj* bobj)
{
    if (BOT_ARRAY == bobj->m_type)
        return benc_array_len((benc_array*)bobj);
    else if (BOT_DICT == bobj->m_type)
        return benc_dict_len((benc_dict*)bobj);
    else
        return -1;
}

void benc_array_delete(benc_array *bobj)
{
    benc_array_data *data = &(bobj->m_data);
    assert(BOT_ARRAY == bobj->m_type);
    while (data) {
        size_t i;
        benc_array_data *next = data->m_next;
        for (i=0; i < data->m_used; i++) {
            benc_obj_delete(data->m_data[i]);
        }
        free(data->m_data);
        if (data != &(bobj->m_data)) {
            /* first m_data is within benc_array, but all others are heap allocated */
            free(data);
        }
        data = next;
    }
}

static void benc_dict_delete(benc_dict *bobj)
{
    benc_dict_data *data = &(bobj->m_data);
    assert(BOT_DICT == bobj->m_type);
    while (data) {
        size_t i;
        benc_dict_data *next = data->m_next;
        for (i=0; i < data->m_used; i++) {
            free(data->m_keys[i]);
            benc_obj_delete(data->m_values[i]);
        }
        free(data->m_values);
        free(data->m_keys);
        data = next;
    }
}

/* Release all memory used by bencoded object */
void benc_obj_delete(benc_obj *bobj)
{
    if (BOT_INT64 == bobj->m_type) {
        /* do nothing */
    } else if (BOT_STRING == bobj->m_type) {
        benc_str *bobj_str = (benc_str*)bobj;
        free(bobj_str->m_str);
    } else if (BOT_ARRAY == bobj->m_type) {
        benc_array_delete((benc_array*)bobj);
    } else if (BOT_DICT == bobj->m_type) {
        benc_dict_delete((benc_dict*)bobj);
    } else {
        assert(0);
    }
    free(bobj);
}

