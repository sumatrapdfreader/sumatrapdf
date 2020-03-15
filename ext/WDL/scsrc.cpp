/*
  WDL - scsrc.cpp
  Copyright (C) 2007, Cockos Incorporated

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
  



*/

#ifdef _WIN32
#include <windows.h>
#else
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
typedef int DWORD;

static void Sleep(int ms)
{
  usleep(ms?ms*1000:100);
}

static unsigned int GetTickCount()
{
  struct timeval tm={0,};
  gettimeofday(&tm,NULL);
  return tm.tv_sec*1000 + tm.tv_usec/1000;
}

#endif // !_WIN32

#include <math.h>

#include "scsrc.h"
#include "jnetlib/jnetlib.h"
#include "wdlcstring.h"

// maybe we need to do this better someday
#define POST_DIV_STRING "zzzASFIJAHFASJFHASLKFHZI8"
#define END_POST_BYTES "\r\n--" POST_DIV_STRING "--\r\n"
    

#define ERR_NOLAME -600
#define ERR_CREATINGENCODER -599

#define ERR_TIMEOUT -4
#define ERR_CONNECT -5
#define ERR_AUTH -6

#define ST_OK 0
#define ST_CONNECTING 1
#define ERR_DISCONNECTED_AFTER_SUCCESS 32
WDL_ShoutcastSource::WDL_ShoutcastSource(const char *host, const char *pass, const char *name, bool pub, 
                                         const char *genre, const char *url,
                                         int nch, int srate, int kbps, const char *ircchan)
{
  m_rs.SetMode(true,1,true);

  m_post_postsleft=0;
  m_postmode_session=GetTickCount();
  m_is_postmode = !!strstr(host,"/");

  totalBitrate=0;
  sendProcessor=0;
  userData=0;

  JNL::open_socketlib();
  m_host.Set(host);
  m_pass.Set(pass);
  if (name) m_name.Set(name);
  if (url) m_url.Set(url);
  if (genre) m_genre.Set(genre);
  if (ircchan) m_ircchan.Set(ircchan);
  m_pub=pub;
  m_br=kbps;

  m_state=ST_CONNECTING; // go to 0 when we 
  m_nch=nch==2?2:1;
  m_bytesout=0;
  m_srate=srate;
  m_sendcon=0;

  m_needtitle=0;
  m_title[0]=0;
  m_titlecon=0;
  m_titlecon_start=0;
  m_encoder_splsin=0;
  m_encoder=new LameEncoder(m_srate,m_nch,m_br);
  int s=m_encoder->Status();
  if (s == 1) m_state=ERR_NOLAME;
  else if (s) m_state=ERR_CREATINGENCODER;

  if (s) { delete m_encoder; m_encoder=0; }

  m_sendcon_start=time(NULL);
  if (m_encoder)
  {
    if (m_is_postmode)
    {
      PostModeConnect();      
    }
    else
    {
      m_sendcon = new JNL_Connection(JNL_CONNECTION_AUTODNS,65536,65536);
      WDL_String hb(m_host.Get());
      int port=8000;
      char *p=strstr(hb.Get(),":");
      if (p)
      {
        *p++=0;
        port = atoi(p);
        if (!port) port=8000;
      }
      m_sendcon->connect(hb.Get(), port+1);

      m_sendcon->send_string(m_pass.Get());
      m_sendcon->send_string("\r\n");
    }
  }

}

WDL_ShoutcastSource::~WDL_ShoutcastSource()
{
  delete m_titlecon;
  delete m_sendcon;
  delete m_encoder;
}

int WDL_ShoutcastSource::GetStatus() // returns 0 if connected/connecting, >0 if disconnected, -1 if failed connect (or other error) from the start
{
  if (m_state<ST_OK || m_state >= ERR_DISCONNECTED_AFTER_SUCCESS) return m_state;
  return 0;
}

