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

#include "DjVuFile.h"
#include "IFFByteStream.h"
#include "GOS.h"
#include "MMRDecoder.h"
#ifdef NEED_JPEG_DECODER
#include "JPEGDecoder.h"
#endif
#include "DjVuAnno.h"
#include "DjVuText.h"
#include "DataPool.h"
#include "JB2Image.h"
#include "IW44Image.h"
#include "DjVuNavDir.h"
#ifndef NEED_DECODER_ONLY
#include "BSByteStream.h"
#endif // NEED_DECODER_ONLY

#include "debug.h"


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


#define STRINGIFY(x) STRINGIFY_(x)
#define STRINGIFY_(x) #x


#define REPORT_EOF(x) \
  {G_TRY{G_THROW( ByteStream::EndOfFile );}G_CATCH(ex){report_error(ex,(x));}G_ENDCATCH;}

static GP<GPixmap> (*djvu_decode_codec)(ByteStream &bs)=0;

class ProgressByteStream : public ByteStream
{
public:
  ProgressByteStream(const GP<ByteStream> & xstr) : str(xstr),
    last_call_pos(0) {}
  virtual ~ProgressByteStream() {}

  virtual size_t read(void *buffer, size_t size)
  {
    int rc=0;
	   // G_TRY {} CATCH; block here is merely to avoid egcs internal error
    G_TRY {
      int cur_pos=str->tell();
      if (progress_cb && (last_call_pos/256!=cur_pos/256))
      {
        progress_cb(cur_pos, progress_cl_data);
        last_call_pos=cur_pos;
      }
      rc=str->read(buffer, size);
    } G_CATCH_ALL {
      G_RETHROW;
    } G_ENDCATCH;
    return rc;
  }
  virtual size_t write(const void *buffer, size_t size)
  {
    return str->write(buffer, size);
  }
  virtual int seek(long offset, int whence = SEEK_SET, bool nothrow=false)
  {
    return str->seek(offset, whence);
  }
  virtual long tell(void ) const { return str->tell(); }

  void		set_progress_cb(void (* xprogress_cb)(int, void *),
  void * xprogress_cl_data)
  {
    progress_cb=xprogress_cb;
    progress_cl_data=xprogress_cl_data;
  }
private:
  GP<ByteStream> str;
  void		* progress_cl_data;
  void		(* progress_cb)(int pos, void *);
  int		last_call_pos;
  
  // Cancel C++ default stuff
  ProgressByteStream & operator=(const ProgressByteStream &);
};


DjVuFile::DjVuFile()
: file_size(0), recover_errors(ABORT), verbose_eof(false), chunks_number(-1),
initialized(false)
{
}

void
DjVuFile::check() const
{
  if (!initialized)
    G_THROW( ERR_MSG("DjVuFile.not_init") );
}

GP<DjVuFile>
DjVuFile::create(
  const GP<ByteStream> & str, const ErrorRecoveryAction recover_errors,
  const bool verbose_eof )
{
  DjVuFile *file=new DjVuFile();
  GP<DjVuFile> retval=file;
  file->set_recover_errors(recover_errors);
  file->set_verbose_eof(verbose_eof);
  file->init(str);
  return retval;
}

void 
DjVuFile::init(const GP<ByteStream> & str)
{
  DEBUG_MSG("DjVuFile::DjVuFile(): ByteStream constructor\n");
  DEBUG_MAKE_INDENT(3);
  
  if (initialized)
    G_THROW( ERR_MSG("DjVuFile.2nd_init") );
  if (!get_count())
    G_THROW( ERR_MSG("DjVuFile.not_secured") );
  
  file_size=0;
  decode_thread=0;
  
  // Read the data from the stream
  data_pool=DataPool::create(str);
  
  // Construct some dummy URL
  GUTF8String buffer;
  buffer.format("djvufile:/%p.djvu", this);
  DEBUG_MSG("DjVuFile::DjVuFile(): url is "<<(const char *)buffer<<"\n");
  url=GURL::UTF8(buffer);
  
  // Set it here because trigger will call other DjVuFile's functions
  initialized=true;
  
  // Add (basically - call) the trigger
  data_pool->add_trigger(-1, static_trigger_cb, this);
}

GP<DjVuFile>
DjVuFile::create(
  const GURL & xurl, GP<DjVuPort> port, 
  const ErrorRecoveryAction recover_errors, const bool verbose_eof ) 
{
  DjVuFile *file=new DjVuFile();
  GP<DjVuFile> retval=file;
  file->set_recover_errors(recover_errors);
  file->set_verbose_eof(verbose_eof);
  file->init(xurl,port);
  return retval;
}

void
DjVuFile::init(const GURL & xurl, GP<DjVuPort> port) 
{
  DEBUG_MSG("DjVuFile::init(): url='" << xurl << "'\n");
  DEBUG_MAKE_INDENT(3);
  
  if (initialized)
    G_THROW( ERR_MSG("DjVuFile.2nd_init") );
  if (!get_count())
    G_THROW( ERR_MSG("DjVuFile.not_secured") );
  if (xurl.is_empty())
    G_THROW( ERR_MSG("DjVuFile.empty_URL") );
  
  url = xurl;
  DEBUG_MSG("DjVuFile::DjVuFile(): url is "<<(const char *)url<<"\n");
  file_size=0;
  decode_thread=0;
  
  DjVuPortcaster * pcaster=get_portcaster();
  
  // We need it 'cause we're waiting for our own termination in stop_decode()
  pcaster->add_route(this, this);
  if (!port)
    port = simple_port = new DjVuSimplePort();
  pcaster->add_route(this, port);
  
  // Set it here because trigger will call other DjVuFile's functions
  initialized=true;
  
  if (!(data_pool=DataPool::create(pcaster->request_data(this, url))))
    G_THROW( ERR_MSG("DjVuFile.no_data") "\t"+url.get_string());
  data_pool->add_trigger(-1, static_trigger_cb, this);
}

DjVuFile::~DjVuFile(void)
{
  DEBUG_MSG("DjVuFile::~DjVuFile(): destroying...\n");
  DEBUG_MAKE_INDENT(3);
  
  // No more messages. They may result in adding this file to a cache
  // which will be very-very bad as we're being destroyed
  get_portcaster()->del_port(this);
  
  // Unregister the trigger (we don't want it to be called and attempt
  // to access the destroyed object)
  if (data_pool)
    data_pool->del_trigger(static_trigger_cb, this);
  
  // We don't have to wait for decoding to finish here. It's already
  // finished (we know it because there is a "life saver" in the
  // thread function)  -- but we need to delete it
  delete decode_thread; decode_thread=0;
}

void
DjVuFile::reset(void)
{
   flags.enter();
   info = 0; 
   anno = 0; 
   text = 0; 
   meta = 0; 
   bg44 = 0; 
   fgbc = 0;
   fgjb = 0; 
   fgjd = 0;
   fgpm = 0;
   dir  = 0; 
   description = ""; 
   mimetype = "";
   flags=(flags&(ALL_DATA_PRESENT|DECODE_STOPPED|DECODE_FAILED));
   flags.leave();
}

unsigned int
DjVuFile::get_memory_usage(void) const
{
   unsigned int size=sizeof(*this);
   if (info) size+=info->get_memory_usage();
   if (bg44) size+=bg44->get_memory_usage();
   if (fgjb) size+=fgjb->get_memory_usage();
   if (fgpm) size+=fgpm->get_memory_usage();
   if (fgbc) size+=fgbc->size()*sizeof(int);
   if (anno) size+=anno->size();
   if (meta) size+=meta->size();
   if (dir) size+=dir->get_memory_usage();
   return size;
}

GPList<DjVuFile>
DjVuFile::get_included_files(bool only_created)
{
  check();
  if (!only_created && !are_incl_files_created())
    process_incl_chunks();
  
  GCriticalSectionLock lock(&inc_files_lock);
  GPList<DjVuFile> list=inc_files_list;	// Get a copy when locked
  return list;
}

void
DjVuFile::wait_for_chunk(void)
// Will return after a chunk has been decoded
{
  check();
  DEBUG_MSG("DjVuFile::wait_for_chunk() called\n");
  DEBUG_MAKE_INDENT(3);
  chunk_mon.enter();
  chunk_mon.wait();
  chunk_mon.leave();
}

bool
DjVuFile::wait_for_finish(bool self)
// if self==TRUE, will block until decoding of this file is over
// if self==FALSE, will block until decoding of a child (direct
// or indirect) is over.
// Will return FALSE if there is nothing to wait for. TRUE otherwise
{
  DEBUG_MSG("DjVuFile::wait_for_finish():  self=" << self <<"\n");
  DEBUG_MAKE_INDENT(3);
  
  check();
  
  if (self)
  {
    // It's best to check for self termination using flags. The reason
    // is that finish_mon is updated in a DjVuPort function, which
    // will not be called if the object is being destroyed
    GMonitorLock lock(&flags);
    if (is_decoding())
    {
      while(is_decoding()) flags.wait();
      DEBUG_MSG("got it\n");
      return 1;
    }
  } else
  {
    // By locking the monitor, we guarantee that situation doesn't change
    // between the moments when we check for pending finish events
    // and when we actually run wait(). If we don't lock, the last child
    // may terminate in between, and we'll wait forever.
    //
    // Locking is required by GMonitor interface too, btw.
    GMonitorLock lock(&finish_mon);
    GP<DjVuFile> file;
    {
      GCriticalSectionLock lock(&inc_files_lock);
      for(GPosition pos=inc_files_list;pos;++pos)
      {
        GP<DjVuFile> & f=inc_files_list[pos];
        if (f->is_decoding())
        {
          file=f; break;
        }
      }
    }
    if (file)
    {
      finish_mon.wait();
      DEBUG_MSG("got it\n");
      return 1;
    }
  }
  DEBUG_MSG("nothing to wait for\n");
  return 0;
}

void
DjVuFile::notify_chunk_done(const DjVuPort *, const GUTF8String &)
{
  check();
  chunk_mon.enter();
  chunk_mon.broadcast();
  chunk_mon.leave();
}

void
DjVuFile::notify_file_flags_changed(const DjVuFile * src,
                                    long set_mask, long clr_mask)
{
  check();
  if (set_mask & (DECODE_OK | DECODE_FAILED | DECODE_STOPPED))
  {
    // Signal threads waiting for file termination
    finish_mon.enter();
    finish_mon.broadcast();
    finish_mon.leave();
    
    // In case a thread is still waiting for a chunk
    chunk_mon.enter();
    chunk_mon.broadcast();
    chunk_mon.leave();
  }
  
  if ((set_mask & ALL_DATA_PRESENT) && src!=this &&
    are_incl_files_created() && is_data_present())
  {
    if (src!=this && are_incl_files_created() && is_data_present())
    {
      // Check if all children have data
      bool all=true;
      {
        GCriticalSectionLock lock(&inc_files_lock);
        for(GPosition pos=inc_files_list;pos;++pos)
          if (!inc_files_list[pos]->is_all_data_present())
          {
            all=false;
            break;
          }
      }
      if (all)
      {
        DEBUG_MSG("Just got ALL data for '" << url << "'\n");
        flags|=ALL_DATA_PRESENT;
        get_portcaster()->notify_file_flags_changed(this, ALL_DATA_PRESENT, 0);
      }
    }
  }
}

