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

/** This file impliments the DjVuProgressTask elements.  The memory
    functions are implimented in a separate file, because only the memory
    functions should be compiled with out overloading of the memory functions.
 */
  

#ifdef NEED_DJVU_PROGRESS
#include "DjVuGlobal.h"


// ----------------------------------------

#include "GOS.h"
#include "GThreads.h"
#include "GException.h"
#include "GContainer.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define INITIAL  500
#define INTERVAL 250

class DjVuProgressTask::Data : public GPEnabled
{
public:
  djvu_progress_callback *callback;
  DjVuProgressTask *head;
  const char *gtask;
  unsigned long lastsigdate;
  Data(djvu_progress_callback *_callback):
    callback(_callback), head(0), gtask(0), lastsigdate(0) {}
};

  
static GPMap<void *,DjVuProgressTask::Data> &
get_map(void)
{
  static GPMap<void *,DjVuProgressTask::Data> xmap;
  return xmap;
}

djvu_progress_callback *
DjVuProgressTask::set_callback(djvu_progress_callback *_callback)
{ 
  djvu_progress_callback *retval=0;
  if(_callback)
  {
    GMap<void *,GP<DjVuProgressTask::Data> > &map=get_map();
    void *threadID=GThread::current();
    if(map.contains(threadID))
    {
      DjVuProgressTask::Data &data=*(map[threadID]);
      retval=data.callback;
      data.callback=_callback;
      data.head=0;
      data.gtask=0;
      data.lastsigdate=0;
    }else
    {
      map[threadID]=new Data(_callback);
    }
  }else
  {
    GMap<void *,GP<DjVuProgressTask::Data> > &map=get_map();
    void *threadID=GThread::current();
    if(map.contains(threadID))
    {
      DjVuProgressTask::Data &data=*(map[threadID]);
      retval=data.callback;
      data.callback=0;
      data.head=0;
      data.gtask=0;
      data.lastsigdate=0;
      map.del(threadID);
    }
  }
  return retval;
}

DjVuProgressTask::DjVuProgressTask(const char *xtask,int nsteps)
  : task(xtask),parent(0), nsteps(nsteps), runtostep(0), gdata(0), data(0)
{
  //  gtask=task;
  {
    GMap<void *,GP<DjVuProgressTask::Data> > &map=get_map();
    void *threadID=GThread::current();
    if(map.contains(threadID))
    {
      gdata=new GP<Data>;
      Data &d=*(data=((*(GP<Data> *)gdata)=map[threadID]));
      if(d.callback)
      {
        unsigned long curdate = GOS::ticks();
        startdate = curdate;
        if (!d.head)
          d.lastsigdate = curdate + INITIAL;
        parent = d.head;
        d.head = this;
      }
    }
  }
}

DjVuProgressTask::~DjVuProgressTask()
{
  if (data && data->callback)
  {
    if (data->head != this)
      G_THROW( ERR_MSG("DjVuGlobal.not_compatible") );
    data->head = parent;
    if (!parent)
    {
      unsigned long curdate = GOS::ticks();
      if((*(data->callback))(data->gtask?data->gtask:"",curdate-startdate, curdate-startdate))
      {
        G_THROW("INTERRUPT");
      }
    }
  }
  delete (GP<Data> *)gdata;
}

void
DjVuProgressTask::run(int tostep)
{
  if(data)
  {
    data->gtask=task;
    if ((data->callback)&&(tostep>runtostep))
    {
      unsigned long curdate = GOS::ticks();
      if (curdate > data->lastsigdate + INTERVAL)
        signal(curdate, curdate);
      runtostep = tostep;
    }
  }
}

void
DjVuProgressTask::signal(unsigned long curdate, unsigned long estdate)
{
  int inprogress = runtostep;
  if (inprogress > nsteps)
    inprogress = nsteps;
  if (inprogress > 0)
    {
      const unsigned long enddate = startdate+
        (unsigned long)(((float)(estdate-startdate) * (float)nsteps) / (float)inprogress);
      if (parent)
      {
        parent->signal(curdate, enddate);
      }
      else if (data && data->callback && curdate<enddate)
      {
        if((*(data->callback))(data->gtask?data->gtask:"",curdate-startdate, enddate-startdate))
        {
          G_THROW("INTERRUPT");
        }
        data->lastsigdate = curdate;
      }
    }
}

// Progress callback
//
djvu_progress_callback *
djvu_set_progress_callback( djvu_progress_callback *callback )
{
   return DjVuProgressTask::set_callback(callback);
}

int djvu_supports_progress_callback(void) {return 1;}

#else

#ifndef HAS_DJVU_PROGRESS_TYPEDEF
extern "C"
{
  void *djvu_set_progress_callback(void *);
  int djvu_supports_progress_callback(void);
}
void *djvu_set_progress_callback(void *) { return 0; }
int djvu_supports_progress_callback(void) {return 0;}
#else
int djvu_supports_progress_callback(void) {return 0;}
djvu_progress_callback *
djvu_set_progress_callback( djvu_progress_callback *) { return 0; }
#endif

#endif

