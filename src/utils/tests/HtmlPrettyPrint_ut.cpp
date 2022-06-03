/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/HtmlPrettyPrint.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

void HtmlPrettyPrintTest() {
    {
        ByteSlice d = PrettyPrintHtml(ToByteSlice("<html><body><p>Content</p></body></html>"));
        char* data = (char*)d.data();
        utassert(str::Eq(data, "<html>\n\t<body>\n\t\t<p>Content</p>\n\t</body>\n</html>\n"));
        str::Free(data);
    }

    {
        ByteSlice d = PrettyPrintHtml(ToByteSlice("<p><b>Test</b></p>"));
        char* data = (char*)d.data();
        utassert(str::Eq(data, "<p><b>Test</b></p>\n"));
        str::Free(data);
    }

    {
        ByteSlice d = PrettyPrintHtml(ToByteSlice("<p><b>Test</p>"));
        char* data = (char*)d.data();
        utassert(str::Eq(data, "<p><b>Test</p>\n"));
        str::Free(data);
    }

    {
        ByteSlice d = PrettyPrintHtml(ToByteSlice("<html><body><p>Content</html></body>"));
        char* data = (char*)d.data();
        // TODO: add newline before non-matching </html> ?
        // TODO: insert missing closing tags (</p> and </body>)?
        utassert(str::Eq(data, "<html>\n\t<body>\n\t\t<p>Content</html>\n</body>\n"));
        str::Free(data);
    }

    {
        ByteSlice d = PrettyPrintHtml(ToByteSlice("<p  attr=' value '><b> bold  text </b> </p>"));
        char* data = (char*)d.data();
        // TODO: normalize whitespace?
        utassert(str::Eq(data, "<p  attr=' value '><b> bold  text </b></p>\n"));
        str::Free(data);
    }
}
