/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/GuessFileType.h"
#include "base/Pixmap.h"
#include "base/Timer.h"
#include "base/Zip.h"

#include "gui/UIModels.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"

#include "base/tests/UtAssert.h"
#include "SumatraLog.h"

#define OFD_NS "xmlns=\"http://www.ofdspec.org/2016\""

// a 10 x 10 mm black square
#define OFD_SQUARE                                               \
    "<PathObject ID=\"2\" Boundary=\"0 0 10 10\" Fill=\"true\">" \
    "<AbbreviatedData>M 0 0 L 10 0 L 10 10 L 0 10 C</AbbreviatedData></PathObject>"

// what each page may take; a cyclic or exponential page used to hang
constexpr double kMaxPageMs = 10 * 1000;

struct OfdPart {
    const char* name;
    const char* xml;
};

static Str MakeOfd(const OfdPart* pages, int nPages, const OfdPart* extra, int nExtra) {
    str::Builder pageList;
    for (int i = 0; i < nPages; i++) {
        pageList.Append(fmt("<Page ID=\"%d\" BaseLoc=\"%s\"/>", 100 + i, Str(pages[i].name)));
    }
    TempStr doc = fmt("<Document " OFD_NS
                      "><CommonData><PageArea><PhysicalBox>0 0 20 20</PhysicalBox></PageArea>"
                      "<TemplatePage ID=\"90\" BaseLoc=\"Tpl.xml\"/></CommonData><Pages>%s</Pages></Document>",
                      ToStr(pageList));

    str::Builder zip;
    ZipCreator zc(zip);
    zc.AddFileData(StrL("OFD.xml"),
                   StrL("<OFD " OFD_NS "><DocBody><DocRoot>Doc/Document.xml</DocRoot></DocBody></OFD>"));
    zc.AddFileData(StrL("Doc/Document.xml"), doc);
    for (int i = 0; i < nPages; i++) {
        zc.AddFileData(str::JoinTemp(StrL("Doc/"), Str(pages[i].name)), Str(pages[i].xml));
    }
    for (int i = 0; i < nExtra; i++) {
        zc.AddFileData(str::JoinTemp(StrL("Doc/"), Str(extra[i].name)), Str(extra[i].xml));
    }
    zc.Finish();
    return zip.TakeStr();
}

// renders every page, each within kMaxPageMs
static void RenderOfdPages(Str ofd, int nPages) {
    EngineBase* engine = CreateEngineMupdfFromData(ofd, StrL("test.ofd"), nullptr);
    utassert(engine);
    if (!engine) {
        return;
    }
    utassert(engine->PageCount() == nPages);
    for (int pageNo = 1; pageNo <= engine->PageCount(); pageNo++) {
        // The fixture emits expected limit warnings. Console writes can block on the test runner.
        bool logToConsole = gLogToConsole;
        gLogToConsole = false;
        auto start = TimeGet();
        RenderPageArgs args(pageNo, 1.f, 0);
        Pixmap* px = engine->RenderPage(args);
        double elapsedMs = TimeSinceInMs(start);
        gLogToConsole = logToConsole;

        utassert(px != nullptr);
        FreePixmap(px);
        if (elapsedMs >= kMaxPageMs) {
            logf("OFD page %d rendered in %.1f ms\n", pageNo, elapsedMs);
        }
        utassert(elapsedMs < kMaxPageMs);
    }
    SafeEngineRelease(&engine);
}

void Ofd_UnitTests() {
    // page 1: a composite unit drawing itself twice
    // page 2: a template page using itself twice
    // page 3: 16 composite units, each drawing the next 4 times (4^15 draws, no cycle)
    str::Builder chain;
    chain.Append(StrL("<Res " OFD_NS "><CompositeGraphicUnits>"));
    constexpr int kChainLen = 16;
    constexpr int kFanOut = 4;
    for (int i = 0; i < kChainLen; i++) {
        int unit = 200 + i;
        chain.Append(fmt("<CompositeGraphicUnit ID=\"%d\" Width=\"10\" Height=\"10\"><Content>", unit));
        if (i + 1 < kChainLen) {
            for (int j = 0; j < kFanOut; j++) {
                chain.Append(fmt("<CompositeObject ID=\"%d\" ResourceID=\"%d\" Boundary=\"0 0 10 10\"/>",
                                 1000 + (i * kFanOut) + j, unit + 1));
            }
        }
        chain.Append(StrL("</Content></CompositeGraphicUnit>"));
    }
    chain.Append(StrL("</CompositeGraphicUnits></Res>"));
    TempStr chainXml = str::DupTemp(ToStr(chain));

    const OfdPart pages[] = {
        {"Page1.xml", "<Page " OFD_NS "><PageRes>Res1.xml</PageRes><Content><Layer ID=\"3\">" OFD_SQUARE
                      "<CompositeObject ID=\"4\" ResourceID=\"5\" Boundary=\"0 0 10 10\"/></Layer></Content></Page>"},
        {"Page2.xml", "<Page " OFD_NS "><Template TemplateID=\"90\"/><Content><Layer ID=\"3\">" OFD_SQUARE
                      "</Layer></Content></Page>"},
        {"Page3.xml", "<Page " OFD_NS "><PageRes>Res3.xml</PageRes><Content><Layer ID=\"3\">"
                      "<CompositeObject ID=\"4\" ResourceID=\"200\" Boundary=\"0 0 10 10\"/></Layer></Content></Page>"},
    };
    OfdPart extra[] = {
        {"Res1.xml", "<Res " OFD_NS "><CompositeGraphicUnits><CompositeGraphicUnit ID=\"5\" Width=\"10\" Height=\"10\">"
                     "<Content><CompositeObject ID=\"6\" ResourceID=\"5\" Boundary=\"0 0 10 10\"/>"
                     "<CompositeObject ID=\"7\" ResourceID=\"5\" Boundary=\"0 0 10 10\"/></Content>"
                     "</CompositeGraphicUnit></CompositeGraphicUnits></Res>"},
        {"Tpl.xml", "<Page " OFD_NS "><Template TemplateID=\"90\"/><Template TemplateID=\"90\"/>"
                    "<Content><Layer ID=\"8\">" OFD_SQUARE "</Layer></Content></Page>"},
        {"Res3.xml", chainXml.s},
    };

    Str ofd = MakeOfd(pages, dimofi(pages), extra, dimofi(extra));
    RenderOfdPages(ofd, dimofi(pages));
    str::Free(ofd);
}
