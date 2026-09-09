//
// Little cms

// Chameleonic header file to instantiate different versions of the
// transform routines.
//
// As a bare minimum the following must be defined on entry:
//   FUNCTION_NAME             the name of the function
//
// In addition, a range of other symbols can be optionally defined on entry
// to make the generated code more efficient. All these symbols (and
// FUNCTION_NAME) will be automatically undefined at the end of the file so
// that repeated #includes of this file are made simple.
//
// If caching is wanted, define CACHED.
//
// If the representation/calculations are to be done using floating point
// define XFORM_FLOAT. In the absence of this it is assumed that the
// calculations will be done in 16 bit with appropriate unpacking/repacking.
//
// If you know the number of input/output channels, define NUMINCHANNELS and
// NUMOUTCHANNELS.
//
// If you know the number of bytes used for the packed version of input and/or
// output, define INPACKEDSAMPLESIZE and OUTPACKEDSAMPLESIZE.
//
// If you do not know the number of channels and/or the sample size, but you
// do know a maximum bound on the number of bytes used to represent the
// unpacked samples, then operation with CACHE can be accelerated by defining
// CMPBYTES to the number of bytes that should be compared to the cached result.
// Usually that is calculated from NUMINCHANNELS and INPACKEDSAMPLESIZE, so
// specifying it directly is only useful if either (or both) of those is not
// known in advance.
//
// For Premultiplied Alpha modes, you must define PREMULT. We only support
// premultiplied alpha where the alpha is the last 'extra' channel, and
// where both source and destination are packed in the same way.
//
// If you know the code to be used to unpack (or pack, or both) data to/from
// the simple 16 bit transform input/output format, then you can choose
// to this directly by defining UNPACK/PACK macros as follows:
//   UNPACK(T,TO,FROM,SIZE,AL) (Opt)   code to unpack input data (T = Transform
//                                     TO = buffer to unpack into, FROM = data,
//                                     SIZE = size of data, AL = Alpha)
//   PACK(T,FROM,TO,SIZE,AL)   (Opt)   code to pack transformed input data
//                                    (T = Transform, FROM = transformed data,
//                                    TO = output buffer to pack into,
//                                    SIZE = size of data, AL = Alpha)
//
// Ignore AL unless PREMULT is defined, in which case it will be in the packed
// format. AL is guaranteed to be non-zero.
//
// If UNPACKINCLUDESPREALPHA is defined, then UNPACK should undo the
// premultiplication by AL (i.e. divide by AL). Otherwise AL should be ignored
// and this routine will do it for you.
//
// If PACKINCLUDESPREALPHA is defined, then PACK should apply AL (i.e. multiply
// by AL). Otherwise AL should be ignored and this routine will do it for you.
//
// As an alternative to the above, if you know the function name that would
// be called, supply that in UNPACKFN and PACKFN and inlining compilers
// should hopefully do the hard work for you.
//   UNPACKFN          (Opt)   function to unpack input data
//   PACKFN            (Opt)   function to pack input data
//
// If the data happens to be in the correct input format anyway, we can skip
// unpacking it entirely and just use it direct.
//   NO_UNPACK         (Opt)   if defined, transform direct from the input
//                             data.
//
// UNPACK/PACK/UNPACKFN/PACKFN/NO_UNPACK are all expected to update their
// TO pointer to point to the next pixels data. This means for cases where
// we have extra bytes, they should skip the extra bytes too!
//
// If the data happens to be in the correct output format anyway, we can skip
// packing it entirely and just transform it direct into the buffer.
//   NO_PACK           (Opt)   if defined, transform direct to the output
//                             data.
//   COPY_MATCHED(FROM,TO)(Opt)if defined, copy output values from FROM to
//                             TO. Used in the case CACHED case where the
//                             cache matches and we have to copy forwards.
//
// GAMUTCHECK can be predefined if a gamut check needs to be done.
//
// If there are a known number of extra bytes to be dealt with, define EXTRABYTES
// to that number (such as 0 for none).
// If you want to provide your own code for copying from input to output, define
// COPY_EXTRAS(TRANS,FROM,TO) to do so.
// If none of these are defined, we call cmsHandleExtraChannels.

#ifndef CMPBYTES
#ifdef NUMINCHANNELS
#ifdef XFORM_FLOAT
#define CMPBYTES (NUMINCHANNELS*4)
#else
#define CMPBYTES (NUMINCHANNELS*2)
#endif
#endif
#endif