void
DjVuFile::static_decode_func(void * cl_data)
{
  DjVuFile * th=(DjVuFile *) cl_data;
  
  /* Please do not undo this life saver. If you do then try to resolve the
  following conflict first:
  1. Decoding starts and there is only one external reference
  to the DjVuFile.
  2. Decoding proceeds and calls DjVuPortcaster::notify_error(),
  which creates inside a temporary GP<DjVuFile>.
  3. While notify_error() is running, the only external reference
  is lost, but the DjVuFile is still alive (remember the
  temporary GP<>?)
  4. The notify_error() returns, the temporary GP<> gets destroyed
  and the DjVuFile is attempting to destroy right in the middle
  of the decoding thread. This is either a dead block (waiting
  for the termination of the decoding from the ~DjVuFile() called
  from the decoding thread) or coredump. */
  GP<DjVuFile> life_saver=th;
  th->decode_life_saver=0;
  G_TRY {
    th->decode_func();
  } G_CATCH_ALL {
  } G_ENDCATCH;
}

void
DjVuFile::decode_func(void)
{
  check();
  DEBUG_MSG("DjVuFile::decode_func() called, url='" << url << "'\n");
  DEBUG_MAKE_INDENT(3);
  
  DjVuPortcaster * pcaster=get_portcaster();
  
  G_TRY {
    const GP<ByteStream> decode_stream(decode_data_pool->get_stream());
    ProgressByteStream *pstr=new ProgressByteStream(decode_stream);
    const GP<ByteStream> gpstr(pstr);
    pstr->set_progress_cb(progress_cb, this);
    
    decode(gpstr);
    
    // Wait for all child files to finish
    while(wait_for_finish(0))
    	continue;
    
    DEBUG_MSG("waiting for children termination\n");
    // Check for termination status
    GCriticalSectionLock lock(&inc_files_lock);
    for(GPosition pos=inc_files_list;pos;++pos)
    {
      GP<DjVuFile> & f=inc_files_list[pos];
      if (f->is_decode_failed())
	G_THROW( ERR_MSG("DjVuFile.decode_fail") );
      if (f->is_decode_stopped())
	G_THROW( DataPool::Stop );
      if (!f->is_decode_ok())
	{
	  DEBUG_MSG("this_url='" << url << "'\n");
	  DEBUG_MSG("incl_url='" << f->get_url() << "'\n");
	  DEBUG_MSG("decoding=" << f->is_decoding() << "\n");
	  DEBUG_MSG("status='" << f->get_flags() << "\n");
	  G_THROW( ERR_MSG("DjVuFile.not_finished") );
	}
    }
  } G_CATCH(exc) {
    G_TRY {
      if (!exc.cmp_cause(DataPool::Stop))
	{
	  flags.enter();
	  flags = (flags & ~DECODING) | DECODE_STOPPED;
	  flags.leave();
	  pcaster->notify_status(this, GUTF8String(ERR_MSG("DjVuFile.stopped"))
				 + GUTF8String("\t") + GUTF8String(url));
	  pcaster->notify_file_flags_changed(this, DECODE_STOPPED, DECODING);
	} else
	{
	  flags.enter();
	  flags = (flags & ~DECODING) | DECODE_FAILED;
	  flags.leave();
	  pcaster->notify_status(this, GUTF8String(ERR_MSG("DjVuFile.failed"))
				 + GUTF8String("\t") + GUTF8String(url));
	  pcaster->notify_error(this, exc.get_cause());
	  pcaster->notify_file_flags_changed(this, DECODE_FAILED, DECODING);
	}
    } G_CATCH_ALL
	{
	  DEBUG_MSG("******* Oops. Almost missed an exception\n");
	} G_ENDCATCH;
  } G_ENDCATCH;

  decode_data_pool->clear_stream();
  G_TRY {
    if (flags.test_and_modify(DECODING, 0, DECODE_OK | INCL_FILES_CREATED, DECODING))
      pcaster->notify_file_flags_changed(this, DECODE_OK | INCL_FILES_CREATED, 
                                         DECODING);
  } G_CATCH_ALL {} G_ENDCATCH;
  DEBUG_MSG("decoding thread for url='" << url << "' ended\n");
}

GP<DjVuFile>
DjVuFile::process_incl_chunk(ByteStream & str, int file_num)
{
  check();
  DEBUG_MSG("DjVuFile::process_incl_chunk(): processing INCL chunk...\n");
  DEBUG_MAKE_INDENT(3);
  
  DjVuPortcaster * pcaster=get_portcaster();
  
  GUTF8String incl_str;
  char buffer[1024];
  int length;
  while((length=str.read(buffer, 1024)))
    incl_str+=GUTF8String(buffer, length);
  
  // Eat '\n' in the beginning and at the end
  while(incl_str.length() && incl_str[0]=='\n')
  {
    incl_str=incl_str.substr(1,(unsigned int)(-1));
  }
  while(incl_str.length()>0 && incl_str[(int)incl_str.length()-1]=='\n')
  {
    incl_str.setat(incl_str.length()-1, 0);
  }
  
  if (incl_str.length()>0)
  {
    if (strchr(incl_str, '/'))
      G_THROW( ERR_MSG("DjVuFile.malformed") );
    
    DEBUG_MSG("incl_str='" << incl_str << "'\n");
    
    GURL incl_url=pcaster->id_to_url(this, incl_str);
    if (incl_url.is_empty())	// Fallback. Should never be used.
      incl_url=GURL::UTF8(incl_str,url.base());
    
    // Now see if there is already a file with this *name* created
    {
      GCriticalSectionLock lock(&inc_files_lock);
      GPosition pos;
      for(pos=inc_files_list;pos;++pos)
      {
        if (inc_files_list[pos]->url.fname()==incl_url.fname())
           break;
      }
      if (pos)
        return inc_files_list[pos];
    }
    
    // No. We have to request a new file
    GP<DjVuFile> file;
    G_TRY
    {
      file=pcaster->id_to_file(this, incl_str);
    }
    G_CATCH(ex)
    {
      unlink_file(incl_str);
      // In order to keep compatibility with the previous
      // release of the DjVu plugin, we will not interrupt
      // decoding here. We will just report the error.
      // NOTE, that it's now the responsibility of the
      // decoder to resolve all chunk dependencies, and
      // abort decoding if necessary.
      
      get_portcaster()->notify_error(this,ex.get_cause());
      return 0;
    }
    G_ENDCATCH;
    if (!file)
    {
      G_THROW( ERR_MSG("DjVuFile.no_create") "\t"+incl_str);
    }
    if (recover_errors!=ABORT)
      file->set_recover_errors(recover_errors);
    if (verbose_eof)
      file->set_verbose_eof(verbose_eof);
    pcaster->add_route(file, this);
    
    // We may have been stopped. Make sure the child will be stopped too.
    if (flags & STOPPED)
      file->stop(false);
    if (flags & BLOCKED_STOPPED)
      file->stop(true);
    
    // Lock the list again and check if the file has already been
    // added by someone else
    {
      GCriticalSectionLock lock(&inc_files_lock);
      GPosition pos;
      for(pos=inc_files_list;pos;++pos)
      {
        if (inc_files_list[pos]->url.fname()==incl_url.fname())
          break;
      }
      if (pos)
      {
        file=inc_files_list[pos];
      } else if (file_num<0 || !(pos=inc_files_list.nth(file_num)))
      {
        inc_files_list.append(file);
      } else 
      {
        inc_files_list.insert_before(pos, file);
      }
    }
    return file;
  }
  return 0;
}


void
DjVuFile::report_error(const GException &ex,bool throw_errors)
{
  data_pool->clear_stream();
  if((!verbose_eof)|| (ex.cmp_cause(ByteStream::EndOfFile)))
  {
    if(throw_errors)
        G_RETHROW(ex);
      else
      get_portcaster()->notify_error(this,ex.get_cause());
    }
  else
  {
    GURL url=get_url();
    GUTF8String url_str=url.get_string();
    GUTF8String msg = GUTF8String( ERR_MSG("DjVuFile.EOF") "\t") + url;
    if(throw_errors)
        G_RETHROW(GException(msg,ex.get_file(),ex.get_line(),ex.get_function()));
      else
      get_portcaster()->notify_error(this,msg);
    }
}

void
DjVuFile::process_incl_chunks(void)
// This function may block for data
// NOTE: It may be called again when INCL_FILES_CREATED is set.
// It happens in insert_file() when it has modified the data
// and wants to create the actual file
{
  DEBUG_MSG("DjVuFile::process_incl_chunks(void)\n");
  DEBUG_MAKE_INDENT(3);
  check();
  
  int incl_cnt=0;
  
  const GP<ByteStream> str(data_pool->get_stream());
  GUTF8String chkid;
  const GP<IFFByteStream> giff(IFFByteStream::create(str));
  IFFByteStream &iff=*giff;
  if (iff.get_chunk(chkid))
  {
    int chunks=0;
    int last_chunk=0;
    G_TRY
    {
      int chunks_left=(recover_errors>SKIP_PAGES)?chunks_number:(-1);
      int chksize;
      for(;(chunks_left--)&&(chksize=iff.get_chunk(chkid));last_chunk=chunks)
      {
        chunks++;
        if (chkid=="INCL")
        {
          G_TRY
          {
            process_incl_chunk(*iff.get_bytestream(), incl_cnt++);
          }
          G_CATCH(ex);
          {
            report_error(ex,(recover_errors <= SKIP_PAGES));
          }
          G_ENDCATCH;
        }else if(chkid=="FAKE")
        {
          set_needs_compression(true);
          set_can_compress(true);
        }else if(chkid=="BGjp")
        {
          set_can_compress(true);
        }else if(chkid=="Smmr")
        {
          set_can_compress(true);
        }
        iff.seek_close_chunk();
      }
      if (chunks_number < 0) chunks_number=last_chunk;
    }
    G_CATCH(ex)
    {	
      if (chunks_number < 0)
        chunks_number=(recover_errors>SKIP_CHUNKS)?chunks:last_chunk;
      report_error(ex,(recover_errors <= SKIP_PAGES));
    }
    G_ENDCATCH;
  }
  flags|=INCL_FILES_CREATED;
  data_pool->clear_stream();
}

GP<JB2Dict>
DjVuFile::static_get_fgjd(void *arg)
{
  DjVuFile *file = (DjVuFile*)arg;
  return file->get_fgjd(1);
}

