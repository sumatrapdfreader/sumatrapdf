/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/HtmlPrettyPrint.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

void HtmlPrettyPrintTest() {
    size_t lenOut;
    AutoFree data;

    data.Set(PrettyPrintHtml("<p><b>Test</b></p>", (size_t)-1, lenOut));
    utassert(str::Len(data) == lenOut && str::Eq(data, "<p><b>Test</b></p>\n"));

    data.Set(PrettyPrintHtml("<p><b>Test</p>", (size_t)-1, lenOut));
    utassert(str::Len(data) == lenOut && str::Eq(data, "<p><b>Test</p>\n"));

    data.Set(PrettyPrintHtml("<html><body><p>Content</p></body></html>", (size_t)-1, lenOut));
    utassert(str::Len(data) == lenOut && str::Eq(data, "<html>\n\t<body>\n\t\t<p>Content</p>\n\t</body>\n</html>\n"));

    data.Set(PrettyPrintHtml("<html><body><p>Content</html></body>", (size_t)-1, lenOut));
    // TODO: add newline before non-matching </html> ?
    // TODO: insert missing closing tags (</p> and </body>)?
    utassert(str::Len(data) == lenOut && str::Eq(data, "<html>\n\t<body>\n\t\t<p>Content</html>\n</body>\n"));

    data.Set(PrettyPrintHtml("<p  attr=' value '><b> bold  text </b> </p>", (size_t)-1, lenOut));
    // TODO: normalize whitespace?
    utassert(str::Len(data) == lenOut && str::Eq(data, "<p  attr=' value '><b> bold  text </b></p>\n"));
}
