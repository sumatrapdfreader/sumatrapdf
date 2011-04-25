//C-  -*- C++ -*-
//C- -------------------------------------------------------------------
//C- DjVuLibre-3.5
//C- Copyright (c) 2002  Leon Bottou and Yann Le Cun.
//C- Copyright (c) 2001  AT&T
//C-
//C- This software is subject to, and may be distributed under, the
//C- GNU General Public License, either Version 2 of the license,
//C- or (at your option) any later version. The license should have
//C- accompanied the software or you may obtain a copy of the license
//C- from the Free Software Foundation at http://www.fsf.org .
//C-
//C- This program is distributed in the hope that it will be useful,
//C- but WITHOUT ANY WARRANTY; without even the implied warranty of
//C- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//C- GNU General Public License for more details.
//C- 
//C- DjVuLibre-3.5 is derived from the DjVu(r) Reference Library from
//C- Lizardtech Software.  Lizardtech Software has authorized us to
//C- replace the original DjVu(r) Reference Library notice by the following
//C- text (see doc/lizard2002.djvu and doc/lizardtech2007.djvu):
//C-
//C-  ------------------------------------------------------------------
//C- | DjVu (r) Reference Library (v. 3.5)
//C- | Copyright (c) 1999-2001 LizardTech, Inc. All Rights Reserved.
//C- | The DjVu Reference Library is protected by U.S. Pat. No.
//C- | 6,058,214 and patents pending.
//C- |
//C- | This software is subject to, and may be distributed under, the
//C- | GNU General Public License, either Version 2 of the license,
//C- | or (at your option) any later version. The license should have
//C- | accompanied the software or you may obtain a copy of the license
//C- | from the Free Software Foundation at http://www.fsf.org .
//C- |
//C- | The computer code originally released by LizardTech under this
//C- | license and unmodified by other parties is deemed "the LIZARDTECH
//C- | ORIGINAL CODE."  Subject to any third party intellectual property
//C- | claims, LizardTech grants recipient a worldwide, royalty-free, 
//C- | non-exclusive license to make, use, sell, or otherwise dispose of 
//C- | the LIZARDTECH ORIGINAL CODE or of programs derived from the 
//C- | LIZARDTECH ORIGINAL CODE in compliance with the terms of the GNU 
//C- | General Public License.   This grant only confers the right to 
//C- | infringe patent claims underlying the LIZARDTECH ORIGINAL CODE to 
//C- | the extent such infringement is reasonably necessary to enable 
//C- | recipient to make, have made, practice, sell, or otherwise dispose 
//C- | of the LIZARDTECH ORIGINAL CODE (or portions thereof) and not to 
//C- | any greater extent that may be necessary to utilize further 
//C- | modifications or combinations.
//C- |
//C- | The LIZARDTECH ORIGINAL CODE is provided "AS IS" WITHOUT WARRANTY
//C- | OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
//C- | TO ANY WARRANTY OF NON-INFRINGEMENT, OR ANY IMPLIED WARRANTY OF
//C- | MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
//C- +------------------------------------------------------------------

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma implementation
#endif

#include "DjVuInfo.h"
#include "GException.h"
#include "ByteStream.h"
#include "GString.h"


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

// ----------------------------------------
// CLASS DJVUINFO


#define STRINGIFY(x) STRINGIFY_(x)
#define STRINGIFY_(x) #x


DjVuInfo::DjVuInfo()
  : width(0), height(0), 
#ifdef DJVUVERSION_FOR_OUTPUT
    version(DJVUVERSION_FOR_OUTPUT),
#else
    version(DJVUVERSION),
#endif
    dpi(300), gamma(2.2), orientation(0)
{
}

void 
DjVuInfo::decode(ByteStream &bs)
{
  // Set to default values
  width = 0;
  height = 0;
  version = DJVUVERSION;
  dpi = 300;
  gamma = 2.2;
  orientation=0;
  // Read data
  unsigned char buffer[10];
  int  size = bs.readall((void*)buffer, sizeof(buffer));
  if (size == 0)
    G_THROW( ByteStream::EndOfFile );
  if (size < 5)
    G_THROW( ERR_MSG("DjVuInfo.corrupt_file") );
  // Analyze data with backward compatibility in mind!
  if (size>=2)
    width = (buffer[0]<<8) + buffer[1];
  if (size>=4)
    height = (buffer[2]<<8) + buffer[3];
  if (size>=5)
    version = buffer[4];
  if (size>=6 && buffer[5]!=0xff)
    version = (buffer[5]<<8) + buffer[4];
  if (size>=8 && buffer[7]!=0xff)
    dpi = (buffer[7]<<8) + buffer[6];
  if (size>=9)
    gamma = 0.1 * buffer[8];
  int flags=0;
  if (size>=10)
    flags = buffer[9];
  // Fixup
  if (gamma<0.3)
    gamma=0.3;
  if (gamma>5.0)
    gamma=5.0;
  if (dpi < 25 || dpi > 6000)
    dpi = 300;
  switch (flags & 0x7)
    {
    case 6:  orientation=1; break; 
    case 2:  orientation=2; break; 
    case 5:  orientation=3; break;
    default: orientation=0; break;
    }
}

void 
DjVuInfo::encode(ByteStream &bs)
{
  bs.write16(width);
  bs.write16(height);
  bs.write8(version & 0xff);
  bs.write8(version >> 8);
  bs.write8(dpi & 0xff);
  bs.write8(dpi >> 8);
  bs.write8((int)(10.0*gamma+0.5) );
  unsigned char flags;
  switch (orientation) 
    {
    default: flags=1; break;
    case 1:  flags=6; break;
    case 2:  flags=2; break;
    case 3:  flags=5; break;
    }
  bs.write8(flags);
}

unsigned int 
DjVuInfo::get_memory_usage() const
{
  return sizeof(DjVuInfo);
}

GUTF8String
DjVuInfo::get_paramtags(void) const
{
  GUTF8String retval;
  if(orientation)
    retval+="<PARAM name=\"ROTATE\" value=\""
      +GUTF8String(((4-orientation)%4)*90)+"\" />\n";
  if(dpi)
    retval+="<PARAM name=\"DPI\" value=\""
      +GUTF8String(dpi)+"\" />\n";
  if(gamma)
    retval+="<PARAM name=\"GAMMA\" value=\""
      +GUTF8String(gamma)+"\" />\n";
  return retval;
}

void
DjVuInfo::writeParam(ByteStream &str_out) const
{
  str_out.writestring(get_paramtags());
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
