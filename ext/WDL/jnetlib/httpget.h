/*
** JNetLib
** Copyright (C) 2008 Cockos Inc
** Copyright (C) 2000-2001 Nullsoft, Inc.
** Author: Justin Frankel
** File: httpget.h - JNL interface for doing HTTP GETs.
** License: see jnetlib.h
**
** Usage:
**   1. Create a JNL_HTTPGet object, optionally specifying a JNL_AsyncDNS
**      object to use (or NULL for none, or JNL_CONNECTION_AUTODNS for auto),
**      and the receive buffer size, and a string specifying proxy (or NULL 
**      for none). See note on proxy string below.
**   2. call addheader() to add whatever headers you want. It is recommended to
**      add at least the following two:
**        addheader("User-Agent:MyApp (Mozilla)");
*///      addheader("Accept:*/*");
/*         ( the comment weirdness is there so I Can do the star-slash :)
**   3. Call connect() with the URL you wish to GET (see URL string note below)
**   4. Call run() once in a while, checking to see if it returns -1 
**      (if it does return -1, call geterrorstr() to see what the error is).
**      (if it returns 1, no big deal, the connection has closed).
**   5. While you're at it, you can call bytes_available() to see if any data
**      from the http stream is available, or getheader() to see if any headers
**      are available, or getreply() to see the HTTP reply, or getallheaders() 
**      to get a double null terminated, null delimited list of headers returned.
**   6. If you want to read from the stream, call get_bytes (which returns how much
**      was actually read).
**   7. content_length() is a helper function that uses getheader() to check the
**      content-length header.
**   8. Delete ye' ol' object when done.
**
** Proxy String:
**   should be in the format of host:port, or user@host:port, or 
**   user:password@host:port. if port is not specified, 80 is assumed.
** URL String:
**   should be in the format of http://user:pass@host:port/requestwhatever
**   note that user, pass, port, and /requestwhatever are all optional :)
**   note that also, http:// is really not important. if you do poo://
**   or even leave out the http:// altogether, it will still work.
*/

#ifndef _HTTPGET_H_
#define _HTTPGET_H_

#include "connection.h"

#ifndef JNL_NO_DEFINE_INTERFACES
  class JNL_IHTTPGet
  {
    public:

      virtual ~JNL_IHTTPGet() { }

      virtual void addheader(const char *header)=0;

      virtual void connect(const char *url, int ver=0, const char *requestmethod="GET")=0;

      virtual int run()=0; // returns: 0 if all is OK. -1 if error (call geterrorstr()). 1 if connection closed.

      virtual int   get_status()=0; // returns 0 if connecting, 1 if reading headers, 
                          // 2 if reading content, -1 if error.

      virtual const char *getallheaders()=0; // double null terminated, null delimited list
      virtual const char *getheader(const char *headername)=0;
      virtual const char *getreply()=0;
      virtual int   getreplycode()=0; // returns 0 if none yet, otherwise returns http reply code.

      virtual const char *geterrorstr()=0;

      virtual int bytes_available()=0;
      virtual int get_bytes(char *buf, int len)=0;
      virtual int peek_bytes(char *buf, int len)=0;

      virtual int content_length()=0;

      virtual JNL_IConnection *get_con()=0;
  };
  #define JNL_HTTPGet_PARENTDEF : public JNL_IHTTPGet
#else
  #define JNL_IHTTPGet JNL_HTTPGet
  #define JNL_HTTPGet_PARENTDEF
#endif

#ifndef JNL_NO_IMPLEMENTATION

class JNL_HTTPGet JNL_HTTPGet_PARENTDEF
{
  public:
    JNL_HTTPGet(JNL_IAsyncDNS *dns=JNL_CONNECTION_AUTODNS, int recvbufsize=16384, char *proxy=NULL);
    ~JNL_HTTPGet();

    void addheader(const char *header);

    void connect(const char *url, int ver=0, const char *requestmethod="GET");

    int run(); // returns: 0 if all is OK. -1 if error (call geterrorstr()). 1 if connection closed.

    int   get_status(); // returns 0 if connecting, 1 if reading headers, 
                        // 2 if reading content, -1 if error.

    const char *getallheaders(); // double null terminated, null delimited list
    const char *getheader(const char *headername);
    const char *getreply() { return m_reply; }
    int   getreplycode(); // returns 0 if none yet, otherwise returns http reply code.

    const char *geterrorstr() { return m_errstr;}

    int bytes_available();
    int get_bytes(char *buf, int len);
    int peek_bytes(char *buf, int len);

    int content_length() { const char *p=getheader("content-length"); if (p) return atoi(p); return 0; }

    JNL_IConnection *get_con() { return m_con; }



    static void do_parse_url(char *url, char **host, int *port, char **req, char **lp); // url gets thrashed, and host/req/lp are freed/allocated
    static void do_encode_mimestr(char *in, char *out);

  protected:
    void reinit();
    void deinit();
    void seterrstr(const char *str) { if (m_errstr) free(m_errstr); m_errstr=(char*)malloc(strlen(str)+1); strcpy(m_errstr,str); }

    JNL_IAsyncDNS *m_dns;
    JNL_IConnection *m_con;
    int m_recvbufsize;

    int m_http_state;

    int m_http_port;
    char *m_http_url;
    char *m_http_host;
    char *m_http_lpinfo;
    char *m_http_request;

    char *m_http_proxylpinfo;
    char *m_http_proxyhost;
    int   m_http_proxyport;

    char *m_sendheaders;
    char *m_recvheaders;
    int m_recvheaders_size;
    char *m_reply;

    char *m_errstr;
};
#endif

#endif // _HTTPGET_H_
