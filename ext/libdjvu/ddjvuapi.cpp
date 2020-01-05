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
# pragma implementation "ddjvuapi.h"
#endif

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>

#ifdef HAVE_NAMESPACES
namespace DJVU {
  struct ddjvu_context_s;
  struct ddjvu_job_s;
  struct ddjvu_document_s;
  struct ddjvu_page_s;
  struct ddjvu_format_s;
  struct ddjvu_message_p;
  struct ddjvu_thumbnail_p;
  struct ddjvu_runnablejob_s;
  struct ddjvu_printjob_s;
  struct ddjvu_savejob_s;
}
using namespace DJVU;
# define DJVUNS DJVU::
#else
# define DJVUNS /**/
#endif

#include "GException.h"
#include "GSmartPointer.h"
#include "GThreads.h"
#include "GContainer.h"
#include "ByteStream.h"
#include "IFFByteStream.h"
#include "BSByteStream.h"
#include "GString.h"
#include "GBitmap.h"
#include "GPixmap.h"
#include "GScaler.h"
#include "DjVuPort.h"
#include "DataPool.h"
#include "DjVuInfo.h"
#include "IW44Image.h"
#include "DjVuImage.h"
#include "DjVuFileCache.h"
#include "DjVuDocument.h"
#include "DjVuDumpHelper.h"
#include "DjVuMessageLite.h"
#include "DjVuMessage.h"
#include "DjVmNav.h"
#include "DjVuText.h"
#include "DjVuAnno.h"
#include "DjVuToPS.h"
#include "DjVmDir.h"
#include "DjVmDir0.h"
#include "DjVuNavDir.h"
#include "DjVmDoc.h"


#include "miniexp.h"
#include "ddjvuapi.h"


// ----------------------------------------
// Private structures


struct DJVUNS ddjvu_message_p : public GPEnabled
{
  GNativeString tmp1;
  GNativeString tmp2;
  ddjvu_message_t p;
  ddjvu_message_p() { memset(&p, 0, sizeof(p)); }
};

struct DJVUNS ddjvu_thumbnail_p : public GPEnabled
{
  ddjvu_document_t *document;
  int pagenum;
  GTArray<char> data;
  GP<DataPool> pool;
  static void callback(void *);
}; 


// ----------------------------------------
// Context, Jobs, Document, Pages


struct DJVUNS ddjvu_context_s : public GPEnabled
{
  GMonitor monitor;
  GP<DjVuFileCache> cache;
  GPList<ddjvu_message_p> mlist;
  GP<ddjvu_message_p> mpeeked;
  int uniqueid;
  ddjvu_message_callback_t callbackfun;
  void *callbackarg;
};

struct DJVUNS ddjvu_job_s : public DjVuPort
{
  GMonitor monitor;
  void *userdata;
  GP<ddjvu_context_s> myctx;
  GP<ddjvu_document_s> mydoc;
  bool released;
  ddjvu_job_s();
  // virtual port functions:
  virtual bool inherits(const GUTF8String&) const;
  virtual bool notify_error(const DjVuPort*, const GUTF8String&);  
  virtual bool notify_status(const DjVuPort*, const GUTF8String&);
  // default implementation of virtual job functions:
  virtual ddjvu_status_t status() {return DDJVU_JOB_NOTSTARTED;}
  virtual void release() {}
  virtual void stop() {}
};

struct DJVUNS ddjvu_document_s : public ddjvu_job_s
{
  GP<DjVuDocument> doc;
  GPMap<int,DataPool> streams;
  GMap<GUTF8String, int> names;
  GPMap<int,ddjvu_thumbnail_p> thumbnails;
  int streamid;
  bool fileflag;
  bool urlflag;
  bool docinfoflag;
  bool pageinfoflag;
  minivar_t protect;
  // virtual job functions:
  virtual ddjvu_status_t status();
  virtual void release();
  // virtual port functions:
  virtual bool inherits(const GUTF8String&) const;
  virtual bool notify_error(const DjVuPort*, const GUTF8String&);  
  virtual bool notify_status(const DjVuPort*, const GUTF8String&);
  virtual void notify_doc_flags_changed(const DjVuDocument*, long, long);
  virtual GP<DataPool> request_data(const DjVuPort*, const GURL&);
  static void callback(void *);
  bool want_pageinfo(void);
};

struct DJVUNS ddjvu_page_s : public ddjvu_job_s
{
  GP<DjVuImage> img;
  ddjvu_job_t *job;
  bool pageinfoflag;            // was the first m_pageinfo sent?
  bool pagedoneflag;            // was the final m_pageinfo sent?
  // virtual job functions:
  virtual ddjvu_status_t status();
  virtual void release();
  // virtual port functions:
  virtual bool inherits(const GUTF8String&) const;
  virtual bool notify_error(const DjVuPort*, const GUTF8String&);  
  virtual bool notify_status(const DjVuPort*, const GUTF8String&);
  virtual void notify_file_flags_changed(const DjVuFile*, long, long);
  virtual void notify_relayout(const class DjVuImage*);
  virtual void notify_redisplay(const class DjVuImage*);
  virtual void notify_chunk_done(const DjVuPort*, const GUTF8String &);
  void sendmessages();
};


// ----------------------------------------
// Helpers


// Hack to increment counter
static void 
ref(GPEnabled *p)
{
  GPBase n(p);
  char *gn = (char*)&n;
  *(GPEnabled**)gn = 0;
  n.assign(0);
}

// Hack to decrement counter
static void 
unref(GPEnabled *p)
{
  GPBase n;
  char *gn = (char*)&n;
  *(GPEnabled**)gn = p;
  n.assign(0);
}

// Allocate strings
static char *
xstr(const char *s)
{
  int l = strlen(s);
  char *p = (char*)malloc(l + 1);
  if (p) 
    {
      strcpy(p, s);
      p[l] = 0;
    }
  return p;
}

// Allocate strings
static char *
xstr(const GNativeString &n)
{
  return xstr( (const char*) n );
}

// Allocate strings
static char *
xstr(const GUTF8String &u)
{
  GNativeString n(u);
  return xstr( n );
}

// Fill a message head
static ddjvu_message_any_t 
xhead(ddjvu_message_tag_t tag,
      ddjvu_context_t *context)
{
  ddjvu_message_any_t any;
  any.tag = tag;
  any.context = context;
  any.document = 0;
  any.page = 0;
  any.job = 0;
  return any;
}
static ddjvu_message_any_t 
xhead(ddjvu_message_tag_t tag,
      ddjvu_job_t *job)
{
  ddjvu_message_any_t any;
  any.tag = tag;
  any.context = job->myctx;
  any.document = job->mydoc;
  any.page = 0;
  any.job = job;
  return any;
}
static ddjvu_message_any_t 
xhead(ddjvu_message_tag_t tag,
      ddjvu_document_t *document)
{
  ddjvu_message_any_t any;
  any.tag = tag;
  any.context = document->myctx;
  any.document = document;
  any.page = 0;
  any.job = document;
  return any;
}
static ddjvu_message_any_t 
xhead(ddjvu_message_tag_t tag,
      ddjvu_page_t *page)
{
  ddjvu_message_any_t any;
  any.tag = tag;
  any.context = page->myctx;
  any.document = page->mydoc;
  any.page = page;
  any.job = page->job;
  return any;
}


// ----------------------------------------
// Version

const char*
ddjvu_get_version_string(void)
{
#ifdef DJVULIBRE_VERSION
  return "DjVuLibre-" DJVULIBRE_VERSION;
#else
  return "DjVuLibre";
#endif
}

int
ddjvu_code_get_version(void)
{
  return DJVUVERSION;
}




// ----------------------------------------
// Context


ddjvu_context_t *
ddjvu_context_create(const char *programname)
{
  ddjvu_context_t *ctx = 0;
  G_TRY
    {
#ifdef LC_ALL
      setlocale(LC_ALL,"");
# ifdef LC_NUMERIC
      setlocale(LC_NUMERIC, "C");
# endif
#endif
      if (programname)
        djvu_programname(programname);
      DjVuMessage::use_language();
      DjVuMessageLite::create();
      ctx = new ddjvu_context_s;
      ref(ctx);
      ctx->uniqueid = 0;
      ctx->callbackfun = 0;
      ctx->callbackarg = 0;
      ctx->cache = DjVuFileCache::create();
    }
  G_CATCH_ALL
    {
      if (ctx)
        unref(ctx);
      ctx = 0;
    }
  G_ENDCATCH;
  return ctx;
}

void 
ddjvu_context_release(ddjvu_context_t *ctx)
{
  G_TRY
    {
      if (ctx)
        unref(ctx);
    }
  G_CATCH_ALL
    {
    }
  G_ENDCATCH;
}


// ----------------------------------------
// Message helpers


// post a new message
static void
msg_push(const ddjvu_message_any_t &head,
         GP<ddjvu_message_p> msg = 0)
{
  ddjvu_context_t *ctx = head.context;
  if (! msg) 
    msg = new ddjvu_message_p;
  msg->p.m_any = head; 
  GMonitorLock lock(&ctx->monitor);
  if ((head.document && head.document->released) ||
      (head.page && head.page->released) ||
      (head.job && head.job->released) )
    return;
  if (ctx->callbackfun) 
    (*ctx->callbackfun)(ctx, ctx->callbackarg);
  ctx->mlist.append(msg);
  ctx->monitor.broadcast();
}

static void
msg_push_nothrow(const ddjvu_message_any_t &head,
                 GP<ddjvu_message_p> msg = 0)
{
  G_TRY
    {
      msg_push(head, msg);
    }
  G_CATCH_ALL
    {
    }
  G_ENDCATCH;
}

// prepare error message from string
static GP<ddjvu_message_p>
msg_prep_error(GUTF8String message,
               const char *function=0, 
               const char *filename=0, 
               int lineno=0)
{
  GP<ddjvu_message_p> p = new ddjvu_message_p;
  p->p.m_error.message = 0;
  p->p.m_error.function = function;
  p->p.m_error.filename = filename;
  p->p.m_error.lineno = lineno;
  G_TRY 
    { 
      p->tmp1 = DjVuMessageLite::LookUpUTF8(message);
      p->p.m_error.message = (const char*)(p->tmp1);
    }
  G_CATCH_ALL 
    {
    } 
  G_ENDCATCH;
  return p;
}

// prepare error message from exception
static GP<ddjvu_message_p>
msg_prep_error(const GException &ex,
               const char *function=0, 
               const char *filename=0, 
               int lineno=0)
{
  GP<ddjvu_message_p> p = new ddjvu_message_p;
  p->p.m_error.message = 0;
  p->p.m_error.function = function;
  p->p.m_error.filename = filename;
  p->p.m_error.lineno = lineno;
  G_TRY 
    { 
      p->tmp1 = DjVuMessageLite::LookUpUTF8(ex.get_cause());
      p->p.m_error.message = (const char*)(p->tmp1);
      p->p.m_error.function = ex.get_function();
      p->p.m_error.filename = ex.get_file();
      p->p.m_error.lineno = ex.get_line();
    }
  G_CATCH_ALL 
    {
    } 
  G_ENDCATCH;
  return p;
}

// prepare status message
static GP<ddjvu_message_p>
msg_prep_info(GUTF8String message)
{
  GP<ddjvu_message_p> p = new ddjvu_message_p;
  p->tmp1 = DjVuMessageLite::LookUpUTF8(message); // i18n nonsense!
  p->p.m_info.message = (const char*)(p->tmp1);
  return p;
}

// ----------------------------------------


#ifdef __GNUG__
# define ERROR1(x, m) \
    msg_push_nothrow(xhead(DDJVU_ERROR,x),\
                     msg_prep_error(m,__func__,__FILE__,__LINE__))
#else
# define ERROR1(x, m) \
    msg_push_nothrow(xhead(DDJVU_ERROR,x),\
                     msg_prep_error(m,0,__FILE__,__LINE__))
#endif


// ----------------------------------------
// Cache

void
ddjvu_cache_set_size(ddjvu_context_t *ctx,
                     unsigned long cachesize)
{
  G_TRY
    {
      GMonitorLock lock(&ctx->monitor);
      if (ctx->cache && cachesize>0)
        ctx->cache->set_max_size(cachesize);
    }
  G_CATCH(ex) 
    {
      ERROR1(ctx, ex);
    }
  G_ENDCATCH;
}

DDJVUAPI unsigned long
ddjvu_cache_get_size(ddjvu_context_t *ctx)
{
  G_TRY
    {
      GMonitorLock lock(&ctx->monitor);
      if (ctx->cache)
        return ctx->cache->get_max_size();
    }
  G_CATCH(ex) 
    { 
      ERROR1(ctx, ex);
    }
  G_ENDCATCH;
  return 0;
}

void
ddjvu_cache_clear(ddjvu_context_t *ctx)
{
  G_TRY
    {
      GMonitorLock lock(&ctx->monitor);
      DataPool::close_all();
      if (ctx->cache)
      {
        ctx->cache->clear();
        return;
      }
    }
  G_CATCH(ex)
    {
      ERROR1(ctx, ex);
    }
  G_ENDCATCH;
 }


// ----------------------------------------
// Jobs

ddjvu_job_s::ddjvu_job_s()
  : userdata(0), released(false)
{
}

bool
ddjvu_job_s::inherits(const GUTF8String &classname) const
{
  return (classname == "ddjvu_job_s") 
    || DjVuPort::inherits(classname);
}

bool 
ddjvu_job_s::notify_error(const DjVuPort *, const GUTF8String &m)
{
  msg_push(xhead(DDJVU_ERROR, this), msg_prep_error(m));
  return true;
}

bool 
ddjvu_job_s::notify_status(const DjVuPort *p, const GUTF8String &m)
{
  msg_push(xhead(DDJVU_INFO, this), msg_prep_info(m));
  return true;
}

void
ddjvu_job_release(ddjvu_job_t *job)
{
  G_TRY
    {
      if (!job)
        return;
      job->release();
      job->userdata = 0;
      job->released = true;
      // clean all messages
      ddjvu_context_t *ctx = job->myctx;
      if (ctx)
        {
          GMonitorLock lock(&ctx->monitor);
          GPosition p = ctx->mlist;
          while (p) 
            {
              GPosition s = p; ++p;
              if (ctx->mlist[s]->p.m_any.job == job ||
                  ctx->mlist[s]->p.m_any.document == job ||
                  ctx->mlist[s]->p.m_any.page == job )
                ctx->mlist.del(s);
            }
          // cleanup pointers in current message as well.
          if (ctx->mpeeked)
            {
              ddjvu_message_t *m = &ctx->mpeeked->p;
              if (m->m_any.job == job)       
                m->m_any.job = 0;
              if (m->m_any.document == job)
                m->m_any.document = 0;
              if (m->m_any.page == job)
                m->m_any.page = 0;
            }
        }
      // decrement reference counter
      unref(job);
    }
  G_CATCH_ALL
    {
    }
  G_ENDCATCH;
}

