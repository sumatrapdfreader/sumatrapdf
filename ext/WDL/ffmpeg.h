/*
    WDL - ffmpeg.h
    Copyright (C) 2005 Cockos Incorporated
    Copyright (C) 1999-2004 Nullsoft, Inc. 
  
    This software is provided 'as-is', without any express or implied
    warranty.  In no event will the authors be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
       claim that you wrote the original software. If you use this software
       in a product, an acknowledgment in the product documentation would be
       appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be
       misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.
      
*/

#ifndef _WDL_FFMPEG_H
#define _WDL_FFMPEG_H

#ifdef _MSC_VER
#include "../sdks/ffmpeg/include/stdint.h"
#include "../sdks/ffmpeg/include/inttypes.h"
#endif

extern "C"
{
#include "../sdks/ffmpeg/include/libavformat/avformat.h"
#include "../sdks/ffmpeg/include/libswscale/swscale.h"
};

#include "queue.h"

#ifndef INT64_C
#define INT64_C(val) val##i64
#endif

#ifndef INT64_MIN
#ifdef _MSC_VER
#define INT64_MIN       (-0x7fffffffffffffff##i64 - 1)
#else
#define INT64_MIN       (-0x7fffffffffffffffLL - 1)
#endif
#endif

#ifndef INT64_MAX
#define INT64_MAX INT64_C(9223372036854775807)
#endif

