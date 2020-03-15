#ifndef _WDL_SHM_MSGREPLY_
#define _WDL_SHM_MSGREPLY_

#include <time.h>
#include "shm_connection.h"
#include "mutex.h"


/*
  4 byte message type

  4 byte unique message ID (0 for no reply needed/wanted)

  4 byte message length (following)


  type: 0 reply
    call specific return data
  type: all others user defined 

*/
// type is user defined, however type=0 is reserved for reply
class SHM_MsgReplyConnection
{
  
public:

  class WaitingMessage
  {
  public:
    WaitingMessage() { } 
    ~WaitingMessage() { }

    WaitingMessage *_next;

    int m_msgid;
    int m_msgtype;

    WDL_HeapBuf m_msgdata;
  };


  SHM_MsgReplyConnection(int bufsize, int maxqueuesize, bool dir, const char *uniquestr=NULL, int timeout_sec=0, int extra_flags=0);
  ~SHM_MsgReplyConnection();

  // be sure to set these, and have OnRecv() Reply() to any nonzero msgID !
  void *userData;
  WaitingMessage *(*OnRecv)(SHM_MsgReplyConnection *con, WaitingMessage *msg);
  bool (*IdleProc)(SHM_MsgReplyConnection *con); // return TRUE to abort (this will set the m_has_had_error to true / kill the connection)
  // can return NULL To temporarily buffer msg, can return a chain of msgs too to return them to the spare list

  // run as you wish, Send() will also run internally when waiting for reply
  // note: the checkForReplyID etc are for INTERNAL USE ONLY :)

  bool Run(bool runFull=true);

  // returns <0 if no reply, otherwise lenght of replybuf used
  // no retbuf = no wait for reply
  int Send(int type, const void *msg, int msglen,  
           void *replybuf, int maxretbuflen, const int *forceMsgID=NULL,
           const void *secondchunk=NULL, int secondchunklen=0, // allow sending two blocks as one message (for speed in certain instances)
           WDL_HeapBuf *hbreplyout=NULL);  // if hbreplyout is set it will get the full message (replybuf can be NULL then)
  void Reply(int msgID, const void *msg, int msglen);
  void Wait(HANDLE extraEvt=NULL);

  const char *GetUniqueString() { return m_uniq; }

  void ReturnSpares(WaitingMessage *msglist);

private:
  bool RunInternal(int checkForReplyID=0, WaitingMessage **replyPtr=0); // nonzero on error

  char m_uniq[256];
  WDL_Mutex m_shmmutex; 
  WDL_SHM_Connection *m_shm;


  WaitingMessage *m_waiting_replies;
  WaitingMessage *m_spares;

  int m_lastmsgid;
  int m_maxqueuesize;
  bool m_has_had_error;
};

#endif