ddjvu_status_t
ddjvu_job_status(ddjvu_job_t *job)
{
  G_TRY
    {
      if (! job)
        return DDJVU_JOB_NOTSTARTED;
      return job->status();
    }
  G_CATCH(ex)
    {
      ERROR1(job, ex);
    }
  G_ENDCATCH;
  return DDJVU_JOB_FAILED;
}

void
ddjvu_job_stop(ddjvu_job_t *job)
{
  G_TRY
    {
      if (job)
        job->stop();
    }
  G_CATCH(ex)
    {
      ERROR1(job, ex);
    }
  G_ENDCATCH;
}

void
ddjvu_job_set_user_data(ddjvu_job_t *job, void *userdata)
{
  if (job)
    job->userdata = userdata;
}

void *
ddjvu_job_get_user_data(ddjvu_job_t *job)
{
  if (job)
    return job->userdata;
  return 0;
}


// ----------------------------------------
// Message queue


ddjvu_message_t *
ddjvu_message_peek(ddjvu_context_t *ctx)
{
  G_TRY
    {
      GMonitorLock lock(&ctx->monitor);
      if (ctx->mpeeked)
        return &ctx->mpeeked->p;        
      if (! ctx->mlist.size())
        ctx->monitor.wait(0);
      GPosition p = ctx->mlist;
      if (! p)
        return 0;
      ctx->mpeeked = ctx->mlist[p];
      ctx->mlist.del(p);
      return &ctx->mpeeked->p;
    }
  G_CATCH_ALL
    {
    }
  G_ENDCATCH;
  return 0;
}

ddjvu_message_t *
ddjvu_message_wait(ddjvu_context_t *ctx)
{
  G_TRY
    {
      GMonitorLock lock(&ctx->monitor);
      if (ctx->mpeeked)
        return &ctx->mpeeked->p;        
      while (! ctx->mlist.size())
        ctx->monitor.wait();
      GPosition p = ctx->mlist;
      if (! p)
        return 0;
      ctx->mpeeked = ctx->mlist[p];
      ctx->mlist.del(p);
      return &ctx->mpeeked->p;        
    }
  G_CATCH_ALL
    {
    }
  G_ENDCATCH;
  return 0;
}

void
ddjvu_message_pop(ddjvu_context_t *ctx)
{
  G_TRY
    {
      GMonitorLock lock(&ctx->monitor);
      ctx->mpeeked = 0;
    }
  G_CATCH_ALL
    {
    }
  G_ENDCATCH;
}

void
ddjvu_message_set_callback(ddjvu_context_t *ctx,
                           ddjvu_message_callback_t callback,
                           void *closure)
{
  GMonitorLock lock(&ctx->monitor);
  ctx->callbackfun = callback;
  ctx->callbackarg = closure;
}


// ----------------------------------------
// Document callbacks


void
ddjvu_document_s::release()
{
  GPosition p;
  GMonitorLock lock(&monitor);
  doc = 0;
  for (p=thumbnails; p; ++p)
    {
      ddjvu_thumbnail_p *thumb = thumbnails[p];
      if (thumb->pool)
        thumb->pool->del_trigger(ddjvu_thumbnail_p::callback, (void*)thumb);
    }
  for (p = streams; p; ++p)
    {
      GP<DataPool> pool = streams[p];
      if (pool)
        pool->del_trigger(callback, (void*)this);
      if (pool && !pool->is_eof())
        pool->stop();
    }
}

ddjvu_status_t
ddjvu_document_s::status()
{
  if (!doc)
    return DDJVU_JOB_NOTSTARTED;
  long flags = doc->get_doc_flags();
  if (flags & DjVuDocument::DOC_INIT_OK)
    return DDJVU_JOB_OK;
  else if (flags & DjVuDocument::DOC_INIT_FAILED)
    return DDJVU_JOB_FAILED;
  return DDJVU_JOB_STARTED;
}

bool
ddjvu_document_s::inherits(const GUTF8String &classname) const
{
  return (classname == "ddjvu_document_s")
    || ddjvu_job_s::inherits(classname);
}

bool 
ddjvu_document_s::notify_error(const DjVuPort *, const GUTF8String &m)
{
  if (!doc) return false;
  msg_push(xhead(DDJVU_ERROR, this), msg_prep_error(m));
  return true;
}
 
bool 
ddjvu_document_s::notify_status(const DjVuPort *p, const GUTF8String &m)
{
  if (!doc) return false;
  msg_push(xhead(DDJVU_INFO, this), msg_prep_info(m));
  return true;
}

void 
ddjvu_document_s::notify_doc_flags_changed(const DjVuDocument *, long, long)
{
  GMonitorLock lock(&monitor);
  if (docinfoflag || !doc) return;
  long flags = doc->get_doc_flags();
  if ((flags & DjVuDocument::DOC_INIT_OK) ||
      (flags & DjVuDocument::DOC_INIT_FAILED) )
  {
    msg_push(xhead(DDJVU_DOCINFO, this));
    docinfoflag = true;
  }
}


void 
ddjvu_document_s::callback(void *arg)
{
  ddjvu_document_t *doc = (ddjvu_document_t *)arg;
  if (doc && doc->pageinfoflag && !doc->fileflag) 
    msg_push(xhead(DDJVU_PAGEINFO, doc));
}


GP<DataPool> 
ddjvu_document_s::request_data(const DjVuPort *p, const GURL &url)
{
  // Note: the following line try to restore
  //       the bytes stored in the djvu file
  //       despite LT's i18n and gurl classes.
  GUTF8String name = (const char*)url.fname(); 
  GMonitorLock lock(&monitor);
  GP<DataPool> pool;
  if (names.contains(name))
    {
      int streamid = names[name];
      return streams[streamid];
    }
  else if (fileflag)
    {
      if (doc && url.is_local_file_url())
        return DataPool::create(url);
    }
  else if (doc)
    {
      // prepare pool
      if (++streamid > 0)
        streams[streamid] = pool = DataPool::create();
      else
        pool = streams[(streamid = 0)];
      names[name] = streamid;
      pool->add_trigger(-1, callback, (void*)this);
      // build message
      GP<ddjvu_message_p> p = new ddjvu_message_p;
      p->p.m_newstream.streamid = streamid;
      p->tmp1 = name;
      p->p.m_newstream.name = (const char*)(p->tmp1);
      p->p.m_newstream.url = 0;
      if (urlflag)
        {
          // Should be urlencoded.
          p->tmp2 = (const char*)url.get_string();
          p->p.m_newstream.url = (const char*)(p->tmp2);
        }
      msg_push(xhead(DDJVU_NEWSTREAM, this), p);
    }
  return pool;
}


bool
ddjvu_document_s::want_pageinfo()
{
  if (doc && docinfoflag && !pageinfoflag)
    {
      pageinfoflag = true;
      int doctype = doc->get_doc_type();
      if (doctype == DjVuDocument::BUNDLED ||
          doctype == DjVuDocument::OLD_BUNDLED )
        {
          GP<DataPool> pool;
          {
            GMonitorLock lock(&monitor);
            if (streams.contains(0))
              pool = streams[0];
          }
          if (pool && doctype == DjVuDocument::BUNDLED)
            {
              GP<DjVmDir> dir = doc->get_djvm_dir();
              if (dir)
                for (int i=0; i<dir->get_files_num(); i++)
                  {
                    GP<DjVmDir::File> f = dir->pos_to_file(i);
                    if (! pool->has_data(f->offset, f->size))
                      pool->add_trigger(f->offset, f->size, callback, (void*)this );
                  }
            }
          else if (pool && doctype == DjVuDocument::OLD_BUNDLED)
            {
              GP<DjVmDir0> dir = doc->get_djvm_dir0();
              if (dir)
                for (int i=0; i<dir->get_files_num(); i++)
                  {
                    GP<DjVmDir0::FileRec> f = dir->get_file(i);
                    if (! pool->has_data(f->offset, f->size))
                      pool->add_trigger(f->offset, f->size, callback, (void*)this );
                  }
            }
        }
    }
  return pageinfoflag;
}


// ----------------------------------------
// Documents


ddjvu_document_t *
ddjvu_document_create(ddjvu_context_t *ctx,
                      const char *url,
                      int cache)
{
  ddjvu_document_t *d = 0;
  G_TRY
    {
      DjVuFileCache *xcache = ctx->cache;
      if (! cache) xcache = 0;
      d = new ddjvu_document_s;
      ref(d);
      GMonitorLock lock(&d->monitor);
      d->streams[0] = DataPool::create();
      d->streamid = -1;
      d->fileflag = false;
      d->docinfoflag = false;
      d->pageinfoflag = false;
      d->myctx = ctx;
      d->mydoc = 0;
      d->doc = DjVuDocument::create_noinit();
      if (url)
        {
          GURL gurl = GUTF8String(url);
          gurl.clear_djvu_cgi_arguments();
          d->urlflag = true;
          d->doc->start_init(gurl, d, xcache);
        }
      else
        {
          GUTF8String s;
          s.format("ddjvu:///doc%d/index.djvu", ++(ctx->uniqueid));;
          GURL gurl = s;
          d->urlflag = false;
          d->doc->start_init(gurl, d, xcache);
        }
    }
  G_CATCH(ex)
    {
      if (d) 
        unref(d);
      d = 0;
      ERROR1(ctx, ex);
    }
  G_ENDCATCH;
  return d;
}

static ddjvu_document_t *
ddjvu_document_create_by_filename_imp(ddjvu_context_t *ctx,
                                      const char *filename,
                                      int cache, int utf8)
{
  ddjvu_document_t *d = 0;
  G_TRY
    {
      DjVuFileCache *xcache = ctx->cache;
      if (! cache) xcache = 0;
      GURL gurl;
      if (utf8) 
        gurl = GURL::Filename::UTF8(filename);
      else
        gurl = GURL::Filename::Native(filename);
      d = new ddjvu_document_s;
      ref(d);
      GMonitorLock lock(&d->monitor);
      d->streamid = -1;
      d->fileflag = true;
      d->pageinfoflag = false;
      d->urlflag = false;
      d->docinfoflag = false;
      d->myctx = ctx;
      d->mydoc = 0;
      d->doc = DjVuDocument::create_noinit();
      d->doc->start_init(gurl, d, xcache);
    }
  G_CATCH(ex)
    {
      if (d)
        unref(d);
      d = 0;
      ERROR1(ctx, ex);
    }
  G_ENDCATCH;
  return d;
}

ddjvu_document_t *
ddjvu_document_create_by_filename(ddjvu_context_t *ctx,
                                  const char *filename,
                                  int cache)
{
  return ddjvu_document_create_by_filename_imp(ctx,filename,cache,0);
}

ddjvu_document_t *
ddjvu_document_create_by_filename_utf8(ddjvu_context_t *ctx,
                                       const char *filename,
                                       int cache)
{
  return ddjvu_document_create_by_filename_imp(ctx,filename,cache,1);
}

/* SumatraPDF: ddjvu_document_create_by_data */
ddjvu_document_t *
ddjvu_document_create_by_data(ddjvu_context_t *ctx,
                              const char *data,
                              unsigned long datalen)
{
  ddjvu_document_t *d = 0;
  G_TRY
    {
      d = new ddjvu_document_s;
      ref(d);
      GMonitorLock lock(&d->monitor);
      d->streams[0] = DataPool::create();
      d->streamid = -1;
      d->fileflag = false;
      d->docinfoflag = false;
      d->pageinfoflag = false;
      d->myctx = ctx;
      d->mydoc = 0;
      d->doc = DjVuDocument::create_noinit();
      ddjvu_stream_write(d, 0, data, datalen);
      ddjvu_stream_close(d, 0, 0);
      GUTF8String s;
      s.format("ddjvu:///doc%d/index.djvu", ++(ctx->uniqueid));;
      GURL gurl = s;
      d->urlflag = false;
      d->doc->start_init(gurl, d, 0);
    }
  G_CATCH(ex)
    {
      if (d) 
        unref(d);
      d = 0;
      ERROR1(ctx, ex);
    }
  G_ENDCATCH;
  return d;
}

ddjvu_job_t *
ddjvu_document_job(ddjvu_document_t *document)
{
  return document;
}


// ----------------------------------------
// Streams


void
ddjvu_stream_write(ddjvu_document_t *doc,
                   int streamid,
                   const char *data,
                   unsigned long datalen )
{
  G_TRY
    {
      GP<DataPool> pool;
      { 
        GMonitorLock lock(&doc->monitor); 
        GPosition p = doc->streams.contains(streamid);
        if (p) pool = doc->streams[p];
      }
      if (! pool)
        G_THROW("Unknown stream ID");
      if (datalen > 0)
        pool->add_data(data, datalen);
    }
  G_CATCH(ex)
    {
      ERROR1(doc,ex);
    }
  G_ENDCATCH;
}

void
ddjvu_stream_close(ddjvu_document_t *doc,
                   int streamid,
                   int stop )
{
  G_TRY
    {
      GP<DataPool> pool;
      { 
        GMonitorLock lock(&doc->monitor); 
        GPosition p = doc->streams.contains(streamid);
        if (p) pool = doc->streams[p];
      }
      if (! pool)
        G_THROW("Unknown stream ID");
      if (stop)
        pool->stop(true);
      pool->set_eof();
    }
  G_CATCH(ex)
    {
      ERROR1(doc, ex);
    }
  G_ENDCATCH;
}


// ----------------------------------------
// Document queries


ddjvu_document_type_t
ddjvu_document_get_type(ddjvu_document_t *document)
{
  G_TRY
    {
      DjVuDocument *doc = document->doc;
      if (doc)
        {
          switch (doc->get_doc_type())
            {
            case DjVuDocument::OLD_BUNDLED:
              return DDJVU_DOCTYPE_OLD_BUNDLED;
            case DjVuDocument::OLD_INDEXED:
              return DDJVU_DOCTYPE_OLD_INDEXED;
            case DjVuDocument::BUNDLED:
              return DDJVU_DOCTYPE_BUNDLED;
            case DjVuDocument::INDIRECT:
              return DDJVU_DOCTYPE_INDIRECT;
            case DjVuDocument::SINGLE_PAGE:
              return DDJVU_DOCTYPE_SINGLEPAGE;
            default:
              break;
            }
        }
    }
  G_CATCH(ex)
    {
      ERROR1(document,ex);
    }
  G_ENDCATCH;
  return DDJVU_DOCTYPE_UNKNOWN;
}


