/*
  LICENSE
  -------
Copyright 2005 Nullsoft, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer. 

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution. 

  * Neither the name of Nullsoft nor the names of its contributors may be used to 
    endorse or promote products derived from this software without specific prior written permission. 
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR 
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND 
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR 
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT 
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/
/*
** nsvlib.h - NSV file/bitstream reading/writing interface
** 
*/

#ifndef _NSVLIB_H_
#define _NSVLIB_H_

/*********************************************************************
** bitstream classes 
*/

#include "nsvbs.h"


/*********************************************************************
** NSV packeting limits
*/
#define NSV_MAX_AUDIO_LEN 0x8000  // 32kb
#define NSV_MAX_VIDEO_LEN 0x80000 // 512kb
#define NSV_MAX_AUX_LEN   0x8000  // 32kb for each aux stream
#define NSV_MAX_AUXSTREAMS 15     // 15 aux streams maximum


/*********************************************************************
** Constants for setting certain metadata items using addHdrMetaData()
*/
#define METADATANAME_AUTHOR			"Author"
#define METADATANAME_TITLE			"Title"
#define METADATANAME_COPYRIGHT		"Copyright"
#define METADATANAME_COMMENT		"Comment"
#define METADATANAME_PROFILE		"Profile"
#define METADATANAME_FILEID			"File ID"

/*********************************************************************
** NSV type utility functions/macros
*/

/*
** Use NSV_MAKETYPE() to quickly make NSV audio/video/aux types.
** ex: NSV_MAKETYPE('R','G','B','A')
*/
#define NSV_MAKETYPE(A,B,C,D) ((A) | ((B)<<8) | ((C)<<16) | ((D)<<24))

/*
** These functions convert types to and from strings.
*/

/* nsv_type_to_string() converts an NSV type to a string.
 * out must be at least 5 bytes long. If 't' is not a valid type, 
 * then out will be set to an empty string
 * ex: 
 *   char out[5]; 
 *   nsv_type_to_string(NSV_MAKETYPE('R','G','B','A'),out); 
 *   strcmp(out,"RGBA") == 0
 */
void nsv_type_to_string(unsigned int t, char *out);

/* nsv_string_to_type() converts a string to an NSV type. 
 * Returns 0 if the type is not valid.
 * ex: nsv_string_to_type("RGBA") == NSV_MAKETYPE('R','G','B','A')
 */
unsigned int nsv_string_to_type(char *in);


/*********************************************************************
** NSV bitstream packeting/unpacketing classes
*/


/* nsv_Packeter is used to packet audio/video/auxiliary data into
 * a bitstream.
 *
 * ex:
 *   nsv_Packeter p;
 *   nsv_OutBS bs;
 *   p.setVidFmt(NSV_MAKETYPE('R','G','B','A'),320,240,30.0);
 *   p.setAudFmt(NSV_MAKETYPE('P','C','M',' '));
 *   for (;;) {
 *     doEncodeAudioAndVideo();
 *     p.setSyncFrame(is_keyframe);
 *     p.setSyncOffset(av_sync_offset);
 *     p.setAudio(audio_data,audio_len);
 *     p.setVideo(video_data,video_len);
 *     p.clearAuxChannels(); // you can add aux channels if you want
 *     if (p.packet(bs)) error();
 *     int outbuflen;
 *     void *outbuf=bs.get(&outbuflen);
 *     fwrite(outbuf,outbuflen,1,fp); // write output
 *     bs.clear(); // clear bitstream
 *   }
 *
 */

class nsv_Packeter {
  public:
    nsv_Packeter();
    ~nsv_Packeter();

    // init (per file) calls
    void setVidFmt(unsigned int vfmt, unsigned int w, unsigned int h, double frt);
    void setAudFmt(unsigned int afmt) { audfmt=afmt; }

    // per frame calls
    void setSyncFrame(int is_syncframe) { is_sync_frame=is_syncframe; }
    void setSyncOffset(int syncoffs) { syncoffset_cur=syncoffs; }
    void setAudio(void *a, int a_len) { audio=a; audio_len=a_len; }
    void setVideo(void *v, int v_len) { video=v; video_len=v_len; }
    int addAuxChannel(unsigned int fmt, void *data, int data_len) // 0 on success
    {
      if (aux_used >= NSV_MAX_AUXSTREAMS) return -1;
      aux[aux_used]=data;
      aux_len[aux_used]=data_len;
      aux_types[aux_used]=fmt;
      aux_used++;
      return 0;
    }
    void clearAuxChannels() { aux_used=0; }

