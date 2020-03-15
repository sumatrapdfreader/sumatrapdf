/*
** JNetLib
** Copyright (C) 2008-2014 Cockos Inc
** Copyright (C) 2003 Nullsoft, Inc.
** Author: Justin Frankel
** File: webserver.h - Generic simple webserver baseclass
** License: see jnetlib.h
**
** You can derive your object from WebServerBaseClass to do simple web serving. Example:

  class wwwServer : public WebServerBaseClass
  {
  public:
    wwwServer() { } 
    virtual IPageGenerator *onConnection(JNL_HTTPServ *serv, int port)
    {
      serv->set_reply_header("Server:jnetlib_test/0.0");
      if (!strcmp(serv->get_request_file(),"/"))
      {
        serv->set_reply_string("HTTP/1.1 200 OK");
        serv->set_reply_header("Content-Type:text/html");
        serv->send_reply();

        return new MemPageGenerator(strdup("Test Web Server v0.0"));
      }
      else
      {
        serv->set_reply_string("HTTP/1.1 404 NOT FOUND");
        serv->send_reply();
        return 0; // no data
      }
    }
  };


    wwwServer foo;
    foo.addListenPort(8080);
    while (1)
    {
      foo.run();
      Sleep(10);
    }

  You will also need to derive from the IPageGenerator interface to provide a data stream, here is an
  example of MemPageGenerator:

    class MemPageGenerator : public IPageGenerator
    {
      public:
        virtual ~MemPageGenerator() { free(m_buf); }
        MemPageGenerator(char *buf, int buf_len=-1) { m_buf=buf; if (buf_len >= 0) m_buf_size=buf_len; else m_buf_size=strlen(buf); m_buf_pos=0; }
        virtual int GetData(char *buf, int size) // return 0 when done
        {
          int a=m_buf_size-m_buf_pos;
          if (a < size) size=a;
          memcpy(buf,m_buf+m_buf_pos,size);
          m_buf_pos+=size;
          return size;
        }

      private:
        char *m_buf;
        int m_buf_size;
        int m_buf_pos;
    };


**
*/


#ifndef _JNL_WEBSERVER_H_
#define _JNL_WEBSERVER_H_

#include "httpserv.h"
#include "../wdlcstring.h"
#include "../ptrlist.h"

class IPageGenerator
{
public:
  virtual ~IPageGenerator() { };
  virtual int IsNonBlocking() { return 0; } // override this and return 1 if GetData should be allowed to return 0
  virtual int GetData(char *buf, int size)=0; // return < 0 when done (or 0 if IsNonBlocking() is 1)
};


class WebServerBaseClass
{
protected: // never create one of these directly, always derive
  WebServerBaseClass();

public:
  virtual ~WebServerBaseClass();

  // stuff for setting limits/timeouts
  void setMaxConnections(int max_con);
  void setRequestTimeout(int timeout_s);

  // stuff for setting listener port
  int addListenPort(int port, unsigned int which_interface=0);
  int getListenPort(int idx, int *err=0);
  void removeListenPort(int port);
  void removeListenIdx(int idx);

  // call this a lot :)
  void run(void);

  // if you want to manually attach a connection, use this:
  // you need to specify the port it came in on so the web server can build
  // links
  void attachConnection(JNL_IConnection *con, int port);

  // derived classes need to override this one =)
  virtual IPageGenerator *onConnection(JNL_HTTPServ *serv, int port)=0;

  // stats getting functions

  // these can be used externally, as well as are used by the web server
  static void url_encode(const char *in, char *out, int max_out);
  static void url_decode(const char *in, char *out, int maxlen);
  static void base64decode(const char *src, char *dest, int destsize);
  static void base64encode(const char *in, char *out);

  static int parseAuth(const char *auth_header, char *out, int out_len);//returns 0 on unknown auth, 1 on basic


protected:

  class WS_conInst
  {
  public:
    WS_conInst(JNL_IConnection *c, int which_port) : m_serv(c), m_pagegen(NULL), m_port(which_port)
    {
      time(&m_connect_time);
    }
    ~WS_conInst()
    {
      delete m_pagegen;
    }

    // these will be used by WebServerBaseClass::onConnection yay
    JNL_HTTPServ m_serv;
    IPageGenerator *m_pagegen;

    int m_port; // port this came in on
    time_t m_connect_time;
  };

  int run_connection(WS_conInst *con);

  int m_timeout_s;
  int m_max_con;

  JNL_AsyncDNS m_dns;

  WDL_PtrList<JNL_IListen> m_listeners;
  WDL_PtrList<WS_conInst> m_connections;
  int m_listener_rot;
};



#ifdef JNETLIB_WEBSERVER_WANT_UTILS

#include "../fileread.h"
#include "../wdlstring.h"

class JNL_FilePageGenerator : public IPageGenerator
{
  public:
    JNL_FilePageGenerator(WDL_FileRead *fr) { m_file = fr; }
    virtual ~JNL_FilePageGenerator() { delete m_file; }
    virtual int GetData(char *buf, int size) { return m_file ? m_file->Read(buf,size) : -1; }

  private:

    WDL_FileRead *m_file;
};
class JNL_StringPageGenerator : public IPageGenerator
{
  public:
    JNL_StringPageGenerator() { m_pos=0; }
    virtual ~JNL_StringPageGenerator() { }
    virtual int GetData(char *buf, int size) 
    { 
      if (size > str.GetLength() - m_pos) size=str.GetLength()-m_pos;
      if (size>0) 
      {
        memcpy(buf,str.Get()+m_pos,size);
        m_pos+=size;
      }
      return size; 
    }

    WDL_FastString str; // set this before sending it off

  private:
    int m_pos;
};

static void JNL_get_mime_type_for_file(const char *fn, char *strout, int stroutsz)
{
  const char *ext = fn;
  while (*ext) ext++;
  while (ext > fn && *ext != '.' && *ext != '/' && *ext != '\\') ext--;

  const char *type = "application/octet-stream";

  if (!stricmp(ext,".jpg")) type = "image/jpeg";
  else if (!stricmp(ext,".png")) type = "image/png";
  else if (!stricmp(ext,".gif")) type = "image/gif";
  else if (!stricmp(ext,".txt")) type = "text/plain";
  else if (!strnicmp(ext,".htm",4)) type = "text/html";
  else if (!stricmp(ext,".js")) type = "application/javascript";
  else if (!stricmp(ext,".css")) type = "text/css";
  else if (!stricmp(ext,".xml")) type = "text/xml";
  else if (!stricmp(ext,".svg")) type = "image/svg+xml";

  lstrcpyn_safe(strout,type,stroutsz);
}

static void JNL_Format_RFC1123(time_t t, char *buf)
{

  buf[0]=0;
  static const char days[] = { "SunMonTueWedThuFriSat" };
  static const char mons[] = { "JanFebMarAprMayJunJulAugSepOctNovDec" };

  struct tm *tm =  gmtime(&t);
  if (!tm) return;
  memcpy(buf, days + (tm->tm_wday%7)*3, 3);
  strcpy(buf+3,", ");
  char *p=buf+5;
  strftime(p, 64, "%d xxx %Y %H:%M:%S GMT", tm);
  while (*p && *p != 'x') p++;
  if (*p) memcpy(p, mons + (tm->tm_mon%12)*3, 3);
}

#endif //JNETLIB_WEBSERVER_WANT_UTILS


#endif//_JNL_WEBSERVER_H_