int
ddjvu_document_get_pagenum(ddjvu_document_t *document)
{
  G_TRY
    {
      DjVuDocument *doc = document->doc;
      if (doc)
        return doc->get_pages_num();
    }
  G_CATCH(ex)
    {
      ERROR1(document,ex);
    }
  G_ENDCATCH;
  return 1;
}


int
ddjvu_document_get_filenum(ddjvu_document_t *document)
{
  G_TRY
    {
      DjVuDocument *doc = document->doc;
      if (! (doc && doc->is_init_ok()))
        return 0;
      int doc_type = doc->get_doc_type();
      if (doc_type == DjVuDocument::BUNDLED ||
          doc_type == DjVuDocument::INDIRECT )
        {
          GP<DjVmDir> dir = doc->get_djvm_dir();
          return dir->get_files_num();
        }
      else if (doc_type == DjVuDocument::OLD_BUNDLED)
        {
          GP<DjVmDir0> dir0 = doc->get_djvm_dir0();
          return dir0->get_files_num();
        }
      else 
        return doc->get_pages_num();
    }
  G_CATCH(ex)
    {
      ERROR1(document,ex);
    }
  G_ENDCATCH;
  return 0;
}


#undef ddjvu_document_get_fileinfo

extern "C" DDJVUAPI ddjvu_status_t
ddjvu_document_get_fileinfo(ddjvu_document_t *d, int f, ddjvu_fileinfo_t *i);

ddjvu_status_t
ddjvu_document_get_fileinfo(ddjvu_document_t *d, int f, ddjvu_fileinfo_t *i)
{
  // for binary backward compatibility with ddjvuapi=17
  struct info17_s { char t; int p,s; const char *d, *n, *l; };
  return ddjvu_document_get_fileinfo_imp(d,f,i,sizeof(info17_s));
}

ddjvu_status_t
ddjvu_document_get_fileinfo_imp(ddjvu_document_t *document, int fileno, 
                                ddjvu_fileinfo_t *info, 
                                unsigned int infosz )
{
  G_TRY
    {
      ddjvu_fileinfo_t myinfo;
      memset(info, 0, infosz);
      if (infosz > sizeof(myinfo))
        return DDJVU_JOB_FAILED;
      DjVuDocument *doc = document->doc;
      if (! doc)
        return DDJVU_JOB_NOTSTARTED;
      if (! doc->is_init_ok())
        return document->status();
      int type = doc->get_doc_type();
      if ( type == DjVuDocument::BUNDLED ||
           type == DjVuDocument::INDIRECT )
        {
          GP<DjVmDir> dir = doc->get_djvm_dir();
          GP<DjVmDir::File> file = dir->pos_to_file(fileno, &myinfo.pageno);
          if (! file)
            G_THROW("Illegal file number");
          myinfo.type = 'I';
          if (file->is_page())
            myinfo.type = 'P';
          else
            myinfo.pageno = -1;
          if (file->is_thumbnails())
            myinfo.type = 'T';
          if (file->is_shared_anno())
            myinfo.type = 'S';
          myinfo.size = file->size;
          myinfo.id = file->get_load_name();
          myinfo.name = file->get_save_name();
          myinfo.title = file->get_title();
          memcpy(info, &myinfo, infosz);
          return DDJVU_JOB_OK;
        }
      else if (type == DjVuDocument::OLD_BUNDLED)
        {
          GP<DjVmDir0> dir0 = doc->get_djvm_dir0();
          GP<DjVuNavDir> nav = doc->get_nav_dir();
          GP<DjVmDir0::FileRec> frec = dir0->get_file(fileno);
          if (! frec)
            G_THROW("Illegal file number");
          myinfo.size = frec->size;
          myinfo.id = (const char*) frec->name;
          myinfo.name = myinfo.title = myinfo.id;
          if (! nav)
            return DDJVU_JOB_STARTED;
          else if (nav->name_to_page(frec->name) >= 0)
            myinfo.type = 'P';
          else
            myinfo.type = 'I';
          memcpy(info, &myinfo, infosz);
          return DDJVU_JOB_OK;
        }
      else 
        {
          if (fileno<0 || fileno>=doc->get_pages_num())
            G_THROW("Illegal file number");
          myinfo.type = 'P';
          myinfo.pageno = fileno;
          myinfo.size = -1;
          GP<DjVuNavDir> nav = doc->get_nav_dir();
          myinfo.id = (nav) ? (const char *) nav->page_to_name(fileno) : 0;
          myinfo.name = myinfo.title = myinfo.id;
          GP<DjVuFile> file = doc->get_djvu_file(fileno, true);
          GP<DataPool> pool; 
          if (file) 
            pool = file->get_init_data_pool();
          if (pool)
            myinfo.size = pool->get_length();
          memcpy(info, &myinfo, infosz);
          return DDJVU_JOB_OK;
        }
    }
  G_CATCH(ex)
    {
      ERROR1(document,ex);
    }
  G_ENDCATCH;
  return DDJVU_JOB_FAILED;
}


int
ddjvu_document_search_pageno(ddjvu_document_t *document, const char *name)
{
  G_TRY
    {
      DjVuDocument *doc = document->doc;
      if (! (doc && doc->is_init_ok()))
        return -1;
      GP<DjVmDir> dir = doc->get_djvm_dir();
      if (! dir)
        return 0;
      GP<DjVmDir::File> file;
      if (! (file = dir->id_to_file(GUTF8String(name))))
        if (! (file = dir->name_to_file(GUTF8String(name))))
          if (! (file = dir->title_to_file(GUTF8String(name))))
            {
              char *edata=0;
              long int p = strtol(name, &edata, 10);
              if (edata!=name && !*edata && p>=1)
                file = dir->page_to_file(p-1);
            }
      if (file)
        {
          int pageno = -1;
          int fileno = dir->get_file_pos(file);
          if (dir->pos_to_file(fileno, &pageno))
            return pageno;
        }
    }
  G_CATCH(ex)
    {
      ERROR1(document,ex);
    }
  G_ENDCATCH;
  return -1;
}



int 
ddjvu_document_check_pagedata(ddjvu_document_t *document, int pageno)
{
  G_TRY
    {
      document->want_pageinfo();
      DjVuDocument *doc = document->doc;
      if (doc && doc->is_init_ok())
        {
          bool dontcreate = false;
          if (doc->get_doc_type() == DjVuDocument::INDIRECT ||
              doc->get_doc_type() == DjVuDocument::OLD_INDEXED )
            {
              dontcreate = true;
              GURL url = doc->page_to_url(pageno);
              if (! url.is_empty())
                {
                  GUTF8String name = (const char*)url.fname();
                  GMonitorLock lock(&document->monitor);
                  if (document->names.contains(name))
                    dontcreate = false;
                }
            }
          GP<DjVuFile> file = doc->get_djvu_file(pageno, dontcreate);
          if (file && file->is_data_present())
            return 1;
        }
    }
  G_CATCH(ex)
    {
      ERROR1(document,ex);
    }
  G_ENDCATCH;
  return 0;
}


#undef ddjvu_document_get_pageinfo

extern "C" DDJVUAPI ddjvu_status_t
ddjvu_document_get_pageinfo(ddjvu_document_t *d, int p, ddjvu_pageinfo_t *i);

ddjvu_status_t
ddjvu_document_get_pageinfo(ddjvu_document_t *d, int p, ddjvu_pageinfo_t *i)
{
  // for binary backward compatibility with ddjvuapi<=17
  struct info17_s { int w; int h; int d; };
  return ddjvu_document_get_pageinfo_imp(d,p,i,sizeof(struct info17_s));
}

ddjvu_status_t
ddjvu_document_get_pageinfo_imp(ddjvu_document_t *document, int pageno, 
                                ddjvu_pageinfo_t *pageinfo, 
                                unsigned int infosz)
{
  G_TRY
    {
      ddjvu_pageinfo_t myinfo;
      memset(pageinfo, 0, infosz);
      if (infosz > sizeof(myinfo))
        return DDJVU_JOB_FAILED;
      DjVuDocument *doc = document->doc;
      if (doc)
        {
          document->want_pageinfo();
          GP<DjVuFile> file = doc->get_djvu_file(pageno);
          if (! file || ! file->is_data_present() )
            return DDJVU_JOB_STARTED;
          const GP<ByteStream> pbs(file->get_djvu_bytestream(false, false));
          const GP<IFFByteStream> iff(IFFByteStream::create(pbs));
          GUTF8String chkid;
          if (iff->get_chunk(chkid))
            {
              if (chkid == "FORM:DJVU")
                {
                  while (iff->get_chunk(chkid) && chkid!="INFO")
                    iff->close_chunk();
                  if (chkid == "INFO")
                    {
                      GP<ByteStream> gbs = iff->get_bytestream();
                      GP<DjVuInfo> info=DjVuInfo::create();
                      info->decode(*gbs);
                      int rot = info->orientation;
                      myinfo.rotation = rot;
                      myinfo.width = (rot&1) ? info->height : info->width;
                      myinfo.height = (rot&1) ? info->width : info->height;
                      myinfo.dpi = info->dpi;
                      myinfo.version = info->version;
                      memcpy(pageinfo, &myinfo, infosz);
                      return DDJVU_JOB_OK;
                    }
                }
              else if (chkid == "FORM:BM44" || chkid == "FORM:PM44")
                {
                  while (iff->get_chunk(chkid) && 
                         chkid!="BM44" && chkid!="PM44")
                    iff->close_chunk();
                  if (chkid=="BM44" || chkid=="PM44")
                    {
                      GP<ByteStream> gbs = iff->get_bytestream();
                      if (gbs->read8() == 0)
                        {
                          gbs->read8();
                          unsigned char vhi = gbs->read8();
                          unsigned char vlo = gbs->read8();
                          unsigned char xhi = gbs->read8();
                          unsigned char xlo = gbs->read8();
                          unsigned char yhi = gbs->read8();
                          unsigned char ylo = gbs->read8();
                          myinfo.width = (xhi<<8)+xlo;
                          myinfo.height = (yhi<<8)+ylo;
                          myinfo.dpi = 100;
                          myinfo.rotation = 0;
                          myinfo.version = (vhi<<8)+vlo;
                          memcpy(pageinfo, &myinfo, infosz);
                        }
                    }
                }
            }
        }
    }
  G_CATCH(ex)
    {
      ERROR1(document, ex);
    }
  G_ENDCATCH;
  return DDJVU_JOB_FAILED;
}


static char *
get_file_dump(DjVuFile *file)
{
  DjVuDumpHelper dumper;
  GP<DataPool> pool = file->get_init_data_pool();
  GP<ByteStream> str = dumper.dump(pool);
  int size = str->size();
  char *buffer;
  if ((size = str->size()) > 0 && (buffer = (char*)malloc(size+1)))
    {
      str->seek(0);
      int len = str->readall(buffer, size);
      buffer[len] = 0;
      return buffer;
    }
  return 0;
}


char *
ddjvu_document_get_pagedump(ddjvu_document_t *document, int pageno)
{
  G_TRY
    {
      DjVuDocument *doc = document->doc;
      if (doc)
        {
          document->want_pageinfo();
          GP<DjVuFile> file = doc->get_djvu_file(pageno);
          if (file && file->is_data_present())
            return get_file_dump(file);
        }
    }
  G_CATCH(ex)
    {
      ERROR1(document, ex);
    }
  G_ENDCATCH;
  return 0;
}


char *
ddjvu_document_get_filedump(ddjvu_document_t *document, int fileno)
{
  G_TRY
    {
      DjVuDocument *doc = document->doc;
      document->want_pageinfo();
      if (doc)
        {
          GP<DjVuFile> file;
          int type = doc->get_doc_type();
          if ( type != DjVuDocument::BUNDLED &&
               type != DjVuDocument::INDIRECT )
            file = doc->get_djvu_file(fileno);
          else
            {
              GP<DjVmDir> dir = doc->get_djvm_dir();
              GP<DjVmDir::File> fdesc = dir->pos_to_file(fileno);
              if (fdesc)
                file = doc->get_djvu_file(fdesc->get_load_name());
            }
          if (file && file->is_data_present())
            return get_file_dump(file);
        }
    }
  G_CATCH(ex)
    {
      ERROR1(document, ex);
    }
  G_ENDCATCH;
  return 0;
}



// ----------------------------------------
// Page

static ddjvu_page_t *
ddjvu_page_create(ddjvu_document_t *document, ddjvu_job_t *job,
                  const char *pageid, int pageno)
{
  ddjvu_page_t *p = 0;
  G_TRY
    {
      DjVuDocument *doc = document->doc;
      if (! doc) return 0;
      p = new ddjvu_page_s;
      ref(p);
      GMonitorLock lock(&p->monitor);
      p->myctx = document->myctx;
      p->mydoc = document;
      p->pageinfoflag = false;
      p->pagedoneflag = false;
      if (! job)
        job = p;
      p->job = job;
      if (pageid)
        p->img = doc->get_page(GNativeString(pageid), false, job);
      else
        p->img = doc->get_page(pageno, false, job);
      // synthetize msgs for pages found in the cache
      ddjvu_status_t status = p->status();
      if (status == DDJVU_JOB_OK)
        p->notify_redisplay(p->img);
      if (status >= DDJVU_JOB_OK)
        p->notify_file_flags_changed(p->img->get_djvu_file(), 0, 0);
    }
  G_CATCH(ex)
    {
      if (p)
        unref(p);
      p = 0;
      ERROR1(document, ex);
    }
  G_ENDCATCH;
  return p;
}

ddjvu_page_t *
ddjvu_page_create_by_pageno(ddjvu_document_t *document, int pageno)
{
  return ddjvu_page_create(document, 0, 0, pageno);
}

ddjvu_page_t *
ddjvu_page_create_by_pageid(ddjvu_document_t *document, const char *pageid)
{
  return ddjvu_page_create(document, 0, pageid, 0);
}

ddjvu_job_t *
ddjvu_page_job(ddjvu_page_t *page)
{
  return page;
}


// ----------------------------------------
// Page callbacks

void
ddjvu_page_s::release()
{
  img = 0;
}