void WDL_ShoutcastSource::GetStatusText(char *buf, int bufsz) // gets status text
{
  if (m_state == ST_OK) snprintf(buf,bufsz,"Connected. Sent %u bytes",m_bytesout);
  else if (m_state == ST_CONNECTING) lstrcpyn_safe(buf,"Connecting...",bufsz);
  else if (m_state == ERR_DISCONNECTED_AFTER_SUCCESS) snprintf(buf,bufsz,"Disconnected after sending %u bytes",m_bytesout);
  else if (m_state == ERR_AUTH) lstrcpyn_safe(buf,"Error authenticating with server",bufsz);
  else if (m_state == ERR_CONNECT) lstrcpyn_safe(buf,"Error connecting to server",bufsz);
  else if (m_state == ERR_TIMEOUT) lstrcpyn_safe(buf,"Timed out connecting to server",bufsz);
  else if (m_state == ERR_CREATINGENCODER) lstrcpyn_safe(buf,"Error creating encoder",bufsz);
  else if (m_state == ERR_NOLAME) lstrcpyn_safe(buf,"Error loading libmp3lame",bufsz);
  else lstrcpyn_safe(buf,"Error creating encoder",bufsz);

}

void WDL_ShoutcastSource::SetCurTitle(const char *title)
{
  m_titlemutex.Enter();
  lstrcpyn_safe(m_title,title,sizeof(m_title));
  m_needtitle=true;
  m_titlemutex.Leave();
  
}


template<class T> static void splcvt(T *ob, float **samples, int innch, int outnch, int chspread, int frames)
{
  int x=frames,a=0;
  if (outnch > 1)
  {
    if (innch < 2)
    {
      while (x--)
      {
        ob[0] = ob[1] = samples[0][a];
        ob+=2;
        a+=chspread;
      }
    }
    else
    {
      while (x--)
      {
        *ob++ = samples[0][a];
        *ob++ = samples[1][a];
        a+=chspread;
      }
    }
  }
  else
  {
    if (innch < 2)
    {
      while (x--)
      {
        *ob++ = samples[0][a];
        a+=chspread;
      }
    }
    else
    {
      while (x--)
      {
        *ob++ = (samples[0][a]+samples[1][a])*0.5f;
        a+=chspread;
      }
    }
  }
}

void WDL_ShoutcastSource::OnSamples(float **samples, int nch, int chspread, int frames, double srate)
{
  if (fabs(srate-m_srate)<1.0)
  {
    m_samplemutex.Enter();
    float *ob;
    if (m_samplequeue.Available() < (int)sizeof(float)*m_nch*96000*4 &&
        m_encoder && m_encoder->outqueue.Available() < 256*1024 &&
        NULL != (ob = (float *)m_samplequeue.Add(NULL,frames*m_nch*sizeof(float))))
    {
      splcvt(ob,samples,nch,m_nch,chspread,frames);
    }

    m_samplemutex.Leave();

    return;
  }

  // resample!
  m_rs.SetRates(srate,m_srate);
  m_rs.SetFeedMode(true);

  for (;;)
  {
    WDL_ResampleSample *ob=NULL;
    int amt = m_rs.ResamplePrepare(frames, m_nch, &ob);
    if (amt > frames) amt=frames;
    if (ob) splcvt(ob,samples,nch,m_nch,chspread,amt);
    frames-=amt;

    WDL_ResampleSample tmp[2048];
    amt = m_rs.ResampleOut(tmp,amt,2048/m_nch,m_nch);

    if (frames < 1 && amt < 1) break;

    m_samplemutex.Enter();
    if (m_samplequeue.Available() < (int)sizeof(float)*m_nch*96000*4 && m_encoder && m_encoder->outqueue.Available() < 256*1024)
    {
      amt *= nch;
      float *p = (float*)m_samplequeue.Add(NULL,amt*sizeof(float));
      if (p)
      {
        WDL_ResampleSample *rd = tmp;
        while (amt--) *p++ = *rd++;
      }
    }
    m_samplemutex.Leave();
  }
}


