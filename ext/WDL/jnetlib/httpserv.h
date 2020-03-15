/*
** JNetLib
** Copyright (C) 2008 Cockos Inc
** Copyright (C) 2001 Nullsoft, Inc.
** Author: Justin Frankel
** File: httpserv.h - JNL interface for doing HTTP GET/POST serving.
** License: see jnetlib.h
** This class just manages the http reply/sending, not where the data 
** comes from, etc.
** for a mini-web server see webserver.h
*/

#ifndef _HTTPSERV_H_
#define _HTTPSERV_H_

#include "connection.h"

#include "../wdlstring.h"
#include "../queue.h"

#ifndef JNL_NO_DEFINE_INTERFACES
class JNL_IHTTPServ
{
  public:

    virtual ~JNL_IHTTPServ() { }

    virtual int run()=0; // returns: < 0 on error, 0 on request not read yet, 1 if reading headers, 2 if reply not sent, 3 if reply sent, sending data. 4 on connection closed.

    virtual const char *geterrorstr()=0;

    // use these when state returned by run() is 2 
    virtual const char *get_request_file()=0; // file portion of http request
    virtual const char *get_request_parm(const char *parmname)=0; // parameter portion (after ?)
    virtual const char *getallheaders()=0;
    virtual const char *getheader(const char *headername)=0;

    virtual void set_reply_string(const char *reply_string)=0; // should be HTTP/1.1 OK or the like
    virtual void set_reply_header(const char *header)=0; // i.e. "content-size: 12345"
    virtual void set_reply_size(int sz)=0; // if set, size will also add keep-alive etc

    virtual void send_reply()=0;

    ////////// sending data ///////////////
    virtual int bytes_inqueue()=0;
    virtual int bytes_cansend()=0;
    virtual void write_bytes(const char *bytes, int length)=0;

    virtual void close(int quick)=0;

    virtual JNL_IConnection *get_con()=0;
    virtual JNL_IConnection *steal_con()=0;
    virtual bool want_keepalive_reset()=0;

    virtual bool canKeepAlive()=0;
};
  #define JNL_HTTPServ_PARENTDEF : public JNL_IHTTPServ
#else
  #define JNL_IHTTPServ JNL_HTTPServ
  #define JNL_HTTPServ_PARENTDEF
#endif


#ifndef JNL_NO_IMPLEMENTATION

class JNL_HTTPServ JNL_HTTPServ_PARENTDEF
{
  public:
    JNL_HTTPServ(JNL_IConnection *con);
    ~JNL_HTTPServ();

    int run(); // returns: < 0 on error, 0 on request not read yet, 1 if reading headers, 2 if reply not sent, 3 if reply sent, sending data. 4 on connection closed.

    const char *geterrorstr() { return m_errstr.Get()[0] ? m_errstr.Get() : NULL; }

    // use these when state returned by run() is 2 
    const char *get_request_file(); // file portion of http request
    const char *get_request_parm(const char *parmname); // parameter portion (after ?)
    const char *getallheaders() { return m_recvheaders.Get(); } // double null terminated, null delimited list
    const char *getheader(const char *headername);

    void set_reply_string(const char *reply_string); // should be HTTP/1.1 OK or the like
    void set_reply_header(const char *header); // i.e. "content-size: 12345"
    void set_reply_size(int sz); // if set, size will also add keep-alive etc

    void send_reply() { m_reply_ready=1; } // send reply, state will advance to 3.

    ////////// sending data ///////////////
    int bytes_inqueue() { if (m_state == 3 || m_state == -1 || m_state ==4) return m_con->send_bytes_in_queue(); else return 0; }
    int bytes_cansend() { if (m_state == 3) return m_con->send_bytes_available() - (m_usechunk?16:0); else return 0; }
    void write_bytes(const char *bytes, int length);

    void close(int quick) { m_con->close(quick); m_state=4; }

    JNL_IConnection *get_con() { return m_con; }
    JNL_IConnection *steal_con() { JNL_IConnection *ret= m_con; m_con=0; return ret; }
    bool want_keepalive_reset();

    bool canKeepAlive() { return m_keepalive; }

  protected:
    void seterrstr(const char *str) { m_errstr.Set(str); } 

    int m_reply_ready;
    int m_state;
    bool m_keepalive, m_usechunk;

    WDL_FastString m_errstr;
    WDL_FastString m_reply_headers;
    WDL_FastString m_reply_string;
    WDL_TypedQueue<char> m_recvheaders;
    WDL_TypedBuf<char> m_recv_request; // either double-null terminated, or may contain parameters after first null.
    JNL_IConnection *m_con;
};

#endif

#endif // _HTTPSERV_H_