#ifdef CMPBYTES
 // Previously, we've attempted to do 'int' based checks here, but this falls
 // foul of some compilers with their strict pointer aliasing. We have the
 // choice of calling memcmp (which tests using chars, so is safe), or of
 // testing using the actual type.
 #ifdef XFORM_FLOAT
  #if CMPBYTES == 4
   #define COMPARE(A,B) ((A)[0] != (B)[0])
  #elif CMPBYTES == 8
   #define COMPARE(A,B) (((A)[0] != (B)[0]) || ((A)[1] != (B)[1]))
  #elif CMPBYTES == 12
   #define COMPARE(A,B) (((A)[0] != (B)[0]) || ((A)[1] != (B)[1]) || ((A)[2] != (B)[2]))
  #elif CMPBYTES == 16
   #define COMPARE(A,B) (((A)[0] != (B)[0]) || ((A)[1] != (B)[1]) || ((A)[2] != (B)[2]) || ((A)[3] != (B)[3]))
  #endif
 #else
  #if CMPBYTES == 2
   #define COMPARE(A,B) ((A)[0] != (B)[0])
  #elif CMPBYTES == 4
   #define COMPARE(A,B) (((A)[0] != (B)[0]) || ((A)[1] != (B)[1]))
  #elif CMPBYTES == 6
   #define COMPARE(A,B) (((A)[0] != (B)[0]) || ((A)[1] != (B)[1]) || ((A)[2] != (B)[2]))
  #elif CMPBYTES == 8
   #define COMPARE(A,B) (((A)[0] != (B)[0]) || ((A)[1] != (B)[1]) || ((A)[2] != (B)[2]) || ((A)[3] != (B)[3]))
  #endif
 #endif
#else
 // Otherwise, set INBYTES to be the maximum size it could possibly be.
 #ifdef XFORM_FLOAT
  #define CMPBYTES (sizeof(cmsFloat32Number)*cmsMAXCHANNELS)
 #else
  #define CMPBYTES (sizeof(cmsUInt16Number)*cmsMAXCHANNELS)
 #endif
#endif

#ifndef COMPARE
 #define COMPARE(A,B) memcmp((A),(B), CMPBYTES)
#endif

#if   defined(UNPACK)
 // Nothing to do, UNPACK is already defined
#elif defined(NO_UNPACK)
 #define UNPACK(CTX,T,TO,FROM,STRIDE,AL) do { } while (0)
#elif defined(UNPACKFN)
 #define UNPACK(CTX,T,TO,FROM,STRIDE,AL) \
    do { (FROM) = UNPACKFN((CTX),(T),(TO),(FROM),(STRIDE),(AL)); } while (0)
#elif defined(XFORM_FLOAT)
 #define UNPACK(CTX,T,TO,FROM,STRIDE,AL) \
    do { (FROM) = (T)->FromInputFloat((CTX),(T),(TO),(FROM),(STRIDE)); } while (0)
#else
 #define UNPACK(CTX,T,TO,FROM,STRIDE,AL) \
    do { (FROM) = (T)->FromInput((CTX),(T),(TO),(FROM),(STRIDE)); } while (0)
#endif

#if defined(PACK)
 // Nothing to do, PACK is already defined
#elif defined(NO_PACK)
 #define PACK(CTX,T,FROM,TO,STRIDE,AL) \
     do { (FROM) += (totaloutbytes/sizeof(XFORM_TYPE)); } while (0)
#elif defined(PACKFN)
 #define PACK(CTX,T,FROM,TO,STRIDE,AL) \
     do { (TO) = PACKFN((CTX),(T),(FROM),(TO),(STRIDE)); } while (0)
#elif defined(XFORM_FLOAT)
 #define PACK(CTX,T,FROM,TO,STRIDE,AL) \
     do { (TO) = (T)->ToOutputFloat((CTX),(T),(FROM),(TO),(STRIDE)); } while (0)
#else
 #define PACK(CTX,T,FROM,TO,STRIDE,AL) \
     do { (TO) = (T)->ToOutput((CTX),(T),(FROM),(TO),(STRIDE)); } while (0)
#endif

#ifndef ZEROPACK
/* The 'default' definition of ZEROPACK only works when
 * inpackedsamplesize == outpackedsamplesize. */
#define ZEROPACK(CTX,T,TO,FROM) do { \
    memset((TO),0,numoutchannels*outpackedsamplesize);\
    if (numextras != 0) memcpy((TO)+numoutchannels*outpackedsamplesize,\
                               (FROM)+numinchannels*inpackedsamplesize,\
                               numextras*outpackedsamplesize);\
    (TO)+=(1+prealphaindexout)*outpackedsamplesize; } while (0)
#endif