static void url_encode(char *in, char *out, int max_out)
{
  while (*in && max_out > 4)
  {
    if ((*in >= 'A' && *in <= 'Z')||
	      (*in >= 'a' && *in <= 'z')||
	      (*in >= '0' && *in <= '9')|| *in == '.' || *in == '_' || *in == '-') 
    {
      *out++=*in++;
      max_out--;
    }
    else
	  {
  	  int i=*in++;
      *out++ = '%';
      int b=(i>>4)&15;
      if (b < 10) *out++='0'+b;
      else *out++='A'+b-10;
      b=i&15;
      if (b < 10) *out++='0'+b;
      else *out++='A'+b-10;
      max_out-=3;
	  }
  }
  *out=0;
}

int WDL_ShoutcastSource::RunStuff()
{
  int ret=0;
  // run connection

  if (m_sendcon)
  {
    if (m_encoder && m_encoder_splsin > 48000*60*60*3) // every 3 hours, reinit the mp3 encoder
    {
      m_encoder_splsin=0;

      WDL_Queue tmp;
      if (m_encoder->outqueue.GetSize()) tmp.Add(m_encoder->outqueue.Get(),m_encoder->outqueue.GetSize());

      delete m_encoder;

      int s=2;
      m_encoder=new LameEncoder(m_srate,m_nch,m_br);
      if (m_encoder && !(s=m_encoder->Status()))
      {
        // copy out queue from m_encoder to newnc
        if (tmp.GetSize()) m_encoder->outqueue.Add(tmp.Get(),tmp.GetSize());
      }
      else 
      {
        if (s == 1) m_state=ERR_NOLAME;
        else if (s) m_state=ERR_CREATINGENCODER;
        delete m_encoder;
        m_encoder=0;
      }

    }

    if (m_encoder)
    {
      // encode data from m_samplequeue
      int n=32;
      int maxl=1152*2;
      m_workbuf.Resize(maxl);
      while (n--)
      {
        m_samplemutex.Enter();
        int d=m_samplequeue.Available()/sizeof(float);
        if (d > 0)
        {
          if (d>maxl) d=maxl;
          m_samplequeue.GetToBuf(0,m_workbuf.Get(),d*sizeof(float));
          m_samplequeue.Advance(d*sizeof(float));
        }
        m_samplemutex.Leave();

        if (!d) break;

        m_encoder_splsin+=d/m_nch;
        m_encoder->Encode(m_workbuf.Get(),d/m_nch,1);
        ret=1;
      }

      if (m_encoder->outqueue.Available() > 128*1024)
      {
        m_encoder->outqueue.Advance(m_encoder->outqueue.Available()-64*1024);
      }

      if (m_state==ST_OK)
      {
        WDL_Queue *srcq = &m_encoder->outqueue;

        if (sendProcessor)
        {
          sendProcessor(userData,&m_procdata,srcq);
          srcq = &m_procdata;
        }

        
        int mb=srcq->Available();
        int mb2=m_sendcon->send_bytes_available();

        if (mb>mb2) mb=mb2;

        if (m_is_postmode)
        {
          if (m_post_bytesleft<=0) PostModeConnect();
          
          if (mb>m_post_bytesleft) mb = m_post_bytesleft;
        }

        if (mb>0)
        {
          m_bytesout+=mb;
          m_sendcon->send_bytes(srcq->Get(),mb);
          if (m_is_postmode) m_post_bytesleft-=mb;
          srcq->Advance(mb);
          srcq->Compact();
          ret=1;
        }
      }
    }

    m_sendcon->run();
    int s = m_sendcon->get_state();

    if (m_state == ST_CONNECTING)
    {
      if (m_is_postmode)
      {
        m_state=ST_OK;
      }
      else if (m_sendcon->recv_lines_available()>0)
      {
        char buf[4096];
        m_sendcon->recv_line(buf, 4095);
        if (strcmp(buf, "OK2")) 
        {
          m_state=ERR_AUTH;
          delete m_sendcon;
          m_sendcon=0;
        }
        else 
        {
          m_state=ST_OK;
          m_sendcon->send_string("icy-name:");
          m_sendcon->send_string(m_name.Get());
          m_sendcon->send_string("\r\n");
          m_sendcon->send_string("icy-genre:");
          m_sendcon->send_string(m_genre.Get());
          m_sendcon->send_string("\r\n");
          m_sendcon->send_string("icy-pub:");
          m_sendcon->send_string(m_pub ? "1":"0");
          m_sendcon->send_string("\r\n");
          m_sendcon->send_string("icy-br:");
          char buf[64];
          snprintf(buf,sizeof(buf),"%d",totalBitrate ? totalBitrate : m_br);
          m_sendcon->send_string(buf);
          m_sendcon->send_string("\r\n");
          m_sendcon->send_string("icy-url:");
          m_sendcon->send_string(m_url.Get());
          m_sendcon->send_string("\r\n");
          if (m_ircchan.Get()[0])
          {
            m_sendcon->send_string("icy-irc:");
            m_sendcon->send_string(m_ircchan.Get());
            m_sendcon->send_string("\r\n");
          }
          m_sendcon->send_string("icy-genre:");
          m_sendcon->send_string(m_genre.Get());
          m_sendcon->send_string("\r\n");
          m_sendcon->send_string("icy-reset:1\r\n\r\n");
        }
      }
    }
    if (!m_is_postmode) switch (s) 
    {
      case JNL_Connection::STATE_ERROR:
      case JNL_Connection::STATE_CLOSED:
        if (m_state==ST_OK) m_state=ERR_DISCONNECTED_AFTER_SUCCESS; 
        else if (m_state>ST_OK && m_state < ERR_DISCONNECTED_AFTER_SUCCESS) m_state = ERR_CONNECT;

        delete m_sendcon;
        m_sendcon=0;
      break;
      default:
        if (m_state > ST_OK && m_state < ERR_DISCONNECTED_AFTER_SUCCESS && time(NULL) > m_sendcon_start+30)
        {
          m_state=ERR_TIMEOUT;
          delete m_sendcon;
          m_sendcon=0;
        }

      break;
    }
  }

  if (m_titlecon)
  {
    if (m_titlecon->run() || time(NULL) > m_titlecon_start+30)
    {
      delete m_titlecon;
      m_titlecon=0;
    }
  }

  if (m_needtitle && m_state==ST_OK && !m_is_postmode)
  {
    char title[512];
    m_titlemutex.Enter();
    url_encode(m_title,title,sizeof(title)-1);
    m_needtitle=false;
    m_titlemutex.Leave();


    WDL_String url;
    url.Append("http://");
    url.Append(m_host.Get());
    url.Append("/admin.cgi?pass=");
    url.Append(m_pass.Get());
    url.Append("&mode=updinfo&song=");
    
    url.Append(title);

    delete m_titlecon;
    m_titlecon=new JNL_HTTPGet;
    m_titlecon->addheader("User-Agent:Cockos WDL scsrc (Mozilla)");
    m_titlecon->addheader("Accept:*/*");
    m_titlecon->connect(url.Get());
    m_titlecon_start=time(NULL);
  }

  return ret;
}

