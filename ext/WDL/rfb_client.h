#ifndef _WDL_RFBCLIENT_H_
#define _WDL_RFBCLIENT_H_

#include "wdlstring.h"
#include "queue.h"
#include "jnetlib/jnetlib.h"
#include "lice/lice.h"

class WDL_RFB_Client
{
public:
  WDL_RFB_Client(JNL_IConnection *con, const char *password);
  ~WDL_RFB_Client();

  int GetScreenWidth() { return m_screen_w; }
  int GetScreenHeight() { return m_screen_h; }

  int Run();  // <0 on disconnect,
  const char *GetError() { return m_errstr; }

  void Invalidate() { m_needref=2; } // tell server to re-send
  void SetUpdateRegion(int x, int y, int w, int h) // if w or h are 0 then whole screen is used
  {
    m_req_x=x;
    m_req_y=y;
    m_req_w=w;
    m_req_h=h;

  }
  void RequestUpdate() { m_needref|=1; }

  void *instance_data;
  void (*DrawRectangleCallback)(WDL_RFB_Client *_this, LICE_IBitmap *drawimg, int dest_x, int dest_y, int dest_w, int dest_h);


private:

  enum { ErrorState=-1, InitialState=0, AuthWaitState,  AuthWaitState2, ServerInitState, RunState, RunState_GettingRects};

  int m_remote_ver; // xxxyyy

  int m_state;
  time_t m_lastt;
  const char *m_errstr;

  WDL_String m_password;
  JNL_IConnection *m_con;

  int m_screen_w, m_screen_h;

  int m_req_x,m_req_y,m_req_w,m_req_h, m_needref;

  WDL_TypedBuf<char> m_namebuf;
  WDL_TypedQueue<unsigned char> m_msg_buf;

  int m_skipdata;

  int m_msg_state; // state specific value / data

  unsigned int GetBE(int nb=4, int queueoffs=0, bool advance=true);

  LICE_MemBitmap m_bm;

  char tmperrbuf[128];
};




#endif//_WDL_RFBCLIENT_H_