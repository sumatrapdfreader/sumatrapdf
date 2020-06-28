/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/HtmlPrettyPrint.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

void HtmlPrettyPrintTest() {
    AutoFree data;

    data.Set(PrettyPrintHtml(str::ToSpan("<p><b>Test</b></p>")));
    utassert(str::Eq(data, "<p><b>Test</b></p>\n"));

    data.Set(PrettyPrintHtml(str::ToSpan("<p><b>Test</p>")));
    utassert(str::Eq(data, "<p><b>Test</p>\n"));

    data.Set(PrettyPrintHtml(str::ToSpan("<html><body><p>Content</p></body></html>")));
    utassert(str::Eq(data, "<html>\n\t<body>\n\t\t<p>Content</p>\n\t</body>\n</html>\n"));

    data.Set(PrettyPrintHtml(str::ToSpan("<html><body><p>Content</html></body>")));
    // TODO: add newline before non-matching </html> ?
    // TODO: insert missing closing tags (</p> and </body>)?
    utassert(str::Eq(data, "<html>\n\t<body>\n\t\t<p>Content</html>\n</body>\n"));

    data.Set(PrettyPrintHtml(str::ToSpan("<p  attr=' value '><b> bold  text </b> </p>")));
    // TODO: normalize whitespace?
    utassert(str::Eq(data, "<p  attr=' value '><b> bold  text </b></p>\n"));
}
