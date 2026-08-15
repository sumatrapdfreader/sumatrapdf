#include "mupdf/fitz.h"
#include "mupdf/classes.h"
#include "mupdf/classes2.h"

#include <assert.h>


int main(int argc, char** argv)
{
    assert(argc == 2);
    const char* path = argv[1];
    mupdf::FzDocument document(path);
    std::string v;
    v = mupdf::fz_lookup_metadata2(document, "format");
    printf("v=%s\n", v.c_str());
    bool raised = false;
    try
    {
        v = mupdf::fz_lookup_metadata2(document, "format___");
    }
    catch (std::exception& e)
    {
        raised = true;
        printf("Received expected exception: %s\n", e.what());
    }
    if (!raised) exit(1);
    printf("v=%s\n", v.c_str());
    fz_rect r = fz_unit_rect;
    printf("r.x0=%f\n", r.x0);

    mupdf::FzStextOptions   options;
    mupdf::FzStextPage stp( document, 0, options);
    std::vector<fz_quad>    quads = mupdf::fz_highlight_selection2(
            stp,
            mupdf::FzPoint(20, 20),
            mupdf::FzPoint(120, 220),
            100
            );
    printf("quads.size()=%zi\n", quads.size());
    assert(quads.size() == 13);
    return 0;
}
