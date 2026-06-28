/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/HtmlPrettyPrint.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

void HtmlPrettyPrintTest() {
    {
        ByteSlice d = PrettyPrintHtml(ToByteSlice("<html><body><p>Content</p></body></html>"));
        Str data((char*)d.data(), (int)d.size());
        utassert(str::Eq(data, "<html>\n\t<body>\n\t\t<p>Content</p>\n\t</body>\n</html>\n"));
        str::Free(data.s);
    }

    {
        ByteSlice d = PrettyPrintHtml(ToByteSlice("<p><b>Test</b></p>"));
        Str data((char*)d.data(), (int)d.size());
        utassert(str::Eq(data, "<p><b>Test</b></p>\n"));
        str::Free(data.s);
    }

    {
        ByteSlice d = PrettyPrintHtml(ToByteSlice("<p><b>Test</p>"));
        Str data((char*)d.data(), (int)d.size());
        utassert(str::Eq(data, "<p><b>Test</p>\n"));
        str::Free(data.s);
    }

    {
        ByteSlice d = PrettyPrintHtml(ToByteSlice("<html><body><p>Content</html></body>"));
        Str data((char*)d.data(), (int)d.size());
        // TODO: add newline before non-matching </html> ?
        // TODO: insert missing closing tags (</p> and </body>)?
        utassert(str::Eq(data, "<html>\n\t<body>\n\t\t<p>Content</html>\n</body>\n"));
        str::Free(data.s);
    }

    {
        ByteSlice d = PrettyPrintHtml(ToByteSlice("<p  attr=' value '><b> bold  text </b> </p>"));
        Str data((char*)d.data(), (int)d.size());
        // TODO: normalize whitespace?
        utassert(str::Eq(data, "<p  attr=' value '><b> bold  text </b></p>\n"));
        str::Free(data.s);
    }
}