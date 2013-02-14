// Executable size report utility.
// Aras Pranckevicius, http://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/

#include "BaseUtil.h"
#include "Dict.h"

#include <vector>
#include <string>
#include <stdarg.h>
#include <algorithm>
#include <map>

#include "Util.h"

#include "DebugInfo.h"

u32 DebugInfo::CountSizeInClass(int type) const
{
    u32 size = 0;
    for (u32 i=0; i<symbols.size(); i++) {
        if (symbols[i].Class == type)
            size += symbols[i].Size;
    }
    return size;
}

bool virtAddressComp(const DISymbol &a,const DISymbol &b)
{
    return a.VA < b.VA;
}

static bool StripTemplateParams(std::string& str)
{
    bool isTemplate = false;
    size_t start = str.find('<', 0);
    while (start != std::string::npos)
    {
        isTemplate = true;
        // scan to matching closing '>'
        size_t i = start + 1;
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

        str = str.erase( start, i-start+1 );
        start = str.find( '<', start );
    }

    return isTemplate;
}

void DebugInfo::FinishedReading()
{
    // fix strings and aggregate templates
    typedef std::map<std::string, int> StringIntMap;
    StringIntMap templateToIndex;

    for (int i=0; i<symbols.size(); i++)
    {
        DISymbol *sym = &symbols[i];

        std::string templateName = GetInternedString(sym->name);
        bool isTemplate = StripTemplateParams(templateName);
        if (isTemplate)
        {
            StringIntMap::iterator it = templateToIndex.find(templateName);
            int index;
            if (it != templateToIndex.end())
            {
                index = it->second;
                templates[index].size += sym->Size;
                templates[index].count++;
            }
            else
            {
                index = templates.size();
                templateToIndex.insert(std::make_pair(templateName, index));
                TemplateSymbol tsym;
                tsym.name = templateName;
                tsym.count = 1;
                tsym.size = sym->Size;
                templates.push_back(tsym);
            }
        }
    }

    // sort symbols by virtual address
    std::sort(symbols.begin(), symbols.end(), virtAddressComp);

    // remove address double-covers
    int symCount = symbols.size();
    DISymbol *syms = new DISymbol[symCount];
    memcpy(syms,&symbols[0],symCount * sizeof(DISymbol));

    symbols.clear();
    u32 oldVA = 0;
    int oldSize = 0;

    for (int i=0; i<symCount; i++)
    {
        DISymbol *in = &syms[i];
        u32 newVA = in->VA;
        u32 newSize = in->Size;

        if (oldVA != 0)
        {
            int adjust = newVA - oldVA;
            if (adjust < 0) // we have to shorten
            {
                newVA = oldVA;
                if (newSize >= -adjust)
                    newSize += adjust;
            }
        }

        if (newSize || in->Class == DIC_END)
        {
            symbols.push_back(DISymbol());
            DISymbol *out = &symbols.back();
            *out = *in;
            out->VA = newVA;
            out->Size = newSize;

            oldVA = newVA + newSize;
            oldSize = newSize;
        }
    }

    delete[] syms;
}

int DebugInfo::GetFile(int fileName)
{
    for (int i=0;i<files.size();i++) {
        if (files[i].fileName == fileName)
            return i;
    }

    files.push_back( DISymFile() );
    DISymFile *file = &files.back();
    file->fileName = fileName;
    file->codeSize = file->dataSize = 0;

    return files.size() - 1;
}

int DebugInfo::GetFileByName(char *objName)
{
    char *p;

    // skip path seperators
    while ((p = (char *) strstr(objName,"\\")))
        objName = p + 1;

    while ((p = (char *) strstr(objName,"/")))
        objName = p + 1;

    return GetFile(InternString(objName));
}

int DebugInfo::GetNameSpace(int name)
{
    for (int i=0; i<namespaces.size(); i++) {
        if (namespaces[i].name == name)
            return i;
    }

    DISymNameSp namesp;
    namesp.name = name;
    namesp.codeSize = namesp.dataSize = 0;
    namespaces.push_back(namesp);

    return namespaces.size() - 1;
}

