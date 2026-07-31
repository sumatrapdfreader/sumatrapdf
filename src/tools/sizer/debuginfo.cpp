// Executable size report utility.
// Aras Pranckevicius, http://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/
// Public domain.

#include "types.hpp"
#include "debuginfo.hpp"
#include <stdarg.h>
#include <algorithm>
#include <map>

/****************************************************************************/

sU32 DebugInfo::CountSizeInClass(sInt type) const
{
    sU32 size = 0;
    for (sInt i = 0; i < Symbols.size(); i++)
        size += (Symbols[i].Class == type) ? Symbols[i].Size : 0;

    return size;
}

void DebugInfo::Init()
{
    BaseAddress = 0;
}

void DebugInfo::Exit()
{
}

sInt DebugInfo::MakeString(const char *s)
{
    string str(s);
    IndexByStringMap::iterator it = m_IndexByString.find(str);
    if (it != m_IndexByString.end())
        return it->second;

    sInt index = m_IndexByString.size();
    m_IndexByString.insert(std::make_pair(str, index));
    m_StringByIndex.push_back(str);
    return index;
}

bool virtAddressComp(const DISymbol &a, const DISymbol &b)
{
    return a.VA < b.VA;
}

static bool StripTemplateParams(std::string& str)
{
    bool isTemplate = false;
    int start = str.find('<', 0);
    while (start != std::string::npos)
    {
        isTemplate = true;
        // scan to matching closing '>'
        int i = start + 1;
        int depth = 1;
        while (i < str.size())
        {
            char ch = str[i];
            if (ch == '<')
                ++depth;
            if (ch == '>')
            {
                --depth;
                if (depth == 0)
                    break;
            }
            ++i;
        }
        if (depth != 0)
            return isTemplate; // no matching '>', just return

        str = str.erase(start, i - start + 1);

        start = str.find('<', start);
    }

    return isTemplate;
}

void DebugInfo::FinishedReading()
{
    // fix strings and aggregate templates
    typedef std::map<std::string, int> StringIntMap;
    StringIntMap templateToIndex;

    for (sInt i = 0; i < Symbols.size(); i++)
    {
        DISymbol *sym = &Symbols[i];

        std::string templateName = GetStringPrep(sym->name);
        bool isTemplate = StripTemplateParams(templateName);
        if (isTemplate)
        {
            StringIntMap::iterator it = templateToIndex.find(templateName);
            int index;
            if (it != templateToIndex.end())
            {
                index = it->second;
                Templates[index].size += sym->Size;
                Templates[index].count++;
            }
            else
            {
                index = Templates.size();
                templateToIndex.insert(std::make_pair(templateName, index));
                TemplateSymbol tsym;
                tsym.name = templateName;
                tsym.mangledName = GetStringPrep(sym->mangledName);
                StripTemplateParams(tsym.mangledName);
                tsym.count = 1;
                tsym.size = sym->Size;
                Templates.push_back(tsym);
            }
        }
    }

    // sort symbols by virtual address
    std::sort(Symbols.begin(), Symbols.end(), virtAddressComp);

    // remove address double-covers
    sInt symCount = Symbols.size();
    DISymbol *syms = new DISymbol[symCount];
    sCopyMem(syms, &Symbols[0], symCount * sizeof(DISymbol));

    Symbols.clear();
    sU32 oldVA = 0;
    sInt oldSize = 0;

    for (sInt i = 0; i < symCount; i++)
    {
        DISymbol *in = &syms[i];
        sU32 newVA = in->VA;
        sU32 newSize = in->Size;

        if (oldVA != 0)
        {
            sInt adjust = newVA - oldVA;
            if (adjust < 0) // we have to shorten
            {
                newVA = oldVA;
                if (newSize >= -adjust)
                    newSize += adjust;
            }
        }

        if (newSize || in->Class == DIC_END)
        {
            Symbols.push_back(DISymbol());
            DISymbol *out = &Symbols.back();
            *out = *in;
            out->VA = newVA;
            out->Size = newSize;

            oldVA = newVA + newSize;
            oldSize = newSize;
        }
    }

    delete[] syms;
}

sInt DebugInfo::GetFile(sInt fileName)
{
    for (sInt i = 0; i < m_Files.size(); i++)
        if (m_Files[i].fileName == fileName)
            return i;

    m_Files.push_back(DISymFile());
    DISymFile *file = &m_Files.back();
    file->fileName = fileName;
    file->codeSize = file->dataSize = 0;

    return m_Files.size() - 1;
}

sInt DebugInfo::GetFileByName(sChar *objName)
{
    sChar *p;

    // skip path seperators
    while ((p = (sChar*)sFindString(objName, "\\")))
        objName = p + 1;

    while ((p = (sChar*)sFindString(objName, "/")))
        objName = p + 1;

    return GetFile(MakeString(objName));
}

sInt DebugInfo::GetNameSpace(sInt name)
{
    for (sInt i = 0; i < NameSps.size(); i++)
        if (NameSps[i].name == name)
            return i;

    DISymNameSp namesp;
    namesp.name = name;
    namesp.codeSize = namesp.dataSize = 0;
    NameSps.push_back(namesp);

    return NameSps.size() - 1;
}