    int packet(nsv_OutBS &bs); // returns 0 on success

    // some utility getting functions
    unsigned int getAudFmt() { return audfmt; }
    unsigned int getVidFmt() { return vidfmt; }
    unsigned int getWidth() { return width; }
    unsigned int getHeight() { return height; }
    double getFrameRate() { return framerate; }

  private:
    unsigned char framerate_idx;
    unsigned int vidfmt;
    unsigned int audfmt;
    unsigned int width;
    unsigned int height;
    double framerate;
    int syncoffset_cur;

    int aux_used;
    void *aux[NSV_MAX_AUXSTREAMS];
    int aux_len[NSV_MAX_AUXSTREAMS];
    unsigned int aux_types[NSV_MAX_AUXSTREAMS];
    int is_sync_frame;
    void *audio;
    int audio_len;
    void *video;
    int video_len;
};


/* nsv_Unpacketer is used to unpacket a bitstream into audio/video/auxiliary data
 * to decode, use an nsv_InBS object with data, and call unpacket().
 * ex:
 *  nsv_Unpacketer up;
 *  nsv_InBS in;
 *  nsv_InBS videoout, audioout;
 *  up.setVideoOut(&videoout);
 *  up.setAudioOut(&audioout);
 *  for (;;) {
 *    int ret=up.unpacket(in);
 *    if (ret < 0) break; // eof
 *    if (ret > 0) add_data_to_bitstream(&in,ret);
 *    if (!ret) { // got frame
 *      int vl=videoout.getbits(32);
 *      int al=videoout.getbits(32);
 *      char *vd=(char*)videoout.getcurbyteptr();
 *      char *ad=(char*)audioout.getcurbyteptr();
 *      doDecode(vd,vl,ad,al);
 *      videoout.seek(vl*8);
 *      audioout.seek(al*8);
 *      videoout.compact(); // free memory up
 *      audioout.compact(); // free memory up
 *      in.compact(); // free memory up
 *    }
 *  }
 */

class nsv_Unpacketer {
  public:
    nsv_Unpacketer() { reset(); }
    ~nsv_Unpacketer() { }

    void reset(int full=1); // if full, full reset is done. 
                            // if not, then it is a partial reset (ie for seeking)

    // when EOF is set, the unpacketer will fail instead of requesting more data at the
    // end; it will also not require that the next frame be available for sync
    // (normally it looks ahead to verify data)
    void setEof(int eof=1) { m_eof=eof; }
    int getEof() { return m_eof; }

    // use these to set where the unpacketer writes the output of each stream
    // set to NULL to ignore output of that stream
    
    void setAudioOut(nsv_InBS *output=NULL) { m_audiobs=output; }
    // the format of the audio data written to the output is:
    //   32 bits: length of frame
    //   ? bytes: audio data
    // (to read):
    //   int l=output->getbits(32);
    //   decode_audio(output->getcurbyteptr(),l);
    //   output->seek(l*8);


    void setVideoOut(nsv_InBS *output=NULL) { m_videobs=output; }
    // the format of the video data written to the output is:
    //   32 bits: length of frame
    //   ? bytes: video data
    // (to read):
    //   int l=output->getbits(32);
    //   decode_video(output->getcurbyteptr(),l);
    //   output->seek(l*8);

    void setAuxOut(nsv_InBS *output=NULL) { m_auxbs=output; }
    // the format of the aux data written to the output is:
    //   32 bits: length of frame
    //   32 bits: type of aux data
    //   ? bytes: aux data
    // (to read):
    //   int l=output->getbits(32);
    //   int type=output->getbits(32);
    //   decode_aux(output->getcurbyteptr(),l);
    //   output->seek(l*8);
    // aux is different than audio/video in that it includes a 32 bit
    // type value that is not included in the length.


    // returns 0 on success, >0 on needs (at least X bytes) more data, 
    // -1 on error (eof and no header found)
    int unpacket(nsv_InBS &bs);


    // do we have enough sync to determine formats/widths/heights/framerates
    int isValid() { return valid; }
    
    // are we fully synched? 
    int isSynched() { return synched; }

    // get sync offset from when we first synched up
    signed int getSyncOffset() { return (signed int) syncoffset; }