GP<JB2Dict>
DjVuFile::get_fgjd(int block)
{
  check();
  
  // Simplest case
  if (fgjd)
    return fgjd;
  // Check wether included files
  chunk_mon.enter();
  G_TRY {
    for(;;)
    {
      int active = 0;
      GPList<DjVuFile> incs = get_included_files();
      for (GPosition pos=incs.firstpos(); pos; ++pos)
      {
        GP<DjVuFile> file = incs[pos];
        if (file->is_decoding())
          active = 1;
        GP<JB2Dict> fgjd = file->get_fgjd();
        if (fgjd)
        {
          chunk_mon.leave();
          return fgjd;
        }
      }
      // Exit if non-blocking mode
      if (! block)
        break;
      // Exit if there is no decoding activity
      if (! active)
        break;
      // Wait until a new chunk gets decoded
      wait_for_chunk();
    }
  } G_CATCH_ALL {
    chunk_mon.leave();
    G_RETHROW;
  } G_ENDCATCH;
  chunk_mon.leave();
  if (is_decode_stopped()) G_THROW( DataPool::Stop );
  return 0;
}

int
DjVuFile::get_dpi(int w, int h)
{
  int dpi=0, red=1;
  if (info)
  {
    for(red=1; red<=12; red++)
      if ((info->width+red-1)/red==w)
        if ((info->height+red-1)/red==h)
          break;
    if (red>12)
      G_THROW( ERR_MSG("DjVuFile.corrupt_BG44") );
    dpi=info->dpi;
  }
  return (dpi ? dpi : 300)/red;
}

static inline bool
is_info(const GUTF8String &chkid)
{
  return (chkid=="INFO");
}

static inline bool
is_annotation(const GUTF8String &chkid)
{
  return (chkid=="ANTa" ||
    chkid=="ANTz" ||
    chkid=="FORM:ANNO" ); 
}

static inline bool
is_text(const GUTF8String &chkid)
{
  return (chkid=="TXTa" || chkid=="TXTz");
}

static inline bool
is_meta(const GUTF8String &chkid)
{
  return (chkid=="METa" || chkid=="METz");
}


GUTF8String
DjVuFile::decode_chunk( const GUTF8String &id, const GP<ByteStream> &gbs,
  bool djvi, bool djvu, bool iw44)
{
  DEBUG_MSG("DjVuFile::decode_chunk()\n");
  ByteStream &bs=*gbs;
  check();
  
  // If this object is referenced by only one GP<> pointer, this
  // pointer should be the "life_saver" created by the decoding thread.
  // If it is the only GP<> pointer, then nobody is interested in the
  // results of the decoding and we can abort now with #DataPool::Stop#
  if (get_count()==1)
    G_THROW( DataPool::Stop );
  
  GUTF8String desc = ERR_MSG("DjVuFile.unrecog_chunk");
  GUTF8String chkid = id;
  DEBUG_MSG("DjVuFile::decode_chunk() : decoding " << id << "\n");
  
  // INFO  (information chunk for djvu page)
  if (is_info(chkid) && (djvu || djvi))
  {
    if (info)
      G_THROW( ERR_MSG("DjVuFile.corrupt_dupl") );
    if (djvi)
      G_THROW( ERR_MSG("DjVuFile.corrupt_INFO") );
    // DjVuInfo::decode no longer throws version exceptions
    GP<DjVuInfo> xinfo=DjVuInfo::create();
    xinfo->decode(bs);
    info = xinfo;
    desc.format( ERR_MSG("DjVuFile.page_info") );
    // Consistency checks (previously in DjVuInfo::decode)
    if (info->width<0 || info->height<0)
      G_THROW( ERR_MSG("DjVuFile.corrupt_zero") );
    if (info->version >= DJVUVERSION_TOO_NEW)
      G_THROW( ERR_MSG("DjVuFile.new_version") "\t" 
               STRINGIFY(DJVUVERSION_TOO_NEW) );
  }
  
  // INCL (inclusion chunk)
  else if (chkid == "INCL" && (djvi || djvu || iw44))
  {
    GP<DjVuFile> file=process_incl_chunk(bs);
    if (file)
    {
      int decode_was_already_started = 1;
      {
        GMonitorLock lock(&file->flags);
          // Start decoding
        if(file->resume_decode())
        {
          decode_was_already_started = 0;
        }
      }
      // Send file notifications if previously started
      if (decode_was_already_started)
      {
        // May send duplicate notifications...
        if (file->is_decode_ok())
          get_portcaster()->notify_file_flags_changed(file, DECODE_OK, 0);
        else if (file->is_decode_failed())
          get_portcaster()->notify_file_flags_changed(file, DECODE_FAILED, 0);
      }
      desc.format( ERR_MSG("DjVuFile.indir_chunk1") "\t" + file->get_url().fname() );
    } else
      desc.format( ERR_MSG("DjVuFile.indir_chunk2") );
  }
  
  // Djbz (JB2 Dictionary)
  else if (chkid == "Djbz" && (djvu || djvi))
  {
    if (this->fgjd)
      G_THROW( ERR_MSG("DjVuFile.dupl_Dxxx") );
    if (this->fgjd)
      G_THROW( ERR_MSG("DjVuFile.Dxxx_after_Sxxx") );
    GP<JB2Dict> fgjd = JB2Dict::create();
    fgjd->decode(gbs);
    this->fgjd = fgjd;
    desc.format( ERR_MSG("DjVuFile.shape_dict") "\t%d", fgjd->get_shape_count() );
  } 
  
  // Sjbz (JB2 encoded mask)
  else if (chkid=="Sjbz" && (djvu || djvi))
  {
    if (this->fgjb)
      G_THROW( ERR_MSG("DjVuFile.dupl_Sxxx") );
    GP<JB2Image> fgjb=JB2Image::create();
    // ---- begin hack
    if (info && info->version <=18)
      fgjb->reproduce_old_bug = true;
    // ---- end hack
    fgjb->decode(gbs, static_get_fgjd, (void*)this);
    this->fgjb = fgjb;
    desc.format( ERR_MSG("DjVuFile.fg_mask") "\t%d\t%d\t%d",
      fgjb->get_width(), fgjb->get_height(),
      get_dpi(fgjb->get_width(), fgjb->get_height()));
  }
  
  // Smmr (MMR-G4 encoded mask)
  else if (chkid=="Smmr" && (djvu || djvi))
  {
    if (this->fgjb)
      G_THROW( ERR_MSG("DjVuFile.dupl_Sxxx") );
    set_can_compress(true);
    this->fgjb = MMRDecoder::decode(gbs);
    desc.format( ERR_MSG("DjVuFile.G4_mask") "\t%d\t%d\t%d",
      fgjb->get_width(), fgjb->get_height(),
      get_dpi(fgjb->get_width(), fgjb->get_height()));
  }
  
  // BG44 (background wavelets)
  else if (chkid == "BG44" && (djvu || djvi))
  {
    if (!bg44)
    {
      if (bgpm)
        G_THROW( ERR_MSG("DjVuFile.dupl_backgrnd") );
      // First chunk
      GP<IW44Image> bg44=IW44Image::create_decode(IW44Image::COLOR);
      bg44->decode_chunk(gbs);
      this->bg44 = bg44;
      desc.format( ERR_MSG("DjVuFile.IW44_bg1") "\t%d\t%d\t%d",
		      bg44->get_width(), bg44->get_height(),
          get_dpi(bg44->get_width(), bg44->get_height()));
    } 
    else
    {
      // Refinement chunks
      GP<IW44Image> bg44 = this->bg44;
      bg44->decode_chunk(gbs);
      desc.format( ERR_MSG("DjVuFile.IW44_bg2") "\t%d\t%d",
		      bg44->get_serial(), get_dpi(bg44->get_width(), bg44->get_height()));
    }
  }
  
  // FG44 (foreground wavelets)
  else if (chkid == "FG44" && (djvu || djvi))
  {
    if (fgpm || fgbc)
      G_THROW( ERR_MSG("DjVuFile.dupl_foregrnd") );
    GP<IW44Image> gfg44=IW44Image::create_decode(IW44Image::COLOR);
    IW44Image &fg44=*gfg44;
    fg44.decode_chunk(gbs);
    fgpm=fg44.get_pixmap();
    desc.format( ERR_MSG("DjVuFile.IW44_fg") "\t%d\t%d\t%d",
      fg44.get_width(), fg44.get_height(),
      get_dpi(fg44.get_width(), fg44.get_height()));
  } 
  
  // LINK (background LINK)
  else if (chkid == "LINK" && (djvu || djvi))
  {
    if (bg44 || bgpm)
      G_THROW( ERR_MSG("DjVuFile.dupl_backgrnd") );
    if(djvu_decode_codec)
    {
      set_modified(true);
      set_can_compress(true);
      set_needs_compression(true);
      this->bgpm = djvu_decode_codec(bs);
      desc.format( ERR_MSG("DjVuFile.color_import1") "\t%d\t%d\t%d",
        bgpm->columns(), bgpm->rows(),
        get_dpi(bgpm->columns(), bgpm->rows()));
    }else
    {
      desc.format( ERR_MSG("DjVuFile.color_import2") );
    }
  } 
  
  // BGjp (background JPEG)
  else if (chkid == "BGjp" && (djvu || djvi))
  {
    if (bg44 || bgpm)
      G_THROW( ERR_MSG("DjVuFile.dupl_backgrnd") );
    set_can_compress(true);
#ifdef NEED_JPEG_DECODER
    this->bgpm = JPEGDecoder::decode(bs);
    desc.format( ERR_MSG("DjVuFile.JPEG_bg1") "\t%d\t%d\t%d",
      bgpm->columns(), bgpm->rows(),
      get_dpi(bgpm->columns(), bgpm->rows()));
#else
    desc.format( ERR_MSG("DjVuFile.JPEG_bg2") );
#endif
  } 
  
  // FGjp (foreground JPEG)
  else if (chkid == "FGjp" && (djvu || djvi))
  {
    if (fgpm || fgbc)
      G_THROW( ERR_MSG("DjVuFile.dupl_foregrnd") );
#ifdef NEED_JPEG_DECODER
    this->fgpm = JPEGDecoder::decode(bs);
    desc.format( ERR_MSG("DjVuFile.JPEG_fg1") "\t%d\t%d\t%d",
      fgpm->columns(), fgpm->rows(),
      get_dpi(fgpm->columns(), fgpm->rows()));
#else
    desc.format( ERR_MSG("DjVuFile.JPEG_fg2") );
#endif
  } 
  
  // BG2k (background JPEG-2000) Note: JPEG2K bitstream not finalized.
  else if (chkid == "BG2k" && (djvu || djvi))
  {
    if (bg44)
      G_THROW( ERR_MSG("DjVuFile.dupl_backgrnd") );
    desc.format( ERR_MSG("DjVuFile.JPEG2K_bg") );
  } 
  
  // FG2k (foreground JPEG-2000) Note: JPEG2K bitstream not finalized.
  else if (chkid == "FG2k" && (djvu || djvi))
  {
    if (fgpm || fgbc)
      G_THROW( ERR_MSG("DjVuFile.dupl_foregrnd") );
    desc.format( ERR_MSG("DjVuFile.JPEG2K_fg") );
  } 
  
  // FGbz (foreground color vector)
  else if (chkid == "FGbz" && (djvu || djvi))
  {
    if (fgpm || fgbc)
      G_THROW( ERR_MSG("DjVuFile.dupl_foregrnd") );
    GP<DjVuPalette> fgbc = DjVuPalette::create();
    fgbc->decode(gbs);
    this->fgbc = fgbc;
    desc.format( ERR_MSG("DjVuFile.JB2_fg") "\t%d\t%d",
      fgbc->size(), fgbc->colordata.size());
  }
  
  // BM44/PM44 (IW44 data)
  else if ((chkid == "PM44" || chkid=="BM44") && iw44)
  {
    if (!bg44)
    {
      // First chunk
      GP<IW44Image> bg44 = IW44Image::create_decode(IW44Image::COLOR);
      bg44->decode_chunk(gbs);
      GP<DjVuInfo> info = DjVuInfo::create();
      info->width = bg44->get_width();
      info->height = bg44->get_height();
      info->dpi = 100;
      this->bg44 = bg44;
      this->info = info;
      desc.format( ERR_MSG("DjVuFile.IW44_data1") "\t%d\t%d\t%d",
                   bg44->get_width(), bg44->get_height(),
                   get_dpi(bg44->get_width(), bg44->get_height()));
    } 
    else
    {
      // Refinement chunks
      GP<IW44Image> bg44 = this->bg44;
      bg44->decode_chunk(gbs);
      desc.format( ERR_MSG("DjVuFile.IW44_data2") "\t%d\t%d",
                   bg44->get_serial(),
                   get_dpi(bg44->get_width(), bg44->get_height()));
    }
  }
  
  // NDIR (obsolete navigation chunk)
  else if (chkid == "NDIR")
  {
    GP<DjVuNavDir> dir=DjVuNavDir::create(url);
    dir->decode(bs);
    this->dir=dir;
    desc.format( ERR_MSG("DjVuFile.nav_dir") );
  }
  
  // FORM:ANNO (obsolete) (must be before other annotations)
  else if (chkid == "FORM:ANNO") 
    {
      const GP<ByteStream> gachunk(ByteStream::create());
      ByteStream &achunk=*gachunk;
      achunk.copy(bs);
      achunk.seek(0);
      GCriticalSectionLock lock(&anno_lock);
      if (! anno)
      {
        anno=ByteStream::create();
      }
      anno->seek(0,SEEK_END);
      if (anno->tell())
      {
        anno->write((void*)"", 1);
      }
      // Copy data
      anno->copy(achunk);
      desc.format( ERR_MSG("DjVuFile.anno1") );
    }
  
  // ANTa/ANTx/TXTa/TXTz annotations
  else if (is_annotation(chkid))  // but not FORM:ANNO
    {
      const GP<ByteStream> gachunk(ByteStream::create());
      ByteStream &achunk=*gachunk;
      achunk.copy(bs);
      achunk.seek(0);
      GCriticalSectionLock lock(&anno_lock);
      if (! anno)
      {
        anno = ByteStream::create();
      }
      anno->seek(0,SEEK_END);
      if (anno->tell() & 1)
      {
        anno->write((const void*)"", 1);
      }
      // Recreate chunk header
      const GP<IFFByteStream> giffout(IFFByteStream::create(anno));
      IFFByteStream &iffout=*giffout;
      iffout.put_chunk(id);
      iffout.copy(achunk);
      iffout.close_chunk();
      desc.format( ERR_MSG("DjVuFile.anno2") );
    }
  else if (is_text(chkid))
    {
      const GP<ByteStream> gachunk(ByteStream::create());
      ByteStream &achunk=*gachunk;
      achunk.copy(bs);
      achunk.seek(0);
      GCriticalSectionLock lock(&text_lock);
      if (! text)
      {
        text = ByteStream::create();
      }
      text->seek(0,SEEK_END);
      if (text->tell())
      {
        text->write((const void*)"", 1);
      }
      // Recreate chunk header
      const GP<IFFByteStream> giffout(IFFByteStream::create(text));
      IFFByteStream &iffout=*giffout;
      iffout.put_chunk(id);
      iffout.copy(achunk);
      iffout.close_chunk();
      desc.format( ERR_MSG("DjVuFile.text") );
    }
  else if (is_meta(chkid))
    {
      const GP<ByteStream> gachunk(ByteStream::create());
      ByteStream &achunk=*gachunk;
      achunk.copy(bs);
      achunk.seek(0);
      GCriticalSectionLock lock(&meta_lock);
      if (! meta)
      {
        meta = ByteStream::create();
      }
      meta->seek(0,SEEK_END);
      if (meta->tell())
      {
        meta->write((const void*)"", 1);
      }
      // Recreate chunk header
      const GP<IFFByteStream> giffout(IFFByteStream::create(meta));
      IFFByteStream &iffout=*giffout;
      iffout.put_chunk(id);
      iffout.copy(achunk);
      iffout.close_chunk();
    }
  else if (chkid == "CELX" || chkid == "SINF")
    {
      G_THROW( ERR_MSG("DjVuFile.securedjvu") );
    }
  
  // Return description
  return desc;
}