#ifndef UNPRE
#ifdef PREALPHA
#else
#define UNPRE(CTX,T,S,A) do {} while (0)
#endif
#endif

#ifndef REPRE
#ifdef PREALPHA
#define REPRE(CTX,T,S,A) do { int i; for (i = 0; i < numoutchannels; i++) \
                                          (S)[i] = mul65535((S)[i],A); } while (0)
#else
#define REPRE(CTX,T,S,A) do {} while (0)
#endif
#endif

#ifndef XFORMVARS
#define XFORMVARS(p) do { } while (0)
#endif

#if defined(NUMOUTCHANNELS)
 #ifdef XFORM_FLOAT
  #define OUTBYTES (sizeof(cmsFloat32Number)*NUMOUTCHANNELS)
 #else
  #define OUTBYTES (sizeof(cmsUInt16Number)*NUMOUTCHANNELS)
 #endif
#endif

#if defined(NO_PACK) && !defined(COPY_MATCHED) && defined(OUTBYTES)
 #if (defined(XFORM_FLOAT) && OUTBYTES == 4) || OUTBYTES == 2
  #define COPY_MATCHED(FROM,TO) ((TO)[0] = (FROM)[0])
 #elif (defined(XFORM_FLOAT) && OUTBYTES == 8) || OUTBYTES == 4
  #define COPY_MATCHED(FROM,TO) ((TO)[0] = (FROM)[0],(TO)[1] = (FROM)[1])
 #elif (defined(XFORM_FLOAT) && OUTBYTES == 12) || OUTBYTES == 6
  #define COPY_MATCHED(FROM,TO) ((TO)[0] = (FROM)[0],(TO)[1] = (FROM)[1],(TO)[2] = (FROM)[2])
 #elif (defined(XFORM_FLOAT) && OUTBYTES == 16) || OUTBYTES == 8
  #define COPY_MATCHED(FROM,TO) ((TO)[0] = (FROM)[0],(TO)[1] = (FROM)[1],(TO)[2] = (FROM)[2],(TO)[3] = (FROM)[3])
 #else
  #define COPY_MATCHED(FROM,TO) memcpy((TO),(FROM),(OUTBYTES))
 #endif
#endif

#ifdef XFORM_FLOAT
 #define XFORM_TYPE cmsFloat32Number
#else
 #define XFORM_TYPE cmsUInt16Number
#endif

#ifndef COPY_EXTRAS
 #ifdef NUMEXTRAS
  #if NUMEXTRAS == 0
   #define COPY_EXTRAS(TRANS,FROM,TO) do { } while (0)
  #else
   #define COPY_EXTRAS(TRANS,FROM,TO) \
       do { memcpy((TO),(FROM),(NUMEXTRAS)*inpackedsamplesize); \
            (TO) += (NUMEXTRAS)*inpackedsamplesize; \
            (FROM) += (NUMEXTRAS)*inpackedsamplesize; \
       } while (0)
  #endif
 #else
  #define BULK_COPY_EXTRAS
  #define COPY_EXTRAS(TRANS,FROM,TO) do { } while (0)
 #endif
#endif