ddjvu_status_t
ddjvu_page_s::status()
{
  if (! img)
    return DDJVU_JOB_NOTSTARTED;        
  DjVuFile *file = img->get_djvu_file();
  DjVuInfo *info = img->get_info();
  if (! file)
    return DDJVU_JOB_NOTSTARTED;
  else if (file->is_decode_stopped())
    return DDJVU_JOB_STOPPED;
  else if (file->is_decode_failed())
    return DDJVU_JOB_FAILED;
  else if (file->is_decode_ok())
    return (info) ? DDJVU_JOB_OK : DDJVU_JOB_FAILED;
  else if (file->is_decoding())
    return DDJVU_JOB_STARTED;
  return DDJVU_JOB_NOTSTARTED;
}

bool
ddjvu_page_s::inherits(const GUTF8String &classname) const
{
  return (classname == "ddjvu_page_s")
    || ddjvu_job_s::inherits(classname);
}

bool 
ddjvu_page_s::notify_error(const DjVuPort *, const GUTF8String &m)
{
  if (!img) return false;
  msg_push(xhead(DDJVU_ERROR, this), msg_prep_error(m));
  return true;
}
 
bool 
ddjvu_page_s::notify_status(const DjVuPort *p, const GUTF8String &m)
{
  if (!img) return false;
  msg_push(xhead(DDJVU_INFO, this), msg_prep_info(m));
  return true;
}

void 
ddjvu_page_s::notify_file_flags_changed(const DjVuFile *sender, long, long)
{
  GMonitorLock lock(&monitor);
  if (!img) return;
  DjVuFile *file = img->get_djvu_file();
  if (file==0 || file!=sender) return;
  long flags = file->get_flags();
  if ((flags & DjVuFile::DECODE_OK) ||
      (flags & DjVuFile::DECODE_FAILED) ||
      (flags & DjVuFile::DECODE_STOPPED) )
    {
      if (pagedoneflag) return;
      msg_push(xhead(DDJVU_PAGEINFO, this));
      pageinfoflag = pagedoneflag = true;
    }
}

void 
ddjvu_page_s::notify_relayout(const DjVuImage *dimg)
{
  GMonitorLock lock(&monitor);
  if (img && !pageinfoflag)
    {
      msg_push(xhead(DDJVU_PAGEINFO, this));
      msg_push(xhead(DDJVU_RELAYOUT, this));
      pageinfoflag = true;
    }
}

void 
ddjvu_page_s::notify_redisplay(const DjVuImage *dimg)
{
  GMonitorLock lock(&monitor);
  if (img && !pageinfoflag)
    {
      msg_push(xhead(DDJVU_PAGEINFO, this));
      msg_push(xhead(DDJVU_RELAYOUT, this));
      pageinfoflag = true;
    }
  if (img && pageinfoflag)
    msg_push(xhead(DDJVU_REDISPLAY, this));
}

void 
ddjvu_page_s::notify_chunk_done(const DjVuPort*, const GUTF8String &name)
{
  GMonitorLock lock(&monitor);
  if (! img) return;
  GP<ddjvu_message_p> p = new ddjvu_message_p;
  p->tmp1 = name;
  p->p.m_chunk.chunkid = (const char*)(p->tmp1);
  msg_push(xhead(DDJVU_CHUNK,this), p);
}


// ----------------------------------------
// Page queries

int
ddjvu_page_get_width(ddjvu_page_t *page)
{
  G_TRY
    {
      if (page && page->img)
        return page->img->get_width();
    }
  G_CATCH(ex)
    {
      ERROR1(page, ex);
    }
  G_ENDCATCH;
  return 0;
}

int
ddjvu_page_get_height(ddjvu_page_t *page)
{
  G_TRY
    {
      if (page && page->img)
        return page->img->get_height();
    }
  G_CATCH(ex)
    {
      ERROR1(page, ex);
    }
  G_ENDCATCH;
  return 0;
}

int
ddjvu_page_get_resolution(ddjvu_page_t *page)
{
  G_TRY
    {
      if (page && page->img)
        return page->img->get_dpi();
    }
  G_CATCH(ex)
    {
      ERROR1(page, ex);
    }
  G_ENDCATCH;
  return 0;
}

double
ddjvu_page_get_gamma(ddjvu_page_t *page)
{
  G_TRY
    {
      if (page && page->img)
        return page->img->get_gamma();
    }
  G_CATCH(ex)
    {
      ERROR1(page, ex);
    }
  G_ENDCATCH;
  return 2.2;
}

int
ddjvu_page_get_version(ddjvu_page_t *page)
{
  G_TRY
    {
      if (page && page->img)
        return page->img->get_version();
    }
  G_CATCH(ex)
    {
      ERROR1(page, ex);
    }
  G_ENDCATCH;
  return DJVUVERSION;
}

ddjvu_page_type_t
ddjvu_page_get_type(ddjvu_page_t *page)
{
  G_TRY
    {
      if (! (page && page->img))
        return DDJVU_PAGETYPE_UNKNOWN;
      else if (page->img->is_legal_bilevel())
        return DDJVU_PAGETYPE_BITONAL;
      else if (page->img->is_legal_photo())
        return DDJVU_PAGETYPE_PHOTO;
      else if (page->img->is_legal_compound())
        return DDJVU_PAGETYPE_COMPOUND;
    }
  G_CATCH(ex)
    {
      ERROR1(page, ex);
    }
  G_ENDCATCH;
  return DDJVU_PAGETYPE_UNKNOWN;
}

char *
ddjvu_page_get_short_description(ddjvu_page_t *page)
{
  G_TRY
    {
      if (page && page->img)
        {
          const char *desc = page->img->get_short_description();
          return xstr(DjVuMessageLite::LookUpUTF8(desc));
        }
    }
  G_CATCH(ex)
    {
      ERROR1(page, ex);
    }
  G_ENDCATCH;
  return 0;
}

char *
ddjvu_page_get_long_description(ddjvu_page_t *page)
{
  G_TRY
    {
      if (page && page->img)
        {
          const char *desc = page->img->get_long_description();
          return xstr(DjVuMessageLite::LookUpUTF8(desc));
        }
    }
  G_CATCH(ex)
    {
      ERROR1(page, ex);
    }
  G_ENDCATCH;
  return 0;
}


// ----------------------------------------
// Rotations

void
ddjvu_page_set_rotation(ddjvu_page_t *page,
                        ddjvu_page_rotation_t rot)
{
  G_TRY
    {
      switch(rot)
        {
        case DDJVU_ROTATE_0:
        case DDJVU_ROTATE_90:
        case DDJVU_ROTATE_180:
        case DDJVU_ROTATE_270:
          if (page && page->img && page->img->get_info())
            page->img->set_rotate((int)rot);
          break;
        default:
          G_THROW("Illegal ddjvu rotation code");
          break;
        }
    }
  G_CATCH(ex)
    {
      ERROR1(page, ex);
    }
  G_ENDCATCH;
}

ddjvu_page_rotation_t
ddjvu_page_get_rotation(ddjvu_page_t *page)
{
  ddjvu_page_rotation_t rot = DDJVU_ROTATE_0;
  G_TRY
    {
      if (page && page->img)
        rot = (ddjvu_page_rotation_t)(page->img->get_rotate() & 3);
    }
  G_CATCH(ex)
    {
      ERROR1(page, ex);
    }
  G_ENDCATCH;
  return rot;
}

ddjvu_page_rotation_t
ddjvu_page_get_initial_rotation(ddjvu_page_t *page)
{
  ddjvu_page_rotation_t rot = DDJVU_ROTATE_0;
  G_TRY
    {
      GP<DjVuInfo> info;
      if (page && page->img)
        info = page->img->get_info();
      if (info)
        rot = (ddjvu_page_rotation_t)(info->orientation & 3);
    }
  G_CATCH(ex)
    {
      ERROR1(page, ex);
    }
  G_ENDCATCH;
  return rot;
}


// ----------------------------------------
// Rectangles

static void
rect2grect(const ddjvu_rect_t *r, GRect &g)
{
  g.xmin = r->x;
  g.ymin = r->y;
  g.xmax = r->x + r->w;
  g.ymax = r->y + r->h;
}

static void
grect2rect(const GRect &g, ddjvu_rect_t *r)
{
  if (g.isempty())
    {
      r->x = r->y = 0;
      r->w = r->h = 0;
    }
  else
    {
      r->x = g.xmin;
      r->y = g.ymin;
      r->w = g.width();
      r->h = g.height();
    }
}

ddjvu_rectmapper_t *
ddjvu_rectmapper_create(ddjvu_rect_t *input, ddjvu_rect_t *output)
{
  GRect ginput, goutput;
  rect2grect(input, ginput);
  rect2grect(output, goutput);
  GRectMapper *mapper = new GRectMapper;
  if (!ginput.isempty())
    mapper->set_input(ginput);
  if (!goutput.isempty())
    mapper->set_output(goutput);
  return (ddjvu_rectmapper_t*)mapper;
}

void
ddjvu_rectmapper_modify(ddjvu_rectmapper_t *mapper,
                        int rotation, int mirrorx, int mirrory)
{
  GRectMapper *gmapper = (GRectMapper*)mapper;
  if (! gmapper) return;
  gmapper->rotate(rotation);
  if (mirrorx & 1)
    gmapper->mirrorx();
  if (mirrory & 1)
    gmapper->mirrory();
}

void 
ddjvu_rectmapper_release(ddjvu_rectmapper_t *mapper)
{
  GRectMapper *gmapper = (GRectMapper*)mapper;
  if (! gmapper) return;
  delete gmapper;
}

void 
ddjvu_map_point(ddjvu_rectmapper_t *mapper, int *x, int *y)
{
  GRectMapper *gmapper = (GRectMapper*)mapper;
  if (! gmapper) return;
  gmapper->map(*x,*y);
}

void 
ddjvu_map_rect(ddjvu_rectmapper_t *mapper, ddjvu_rect_t *rect)
{
  GRectMapper *gmapper = (GRectMapper*)mapper;
  if (! gmapper) return;
  GRect grect;
  rect2grect(rect,grect);
  gmapper->map(grect);
  grect2rect(grect,rect);
}

void 
ddjvu_unmap_point(ddjvu_rectmapper_t *mapper, int *x, int *y)
{
  GRectMapper *gmapper = (GRectMapper*)mapper;
  if (! gmapper) return;
  gmapper->unmap(*x,*y);
}

void 
ddjvu_unmap_rect(ddjvu_rectmapper_t *mapper, ddjvu_rect_t *rect)
{
  GRectMapper *gmapper = (GRectMapper*)mapper;
  if (! gmapper) return;
  GRect grect;
  rect2grect(rect,grect);
  gmapper->unmap(grect);
  grect2rect(grect,rect);
}


// ----------------------------------------
// Render

struct DJVUNS ddjvu_format_s
{
  ddjvu_format_style_t style;
  uint32_t rgb[3][256];
  uint32_t palette[6*6*6];
  uint32_t xorval;
  double gamma;
  GPixel white;
  char ditherbits;
  bool rtoptobottom;
  bool ytoptobottom;
};

static ddjvu_format_t *
fmt_error(ddjvu_format_t *fmt)
{
  delete fmt;
  return 0;
}

ddjvu_format_t *
ddjvu_format_create(ddjvu_format_style_t style,
                    int nargs, unsigned int *args)
{
  ddjvu_format_t *fmt = new ddjvu_format_s;
  memset(fmt, 0, sizeof(ddjvu_format_t));
  fmt->style = style;  
  fmt->rtoptobottom = false;
  fmt->ytoptobottom = false;
  fmt->gamma = 2.2;
  fmt->white = GPixel::WHITE;
  // Ditherbits
  fmt->ditherbits = 32;
  if (style==DDJVU_FORMAT_RGBMASK16)
    fmt->ditherbits = 16;
  else if (style==DDJVU_FORMAT_PALETTE8)
    fmt->ditherbits = 8;
  else if (style==DDJVU_FORMAT_MSBTOLSB || style==DDJVU_FORMAT_LSBTOMSB)
    fmt->ditherbits = 1;
  // Args
  switch(style)
    {
    case DDJVU_FORMAT_RGBMASK16:
    case DDJVU_FORMAT_RGBMASK32: 
      {
        if (sizeof(uint16_t)!=2 || sizeof(uint32_t)!=4)
          return fmt_error(fmt);
        if (!args || nargs<3 || nargs>4)
          return fmt_error(fmt);
        { // extra nesting for windows
          for (int j=0; j<3; j++)
          {
            int shift = 0;
            uint32_t mask = args[j];
            for (shift=0; shift<32 && !(mask & 1); shift++)
              mask >>= 1;
            if ((shift>=32) || (mask&(mask+1)))
              return fmt_error(fmt);
            for (int i=0; i<256; i++)
              fmt->rgb[j][i] = (mask & ((int)((i*mask+127.0)/255.0)))<<shift;
          }
        }
        if (nargs >= 4)
          fmt->xorval = args[3];
        break;
      }
    case DDJVU_FORMAT_PALETTE8:
      {
        if (nargs!=6*6*6 || !args)
          return fmt_error(fmt);
        { // extra nesting for windows
          for (int k=0; k<6*6*6; k++)
            fmt->palette[k] = args[k];
        }
        { // extra nesting for windows
          int j=0;
          for(int i=0; i<6; i++)
            for(; j < (i+1)*0x33 - 0x19 && j<256; j++)
            {
              fmt->rgb[0][j] = i * 6 * 6;
              fmt->rgb[1][j] = i * 6;
              fmt->rgb[2][j] = i;
            }
        }
        break;
      }
    case DDJVU_FORMAT_RGB24:
    case DDJVU_FORMAT_BGR24:
    case DDJVU_FORMAT_GREY8:
    case DDJVU_FORMAT_LSBTOMSB:
    case DDJVU_FORMAT_MSBTOLSB:
      if (!nargs) 
        break;
      /* FALLTHRU */
    default:
      return fmt_error(fmt);
    }
  return fmt;
}

void
ddjvu_format_set_row_order(ddjvu_format_t *format, int top_to_bottom)
{
  format->rtoptobottom = !! top_to_bottom;
}

void
ddjvu_format_set_y_direction(ddjvu_format_t *format, int top_to_bottom)
{
  format->ytoptobottom = !! top_to_bottom;
}

void
ddjvu_format_set_ditherbits(ddjvu_format_t *format, int bits)
{
  if (bits>0 && bits<=64)
    format->ditherbits = bits;
}

void
ddjvu_format_set_gamma(ddjvu_format_t *format, double gamma)
{
  if (gamma>=0.5 && gamma<=5.0)
    format->gamma = gamma;
}

void
ddjvu_format_set_white(ddjvu_format_t *format, 
                       unsigned char b, unsigned char g, unsigned char r)
{
  format->white.b = b;
  format->white.g = g;
  format->white.r = r;
}

