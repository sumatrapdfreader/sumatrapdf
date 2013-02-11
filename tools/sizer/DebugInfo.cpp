// Executable size report utility.
// Aras Pranckevicius, http://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/

#include "BaseUtil.h"
#include "Types.h"

#include "DebugInfo.h"
#include <stdarg.h>
#include <algorithm>
#include <map>

u32 DebugInfo::CountSizeInClass(int type) const
{
    u32 size = 0;
    for (int i=0; i<Symbols.size(); i++) {
        if (Symbols[i].Class == type)
            size += Symbols[i].Size;
    }
    return size;
}

void DebugInfo::Init()
{
    BaseAddress = 0;
}

void DebugInfo::Exit()
{
}

int DebugInfo::MakeString(char *s)
{
    string str(s);
    IndexByStringMap::iterator it = m_IndexByString.find(str);
    if (it != m_IndexByString.end())
        return it->second;

    int index = m_IndexByString.size();
    m_IndexByString.insert(std::make_pair(str,index));
    m_StringByIndex.push_back(str);
    return index;
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

    for (int i=0; i<Symbols.size(); i++)
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
                tsym.count = 1;
                tsym.size = sym->Size;
                Templates.push_back(tsym);
            }
        }
    }

    // sort symbols by virtual address
    std::sort(Symbols.begin(), Symbols.end(), virtAddressComp);

    // remove address double-covers
    int symCount = Symbols.size();
    DISymbol *syms = new DISymbol[symCount];
    memcpy(syms,&Symbols[0],symCount * sizeof(DISymbol));

    Symbols.clear();
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

int DebugInfo::GetFile(int fileName)
{
    for (int i=0;i<m_Files.size();i++) {
        if (m_Files[i].fileName == fileName)
            return i;
    }

    m_Files.push_back( DISymFile() );
    DISymFile *file = &m_Files.back();
    file->fileName = fileName;
    file->codeSize = file->dataSize = 0;

    return m_Files.size() - 1;
}

int DebugInfo::GetFileByName(char *objName)
{
    char *p;

    // skip path seperators
    while ((p = (char *) strstr(objName,"\\")))
        objName = p + 1;

    while ((p = (char *) strstr(objName,"/")))
        objName = p + 1;

    return GetFile(MakeString(objName));
}

int DebugInfo::GetNameSpace(int name)
{
    for (int i=0;i<NameSps.size();i++) {
        if (NameSps[i].name == name)
            return i;
    }

    DISymNameSp namesp;
    namesp.name = name;
    namesp.codeSize = namesp.dataSize = 0;
    NameSps.push_back(namesp);

    return NameSps.size() - 1;
}