    // get sync offset from current frame (not usually used)
    signed int getCurSyncOffset() { return (signed int) syncoffset_cur; }

    // get video, audio, width, height, framerate formats.
    unsigned int getVidFmt() { return vidfmt; }
    unsigned int getAudFmt() { return audfmt; }
    unsigned int getWidth() { return width; }
    unsigned int getHeight() { return height; }
    double getFrameRate() { return framerate; }
    unsigned char getFrameRateIdx() { return framerate_idx; }

    // is current frame a sync frame?
    int isSynchFrame() { return is_sync_frame; }

  private:
    nsv_InBS *m_audiobs, *m_videobs, *m_auxbs;
    int valid; // contents of stream info are valid for syncing
    int synched; // turns off anal packet checking
    unsigned int vidfmt;
    unsigned int audfmt;
    unsigned int width;
    unsigned int height;
    double framerate;
    int is_sync_frame;
    unsigned char framerate_idx;
    int syncoffset;
    int syncoffset_cur;

    int m_eof;
};


/*********************************************************************
** NSV file header reading/writing functions
*/


typedef struct {
  // header_size is the size of NSV header. nsv_writeheader() and nsv_readheader()
  // will set this automatically
  unsigned int header_size; 

  // file_lenbytes is the size of the NSV bitstream (not including the header size)
  // this can be 0xFFFFFFFF to signify unknown length
  unsigned int file_lenbytes;

  // file_lenms is the length of the NSV bitstream in milliseconds.
  // this can be 0xFFFFFFFF to signify unknown length
  unsigned int file_lenms;

  // metadata_len describes the length of the metadata.
  unsigned int metadata_len;

  // toc_alloc describes the allocated length of the TOC (in entries).
  // set this to zero to use toc_size (recommended).
  unsigned int toc_alloc;

  // toc_size describes the used size of the TOC (in entries)
  // set this to zero to disable the TOC. When using toc_ex,
  // this must be < toc_alloc/2 (if using nsv_writeheader, and
  // toc_size is too big, toc_alloc will be grown automatically.
  unsigned int toc_size;

  // buffer which contains the TOC. this will be automatically 
  // allocated when using nsv_readheader(), but you should allocate
  // this yourself when using nsv_writeheader()
  unsigned int *toc;

  // if used, contains time pairs (in frames) for the offset. this will be 
  // automatically allocated when using nsv_readheader(), but you should allocate
  // this yourself when using nsv_writeheader()
  // DO NOT FREE THIS VALUE IF IT WAS ALLOCATED FROM NSV_READHEADER. :)
  // (it is just an extension of toc, which should be freed with free())
  unsigned int *toc_ex;

  // buffer which contains metadata. allocated when using nsv_readheader(), 
  // but you should allocate this yourself when using nsv_writeheader()
  // note that nsv_readheader() will NULL terminate this buffer.
  void *metadata;

} nsv_fileHeader;

// nsv_writeheader() writes the NSV file header to the bitstream bs.
// the NSV file header will be at LEAST padto bytes long (usually
// you will leave padto to 0)
void nsv_writeheader(nsv_OutBS &bs, nsv_fileHeader *hdr, unsigned int padto);

// nsv_readheader() reads an NSV file header from a bitstream bs.
// if the return value is less than zero, then there is no NSV
// file header in bs. if the return value is zero, the NSV file
// header was succesfully read. if the return value is positive,
// then at least that many more bytes are needed to decode the
// header.
// ex:
//   nsv_InBS bs;
//   nsv_fileHeader hdr;
//   for (;;) {
//      int ret=nsv_readheader(bs,&hdr);
//      if (ret<=0) break;
//      addBytesToBs(bs,ret);
//   }
//   if (hdr.header_size) { we_got_valid_header(&hdr); }
//

int nsv_readheader(nsv_InBS &bs, nsv_fileHeader *hdr);


// nsv_getmetadata() retrieves a metadata item from the metadata 
// block. if that item is not found, NULL is returned.
// Note that the value returned by nsv_getmetadata() has been
// malloc()'d, and you must free() it when you are done.
// ex:
//   char *v=nsv_getmetadata(hdr.metadata,"TITLE");
//   if (v) printf("title=%s\n",v);
//   free(v);
//

char *nsv_getmetadata(void *metadata, char *name);


#endif//_NSVLIB_H_
