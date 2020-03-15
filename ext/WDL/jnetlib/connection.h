/*
** JNetLib
** Copyright (C) 2008 Cockos Inc
** Copyright (C) 2000-2001 Nullsoft, Inc.
** Author: Justin Frankel
** File: connection.h - JNL TCP connection interface
** License: see jnetlib.h
**
** Usage:
**   1. Create a JNL_Connection object, optionally specifying a JNL_IAsyncDNS
**      object to use (or NULL for none, or JNL_CONNECTION_AUTODNS for auto),
**      and the send and receive buffer sizes.
**   2. Call connect() to have it connect to a host/port (the hostname will be 
**      resolved if possible).
**   3. call run() with the maximum send/recv amounts, and optionally parameters
**      so you can tell how much has been send/received. You want to do this a lot, while:
**   4. check get_state() to check the state of the connection. The states are:
**        JNL_Connection::STATE_ERROR
**          - an error has occured on the connection. the connection has closed,
**            and you can no longer write to the socket (there still might be 
**            data in the receive buffer - use recv_bytes_available()). 
**        JNL_Connection::STATE_NOCONNECTION
**          - no connection has been made yet. call connect() already! :)
**        JNL_Connection::STATE_RESOLVING
**          - the connection is still waiting for a JNL_AsycnDNS to resolve the
**            host. 
**        JNL_Connection::STATE_CONNECTING
**          - the asynchronous call to connect() is still running.
**        JNL_Connection::STATE_CONNECTED
**          - the connection has connected, all is well.
**        JNL_Connection::STATE_CLOSING
**          - the connection is closing. This happens after a call to close,
**            without the quick parameter set. This means that the connection
**            will close once the data in the send buffer is sent (data could
**            still be being received when it would be closed). After it is 
**            closed, the state will transition to:
**        JNL_Connection::STATE_CLOSED
**          - the connection has closed, generally without error. There still
**            might be data in the receieve buffer, use recv_bytes_available().
**   5. Use send() and send_string() to send data. You can use 
**      send_bytes_in_queue() to see how much has yet to go out, or 
**      send_bytes_available() to see how much you can write. If you use send()
**      or send_string() and not enough room is available, both functions will 
**      return error ( < 0)
**   6. Use recv() and recv_line() to get data. If you want to see how much data 
**      there is, use recv_bytes_available() and recv_lines_available(). If you 
**      call recv() and not enough data is available, recv() will return how much
**      data was actually read. See comments at the function defs.
**
**   7. To close, call close(1) for a quick close, or close() for a close that will
**      make the socket close after sending all the data sent. 
**  
**   8. delete ye' ol' object.
*/

#ifndef _CONNECTION_H_
#define _CONNECTION_H_

#include "asyncdns.h"
#include "netinc.h"
#include "../heapbuf.h"

#define JNL_CONNECTION_AUTODNS ((JNL_IAsyncDNS*)-1)

struct sockaddr_in;

#ifndef JNL_NO_DEFINE_INTERFACES
class JNL_IConnection
{
  public:
    virtual ~JNL_IConnection() { }
    virtual void connect(const char *hostname, int port)=0;
    virtual void connect(SOCKET sock, struct sockaddr_in *loc=NULL)=0; // used by the listen object, usually not needed by users.

    virtual void run(int max_send_bytes=-1, int max_recv_bytes=-1, int *bytes_sent=NULL, int *bytes_rcvd=NULL)=0;
    virtual int  get_state()=0;
    virtual const char *get_errstr()=0;

    virtual void close(int quick=0)=0;
    virtual void flush_send(void)=0;

    virtual int send_bytes_in_queue(void)=0;
    virtual int send_bytes_available(void)=0;
    virtual int send(const void *data, int length)=0; // returns -1 if not enough room
    virtual int send_bytes(const void *data, int length)=0;
    virtual int send_string(const char *line)=0;      // returns -1 if not enough room