void
DjVuFile::set_decode_codec(GP<GPixmap> (*codec)(ByteStream &bs))
{
  djvu_decode_codec=codec;
}

void
DjVuFile::decode(const GP<ByteStream> &gbs)
{
  check();
  DEBUG_MSG("DjVuFile::decode(), url='" << url << "'\n");
  DEBUG_MAKE_INDENT(3);
  DjVuPortcaster * pcaster=get_portcaster();
  
  // Get form chunk
  GUTF8String chkid;
  const GP<IFFByteStream> giff(IFFByteStream::create(gbs));
  IFFByteStream &iff=*giff;
  if (!iff.get_chunk(chkid)) 
    REPORT_EOF(true)
    
    // Check file format
  bool djvi = (chkid=="FORM:DJVI")?true:false;
  bool djvu = (chkid=="FORM:DJVU")?true:false;
  bool iw44 = ((chkid=="FORM:PM44") || (chkid=="FORM:BM44"));
  if (djvi || djvu)
    mimetype = "image/x.djvu";
  else if (iw44)
    mimetype = "image/x-iw44";
  else
    G_THROW( ERR_MSG("DjVuFile.unexp_image") );
  
  // Process chunks
  int size_so_far=iff.tell();
  int chunks=0;
  int last_chunk=0;
  G_TRY
  {
    int chunks_left=(recover_errors>SKIP_PAGES)?chunks_number:(-1);
    int chksize;
    for(;(chunks_left--)&&(chksize = iff.get_chunk(chkid));last_chunk=chunks)
    {
      chunks++;

      // Decode and get chunk description
      GUTF8String str = decode_chunk(chkid, iff.get_bytestream(), djvi, djvu, iw44);
      // Add parameters to the chunk description to give the size and chunk id
      GUTF8String desc;
      desc.format("\t%5.1f\t%s", chksize/1024.0, (const char*)chkid);
      // Append the whole thing to the growing file description
      description = description + str + desc + "\n";

      pcaster->notify_chunk_done(this, chkid);
      // Close chunk
      iff.seek_close_chunk();
      // Record file size
      size_so_far=iff.tell();
    }
    if (chunks_number < 0) chunks_number=last_chunk;
  }
  G_CATCH(ex)
  {
    if(!ex.cmp_cause(ByteStream::EndOfFile))
    {
      if (chunks_number < 0)
        chunks_number=(recover_errors>SKIP_CHUNKS)?chunks:last_chunk;
      report_error(ex,(recover_errors <= SKIP_PAGES));
    }else
    {
      report_error(ex,true);
    }
  }
  G_ENDCATCH;
  
  // Record file size
  file_size=size_so_far;
  // Close form chunk
  iff.close_chunk();
  // Close BG44 codec
  if (bg44) 
    bg44->close_codec();
  
  // Complete description
  if (djvu && !info)
    G_THROW( ERR_MSG("DjVuFile.corrupt_missing_info") );
  if (iw44 && !info)
    G_THROW( ERR_MSG("DjVuFile.corrupt_missing_IW44") );
  if (info)
  {
    GUTF8String desc;
    if (djvu || djvi)
      desc.format( ERR_MSG("DjVuFile.djvu_header") "\t%d\t%d\t%d\t%d", 
        info->width, info->height,
        info->dpi, info->version);
    else if (iw44)
      desc.format( ERR_MSG("DjVuFile.IW44_header") "\t%d\t%d\t%d", 
        info->width, info->height, info->dpi);
    description=desc + "\n" + description;
    int rawsize=info->width*info->height*3;
    desc.format( ERR_MSG("DjVuFile.ratio") "\t%0.1f\t%0.1f",
      (double)rawsize/file_size, file_size/1024.0 );
    description=description+desc;
  }
}

void
DjVuFile::start_decode(void)
{
  check();
  DEBUG_MSG("DjVuFile::start_decode(), url='" << url << "'\n");
  DEBUG_MAKE_INDENT(3);
  
  GThread * thread_to_delete=0;
  flags.enter();
  G_TRY {
    if (!(flags & DONT_START_DECODE) && !is_decoding())
    {
      if (flags & DECODE_STOPPED) reset();
      flags&=~(DECODE_OK | DECODE_STOPPED | DECODE_FAILED);
      flags|=DECODING;
      
      // Don't delete the thread while you're owning the flags lock
      // Beware of deadlock!
      thread_to_delete=decode_thread; decode_thread=0;
      
      // We want to create it right here to be able to stop the
      // decoding thread even before its function is called (it starts)
      decode_data_pool=DataPool::create(data_pool);
      decode_life_saver=this;
      
      decode_thread=new GThread();
      decode_thread->create(static_decode_func, this);
    }
  }
  G_CATCH_ALL
  {
    flags&=~DECODING;
    flags|=DECODE_FAILED;
    flags.leave();
    get_portcaster()->notify_file_flags_changed(this, DECODE_FAILED, DECODING);
    delete thread_to_delete;
    G_RETHROW;
  }
  G_ENDCATCH;
  flags.leave();
  delete thread_to_delete;
}

bool
DjVuFile::resume_decode(const bool sync)
{
  bool retval=false;
  {
    GMonitorLock lock(&flags);
    if( !is_decoding() && !is_decode_ok() && !is_decode_failed() )
    {
      start_decode();
      retval=true;
    }
  }
  if(sync)
  {
    wait_for_finish();
  }
  return retval;
}