int DebugInfo::GetNameSpaceByName(char *name)
{
    char *pp = name - 2;
    char *p;
    int cname;

    while ((p = (char *) strstr(pp+2,"::")))
        pp = p;

    while ((p = (char *) strstr(pp+1,".")))
        pp = p;

    if (pp != name - 2)
    {
        char buffer[2048];
        strncpy(buffer,name,2048);

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
    int i;

    for (i=0;i<m_Files.size();i++)
    {
        m_Files[i].codeSize = m_Files[i].dataSize = 0;
    }

    for (i=0;i<NameSps.size();i++)
    {
        NameSps[i].codeSize = NameSps[i].dataSize = 0;
    }
}

void DebugInfo::FinishAnalyze()
{
    int i;

    for(i=0;i<Symbols.size();i++)
    {
        if( Symbols[i].Class == DIC_CODE )
        {
            m_Files[Symbols[i].objFileNum].codeSize += Symbols[i].Size;
            NameSps[Symbols[i].NameSpNum].codeSize += Symbols[i].Size;
        }
        else if( Symbols[i].Class == DIC_DATA )
        {
            m_Files[Symbols[i].objFileNum].dataSize += Symbols[i].Size;
            NameSps[Symbols[i].NameSpNum].dataSize += Symbols[i].Size;
        }
    }
}

bool DebugInfo::FindSymbol(u32 VA,DISymbol **sym)
{
    int l,r,x;

    l = 0;
    r = Symbols.size();
    while(l<r)
    {
        x = (l + r) / 2;

        if(VA < Symbols[x].VA)
            r = x; // continue in left half
        else if(VA >= Symbols[x].VA + Symbols[x].Size)
            l = x + 1; // continue in left half
        else
        {
            *sym = &Symbols[x]; // we found a match
            return true;
        }
    }

    *sym = (l + 1 < Symbols.size()) ? &Symbols[l+1] : 0;
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
    std::sort(Symbols.begin(),Symbols.end(),symSizeComp);

    for(i=0;i<Symbols.size();i++)
    {
        if( Symbols[i].Size < kMinSymbolSize )
            break;
        if(Symbols[i].Class == DIC_CODE)
            sAppendPrintF(Report,"%5d.%02d: %-50s %s\n",
            Symbols[i].Size/1024,(Symbols[i].Size%1024)*100/1024,
            GetStringPrep(Symbols[i].name), GetStringPrep(m_Files[Symbols[i].objFileNum].fileName));
    }

    // templates
    sAppendPrintF(Report,"\nAggregated templates by size (kilobytes):\n");

    std::sort(Templates.begin(),Templates.end(),templateSizeComp);

    for(i=0;i<Templates.size();i++)
    {
        if( Templates[i].size < kMinTemplateSize )
            break;
        sAppendPrintF(Report,"%5d.%02d #%5d: %s\n",
            Templates[i].size/1024,(Templates[i].size%1024)*100/1024,
            Templates[i].count,
            Templates[i].name.c_str() );
    }

    sAppendPrintF(Report,"\nData by size (kilobytes):\n");
    for(i=0;i<Symbols.size();i++)
    {
        if( Symbols[i].Size < kMinDataSize )
            break;
        if(Symbols[i].Class == DIC_DATA)
        {
            sAppendPrintF(Report,"%5d.%02d: %-50s %s\n",
                Symbols[i].Size/1024,(Symbols[i].Size%1024)*100/1024,
                GetStringPrep(Symbols[i].name), GetStringPrep(m_Files[Symbols[i].objFileNum].fileName));
        }
    }

    sAppendPrintF(Report,"\nBSS by size (kilobytes):\n");
    for(i=0;i<Symbols.size();i++)
    {
        if( Symbols[i].Size < kMinDataSize )
            break;
        if(Symbols[i].Class == DIC_BSS)
        {
            sAppendPrintF(Report,"%5d.%02d: %-50s %s\n",
                Symbols[i].Size/1024,(Symbols[i].Size%1024)*100/1024,
                GetStringPrep(Symbols[i].name), GetStringPrep(m_Files[Symbols[i].objFileNum].fileName));
        }
    }

    /*
    sSPrintF(Report,512,"\nFunctions by object file and size:\n");
    Report += strlen(Report);

    for(i=1;i<Symbols.size();i++)
    for(j=i;j>0;j--)
    {
    int f1 = Symbols[j].FileNum;
    int f2 = Symbols[j-1].FileNum;

    if(f1 == -1 || f2 != -1 && stricmp(Files[f1].Name.String,Files[f2].Name.String) < 0)
    std::swap(Symbols[j],Symbols[j-1]);
    }

    for(i=0;i<Symbols.size();i++)
    {
    if(Symbols[i].Class == DIC_CODE)
    {
    sSPrintF(Report,512,"%5d.%02d: %-50s %s\n",
    Symbols[i].Size/1024,(Symbols[i].Size%1024)*100/1024,
    Symbols[i].Name,Files[Symbols[i].FileNum].Name);

    Report += strlen(Report);
    }
    }
    */

    sAppendPrintF(Report,"\nClasses/Namespaces by code size (kilobytes):\n");
    std::sort(NameSps.begin(),NameSps.end(),nameCodeSizeComp);

    for(i=0;i<NameSps.size();i++)
    {
        if( NameSps[i].codeSize < kMinClassSize )
            break;
        sAppendPrintF(Report,"%5d.%02d: %s\n",
            NameSps[i].codeSize/1024,(NameSps[i].codeSize%1024)*100/1024, GetStringPrep(NameSps[i].name) );
    }

    sAppendPrintF(Report,"\nObject files by code size (kilobytes):\n");
    std::sort(m_Files.begin(),m_Files.end(),fileCodeSizeComp);

    for(i=0;i<m_Files.size();i++)
    {
        if( m_Files[i].codeSize < kMinFileSize )
            break;
        sAppendPrintF(Report,"%5d.%02d: %s\n",m_Files[i].codeSize/1024,
            (m_Files[i].codeSize%1024)*100/1024, GetStringPrep(m_Files[i].fileName) );
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