class WDL_VideoEncode
{
public:
  //bitrates are in kbps
  WDL_VideoEncode(const char *format, int width, int height, double fps, int bitrate, const char *audioformat=NULL, int asr=44100, int ach=2, int abitrate=0)
  {
    m_init = 0;
    m_img_resample_ctx = NULL;
    m_stream = NULL;
    m_astream = NULL;
    m_video_enc = NULL;
    m_audio_enc = NULL;
    m_bit_buffer = NULL;
    avcodec_get_frame_defaults(&m_cvtpic);
    
    //initialize FFMpeg
    {
      static int init = 0;
      if(!init) av_register_all();
      init = 1;
    }

    m_ctx = av_alloc_format_context();
    AVOutputFormat *fmt = guess_format(format, NULL, NULL);
    if(!m_ctx || !fmt) return;

    m_ctx->oformat = fmt;
    
    m_stream = av_new_stream(m_ctx, m_ctx->nb_streams); 
    if(!m_stream) return;

    //init video
    avcodec_get_context_defaults2(m_stream->codec, CODEC_TYPE_VIDEO);
    m_video_enc = m_stream->codec;

    CodecID codec_id = av_guess_codec(m_ctx->oformat, NULL, NULL, NULL, CODEC_TYPE_VIDEO);
    m_video_enc->codec_id = codec_id;

    AVCodec *codec;
    codec = avcodec_find_encoder(codec_id);
    if (!codec) return;

    m_video_enc->width = width;
    m_video_enc->height = height;
    m_video_enc->time_base.den = fps * 10000;
    m_video_enc->time_base.num = 10000;
    
    m_video_enc->pix_fmt = PIX_FMT_BGRA;
    if (codec && codec->pix_fmts) 
    {
        const enum PixelFormat *p= codec->pix_fmts;
        for (; *p!=-1; p++) {
            if (*p == m_video_enc->pix_fmt)
                break;
        }
        if (*p == -1)
            m_video_enc->pix_fmt = codec->pix_fmts[0];
    }

    if(m_video_enc->pix_fmt != PIX_FMT_BGRA)
    {
      //this codec needs colorplane conversion
      int sws_flags = SWS_BICUBIC;
      m_img_resample_ctx = sws_getContext(
                                     width,
                                     height,
                                     PIX_FMT_BGRA,
                                     width,
                                     height,
                                     m_video_enc->pix_fmt,
                                     sws_flags, NULL, NULL, NULL);

      if ( avpicture_alloc( (AVPicture*)&m_cvtpic, m_video_enc->pix_fmt,
                          m_video_enc->width, m_video_enc->height) )
        return;
    }

    m_video_enc->bit_rate = bitrate*1024;
    m_video_enc->gop_size = 12; /* emit one intra frame every twelve frames at most */

    // some formats want stream headers to be separate 
    if(m_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        m_video_enc->flags |= CODEC_FLAG_GLOBAL_HEADER; 

    m_video_enc->max_qdiff = 3; // set the default maximum quantizer difference between frames
    m_video_enc->thread_count = 1; // set how many thread need be used in encoding
    m_video_enc->rc_override_count = 0; // set ratecontrol override to 0
    if (!m_video_enc->rc_initial_buffer_occupancy) 
    {
        m_video_enc->rc_initial_buffer_occupancy = m_video_enc->rc_buffer_size*3/4; // set decoder buffer size
    }
    m_video_enc->me_threshold = 0; // set motion estimation threshold value to 0
    m_video_enc->intra_dc_precision = 0;
    m_video_enc->strict_std_compliance = 0;
    m_ctx->preload = (int)(0.5 * AV_TIME_BASE);
    m_ctx->max_delay = (int)(0.7 * AV_TIME_BASE);
    m_ctx->loop_output = -1;
    
    m_ctx->timestamp = 0;

    av_log_set_callback(ffmpeg_avcodec_log);
    av_log_set_level(AV_LOG_ERROR);

    if (avcodec_open(m_video_enc, codec) < 0) 
    {
      return;
    }

    //init audio
    if(abitrate)
    {
      m_astream = av_new_stream(m_ctx, m_ctx->nb_streams); 
      if(!m_astream) return;

      avcodec_get_context_defaults2(m_astream->codec, CODEC_TYPE_AUDIO);
      m_audio_enc = m_astream->codec;

      //use the format's default audio codec
      CodecID codeca_id = av_guess_codec(m_ctx->oformat, audioformat, NULL, NULL, CODEC_TYPE_AUDIO);
      m_audio_enc->codec_id = codeca_id;

      AVCodec *acodec;
      acodec = avcodec_find_encoder(codeca_id);
      if (!acodec) return;

      m_audio_enc->bit_rate = abitrate*1024;
      m_audio_enc->sample_rate = asr;
      m_audio_enc->channels = ach;

      if (avcodec_open(m_audio_enc, acodec) < 0) return;
    }

    AVFormatParameters params, *ap = &params;
    memset(ap, 0, sizeof(*ap));
    if (av_set_parameters(m_ctx, ap) < 0) return;
  
    url_open_dyn_buf(&m_ctx->pb);
    av_write_header(m_ctx);
    
    int size = width * height;
    m_bit_buffer_size = 1024 * 256;
    m_bit_buffer_size= FFMAX(m_bit_buffer_size, 4*size);

    m_bit_buffer = (uint8_t*)av_malloc(m_bit_buffer_size);

    m_init = 1;
  }
  ~WDL_VideoEncode()
  {
    if(m_stream && m_stream->codec) avcodec_close(m_stream->codec);     
    if(m_astream && m_astream->codec) avcodec_close(m_astream->codec);     
    av_free(m_bit_buffer);
    av_free(m_cvtpic.data[0]);
    av_free(m_ctx);
  }
  
  int isInited() { return m_init; }

  void encodeVideo(const LICE_pixel *buf)
  {
    if(m_img_resample_ctx)
    {
      //convert to output format
      uint8_t *p[1]={(uint8_t*)buf};
      int w[1]={m_video_enc->width*4};
      sws_scale(m_img_resample_ctx, p, w,
              0, m_video_enc->height, m_cvtpic.data, m_cvtpic.linesize);
    }
    int ret = avcodec_encode_video(m_video_enc, m_bit_buffer, m_bit_buffer_size, &m_cvtpic);
    if(ret>0)
    {
      AVPacket pkt;
      av_init_packet(&pkt);
      pkt.stream_index = 0;
      pkt.data = m_bit_buffer;
      pkt.size = ret;
      if (m_video_enc->coded_frame->pts != AV_NOPTS_VALUE)
            pkt.pts= av_rescale_q(m_video_enc->coded_frame->pts, m_video_enc->time_base, m_stream->time_base);
      if(m_video_enc->coded_frame->key_frame)
        pkt.flags |= PKT_FLAG_KEY; 
      av_interleaved_write_frame(m_ctx, &pkt);
    }
  }

  void encodeAudio(short *data, int nbsamples)
  {
    AVPacket pkt;
    int l = nbsamples;
    int fs = m_audio_enc->frame_size*m_audio_enc->channels;
    while(l>=fs)
    {
      av_init_packet(&pkt);
      pkt.size= avcodec_encode_audio(m_audio_enc, m_bit_buffer, m_bit_buffer_size, data); 
      if (m_audio_enc->coded_frame->pts != AV_NOPTS_VALUE)
        pkt.pts= av_rescale_q(m_audio_enc->coded_frame->pts, m_audio_enc->time_base, m_astream->time_base);
      pkt.flags |= PKT_FLAG_KEY;
      pkt.stream_index = 1;
      pkt.data = m_bit_buffer;
      av_interleaved_write_frame(m_ctx, &pkt);

      data += fs;
      l -= fs;
    }
  }

  int getBytes(unsigned char *p, int size)
  {
    //looks like there's no other way to get data from ffmpeg's dynamic buffers apart from closing them
    if (m_queue.GetSize() < size && m_init)
    {
      uint8_t *pb_buffer;
      int l = url_close_dyn_buf(m_ctx-> pb, &pb_buffer);
      if(l > 0)
      {
        m_queue.Add(pb_buffer, l);
        av_free(pb_buffer);
      }
      url_open_dyn_buf(&m_ctx->pb); //sets up next dynamic buffer for ffmpeg
    }

    int s = wdl_min(size, m_queue.GetSize());
    if(s)
    {
      memcpy(p, m_queue.Get(), s);
      m_queue.Advance(s);
      m_queue.Compact();
    }
    return s;
  }

  void close()
  {
    av_write_trailer(m_ctx);
    uint8_t *pb_buffer;
    int l = url_close_dyn_buf(m_ctx-> pb, &pb_buffer);
    if(l)
    {
      m_queue.Add(pb_buffer, l);
      av_free(pb_buffer);
    }
    m_init=0;
  }

  //useful to get debugging information from ffmpeg
  static void ffmpeg_avcodec_log(void *ptr, int val, const char * msg, va_list ap)
  {
    AVClass* avc= ptr ? *(AVClass**)ptr : NULL;
    vprintf(msg, ap);
  }

protected:
  int m_init;

  AVFormatContext *m_ctx;
  AVStream *m_stream, *m_astream;
  AVCodecContext *m_video_enc, *m_audio_enc;
  struct SwsContext *m_img_resample_ctx;
  AVFrame m_cvtpic;
  uint8_t *m_bit_buffer;
  int m_bit_buffer_size;

  WDL_Queue m_queue;
};

class WDL_VideoDecode
{
public:
  WDL_VideoDecode(const char *fn)
  {
    m_inited = 0;
    m_ctx = NULL;
    m_frame = NULL;
    m_ic = NULL;
    m_sws = NULL;
    m_sws_destw=0;
    m_sws_desth=0;
    m_curtime=-1.0;

    //initialize FFMpeg
    {
      static int init = 0;
      if(!init) av_register_all();
      init = 1;
    }
    
    int ret = av_open_input_file(&m_ic, fn, NULL, 0, NULL);
    if (ret < 0) return;

    ret = av_find_stream_info(m_ic);
    if (ret < 0) return;
    
    // find the stream that corresponds to the stream type
    int i, stream = -1;
    for(i=0; i < (int)m_ic->nb_streams; i++)
    {
      int st = m_ic->streams[i]->codec->codec_type;
      if(st==CODEC_TYPE_VIDEO)
      {
        stream = i;
        break;
      }
    }
    if(stream==-1) return; //no stream found

    m_ctx = m_ic->streams[stream]->codec;

    AVCodec *pCodec = avcodec_find_decoder(m_ctx->codec_id);
    if(pCodec == NULL) return; // codec not found

    if(avcodec_open(m_ctx, pCodec)<0) return; // Could not open codec
    
    AVStream *st = m_ic->streams[stream];
    if(st->r_frame_rate.den && st->r_frame_rate.num)
      m_fps = av_q2d(st->r_frame_rate);
    else
      m_fps = 1/av_q2d(st->codec->time_base);
    
    m_frame = avcodec_alloc_frame();
    
    m_w = m_ctx->width;
    m_h = m_ctx->height;

    m_pixfmt=st->codec->pix_fmt;
        
    if(m_ic->duration == AV_NOPTS_VALUE)
    {
      //FFmpeg can't get the duration
      //approximate the duration of the file with the first packets bitrates
      AVStream *st = m_ic->streams[stream];
      int bitrate = 0;
      for(i=0; i < (int)m_ic->nb_streams; i++)
      {
        bitrate += m_ic->streams[i]->codec->bit_rate;
      }
      bitrate /= 8;
      if(bitrate)
        m_len = (double)m_ic->file_size/bitrate;
      else
        m_len = 30; //last resort
    }
    else
      m_len = (double)m_ic->duration/AV_TIME_BASE;
    m_stream = stream;
   
    m_inited = 1;
  }
  ~WDL_VideoDecode()
  {
    if(m_frame) av_free(m_frame);
    if(m_ic) av_close_input_file(m_ic);
    if(m_sws) sws_freeContext(m_sws);
  }
  int isInited() { return m_inited; }
  int GetVideoFrameAtTime(LICE_IBitmap *dst, double atTime, double *startTime, double *endTime, bool resizeToBuf)
  {
    if(!m_inited) return 0;
    if(m_curtime == -1.0 || atTime<m_curtime || (atTime-m_curtime)>(1.0/m_fps))
    {
      if(avformat_seek_file(m_ic, -1, INT64_MIN, atTime*AV_TIME_BASE, INT64_MAX, AVSEEK_FLAG_BACKWARD) < 0)
      {
        //fallback to old seeking API
        av_seek_frame(m_ic, -1, atTime*AV_TIME_BASE, AVSEEK_FLAG_BACKWARD);
      }
      avcodec_flush_buffers(m_ctx);
    }

    double startpts = -1;
    while(1)
    {
      AVPacket packet;
      if(av_read_frame(m_ic, &packet)<0) return 0; //end of file
      if(packet.stream_index==m_stream)
      {
        double packetpts = getPresentationTime(&packet);
        if(startpts == -1) startpts = packetpts;

        int frameFinished = 0;
        int l = avcodec_decode_video(m_ctx, m_frame, &frameFinished, packet.data, packet.size);
        if(l>=0)
        {
          // Did we get a video frame?
          if(frameFinished)
          {
            double pts = startpts;
            double epts = packetpts+(1.0/m_fps);

            if(epts<atTime)
            {
              //keep decoding until we get to the desired seek frame
              startpts = packetpts;
              continue;
            }

            if(startTime) *startTime = pts;
            if(endTime) *endTime = epts;
            m_curtime = epts;

            //convert decoded image to correct format
            int w = m_w;
            int h = m_h;
            if(resizeToBuf) 
            {
              w = dst->getWidth();
              h = dst->getHeight();
            }
            else
            {
              dst->resize(w, h);
            }
            unsigned int *bits = dst->getBits();
/*#ifdef _WIN32
            uint8_t *dstd[4]= {(uint8_t *)bits+(dst->getRowSpan()*4*(h-1)),};
            int dst_stride[4]={-dst->getRowSpan()*4,};
#else*/
            uint8_t *dstd[4]= {(uint8_t *)bits,};
            int dst_stride[4]={dst->getRowSpan()*4,};
//#endif

            if (!m_sws || m_sws_desth != h || m_sws_destw != w)
            {
              int sws_flags = SWS_BICUBIC;
              PixelFormat pfout = 
          #ifdef _WIN32
                PIX_FMT_RGB32;
          #else
              PIX_FMT_BGR32_1;
          #endif
              if(m_sws) sws_freeContext(m_sws);
              m_sws = sws_getContext(m_w, m_h, m_pixfmt, w, h, pfout, sws_flags, NULL, NULL, NULL);
              m_sws_desth = h;
              m_sws_destw = w;
            }

            if (m_sws)
              sws_scale(m_sws, m_frame->data, m_frame->linesize, 0, m_h, dstd, dst_stride);
              
            av_free_packet(&packet);
            return 1;
          }
        }
        
        av_free_packet(&packet);
      }
    }

    return 0;
  }

protected:

  double getPresentationTime(AVPacket *packet)
  {
    double mpts = 0;
    if(packet->dts != AV_NOPTS_VALUE) mpts = (double)packet->dts;
    mpts *= av_q2d(m_ic->streams[packet->stream_index]->time_base);
    mpts -= (double)m_ic->start_time/AV_TIME_BASE;
    return mpts;
  }

  int m_inited;
  
  AVFormatContext *m_ic;
  AVCodecContext *m_ctx;

  AVFrame *m_frame;
  
  int m_stream;
  
  int m_w, m_h, m_format;
  double m_fps, m_len;
  struct SwsContext *m_sws;
  int m_sws_desth, m_sws_destw;
  PixelFormat m_pixfmt;

  double m_curtime;
};

#endif
