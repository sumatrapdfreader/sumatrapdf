#ifndef OPJ_INTMATH_H
#define OPJ_INTMATH_H

static INLINE OPJ_INT32 opj_int_min(OPJ_INT32 a, OPJ_INT32 b)
{
    return a < b ? a : b;
}

static INLINE OPJ_UINT32 opj_uint_min(OPJ_UINT32 a, OPJ_UINT32 b)
{
    return a < b ? a : b;
}

static INLINE OPJ_INT32 opj_int_max(OPJ_INT32 a, OPJ_INT32 b)
{
    return (a > b) ? a : b;
}

static INLINE OPJ_UINT32 opj_uint_max(OPJ_UINT32  a, OPJ_UINT32  b)
{
    return (a > b) ? a : b;
}

static INLINE OPJ_UINT32 opj_uint_adds(OPJ_UINT32 a, OPJ_UINT32 b)
{
    OPJ_UINT64 sum = (OPJ_UINT64)a + (OPJ_UINT64)b;
    return (OPJ_UINT32)(-(OPJ_INT32)(sum >> 32)) | (OPJ_UINT32)sum;
}

static INLINE OPJ_UINT32 opj_uint_subs(OPJ_UINT32 a, OPJ_UINT32 b)
{
    return (a >= b) ? a - b : 0;
}

static INLINE OPJ_INT32 opj_int_clamp(OPJ_INT32 a, OPJ_INT32 min,
                                      OPJ_INT32 max)
{
    if (a < min) {
        return min;
    }
    if (a > max) {
        return max;
    }
    return a;
}

static INLINE OPJ_INT64 opj_int64_clamp(OPJ_INT64 a, OPJ_INT64 min,
                                        OPJ_INT64 max)
{
    if (a < min) {
        return min;
    }
    if (a > max) {
        return max;
    }
    return a;
}

static INLINE OPJ_INT32 opj_int_abs(OPJ_INT32 a)
{
    return a < 0 ? -a : a;
}

static INLINE OPJ_INT32 opj_int_ceildiv(OPJ_INT32 a, OPJ_INT32 b)
{
    assert(b);
    return (OPJ_INT32)(((OPJ_INT64)a + b - 1) / b);
}

static INLINE OPJ_UINT32  opj_uint_ceildiv(OPJ_UINT32  a, OPJ_UINT32  b)
{
    assert(b);
    return (OPJ_UINT32)(((OPJ_UINT64)a + b - 1) / b);
}

static INLINE OPJ_UINT32  opj_uint64_ceildiv_res_uint32(OPJ_UINT64 a,
        OPJ_UINT64 b)
{
    assert(b);
    return (OPJ_UINT32)((a + b - 1) / b);
}

static INLINE OPJ_INT32 opj_int_ceildivpow2(OPJ_INT32 a, OPJ_INT32 b)
{
    return (OPJ_INT32)((a + ((OPJ_INT64)1 << b) - 1) >> b);
}

static INLINE OPJ_INT32 opj_int64_ceildivpow2(OPJ_INT64 a, OPJ_INT32 b)
{
    return (OPJ_INT32)((a + ((OPJ_INT64)1 << b) - 1) >> b);
}

static INLINE OPJ_UINT32 opj_uint_ceildivpow2(OPJ_UINT32 a, OPJ_UINT32 b)
{
    return (OPJ_UINT32)((a + ((OPJ_UINT64)1U << b) - 1U) >> b);
}

static INLINE OPJ_INT32 opj_int_floordivpow2(OPJ_INT32 a, OPJ_INT32 b)
{
    return a >> b;
}

static INLINE OPJ_UINT32 opj_uint_floordivpow2(OPJ_UINT32 a, OPJ_UINT32 b)
{
    return a >> b;
}

static INLINE OPJ_INT32 opj_int_floorlog2(OPJ_INT32 a)
{
    OPJ_INT32 l;
    for (l = 0; a > 1; l++) {
        a >>= 1;
    }
    return l;
}

static INLINE OPJ_UINT32  opj_uint_floorlog2(OPJ_UINT32  a)
{
    OPJ_UINT32  l;
    for (l = 0; a > 1; ++l) {
        a >>= 1;
    }
    return l;
}

static INLINE OPJ_INT32 opj_int_fix_mul(OPJ_INT32 a, OPJ_INT32 b)
{
#if defined(_MSC_VER) && (_MSC_VER >= 1400) && !defined(__INTEL_COMPILER) && defined(_M_IX86)
    OPJ_INT64 temp = __emul(a, b);
#else
    OPJ_INT64 temp = (OPJ_INT64) a * (OPJ_INT64) b ;
#endif
    temp += 4096;
    assert((temp >> 13) <= (OPJ_INT64)0x7FFFFFFF);
    assert((temp >> 13) >= (-(OPJ_INT64)0x7FFFFFFF - (OPJ_INT64)1));
    return (OPJ_INT32)(temp >> 13);
}

static INLINE OPJ_INT32 opj_int_fix_mul_t1(OPJ_INT32 a, OPJ_INT32 b)
{
#if defined(_MSC_VER) && (_MSC_VER >= 1400) && !defined(__INTEL_COMPILER) && defined(_M_IX86)
    OPJ_INT64 temp = __emul(a, b);
#else
    OPJ_INT64 temp = (OPJ_INT64) a * (OPJ_INT64) b ;
#endif
    temp += 4096;
    assert((temp >> (13 + 11 - T1_NMSEDEC_FRACBITS)) <= (OPJ_INT64)0x7FFFFFFF);
    assert((temp >> (13 + 11 - T1_NMSEDEC_FRACBITS)) >= (-(OPJ_INT64)0x7FFFFFFF -
            (OPJ_INT64)1));
    return (OPJ_INT32)(temp >> (13 + 11 - T1_NMSEDEC_FRACBITS)) ;
}

static INLINE OPJ_INT32 opj_int_add_no_overflow(OPJ_INT32 a, OPJ_INT32 b)
{
    void* pa = &a;
    void* pb = &b;
    OPJ_UINT32* upa = (OPJ_UINT32*)pa;
    OPJ_UINT32* upb = (OPJ_UINT32*)pb;
    OPJ_UINT32 ures = *upa + *upb;
    void* pures = &ures;
    OPJ_INT32* ipres = (OPJ_INT32*)pures;
    return *ipres;
}

static INLINE OPJ_INT32 opj_int_sub_no_overflow(OPJ_INT32 a, OPJ_INT32 b)
{
    void* pa = &a;
    void* pb = &b;
    OPJ_UINT32* upa = (OPJ_UINT32*)pa;
    OPJ_UINT32* upb = (OPJ_UINT32*)pb;
    OPJ_UINT32 ures = *upa - *upb;
    void* pures = &ures;
    OPJ_INT32* ipres = (OPJ_INT32*)pures;
    return *ipres;
}

#endif