void
ddjvu_format_release(ddjvu_format_t *format)
{
  delete format;
}

static void
fmt_convert_row(const GPixel *p, int w, 
                const ddjvu_format_t *fmt, char *buf)
{
  const uint32_t (*r)[256] = fmt->rgb;
  const uint32_t xorval = fmt->xorval;
  switch(fmt->style)
    {
    case DDJVU_FORMAT_BGR24:    /* truecolor 24 bits in BGR order */
      {
        memcpy(buf, (const char*)p, 3*w);
        break;
      }
    case DDJVU_FORMAT_RGB24:    /* truecolor 24 bits in RGB order */
      { 
        while (--w >= 0) { 
          buf[0]=p->r; buf[1]=p->g; buf[2]=p->b; 
          buf+=3; p+=1; 
        }
        break;
      }
    case DDJVU_FORMAT_RGBMASK16: /* truecolor 16 bits with masks */
      {
        uint16_t *b = (uint16_t*)buf;
        while (--w >= 0) {
          b[0]=(r[0][p->r]|r[1][p->g]|r[2][p->b])^xorval; 
          b+=1; p+=1; 
        }
        break;
      }
    case DDJVU_FORMAT_RGBMASK32: /* truecolor 32 bits with masks */
      {
        uint32_t *b = (uint32_t*)buf;
        while (--w >= 0) {
          b[0]=(r[0][p->r]|r[1][p->g]|r[2][p->b])^xorval; 
          b+=1; p+=1; 
        }
        break;
      }
    case DDJVU_FORMAT_GREY8:    /* greylevel 8 bits */
      {
        while (--w >= 0) { 
          buf[0]=(5*p->r + 9*p->g + 2*p->b)>>4; 
          buf+=1; p+=1; 
        }
        break;
      }
    case DDJVU_FORMAT_PALETTE8: /* paletized 8 bits (6x6x6 color cube) */
      {
        const uint32_t *u = fmt->palette;
        while (--w >= 0) {
          buf[0] = u[r[0][p->r]+r[1][p->g]+r[2][p->b]]; 
          buf+=1; p+=1; 
        }
        break;
      }
    case DDJVU_FORMAT_MSBTOLSB: /* packed bits, msb on the left */
      {
        int t = (5*fmt->white.r + 9*fmt->white.g + 2*fmt->white.b + 16);
        t = t * 0xc / 0x10;
        unsigned char s=0, m=0x80;
        while (--w >= 0) {
          if ( 5*p->r + 9*p->g + 2*p->b < t ) { s |= m; }
          if (! (m >>= 1)) { *buf++ = s; s=0; m=0x80; }
          p += 1;
        }
        if (m < 0x80) { *buf++ = s; }
        break;
      }
    case DDJVU_FORMAT_LSBTOMSB: /* packed bits, lsb on the left */
      {
        int t = 5*fmt->white.r + 9*fmt->white.g + 2*fmt->white.b + 16;
        t = t * 0xc / 0x10;
        unsigned char s=0, m=0x1;
        while (--w >= 0) {
          if ( 5*p->r + 9*p->g + 2*p->b < t ) { s |= m; }
          if (! (m <<= 1)) { *buf++ = s; s=0; m=0x1; }
          p += 1;
        }
        if (m > 0x1) { *buf++ = s; }
        break;
      }
    }
}

static void
fmt_convert(GPixmap *pm, const ddjvu_format_t *fmt, char *buffer, int rowsize)
{
  int w = pm->columns();
  int h = pm->rows();
  // Loop on rows
  if (fmt->rtoptobottom)
    {
      for(int r=h-1; r>=0; r--, buffer+=rowsize)
        fmt_convert_row((*pm)[r], w, fmt, buffer);
    }
  else
    {
      for(int r=0; r<h; r++, buffer+=rowsize)
        fmt_convert_row((*pm)[r], w, fmt, buffer);
    }
}

static void
fmt_convert_row(unsigned char *p, unsigned char g[256][4], int w, 
                const ddjvu_format_t *fmt, char *buf)
{
  const uint32_t (*r)[256] = fmt->rgb;
  const uint32_t xorval = fmt->xorval;
  switch(fmt->style)
    {
    case DDJVU_FORMAT_BGR24:    /* truecolor 24 bits in BGR order */
      { 
        while (--w >= 0) { 
          buf[0]=g[*p][0];
          buf[1]=g[*p][1];
          buf[2]=g[*p][2];
          buf+=3; p+=1; 
        }
        break;
      }
    case DDJVU_FORMAT_RGB24:    /* truecolor 24 bits in RGB order */
      { 
        while (--w >= 0) { 
          buf[0]=g[*p][2];
          buf[1]=g[*p][1];
          buf[2]=g[*p][0];
          buf+=3; p+=1; 
        }
        break;
      }
    case DDJVU_FORMAT_RGBMASK16: /* truecolor 16 bits with masks */
      {
        uint16_t *b = (uint16_t*)buf;
        while (--w >= 0) {
          unsigned char x = *p;
          b[0]=(r[0][g[x][2]]|r[1][g[x][1]]|r[2][g[x][0]])^xorval; 
          b+=1; p+=1; 
        }
        break;
      }
    case DDJVU_FORMAT_RGBMASK32: /* truecolor 32 bits with masks */
      {
        uint32_t *b = (uint32_t*)buf;
        while (--w >= 0) {
          unsigned char x = *p;
          b[0]=(r[0][g[x][2]]|r[1][g[x][1]]|r[2][g[x][0]])^xorval; 
          b+=1; p+=1; 
        }
        break;
      }
    case DDJVU_FORMAT_GREY8:    /* greylevel 8 bits */
      {
        while (--w >= 0) { 
          buf[0]=g[*p][3];
          buf+=1; p+=1; 
        }
        break;
      }
    case DDJVU_FORMAT_PALETTE8: /* paletized 8 bits (6x6x6 color cube) */
      {
        const uint32_t *u = fmt->palette;
        while (--w >= 0) {
          unsigned char x = *p;
          buf[0] = u[r[0][g[x][0]]+r[1][g[x][1]]+r[2][g[x][2]]]; 
          buf+=1; p+=1; 
        }
        break;
      }
    case DDJVU_FORMAT_MSBTOLSB: /* packed bits, msb on the left */
      {
        int t = 5*fmt->white.r + 9*fmt->white.g + 2*fmt->white.b + 16;
        t = t * 0xc / 0x100;
        unsigned char s=0, m=0x80;
        while (--w >= 0) {
          unsigned char x = *p;
          if ( g[x][3] < t ) { s |= m; }
          if (! (m >>= 1)) { *buf++ = s; s=0; m=0x80; }
          p += 1;
        }
        if (m < 0x80) { *buf++ = s; }
        break;
      }
    case DDJVU_FORMAT_LSBTOMSB: /* packed bits, lsb on the left */
      {
        int t = 5*fmt->white.r + 9*fmt->white.g + 2*fmt->white.b + 16;
        t = t * 0xc / 0x100;
        unsigned char s=0, m=0x1;
        while (--w >= 0) {
          unsigned char x = *p;
          if ( g[x][3] < t ) { s |= m; }
          if (! (m <<= 1)) { *buf++ = s; s=0; m=0x1; }
          p += 1;
        }
        if (m > 0x1) { *buf++ = s; }
        break;
      }
    }
}

static void
fmt_convert(GBitmap *bm, const ddjvu_format_t *fmt, char *buffer, int rowsize)
{
  int w = bm->columns();
  int h = bm->rows();
  int m = bm->get_grays();
  // Gray levels
  int i;
  unsigned char g[256][4];
  const GPixel &wh = fmt->white;
  for (i=0; i<m; i++)
    {
      g[i][0] = wh.b - ( i * wh.b + (m - 1)/2 ) / (m - 1);
      g[i][1] = wh.g - ( i * wh.g + (m - 1)/2 ) / (m - 1);
      g[i][2] = wh.r - ( i * wh.r + (m - 1)/2 ) / (m - 1);
      g[i][3] = (5*g[i][2] + 9*g[i][1] + 2*g[i][0])>>4; 
    }
  for (i=m; i<256; i++)
    g[i][0] = g[i][1] = g[i][2] = g[i][3] = 0;
  
  // Loop on rows
  if (fmt->rtoptobottom)
    {
      for(int r=h-1; r>=0; r--, buffer+=rowsize)
        fmt_convert_row((*bm)[r], g, w, fmt, buffer);
    }
  else
    {
      for(int r=0; r<h; r++, buffer+=rowsize)
        fmt_convert_row((*bm)[r], g, w, fmt, buffer);
    }
}

static void
fmt_dither(GPixmap *pm, const ddjvu_format_t *fmt, int x, int y)
{
  if (fmt->ditherbits < 8)
    return;
  else if (fmt->ditherbits < 15)
    pm->ordered_666_dither(x, y);
  else if (fmt->ditherbits < 24)
    pm->ordered_32k_dither(x, y);
}


// ----------------------------------------

int
ddjvu_page_render(ddjvu_page_t *page,
                  const ddjvu_render_mode_t mode,
                  const ddjvu_rect_t *pagerect,
                  const ddjvu_rect_t *renderrect,
                  const ddjvu_format_t *format,
                  unsigned long rowsize,
                  char *imagebuffer )
{
  G_TRY
    {
      GP<GPixmap> pm;
      GP<GBitmap> bm;
      GRect prect, rrect;
      rect2grect(pagerect, prect);
      rect2grect(renderrect, rrect);
      if (format && format->ytoptobottom)
        {
          prect.ymin = renderrect->y + renderrect->h;
          prect.ymax = prect.ymin + pagerect->h;
          rrect.ymin = pagerect->y + pagerect->h;
          rrect.ymax = rrect.ymin + renderrect->h;
        }

      DjVuImage *img = page->img;
      if (img) 
        {
          switch (mode)
            {
            case DDJVU_RENDER_COLOR:
              pm = img->get_pixmap(rrect,prect, format->gamma,format->white);
              if (! pm) 
                bm = img->get_bitmap(rrect,prect);
              break;
            case DDJVU_RENDER_BLACK:
              bm = img->get_bitmap(rrect,prect);
              if (! bm)
                pm = img->get_pixmap(rrect,prect, format->gamma,format->white);
              break;
            case DDJVU_RENDER_MASKONLY:
              bm = img->get_bitmap(rrect,prect);
              break;
            case DDJVU_RENDER_COLORONLY:
              pm = img->get_pixmap(rrect,prect, format->gamma,format->white);
              break;
            case DDJVU_RENDER_BACKGROUND:
              pm = img->get_bg_pixmap(rrect,prect, format->gamma,format->white);
              break;
            case DDJVU_RENDER_FOREGROUND:
              pm = img->get_fg_pixmap(rrect,prect, format->gamma,format->white);
              if (! pm) 
                bm = img->get_bitmap(rrect,prect);
              break;
            }
        }
      if (pm)
        {
          int dx = rrect.xmin - prect.xmin;
          int dy = rrect.ymin - prect.xmin;
          fmt_dither(pm, format, dx, dy);
          fmt_convert(pm, format, imagebuffer, rowsize);
          return 2;
        }
      else if (bm)
        {
          fmt_convert(bm, format, imagebuffer, rowsize);
          return 1;
        }
    }
  G_CATCH(ex)
    {
      ERROR1(page, ex);
    }
  G_ENDCATCH;
  return 0;
}


// ----------------------------------------
// Thumbnails

void
ddjvu_thumbnail_p::callback(void *cldata)
{
  ddjvu_thumbnail_p *thumb = (ddjvu_thumbnail_p*)cldata;
  if (thumb->document)
    {
      GMonitorLock lock(&thumb->document->monitor);
      if (thumb->pool && thumb->pool->is_eof())
        {
          GP<DataPool> pool = thumb->pool;
          int size = pool->get_size();
          thumb->pool = 0;
          G_TRY
            {
              thumb->data.resize(0,size-1);
              pool->get_data( (void*)(char*)thumb->data, 0, size);
            }
          G_CATCH_ALL
            {
              thumb->data.empty();
            }
          G_ENDCATCH;
          if (thumb->document->doc)
            {
              GP<ddjvu_message_p> p = new ddjvu_message_p;
              p->p.m_thumbnail.pagenum = thumb->pagenum;
              msg_push(xhead(DDJVU_THUMBNAIL, thumb->document), p);
            } 
        }
    }
}

ddjvu_status_t
ddjvu_thumbnail_status(ddjvu_document_t *document, int pagenum, int start)
{
  G_TRY
    {
      GP<ddjvu_thumbnail_p> thumb;
      DjVuDocument* doc = document->doc;
      if (doc)
        {
          GMonitorLock lock(&document->monitor);
          GPosition p = document->thumbnails.contains(pagenum);
          if (p)
            thumb = document->thumbnails[p];
        }
      if (!thumb && doc)
        {
          GP<DataPool> pool = doc->get_thumbnail(pagenum, !start);
          if (pool)
            {
              GMonitorLock lock(&document->monitor);
              thumb = new ddjvu_thumbnail_p;
              thumb->document = document;
              thumb->pagenum = pagenum;
              thumb->pool = pool;
              document->thumbnails[pagenum] = thumb;
            }
          if (thumb)
            pool->add_trigger(-1, ddjvu_thumbnail_p::callback, 
                              (void*)(ddjvu_thumbnail_p*)thumb);
        } 
      if (! thumb)
        return DDJVU_JOB_NOTSTARTED;        
      else if (thumb->pool)
        return DDJVU_JOB_STARTED;
      else if (thumb->data.size() > 0)
        return DDJVU_JOB_OK;
    }
  G_CATCH(ex)
    {
      ERROR1(document, ex);
    }
  G_ENDCATCH;
  return DDJVU_JOB_FAILED;
}
 
