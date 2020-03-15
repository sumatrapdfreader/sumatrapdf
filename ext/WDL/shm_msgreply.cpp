#include "shm_msgreply.h"
#include "wdlcstring.h"

//#define VERIFY_MESSAGES // this is not endian-aware (so it'll fail if enabled and doing ppc<->x86 etc)
#ifdef VERIFY_MESSAGES
#define WDL_SHA1 WDL_SHA1_msgreplydef
#include "sha.cpp"
#endif

SHM_MsgReplyConnection::SHM_MsgReplyConnection(int bufsize, int maxqueuesize, bool dir, const char *uniquestr, int timeout_sec, int extra_flags)
{
  m_maxqueuesize=maxqueuesize;
  m_has_had_error=false;
  userData=0;
  OnRecv=0;
  IdleProc=0;
  m_lastmsgid=1;
  m_shm = 0;
  m_spares=0;
  m_waiting_replies=0;

  if (uniquestr) lstrcpyn_safe(m_uniq,uniquestr,sizeof(m_uniq));
  else
  {
#ifdef _WIN32
    WDL_INT64 pid = (WDL_INT64) GetCurrentProcessId();
#else
    WDL_INT64 pid = (WDL_INT64) getpid();
#endif
    WDL_INT64 thisptr = (WDL_INT64) (INT_PTR) this;
    static int cnt=0xdeadf00d;
    sprintf(m_uniq,"%08x%08x%08x%08x",
      (int)(pid&0xffffffff),
      (int)(pid>>32),
      (int)(thisptr&0xffffffff) ^ GetTickCount(),
      (int)(thisptr>>32)^(cnt++));
  }

  m_shm = new WDL_SHM_Connection(dir,m_uniq,bufsize,timeout_sec,extra_flags);

}

SHM_MsgReplyConnection::~SHM_MsgReplyConnection()
{
  delete m_shm;
  WaitingMessage *tmp=m_waiting_replies;
  while (tmp)
  {
    WaitingMessage *p=tmp;
    tmp=tmp->_next;
    delete p;
  }
  tmp=m_spares;
  while (tmp)
  {
    WaitingMessage *p=tmp;
    tmp=tmp->_next;
    delete p;
  }
}

void SHM_MsgReplyConnection::Reply(int msgID, const void *msg, int msglen)
{
  if (msgID) Send(0,msg,msglen,NULL,0,&msgID);
}


int SHM_MsgReplyConnection::Send(int type, const void *msg, int msglen,  
                           void *replybuf, int maxretbuflen, const int *forceMsgID,
                           const void *secondchunk, int secondchunklen,
                           WDL_HeapBuf *hbreplyout)
{
  if (!m_shm||m_has_had_error) return -1;

  if (secondchunk && secondchunklen>0) msglen+=secondchunklen;
  else secondchunklen=0;

  int msgid;
  {
    WDL_MutexLock lock(&m_shmmutex);
    m_shm->send_queue.AddDataToLE(&type,4,4);

    if (forceMsgID) msgid = *forceMsgID;
    else
    {
      if (!replybuf&&!hbreplyout) msgid=0;
      else if (!(msgid = ++m_lastmsgid)) msgid = ++m_lastmsgid;
    }

    m_shm->send_queue.AddDataToLE(&msgid,4,4);
    m_shm->send_queue.AddDataToLE(&msglen,4,4);
    if (msglen>secondchunklen) m_shm->send_queue.Add(msg,msglen-secondchunklen);
    if (secondchunklen>0) m_shm->send_queue.Add(secondchunk,secondchunklen);

#ifdef VERIFY_MESSAGES
    WDL_SHA1 t;
    t.add(&type,4);
    t.add(&msgid,4);
    t.add(&msglen,4);
    if (msglen>secondchunklen) t.add(msg,msglen-secondchunklen);
    if (secondchunklen>0) t.add(secondchunk,secondchunklen);

    char tb[WDL_SHA1SIZE];
    t.result(tb);
    m_shm->send_queue.Add(tb,sizeof(tb));
#endif


    if ((!replybuf && !hbreplyout) || !msgid) m_shm->Run(); // get this reply out ASAP
  }

  if ((hbreplyout||replybuf) && msgid)
  {
    int wait_cnt=30; // dont run idleproc for first Xms or so

    while (!m_has_had_error)
    {
      if (wait_cnt<=0 && IdleProc && IdleProc(this))
      {
        m_has_had_error=true;
        break;
      }

      WaitingMessage *wmsg=NULL;
      bool r = RunInternal(msgid,&wmsg);

      if (wmsg)
      {
        int rv = wmsg->m_msgdata.GetSize();

        if (hbreplyout)
        {
          memcpy(hbreplyout->Resize(rv,false),wmsg->m_msgdata.Get(),rv);
        }

        if (replybuf)
        {
          if (rv > maxretbuflen) rv=maxretbuflen;
          if (rv>0) memcpy(replybuf,wmsg->m_msgdata.Get(),rv);
        }

        m_shmmutex.Enter();
        wmsg->_next = m_spares;
        m_spares=wmsg;
        m_shmmutex.Leave();
        return rv;
      }
      else if (r) break;


      if (wait_cnt>0) wait_cnt--;

      HANDLE evt=m_shm->GetWaitEvent();
      if (evt) WaitForSingleObject(evt,1);
      else Sleep(1);

    }
  }

  if (hbreplyout) hbreplyout->Resize(0,false);

  return -1;
}