static void AddTextField(WDL_FastString *s, const char *name, const char *value)
{
    s->AppendFormatted(4096,"--" POST_DIV_STRING "\r\n"
                          "Content-Disposition: form-data; name=\"%s\"\r\n"
                          "\r\n"
                          "%s\r\n",
                              name,
                              value);
}

void WDL_ShoutcastSource::PostModeConnect()
{
  const char *hsrc = m_host.Get();
  if (!strnicmp(hsrc,"http://",7)) hsrc+=7;
  WDL_String hb(hsrc);
  int port=80;
  char zb[32]={0,};
  char *req = zb, *parms=zb;
  char *p=hb.Get();
  while (*p && *p != ':' && *p != '/' && *p != '?') p++;
  if (*p == ':')
  {
    *p++=0;
    port = atoi(p);
    if (!port) port=80;
  }
  while (*p && *p != '/' && *p != '?') p++;
  if (*p == '/')
  {
    *p++=0;
    req = p;
  }
  while (*p && *p != '?') p++;
  if (*p == '?')
  {
    *p++=0;
    parms = p;
  }

  bool allowReuse=false;

  if (m_sendcon)
  {
    m_sendcon->send_string(END_POST_BYTES);
    m_sendcon->run();
    DWORD start=GetTickCount();
    bool done=false,hadResp=false;
    while (GetTickCount() < start+1000 && !done)
    {
      Sleep(50);
      m_sendcon->run();
      if (m_sendcon->get_state() == JNL_Connection::STATE_ERROR ||
          m_sendcon->get_state() > JNL_Connection::STATE_CONNECTED) break;

      while (m_sendcon->recv_lines_available()>0)
      {
        char buf[4096];
        m_sendcon->recv_line(buf, 4095);
//        OutputDebugString(buf);

        if (!strnicmp(buf,"HTTP/",5)) hadResp=true;
        if (!strnicmp(buf,"HTTP/1.1",8)) allowReuse=true;
  
        const char *con="Connection:";
        if (!strnicmp(buf,con,strlen(con)))
        {
          char *p=buf+strlen(con);
          while (*p == ' ') p++;
          if (!strnicmp(p,"close",5)) allowReuse=false;
          else if (!strnicmp(p,"keep-alive",10)) allowReuse=true;

          done=true;
        }
        if (hadResp && (!buf[0] || buf[0] == '\r' || buf[0] == '\n')) 
          done=true;

      }
    }
  }

  if (m_sendcon) m_sendcon->run();

  if (m_sendcon && 
      (!allowReuse || m_post_postsleft<=0 ||
        m_sendcon->get_state() == JNL_Connection::STATE_ERROR ||
        m_sendcon->get_state() > JNL_Connection::STATE_CONNECTED))
        
  {
    delete m_sendcon;
    m_sendcon=0;
  }

  if (!m_sendcon)
  {
//    OutputDebugString("new connection\n");
    m_sendcon = new JNL_Connection(JNL_CONNECTION_AUTODNS,65536,65536);
    m_sendcon->connect(hb.Get(),port);
    m_post_postsleft=16; // todo: some configurable amt?
  }

  int csize = (totalBitrate ? totalBitrate*2 : m_br) * 2000 / 8 + 512; // 2s of audio plus pad
  if (csize<16384) csize=16384;


  WDL_FastString tmp;
  tmp.SetFormatted(4096,"POST /%s HTTP/1.1\r\n"
                        "Connection: %s\r\n"
                        "Host: %s\r\n"
                        "User-Agent: WDLScSrc(Mozilla)\r\n"
                        "MIME-Version: 1.0\r\n"
                        "Content-type: multipart/form-data; boundary=" POST_DIV_STRING "\r\n"
                        "Content-length: %d\r\n"
                        "\r\n",
                        req,
                        --m_post_postsleft > 0 ? "Keep-Alive" : "close",
                        hb.Get(),
                        csize);
  m_sendcon->send_string(tmp.Get());

  m_post_bytesleft = csize - (int)strlen(END_POST_BYTES);

  tmp.Set("");

  p = parms;
  while (*p)
  {
    char *eq = p;
    while (*eq && *eq != '=') eq++;
    if (!*eq) break;
    *eq++=0;
    char *np = eq;
    while (*np && *np != '&') np++;

    AddTextField(&tmp,p,eq);


    if (!*np) break;
    p=np+1;
  }

  AddTextField(&tmp,"broadcast",m_pass.Get());

  if (m_title[0])
  {
    m_titlemutex.Enter();
    if (m_title[0]) AddTextField(&tmp,"title",m_title);
    m_titlemutex.Leave();
  }

  AddTextField(&tmp,"name",m_name.Get());


  {
    char buf[512];
    sprintf(buf,"%u",m_postmode_session);
    AddTextField(&tmp,"session",buf);
  }

  tmp.AppendFormatted(4096,"--" POST_DIV_STRING "\r\n"
                        "Content-Disposition: form-data; name=\"file\"; filename=\"bla.dat\"\r\n"
                        "Content-Type: application/octet-stream\r\n"
                        "Content-transfer-encoding: binary\r\n"
                       );


  tmp.Append("\r\n");

  m_sendcon->send_string(tmp.Get());
  m_post_bytesleft -= tmp.GetLength();

}