void
DjVuFile::stop_decode(bool sync)
{
  check();
  
  DEBUG_MSG("DjVuFile::stop_decode(), url='" << url <<
    "', sync=" << (int) sync << "\n");
  DEBUG_MAKE_INDENT(3);
  
  G_TRY
  {
    flags|=DONT_START_DECODE;
    
    // Don't stop SYNCHRONOUSLY from the thread where the decoding is going!!!
    {
      // First - ask every included child to stop in async mode
      GCriticalSectionLock lock(&inc_files_lock);
      for(GPosition pos=inc_files_list;pos;++pos)
        inc_files_list[pos]->stop_decode(0);
      
//      if (decode_data_pool) decode_data_pool->stop();
    }
    
    if (sync)
    {
      while(1)
      {
        GP<DjVuFile> file;
        {
          GCriticalSectionLock lock(&inc_files_lock);
          for(GPosition pos=inc_files_list;pos;++pos)
          {
            GP<DjVuFile> & f=inc_files_list[pos];
            if (f->is_decoding())
            {
              file=f; break;
            }
          }
        }
        if (!file) break;
        
        file->stop_decode(1);
      }
      
      wait_for_finish(1);	// Wait for self termination
      
      // Don't delete the thread here. Until GPBase::preserve() is
      // reimplemented somehow at the GThread level.
      // delete decode_thread; decode_thread=0;
    }
    flags&=~(DONT_START_DECODE);
  } G_CATCH_ALL {
    flags&=~(DONT_START_DECODE);
    G_RETHROW;
  } G_ENDCATCH;
}

void
DjVuFile::stop(bool only_blocked)
// This is a one-way function. There is no way to undo the stop()
// command.
{
  DEBUG_MSG("DjVuFile::stop(): Stopping everything\n");
  DEBUG_MAKE_INDENT(3);
  
  flags|=only_blocked ? BLOCKED_STOPPED : STOPPED;
  if (data_pool) data_pool->stop(only_blocked);
  GCriticalSectionLock lock(&inc_files_lock);
  for(GPosition pos=inc_files_list;pos;++pos)
    inc_files_list[pos]->stop(only_blocked);
}

GP<DjVuNavDir>
DjVuFile::find_ndir(GMap<GURL, void *> & map)
{
  check();
  
  DEBUG_MSG("DjVuFile::find_ndir(): looking for NDIR in '" << url << "'\n");
  DEBUG_MAKE_INDENT(3);
  
  if (dir) return dir;
  
  if (!map.contains(url))
  {
    map[url]=0;
    
    GPList<DjVuFile> list=get_included_files(false);
    for(GPosition pos=list;pos;++pos)
    {
      GP<DjVuNavDir> d=list[pos]->find_ndir(map);
      if (d) return d;
    }
  }
  return 0;
}

GP<DjVuNavDir>
DjVuFile::find_ndir(void)
{
  GMap<GURL, void *> map;
  return find_ndir(map);
}

GP<DjVuNavDir>
DjVuFile::decode_ndir(GMap<GURL, void *> & map)
{
  check();
  
  DEBUG_MSG("DjVuFile::decode_ndir(): decoding for NDIR in '" << url << "'\n");
  DEBUG_MAKE_INDENT(3);
  
  if (dir) return dir;
  
  if (!map.contains(url))
  {
    map[url]=0;
    
    const GP<ByteStream> str(data_pool->get_stream());
    
    GUTF8String chkid;
    const GP<IFFByteStream> giff(IFFByteStream::create(str));
    IFFByteStream &iff=*giff;
    if (!iff.get_chunk(chkid)) 
      REPORT_EOF(true)

    int chunks=0;
    int last_chunk=0;
#ifndef SLOW_BUT_EXACT_DETECTION_OF_NDIR
    int found_incl=0;
#endif
    G_TRY
    {
      int chunks_left=(recover_errors>SKIP_PAGES)?chunks_number:(-1);
      int chksize;
      for(;(chunks_left--)&&(chksize=iff.get_chunk(chkid));last_chunk=chunks)
      {
        chunks++;
        if (chkid=="NDIR")
        {
          GP<DjVuNavDir> d=DjVuNavDir::create(url);
          d->decode(*iff.get_bytestream());
          dir=d;
          break;
        }
#ifndef SLOW_BUT_EXACT_DETECTION_OF_NDIR
        if (chkid=="INCL")
          found_incl = 1;
        if (chunks>2 && !found_incl && !data_pool->is_eof())
          return 0;
#endif
        iff.seek_close_chunk();
      }
      if ((!dir)&&(chunks_number < 0)) chunks_number=last_chunk;
    }
    G_CATCH(ex)
    {
       if(!ex.cmp_cause(ByteStream::EndOfFile))
       {
          if (chunks_number < 0)
             chunks_number=(recover_errors>SKIP_CHUNKS)?chunks:last_chunk;
          report_error(ex,(recover_errors<=SKIP_PAGES));
       }else
       {
          report_error(ex,true);
       }
    }
    G_ENDCATCH;
    
    data_pool->clear_stream();
    if (dir) return dir;
    
    GPList<DjVuFile> list=get_included_files(false);
    for(GPosition pos=list;pos;++pos)
    {
      GP<DjVuNavDir> d=list[pos]->decode_ndir(map);
      if (d) return d;
    }
    data_pool->clear_stream();
  }
  return 0;
}

GP<DjVuNavDir>
DjVuFile::decode_ndir(void)
{
  GMap<GURL, void *> map;
  return decode_ndir(map);
}

void
DjVuFile::get_merged_anno(const GP<DjVuFile> & file,
  const GP<ByteStream> &gstr_out, const GList<GURL> & ignore_list,
  int level, int & max_level, GMap<GURL, void *> & map)
{
  DEBUG_MSG("DjVuFile::get_merged_anno()\n");
  GURL url=file->get_url();
  if (!map.contains(url))
  {
    ByteStream &str_out=*gstr_out;
    map[url]=0;

    // Do the included files first (To make sure that they have
    // less precedence)
    // Depending on if we have all data present, we will
    // either create all included files or will use only
    // those that have already been created
    GPList<DjVuFile> list=file->get_included_files(!file->is_data_present());
    for(GPosition pos=list;pos;++pos)
      get_merged_anno(list[pos], gstr_out, ignore_list, level+1, max_level, map);

    // Now process the DjVuFile's own annotations
    if (!ignore_list.contains(file->get_url()))
      {
	if (!file->is_data_present() ||
	    (file->is_modified() && file->anno))
	  {
	    // Process the decoded (?) anno
	    GCriticalSectionLock lock(&file->anno_lock);
	    if (file->anno && file->anno->size())
	      {
		if (str_out.tell())
		  {
		    str_out.write((void *) "", 1);
		  }
		file->anno->seek(0);
		str_out.copy(*file->anno);
	      }
	  } else if (file->is_data_present())
	  {
	    // Copy all annotations chunks, but do NOT modify
	    // this->anno (to avoid correlation with DjVuFile::decode())
	    const GP<ByteStream> str(file->data_pool->get_stream());
	    const GP<IFFByteStream> giff(IFFByteStream::create(str));
	    IFFByteStream &iff=*giff;
	    GUTF8String chkid;
	    if (iff.get_chunk(chkid))
	      while(iff.get_chunk(chkid))
		{
		  if (chkid=="FORM:ANNO")
		    {
		      if (max_level<level)
			max_level=level;
		      if (str_out.tell())
			{
			  str_out.write((void *) "", 1);
			}
		      str_out.copy(*iff.get_bytestream());
		    }
		  else if (is_annotation(chkid)) // but not FORM:ANNO
		    {
		      if (max_level<level)
			max_level=level;
		      if (str_out.tell()&&chkid != "ANTz")
			{
			  str_out.write((void *) "", 1);
			}
		      const GP<IFFByteStream> giff_out(IFFByteStream::create(gstr_out));
		      IFFByteStream &iff_out=*giff_out;
		      iff_out.put_chunk(chkid);
		      iff_out.copy(*iff.get_bytestream());
		      iff_out.close_chunk();
		    }
		  iff.close_chunk();
		}
	    file->data_pool->clear_stream();
	  }
      }
  }
}

GP<ByteStream>
DjVuFile::get_merged_anno(const GList<GURL> & ignore_list,
                          int * max_level_ptr)
                          // Will do the same thing as get_merged_anno(int *), but will
                          // ignore DjVuFiles with URLs from the ignore_list
{
  DEBUG_MSG("DjVuFile::get_merged_anno()\n");
  GP<ByteStream> gstr(ByteStream::create());
  GMap<GURL, void *> map;
  int max_level=0;
  get_merged_anno(this, gstr, ignore_list, 0, max_level, map);
  if (max_level_ptr)
    *max_level_ptr=max_level;
  ByteStream &str=*gstr;
  if (!str.tell()) 
  {
    gstr=0;
  }else
  {
    str.seek(0);
  }
  return gstr;
}

GP<ByteStream>
DjVuFile::get_merged_anno(int * max_level_ptr)
// Will go down the DjVuFile's hierarchy and decode all DjVuAnno even
// when the DjVuFile is not fully decoded yet. To avoid correlations
// with DjVuFile::decode(), we do not modify DjVuFile::anno data.
//
// Files deeper in the hierarchy have less influence on the
// results. It means, for example, that the if annotations are
// specified in the top level page file and in a shared file,
// the top level page file settings will take precedence.
//
// NOTE! This function guarantees correct results only if the
// DjVuFile has all data
{
  GList<GURL> ignore_list;
  return get_merged_anno(ignore_list, max_level_ptr);
}


// [LB->BCR] The following six functions get_anno, get_text, get_meta 
// contain the same code in triplicate!!!

void
DjVuFile::get_anno(
  const GP<DjVuFile> & file, const GP<ByteStream> &gstr_out)
{
  DEBUG_MSG("DjVuFile::get_anno()\n");
  ByteStream &str_out=*gstr_out;
  if (!file->is_data_present() ||
      (file->is_modified() && file->anno))
    {
      // Process the decoded (?) anno
      GCriticalSectionLock lock(&file->anno_lock);
      if (file->anno && file->anno->size())
	{
	  if (str_out.tell())
	    {
	      str_out.write((void *) "", 1);
	    }
	  file->anno->seek(0);
	  str_out.copy(*file->anno);
	}
    } else if (file->is_data_present())
    {
      // Copy all anno chunks, but do NOT modify
      // DjVuFile::anno (to avoid correlation with DjVuFile::decode())
      const GP<ByteStream> str=file->data_pool->get_stream();
      const GP<IFFByteStream> giff=IFFByteStream::create(str);
      IFFByteStream &iff=*giff;
      GUTF8String chkid;
      if (iff.get_chunk(chkid))
	{
	  while(iff.get_chunk(chkid))
	    {
	      if (is_annotation(chkid))
		{
		  if (str_out.tell())
		    {
		      str_out.write((void *) "", 1);
		    }
		  const GP<IFFByteStream> giff_out(IFFByteStream::create(gstr_out));
		  IFFByteStream &iff_out=*giff_out;
		  iff_out.put_chunk(chkid);
		  iff_out.copy(*iff.get_bytestream());
		  iff_out.close_chunk();
		}
	      iff.close_chunk();
	    }
	}
      file->data_pool->clear_stream();
    }
}