void SHM_MsgReplyConnection::Wait(HANDLE extraEvt)
{
  HANDLE evt=m_shm ? m_shm->GetWaitEvent() : extraEvt;
  if (evt && extraEvt && evt != extraEvt)
  {
    HANDLE hds[2] = {evt,extraEvt};
#ifdef _WIN32
    WaitForMultipleObjects(2,hds,FALSE,1);
#else
    WaitForAnySocketObject(2,hds,1);
#endif
  }
  else if (evt) WaitForSingleObject(evt,1);
  else Sleep(1);
}

void SHM_MsgReplyConnection::ReturnSpares(WaitingMessage *msglist)
{
  if (msglist)
  {
    WaitingMessage *msgtail = msglist;
    while (msgtail && msgtail->_next) msgtail=msgtail->_next;

    m_shmmutex.Enter(); 
    msgtail->_next = m_spares;
    m_spares=msglist;
    m_shmmutex.Leave(); 
  }
}

bool SHM_MsgReplyConnection::Run(bool runFull)
{ 
  if (m_has_had_error) return true;

  if (runFull) return RunInternal(); 

  m_shmmutex.Enter();
  int s=m_shm->Run();
  if (m_shm->send_queue.Available() > m_maxqueuesize) s=-1;
  m_shmmutex.Leave();
  
  if (s<0) m_has_had_error=true;
  else if (m_shm && m_shm->WantSendKeepAlive()) 
  {
    int zer=0;
    Send(0,NULL,0,NULL,0,&zer);
  }

  return s<0;
} 

bool SHM_MsgReplyConnection::RunInternal(int checkForReplyID, WaitingMessage **replyPtr)
{
  if (!m_shm||m_has_had_error) return true;

  if (replyPtr) *replyPtr=0;

  int s=0;
  
  do
  {
    m_shmmutex.Enter();
    
    // autocompact on first time through
    if (!s) m_shm->recv_queue.Compact();

    s = m_shm->Run();
    if (m_shm->send_queue.Available() > m_maxqueuesize) s=-1;

    while (m_shm->recv_queue.GetSize()>=12)
    {
      int datasz = *(int *)((char *)m_shm->recv_queue.Get()+8);
      WDL_Queue::WDL_Queue__bswap_buffer(&datasz,4); // convert to LE if needed

      if (m_shm->recv_queue.GetSize() < 12 + datasz) break;

#ifdef VERIFY_MESSAGES
      if (m_shm->recv_queue.GetSize() < 12 + datasz + WDL_SHA1SIZE) break;
#endif

      int type = *(int *)((char *)m_shm->recv_queue.Get());
      WDL_Queue::WDL_Queue__bswap_buffer(&type,4); // convert to LE if needed
      
      WaitingMessage *msg = m_spares;
      if (msg) m_spares = m_spares->_next;
      else msg = new WaitingMessage;

      msg->m_msgid = *(int *)((char *)m_shm->recv_queue.Get() + 4);
      WDL_Queue::WDL_Queue__bswap_buffer(&msg->m_msgid,4); // convert to LE if needed

      msg->m_msgtype = type;
      memcpy(msg->m_msgdata.Resize(datasz,false),(char *)m_shm->recv_queue.Get()+12, datasz);

      m_shm->recv_queue.Advance(12+datasz);

#ifdef VERIFY_MESSAGES
      WDL_SHA1 t;
      t.add(&type,4);
      t.add(&msg->m_msgid,4);
      t.add(&datasz,4);
      t.add(msg->m_msgdata.Get(),msg->m_msgdata.GetSize());
      char tb[WDL_SHA1SIZE];
      t.result(tb);
      if (memcmp(m_shm->recv_queue.Get(),tb,WDL_SHA1SIZE))
        MessageBox(NULL,"FAIL","A",0);
      m_shm->recv_queue.Advance(WDL_SHA1SIZE);
#endif


      if (type==0)
      {
        if (checkForReplyID && replyPtr && !*replyPtr && 
            checkForReplyID == msg->m_msgid)
        {
          *replyPtr = msg;
          s=0;
          break; // we're done!
        }
        else
        {
          msg->_next = m_waiting_replies;
          m_waiting_replies = msg;
        }
      }
      else 
      {
        m_shmmutex.Leave(); 

        WaitingMessage *msgtail=NULL;

        if (OnRecv) 
        {
          msg->_next=0;
          msgtail = msg = OnRecv(this,msg);          
          while (msgtail && msgtail->_next) msgtail=msgtail->_next;
        }
        else if (msg->m_msgid) Reply(msg->m_msgid,"",0); // send an empty reply

        m_shmmutex.Enter(); // get shm again

        if (msg)
        {
          (msgtail?msgtail:msg)->_next = m_spares;
          m_spares=msg;
        }
      }
    } // while queue has stuff

    if (checkForReplyID && replyPtr && !*replyPtr)
    {
      WaitingMessage *m=m_waiting_replies;
      WaitingMessage *lp=NULL;

      while (m)
      {
        if (m->m_msgid == checkForReplyID)
        {
          if (lp) lp->_next = m->_next;
          else m_waiting_replies=m->_next;
          
          *replyPtr = m;
          s=0; // make sure we return ASAP
          break;
        }
        lp = m;
        m=m->_next;
      }
    }

    m_shmmutex.Leave();

  } while (s>0);

  if (s<0) m_has_had_error=true;
  else if (m_shm && m_shm->WantSendKeepAlive())
  {
    int zer=0;
    Send(0,NULL,0,NULL,0,&zer);
  }
  return s<0; 
}