int DebugInfo::GetNameSpaceByName(const char *name)
{
    const char *pp = name - 2;
    const char *p;
    int cname;

    while ((p = strstr(pp+2,"::")))
        pp = p;

    while ((p = strstr(pp+1,".")))
        pp = p;

    if (pp != name - 2)
    {
        char buffer[2048];
        strncpy(buffer,name,2048);

        if (pp - name < 2048)
            buffer[pp - name] = 0;

        cname = InternString(buffer);
    }
    else
        cname = InternString("<global>");

    return GetNameSpace(cname);
}

void DebugInfo::StartAnalyze()
{
    int i;

    for (i=0;i<files.size();i++)
    {
        files[i].codeSize = files[i].dataSize = 0;
    }

    for (i=0;i<namespaces.size();i++)
    {
        namespaces[i].codeSize = namespaces[i].dataSize = 0;
    }
}

void DebugInfo::FinishAnalyze()
{
    int i;

    for(i=0;i<symbols.size();i++)
    {
        if( symbols[i].Class == DIC_CODE )
        {
            files[symbols[i].objFileNum].codeSize += symbols[i].Size;
            namespaces[symbols[i].NameSpNum].codeSize += symbols[i].Size;
        }
        else if( symbols[i].Class == DIC_DATA )
        {
            files[symbols[i].objFileNum].dataSize += symbols[i].Size;
            namespaces[symbols[i].NameSpNum].dataSize += symbols[i].Size;
        }
    }
}

bool DebugInfo::FindSymbol(u32 VA,DISymbol **sym)
{
    int l,r,x;

    l = 0;
    r = symbols.size();
    while(l<r)
    {
        x = (l + r) / 2;

        if(VA < symbols[x].VA)
            r = x; // continue in left half
        else if(VA >= symbols[x].VA + symbols[x].Size)
            l = x + 1; // continue in left half
        else
        {
            *sym = &symbols[x]; // we found a match
            return true;
        }
    }

    *sym = (l + 1 < symbols.size()) ? &symbols[l+1] : 0;
    return false;
}

static bool symSizeComp(const DISymbol &a,const DISymbol &b)
{
    return a.Size > b.Size;
}

static bool templateSizeComp(const TemplateSymbol& a, const TemplateSymbol& b)
{
    return a.size > b.size;
}

static bool nameCodeSizeComp( const DISymNameSp &a,const DISymNameSp &b )
{
    return a.codeSize > b.codeSize;
}

static bool fileCodeSizeComp(const DISymFile &a,const DISymFile &b)
{
    return a.codeSize > b.codeSize;
}

static void sAppendPrintF(std::string &str,const char *format,...)
{
    static const int bufferSize = 512; // cut off after this
    char buffer[bufferSize];
    va_list arg;

    va_start(arg,format);
    _vsnprintf(buffer,bufferSize-1,format,arg);
    va_end(arg);

    strcpy(&buffer[bufferSize-4],"...");
    str += buffer;
}