sInt DebugInfo::GetNameSpaceByName(sChar *name)
{
    sChar *pp = name - 2;
    sChar *p;
    sInt cname;

    while ((p = (sChar*)sFindString(pp + 2, "::")))
        pp = p;

    while ((p = (sChar*)sFindString(pp + 1, ".")))
        pp = p;

    if (pp != name - 2)
    {
        sChar buffer[2048];
        sCopyString(buffer, sizeof(buffer), name, 2048 - 1);

        if (pp - name < 2048)
            buffer[pp - name] = 0;

        cname = MakeString(buffer);
    }
    else
        cname = MakeString("<global>");

    return GetNameSpace(cname);
}

void DebugInfo::StartAnalyze()
{
    sInt i;

    for (i = 0; i < m_Files.size(); i++)
    {
        m_Files[i].codeSize = m_Files[i].dataSize = 0;
    }

    for (i = 0; i < NameSps.size(); i++)
    {
        NameSps[i].codeSize = NameSps[i].dataSize = 0;
    }
}

void DebugInfo::FinishAnalyze()
{
    sInt i;

    for (i = 0; i < Symbols.size(); i++)
    {
        if (Symbols[i].Class == DIC_CODE)
        {
            m_Files[Symbols[i].objFileNum].codeSize += Symbols[i].Size;
            NameSps[Symbols[i].NameSpNum].codeSize += Symbols[i].Size;
        }
        else if (Symbols[i].Class == DIC_DATA)
        {
            m_Files[Symbols[i].objFileNum].dataSize += Symbols[i].Size;
            NameSps[Symbols[i].NameSpNum].dataSize += Symbols[i].Size;
        }
    }
}

sBool DebugInfo::FindSymbol(sU32 VA, DISymbol **sym)
{
    sInt l, r, x;

    l = 0;
    r = Symbols.size();
    while (l < r)
    {
        x = (l + r) / 2;

        if (VA < Symbols[x].VA)
            r = x; // continue in left half
        else if (VA >= Symbols[x].VA + Symbols[x].Size)
            l = x + 1; // continue in left half
        else
        {
            *sym = &Symbols[x]; // we found a match
            return true;
        }
    }

    *sym = (l + 1 < Symbols.size()) ? &Symbols[l + 1] : 0;
    return false;
}

static bool symSizeComp(const DISymbol &a, const DISymbol &b)
{
    return a.Size > b.Size;
}

static bool templateSizeComp(const TemplateSymbol& a, const TemplateSymbol& b)
{
    return a.size > b.size;
}

static bool nameCodeSizeComp(const DISymNameSp &a, const DISymNameSp &b)
{
    return a.codeSize > b.codeSize;
}

static bool fileCodeSizeComp(const DISymFile &a, const DISymFile &b)
{
    return a.codeSize > b.codeSize;
}

static void sAppendPrintF(std::string &str, const char *format, ...)
{
    static const int bufferSize = 512; // cut off after this
    char buffer[bufferSize];
    va_list arg;

    va_start(arg, format);
    _vsnprintf(buffer, bufferSize - 1, format, arg);
    va_end(arg);

    strcpy(&buffer[bufferSize - 5], "...\n");
    str += buffer;
}

