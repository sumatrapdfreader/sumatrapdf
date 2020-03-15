/*
** JNetLib
** Copyright (C) 2000-2001 Nullsoft, Inc.
** Author: Justin Frankel
** File: testbnc.cpp - JNL network bounce test code
** License: see jnetlib.h
*/

#ifdef _WIN32
#include <windows.h>
#else
#define Sleep(x) usleep((x)*1000)
#endif
#include <stdio.h>
#include "jnetlib.h"


int main(int argc, char *argv[])
{
  JNL_Connection *cons[32]={0,};
  JNL_Connection *outcons[32]={0,};
  int n_cons=0;

  if (argc != 4 || !atoi(argv[1]) || !atoi(argv[3]) || !argv[2][0])
  {
    printf("usage: redir localport host remoteport\n");
    exit(1);
  }
  
  JNL::open_socketlib();
  JNL_AsyncDNS dns;
  JNL_Listen l((short)atoi(argv[1]));
  printf("running...\n");
  while (!l.is_error())
  {
    Sleep(10);
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
            outcons[x]=new JNL_Connection();
            outcons[x]->connect(argv[2],atoi(argv[3]));
            cons[x]=con;
            char host[256];
            JNL::addr_to_ipstr(cons[x]->get_remote(),host,sizeof(host));
            n_cons++;
            printf("Connection %d (%s) opened (%d).\n",x,host,n_cons);
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
        outcons[x]->run();

        int cerr=(cons[x]->get_state() == JNL_Connection::STATE_ERROR || cons[x]->get_state()==JNL_Connection::STATE_CLOSED);
        int oerr=(outcons[x]->get_state() == JNL_Connection::STATE_ERROR || outcons[x]->get_state()==JNL_Connection::STATE_CLOSED);

        if ((!outcons[x]->send_bytes_in_queue() && !cons[x]->recv_bytes_available() && cerr) ||
            (!cons[x]->send_bytes_in_queue() && !outcons[x]->recv_bytes_available() && oerr) ||
            (cerr && oerr))
        {
          char host[256];
          JNL::addr_to_ipstr(cons[x]->get_remote(),host,sizeof(host));
          delete cons[x];
          delete outcons[x];
          outcons[x]=0;
          cons[x]=0;
          n_cons--;
          printf("Connection %d (%s) closed (%d)\n",x,host,n_cons);
        }
        else
        {
          char buf[4096];
          int l;
          l=outcons[x]->send_bytes_available();
          if (l > 4096) l=4096;
          if (l) l=cons[x]->recv_bytes(buf,l);
          if (l) outcons[x]->send(buf,l);           

          l=cons[x]->send_bytes_available();
          if (l > 4096) l=4096;
          if (l) l=outcons[x]->recv_bytes(buf,l);
          if (l) cons[x]->send(buf,l);                     
        }
      }
    }
  }
  JNL::close_socketlib();
  return 0;
}
