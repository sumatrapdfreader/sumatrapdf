#include "rfb_client.h"
#include "lice/lice.h"

#include "des.h"

#define CONNECTION_TIMEOUT 30
#define NORMAL_TIMEOUT 30
#define OUR_VERSION_STRING "RFB 003.000\n"


//#define WANT_RRE // seems to be broken on many servers

#define ENCODE_TYPE_RAW 0
#define ENCODE_TYPE_SCREENSIZE -223
#define ENCODE_TYPE_RRE 2
//#define ENCODE_TYPE_CORRE 3
//#define ENCODE_TYPE_HEXTILE 5


WDL_RFB_Client::WDL_RFB_Client(JNL_IConnection *con, const char *password)
{
  m_skipdata=0;
  m_req_x=m_req_y=m_req_w=m_req_h=m_needref=0;
  m_errstr=0;
  m_state=InitialState;
  m_con=con;
  m_password.Set(password?password:"");
  m_screen_w=m_screen_h=0;
  m_remote_ver=0;
  instance_data=0;
  DrawRectangleCallback=0;
  time(&m_lastt);
}

WDL_RFB_Client::~WDL_RFB_Client()
{
  delete m_con;
}


unsigned int WDL_RFB_Client::GetBE(int nb, int queueoffs, bool advance)
{
  unsigned char *buf=m_msg_buf.Get()+queueoffs;
  
  unsigned int a=0;
  if (nb==4) a= (buf[0]<<24) | (buf[1]<<16) | (buf[2]<<8) | buf[3];
  else if (nb==3) a=(buf[0]<<16) | (buf[1]<<8) | buf[2];
  else if (nb==2) a=(buf[0]<<8) | (buf[1]);
  else if (nb==1) a=(buf[0]);

  if (advance) m_msg_buf.Advance(queueoffs+nb);

  return a;
}

