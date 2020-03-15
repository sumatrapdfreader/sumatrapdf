/*
** JNetLib
** Copyright (C) 2000-2001 Nullsoft, Inc.
** Author: Justin Frankel
** File: test.cpp - JNL test code
** License: see jnetlib.h
*/

#ifdef _WIN32
#include <windows.h>
#else
#define Sleep(x) usleep((x)*1000)
#endif
#include <stdio.h>
#include "jnetlib.h"


#define TEST_ASYNCDNS 0
#define TEST_CONNECTION 0
#define TEST_LISTEN 0
#define TEST_TELNET_GATEWAY 0
#define TEST_HTTPGET 0
#define TEST_WEBSERVER 1

#define TEST_UDP 0 //udp is not done yet tho :)



#if (TEST_WEBSERVER)
#include "webserver.h"


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


int main(int argc, char **argv)
{
  JNL::open_socketlib();
  {
    wwwServer foo;
    foo.addListenPort(8080);
    while (1)
    {
      foo.run();
      Sleep(10);
    }
  }
  JNL::close_socketlib();
  return 0;
}

#endif


#if (TEST_HTTPGET)
int main(int argc, char **argv)
{

  if (argc != 3)
  {
    printf("usage: httpget <url> <outfile>\n");
    exit(0);
  }

  JNL_HTTPGet get;
  JNL::open_socketlib();

  get.addheader("User-Agent:PooHead (Mozilla)");
  get.addheader("Accept:*/*");
  get.connect(argv[1]);

  FILE *fp=fopen(argv[2],"wb");
  int headerstate=0;
  int has_printed_headers=0;
  int has_printed_reply=0;
  while (1)
  {
    int st=get.run();
    if (st<0)
    {
      printf("HTTPGet error: %s\n",get.geterrorstr());
      break;
    }
    if (get.get_status()>0)
    {
      if (!has_printed_reply)
      {
        has_printed_reply=1;
        printf("reply: %s (code:%d)\n",get.getreply(),get.getreplycode());
      }
      if (get.get_status()==2)
      {
        int len;
        if (!has_printed_headers)
        {
          has_printed_headers=1;
          printf("headers:\n");
          char *p=get.getallheaders();
          while (p&&*p)
          {
            printf("%s\n",p);
            p+=strlen(p)+1;
          }
        }
        while ((len=get.bytes_available()) > 0)
        {
          char buf[4096];
          if (len > 4096) len=4096;
          len=get.get_bytes(buf,len);
          if (len>0)fwrite(buf,len,1,fp);
        }
      }
    }
    if (st==1) // 1 means connection closed
    {
      printf("HTTPGet done!\n");
      break;
    }
  }
  if (fp) fclose(fp);
  JNL::close_socketlib();
  return 0;
}

#endif


#if (TEST_TELNET_GATEWAY)