void
DjVuFile::get_text(
  const GP<DjVuFile> & file, const GP<ByteStream> &gstr_out)
{
  DEBUG_MSG("DjVuFile::get_text()\n");
  ByteStream &str_out=*gstr_out;
  if (!file->is_data_present() ||
      (file->is_modified() && file->text))
    {
      // Process the decoded (?) text
      GCriticalSectionLock lock(&file->text_lock);
      if (file->text && file->text->size())
	{
	  if (str_out.tell())
	    {
	      str_out.write((void *) "", 1);
	    }
	  file->text->seek(0);
	  str_out.copy(*file->text);
	}
    } else if (file->is_data_present())
    {
      // Copy all text chunks, but do NOT modify
      // DjVuFile::text (to avoid correlation with DjVuFile::decode())
      const GP<ByteStream> str=file->data_pool->get_stream();
      const GP<IFFByteStream> giff=IFFByteStream::create(str);
      IFFByteStream &iff=*giff;
      GUTF8String chkid;
      if (iff.get_chunk(chkid))
	{
	  while(iff.get_chunk(chkid))
	    {
	      if (is_text(chkid))
		{
		  if (str_out.tell())
		    {
		      str_out.write((void *) "", 1);
		    }
		  const GP<IFFByteStream> giff_out(IFFByteStream::create(gstr_out));
		  IFFByteStream &iff_out=*giff_out;
		  iff_out.put_chunk(chkid);
		  iff_out.copy(*iff.get_bytestream());
		  iff_out.close_chunk();
		}
	      iff.close_chunk();
	    }
	}
      file->data_pool->clear_stream();
    }
}

void
DjVuFile::get_meta(
  const GP<DjVuFile> & file, const GP<ByteStream> &gstr_out)
{
  DEBUG_MSG("DjVuFile::get_meta()\n");
  ByteStream &str_out=*gstr_out;
  if (!file->is_data_present() ||
      (file->is_modified() && file->meta))
    {
      // Process the decoded (?) meta
      GCriticalSectionLock lock(&file->meta_lock);
      if (file->meta && file->meta->size())
	{
	  if (str_out.tell())
	    {
	      str_out.write((void *) "", 1);
	    }
	  file->meta->seek(0);
	  str_out.copy(*file->meta);
	}
    } else if (file->is_data_present())
    {
      // Copy all meta chunks, but do NOT modify
      // DjVuFile::meta (to avoid correlation with DjVuFile::decode())
      const GP<ByteStream> str=file->data_pool->get_stream();
      const GP<IFFByteStream> giff=IFFByteStream::create(str);
      IFFByteStream &iff=*giff;
      GUTF8String chkid;
      if (iff.get_chunk(chkid))
	{
	  while(iff.get_chunk(chkid))
	    {
	      if (is_meta(chkid))
		{
		  if (str_out.tell())
		    {
		      str_out.write((void *) "", 1);
		    }
		  const GP<IFFByteStream> giff_out(IFFByteStream::create(gstr_out));
		  IFFByteStream &iff_out=*giff_out;
		  iff_out.put_chunk(chkid);
		  iff_out.copy(*iff.get_bytestream());
		  iff_out.close_chunk();
		}
	      iff.close_chunk();
	    }
	}
      file->data_pool->clear_stream();
    }
}

GP<ByteStream>
DjVuFile::get_anno(void)
{
  DEBUG_MSG("DjVuFile::get_text(void)\n");
  GP<ByteStream> gstr(ByteStream::create());
  get_anno(this, gstr);
  ByteStream &str=*gstr;
  if (!str.tell())
  { 
    gstr=0;
  }else
  {
    str.seek(0);
  }
  return gstr;
}

GP<ByteStream>
DjVuFile::get_text(void)
{
  DEBUG_MSG("DjVuFile::get_text(void)\n");
  GP<ByteStream> gstr(ByteStream::create());
  get_text(this, gstr);
  ByteStream &str=*gstr;
  if (!str.tell())
  { 
    gstr=0;
  }else
  {
    str.seek(0);
  }
  return gstr;
}

GP<ByteStream>
DjVuFile::get_meta(void)
{
  DEBUG_MSG("DjVuFile::get_meta(void)\n");
  GP<ByteStream> gstr(ByteStream::create());
  get_meta(this, gstr);
  ByteStream &str=*gstr;
  if (!str.tell())
  { 
    gstr=0;
  }else
  {
    str.seek(0);
  }
  return gstr;
}

void
DjVuFile::static_trigger_cb(void * cl_data)
{
  DjVuFile * th=(DjVuFile *) cl_data;
  G_TRY {
    GP<DjVuPort> port=DjVuPort::get_portcaster()->is_port_alive(th);
    if (port && port->inherits("DjVuFile"))
      ((DjVuFile *) (DjVuPort *) port)->trigger_cb();
  } G_CATCH(exc) {
    G_TRY {
      get_portcaster()->notify_error(th, exc.get_cause());
    } G_CATCH_ALL {} G_ENDCATCH;
  } G_ENDCATCH;
}

void
DjVuFile::trigger_cb(void)
{
  GP<DjVuFile> life_saver=this;
  
  DEBUG_MSG("DjVuFile::trigger_cb(): got data for '" << url << "'\n");
  DEBUG_MAKE_INDENT(3);
  
  file_size=data_pool->get_length();
  flags|=DATA_PRESENT;
  get_portcaster()->notify_file_flags_changed(this, DATA_PRESENT, 0);
  
  if (!are_incl_files_created())
    process_incl_chunks();
  
  bool all=true;
  inc_files_lock.lock();
  GPList<DjVuFile> files_list=inc_files_list;
  inc_files_lock.unlock();
  for(GPosition pos=files_list;pos&&(all=files_list[pos]->is_all_data_present());++pos)
    EMPTY_LOOP;
  if (all)
  {
    DEBUG_MSG("DjVuFile::trigger_cb(): We have ALL data for '" << url << "'\n");
    flags|=ALL_DATA_PRESENT;
    get_portcaster()->notify_file_flags_changed(this, ALL_DATA_PRESENT, 0);
  }
}

void
DjVuFile::progress_cb(int pos, void * cl_data)
{
  DEBUG_MSG("DjVuFile::progress_cb() called\n");
  DEBUG_MAKE_INDENT(3);
  
  DjVuFile * th=(DjVuFile *) cl_data;
  
  int length=th->decode_data_pool->get_length();
  if (length>0)
  {
    float progress=(float) pos/length;
    DEBUG_MSG("progress=" << progress << "\n");
    get_portcaster()->notify_decode_progress(th, progress);
  } else
  {
    DEBUG_MSG("DataPool size is still unknown => ignoring\n");
  }
}

//*****************************************************************************
//******************************** Utilities **********************************
//*****************************************************************************

void
DjVuFile::move(GMap<GURL, void *> & map, const GURL & dir_url)
// This function may block for data.
{
  if (!map.contains(url))
  {
    map[url]=0;
    
    url=GURL::UTF8(url.name(),dir_url);
    
    
    // Leave the lock here!
    GCriticalSectionLock lock(&inc_files_lock);
    for(GPosition pos=inc_files_list;pos;++pos)
      inc_files_list[pos]->move(map, dir_url);
  }
}

void
DjVuFile::move(const GURL & dir_url)
// This function may block for data.
{
  check();
  DEBUG_MSG("DjVuFile::move(): dir_url='" << dir_url << "'\n");
  DEBUG_MAKE_INDENT(3);
  
  GMap<GURL, void *> map;
  move(map, dir_url);
}

void
DjVuFile::set_name(const GUTF8String &name)
{
  DEBUG_MSG("DjVuFile::set_name(): name='" << name << "'\n");
  DEBUG_MAKE_INDENT(3);
  url=GURL::UTF8(name,url.base());
}

//*****************************************************************************
//****************************** Data routines ********************************
//*****************************************************************************

int
DjVuFile::get_chunks_number(void)
{
  if(chunks_number < 0)
  {
    const GP<ByteStream> str(data_pool->get_stream());
    GUTF8String chkid;
    const GP<IFFByteStream> giff(IFFByteStream::create(str));
    IFFByteStream &iff=*giff;
    if (!iff.get_chunk(chkid))
      REPORT_EOF(true)
      
      int chunks=0;
    int last_chunk=0;
    G_TRY
    {
      int chksize;
      for(;(chksize=iff.get_chunk(chkid));last_chunk=chunks)
      {
        chunks++;
        iff.seek_close_chunk();
      }
      chunks_number=last_chunk;
    }
    G_CATCH(ex)
    {
      chunks_number=(recover_errors>SKIP_CHUNKS)?chunks:last_chunk;
      report_error(ex,(recover_errors<=SKIP_PAGES));
    }
    G_ENDCATCH;
    data_pool->clear_stream();
  }
  return chunks_number;
}

GUTF8String
DjVuFile::get_chunk_name(int chunk_num)
{
  if(chunk_num < 0)
  {
    G_THROW( ERR_MSG("DjVuFile.illegal_chunk") );
  }
  if((chunks_number >= 0)&&(chunk_num > chunks_number))
  {
    G_THROW( ERR_MSG("DjVuFile.missing_chunk") );
  }
  check();
  
  GUTF8String name;
  const GP<ByteStream> str(data_pool->get_stream());
  GUTF8String chkid;
  const GP<IFFByteStream> giff(IFFByteStream::create(str));
  IFFByteStream &iff=*giff;
  if (!iff.get_chunk(chkid)) 
    REPORT_EOF(true)
    
    int chunks=0;
  int last_chunk=0;
  G_TRY
  {
    int chunks_left=(recover_errors>SKIP_PAGES)?chunks_number:(-1);
    int chksize;
    for(;(chunks_left--)&&(chksize=iff.get_chunk(chkid));last_chunk=chunks)
    {
      if (chunks++==chunk_num) { name=chkid; break; }
      iff.seek_close_chunk();
    }
  }
  G_CATCH(ex)
  {
    if (chunks_number < 0)
      chunks_number=(recover_errors>SKIP_CHUNKS)?chunks:last_chunk;
    report_error(ex,(recover_errors <= SKIP_PAGES));
  }
  G_ENDCATCH;
  if (!name.length())
  {
    if (chunks_number < 0) chunks_number=chunks;
    G_THROW( ERR_MSG("DjVuFile.missing_chunk") );
  }
  return name;
}