std::string DebugInfo::WriteReport(const DebugFilters& filters)
{
    std::string Report;
    sInt i; //,j;
    sU32 size;
    const char* filterName = filters.name.empty() ? NULL : filters.name.c_str();

    Report.reserve(16384); // start out with 16k space
    if (filterName)
    {
        sAppendPrintF(Report, "Only including things with '%s' in their name/file\n\n", filterName);
    }

    // symbols
    sAppendPrintF(Report, "Functions by size (kilobytes, min %.2f):\n", filters.minFunction/1024.0);
    std::sort(Symbols.begin(), Symbols.end(), symSizeComp);

    for (i = 0; i < Symbols.size(); i++)
    {
        if (Symbols[i].Size < filters.minFunction)
            break;
        if (Symbols[i].Class == DIC_CODE)
        {
            const char* name1 = GetStringPrep(Symbols[i].mangledName);
            const char* name2 = GetStringPrep(m_Files[Symbols[i].objFileNum].fileName);
            const char* name3 = GetStringPrep(Symbols[i].name);
            if (filterName && !strstr(name1, filterName) && !strstr(name2, filterName) && !strstr(name3, filterName))
                continue;
            sAppendPrintF(Report, "%5d.%02d: %-80s %-40s %s\n",
                Symbols[i].Size / 1024, (Symbols[i].Size % 1024) * 100 / 1024,
                name1, name2, name3);
        }
    }

    // templates
    sAppendPrintF(Report, "\nAggregated templates by size (kilobytes, min %.2f / %i):\n", filters.minTemplate/1024.0, filters.minTemplateCount);

    std::sort(Templates.begin(), Templates.end(), templateSizeComp);

    for (i = 0; i < Templates.size(); i++)
    {
        if (Templates[i].size < filters.minTemplate || Templates[i].count < filters.minTemplateCount)
            break;
        const char* name1 = Templates[i].mangledName.c_str();
        const char* name2 = Templates[i].name.c_str();
        if (filterName && !strstr(name1, filterName) && !strstr(name2, filterName))
            continue;
        sAppendPrintF(Report, "%5d.%02d #%5d: %-80s %s\n",
            Templates[i].size / 1024, (Templates[i].size % 1024) * 100 / 1024,
            Templates[i].count,
            name1,
            name2);
    }

    sAppendPrintF(Report, "\nData by size (kilobytes, min %.2f):\n", filters.minData/1024.0);
    for (i = 0; i < Symbols.size(); i++)
    {
        if (Symbols[i].Size < filters.minData)
            break;
        if (Symbols[i].Class == DIC_DATA)
        {
            const char* name1 = GetStringPrep(Symbols[i].name);
            const char* name2 = GetStringPrep(m_Files[Symbols[i].objFileNum].fileName);
            if (filterName && !strstr(name1, filterName) && !strstr(name2, filterName))
                continue;
            sAppendPrintF(Report, "%5d.%02d: %-50s %s\n",
                Symbols[i].Size / 1024, (Symbols[i].Size % 1024) * 100 / 1024,
                name1, name2);
        }
    }

    sAppendPrintF(Report, "\nBSS by size (kilobytes, min %.2f):\n", filters.minData/1024.0);
    for (i = 0; i < Symbols.size(); i++)
    {
        if (Symbols[i].Size < filters.minData)
            break;
        if (Symbols[i].Class == DIC_BSS)
        {
            const char* name1 = GetStringPrep(Symbols[i].name);
            const char* name2 = GetStringPrep(m_Files[Symbols[i].objFileNum].fileName);
            if (filterName && !strstr(name1, filterName) && !strstr(name2, filterName))
                continue;
            sAppendPrintF(Report, "%5d.%02d: %-50s %s\n",
                Symbols[i].Size / 1024, (Symbols[i].Size % 1024) * 100 / 1024,
                name1, name2);
        }
    }

    /*
    sSPrintF(Report,512,"\nFunctions by object file and size:\n");
    Report += sGetStringLen(Report);

    for(i=1;i<Symbols.size();i++)
      for(j=i;j>0;j--)
      {
        sInt f1 = Symbols[j].FileNum;
        sInt f2 = Symbols[j-1].FileNum;

        if(f1 == -1 || f2 != -1 && sCmpStringI(Files[f1].Name.String,Files[f2].Name.String) < 0)
          sSwap(Symbols[j],Symbols[j-1]);
      }

    for(i=0;i<Symbols.size();i++)
    {
      if(Symbols[i].Class == DIC_CODE)
      {
        sSPrintF(Report,512,"%5d.%02d: %-50s %s\n",
          Symbols[i].Size/1024,(Symbols[i].Size%1024)*100/1024,
          Symbols[i].Name,Files[Symbols[i].FileNum].Name);

        Report += sGetStringLen(Report);
      }
    }
    */

    sAppendPrintF(Report, "\nClasses/Namespaces by code size (kilobytes, min %.2f):\n", filters.minClass/1024.0);
    std::sort(NameSps.begin(), NameSps.end(), nameCodeSizeComp);

    for (i = 0; i < NameSps.size(); i++)
    {
        if (NameSps[i].codeSize < filters.minClass)
            break;
        const char* name1 = GetStringPrep(NameSps[i].name);
        if (filterName && !strstr(name1, filterName))
            continue;
        sAppendPrintF(Report, "%5d.%02d: %s\n",
            NameSps[i].codeSize / 1024, (NameSps[i].codeSize % 1024) * 100 / 1024, name1);
    }

    sAppendPrintF(Report, "\nObject files by code size (kilobytes, min %.2f):\n", filters.minFile/1024.0);
    std::sort(m_Files.begin(), m_Files.end(), fileCodeSizeComp);

    for (i = 0; i < m_Files.size(); i++)
    {
        if (m_Files[i].codeSize < filters.minFile)
            break;
        const char* name1 = GetStringPrep(m_Files[i].fileName);
        if (filterName && !strstr(name1, filterName))
            continue;
        sAppendPrintF(Report, "%5d.%02d: %s\n", m_Files[i].codeSize / 1024,
            (m_Files[i].codeSize % 1024) * 100 / 1024, name1);
    }

    size = CountSizeInClass(DIC_CODE);
    sAppendPrintF(Report, "\nOverall code:  %5d.%02d kb\n", size / 1024,
        (size % 1024) * 100 / 1024);

    size = CountSizeInClass(DIC_DATA);
    sAppendPrintF(Report, "Overall data:  %5d.%02d kb\n", size / 1024,
        (size % 1024) * 100 / 1024);

    size = CountSizeInClass(DIC_BSS);
    sAppendPrintF(Report, "Overall BSS:   %5d.%02d kb\n", size / 1024,
        (size % 1024) * 100 / 1024);

    size = CountSizeInClass(DIC_UNKNOWN);
    sAppendPrintF(Report, "Overall other: %5d.%02d kb\n", size / 1024,
        (size % 1024) * 100 / 1024);

    return Report;
}