int
ddjvu_thumbnail_render(ddjvu_document_t *document, int pagenum, 
                       int *wptr, int *hptr,
                       const ddjvu_format_t *format,
                       unsigned long rowsize,
                       char *imagebuffer)
{
  G_TRY
    {
      GP<ddjvu_thumbnail_p> thumb;
      ddjvu_status_t status = ddjvu_thumbnail_status(document,pagenum,FALSE);
      if (status == DDJVU_JOB_OK)
        {
          GMonitorLock lock(&document->monitor);
          thumb = document->thumbnails[pagenum];
        }
      if (! (thumb && wptr && hptr))
        return FALSE;
      if (! (thumb->data.size() > 0))
        return FALSE;
      /* Decode wavelet data */
      int size = thumb->data.size();
      char *data = (char*)thumb->data;
      GP<IW44Image> iw = IW44Image::create_decode();
      iw->decode_chunk(ByteStream::create_static((void*)data, size));
      int w = iw->get_width();
      int h = iw->get_height();
      /* Restore aspect ratio */
      double dw = (double)w / *wptr;
      double dh = (double)h / *hptr;
      if (dw > dh) 
        *hptr = (int)(h / dw);
      else
        *wptr = (int)(w / dh);
      if (! imagebuffer)
        return TRUE;
      /* Render and scale image */
      GP<GPixmap> pm = iw->get_pixmap();
      double thumbgamma = document->doc->get_thumbnails_gamma();
      pm->color_correct(format->gamma/thumbgamma, format->white);
      GP<GPixmapScaler> scaler = GPixmapScaler::create(w, h, *wptr, *hptr);
      GP<GPixmap> scaledpm = GPixmap::create();
      GRect scaledrect(0, 0, *wptr, *hptr);
      scaler->scale(GRect(0, 0, w, h), *pm, scaledrect, *scaledpm);
      /* Convert */
      fmt_dither(scaledpm, format, 0, 0);
      fmt_convert(scaledpm, format, imagebuffer, rowsize);
      return TRUE;
    }
  G_CATCH(ex)
    {
      ERROR1(document, ex);
    }
  G_ENDCATCH;
  return FALSE;
}


// ----------------------------------------
// Threaded jobs

struct DJVUNS ddjvu_runnablejob_s : public ddjvu_job_s
{
  bool mystop;
  int  myprogress;
  ddjvu_status_t mystatus;
  // methods
  ddjvu_runnablejob_s();
  ddjvu_status_t start();
  void progress(int p);
  // thread function
  virtual ddjvu_status_t run() = 0;
  // virtual port functions:
  virtual bool inherits(const GUTF8String&) const;
  virtual ddjvu_status_t status();
  virtual void stop();
private:
  static void cbstart(void*);
};

ddjvu_runnablejob_s::ddjvu_runnablejob_s()
  : mystop(false), myprogress(-1),
    mystatus(DDJVU_JOB_NOTSTARTED) 
{
}

void 
ddjvu_runnablejob_s::progress(int x)
{
  if ((mystatus>=DDJVU_JOB_OK) || (x>myprogress && x<100))
    {
      GMonitorLock lock(&monitor);
      GP<ddjvu_message_p> p = new ddjvu_message_p;
      p->p.m_progress.status = mystatus;
      p->p.m_progress.percent = myprogress = x;
      msg_push(xhead(DDJVU_PROGRESS,this),p);
    }
}

ddjvu_status_t
ddjvu_runnablejob_s::start()
{
  GMonitorLock lock(&monitor);
  if (mystatus==DDJVU_JOB_NOTSTARTED && myctx)
    {
      GThread thr;
      thr.create(cbstart, (void*)this);
      monitor.wait();
    }
  return mystatus;
}

void
ddjvu_runnablejob_s::cbstart(void *arg)
{
  GP<ddjvu_runnablejob_s> self = (ddjvu_runnablejob_s*)arg;
  {
    GMonitorLock lock(&self->monitor);
    self->mystatus = DDJVU_JOB_STARTED;
    self->monitor.signal();
  }
  ddjvu_status_t r;
  G_TRY
    {
      G_TRY
        {
          self->progress(0);
          r = self->run();
        }
      G_CATCH(ex)
        {
          ERROR1(self, ex);
          G_RETHROW;
        }
      G_ENDCATCH;
    }
  G_CATCH_ALL
    {
      r = DDJVU_JOB_FAILED;
      if (self && self->mystop)
        r = DDJVU_JOB_STOPPED;
    }
  G_ENDCATCH;
  {
    GMonitorLock lock(&self->monitor);
    self->mystatus = r;
  }
  if (self && self->mystatus> DDJVU_JOB_OK)
    self->progress(self->myprogress);
  else
    self->progress(100);
}

bool 
ddjvu_runnablejob_s::inherits(const GUTF8String &classname) const
{
  return (classname == "ddjvu_runnablejob_s") 
    || ddjvu_job_s::inherits(classname);
}

ddjvu_status_t 
ddjvu_runnablejob_s::status()
{
  return mystatus;
}

void
ddjvu_runnablejob_s::stop()
{
  mystop = true;
}


// ----------------------------------------
// Printing

struct DJVUNS ddjvu_printjob_s : public ddjvu_runnablejob_s
{
  DjVuToPS printer;
  GUTF8String pages;
  GP<ByteStream> obs;
  virtual ddjvu_status_t run();
  // virtual port functions:
  virtual bool inherits(const GUTF8String&) const;
  // progress
  static void cbrefresh(void*);
  static void cbprogress(double, void*);
  static void cbinfo(int, int, int, DjVuToPS::Stage, void*);
  double progress_low;
  double progress_high;
};

bool 
ddjvu_printjob_s::inherits(const GUTF8String &classname) const
{
  return (classname == "ddjvu_printjob_s") 
    || ddjvu_runnablejob_s::inherits(classname);
}

ddjvu_status_t 
ddjvu_printjob_s::run()
{
  mydoc->doc->wait_for_complete_init();
  progress_low = 0;
  progress_high = 1;
  printer.set_refresh_cb(cbrefresh, (void*)this);
  printer.set_dec_progress_cb(cbprogress, (void*)this);
  printer.set_prn_progress_cb(cbprogress, (void*)this);
  printer.set_info_cb(cbinfo, (void*)this);
  printer.print(*obs, mydoc->doc, pages);
  return DDJVU_JOB_OK;
}

void
ddjvu_printjob_s::cbrefresh(void *data)
{
  ddjvu_printjob_s *self = (ddjvu_printjob_s*)data;
  if (self->mystop)
    {
      msg_push(xhead(DDJVU_INFO,self), msg_prep_info("Print job stopped"));
      G_THROW(DataPool::Stop);
    }
}

void
ddjvu_printjob_s::cbprogress(double done, void *data)
{
  ddjvu_printjob_s *self = (ddjvu_printjob_s*)data;
  double &low = self->progress_low;
  double &high = self->progress_high;
  double progress = low;
  if (done >= 1)
    progress = high;
  else if (done >= 0)
    progress = low + done * (high-low);
  self->progress((int)(progress * 100));
  ddjvu_printjob_s::cbrefresh(data);
}

void
ddjvu_printjob_s::cbinfo(int pnum, int pcnt, int ptot,
                         DjVuToPS::Stage stage, void *data)
{
  ddjvu_printjob_s *self = (ddjvu_printjob_s*)data;
  double &low = self->progress_low;
  double &high = self->progress_high;
  low = 0;
  high = 1;
  if (ptot > 0) 
    {
      double step = 1.0 / (double)ptot;
      low = (double)pcnt * step;
      if (stage != DjVuToPS::DECODING) 
	low += step / 2.0;
      high = low  + step / 2.0;
    }
  if (low < 0)
    low = 0;
  if (low > 1) 
    low = 1;
  if (high < low) 
    high = low;
  if (high > 1)
    high = 1;
  self->progress((int)(low * 100));
  ddjvu_printjob_s::cbrefresh(data);
}

static void
complain(GUTF8String opt, const char *msg)
{
  GUTF8String message;
  if (opt.length() > 0)
    message = "Parsing \"" + opt + "\": " + msg;
  else
    message = msg;
  G_RETHROW(GException((const char*)message));
}

ddjvu_job_t *
ddjvu_document_print(ddjvu_document_t *document, FILE *output,
                     int optc, const char * const * optv)
{
  ddjvu_printjob_s *job = 0;
  G_TRY
    {
      job = new ddjvu_printjob_s;
      ref(job);
      job->myctx = document->myctx;
      job->mydoc = document;
      // parse options (see djvups(1))
      DjVuToPS::Options &options = job->printer.options;
      GUTF8String &pages = job->pages;
      while (optc>0)
        {
          // normalize
          GNativeString narg(optv[0]);
          GUTF8String uarg = narg;
          const char *s1 = (const char*)narg;
          if (s1[0] == '-') s1++;
          if (s1[0] == '-') s1++;
          // separate arguments
          const char *s2 = s1;
          while (*s2 && *s2 != '=') s2++;
          GUTF8String s( s1, s2-s1 );
          GUTF8String arg( s2[0] && s2[1] ? s2+1 : "" );
          // rumble!
          if (s == "page" || s == "pages")
            {
              if (pages.length())
                pages = pages + ",";
              pages = pages + arg;
            }
          else if (s == "format")
            {
              if (arg == "ps")
                options.set_format(DjVuToPS::Options::PS);
              else if (arg == "eps")
                options.set_format(DjVuToPS::Options::EPS);
              else
                complain(uarg,"Invalid format. Use \"ps\" or \"eps\".");
            }
          else if (s == "level")
            {
              int endpos;
              int lvl = arg.toLong(0, endpos);
              if (endpos != (int)arg.length() || lvl < 1 || lvl > 4)
                complain(uarg,"Invalid Postscript language level.");
              options.set_level(lvl);
            }
          else if (s == "orient" || s == "orientation")
            {
              if (arg == "a" || arg == "auto" )
                options.set_orientation(DjVuToPS::Options::AUTO);
              else if (arg == "l" || arg == "landscape" )
                options.set_orientation(DjVuToPS::Options::LANDSCAPE);
              else if (arg == "p" || arg == "portrait" )
                options.set_orientation(DjVuToPS::Options::PORTRAIT);
              else
                complain(uarg,"Invalid orientation. Use \"auto\", "
                         "\"landscape\" or \"portrait\".");
            }
          else if (s == "mode")
            {
              if (arg == "c" || arg == "color" )
                options.set_mode(DjVuToPS::Options::COLOR);
              else if (arg == "black" || arg == "bw")
                options.set_mode(DjVuToPS::Options::BW);
              else if (arg == "fore" || arg == "foreground")
                options.set_mode(DjVuToPS::Options::FORE);
              else if (arg == "back" || arg == "background" )
                options.set_mode(DjVuToPS::Options::BACK);
              else
                complain(uarg,"Invalid mode. Use \"color\", \"bw\", "
                         "\"foreground\", or \"background\".");
            }
          else if (s == "zoom")
            {
              if (arg == "auto" || arg == "fit" || arg == "fit_page")
                options.set_zoom(0);
              else if (arg == "1to1" || arg == "onetoone")
                options.set_zoom(100);                
              else 
                {
                  int endpos;
                  int z = arg.toLong(0,endpos);
                  if (endpos != (int)arg.length() || z < 25 || z > 2400)
                    complain(uarg,"Invalid zoom factor.");
                  options.set_zoom(z);
                }
            }
          else if (s == "color")
            {
              if (arg == "yes" || arg == "")
                options.set_color(true);
              else if (arg == "no")
                options.set_color(false);
              else
                complain(uarg,"Invalid argument. Use \"yes\" or \"no\".");
            }
          else if (s == "gray" || s == "grayscale")
            {
              if (arg.length())
                complain(uarg,"No argument was expected.");
              options.set_color(false);
            }
          else if (s == "srgb" || s == "colormatch")
            {
              if (arg == "yes" || arg == "")
                options.set_sRGB(true);
              else if (arg == "no")
                options.set_sRGB(false);
              else
                complain(uarg,"Invalid argument. Use \"yes\" or \"no\".");
            }
          else if (s == "gamma")
            {
              int endpos;
              double g = arg.toDouble(0,endpos);
              if (endpos != (int)arg.length() || g < 0.3 || g > 5.0)
                complain(uarg,"Invalid gamma factor. "
                              "Use a number in range 0.3 ... 5.0.");
              options.set_gamma(g);
            }
          else if (s == "copies")
            {
              int endpos;
              int n = arg.toLong(0, endpos);
              if (endpos != (int)arg.length() || n < 1 || n > 999999)
                complain(uarg,"Invalid number of copies.");
              options.set_copies(n);
            }
          else if (s == "frame")
            {
              if (arg == "yes" || arg == "")
                options.set_frame(true);
              else if (arg == "no")
                options.set_frame(false);
              else
                complain(uarg,"Invalid argument. Use \"yes\" or \"no\".");
            }
          else if (s == "cropmarks")
            {
              if (arg == "yes" || arg == "")
                options.set_cropmarks(true);
              else if (arg == "no")
                options.set_cropmarks(false);
              else
                complain(uarg,"Invalid argument. Use \"yes\" or \"no\".");
            }
          else if (s == "text")
            {
              if (arg == "yes" || arg == "")
                options.set_text(true);
              else if (arg == "no")
                options.set_text(false);
              else
                complain(uarg,"Invalid argument. Use \"yes\" or \"no\".");
            }
          else if (s == "booklet")
            {
              if (arg == "no")
                options.set_bookletmode(DjVuToPS::Options::OFF);
              else if (arg == "recto")
                options.set_bookletmode(DjVuToPS::Options::RECTO);
              else if (arg == "verso")
                options.set_bookletmode(DjVuToPS::Options::VERSO);
              else if (arg == "rectoverso" || arg=="yes" || arg=="")
                options.set_bookletmode(DjVuToPS::Options::RECTOVERSO);
              else 
                complain(uarg,"Invalid argument."
                         "Use \"no\", \"yes\", \"recto\", or \"verso\".");
            }
          else if (s == "bookletmax")
            {
              int endpos;
              int n = arg.toLong(0, endpos);
              if (endpos != (int)arg.length() || n < 0 || n > 999999)
                complain(uarg,"Invalid argument.");
              options.set_bookletmax(n);
            }
          else if (s == "bookletalign")
            {
              int endpos;
              int n = arg.toLong(0, endpos);
              if (endpos != (int)arg.length() || n < -720 || n > +720)
                complain(uarg,"Invalid argument.");
              options.set_bookletalign(n);
            }
          else if (s == "bookletfold")
            {
              int endpos = 0;
              int m = 250;
              int n = arg.toLong(0, endpos);
              if (endpos>0 && endpos<(int)arg.length() && arg[endpos]=='+')
                m = arg.toLong(endpos+1, endpos);
              if (endpos != (int)arg.length() || m<0 || m>720 || n<0 || n>9999 )
                complain(uarg,"Invalid argument.");
              options.set_bookletfold(n,m);
            }
          else
            {
              complain(uarg, "Unrecognized option.");
            }
          // Next option
          optc -= 1;
          optv += 1;
        }
      // go
      job->obs = ByteStream::create(output, "wb", false);
      job->start();
    }
  G_CATCH(ex)
    {
      if (job) 
        unref(job);
      job = 0;
      ERROR1(document, ex);
    }
  G_ENDCATCH;
  return job;
}