bool
DjVuFile::contains_chunk(const GUTF8String &chunk_name)
{
  check();
  DEBUG_MSG("DjVuFile::contains_chunk(): url='" << url << "', chunk_name='" <<
    chunk_name << "'\n");
  DEBUG_MAKE_INDENT(3);
  
  bool contains=0;
  const GP<ByteStream> str(data_pool->get_stream());
  GUTF8String chkid;
  const GP<IFFByteStream> giff(IFFByteStream::create(str));
  IFFByteStream &iff=*giff;
  if (!iff.get_chunk(chkid)) 
    REPORT_EOF((recover_errors<=SKIP_PAGES))
    
    int chunks=0;
  int last_chunk=0;
  G_TRY
  {
    int chunks_left=(recover_errors>SKIP_PAGES)?chunks_number:(-1);
    int chksize;
    for(;(chunks_left--)&&(chksize=iff.get_chunk(chkid));last_chunk=chunks)
    {
      chunks++;
      if (chkid==chunk_name) { contains=1; break; }
      iff.seek_close_chunk();
    }
    if (!contains &&(chunks_number < 0)) chunks_number=last_chunk;
  }
  G_CATCH(ex)
  {
    if (chunks_number < 0)
      chunks_number=(recover_errors>SKIP_CHUNKS)?chunks:last_chunk;
    report_error(ex,(recover_errors <= SKIP_PAGES));
  }
  G_ENDCATCH;
  data_pool->clear_stream();
  return contains;
}

bool
DjVuFile::contains_anno(void)
{
  const GP<ByteStream> str(data_pool->get_stream());
  
  GUTF8String chkid;
  const GP<IFFByteStream> giff(IFFByteStream::create(str));
  IFFByteStream &iff=*giff;
  if (!iff.get_chunk(chkid))
    G_THROW( ByteStream::EndOfFile );
  
  while(iff.get_chunk(chkid))
  {
    if (is_annotation(chkid))
      return true;
    iff.close_chunk();
  }
  
  data_pool->clear_stream();
  return false;
}

bool
DjVuFile::contains_text(void)
{
  const GP<ByteStream> str(data_pool->get_stream());
  
  GUTF8String chkid;
  const GP<IFFByteStream> giff(IFFByteStream::create(str));
  IFFByteStream &iff=*giff;
  if (!iff.get_chunk(chkid))
    G_THROW( ByteStream::EndOfFile );
  
  while(iff.get_chunk(chkid))
  {
    if (is_text(chkid))
      return true;
    iff.close_chunk();
  }
  
  data_pool->clear_stream();
  return false;
}

bool
DjVuFile::contains_meta(void)
{
  const GP<ByteStream> str(data_pool->get_stream());
  
  GUTF8String chkid;
  const GP<IFFByteStream> giff(IFFByteStream::create(str));
  IFFByteStream &iff=*giff;
  if (!iff.get_chunk(chkid))
    G_THROW( ByteStream::EndOfFile );
  
  while(iff.get_chunk(chkid))
  {
    if (is_meta(chkid))
      return true;
    iff.close_chunk();
  }
  
  data_pool->clear_stream();
  return false;
}

//*****************************************************************************
//****************************** Save routines ********************************
//*****************************************************************************

static void
copy_chunks(const GP<ByteStream> &from, IFFByteStream &ostr)
{
  from->seek(0);
  const GP<IFFByteStream> giff(IFFByteStream::create(from));
  IFFByteStream &iff=*giff;
  GUTF8String chkid;
  int chksize;
  while ((chksize=iff.get_chunk(chkid)))
  {
    ostr.put_chunk(chkid);
    int ochksize=ostr.copy(*iff.get_bytestream());
    ostr.close_chunk();
    iff.seek_close_chunk();
    if(ochksize != chksize)
    {
      G_THROW( ByteStream::EndOfFile );
    }
  }
}


void
DjVuFile::add_djvu_data(IFFByteStream & ostr, GMap<GURL, void *> & map,
                        const bool included_too, const bool no_ndir)
{
  check();
  if (map.contains(url)) return;
  bool top_level = !map.size();
  map[url]=0;
  bool processed_annotation = false;
  bool processed_text = false;
  bool processed_meta = false;
  
  const GP<ByteStream> str(data_pool->get_stream());
  GUTF8String chkid;
  const GP<IFFByteStream> giff(IFFByteStream::create(str));
  IFFByteStream &iff=*giff;
  if (!iff.get_chunk(chkid)) 
    REPORT_EOF(true)
    
    // Open toplevel form
    if (top_level) 
      ostr.put_chunk(chkid);
    // Process chunks
    int chunks=0;
    int last_chunk=0;
    G_TRY
    {
      int chunks_left=(recover_errors>SKIP_PAGES)?chunks_number:(-1);
      int chksize;
      for(;(chunks_left--)&&(chksize = iff.get_chunk(chkid));last_chunk=chunks)
      {
        chunks++;
        if (is_info(chkid) && info)
        {
          ostr.put_chunk(chkid);
          info->encode(*ostr.get_bytestream());
          ostr.close_chunk();
        }
        else if (chkid=="INCL" && included_too)
        {
          GP<DjVuFile> file = process_incl_chunk(*iff.get_bytestream());
          if (file)
          {
            if(recover_errors!=ABORT)
              file->set_recover_errors(recover_errors);
            if(verbose_eof)
              file->set_verbose_eof(verbose_eof);
            file->add_djvu_data(ostr, map, included_too, no_ndir);
          }
        } 
        else if (is_annotation(chkid) && anno && anno->size())
        {
          if (!processed_annotation)
          {
            processed_annotation = true;
            GCriticalSectionLock lock(&anno_lock);
            copy_chunks(anno, ostr);
          }
        }
        else if (is_text(chkid) && text && text->size())
        {
          if (!processed_text)
          {
            processed_text = true;
            GCriticalSectionLock lock(&text_lock);
            copy_chunks(text, ostr);
          }
        }
        else if (is_meta(chkid) && meta && meta->size())
        {
          if (!processed_meta)
          {
            processed_meta = true;
            GCriticalSectionLock lock(&meta_lock);
            copy_chunks(meta, ostr);
          }
        }
        else if (chkid!="NDIR"||!(no_ndir || dir))
        {  // Copy NDIR chunks, but never generate new ones.
          ostr.put_chunk(chkid);
          ostr.copy(*iff.get_bytestream());
          ostr.close_chunk();
        }
        iff.seek_close_chunk();
      }
      if (chunks_number < 0) chunks_number=last_chunk;
    }
    G_CATCH(ex)
    {
      if(!ex.cmp_cause(ByteStream::EndOfFile))
      {
        if (chunks_number < 0)
          chunks_number=(recover_errors>SKIP_CHUNKS)?chunks:last_chunk;
        report_error(ex,(recover_errors<=SKIP_PAGES));
      }else
      {
        report_error(ex,true);
      }
    }
    G_ENDCATCH;
    
    // Otherwise, writes annotation at the end (annotations could be big)
    if (!processed_annotation && anno && anno->size())
    {
      processed_annotation = true;
      GCriticalSectionLock lock(&anno_lock);
      copy_chunks(anno, ostr);
    }
    if (!processed_text && text && text->size())
    {
      processed_text = true;
      GCriticalSectionLock lock(&text_lock);
      copy_chunks(text, ostr);
    }
    if (!processed_meta && meta && meta->size())
    {
      processed_meta = true;
      GCriticalSectionLock lock(&meta_lock);
      copy_chunks(meta, ostr);
    }
    // Close iff
    if (top_level) 
      ostr.close_chunk();

  data_pool->clear_stream();
}

GP<ByteStream>  
DjVuFile::get_djvu_bytestream(const bool included_too, const bool no_ndir)
{
   check();
   DEBUG_MSG("DjVuFile::get_djvu_bytestream(): creating DjVu raw file\n");
   DEBUG_MAKE_INDENT(3);
   const GP<ByteStream> pbs(ByteStream::create());
   const GP<IFFByteStream> giff=IFFByteStream::create(pbs);
   IFFByteStream &iff=*giff;
   GMap<GURL, void *> map;
   add_djvu_data(iff, map, included_too, no_ndir);
   iff.flush();
   pbs->seek(0, SEEK_SET);
   return pbs;
}

GP<DataPool>
DjVuFile::get_djvu_data(const bool included_too, const bool no_ndir)
{
  const GP<ByteStream> pbs = get_djvu_bytestream(included_too, no_ndir);
  return DataPool::create(pbs);
}

void
DjVuFile::merge_anno(ByteStream &out)
{
  // Reuse get_merged_anno(), which is better than the previous
  // implementation due to three things:
  //  1. It works even before the file is completely decoded
  //  2. It merges annotations taking into account where a child DjVuFile
  //     is included.
  //  3. It handles loops in DjVuFile's hierarchy
  
  const GP<ByteStream> str(get_merged_anno());
  if (str)
  {
    str->seek(0);
    if (out.tell())
    {
      out.write((void *) "", 1);
    }
    out.copy(*str);
  }
}

void
DjVuFile::get_text(ByteStream &out)
{
  const GP<ByteStream> str(get_text());
  if (str)
  {
    str->seek(0);
    if (out.tell())
    {
      out.write((void *) "", 1);
    }
    out.copy(*str);
  }
}

void
DjVuFile::get_meta(ByteStream &out)
{
  const GP<ByteStream> str(get_meta());
  if (str)
  {
    str->seek(0);
    if (out.tell())
    {
      out.write((void *) "", 1);
    }
    out.copy(*str);
  }
}



//****************************************************************************
//******************************* Modifying **********************************
//****************************************************************************

void
DjVuFile::remove_anno(void)
{
  DEBUG_MSG("DjVuFile::remove_anno()\n");
  const GP<ByteStream> str_in(data_pool->get_stream());
  const GP<ByteStream> gstr_out(ByteStream::create());
  
  GUTF8String chkid;
  const GP<IFFByteStream> giff_in(IFFByteStream::create(str_in));
  IFFByteStream &iff_in=*giff_in;
  if (!iff_in.get_chunk(chkid))
    G_THROW( ByteStream::EndOfFile );
  
  const GP<IFFByteStream> giff_out(IFFByteStream::create(gstr_out));
  IFFByteStream &iff_out=*giff_out;
  iff_out.put_chunk(chkid);
  
  while(iff_in.get_chunk(chkid))
  {
    if (!is_annotation(chkid))
    {
      iff_out.put_chunk(chkid);
      iff_out.copy(*iff_in.get_bytestream());
      iff_out.close_chunk();
    }
    iff_in.close_chunk();
  }
  
  iff_out.close_chunk();
  
  gstr_out->seek(0, SEEK_SET);
  data_pool=DataPool::create(gstr_out);
  chunks_number=-1;
  
  anno=0;
  
  flags|=MODIFIED;
  data_pool->clear_stream();
}