std::string DebugInfo::WriteReport()
{
    const int kMinSymbolSize = 512;
    const int kMinTemplateSize = 512;
    const int kMinDataSize = 1024;
    const int kMinClassSize = 2048;
    const int kMinFileSize = 2048;

    std::string Report;
    int i; //,j;
    u32 size;

    Report.reserve(16384); // start out with 16k space

    // symbols
    sAppendPrintF(Report,"Functions by size (kilobytes):\n");
    std::sort(symbols.begin(),symbols.end(),symSizeComp);

    for(i=0;i<symbols.size();i++)
    {
        if( symbols[i].Size < kMinSymbolSize )
            break;
        if(symbols[i].Class == DIC_CODE)
            sAppendPrintF(Report,"%5d.%02d: %-50s %s\n",
            symbols[i].Size/1024,(symbols[i].Size%1024)*100/1024,
            GetInternedString(symbols[i].name), GetInternedString(files[symbols[i].objFileNum].fileName));
    }

    // templates
    sAppendPrintF(Report,"\nAggregated templates by size (kilobytes):\n");

    std::sort(templates.begin(),templates.end(),templateSizeComp);

    for(i=0;i<templates.size();i++)
    {
        if( templates[i].size < kMinTemplateSize )
            break;
        sAppendPrintF(Report,"%5d.%02d #%5d: %s\n",
            templates[i].size/1024,(templates[i].size%1024)*100/1024,
            templates[i].count,
            templates[i].name.c_str() );
    }

    sAppendPrintF(Report,"\nData by size (kilobytes):\n");
    for(i=0;i<symbols.size();i++)
    {
        if( symbols[i].Size < kMinDataSize )
            break;
        if(symbols[i].Class == DIC_DATA)
        {
            sAppendPrintF(Report,"%5d.%02d: %-50s %s\n",
                symbols[i].Size/1024,(symbols[i].Size%1024)*100/1024,
                GetInternedString(symbols[i].name), GetInternedString(files[symbols[i].objFileNum].fileName));
        }
    }

    sAppendPrintF(Report,"\nBSS by size (kilobytes):\n");
    for(i=0;i<symbols.size();i++)
    {
        if( symbols[i].Size < kMinDataSize )
            break;
        if(symbols[i].Class == DIC_BSS)
        {
            sAppendPrintF(Report,"%5d.%02d: %-50s %s\n",
                symbols[i].Size/1024,(symbols[i].Size%1024)*100/1024,
                GetInternedString(symbols[i].name), GetInternedString(files[symbols[i].objFileNum].fileName));
        }
    }

    /*
    sSPrintF(Report,512,"\nFunctions by object file and size:\n");
    Report += strlen(Report);

    for(i=1;i<symbols.size();i++)
    for(j=i;j>0;j--)
    {
    int f1 = symbols[j].FileNum;
    int f2 = symbols[j-1].FileNum;

    if(f1 == -1 || f2 != -1 && stricmp(Files[f1].Name.String,Files[f2].Name.String) < 0)
    std::swap(symbols[j],symbols[j-1]);
    }

    for(i=0;i<symbols.size();i++)
    {
    if(symbols[i].Class == DIC_CODE)
    {
    sSPrintF(Report,512,"%5d.%02d: %-50s %s\n",
    symbols[i].Size/1024,(symbols[i].Size%1024)*100/1024,
    symbols[i].Name,Files[symbols[i].FileNum].Name);

    Report += strlen(Report);
    }
    }
    */

    sAppendPrintF(Report,"\nClasses/Namespaces by code size (kilobytes):\n");
    std::sort(namespaces.begin(),namespaces.end(),nameCodeSizeComp);

    for(i=0;i<namespaces.size();i++)
    {
        if( namespaces[i].codeSize < kMinClassSize )
            break;
        sAppendPrintF(Report,"%5d.%02d: %s\n",
            namespaces[i].codeSize/1024,(namespaces[i].codeSize%1024)*100/1024, GetInternedString(namespaces[i].name) );
    }

    sAppendPrintF(Report,"\nObject files by code size (kilobytes):\n");
    std::sort(files.begin(),files.end(),fileCodeSizeComp);

    for(i=0;i<files.size();i++)
    {
        if( files[i].codeSize < kMinFileSize )
            break;
        sAppendPrintF(Report,"%5d.%02d: %s\n",files[i].codeSize/1024,
            (files[i].codeSize%1024)*100/1024, GetInternedString(files[i].fileName) );
    }

    size = CountSizeInClass(DIC_CODE);
    sAppendPrintF(Report,"\nOverall code: %5d.%02d kb\n",size/1024,
        (size%1024)*100/1024 );

    size = CountSizeInClass(DIC_DATA);
    sAppendPrintF(Report,"Overall data: %5d.%02d kb\n",size/1024,
        (size%1024)*100/1024);

    size = CountSizeInClass(DIC_BSS);
    sAppendPrintF(Report,"Overall BSS:  %5d.%02d kb\n",size/1024,
        (size%1024)*100/1024);

    return Report;
}
