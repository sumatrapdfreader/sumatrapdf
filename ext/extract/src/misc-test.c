#include "memento.h"
#include "xml.h"

#include <errno.h>
#include <stdio.h>


static int s_num_fails = 0;

static void s_check(
        int         values_equal,
        const char* text,
        int         ret,
        const char* value_s,
        int         errno_,
        const char* value_expected_s,
        int         errno_expected
        )
{
    int ok;
    if (errno_expected) {
        ok = (ret == -1 && errno_ == errno_expected);
    }
    else {
        ok = (ret == 0 && values_equal);
    }
    
    if (ok) printf("    ok:  ");
    else printf("    fail:");
    printf(" text=%16s", text);
    if (errno_expected) printf(" errno_expected=%6i", errno_expected);
    else                printf(" value_expected=%6s", value_expected_s);
    printf(". result: ret=%2i value=%6s errno=%3i", ret, value_s, errno_);
    printf(".\n");
    if (!ok) s_num_fails += 1;
}

static void s_check_int(const char* text, int value_expected, int expected_errno)
{
    int     value;
    int     ret = extract_xml_str_to_int(text, &value);
    char    value_s[32];
    char    value_expected_s[32];
    snprintf(value_s, sizeof(value_s), "%i", value);
    snprintf(value_expected_s, sizeof(value_expected_s), "%i", value_expected);
    s_check(value == value_expected, text, ret, value_s, errno, value_expected_s, expected_errno);
    return;
}

static void s_check_uint(const char* text, unsigned expected_value, int expected_errno)
{
    unsigned    value;
    int         ret = extract_xml_str_to_uint(text, &value);
    char        value_s[32];
    char        value_expected_s[32];
    snprintf(value_s, sizeof(value_s), "%u", value);
    snprintf(value_expected_s, sizeof(value_expected_s), "%u", value);
    s_check(value == expected_value, text, ret, value_s, errno, value_expected_s, expected_errno);
    return;
}

int main(void)
{
    printf("testing extract_xml_str_to_int():\n");
    s_check_int("2", 2, 0);
    s_check_int("-20", -20, 0);
    s_check_int("-20b", 0, EINVAL);
    s_check_int("123456789123", 0, ERANGE);
    
    printf("testing extract_xml_str_to_uint():\n");
    s_check_uint("2", 2, 0);
    s_check_uint("-20", 0, ERANGE);
    s_check_uint("-20b", 0, EINVAL);
    s_check_uint("123456789123", 0, ERANGE);
    
    printf("s_num_fails=%i\n", s_num_fails);
    
    if (s_num_fails) {
        printf("Failed\n");
        return 1;
    }
    else {
        printf("Succeeded\n");
        return 0;
    }
}