    virtual int recv_bytes_available(void)=0;
    virtual int recv_bytes(void *data, int maxlength)=0; // returns actual bytes read
    virtual int recv_lines_available(void)=0;
    virtual int recv_line(char *line, int maxlength)=0; // returns 0 if the line was terminated with a \r or \n, 1 if not.
                                              // (i.e. if you specify maxlength=10, and the line is 12 bytes long
                                              // it will return 1. or if there is no \r or \n and that's all the data
                                              // the connection has.)
    virtual int recv_get_linelen()=0; // length in bytes for current line (including \r and/or \n), or 0 if no newline in buffer
    virtual int peek_bytes(void *data, int maxlength)=0; // returns bytes peeked

    virtual unsigned int get_interface(void)=0;        // this returns the interface the connection is on
    virtual unsigned int get_remote(void)=0; // remote host ip.
    virtual short get_remote_port(void)=0; // this returns the remote port of connection

    virtual void set_interface(int useInterface)=0; // call before connect if needed
  };

  #define JNL_Connection_PARENTDEF : public JNL_IConnection
#else
  #define JNL_IConnection JNL_Connection
  #define JNL_Connection_PARENTDEF
#endif

#ifndef JNL_NO_IMPLEMENTATION
  
class JNL_Connection JNL_Connection_PARENTDEF
{
  public:
    typedef enum 
    { 
      STATE_ERROR, 
      STATE_NOCONNECTION,
      STATE_RESOLVING, 
      STATE_CONNECTING, 
      STATE_CONNECTED, 
      STATE_CLOSING, 
      STATE_CLOSED 
    } state;

    JNL_Connection(JNL_IAsyncDNS *dns=JNL_CONNECTION_AUTODNS, int sendbufsize=8192, int recvbufsize=8192);
    ~JNL_Connection();

    void connect(const char *hostname, int port);
    void connect(SOCKET sock, struct sockaddr_in *loc=NULL); // used by the listen object, usually not needed by users.

    void run(int max_send_bytes=-1, int max_recv_bytes=-1, int *bytes_sent=NULL, int *bytes_rcvd=NULL);
    int  get_state() { return m_state; }
    const char *get_errstr() { return m_errorstr; }

    void close(int quick=0);
    void flush_send(void) { m_send_len=m_send_pos=0; }

    int send_bytes_in_queue(void);
    int send_bytes_available(void);
    int send(const void *data, int length); // returns -1 if not enough room
    inline int send_bytes(const void *data, int length) { return send(data, length); }
    int send_string(const char *line);      // returns -1 if not enough room


    int recv_bytes_available(void);
    int recv_bytes(void *data, int maxlength); // returns actual bytes read
    int recv_lines_available(void);
    int recv_line(char *line, int maxlength); // returns 0 if the line was terminated with a \r or \n, 1 if not.
                                              // (i.e. if you specify maxlength=10, and the line is 12 bytes long
                                              // it will return 1. or if there is no \r or \n and that's all the data
                                              // the connection has.)
    int recv_get_linelen();                   // length in bytes for current line (including \r and/or \n), or 0 if no newline in buffer
    int peek_bytes(void *data, int maxlength); // returns bytes peeked

    unsigned int get_interface(void);        // this returns the interface the connection is on
    unsigned int get_remote(void); // remote host ip.
    short get_remote_port(void); // this returns the remote port of connection
  
    void set_interface(int useInterface); // call before connect if needed

  protected:
    SOCKET m_socket;
    short m_remote_port;
    WDL_TypedBuf<unsigned char> m_recv_buffer;
    WDL_TypedBuf<unsigned char> m_send_buffer;

    int  m_recv_pos;
    int  m_recv_len;
    int  m_send_pos;
    int  m_send_len;

    int m_localinterfacereq;
    struct sockaddr_in *m_saddr;
    char m_host[256];

    JNL_IAsyncDNS *m_dns;
    int m_dns_owned;

    state m_state;
    const char *m_errorstr;

    int getbfromrecv(int pos, int remove); // used by recv_line*

};

#endif

#endif // _Connection_H_