int WDL_RFB_Client::Run()
{
  if (!m_con||m_state == ErrorState) return -1;

  int cnt=0;

  while (m_state != ErrorState)
  {
    int bytes_needed=1;
    int old_avail  = m_msg_buf.Available();

    switch (m_state)
    {
      case InitialState:
        if (m_msg_buf.Available()>=12)
        {
          unsigned char *buf = m_msg_buf.Get();

          if (memcmp(buf,"RFB ",4) || buf[7] != '.'||buf[11] != '\n')
          {
            m_state=ErrorState;
            m_errstr = "Server gave invalid line";
          }
          else
          {
            buf[7]=0;
            buf[11]=0;
            m_remote_ver = atoi((char*)buf+4)*1000 + atoi((char*)buf+8);
            if (m_remote_ver < 3000 || m_remote_ver >= 4000)
            {
              m_state=ErrorState;
              m_errstr = "Server gave invalid version";
            }
            else
            {
              m_con->send_bytes(OUR_VERSION_STRING,strlen(OUR_VERSION_STRING));
              m_state = AuthWaitState;
            }
          }
          m_msg_buf.Advance(12); 
        }
        else bytes_needed=12;
      break;
      case AuthWaitState:
        if (m_msg_buf.Available()>=4)
        {
          unsigned int type = GetBE(4,0,false);
          if (type==0)
          {
            m_state=ErrorState;
            m_errstr = "Server permission denied";
          }
          else if (type==1) 
          {
            m_state=ServerInitState;
            m_msg_buf.Advance(4);

            char c=1; // allow sharing
            m_con->send_bytes(&c,1); 
          }
          else if (type == 2)
          {
            if (m_msg_buf.Available() >= 4+16)
            {
              m_msg_buf.Advance(4);
              unsigned char challenge[16];
              memcpy(challenge,m_msg_buf.Get(),16);
              m_msg_buf.Advance(16);

              unsigned char buf[8];
              memset(buf,0,sizeof(buf));
              memcpy(buf,m_password.Get(),wdl_min(strlen(m_password.Get()),8));
              WDL_DES des;
              des.SetKey(buf,true);
              des.Process8(challenge);
              des.Process8(challenge+8);
              m_con->send_bytes(challenge,16);
              m_state=AuthWaitState2;
            }
            else bytes_needed=4+16;
          } 
          else
          {
            m_state=ErrorState;
            m_errstr = "Unknown authentication method";
          }
        }
        else bytes_needed=4;
      break;
      case AuthWaitState2:
        if (m_msg_buf.Available()>=4)
        {
          unsigned int type = GetBE();
          if (type==0)
          {
            m_state=ServerInitState;

            char c=1; // allow sharing
            m_con->send_bytes(&c,1); 
          }
          else 
          {
            m_state=ErrorState;
            m_errstr = type==1 ? "Auth failed" : type==2 ? "Too many connections" : "Auth failed (unknown reason)";
          }
        }
        else bytes_needed=4;
      break;
      case ServerInitState:
        {
          if (m_msg_buf.Available()>=2+2+16+4)
          {
            unsigned int nl = GetBE(4,2+2+16,false);
            if (m_msg_buf.Available()>=2+2+16+4 + nl)
            {
              m_screen_w = GetBE(2);
              m_screen_h = GetBE(2);

              m_msg_buf.Advance(16+4); // skip color map and length

              memcpy(m_namebuf.Resize(nl+1),m_msg_buf.Get(),nl);
              m_msg_buf.Advance(nl);
              m_namebuf.Get()[nl]=0;

              // request our pixel format

              {
                unsigned char buf[20]={0,};
                buf[0] = 0; // request pixel format
                // 3 bytes padding
                buf[4] = 32;
                buf[5] = 24;
                buf[6] = 0; // always LE
                buf[7] = 1; // true-color

                buf[8] = 0; buf[9]=255; // masks
                buf[10] = 0; buf[11]=255;
                buf[12] = 0; buf[13]=255;

                buf[14] = LICE_PIXEL_R*8; // shifts
                buf[15] = LICE_PIXEL_G*8;
                buf[16] = LICE_PIXEL_B*8;

                // 3 bytes padding

                m_con->send_bytes(buf,sizeof(buf));
              }
              // request encodings
              {
                const int encs[]={
#ifdef WANT_RRE
                  ENCODE_TYPE_RRE,
#endif
                    ENCODE_TYPE_RAW,ENCODE_TYPE_SCREENSIZE,};
                const int nencs = sizeof(encs)/sizeof(encs[0]);

                unsigned char buf[4+nencs*4]={0,};
                buf[0]=2; //encodings
                buf[2] = (nencs>>8)&0xff;
                buf[3] = nencs&0xff;
                int x;
                for(x=0;x<nencs;x++)
                {
                  buf[4+x*4] = (encs[x]>>24)&0xff;
                  buf[4+x*4+1] = (encs[x]>>16)&0xff;
                  buf[4+x*4+2] = (encs[x]>>8)&0xff;
                  buf[4+x*4+3] = (encs[x])&0xff;
                }
                m_con->send_bytes(buf,sizeof(buf));
              }

              m_state=RunState;
              m_needref=2;
            }
            else bytes_needed=2+2+16+4+nl;
          }
          else bytes_needed=2+2+16+4;
        }
      break;
      case RunState:

        if (m_needref)
        {
          unsigned char buf[10]={0,};
          buf[0]=3;
          buf[1]=(m_needref & 2)?false:true;
          if (!m_req_w || !m_req_h)
          {
            buf[6]=(m_screen_w>>8)&0xff;
            buf[7]=m_screen_w&0xff;
            buf[8]=(m_screen_h>>8)&0xff;
            buf[9]=m_screen_h&0xff;
          }
          else
          {
            buf[2]=(m_req_x>>8)&0xff;
            buf[3]=m_req_x&0xff;
            buf[4]=(m_req_x>>8)&0xff;
            buf[5]=m_req_x&0xff;
            buf[6]=(m_req_w>>8)&0xff;
            buf[7]=m_req_w&0xff;
            buf[8]=(m_req_h>>8)&0xff;
            buf[9]=m_req_h&0xff;
          }
          m_needref=0;
          m_con->send_bytes(buf,sizeof(buf));
        }

        if (m_skipdata>0)
        {
          int a= wdl_min(m_msg_buf.Available(),m_skipdata);
          m_msg_buf.Advance(a);
          m_skipdata-=a;
        }

        if (m_msg_buf.Available()>0)
        {
          m_skipdata=0;
          unsigned char msg = *(unsigned char *)m_msg_buf.Get();
          switch (msg)
          {
            case 0: // framebuffer update
              if (m_msg_buf.Available() >= 4)
              {
                unsigned char *buf=m_msg_buf.Get();
                m_msg_buf.Advance(4);

                m_msg_state=(buf[2]<<8)+buf[3];
                if (m_msg_state>0)
                {
                  m_state=RunState_GettingRects;
                }
              }
              else bytes_needed=4;
            break;
            case 1: // color map crap
              if (m_msg_buf.Available()>=6)
              {
                m_skipdata = GetBE(2,4)*6; // skip 4 bytes, read 2, then skip that
              }
            break;
            case 2: // bell, skip
              m_msg_buf.Advance(1);
            break;
            case 3: // copy text, skip
              if (m_msg_buf.Available()>=8)
              {
                m_skipdata=GetBE(4,4); // skip 4 bytes, read 4
              }
            break;
            default:
              m_state=ErrorState;
              sprintf(tmperrbuf,"Got unknown message: %d",msg);
              m_errstr=tmperrbuf;
            break;
          }
        }
      break;
      case RunState_GettingRects:
        if (m_msg_state>0)
        {
          if (m_msg_buf.GetSize()>=12)
          {
            int xpos = GetBE(2,0,false);
            int ypos = GetBE(2,2,false);
            int w=GetBE(2,4,false);
            int h=GetBE(2,6,false);
            int enc = GetBE(4,8,false);

            switch (enc)
            {
              case ENCODE_TYPE_SCREENSIZE:
                {
                  if (w>m_screen_w || h>m_screen_h) m_needref=2;
                  m_screen_w = w;
                  m_screen_h = h;
                  m_msg_buf.Advance(12);
                }
              break;
              case ENCODE_TYPE_RRE:
                if (m_msg_buf.GetSize()>=12+8) 
                {
                  unsigned int nr = GetBE(4,12,false);
                  if (m_msg_buf.GetSize()>=12+8 + nr*12) 
                  {
                    LICE_pixel bgc = GetBE(4,12+4);
                    if (DrawRectangleCallback) 
                    {
                      m_bm.resize(w,h);
                      LICE_FillRect(&m_bm,0,0,w,h,bgc,1.0f,LICE_BLIT_MODE_COPY);

                      while (nr-->0)
                      {
                        bgc=GetBE();
                        int lx=GetBE(2);
                        int ly=GetBE(2);
                        int lw=GetBE(2);
                        int lh=GetBE(2);
                        LICE_FillRect(&m_bm,lx,ly,lw,lh,bgc,1.0f,LICE_BLIT_MODE_COPY);
                      }                      

                      DrawRectangleCallback(this,&m_bm,xpos,ypos,w,h);
                    }
                    else m_msg_buf.Advance(nr*12);                   

                    --m_msg_state;
                  }
                  else bytes_needed = 12+8 + nr*12;
                }
                else bytes_needed=12+8;
              break;
              case ENCODE_TYPE_RAW: // raw  
                if (m_msg_buf.GetSize()>=12+w*h*4) // we only support 32bpp for now
                {
                  if (DrawRectangleCallback) 
                  {
                    LICE_pixel *src = (LICE_pixel *)(m_msg_buf.Get()+12);
                    m_bm.resize(w,h);
                    LICE_pixel *out = m_bm.getBits();
                    int rs = m_bm.getRowSpan();
                    int a;
                    for (a=0;a<h;a++)
                    {
                      memcpy(out,src,w*sizeof(LICE_pixel));
                      src+=w;
                      out+=rs;
                    }


                    DrawRectangleCallback(this,&m_bm,xpos,ypos,w,h);
                  }
            
                  m_msg_buf.Advance(12 + w*h*4);
                  --m_msg_state;
                }
                else bytes_needed=12+w*h*4;
              break;
              default:
                m_state=ErrorState;
                sprintf(tmperrbuf,"Got unknown encoding: %d",enc);
                m_errstr=tmperrbuf;
              break;
            }
          }
          else bytes_needed=12;
        }

        if (m_msg_state<=0)
        {
          m_state = RunState;
          if (DrawRectangleCallback) DrawRectangleCallback(this,NULL,0,0,0,0);
        }
      break;
    }

    if (old_avail != m_msg_buf.Available()) cnt=1;

    m_con->run();

    while (m_msg_buf.Available()<m_skipdata+bytes_needed)
    {
      int a = m_con->recv_bytes_available();
      if (a>0)
      {
        m_con->recv_bytes(m_msg_buf.Add(NULL,a),a);
        time(&m_lastt);
      }
      else break;

      m_con->run();
    }

    if (old_avail == m_msg_buf.Available()||
        m_msg_buf.Available()<m_skipdata+bytes_needed) break;
  }

  m_msg_buf.Compact();

  if (m_state != ErrorState)
    if (time(NULL) > m_lastt+(m_state < RunState ? CONNECTION_TIMEOUT : NORMAL_TIMEOUT) || m_con->get_state()==JNL_Connection::STATE_CLOSED|| m_con->get_state()==JNL_Connection::STATE_ERROR)
    {
      m_state=ErrorState;
      m_errstr = "Timed out";
    }

  return cnt>0;
}