int main()
{
  JNL_Connection *cons[32]={0,};
  JNL_Connection *outcons[32]={0,};
  char textpos[32][256];
  int n_cons=0;
  int states[32]={0,};
  
  JNL::open_socketlib();
  JNL_AsyncDNS dns;
  JNL_Listen l(23);
  while (!l.is_error())
  {
    Sleep(30);
    if (n_cons<32)
    {
      JNL_Connection *con=l.get_connect();
      if (con)
      {
        int x;
        for (x = 0; x < 32; x ++)
        {
          if (!cons[x])
          {
            cons[x]=con;
            outcons[x]=0;
            states[x]=0;
            n_cons++;
            break;
          }
        }
      }
    }
    int x;
    for (x = 0; x < 32; x ++)
    {
      if (cons[x])
      {
        cons[x]->run();
        if (outcons[x]) outcons[x]->run();

        if (cons[x]->get_state() == JNL_Connection::STATE_ERROR || cons[x]->get_state()==JNL_Connection::STATE_CLOSED ||
            (outcons[x] && (cons[x]->get_state() == JNL_Connection::STATE_ERROR || cons[x]->get_state()==JNL_Connection::STATE_CLOSED)))
        {
          delete cons[x];
          if (outcons[x]) delete outcons[x];
          outcons[x]=0;
          cons[x]=0;
          states[x]=0;
          n_cons--;
        }
        else
        {
          if (states[x]==0)
          {
            cons[x]->send_string("\r\nwelcome ");
            states[x]++;
          }
          if (states[x]==1)
          {
            char hoststr[256];
            int ret=dns.reverse(cons[x]->get_remote(),hoststr);
            if (ret==0)
            {
              cons[x]->send_string(hoststr);
              cons[x]->send_string(". host: ");
              states[x]++;
              textpos[x][0]=0;
            }
            if (ret==-1)
            {
              JNL::addr_to_ipstr(cons[x]->get_remote(),hoststr,256);
              cons[x]->send_string(hoststr);
              cons[x]->send_string(". host: ");
              states[x]++;
              textpos[x][0]=0;
            }
          }
          if (states[x]==2)
          {
            char b;
            while (cons[x]->recv_bytes(&b,1) && states[x]==2)
            {
              if (b == '\r' || b == '\n')
              {
                if (strlen(textpos[x]))
                {
                  char *p=strstr(textpos[x],":");
                  int port=23;
                  if (p) 
                  {
                    *p++=0;
                    if (atoi(p)) port=atoi(p);
                  }
                  outcons[x]=new JNL_Connection(&dns);
                  outcons[x]->connect(textpos[x],port);

                  char str[512];
                  sprintf(str,"\r\nconnecting to port %d of %s\r\n",port,textpos[x]);
                  cons[x]->send_string(str);
                  states[x]++;
                }
                else states[x]=0;
              }
              else if (b == '\b') 
              {
                if (textpos[x][0])
                {
                  textpos[x][strlen(textpos[x])-1]=0;
                  cons[x]->send_string("\b \b");
              }
              }
              else
              {
                textpos[x][strlen(textpos[x])+1]=0;
                textpos[x][strlen(textpos[x])]=b;
                cons[x]->send(&b,1);
              }
            }
          }
          if (states[x]==3)
          {
            char buf[1024];
            outcons[x]->run();
            int l=cons[x]->recv_bytes(buf,1024);
            if (l) outcons[x]->send(buf,l);           
            l=outcons[x]->recv_bytes(buf,1024);
            if (l) cons[x]->send(buf,l);                     
          }
        }
      }
    }
  }
  JNL::close_socketlib();
  return 0;
}


#endif

#if (TEST_LISTEN)

int main()
{
  JNL_HTTPServ *cons[32]={0,};
  char *contents[32]={0,};
  int n_cons=0;
  
  JNL::open_socketlib();
  JNL_AsyncDNS dns;
  JNL_Listen l(8000);
  while (!l.is_error())
  {
    Sleep(100);
    if (n_cons<32)
    {
      JNL_Connection *con=l.get_connect();
      if (con)
      {
        int x;
        for (x = 0; x < 32; x ++)
        {
          if (!cons[x])
          {
            cons[x]=new JNL_HTTPServ(con);
            n_cons++;
            break;
          }
        }
      }
    }
    int x;
    for (x = 0; x < 32; x ++)
    {
      if (cons[x])
      {
        int r=cons[x]->run();
        if (r == -1 || r == 4)
        {
          if (r == -1) printf("error:%s\n",cons[x]->geterrorstr());
          delete cons[x];
          cons[x]=0;
          free(contents[x]);
          contents[x]=0;
          n_cons--;
        }
        if (r == 2)
        {
          cons[x]->set_reply_string("HTTP/1.1 200 OK");
          cons[x]->set_reply_header("Content-type:text/plain");
          cons[x]->set_reply_header("Server:JNLTest");
          contents[x]=(char*)malloc(32768);
          char *poop=cons[x]->get_request_parm("poop");
          sprintf(contents[x],"test, sucka\r\n%s\r\n\r\n",poop?poop:"no poop");
          cons[x]->send_reply();
        }
        if (r == 3)
        {
          if (contents[x] && cons[x]->bytes_cansend()>strlen(contents[x]))
          {
            cons[x]->write_bytes(contents[x],strlen(contents[x]));
            cons[x]->close(0);
            free(contents[x]);
            contents[x]=0;
          }
        }
      }
    }
  }
  JNL::close_socketlib();
  return 0;
}

#endif

