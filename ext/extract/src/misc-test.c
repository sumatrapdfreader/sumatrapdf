#include "memento.h"
#include "xml.h"

#include <errno.h>
#include <stdio.h>


static int s_num_fails = 0;

static void s_check(
		int         values_equal,
		const char *text,
		int         ret,
		const char *value_s,
		int         errno_,
		const char *value_expected_s,
		int         errno_expected
		)
{
	int ok;

	if (errno_expected)
		ok = (ret == -1 && errno_ == errno_expected);
	else
		ok = (ret == 0 && values_equal);

	if (ok)
		printf("    ok:  ");
	else
		printf("    fail:");
	printf(" text=%16s", text);
	if (errno_expected)
		printf(" errno_expected=%6i", errno_expected);
	else
		printf(" value_expected=%6s", value_expected_s);
	printf(". result: ret=%2i value=%6s errno=%3i", ret, value_s, errno_);
	printf(".\n");
	if (!ok)
		s_num_fails += 1;
}

static void s_check_e( int e, const char* text)
{
	if (e)
	{
		s_num_fails += 1;
		printf( "Error: e=%i: %s\n", e, text);
	}
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
}

static void s_check_xml_parse()
{
	int e;
	extract_buffer_t* buffer;
	extract_xml_tag_t tag;
	unsigned i;
	const char* texts[] = {
			"<foo a=1>text</foo>",
			"< >",
			"<foo bar=>",
			"< bar=>",
			"< =>",
			};

	extract_xml_tag_init( &tag);

	for (i=0; i<sizeof(texts) / sizeof(texts[0]); ++i)
	{
		const char* text = texts[i];
		printf("testing extract_xml_pparse_*(): %s\n", text);
		e = extract_buffer_open_simple(
				NULL /*alloc*/,
				text,
				strlen(text),
				NULL /*handle*/,
				NULL /*fn_close*/,
				&buffer
				);
		s_check_e( e, "extract_buffer_open_simple()");
		e = extract_xml_pparse_init( NULL /*alloc*/, buffer, NULL /*first_line*/);
		s_check_e( e, "extract_xml_pparse_init()");

		e = extract_xml_pparse_next( buffer, &tag);
		s_check_e( e, "extract_xml_pparse_next()");
		s_check_e( tag.name ? 0 : 1, "tag.name is not null");

		{
			int j;
			for (j=0; j<tag.attributes_num; ++j)
			{
				s_check_e( tag.attributes[j].name ? 0 : 1, "attribute is non-null");
				s_check_e( tag.attributes[j].value ? 0 : 1, "attribute is non-null");
			}
		}
	}
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

	s_check_xml_parse();

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