void
DjVuFile::remove_text(void)
{
  DEBUG_MSG("DjVuFile::remove_text()\n");
  const GP<ByteStream> str_in(data_pool->get_stream());
  const GP<ByteStream> gstr_out(ByteStream::create());
  
  GUTF8String chkid;
  const GP<IFFByteStream> giff_in(IFFByteStream::create(str_in));
  IFFByteStream &iff_in=*giff_in;
  if (!iff_in.get_chunk(chkid))
    G_THROW( ByteStream::EndOfFile );
  
  const GP<IFFByteStream> giff_out(IFFByteStream::create(gstr_out));
  IFFByteStream &iff_out=*giff_out;
  iff_out.put_chunk(chkid);
  
  while(iff_in.get_chunk(chkid))
  {
    if (!is_text(chkid))
    {
      iff_out.put_chunk(chkid);
      iff_out.copy(*iff_in.get_bytestream());
      iff_out.close_chunk();
    }
    iff_in.close_chunk();
  }
  
  iff_out.close_chunk();
  
  gstr_out->seek(0, SEEK_SET);
  data_pool=DataPool::create(gstr_out);
  chunks_number=-1;
  
  text=0;
  
  flags|=MODIFIED;
  data_pool->clear_stream();
}

void
DjVuFile::remove_meta(void)
{
  DEBUG_MSG("DjVuFile::remove_meta()\n");
  const GP<ByteStream> str_in(data_pool->get_stream());
  const GP<ByteStream> gstr_out(ByteStream::create());
  
  GUTF8String chkid;
  const GP<IFFByteStream> giff_in(IFFByteStream::create(str_in));
  IFFByteStream &iff_in=*giff_in;
  if (!iff_in.get_chunk(chkid))
    G_THROW( ByteStream::EndOfFile );
  
  const GP<IFFByteStream> giff_out(IFFByteStream::create(gstr_out));
  IFFByteStream &iff_out=*giff_out;
  iff_out.put_chunk(chkid);
  
  while(iff_in.get_chunk(chkid))
  {
    if (!is_meta(chkid))
    {
      iff_out.put_chunk(chkid);
      iff_out.copy(*iff_in.get_bytestream());
      iff_out.close_chunk();
    }
    iff_in.close_chunk();
  }
  
  iff_out.close_chunk();
  
  gstr_out->seek(0, SEEK_SET);
  data_pool=DataPool::create(gstr_out);
  chunks_number=-1;
  
  meta=0;
  
  flags|=MODIFIED;
  data_pool->clear_stream();
}

void
DjVuFile::rebuild_data_pool(void)
{
  data_pool=get_djvu_data(false,false);
  chunks_number=1;
  flags|=MODIFIED;
}

// Do NOT comment this function out. It's used by DjVuDocEditor to convert
// old-style DjVu documents to BUNDLED format.

GP<DataPool>
DjVuFile::unlink_file(const GP<DataPool> & data, const GUTF8String &name)
// Will process contents of data[] and remove any INCL chunk
// containing 'name'
{
  DEBUG_MSG("DjVuFile::unlink_file()\n");
  const GP<ByteStream> gstr_out(ByteStream::create());
  const GP<IFFByteStream> giff_out(IFFByteStream::create(gstr_out));
  IFFByteStream &iff_out=*giff_out;
  
  const GP<ByteStream> str_in(data->get_stream());
  const GP<IFFByteStream> giff_in(IFFByteStream::create(str_in));
  IFFByteStream &iff_in=*giff_in;
  
  int chksize;
  GUTF8String chkid;
  if (!iff_in.get_chunk(chkid)) return data;
  
  iff_out.put_chunk(chkid);
  
  while((chksize=iff_in.get_chunk(chkid)))
  {
    if (chkid=="INCL")
    {
      GUTF8String incl_str;
      char buffer[1024];
      int length;
      while((length=iff_in.read(buffer, 1024)))
        incl_str+=GUTF8String(buffer, length);
      
      // Eat '\n' in the beginning and at the end
      while(incl_str.length() && incl_str[0]=='\n')
      {
        incl_str=incl_str.substr(1,(unsigned int)(-1));
      }
      while(incl_str.length()>0 && incl_str[(int)incl_str.length()-1]=='\n')
      {
        incl_str.setat(incl_str.length()-1, 0);
      }
      if (incl_str!=name)
      {
        iff_out.put_chunk(chkid);
        iff_out.get_bytestream()->writestring(incl_str);
        iff_out.close_chunk();
      }
    } else
    {
      iff_out.put_chunk(chkid);
      char buffer[1024];
      int length;
      for(const GP<ByteStream> gbs(iff_out.get_bytestream());
        (length=iff_in.read(buffer, 1024));)
      {
        gbs->writall(buffer, length);
      }
      iff_out.close_chunk();
    }
    iff_in.close_chunk();
  }
  iff_out.close_chunk();
  iff_out.flush();
  gstr_out->seek(0, SEEK_SET);
  data->clear_stream();
  return DataPool::create(gstr_out);
}

#ifndef NEED_DECODER_ONLY
void
DjVuFile::insert_file(const GUTF8String &id, int chunk_num)
{
  DEBUG_MSG("DjVuFile::insert_file(): id='" << id << "', chunk_num="
    << chunk_num << "\n");
  DEBUG_MAKE_INDENT(3);
  
  // First: create new data
  const GP<ByteStream> str_in(data_pool->get_stream());
  const GP<IFFByteStream> giff_in(IFFByteStream::create(str_in));
  IFFByteStream &iff_in=*giff_in;
  
  const GP<ByteStream> gstr_out(ByteStream::create());
  const GP<IFFByteStream> giff_out(IFFByteStream::create(gstr_out));
  IFFByteStream &iff_out=*giff_out;
  
  int chunk_cnt=0;
  bool done=false;
  GUTF8String chkid;
  if (iff_in.get_chunk(chkid))
  {
    iff_out.put_chunk(chkid);
    int chksize;
    while((chksize=iff_in.get_chunk(chkid)))
    {
      if (chunk_cnt++==chunk_num)
      {
        iff_out.put_chunk("INCL");
        iff_out.get_bytestream()->writestring(id);
        iff_out.close_chunk();
        done=true;
      }
      iff_out.put_chunk(chkid);
      iff_out.copy(*iff_in.get_bytestream());
      iff_out.close_chunk();
      iff_in.close_chunk();
    }
    if (!done)
    {
      iff_out.put_chunk("INCL");
      iff_out.get_bytestream()->writestring(id);
      iff_out.close_chunk();
    }
    iff_out.close_chunk();
  }
  gstr_out->seek(0, SEEK_SET);
  data_pool=DataPool::create(gstr_out);
  chunks_number=-1;
  
  // Second: create missing DjVuFiles
  process_incl_chunks();
  
  flags|=MODIFIED;
  data_pool->clear_stream();
}
#endif

void
DjVuFile::unlink_file(const GUTF8String &id)
{
  DEBUG_MSG("DjVuFile::insert_file(): id='" << id << "'\n");
  DEBUG_MAKE_INDENT(3);
  
  // Remove the file from the list of included files
  {
    GURL url=DjVuPort::get_portcaster()->id_to_url(this, id);
    if (url.is_empty()) url=GURL::UTF8(id,this->url.base());
    GCriticalSectionLock lock(&inc_files_lock);
    for(GPosition pos=inc_files_list;pos;)
      if (inc_files_list[pos]->get_url()==url)
      {
        GPosition this_pos=pos;
        ++pos;
        inc_files_list.del(this_pos);
      } else ++pos;
  }
  
  // And update the data.
  const GP<ByteStream> str_in(data_pool->get_stream());
  const GP<IFFByteStream> giff_in(IFFByteStream::create(str_in));
  IFFByteStream &iff_in=*giff_in;
  
  const GP<ByteStream> gstr_out(ByteStream::create());
  const GP<IFFByteStream> giff_out(IFFByteStream::create(gstr_out));
  IFFByteStream &iff_out=*giff_out;
  
  GUTF8String chkid;
  if (iff_in.get_chunk(chkid))
  {
    iff_out.put_chunk(chkid);
    int chksize;
    while((chksize=iff_in.get_chunk(chkid)))
    {
      if (chkid!="INCL")
      {
        iff_out.put_chunk(chkid);
        iff_out.copy(*iff_in.get_bytestream());
        iff_out.close_chunk();
      } else
      {
        GUTF8String incl_str;
        char buffer[1024];
        int length;
        while((length=iff_in.read(buffer, 1024)))
          incl_str+=GUTF8String(buffer, length);
        
	       // Eat '\n' in the beginning and at the end
        while(incl_str.length() && incl_str[0]=='\n')
        {
          incl_str=incl_str.substr(1,(unsigned int)(-1));
        }
        while(incl_str.length()>0 && incl_str[(int)incl_str.length()-1]=='\n')
          incl_str.setat(incl_str.length()-1, 0);
        if (incl_str!=id)
        {
          iff_out.put_chunk("INCL");
          iff_out.get_bytestream()->writestring(incl_str);
          iff_out.close_chunk();
        }
      }
      iff_in.close_chunk();
    }
    iff_out.close_chunk();
  }
  
  gstr_out->seek(0, SEEK_SET);
  data_pool=DataPool::create(gstr_out);
  chunks_number=-1;
  
  flags|=MODIFIED;
}

void
DjVuFile::change_info(GP<DjVuInfo> xinfo,const bool do_reset)
{
  DEBUG_MSG("DjVuFile::change_text()\n");
  // Mark this as modified
  set_modified(true);
  if(do_reset)
    reset();
  info=xinfo;
}

#ifndef NEED_DECODER_ONLY
void
DjVuFile::change_text(GP<DjVuTXT> txt,const bool do_reset)
{
  DEBUG_MSG("DjVuFile::change_text()\n");
  GP<DjVuText> gtext_c=DjVuText::create();
  DjVuText &text_c=*gtext_c;
  if(contains_text())
  {
    const GP<ByteStream> file_text(get_text());
    if(file_text)
    {
      text_c.decode(file_text);
    }
  }
  GCriticalSectionLock lock(&text_lock);
  // Mark this as modified
  set_modified(true);
  if(do_reset)
    reset();
  text_c.txt = txt;
  text=ByteStream::create();
  text_c.encode(text);
}

void
DjVuFile::change_meta(const GUTF8String &xmeta,const bool do_reset)
{
  DEBUG_MSG("DjVuFile::change_meta()\n");
  // Mark this as modified
  set_modified(true);
  if(contains_meta())
  {
    (void)get_meta();
  }
  if(do_reset)
    reset();
  GCriticalSectionLock lock(&meta_lock);
  meta=ByteStream::create();
  if(xmeta.length())
  {
    const GP<IFFByteStream> giff=IFFByteStream::create(meta);
    IFFByteStream &iff=*giff;
    iff.put_chunk("METz");
    {
      GP<ByteStream> gbsiff=BSByteStream::create(iff.get_bytestream(),50);
      gbsiff->writestring(xmeta);
    }
    iff.close_chunk();
  }
}
#endif


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