static
void FUNCTION_NAME(cmsContext ContextID,
		   _cmsTRANSFORM* p,
                   const void* in,
                   void* out,
                   cmsUInt32Number PixelsPerLine,
                   cmsUInt32Number LineCount,
                   const cmsStride* Stride)
{
    _cmsTRANSFORMCORE *core = p->core;
#ifndef NO_UNPACK
 #ifdef XFORM_FLOAT
    cmsFloat32Number wIn[cmsMAXCHANNELS*2];
 #else
    cmsUInt16Number wIn[cmsMAXCHANNELS*2];
 #endif
 #define wIn0 (&wIn[0])
 #define wIn1 (&wIn[cmsMAXCHANNELS])
#endif
    XFORM_TYPE *currIn;
#ifdef CACHED
    XFORM_TYPE *prevIn;
#endif /* CACHED */
#ifdef NO_PACK
    XFORM_TYPE *wOut = (XFORM_TYPE *)out;
    XFORM_TYPE *prevOut = (XFORM_TYPE *)p->Cache.CacheOut;
#else
    XFORM_TYPE wOut[cmsMAXCHANNELS];
#endif
#if defined(PREALPHA) && !defined(PACKINCLUDESPREALPHA)
    XFORM_TYPE wScaled[cmsMAXCHANNELS];
#endif
#ifdef GAMUTCHECK
    _cmsPipelineEval16Fn evalGamut = core->GamutCheck->Eval16Fn;
#endif /* GAMUTCHECK */
#ifdef XFORM_FLOAT
    _cmsPipelineEvalFloatFn eval = core->Lut->EvalFloatFn;
    const cmsPipeline *data = core->Lut;
#else
    _cmsPipelineEval16Fn eval = core->Lut->Eval16Fn;
    void *data = core->Lut->Data;
#endif
    cmsUInt32Number bppi = Stride->BytesPerPlaneIn;
    cmsUInt32Number bppo = Stride->BytesPerPlaneOut;
#ifdef NUMINCHANNELS
    int numinchannels = NUMINCHANNELS;
#else
    int numinchannels = T_CHANNELS(p->InputFormat);
#endif
#ifdef NUMOUTCHANNELS
    int numoutchannels = NUMOUTCHANNELS;
#else
    int numoutchannels = T_CHANNELS(p->OutputFormat);
#endif
#ifdef NUMEXTRAS
    int numextras = NUMEXTRAS;
#else
    int numextras = T_EXTRA(p->InputFormat);
#endif
#ifdef INPACKEDSAMPLESIZE
    int inpackedsamplesize = INPACKEDSAMPLESIZE;
#else
    int inpackedsamplesize = T_BYTES(p->InputFormat);
#endif
#ifdef OUTPACKEDSAMPLESIZE
    int outpackedsamplesize = OUTPACKEDSAMPLESIZE;
#else
    int outpackedsamplesize = T_BYTES(p->OutputFormat);
#endif
    int prealphaindexin = numinchannels + numextras - 1;
    int prealphaindexout = numoutchannels + numextras - 1;
    int totalinbytes = (numinchannels + numextras)*inpackedsamplesize;
    int totaloutbytes = (numoutchannels + numextras)*outpackedsamplesize;

    /* Silence some warnings */
    (void)bppi;
    (void)bppo;
    (void)prealphaindexin;
    (void)numextras;
    (void)prealphaindexout;
    (void)inpackedsamplesize;
    (void)outpackedsamplesize;
    (void)totalinbytes;
    (void)totaloutbytes;

#ifdef BULK_COPY_EXTRAS
    if (core->dwOriginalFlags & cmsFLAGS_COPY_ALPHA)
        _cmsHandleExtraChannels(ContextID, p, in, out, PixelsPerLine, LineCount, Stride);
#endif

    if (PixelsPerLine == 0)
        return;

#ifdef NO_UNPACK
    prevIn = (XFORM_TYPE *)p->Cache.CacheIn;
#else
 #ifdef CACHED
    // Empty buffers for quick memcmp
    memset(wIn1, 0, sizeof(XFORM_TYPE) * cmsMAXCHANNELS);

    // Get copy of zero cache
    memcpy(wIn0, p->Cache.CacheIn,  sizeof(XFORM_TYPE) * cmsMAXCHANNELS);
    memcpy(wOut, p->Cache.CacheOut, sizeof(XFORM_TYPE) * cmsMAXCHANNELS);

    // The caller guarantees us that the cache is always valid on entry; if
    // the representation is changed, the cache is reset.
    prevIn = wIn0;
 #endif /* CACHED */
    currIn = wIn1;
#endif

    while (LineCount-- > 0)
    {
        cmsUInt32Number n = PixelsPerLine;
        cmsUInt8Number* accum  = (cmsUInt8Number*) in;
        cmsUInt8Number* output = (cmsUInt8Number*) out;
#ifdef NO_UNPACK
        currIn = (XFORM_TYPE *)accum;
#endif
        while (n-- > 0) { // prevIn == CacheIn, wOut = CacheOut
#ifdef PREALPHA
 #ifdef XFORM_FLOAT
            cmsFloat32Number alpha = ((cmsFloat32Number *)accum)[prealphaindexin];
 #else
            cmsUInt32Number alpha = inpackedsamplesize == 2 ?
                                     ((cmsUInt16Number *)accum)[prealphaindexin] :
                                     (accum[prealphaindexin]);
 #endif
            if (alpha == 0) {
                ZEROPACK(ContextID,p,output,accum);
                accum += inpackedsamplesize*(prealphaindexin+1);
            } else {
#endif
                UNPACK(ContextID,p,currIn,accum,bppi,alpha);
#ifdef PREALPHA
 #ifndef UNPACKINCLUDESPREALPHA
  #ifdef XFORM_FLOAT
                {
                    int i;
                    cmsFloat32Number inva = 1.0f / alpha;
                    for (i = 0; i < numinchannels; i++)
                        currIn[i] *= inva;
                }
  #else
                {
                    int i;
                    cmsUInt32Number al = inpackedsamplesize == 1 ? alpha*0x101 : alpha;
                    cmsUInt32Number inva = 0xffff0000U / al;
                    for (i = 0; i < numinchannels; i++)
                        currIn[i] = ((currIn[i] * inva)>>16);
                }
  #endif
 #endif
#endif
#ifdef CACHED
                if (COMPARE(currIn, prevIn))
#endif /* CACHED */
                {
#ifdef GAMUTCHECK
 #ifdef XFORM_FLOAT
                    cmsFloat32Number OutOfGamut;

                    // Evaluate gamut marker.
                    cmsPipelineEvalFloat(currIn, &OutOfGamut, core->GamutCheck);

                    // Is current color out of gamut?
                    if (OutOfGamut > 0.0)
                        // Certainly, out of gamut
                        for (j=0; j < cmsMAXCHANNELS; j++)
                            fOut[j] = -1.0;
                    else
 #else
                    cmsUInt16Number wOutOfGamut;

                    evalGamut(ContextID, currIn, &wOutOfGamut, core->GamutCheck->Data);
                    if (wOutOfGamut >= 1)
                        /* RJW: Could be faster? copy once to a local buffer? */
                        cmsGetAlarmCodes(ContextID, wOut);
                    else
 #endif /* FLOAT_XFORM */
#endif /* GAMUTCHECK */
                        eval(ContextID, currIn, wOut, data);
#ifdef NO_UNPACK
 #ifdef CACHED
                    prevIn = currIn;
 #endif
                    currIn = (XFORM_TYPE *)(((char *)currIn) + totalinbytes);
#else
 #ifdef CACHED
                    {XFORM_TYPE *tmp = currIn; currIn = prevIn; prevIn = tmp;} // SWAP
 #endif /* CACHED */
#endif /* NO_UNPACK */
                }
#ifdef NO_PACK
                else
                    COPY_MATCHED(prevOut,wOut);
                prevOut = wOut;
#endif
#ifdef PREALPHA
 #ifndef PACKINCLUDESPREALPHA
  #ifdef XFORM_FLOAT
                {
                    int i;
                    for (i = 0; i < numoutchannels; i++)
                        wScaled = wOut[i] * alpha;
                }
  #else
                {
                    int i;
                    cmsUInt32Number al = inpackedsamplesize == 1 ? alpha*0x101 : alpha;
                    for (i = 0; i < numoutchannels; i++)
                        wScaled[i] = mul65535(wOut[i],al);
                }
  #endif
                PACK(ContextID,p,wScaled,output,bppo,alpha);
 #else
                PACK(ContextID,p,wOut,output,bppo,alpha);
 #endif
#else
                PACK(ContextID,p,wOut,output,bppo,alpha);
#endif
                COPY_EXTRAS(p,accum,output);
#ifdef PREALPHA
            }
#endif
        } /* End x loop */
        in = (void *)((cmsUInt8Number *)in + Stride->BytesPerLineIn);
        out = (void *)((cmsUInt8Number *)out + Stride->BytesPerLineOut);
    } /* End y loop */
    /* The following code is only safe if we know that a given transform is
     * called on one thread a time. */
#if 0
#ifdef CACHED
#ifdef NO_UNPACK
    memcpy(p->Cache.CacheIn,prevIn, CMPBYTES);
#else
    memcpy(p->Cache.CacheIn, prevIn, sizeof(XFORM_TYPE) * cmsMAXCHANNELS);
#endif
#ifdef NO_PACK
    COPY_MATCHED(prevOut,p->Cache.CacheOut);
#else
    memcpy(p->Cache.CacheOut, wOut, sizeof(XFORM_TYPE) * cmsMAXCHANNELS);
#endif /* NO_PACK */
#endif /* CACHED */
#endif
}

#undef wIn0
#undef wIn1
#undef XFORM_TYPE
#undef XFORM_FLOAT

#undef FUNCTION_NAME
#undef COMPARE
#undef CMPBYTES
#undef OUTBYTES
#undef UNPACK
#undef NO_UNPACK
#undef PACK
#undef NO_PACK
#undef UNPACKFN
#undef PACKFN
#undef GAMUTCHECK
#undef CACHED
#undef COPY_MATCHED
#undef EXTRABYTES
#undef COPY_EXTRAS
#undef BULK_COPY_EXTRAS
#undef PREALPHA
#undef ZEROPACK
#undef XFORMVARS
#undef UNPRE
#undef REPRE
#undef INPACKEDSAMPLESIZE
#undef OUTPACKEDSAMPLESIZE
#undef NUMINCHANNELS
#undef NUMOUTCHANNELS
#undef NUMEXTRAS