// ----------------------------------------
// Saving

struct DJVUNS ddjvu_savejob_s : public ddjvu_runnablejob_s
{
  GP<ByteStream> obs;
  GURL           odir;  
  GUTF8String    oname;
  GUTF8String    pages;
  GTArray<char>       comp_flags;
  GArray<GUTF8String> comp_ids;
  GPArray<DjVuFile>   comp_files;
  GMonitor monitor;
  // thread routine
  virtual ddjvu_status_t run();
  // virtual port functions:
  virtual bool inherits(const GUTF8String&) const;
  virtual void notify_file_flags_changed(const DjVuFile*, long, long);
  // helpers
  bool parse_pagespec(const char *s, int npages, bool *flags);
  void mark_included_files(DjVuFile *file);
};

bool 
ddjvu_savejob_s::inherits(const GUTF8String &classname) const
{
  return (classname == "ddjvu_savejob_s") 
    || ddjvu_runnablejob_s::inherits(classname);
}

void
ddjvu_savejob_s::notify_file_flags_changed(const DjVuFile *file, 
                                           long mask, long)
{
  if (mask & (DjVuFile::ALL_DATA_PRESENT | DjVuFile::DATA_PRESENT |
              DjVuFile::DECODE_FAILED | DjVuFile::DECODE_STOPPED |
              DjVuFile::STOPPED ))
    {
      GMonitorLock lock(&monitor);
      monitor.signal();
    }
}

bool
ddjvu_savejob_s::parse_pagespec(const char *s, int npages, bool *flags)
{
  int spec = 0;
  int both = 1;
  int start_page = 1;
  int end_page = npages;
  int pageno;
  char *p = (char*)s;
  while (*p)
    {
      spec = 0;
      while (*p==' ')
        p += 1;
      if (! *p)
        break;
      if (*p>='0' && *p<='9') {
        end_page = strtol(p, &p, 10);
        spec = 1;
      } else if (*p=='$') {
        spec = 1;
        end_page = npages;
        p += 1;
      } else if (both) {
        end_page = 1;
      } else {
        end_page = npages;
      }
      while (*p==' ')
        p += 1;
      if (both) {
        start_page = end_page;
        if (*p == '-') {
          p += 1;
          both = 0;
          continue;
        }
      }
      both = 1;
      while (*p==' ')
        p += 1;
      if (*p && *p != ',')
        return false;
      if (*p == ',')
        p += 1;
      if (! spec)
        return false;
      if (end_page <= 0)
        end_page = 1;
      if (start_page <= 0)
        start_page = 1;
      if (end_page > npages)
        end_page = npages;
      if (start_page > npages)
        start_page = npages;
      if (start_page <= end_page)
        for(pageno=start_page; pageno<=end_page; pageno++)
          flags[pageno-1] = true;
      else
        for(pageno=start_page; pageno>=end_page; pageno--)
          flags[pageno-1] = true;
    }
  if (!spec)
    return false;
  return true;
}

void 
ddjvu_savejob_s::mark_included_files(DjVuFile *file)
{
  GP<DataPool> pool = file->get_init_data_pool();
  GP<ByteStream> str(pool->get_stream());
  GP<IFFByteStream> iff(IFFByteStream::create(str));
  GUTF8String chkid;
  if (!iff->get_chunk(chkid)) 
    return;
  while (iff->get_chunk(chkid))
    {
      if (chkid == "INCL")
        {
          GP<ByteStream> incl = iff->get_bytestream();
          GUTF8String fileid;
          char buffer[1024];
          int length;
          while((length=incl->read(buffer, 1024)))
            fileid += GUTF8String(buffer, length);
          for (int i=0; i<comp_ids.size(); i++)
            if (fileid == comp_ids[i] && !comp_flags[i])
              comp_flags[i] = 1;
        }
      iff->close_chunk();
    }
  iff->close_chunk();
  pool->clear_stream();
}

ddjvu_status_t 
ddjvu_savejob_s::run()
{
  DjVuDocument *doc = mydoc->doc;
  doc->wait_for_complete_init();

  // Determine which pages to save
  int npages = doc->get_pages_num();
  GTArray<bool> page_flags(0, npages-1);
  if (!pages)
    {
      for (int pageno=0; pageno<npages; pageno++)
        page_flags[pageno] = true;
    }
  else
    {
      const char *s = pages;
      while (*s && *s!='=')
        s += 1;
      for (int pageno=0; pageno<npages; pageno++)
        page_flags[pageno] = false;
      if ((*s != '=') || !parse_pagespec(s+1, npages, (bool*)page_flags))
        complain(pages,"Illegal page specification");
      if (doc->get_doc_type()==DjVuDocument::OLD_BUNDLED ||
          doc->get_doc_type()==DjVuDocument::OLD_INDEXED )
        complain(pages,"Saving subsets of obsolete formats is not supported");
    }
  
  // Determine which component files to save
  int ncomps;
  if (doc->get_doc_type()==DjVuDocument::BUNDLED ||
      doc->get_doc_type()==DjVuDocument::INDIRECT)
    {
      GP<DjVmDir> dir = doc->get_djvm_dir();
      ncomps = dir->get_files_num();
      comp_ids.resize(ncomps - 1);
      comp_flags.resize(ncomps - 1);
      comp_files.resize(ncomps - 1);
      int pageno = 0;
      GPList<DjVmDir::File> flist = dir->get_files_list();
      GPosition pos=flist;
      for (int comp=0; comp<ncomps; ++pos, ++comp)
        {
          DjVmDir::File *file = flist[pos];
          comp_ids[comp] = file->get_load_name();
          comp_flags[comp] = 0;
          if (file->is_page() && page_flags[pageno++])
            comp_flags[comp] = 1;
        }
    }
  else
    {
      ncomps = npages;
      comp_flags.resize(ncomps - 1);
      comp_files.resize(ncomps - 1);
      for (int comp=0; comp<ncomps; ++comp)
        comp_flags[comp] = page_flags[comp];
    }
  
  // Download
  get_portcaster()->add_route(doc, this);
  while (!mystop)
    {
      int comp;
      int wanted = 0;
      int loaded = 0;
      int asked = 0;
      for (comp=0; comp<ncomps; comp++)
        {
          int flags = comp_flags[comp];
          if (flags > 2)
            loaded += 1;
          else if (flags < 2)
            continue;
          else if (!comp_files[comp]->is_data_present())
            asked += 1;
          else 
            {
              comp_flags[comp] += 1;
              mark_included_files(comp_files[comp]);
            } 
        }
      for (comp=0; comp<ncomps; comp++)
        if (comp_flags[comp] > 0)
          wanted += 1;
      progress(loaded * 100 / wanted);
      if (wanted == loaded)
        break;
      for (comp=0; comp<ncomps && asked < 2; comp++)
        if (comp_flags[comp] == 1)
          {
            if (comp_ids.size() > 0)
              comp_files[comp] = doc->get_djvu_file(comp_ids[comp]);
            else
              comp_files[comp] = doc->get_djvu_file(comp);
            comp_flags[comp] += 1;
            if (!comp_files[comp]->is_data_present())
              asked += 1;
          }
      GMonitorLock lock(&monitor);
      for (comp=0; comp<ncomps; comp++)
        if (comp_flags[comp] == 2)
          if (! comp_files[comp]->is_data_present())
            {
              monitor.wait();
              break;
            }
    }
  if (mystop)
    G_THROW(DataPool::Stop);
  // Saving!
  GP<DjVmDoc> djvm;
  if (! pages)
    {
      djvm = doc->get_djvm_doc();
    }
  else
    {
      djvm = DjVmDoc::create();
      GP<DjVmDir> dir = doc->get_djvm_dir();
      GPList<DjVmDir::File> flist = dir->get_files_list();
      GPosition pos=flist;
      int pageno = 0;
      for (int comp=0; comp<ncomps; ++pos, ++comp)
        {
          if (flist[pos]->is_page())
            pageno += 1;
          if (comp_flags[comp])
            {
              GP<DjVmDir::File> f = new DjVmDir::File(*flist[pos]);
              if (f->is_page() && f->get_save_name()==f->get_title())
                f->set_title(GUTF8String(pageno));
              GP<DjVuFile> file = comp_files[comp];
              GP<DataPool> data = file->get_init_data_pool();
              djvm->insert_file(f, data);
            }
        }
    }
  if (obs)
    djvm->write(obs);
  else if (odir.is_valid() && oname.length() > 0)
    djvm->expand(odir, oname);
  return DDJVU_JOB_OK;
}


ddjvu_job_t *
ddjvu_document_save(ddjvu_document_t *document, FILE *output, 
                    int optc, const char * const * optv)
{
  ddjvu_savejob_s *job = 0;
  G_TRY
    {
      job = new ddjvu_savejob_s;
      ref(job);
      job->myctx = document->myctx;
      job->mydoc = document;
      bool indirect = false;
      // parse options
      while (optc>0)
        {
          GNativeString narg(optv[0]);
          GUTF8String uarg = narg;
          const char *s1 = (const char*)narg;
          if (s1[0] == '-') s1++;
          if (s1[0] == '-') s1++;
          // separate arguments
          if (!strncmp(s1, "page=", 5) ||
              !strncmp(s1, "pages=", 6) )
            {
              if (job->pages.length())
                complain(uarg,"multiple page specifications");
              job->pages = uarg;
            }
          else if (!strncmp(s1, "indirect=", 9))
            {
              GURL oname = GURL::Filename::UTF8(s1 + 9);
              job->odir = oname.base();
              job->oname = oname.fname();
              indirect = true;
            }
          else
            {
              complain(uarg, "Unrecognized option.");
            }
          // next option
          optc -= 1;
          optv += 1;
        }
      // go
      if (!indirect)
        job->obs = ByteStream::create(output, "wb", false);
      else 
        job->obs = 0;
      job->start();
    }
  G_CATCH(ex)
    {
      if (job) 
        unref(job);
      job = 0;
      ERROR1(document, ex);
    }
  G_ENDCATCH;
  return job;
}




// ----------------------------------------
// S-Expressions (generic)

static miniexp_t
miniexp_status(ddjvu_status_t status)
{
  if (status < DDJVU_JOB_OK)
    return miniexp_dummy;
  else if (status == DDJVU_JOB_STOPPED)
    return miniexp_symbol("stopped");
  else if (status > DDJVU_JOB_OK)
    return miniexp_symbol("failed");    
  return miniexp_nil;
}

static void
miniexp_protect(ddjvu_document_t *document, miniexp_t expr)
{
  GMonitorLock lock(&document->myctx->monitor);
    for(miniexp_t p=document->protect; miniexp_consp(p); p=miniexp_cdr(p))
      if (miniexp_car(p) == expr)
        return;
  if (miniexp_consp(expr) || miniexp_objectp(expr))
    document->protect = miniexp_cons(expr, document->protect);
}

void
ddjvu_miniexp_release(ddjvu_document_t *document, miniexp_t expr)
{
  GMonitorLock lock(&document->myctx->monitor);
  miniexp_t q = miniexp_nil;
  miniexp_t p = document->protect;
  while (miniexp_consp(p))
    {
      if (miniexp_car(p) != expr)
        q = p;
      else if (q)
        miniexp_rplacd(q, miniexp_cdr(p));
      else
        document->protect = miniexp_cdr(p);
      p = miniexp_cdr(p);
    }
}



// ----------------------------------------
// S-Expressions (outline)

static miniexp_t
outline_sub(const GP<DjVmNav> &nav, int &pos, int count)
{
  GP<DjVmNav::DjVuBookMark> entry;
  minivar_t p,q,s;
  while (count > 0 && pos < nav->getBookMarkCount())
    {
      nav->getBookMark(entry, pos++);
      q = outline_sub(nav, pos, entry->count);
      s = miniexp_string((const char*)(entry->url));
      q = miniexp_cons(s, q);
      s = miniexp_string((const char*)(entry->displayname));
      q = miniexp_cons(s, q);
      p = miniexp_cons(q, p);
      count--;
    }
  return miniexp_reverse(p);
}

miniexp_t
ddjvu_document_get_outline(ddjvu_document_t *document)
{
  G_TRY
    {
      ddjvu_status_t status = document->status();
      if (status != DDJVU_JOB_OK)
        return miniexp_status(status);
      DjVuDocument *doc = document->doc;
      if (doc)
        {
          GP<DjVmNav> nav = doc->get_djvm_nav();
          if (! nav) 
            return miniexp_nil;
          minivar_t result;
          int pos = 0;
          result = outline_sub(nav, pos, nav->getBookMarkCount());
          result = miniexp_cons(miniexp_symbol("bookmarks"), result);
          miniexp_protect(document, result);
          return result;
        }
    }
  G_CATCH(ex)
    {
      ERROR1(document, ex);
    }
  G_ENDCATCH;
  return miniexp_status(DDJVU_JOB_FAILED);
}




// ----------------------------------------
// S-Expressions (text)

static struct zone_names_s {
  const char *name;
  DjVuTXT::ZoneType ztype;
  char separator;
} zone_names[] = {
  { "page",   DjVuTXT::PAGE,      0 },
  { "column", DjVuTXT::COLUMN,    DjVuTXT::end_of_column },
  { "region", DjVuTXT::REGION,    DjVuTXT::end_of_region },
  { "para",   DjVuTXT::PARAGRAPH, DjVuTXT::end_of_paragraph },
  { "line",   DjVuTXT::LINE,      DjVuTXT::end_of_line },
  { "word",   DjVuTXT::WORD,      ' ' },
  { "char",   DjVuTXT::CHARACTER, 0 },
  { 0, (DjVuTXT::ZoneType)0 ,0 }
};

static miniexp_t
pagetext_sub(const GP<DjVuTXT> &txt, DjVuTXT::Zone &zone, 
             DjVuTXT::ZoneType detail)
{
  int zinfo;
  for (zinfo=0; zone_names[zinfo].name; zinfo++)
    if (zone.ztype == zone_names[zinfo].ztype)
      break;
  minivar_t p;
  minivar_t a;
  bool gather = zone.children.isempty();
  { // extra nesting for windows
    for (GPosition pos=zone.children; pos; ++pos)
      if (zone.children[pos].ztype > detail)
        gather = true;
  }
  if (gather)
    {
      const char *data = (const char*)(txt->textUTF8) + zone.text_start;
      int length = zone.text_length;
      if (length>0 && data[length-1]==zone_names[zinfo].separator)
        length -= 1;
      a = miniexp_substring(data, length);
      p = miniexp_cons(a, p);
    }
  else
    {
      for (GPosition pos=zone.children; pos; ++pos)
        {
          a = pagetext_sub(txt, zone.children[pos], detail);
          p = miniexp_cons(a, p);
        }
    }
  p = miniexp_reverse(p);
  const char *s = zone_names[zinfo].name;
  if (s)
    {
      p = miniexp_cons(miniexp_number(zone.rect.ymax), p);
      p = miniexp_cons(miniexp_number(zone.rect.xmax), p);
      p = miniexp_cons(miniexp_number(zone.rect.ymin), p);
      p = miniexp_cons(miniexp_number(zone.rect.xmin), p);
      p = miniexp_cons(miniexp_symbol(s), p);
      return p;
    }
  return miniexp_nil;
}