#if (TEST_CONNECTION)
int main()
{
  JNL::open_socketlib();
  {
    JNL_AsyncDNS dns;
    JNL_Connection con(&dns);
    con.connect("localhost",80);
    FILE *fp=fopen("c:\\hi.raw","wb");
    while (1)
    {
      con.run();
      if (con.get_state()==JNL_Connection::STATE_ERROR)
      {
        printf("error %s\n",con.get_errstr());
      }
      if (con.get_state()==JNL_Connection::STATE_CLOSED)
      {
      }
      while (con.recv_bytes_available()>0)
      {
        char buf[1024];
        int a=con.recv_bytes_available();
        if (a > 1024) a=1024;
        con.recv_bytes(buf,a);
        fwrite(buf,a,1,fp);
      }
    }
    if (fp) fclose(fp);
  }
  JNL::close_socketlib();
  return 0;
}
#endif

#if (TEST_ASYNCDNS)
int main() 
{
  JNL_AsyncDNS dns;
  char *hosts[]=
  { 
    "www.firehose.net",
    "gnutella.com",
    "207.48.52.200",
    "www.slashdot.org",
    "www.google.com",
    "www.winamp.com",
    "www.genekan.com",
  };
  char *reverses[]=
  {
    "64.0.160.98",
    "205.188.245.120",
    "207.48.52.222",
    "207.48.52.200",
  };
  int n=0;
  int pass=0;
  while (n<sizeof(hosts)/sizeof(hosts[0])+sizeof(reverses)/sizeof(reverses[0]))
  {
    int x;
    n=0;
    printf("pass %d\n",pass++);
    for (x = 0; x < sizeof(hosts)/sizeof(hosts[0]); x ++)
    {
      unsigned int addr;
      printf("%-30s",hosts[x]);
      switch (dns.resolve(hosts[x],&addr))
      {
        case 0: 
          {
            char str[256];
            JNL::addr_to_ipstr(addr,str,256);
            printf("%s\n",str); 
            n++; 
          }
          break;
        case 1: printf("looking up\n"); break;
        case -1: printf("error\n"); n++; break;
      }
    }
    for (x = 0; x < sizeof(reverses)/sizeof(reverses[0]); x ++)
    {
      printf("reverse: %-21s",reverses[x]);
      char hn[512];
      switch (dns.reverse(JNL::ipstr_to_addr(reverses[x]),hn))
      {
        case 0: printf("%s\n",hn); n++; break;
        case 1: printf("looking up\n"); break;
        case -1: printf("error\n"); n++; break;
      }
    }
    Sleep(100);
  }
  return 0;
};
#endif

#if (TEST_UDP)
int main(int argc, char **argv)
{
  if (argc != 2)
  {
    printf("usage: udptest <mode>\n");
    printf("mode: 0 for client, 1 for server\n");
    exit(0);
  }
  int mode=atoi(argv[1]);

  JNL::open_socketlib();

  switch(mode)
  {
  case 0:   // client mode
    {
      JNL_AsyncDNS dns;
      JNL_UDPConnection con(0,&dns); // 0 chooses a random port
      con.setpeer("localhost",80);
      printf("Sending message...\n");
      con.send("blah",5);
      while (1)
      {
        con.run();
        if (con.get_state()==JNL_UDPConnection::STATE_ERROR)
        {
          printf("error %s\n",con.get_errstr());
        }
        while (con.recv_bytes_available()>0)
        {
          char buf[1024];
          int s=min(con.recv_bytes_available(), sizeof(buf));
          con.recv_bytes(buf,s);
          printf("received message: %s\n", buf);
        }
      }
    }
    break;
  case 1:   // server (listening) mode
    {
      JNL_UDPConnection con(80);
      printf("Waiting for messages...\n");
      while(1)
      {
        con.run();
        if (con.get_state()==JNL_UDPConnection::STATE_ERROR)
        {
          printf("error %s\n",con.get_errstr());
        }
        while (con.recv_bytes_available()>0)
        {
          char buf[1024];
          int s=min(con.recv_bytes_available(), sizeof(buf));
          con.recv_bytes(buf,s);
          printf("message received: %s. Replying...\n", buf);
          // reply on the addr:port from sender
          struct sockaddr from;
          con.get_last_recv_msg_addr(&from);
          con.setpeer(&from);
          con.send("blorp",6);
        }
      }
    }
    break;
  }

  JNL::close_socketlib();
  return 0;
}
#endif