miniexp_t
ddjvu_document_get_pagetext(ddjvu_document_t *document, int pageno,
                            const char *maxdetail)
{
  G_TRY
    {
      ddjvu_status_t status = document->status();
      if (status != DDJVU_JOB_OK)
        return miniexp_status(status);
      DjVuDocument *doc = document->doc;
      if (doc)
        {
          document->pageinfoflag = true;
          GP<DjVuFile> file = doc->get_djvu_file(pageno);
          if (! file || ! file->is_data_present() )
            return miniexp_dummy;
          GP<ByteStream> bs = file->get_text();
          if (! bs)
            return miniexp_nil;
          GP<DjVuText> text = DjVuText::create();
          text->decode(bs);
          GP<DjVuTXT> txt = text->txt;
          if (! txt)
            return miniexp_nil;
          minivar_t result;
          DjVuTXT::ZoneType detail = DjVuTXT::CHARACTER;
          { // extra nesting for windows
            for (int i=0; zone_names[i].name; i++)
              if (maxdetail && !strcmp(maxdetail, zone_names[i].name))
                detail = zone_names[i].ztype;
          }
          result = pagetext_sub(txt, txt->page_zone, detail);
          miniexp_protect(document, result);
          return result;
        }
    }
  G_CATCH(ex)
    {
      ERROR1(document, ex);
    }
  G_ENDCATCH;
  return miniexp_status(DDJVU_JOB_FAILED);
}


// ----------------------------------------
// S-Expressions (annotations)

// The difficulty here lies with the syntax of strings in annotation chunks.
// - Early versions of djvu only had one possible escape 
//   sequence (\") in annotation strings. All other characters
//   are accepted literally until reaching the closing double quote.
// - Current versions of djvu understand the usual backslash escapes.
//   All non printable ascii characters must however be escaped.
//   This is a subset of the miniexp syntax.
// We first check if strings in the annotation chunk obey the modern syntax.
// The compatibility mode is turned on if they contain non printable ascii 
// characters or illegal backslash sequences. Function <anno_getc()> then 
// creates the proper escapes on the fly.


struct anno_dat_s {
  const char *s;
  char buf[8];
  int  blen;
  int  state;
  bool compat;
  bool eof;
};


static bool
anno_compat(const char *s)
{
  int state = 0;
  bool compat = false;
  while (s && *s && !compat)
    {
      int i = (int)(unsigned char)*s++;
      switch(state)
        {
        case 0:
          if (i == '\"')
            state = '\"';
          break;
        case '\"':
          if (i == '\"')
            state = 0;
          else if (i == '\\')
            state = '\\';
          else if (isascii(i) && !isprint(i))
            compat = true;
          break;
        case '\\':
          if (!strchr("01234567abtnvfr\"\\",i))
            compat = true;
          state = '\"';
          break;
        }
    }
  return compat;
}


static int
anno_fgetc(miniexp_io_t *io)
{
  struct anno_dat_s *anno_dat_p = (struct anno_dat_s*)(io->data[0]);
  struct anno_dat_s &anno_dat = *anno_dat_p;
  if (anno_dat.blen>0)
    {
      anno_dat.blen--;
      char c = anno_dat.buf[0];
      for (int i=0; i<anno_dat.blen; i++)
        anno_dat.buf[i] = anno_dat.buf[i+1];
      return c;
    }
  if (! *anno_dat.s)
    return EOF;
  int c = (int)(unsigned char)*anno_dat.s++;
  if (anno_dat.compat)
    {
      switch (anno_dat.state)
        {
        case 0:
          if (c == '\"') 
            anno_dat.state = '\"';
          break;
        case '\"':
          if (c == '\"') 
            anno_dat.state = 0;
          else if (c == '\\')
            anno_dat.state = '\\';
          else if (isascii(c) && !isprint(c))
            {
              sprintf(anno_dat.buf,"%03o", c);
              anno_dat.blen = strlen(anno_dat.buf);
              c = '\\';
            }
          break;
        case '\\':
          anno_dat.state = '\"';
          if (c != '\"')
            {
              sprintf(anno_dat.buf,"\\%03o", c);
              anno_dat.blen = strlen(anno_dat.buf);
              c = '\\';
            }
          break;
        }
    }
  return c;
}


static int
anno_ungetc(miniexp_io_t *io, int c)
{
  if (c == EOF)
    return EOF;
  struct anno_dat_s *anno_dat_p = (struct anno_dat_s*)(io->data[0]);
  struct anno_dat_s &anno_dat = *anno_dat_p;
  if (anno_dat.blen>=(int)sizeof(anno_dat.buf))
    return EOF;
  for (int i=anno_dat.blen; i>0; i--)
    anno_dat.buf[i] = anno_dat.buf[i-1];
  anno_dat.blen += 1;
  anno_dat.buf[0] = c;
  return c;
}


static void
anno_sub(ByteStream *bs, miniexp_t &result)
{
  // Read bs
  GUTF8String raw;
  char buffer[1024];
  int length;
  while ((length=bs->read(buffer, sizeof(buffer))))
    raw += GUTF8String(buffer, length);
  // Prepare 
  miniexp_t a;
  struct anno_dat_s anno_dat;
  anno_dat.s = (const char*)raw;
  anno_dat.compat = anno_compat(anno_dat.s);
  anno_dat.blen = 0;
  anno_dat.state = 0;
  anno_dat.eof = false;
  miniexp_io_t io;
  miniexp_io_init(&io);
  io.data[0] = (void*)&anno_dat;
  io.fgetc = anno_fgetc;
  io.ungetc = anno_ungetc;
  io.p_macrochar = 0;
  io.p_diezechar = 0;
  io.p_macroqueue = 0;
  // Read
  while (* anno_dat.s )
    if ((a = miniexp_read_r(&io)) != miniexp_dummy)
      result = miniexp_cons(a, result);
}


static miniexp_t
get_bytestream_anno(GP<ByteStream> annobs)
{
  if (! (annobs && annobs->size()))
    return miniexp_nil;
  GP<IFFByteStream> iff = IFFByteStream::create(annobs);
  GUTF8String chkid;
  minivar_t result;
  while (iff->get_chunk(chkid))
    {
      GP<ByteStream> bs;
      if (chkid == "ANTa") 
        bs = iff->get_bytestream();
      else if (chkid == "ANTz")
        bs = BSByteStream::create(iff->get_bytestream());
      if (bs)
        anno_sub(bs, result);
      iff->close_chunk();
    }
  return miniexp_reverse(result);
}


static miniexp_t
get_file_anno(GP<DjVuFile> file)
{
  // Make sure all data is present
  if (! file || ! file->is_all_data_present())
    {
      if (file && file->is_data_present())
        {
          if (! file->are_incl_files_created())
            file->process_incl_chunks();
          if (! file->are_incl_files_created())
            {
              if (file->get_flags() & DjVuFile::STOPPED)
                return miniexp_status(DDJVU_JOB_STOPPED);
              return miniexp_status(DDJVU_JOB_FAILED);
            }
          /* SumatraPDF: TODO: how to prevent a potentially infinite loop? */
          return miniexp_status(DDJVU_JOB_FAILED);
        }
      return miniexp_dummy;
    }
  // Access annotation data
  return get_bytestream_anno(file->get_merged_anno());
}


miniexp_t
ddjvu_document_get_pageanno(ddjvu_document_t *document, int pageno)
{
  G_TRY
    {
      ddjvu_status_t status = document->status();
      if (status != DDJVU_JOB_OK)
        return miniexp_status(status);
      DjVuDocument *doc = document->doc;
      if (doc)
        {
          document->pageinfoflag = true;
          minivar_t result = get_file_anno( doc->get_djvu_file(pageno) );
          if (miniexp_consp(result))
            miniexp_protect(document, result);
          return result;
        }
    }
  G_CATCH(ex)
    {
      ERROR1(document, ex);
    }
  G_ENDCATCH;
  return miniexp_status(DDJVU_JOB_FAILED);
}


miniexp_t
ddjvu_document_get_anno(ddjvu_document_t *document, int compat)
{
  G_TRY
    {
      ddjvu_status_t status = document->status();
      if (status != DDJVU_JOB_OK)
        return miniexp_status(status);
      DjVuDocument *doc = document->doc;
      if (doc)
        {
#if EXPERIMENTAL_DOCUMENT_ANNOTATIONS
          // not yet implemented
          GP<ByteStream> anno = doc->get_document_anno();
          if (anno)
            return get_bytestream_anno(anno);
#endif
          if (compat)
            {
              // look for shared annotations
              int doc_type = doc->get_doc_type();
              if (doc_type != DjVuDocument::BUNDLED &&
                  doc_type != DjVuDocument::INDIRECT )
                return miniexp_nil;
              GP<DjVmDir> dir = doc->get_djvm_dir();
              int filenum = dir->get_files_num();
              GP<DjVmDir::File> fdesc;
              for (int i=0; i<filenum; i++)
                {
                  GP<DjVmDir::File> f = dir->pos_to_file(i);
                  if (!f->is_shared_anno())
                    continue;
                  if (fdesc)
                    return miniexp_nil;
                  fdesc = f;
                }
              if (fdesc)
                {
                  GUTF8String id = fdesc->get_load_name();
                  return get_file_anno(doc->get_djvu_file(id));
                }
            }
          return miniexp_nil;
        }
    }
  G_CATCH(ex)
    {
      ERROR1(document, ex);
    }
  G_ENDCATCH;
  return miniexp_status(DDJVU_JOB_FAILED);
}




/* ------ helpers for annotations ---- */

static const char *
simple_anno_sub(miniexp_t p, miniexp_t s, int i)
{
  const char *result = 0;
  while (miniexp_consp(p))
    {
      miniexp_t a = miniexp_car(p);
      p = miniexp_cdr(p);
      if (miniexp_car(a) == s)
        {
          miniexp_t q = miniexp_nth(i, a);
          if (miniexp_symbolp(q))
            result = miniexp_to_name(q);
        }
    }
  return result;
}

const char *
ddjvu_anno_get_bgcolor(miniexp_t p)
{
  return simple_anno_sub(p, miniexp_symbol("background"), 1);
}

const char *
ddjvu_anno_get_zoom(miniexp_t p)
{
  return simple_anno_sub(p, miniexp_symbol("zoom"), 1);
}

const char *
ddjvu_anno_get_mode(miniexp_t p)
{
  return simple_anno_sub(p, miniexp_symbol("mode"), 1);
}

const char *
ddjvu_anno_get_horizalign(miniexp_t p)
{
  return simple_anno_sub(p, miniexp_symbol("align"), 1);
}

const char *
ddjvu_anno_get_vertalign(miniexp_t p)
{
  return simple_anno_sub(p, miniexp_symbol("align"), 2);
}

miniexp_t *
ddjvu_anno_get_hyperlinks(miniexp_t annotations)
{
  miniexp_t p;
  miniexp_t s_maparea = miniexp_symbol("maparea");
  int i = 0;
  for (p = annotations; miniexp_consp(p); p = miniexp_cdr(p))
    if (miniexp_caar(p) == s_maparea)
      i += 1;
  miniexp_t *k = (miniexp_t*)malloc((1+i)*sizeof(miniexp_t));
  if (! k) return 0;
  i = 0;
  for (p = annotations; miniexp_consp(p); p = miniexp_cdr(p))
    if (miniexp_caar(p) == s_maparea)
      k[i++] = miniexp_car(p);
  k[i] = 0;
  return k;
}

static void
metadata_sub(miniexp_t p, GMap<miniexp_t,miniexp_t> &m)
{
  miniexp_t s_metadata = miniexp_symbol("metadata");
  while (miniexp_consp(p))
    {
      if (miniexp_caar(p) == s_metadata)
        {
          miniexp_t q = miniexp_cdar(p);
          while (miniexp_consp(q))
            {
              miniexp_t a = miniexp_car(q);
              q = miniexp_cdr(q);
              if (miniexp_consp(a) && 
                  miniexp_symbolp(miniexp_car(a)) &&
                  miniexp_stringp(miniexp_cadr(a)) )
                {
                  m[miniexp_car(a)] = miniexp_cadr(a);
                }
            }
        }
      p = miniexp_cdr(p);
    }
}

miniexp_t *
ddjvu_anno_get_metadata_keys(miniexp_t p)
{
  minivar_t l;
  GMap<miniexp_t,miniexp_t> m;
  metadata_sub(p, m);
  int i = m.size();
  miniexp_t *k = (miniexp_t*)malloc((1+i)*sizeof(miniexp_t));
  if (! k) return 0;
  i = 0;
    for (GPosition p=m; p; ++p)
      k[i++] = m.key(p);
  k[i] = 0;
  return k;
}

const char *
ddjvu_anno_get_metadata(miniexp_t p, miniexp_t key)
{
  GMap<miniexp_t,miniexp_t> m;
  metadata_sub(p, m);
  if (m.contains(key))
    return miniexp_to_str(m[key]);
  return 0;
}

const char *
ddjvu_anno_get_xmp(miniexp_t p)
{
  miniexp_t s = miniexp_symbol("xmp");
  while (miniexp_consp(p))
    {
      miniexp_t a = miniexp_car(p);
      p = miniexp_cdr(p);
      if (miniexp_car(a) == s)
        {
          miniexp_t q = miniexp_nth(1, a);
          if (miniexp_stringp(q))
            return miniexp_to_str(q);
        }
    }
  return 0;
}


// ----------------------------------------
// Backdoors

GP<DjVuImage>
ddjvu_get_DjVuImage(ddjvu_page_t *page)
{
  return page->img;
}


GP<DjVuDocument>
ddjvu_get_DjVuDocument(ddjvu_document_t *document)
{
  return document->doc;
}


/* SumatraPDF: access to free() mirroring malloc() above */
void ddjvu_free(void *ptr)
{
  free(ptr);
}
