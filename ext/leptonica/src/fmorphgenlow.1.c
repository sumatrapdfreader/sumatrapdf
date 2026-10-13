/*====================================================================*
 -  Copyright (C) 2001 Leptonica.  All rights reserved.
 -
 -  Redistribution and use in source and binary forms, with or without
 -  modification, are permitted provided that the following conditions
 -  are met:
 -  1. Redistributions of source code must retain the above copyright
 -     notice, this list of conditions and the following disclaimer.
 -  2. Redistributions in binary form must reproduce the above
 -     copyright notice, this list of conditions and the following
 -     disclaimer in the documentation and/or other materials
 -     provided with the distribution.
 -
 -  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 -  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 -  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 -  A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ANY
 -  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 -  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 -  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 -  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 -  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 -  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 -  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *====================================================================*/

/*!
 *     Low-level fast binary morphology with auto-generated sels
 *
 *      Dispatcher:
 *             l_int32    fmorphopgen_low_1()
 *
 *      Static Low-level:
 *             void       fdilate_1_*()
 *             void       ferode_1_*()
 */

#include "allheaders.h"

static void  fdilate_1_0(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_0(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_1(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_1(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_2(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_2(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_3(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_3(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_4(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_4(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_5(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_5(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_6(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_6(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_7(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_7(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_8(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_8(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_9(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_9(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_10(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_10(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_11(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_11(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_12(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_12(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_13(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_13(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_14(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_14(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_15(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_15(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_16(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_16(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_17(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_17(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_18(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_18(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_19(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_19(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_20(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_20(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_21(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_21(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_22(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_22(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_23(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_23(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_24(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_24(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_25(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_25(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_26(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_26(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_27(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_27(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_28(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_28(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_29(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_29(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_30(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_30(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_31(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_31(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_32(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_32(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_33(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_33(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_34(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_34(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_35(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_35(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_36(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_36(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_37(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_37(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_38(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_38(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_39(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_39(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_40(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_40(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_41(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_41(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_42(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_42(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_43(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_43(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_44(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_44(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_45(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_45(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_46(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_46(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_47(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_47(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_48(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_48(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_49(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_49(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_50(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_50(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_51(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_51(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_52(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_52(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_53(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_53(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_54(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_54(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_55(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_55(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_56(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_56(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fdilate_1_57(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  ferode_1_57(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);


/*---------------------------------------------------------------------*
 *                          Fast morph dispatcher                      *
 *---------------------------------------------------------------------*/
/*!
 *  fmorphopgen_low_1()
 *
 *       a dispatcher to appropriate low-level code
 */
l_int32
fmorphopgen_low_1(l_uint32  *datad,
                  l_int32    w,
                  l_int32    h,
                  l_int32    wpld,
                  l_uint32  *datas,
                  l_int32    wpls,
                  l_int32    index)
{

    switch (index)
    {
    case 0:
        fdilate_1_0(datad, w, h, wpld, datas, wpls);
        break;
    case 1:
        ferode_1_0(datad, w, h, wpld, datas, wpls);
        break;
    case 2:
        fdilate_1_1(datad, w, h, wpld, datas, wpls);
        break;
    case 3:
        ferode_1_1(datad, w, h, wpld, datas, wpls);
        break;
    case 4:
        fdilate_1_2(datad, w, h, wpld, datas, wpls);
        break;
    case 5:
        ferode_1_2(datad, w, h, wpld, datas, wpls);
        break;
    case 6:
        fdilate_1_3(datad, w, h, wpld, datas, wpls);
        break;
    case 7:
        ferode_1_3(datad, w, h, wpld, datas, wpls);
        break;
    case 8:
        fdilate_1_4(datad, w, h, wpld, datas, wpls);
        break;
    case 9:
        ferode_1_4(datad, w, h, wpld, datas, wpls);
        break;
    case 10:
        fdilate_1_5(datad, w, h, wpld, datas, wpls);
        break;
    case 11:
        ferode_1_5(datad, w, h, wpld, datas, wpls);
        break;
    case 12:
        fdilate_1_6(datad, w, h, wpld, datas, wpls);
        break;
    case 13:
        ferode_1_6(datad, w, h, wpld, datas, wpls);
        break;
    case 14:
        fdilate_1_7(datad, w, h, wpld, datas, wpls);
        break;
    case 15:
        ferode_1_7(datad, w, h, wpld, datas, wpls);
        break;
    case 16:
        fdilate_1_8(datad, w, h, wpld, datas, wpls);
        break;
    case 17:
        ferode_1_8(datad, w, h, wpld, datas, wpls);
        break;
    case 18:
        fdilate_1_9(datad, w, h, wpld, datas, wpls);
        break;
    case 19:
        ferode_1_9(datad, w, h, wpld, datas, wpls);
        break;
    case 20:
        fdilate_1_10(datad, w, h, wpld, datas, wpls);
        break;
    case 21:
        ferode_1_10(datad, w, h, wpld, datas, wpls);
        break;
    case 22:
        fdilate_1_11(datad, w, h, wpld, datas, wpls);
        break;
    case 23:
        ferode_1_11(datad, w, h, wpld, datas, wpls);
        break;
    case 24:
        fdilate_1_12(datad, w, h, wpld, datas, wpls);
        break;
    case 25:
        ferode_1_12(datad, w, h, wpld, datas, wpls);
        break;
    case 26:
        fdilate_1_13(datad, w, h, wpld, datas, wpls);
        break;
    case 27:
        ferode_1_13(datad, w, h, wpld, datas, wpls);
        break;
    case 28:
        fdilate_1_14(datad, w, h, wpld, datas, wpls);
        break;
    case 29:
        ferode_1_14(datad, w, h, wpld, datas, wpls);
        break;
    case 30:
        fdilate_1_15(datad, w, h, wpld, datas, wpls);
        break;
    case 31:
        ferode_1_15(datad, w, h, wpld, datas, wpls);
        break;
    case 32:
        fdilate_1_16(datad, w, h, wpld, datas, wpls);
        break;
    case 33:
        ferode_1_16(datad, w, h, wpld, datas, wpls);
        break;
    case 34:
        fdilate_1_17(datad, w, h, wpld, datas, wpls);
        break;
    case 35:
        ferode_1_17(datad, w, h, wpld, datas, wpls);
        break;
    case 36:
        fdilate_1_18(datad, w, h, wpld, datas, wpls);
        break;
    case 37:
        ferode_1_18(datad, w, h, wpld, datas, wpls);
        break;
    case 38:
        fdilate_1_19(datad, w, h, wpld, datas, wpls);
        break;
    case 39:
        ferode_1_19(datad, w, h, wpld, datas, wpls);
        break;
    case 40:
        fdilate_1_20(datad, w, h, wpld, datas, wpls);
        break;
    case 41:
        ferode_1_20(datad, w, h, wpld, datas, wpls);
        break;
    case 42:
        fdilate_1_21(datad, w, h, wpld, datas, wpls);
        break;
    case 43:
        ferode_1_21(datad, w, h, wpld, datas, wpls);
        break;
    case 44:
        fdilate_1_22(datad, w, h, wpld, datas, wpls);
        break;
    case 45:
        ferode_1_22(datad, w, h, wpld, datas, wpls);
        break;
    case 46:
        fdilate_1_23(datad, w, h, wpld, datas, wpls);
        break;
    case 47:
        ferode_1_23(datad, w, h, wpld, datas, wpls);
        break;
    case 48:
        fdilate_1_24(datad, w, h, wpld, datas, wpls);
        break;
    case 49:
        ferode_1_24(datad, w, h, wpld, datas, wpls);
        break;
    case 50:
        fdilate_1_25(datad, w, h, wpld, datas, wpls);
        break;
    case 51:
        ferode_1_25(datad, w, h, wpld, datas, wpls);
        break;
    case 52:
        fdilate_1_26(datad, w, h, wpld, datas, wpls);
        break;
    case 53:
        ferode_1_26(datad, w, h, wpld, datas, wpls);
        break;
    case 54:
        fdilate_1_27(datad, w, h, wpld, datas, wpls);
        break;
    case 55:
        ferode_1_27(datad, w, h, wpld, datas, wpls);
        break;
    case 56:
        fdilate_1_28(datad, w, h, wpld, datas, wpls);
        break;
    case 57:
        ferode_1_28(datad, w, h, wpld, datas, wpls);
        break;
    case 58:
        fdilate_1_29(datad, w, h, wpld, datas, wpls);
        break;
    case 59:
        ferode_1_29(datad, w, h, wpld, datas, wpls);
        break;
    case 60:
        fdilate_1_30(datad, w, h, wpld, datas, wpls);
        break;
    case 61:
        ferode_1_30(datad, w, h, wpld, datas, wpls);
        break;
    case 62:
        fdilate_1_31(datad, w, h, wpld, datas, wpls);
        break;
    case 63:
        ferode_1_31(datad, w, h, wpld, datas, wpls);
        break;
    case 64:
        fdilate_1_32(datad, w, h, wpld, datas, wpls);
        break;
    case 65:
        ferode_1_32(datad, w, h, wpld, datas, wpls);
        break;
    case 66:
        fdilate_1_33(datad, w, h, wpld, datas, wpls);
        break;
    case 67:
        ferode_1_33(datad, w, h, wpld, datas, wpls);
        break;
    case 68:
        fdilate_1_34(datad, w, h, wpld, datas, wpls);
        break;
    case 69:
        ferode_1_34(datad, w, h, wpld, datas, wpls);
        break;
    case 70:
        fdilate_1_35(datad, w, h, wpld, datas, wpls);
        break;
    case 71:
        ferode_1_35(datad, w, h, wpld, datas, wpls);
        break;
    case 72:
        fdilate_1_36(datad, w, h, wpld, datas, wpls);
        break;
    case 73:
        ferode_1_36(datad, w, h, wpld, datas, wpls);
        break;
    case 74:
        fdilate_1_37(datad, w, h, wpld, datas, wpls);
        break;
    case 75:
        ferode_1_37(datad, w, h, wpld, datas, wpls);
        break;
    case 76:
        fdilate_1_38(datad, w, h, wpld, datas, wpls);
        break;
    case 77:
        ferode_1_38(datad, w, h, wpld, datas, wpls);
        break;
    case 78:
        fdilate_1_39(datad, w, h, wpld, datas, wpls);
        break;
    case 79:
        ferode_1_39(datad, w, h, wpld, datas, wpls);
        break;
    case 80:
        fdilate_1_40(datad, w, h, wpld, datas, wpls);
        break;
    case 81:
        ferode_1_40(datad, w, h, wpld, datas, wpls);
        break;
    case 82:
        fdilate_1_41(datad, w, h, wpld, datas, wpls);
        break;
    case 83:
        ferode_1_41(datad, w, h, wpld, datas, wpls);
        break;
    case 84:
        fdilate_1_42(datad, w, h, wpld, datas, wpls);
        break;
    case 85:
        ferode_1_42(datad, w, h, wpld, datas, wpls);
        break;
    case 86:
        fdilate_1_43(datad, w, h, wpld, datas, wpls);
        break;
    case 87:
        ferode_1_43(datad, w, h, wpld, datas, wpls);
        break;
    case 88:
        fdilate_1_44(datad, w, h, wpld, datas, wpls);
        break;
    case 89:
        ferode_1_44(datad, w, h, wpld, datas, wpls);
        break;
    case 90:
        fdilate_1_45(datad, w, h, wpld, datas, wpls);
        break;
    case 91:
        ferode_1_45(datad, w, h, wpld, datas, wpls);
        break;
    case 92:
        fdilate_1_46(datad, w, h, wpld, datas, wpls);
        break;
    case 93:
        ferode_1_46(datad, w, h, wpld, datas, wpls);
        break;
    case 94:
        fdilate_1_47(datad, w, h, wpld, datas, wpls);
        break;
    case 95:
        ferode_1_47(datad, w, h, wpld, datas, wpls);
        break;
    case 96:
        fdilate_1_48(datad, w, h, wpld, datas, wpls);
        break;
    case 97:
        ferode_1_48(datad, w, h, wpld, datas, wpls);
        break;
    case 98:
        fdilate_1_49(datad, w, h, wpld, datas, wpls);
        break;
    case 99:
        ferode_1_49(datad, w, h, wpld, datas, wpls);
        break;
    case 100:
        fdilate_1_50(datad, w, h, wpld, datas, wpls);
        break;
    case 101:
        ferode_1_50(datad, w, h, wpld, datas, wpls);
        break;
    case 102:
        fdilate_1_51(datad, w, h, wpld, datas, wpls);
        break;
    case 103:
        ferode_1_51(datad, w, h, wpld, datas, wpls);
        break;
    case 104:
        fdilate_1_52(datad, w, h, wpld, datas, wpls);
        break;
    case 105:
        ferode_1_52(datad, w, h, wpld, datas, wpls);
        break;
    case 106:
        fdilate_1_53(datad, w, h, wpld, datas, wpls);
        break;
    case 107:
        ferode_1_53(datad, w, h, wpld, datas, wpls);
        break;
    case 108:
        fdilate_1_54(datad, w, h, wpld, datas, wpls);
        break;
    case 109:
        ferode_1_54(datad, w, h, wpld, datas, wpls);
        break;
    case 110:
        fdilate_1_55(datad, w, h, wpld, datas, wpls);
        break;
    case 111:
        ferode_1_55(datad, w, h, wpld, datas, wpls);
        break;
    case 112:
        fdilate_1_56(datad, w, h, wpld, datas, wpls);
        break;
    case 113:
        ferode_1_56(datad, w, h, wpld, datas, wpls);
        break;
    case 114:
        fdilate_1_57(datad, w, h, wpld, datas, wpls);
        break;
    case 115:
        ferode_1_57(datad, w, h, wpld, datas, wpls);
        break;
    }

    return 0;
}


/*--------------------------------------------------------------------------*
 *                 Low-level auto-generated static routines                 *
 *--------------------------------------------------------------------------*/
/*
 *  N.B.  In all the low-level routines, the part of the image
 *        that is accessed has been clipped by 32 pixels on
 *        all four sides.  This is done in the higher level
 *        code by redefining w and h smaller and by moving the
 *        start-of-image pointers up to the beginning of this
 *        interior rectangle.
 */
static void
fdilate_1_0(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr);
        }
    }
}

static void
ferode_1_0(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr);
        }
    }
}

static void
fdilate_1_1(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31));
        }
    }
}

static void
ferode_1_1(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31));
        }
    }
}

static void
fdilate_1_2(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31));
        }
    }
}

static void
ferode_1_2(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31));
        }
    }
}

static void
fdilate_1_3(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30));
        }
    }
}

static void
ferode_1_3(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30));
        }
    }
}

static void
fdilate_1_4(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 3) | (*(sptr + 1) >> 29)) |
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30));
        }
    }
}

static void
ferode_1_4(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 3) | (*(sptr - 1) << 29)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30));
        }
    }
}

static void
fdilate_1_5(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 3) | (*(sptr + 1) >> 29)) |
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) |
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29));
        }
    }
}

static void
ferode_1_5(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 3) | (*(sptr - 1) << 29)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) &
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29));
        }
    }
}

static void
fdilate_1_6(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 4) | (*(sptr + 1) >> 28)) |
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) |
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) |
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29));
        }
    }
}

static void
ferode_1_6(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 4) | (*(sptr - 1) << 28)) &
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) &
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29));
        }
    }
}

static void
fdilate_1_7(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 4) | (*(sptr + 1) >> 28)) |
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) |
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) |
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) |
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28));
        }
    }
}

static void
ferode_1_7(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 4) | (*(sptr - 1) << 28)) &
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) &
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) &
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28));
        }
    }
}

static void
fdilate_1_8(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 5) | (*(sptr + 1) >> 27)) |
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) |
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) |
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) |
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) |
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28));
        }
    }
}

static void
ferode_1_8(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 5) | (*(sptr - 1) << 27)) &
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) &
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) &
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) &
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28));
        }
    }
}

static void
fdilate_1_9(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 5) | (*(sptr + 1) >> 27)) |
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) |
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) |
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) |
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) |
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) |
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27));
        }
    }
}

static void
ferode_1_9(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 5) | (*(sptr - 1) << 27)) &
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) &
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) &
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) &
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) &
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27));
        }
    }
}

static void
fdilate_1_10(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 6) | (*(sptr + 1) >> 26)) |
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) |
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) |
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) |
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) |
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) |
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) |
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27));
        }
    }
}

static void
ferode_1_10(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 6) | (*(sptr - 1) << 26)) &
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) &
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) &
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) &
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) &
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) &
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27));
        }
    }
}

static void
fdilate_1_11(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 6) | (*(sptr + 1) >> 26)) |
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) |
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) |
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) |
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) |
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) |
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) |
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) |
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26));
        }
    }
}

static void
ferode_1_11(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 6) | (*(sptr - 1) << 26)) &
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) &
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) &
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) &
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) &
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) &
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) &
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26));
        }
    }
}

static void
fdilate_1_12(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 7) | (*(sptr + 1) >> 25)) |
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) |
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) |
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) |
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) |
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) |
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) |
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) |
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) |
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26));
        }
    }
}

static void
ferode_1_12(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 7) | (*(sptr - 1) << 25)) &
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) &
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) &
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) &
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) &
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) &
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) &
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) &
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26));
        }
    }
}

static void
fdilate_1_13(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 7) | (*(sptr + 1) >> 25)) |
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) |
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) |
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) |
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) |
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) |
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) |
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) |
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) |
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) |
                    ((*(sptr) >> 7) | (*(sptr - 1) << 25));
        }
    }
}

static void
ferode_1_13(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 7) | (*(sptr - 1) << 25)) &
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) &
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) &
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) &
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) &
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) &
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) &
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) &
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) &
                    ((*(sptr) << 7) | (*(sptr + 1) >> 25));
        }
    }
}

static void
fdilate_1_14(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 10) | (*(sptr + 1) >> 22)) |
                    ((*(sptr) << 9) | (*(sptr + 1) >> 23)) |
                    ((*(sptr) << 8) | (*(sptr + 1) >> 24)) |
                    ((*(sptr) << 7) | (*(sptr + 1) >> 25)) |
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) |
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) |
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) |
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) |
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) |
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) |
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) |
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) |
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) |
                    ((*(sptr) >> 7) | (*(sptr - 1) << 25)) |
                    ((*(sptr) >> 8) | (*(sptr - 1) << 24)) |
                    ((*(sptr) >> 9) | (*(sptr - 1) << 23));
        }
    }
}

static void
ferode_1_14(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 10) | (*(sptr - 1) << 22)) &
                    ((*(sptr) >> 9) | (*(sptr - 1) << 23)) &
                    ((*(sptr) >> 8) | (*(sptr - 1) << 24)) &
                    ((*(sptr) >> 7) | (*(sptr - 1) << 25)) &
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) &
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) &
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) &
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) &
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) &
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) &
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) &
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) &
                    ((*(sptr) << 7) | (*(sptr + 1) >> 25)) &
                    ((*(sptr) << 8) | (*(sptr + 1) >> 24)) &
                    ((*(sptr) << 9) | (*(sptr + 1) >> 23));
        }
    }
}

static void
fdilate_1_15(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 10) | (*(sptr + 1) >> 22)) |
                    ((*(sptr) << 9) | (*(sptr + 1) >> 23)) |
                    ((*(sptr) << 8) | (*(sptr + 1) >> 24)) |
                    ((*(sptr) << 7) | (*(sptr + 1) >> 25)) |
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) |
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) |
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) |
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) |
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) |
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) |
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) |
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) |
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) |
                    ((*(sptr) >> 7) | (*(sptr - 1) << 25)) |
                    ((*(sptr) >> 8) | (*(sptr - 1) << 24)) |
                    ((*(sptr) >> 9) | (*(sptr - 1) << 23)) |
                    ((*(sptr) >> 10) | (*(sptr - 1) << 22));
        }
    }
}

static void
ferode_1_15(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 10) | (*(sptr - 1) << 22)) &
                    ((*(sptr) >> 9) | (*(sptr - 1) << 23)) &
                    ((*(sptr) >> 8) | (*(sptr - 1) << 24)) &
                    ((*(sptr) >> 7) | (*(sptr - 1) << 25)) &
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) &
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) &
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) &
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) &
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) &
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) &
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) &
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) &
                    ((*(sptr) << 7) | (*(sptr + 1) >> 25)) &
                    ((*(sptr) << 8) | (*(sptr + 1) >> 24)) &
                    ((*(sptr) << 9) | (*(sptr + 1) >> 23)) &
                    ((*(sptr) << 10) | (*(sptr + 1) >> 22));
        }
    }
}

static void
fdilate_1_16(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 12) | (*(sptr + 1) >> 20)) |
                    ((*(sptr) << 11) | (*(sptr + 1) >> 21)) |
                    ((*(sptr) << 10) | (*(sptr + 1) >> 22)) |
                    ((*(sptr) << 9) | (*(sptr + 1) >> 23)) |
                    ((*(sptr) << 8) | (*(sptr + 1) >> 24)) |
                    ((*(sptr) << 7) | (*(sptr + 1) >> 25)) |
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) |
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) |
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) |
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) |
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) |
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) |
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) |
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) |
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) |
                    ((*(sptr) >> 7) | (*(sptr - 1) << 25)) |
                    ((*(sptr) >> 8) | (*(sptr - 1) << 24)) |
                    ((*(sptr) >> 9) | (*(sptr - 1) << 23)) |
                    ((*(sptr) >> 10) | (*(sptr - 1) << 22)) |
                    ((*(sptr) >> 11) | (*(sptr - 1) << 21)) |
                    ((*(sptr) >> 12) | (*(sptr - 1) << 20));
        }
    }
}

static void
ferode_1_16(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 12) | (*(sptr - 1) << 20)) &
                    ((*(sptr) >> 11) | (*(sptr - 1) << 21)) &
                    ((*(sptr) >> 10) | (*(sptr - 1) << 22)) &
                    ((*(sptr) >> 9) | (*(sptr - 1) << 23)) &
                    ((*(sptr) >> 8) | (*(sptr - 1) << 24)) &
                    ((*(sptr) >> 7) | (*(sptr - 1) << 25)) &
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) &
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) &
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) &
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) &
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) &
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) &
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) &
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) &
                    ((*(sptr) << 7) | (*(sptr + 1) >> 25)) &
                    ((*(sptr) << 8) | (*(sptr + 1) >> 24)) &
                    ((*(sptr) << 9) | (*(sptr + 1) >> 23)) &
                    ((*(sptr) << 10) | (*(sptr + 1) >> 22)) &
                    ((*(sptr) << 11) | (*(sptr + 1) >> 21)) &
                    ((*(sptr) << 12) | (*(sptr + 1) >> 20));
        }
    }
}

static void
fdilate_1_17(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 15) | (*(sptr + 1) >> 17)) |
                    ((*(sptr) << 14) | (*(sptr + 1) >> 18)) |
                    ((*(sptr) << 13) | (*(sptr + 1) >> 19)) |
                    ((*(sptr) << 12) | (*(sptr + 1) >> 20)) |
                    ((*(sptr) << 11) | (*(sptr + 1) >> 21)) |
                    ((*(sptr) << 10) | (*(sptr + 1) >> 22)) |
                    ((*(sptr) << 9) | (*(sptr + 1) >> 23)) |
                    ((*(sptr) << 8) | (*(sptr + 1) >> 24)) |
                    ((*(sptr) << 7) | (*(sptr + 1) >> 25)) |
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) |
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) |
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) |
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) |
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) |
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) |
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) |
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) |
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) |
                    ((*(sptr) >> 7) | (*(sptr - 1) << 25)) |
                    ((*(sptr) >> 8) | (*(sptr - 1) << 24)) |
                    ((*(sptr) >> 9) | (*(sptr - 1) << 23)) |
                    ((*(sptr) >> 10) | (*(sptr - 1) << 22)) |
                    ((*(sptr) >> 11) | (*(sptr - 1) << 21)) |
                    ((*(sptr) >> 12) | (*(sptr - 1) << 20)) |
                    ((*(sptr) >> 13) | (*(sptr - 1) << 19)) |
                    ((*(sptr) >> 14) | (*(sptr - 1) << 18));
        }
    }
}

static void
ferode_1_17(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 15) | (*(sptr - 1) << 17)) &
                    ((*(sptr) >> 14) | (*(sptr - 1) << 18)) &
                    ((*(sptr) >> 13) | (*(sptr - 1) << 19)) &
                    ((*(sptr) >> 12) | (*(sptr - 1) << 20)) &
                    ((*(sptr) >> 11) | (*(sptr - 1) << 21)) &
                    ((*(sptr) >> 10) | (*(sptr - 1) << 22)) &
                    ((*(sptr) >> 9) | (*(sptr - 1) << 23)) &
                    ((*(sptr) >> 8) | (*(sptr - 1) << 24)) &
                    ((*(sptr) >> 7) | (*(sptr - 1) << 25)) &
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) &
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) &
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) &
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) &
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) &
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) &
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) &
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) &
                    ((*(sptr) << 7) | (*(sptr + 1) >> 25)) &
                    ((*(sptr) << 8) | (*(sptr + 1) >> 24)) &
                    ((*(sptr) << 9) | (*(sptr + 1) >> 23)) &
                    ((*(sptr) << 10) | (*(sptr + 1) >> 22)) &
                    ((*(sptr) << 11) | (*(sptr + 1) >> 21)) &
                    ((*(sptr) << 12) | (*(sptr + 1) >> 20)) &
                    ((*(sptr) << 13) | (*(sptr + 1) >> 19)) &
                    ((*(sptr) << 14) | (*(sptr + 1) >> 18));
        }
    }
}

static void
fdilate_1_18(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 15) | (*(sptr + 1) >> 17)) |
                    ((*(sptr) << 14) | (*(sptr + 1) >> 18)) |
                    ((*(sptr) << 13) | (*(sptr + 1) >> 19)) |
                    ((*(sptr) << 12) | (*(sptr + 1) >> 20)) |
                    ((*(sptr) << 11) | (*(sptr + 1) >> 21)) |
                    ((*(sptr) << 10) | (*(sptr + 1) >> 22)) |
                    ((*(sptr) << 9) | (*(sptr + 1) >> 23)) |
                    ((*(sptr) << 8) | (*(sptr + 1) >> 24)) |
                    ((*(sptr) << 7) | (*(sptr + 1) >> 25)) |
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) |
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) |
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) |
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) |
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) |
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) |
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) |
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) |
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) |
                    ((*(sptr) >> 7) | (*(sptr - 1) << 25)) |
                    ((*(sptr) >> 8) | (*(sptr - 1) << 24)) |
                    ((*(sptr) >> 9) | (*(sptr - 1) << 23)) |
                    ((*(sptr) >> 10) | (*(sptr - 1) << 22)) |
                    ((*(sptr) >> 11) | (*(sptr - 1) << 21)) |
                    ((*(sptr) >> 12) | (*(sptr - 1) << 20)) |
                    ((*(sptr) >> 13) | (*(sptr - 1) << 19)) |
                    ((*(sptr) >> 14) | (*(sptr - 1) << 18)) |
                    ((*(sptr) >> 15) | (*(sptr - 1) << 17));
        }
    }
}

static void
ferode_1_18(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 15) | (*(sptr - 1) << 17)) &
                    ((*(sptr) >> 14) | (*(sptr - 1) << 18)) &
                    ((*(sptr) >> 13) | (*(sptr - 1) << 19)) &
                    ((*(sptr) >> 12) | (*(sptr - 1) << 20)) &
                    ((*(sptr) >> 11) | (*(sptr - 1) << 21)) &
                    ((*(sptr) >> 10) | (*(sptr - 1) << 22)) &
                    ((*(sptr) >> 9) | (*(sptr - 1) << 23)) &
                    ((*(sptr) >> 8) | (*(sptr - 1) << 24)) &
                    ((*(sptr) >> 7) | (*(sptr - 1) << 25)) &
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) &
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) &
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) &
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) &
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) &
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) &
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) &
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) &
                    ((*(sptr) << 7) | (*(sptr + 1) >> 25)) &
                    ((*(sptr) << 8) | (*(sptr + 1) >> 24)) &
                    ((*(sptr) << 9) | (*(sptr + 1) >> 23)) &
                    ((*(sptr) << 10) | (*(sptr + 1) >> 22)) &
                    ((*(sptr) << 11) | (*(sptr + 1) >> 21)) &
                    ((*(sptr) << 12) | (*(sptr + 1) >> 20)) &
                    ((*(sptr) << 13) | (*(sptr + 1) >> 19)) &
                    ((*(sptr) << 14) | (*(sptr + 1) >> 18)) &
                    ((*(sptr) << 15) | (*(sptr + 1) >> 17));
        }
    }
}

static void
fdilate_1_19(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 17) | (*(sptr + 1) >> 15)) |
                    ((*(sptr) << 16) | (*(sptr + 1) >> 16)) |
                    ((*(sptr) << 15) | (*(sptr + 1) >> 17)) |
                    ((*(sptr) << 14) | (*(sptr + 1) >> 18)) |
                    ((*(sptr) << 13) | (*(sptr + 1) >> 19)) |
                    ((*(sptr) << 12) | (*(sptr + 1) >> 20)) |
                    ((*(sptr) << 11) | (*(sptr + 1) >> 21)) |
                    ((*(sptr) << 10) | (*(sptr + 1) >> 22)) |
                    ((*(sptr) << 9) | (*(sptr + 1) >> 23)) |
                    ((*(sptr) << 8) | (*(sptr + 1) >> 24)) |
                    ((*(sptr) << 7) | (*(sptr + 1) >> 25)) |
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) |
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) |
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) |
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) |
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) |
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) |
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) |
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) |
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) |
                    ((*(sptr) >> 7) | (*(sptr - 1) << 25)) |
                    ((*(sptr) >> 8) | (*(sptr - 1) << 24)) |
                    ((*(sptr) >> 9) | (*(sptr - 1) << 23)) |
                    ((*(sptr) >> 10) | (*(sptr - 1) << 22)) |
                    ((*(sptr) >> 11) | (*(sptr - 1) << 21)) |
                    ((*(sptr) >> 12) | (*(sptr - 1) << 20)) |
                    ((*(sptr) >> 13) | (*(sptr - 1) << 19)) |
                    ((*(sptr) >> 14) | (*(sptr - 1) << 18)) |
                    ((*(sptr) >> 15) | (*(sptr - 1) << 17)) |
                    ((*(sptr) >> 16) | (*(sptr - 1) << 16)) |
                    ((*(sptr) >> 17) | (*(sptr - 1) << 15));
        }
    }
}

static void
ferode_1_19(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 17) | (*(sptr - 1) << 15)) &
                    ((*(sptr) >> 16) | (*(sptr - 1) << 16)) &
                    ((*(sptr) >> 15) | (*(sptr - 1) << 17)) &
                    ((*(sptr) >> 14) | (*(sptr - 1) << 18)) &
                    ((*(sptr) >> 13) | (*(sptr - 1) << 19)) &
                    ((*(sptr) >> 12) | (*(sptr - 1) << 20)) &
                    ((*(sptr) >> 11) | (*(sptr - 1) << 21)) &
                    ((*(sptr) >> 10) | (*(sptr - 1) << 22)) &
                    ((*(sptr) >> 9) | (*(sptr - 1) << 23)) &
                    ((*(sptr) >> 8) | (*(sptr - 1) << 24)) &
                    ((*(sptr) >> 7) | (*(sptr - 1) << 25)) &
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) &
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) &
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) &
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) &
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) &
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) &
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) &
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) &
                    ((*(sptr) << 7) | (*(sptr + 1) >> 25)) &
                    ((*(sptr) << 8) | (*(sptr + 1) >> 24)) &
                    ((*(sptr) << 9) | (*(sptr + 1) >> 23)) &
                    ((*(sptr) << 10) | (*(sptr + 1) >> 22)) &
                    ((*(sptr) << 11) | (*(sptr + 1) >> 21)) &
                    ((*(sptr) << 12) | (*(sptr + 1) >> 20)) &
                    ((*(sptr) << 13) | (*(sptr + 1) >> 19)) &
                    ((*(sptr) << 14) | (*(sptr + 1) >> 18)) &
                    ((*(sptr) << 15) | (*(sptr + 1) >> 17)) &
                    ((*(sptr) << 16) | (*(sptr + 1) >> 16)) &
                    ((*(sptr) << 17) | (*(sptr + 1) >> 15));
        }
    }
}

static void
fdilate_1_20(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 20) | (*(sptr + 1) >> 12)) |
                    ((*(sptr) << 19) | (*(sptr + 1) >> 13)) |
                    ((*(sptr) << 18) | (*(sptr + 1) >> 14)) |
                    ((*(sptr) << 17) | (*(sptr + 1) >> 15)) |
                    ((*(sptr) << 16) | (*(sptr + 1) >> 16)) |
                    ((*(sptr) << 15) | (*(sptr + 1) >> 17)) |
                    ((*(sptr) << 14) | (*(sptr + 1) >> 18)) |
                    ((*(sptr) << 13) | (*(sptr + 1) >> 19)) |
                    ((*(sptr) << 12) | (*(sptr + 1) >> 20)) |
                    ((*(sptr) << 11) | (*(sptr + 1) >> 21)) |
                    ((*(sptr) << 10) | (*(sptr + 1) >> 22)) |
                    ((*(sptr) << 9) | (*(sptr + 1) >> 23)) |
                    ((*(sptr) << 8) | (*(sptr + 1) >> 24)) |
                    ((*(sptr) << 7) | (*(sptr + 1) >> 25)) |
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) |
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) |
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) |
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) |
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) |
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) |
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) |
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) |
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) |
                    ((*(sptr) >> 7) | (*(sptr - 1) << 25)) |
                    ((*(sptr) >> 8) | (*(sptr - 1) << 24)) |
                    ((*(sptr) >> 9) | (*(sptr - 1) << 23)) |
                    ((*(sptr) >> 10) | (*(sptr - 1) << 22)) |
                    ((*(sptr) >> 11) | (*(sptr - 1) << 21)) |
                    ((*(sptr) >> 12) | (*(sptr - 1) << 20)) |
                    ((*(sptr) >> 13) | (*(sptr - 1) << 19)) |
                    ((*(sptr) >> 14) | (*(sptr - 1) << 18)) |
                    ((*(sptr) >> 15) | (*(sptr - 1) << 17)) |
                    ((*(sptr) >> 16) | (*(sptr - 1) << 16)) |
                    ((*(sptr) >> 17) | (*(sptr - 1) << 15)) |
                    ((*(sptr) >> 18) | (*(sptr - 1) << 14)) |
                    ((*(sptr) >> 19) | (*(sptr - 1) << 13));
        }
    }
}

static void
ferode_1_20(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 20) | (*(sptr - 1) << 12)) &
                    ((*(sptr) >> 19) | (*(sptr - 1) << 13)) &
                    ((*(sptr) >> 18) | (*(sptr - 1) << 14)) &
                    ((*(sptr) >> 17) | (*(sptr - 1) << 15)) &
                    ((*(sptr) >> 16) | (*(sptr - 1) << 16)) &
                    ((*(sptr) >> 15) | (*(sptr - 1) << 17)) &
                    ((*(sptr) >> 14) | (*(sptr - 1) << 18)) &
                    ((*(sptr) >> 13) | (*(sptr - 1) << 19)) &
                    ((*(sptr) >> 12) | (*(sptr - 1) << 20)) &
                    ((*(sptr) >> 11) | (*(sptr - 1) << 21)) &
                    ((*(sptr) >> 10) | (*(sptr - 1) << 22)) &
                    ((*(sptr) >> 9) | (*(sptr - 1) << 23)) &
                    ((*(sptr) >> 8) | (*(sptr - 1) << 24)) &
                    ((*(sptr) >> 7) | (*(sptr - 1) << 25)) &
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) &
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) &
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) &
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) &
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) &
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) &
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) &
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) &
                    ((*(sptr) << 7) | (*(sptr + 1) >> 25)) &
                    ((*(sptr) << 8) | (*(sptr + 1) >> 24)) &
                    ((*(sptr) << 9) | (*(sptr + 1) >> 23)) &
                    ((*(sptr) << 10) | (*(sptr + 1) >> 22)) &
                    ((*(sptr) << 11) | (*(sptr + 1) >> 21)) &
                    ((*(sptr) << 12) | (*(sptr + 1) >> 20)) &
                    ((*(sptr) << 13) | (*(sptr + 1) >> 19)) &
                    ((*(sptr) << 14) | (*(sptr + 1) >> 18)) &
                    ((*(sptr) << 15) | (*(sptr + 1) >> 17)) &
                    ((*(sptr) << 16) | (*(sptr + 1) >> 16)) &
                    ((*(sptr) << 17) | (*(sptr + 1) >> 15)) &
                    ((*(sptr) << 18) | (*(sptr + 1) >> 14)) &
                    ((*(sptr) << 19) | (*(sptr + 1) >> 13));
        }
    }
}

static void
fdilate_1_21(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 20) | (*(sptr + 1) >> 12)) |
                    ((*(sptr) << 19) | (*(sptr + 1) >> 13)) |
                    ((*(sptr) << 18) | (*(sptr + 1) >> 14)) |
                    ((*(sptr) << 17) | (*(sptr + 1) >> 15)) |
                    ((*(sptr) << 16) | (*(sptr + 1) >> 16)) |
                    ((*(sptr) << 15) | (*(sptr + 1) >> 17)) |
                    ((*(sptr) << 14) | (*(sptr + 1) >> 18)) |
                    ((*(sptr) << 13) | (*(sptr + 1) >> 19)) |
                    ((*(sptr) << 12) | (*(sptr + 1) >> 20)) |
                    ((*(sptr) << 11) | (*(sptr + 1) >> 21)) |
                    ((*(sptr) << 10) | (*(sptr + 1) >> 22)) |
                    ((*(sptr) << 9) | (*(sptr + 1) >> 23)) |
                    ((*(sptr) << 8) | (*(sptr + 1) >> 24)) |
                    ((*(sptr) << 7) | (*(sptr + 1) >> 25)) |
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) |
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) |
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) |
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) |
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) |
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) |
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) |
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) |
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) |
                    ((*(sptr) >> 7) | (*(sptr - 1) << 25)) |
                    ((*(sptr) >> 8) | (*(sptr - 1) << 24)) |
                    ((*(sptr) >> 9) | (*(sptr - 1) << 23)) |
                    ((*(sptr) >> 10) | (*(sptr - 1) << 22)) |
                    ((*(sptr) >> 11) | (*(sptr - 1) << 21)) |
                    ((*(sptr) >> 12) | (*(sptr - 1) << 20)) |
                    ((*(sptr) >> 13) | (*(sptr - 1) << 19)) |
                    ((*(sptr) >> 14) | (*(sptr - 1) << 18)) |
                    ((*(sptr) >> 15) | (*(sptr - 1) << 17)) |
                    ((*(sptr) >> 16) | (*(sptr - 1) << 16)) |
                    ((*(sptr) >> 17) | (*(sptr - 1) << 15)) |
                    ((*(sptr) >> 18) | (*(sptr - 1) << 14)) |
                    ((*(sptr) >> 19) | (*(sptr - 1) << 13)) |
                    ((*(sptr) >> 20) | (*(sptr - 1) << 12));
        }
    }
}

static void
ferode_1_21(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 20) | (*(sptr - 1) << 12)) &
                    ((*(sptr) >> 19) | (*(sptr - 1) << 13)) &
                    ((*(sptr) >> 18) | (*(sptr - 1) << 14)) &
                    ((*(sptr) >> 17) | (*(sptr - 1) << 15)) &
                    ((*(sptr) >> 16) | (*(sptr - 1) << 16)) &
                    ((*(sptr) >> 15) | (*(sptr - 1) << 17)) &
                    ((*(sptr) >> 14) | (*(sptr - 1) << 18)) &
                    ((*(sptr) >> 13) | (*(sptr - 1) << 19)) &
                    ((*(sptr) >> 12) | (*(sptr - 1) << 20)) &
                    ((*(sptr) >> 11) | (*(sptr - 1) << 21)) &
                    ((*(sptr) >> 10) | (*(sptr - 1) << 22)) &
                    ((*(sptr) >> 9) | (*(sptr - 1) << 23)) &
                    ((*(sptr) >> 8) | (*(sptr - 1) << 24)) &
                    ((*(sptr) >> 7) | (*(sptr - 1) << 25)) &
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) &
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) &
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) &
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) &
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) &
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) &
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) &
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) &
                    ((*(sptr) << 7) | (*(sptr + 1) >> 25)) &
                    ((*(sptr) << 8) | (*(sptr + 1) >> 24)) &
                    ((*(sptr) << 9) | (*(sptr + 1) >> 23)) &
                    ((*(sptr) << 10) | (*(sptr + 1) >> 22)) &
                    ((*(sptr) << 11) | (*(sptr + 1) >> 21)) &
                    ((*(sptr) << 12) | (*(sptr + 1) >> 20)) &
                    ((*(sptr) << 13) | (*(sptr + 1) >> 19)) &
                    ((*(sptr) << 14) | (*(sptr + 1) >> 18)) &
                    ((*(sptr) << 15) | (*(sptr + 1) >> 17)) &
                    ((*(sptr) << 16) | (*(sptr + 1) >> 16)) &
                    ((*(sptr) << 17) | (*(sptr + 1) >> 15)) &
                    ((*(sptr) << 18) | (*(sptr + 1) >> 14)) &
                    ((*(sptr) << 19) | (*(sptr + 1) >> 13)) &
                    ((*(sptr) << 20) | (*(sptr + 1) >> 12));
        }
    }
}

static void
fdilate_1_22(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 22) | (*(sptr + 1) >> 10)) |
                    ((*(sptr) << 21) | (*(sptr + 1) >> 11)) |
                    ((*(sptr) << 20) | (*(sptr + 1) >> 12)) |
                    ((*(sptr) << 19) | (*(sptr + 1) >> 13)) |
                    ((*(sptr) << 18) | (*(sptr + 1) >> 14)) |
                    ((*(sptr) << 17) | (*(sptr + 1) >> 15)) |
                    ((*(sptr) << 16) | (*(sptr + 1) >> 16)) |
                    ((*(sptr) << 15) | (*(sptr + 1) >> 17)) |
                    ((*(sptr) << 14) | (*(sptr + 1) >> 18)) |
                    ((*(sptr) << 13) | (*(sptr + 1) >> 19)) |
                    ((*(sptr) << 12) | (*(sptr + 1) >> 20)) |
                    ((*(sptr) << 11) | (*(sptr + 1) >> 21)) |
                    ((*(sptr) << 10) | (*(sptr + 1) >> 22)) |
                    ((*(sptr) << 9) | (*(sptr + 1) >> 23)) |
                    ((*(sptr) << 8) | (*(sptr + 1) >> 24)) |
                    ((*(sptr) << 7) | (*(sptr + 1) >> 25)) |
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) |
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) |
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) |
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) |
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) |
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) |
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) |
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) |
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) |
                    ((*(sptr) >> 7) | (*(sptr - 1) << 25)) |
                    ((*(sptr) >> 8) | (*(sptr - 1) << 24)) |
                    ((*(sptr) >> 9) | (*(sptr - 1) << 23)) |
                    ((*(sptr) >> 10) | (*(sptr - 1) << 22)) |
                    ((*(sptr) >> 11) | (*(sptr - 1) << 21)) |
                    ((*(sptr) >> 12) | (*(sptr - 1) << 20)) |
                    ((*(sptr) >> 13) | (*(sptr - 1) << 19)) |
                    ((*(sptr) >> 14) | (*(sptr - 1) << 18)) |
                    ((*(sptr) >> 15) | (*(sptr - 1) << 17)) |
                    ((*(sptr) >> 16) | (*(sptr - 1) << 16)) |
                    ((*(sptr) >> 17) | (*(sptr - 1) << 15)) |
                    ((*(sptr) >> 18) | (*(sptr - 1) << 14)) |
                    ((*(sptr) >> 19) | (*(sptr - 1) << 13)) |
                    ((*(sptr) >> 20) | (*(sptr - 1) << 12)) |
                    ((*(sptr) >> 21) | (*(sptr - 1) << 11)) |
                    ((*(sptr) >> 22) | (*(sptr - 1) << 10));
        }
    }
}

static void
ferode_1_22(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 22) | (*(sptr - 1) << 10)) &
                    ((*(sptr) >> 21) | (*(sptr - 1) << 11)) &
                    ((*(sptr) >> 20) | (*(sptr - 1) << 12)) &
                    ((*(sptr) >> 19) | (*(sptr - 1) << 13)) &
                    ((*(sptr) >> 18) | (*(sptr - 1) << 14)) &
                    ((*(sptr) >> 17) | (*(sptr - 1) << 15)) &
                    ((*(sptr) >> 16) | (*(sptr - 1) << 16)) &
                    ((*(sptr) >> 15) | (*(sptr - 1) << 17)) &
                    ((*(sptr) >> 14) | (*(sptr - 1) << 18)) &
                    ((*(sptr) >> 13) | (*(sptr - 1) << 19)) &
                    ((*(sptr) >> 12) | (*(sptr - 1) << 20)) &
                    ((*(sptr) >> 11) | (*(sptr - 1) << 21)) &
                    ((*(sptr) >> 10) | (*(sptr - 1) << 22)) &
                    ((*(sptr) >> 9) | (*(sptr - 1) << 23)) &
                    ((*(sptr) >> 8) | (*(sptr - 1) << 24)) &
                    ((*(sptr) >> 7) | (*(sptr - 1) << 25)) &
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) &
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) &
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) &
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) &
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) &
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) &
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) &
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) &
                    ((*(sptr) << 7) | (*(sptr + 1) >> 25)) &
                    ((*(sptr) << 8) | (*(sptr + 1) >> 24)) &
                    ((*(sptr) << 9) | (*(sptr + 1) >> 23)) &
                    ((*(sptr) << 10) | (*(sptr + 1) >> 22)) &
                    ((*(sptr) << 11) | (*(sptr + 1) >> 21)) &
                    ((*(sptr) << 12) | (*(sptr + 1) >> 20)) &
                    ((*(sptr) << 13) | (*(sptr + 1) >> 19)) &
                    ((*(sptr) << 14) | (*(sptr + 1) >> 18)) &
                    ((*(sptr) << 15) | (*(sptr + 1) >> 17)) &
                    ((*(sptr) << 16) | (*(sptr + 1) >> 16)) &
                    ((*(sptr) << 17) | (*(sptr + 1) >> 15)) &
                    ((*(sptr) << 18) | (*(sptr + 1) >> 14)) &
                    ((*(sptr) << 19) | (*(sptr + 1) >> 13)) &
                    ((*(sptr) << 20) | (*(sptr + 1) >> 12)) &
                    ((*(sptr) << 21) | (*(sptr + 1) >> 11)) &
                    ((*(sptr) << 22) | (*(sptr + 1) >> 10));
        }
    }
}

static void
fdilate_1_23(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 25) | (*(sptr + 1) >> 7)) |
                    ((*(sptr) << 24) | (*(sptr + 1) >> 8)) |
                    ((*(sptr) << 23) | (*(sptr + 1) >> 9)) |
                    ((*(sptr) << 22) | (*(sptr + 1) >> 10)) |
                    ((*(sptr) << 21) | (*(sptr + 1) >> 11)) |
                    ((*(sptr) << 20) | (*(sptr + 1) >> 12)) |
                    ((*(sptr) << 19) | (*(sptr + 1) >> 13)) |
                    ((*(sptr) << 18) | (*(sptr + 1) >> 14)) |
                    ((*(sptr) << 17) | (*(sptr + 1) >> 15)) |
                    ((*(sptr) << 16) | (*(sptr + 1) >> 16)) |
                    ((*(sptr) << 15) | (*(sptr + 1) >> 17)) |
                    ((*(sptr) << 14) | (*(sptr + 1) >> 18)) |
                    ((*(sptr) << 13) | (*(sptr + 1) >> 19)) |
                    ((*(sptr) << 12) | (*(sptr + 1) >> 20)) |
                    ((*(sptr) << 11) | (*(sptr + 1) >> 21)) |
                    ((*(sptr) << 10) | (*(sptr + 1) >> 22)) |
                    ((*(sptr) << 9) | (*(sptr + 1) >> 23)) |
                    ((*(sptr) << 8) | (*(sptr + 1) >> 24)) |
                    ((*(sptr) << 7) | (*(sptr + 1) >> 25)) |
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) |
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) |
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) |
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) |
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) |
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) |
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) |
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) |
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) |
                    ((*(sptr) >> 7) | (*(sptr - 1) << 25)) |
                    ((*(sptr) >> 8) | (*(sptr - 1) << 24)) |
                    ((*(sptr) >> 9) | (*(sptr - 1) << 23)) |
                    ((*(sptr) >> 10) | (*(sptr - 1) << 22)) |
                    ((*(sptr) >> 11) | (*(sptr - 1) << 21)) |
                    ((*(sptr) >> 12) | (*(sptr - 1) << 20)) |
                    ((*(sptr) >> 13) | (*(sptr - 1) << 19)) |
                    ((*(sptr) >> 14) | (*(sptr - 1) << 18)) |
                    ((*(sptr) >> 15) | (*(sptr - 1) << 17)) |
                    ((*(sptr) >> 16) | (*(sptr - 1) << 16)) |
                    ((*(sptr) >> 17) | (*(sptr - 1) << 15)) |
                    ((*(sptr) >> 18) | (*(sptr - 1) << 14)) |
                    ((*(sptr) >> 19) | (*(sptr - 1) << 13)) |
                    ((*(sptr) >> 20) | (*(sptr - 1) << 12)) |
                    ((*(sptr) >> 21) | (*(sptr - 1) << 11)) |
                    ((*(sptr) >> 22) | (*(sptr - 1) << 10)) |
                    ((*(sptr) >> 23) | (*(sptr - 1) << 9)) |
                    ((*(sptr) >> 24) | (*(sptr - 1) << 8));
        }
    }
}

static void
ferode_1_23(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 25) | (*(sptr - 1) << 7)) &
                    ((*(sptr) >> 24) | (*(sptr - 1) << 8)) &
                    ((*(sptr) >> 23) | (*(sptr - 1) << 9)) &
                    ((*(sptr) >> 22) | (*(sptr - 1) << 10)) &
                    ((*(sptr) >> 21) | (*(sptr - 1) << 11)) &
                    ((*(sptr) >> 20) | (*(sptr - 1) << 12)) &
                    ((*(sptr) >> 19) | (*(sptr - 1) << 13)) &
                    ((*(sptr) >> 18) | (*(sptr - 1) << 14)) &
                    ((*(sptr) >> 17) | (*(sptr - 1) << 15)) &
                    ((*(sptr) >> 16) | (*(sptr - 1) << 16)) &
                    ((*(sptr) >> 15) | (*(sptr - 1) << 17)) &
                    ((*(sptr) >> 14) | (*(sptr - 1) << 18)) &
                    ((*(sptr) >> 13) | (*(sptr - 1) << 19)) &
                    ((*(sptr) >> 12) | (*(sptr - 1) << 20)) &
                    ((*(sptr) >> 11) | (*(sptr - 1) << 21)) &
                    ((*(sptr) >> 10) | (*(sptr - 1) << 22)) &
                    ((*(sptr) >> 9) | (*(sptr - 1) << 23)) &
                    ((*(sptr) >> 8) | (*(sptr - 1) << 24)) &
                    ((*(sptr) >> 7) | (*(sptr - 1) << 25)) &
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) &
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) &
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) &
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) &
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) &
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) &
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) &
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) &
                    ((*(sptr) << 7) | (*(sptr + 1) >> 25)) &
                    ((*(sptr) << 8) | (*(sptr + 1) >> 24)) &
                    ((*(sptr) << 9) | (*(sptr + 1) >> 23)) &
                    ((*(sptr) << 10) | (*(sptr + 1) >> 22)) &
                    ((*(sptr) << 11) | (*(sptr + 1) >> 21)) &
                    ((*(sptr) << 12) | (*(sptr + 1) >> 20)) &
                    ((*(sptr) << 13) | (*(sptr + 1) >> 19)) &
                    ((*(sptr) << 14) | (*(sptr + 1) >> 18)) &
                    ((*(sptr) << 15) | (*(sptr + 1) >> 17)) &
                    ((*(sptr) << 16) | (*(sptr + 1) >> 16)) &
                    ((*(sptr) << 17) | (*(sptr + 1) >> 15)) &
                    ((*(sptr) << 18) | (*(sptr + 1) >> 14)) &
                    ((*(sptr) << 19) | (*(sptr + 1) >> 13)) &
                    ((*(sptr) << 20) | (*(sptr + 1) >> 12)) &
                    ((*(sptr) << 21) | (*(sptr + 1) >> 11)) &
                    ((*(sptr) << 22) | (*(sptr + 1) >> 10)) &
                    ((*(sptr) << 23) | (*(sptr + 1) >> 9)) &
                    ((*(sptr) << 24) | (*(sptr + 1) >> 8));
        }
    }
}

static void
fdilate_1_24(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 25) | (*(sptr + 1) >> 7)) |
                    ((*(sptr) << 24) | (*(sptr + 1) >> 8)) |
                    ((*(sptr) << 23) | (*(sptr + 1) >> 9)) |
                    ((*(sptr) << 22) | (*(sptr + 1) >> 10)) |
                    ((*(sptr) << 21) | (*(sptr + 1) >> 11)) |
                    ((*(sptr) << 20) | (*(sptr + 1) >> 12)) |
                    ((*(sptr) << 19) | (*(sptr + 1) >> 13)) |
                    ((*(sptr) << 18) | (*(sptr + 1) >> 14)) |
                    ((*(sptr) << 17) | (*(sptr + 1) >> 15)) |
                    ((*(sptr) << 16) | (*(sptr + 1) >> 16)) |
                    ((*(sptr) << 15) | (*(sptr + 1) >> 17)) |
                    ((*(sptr) << 14) | (*(sptr + 1) >> 18)) |
                    ((*(sptr) << 13) | (*(sptr + 1) >> 19)) |
                    ((*(sptr) << 12) | (*(sptr + 1) >> 20)) |
                    ((*(sptr) << 11) | (*(sptr + 1) >> 21)) |
                    ((*(sptr) << 10) | (*(sptr + 1) >> 22)) |
                    ((*(sptr) << 9) | (*(sptr + 1) >> 23)) |
                    ((*(sptr) << 8) | (*(sptr + 1) >> 24)) |
                    ((*(sptr) << 7) | (*(sptr + 1) >> 25)) |
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) |
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) |
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) |
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) |
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) |
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) |
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) |
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) |
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) |
                    ((*(sptr) >> 7) | (*(sptr - 1) << 25)) |
                    ((*(sptr) >> 8) | (*(sptr - 1) << 24)) |
                    ((*(sptr) >> 9) | (*(sptr - 1) << 23)) |
                    ((*(sptr) >> 10) | (*(sptr - 1) << 22)) |
                    ((*(sptr) >> 11) | (*(sptr - 1) << 21)) |
                    ((*(sptr) >> 12) | (*(sptr - 1) << 20)) |
                    ((*(sptr) >> 13) | (*(sptr - 1) << 19)) |
                    ((*(sptr) >> 14) | (*(sptr - 1) << 18)) |
                    ((*(sptr) >> 15) | (*(sptr - 1) << 17)) |
                    ((*(sptr) >> 16) | (*(sptr - 1) << 16)) |
                    ((*(sptr) >> 17) | (*(sptr - 1) << 15)) |
                    ((*(sptr) >> 18) | (*(sptr - 1) << 14)) |
                    ((*(sptr) >> 19) | (*(sptr - 1) << 13)) |
                    ((*(sptr) >> 20) | (*(sptr - 1) << 12)) |
                    ((*(sptr) >> 21) | (*(sptr - 1) << 11)) |
                    ((*(sptr) >> 22) | (*(sptr - 1) << 10)) |
                    ((*(sptr) >> 23) | (*(sptr - 1) << 9)) |
                    ((*(sptr) >> 24) | (*(sptr - 1) << 8)) |
                    ((*(sptr) >> 25) | (*(sptr - 1) << 7));
        }
    }
}

static void
ferode_1_24(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 25) | (*(sptr - 1) << 7)) &
                    ((*(sptr) >> 24) | (*(sptr - 1) << 8)) &
                    ((*(sptr) >> 23) | (*(sptr - 1) << 9)) &
                    ((*(sptr) >> 22) | (*(sptr - 1) << 10)) &
                    ((*(sptr) >> 21) | (*(sptr - 1) << 11)) &
                    ((*(sptr) >> 20) | (*(sptr - 1) << 12)) &
                    ((*(sptr) >> 19) | (*(sptr - 1) << 13)) &
                    ((*(sptr) >> 18) | (*(sptr - 1) << 14)) &
                    ((*(sptr) >> 17) | (*(sptr - 1) << 15)) &
                    ((*(sptr) >> 16) | (*(sptr - 1) << 16)) &
                    ((*(sptr) >> 15) | (*(sptr - 1) << 17)) &
                    ((*(sptr) >> 14) | (*(sptr - 1) << 18)) &
                    ((*(sptr) >> 13) | (*(sptr - 1) << 19)) &
                    ((*(sptr) >> 12) | (*(sptr - 1) << 20)) &
                    ((*(sptr) >> 11) | (*(sptr - 1) << 21)) &
                    ((*(sptr) >> 10) | (*(sptr - 1) << 22)) &
                    ((*(sptr) >> 9) | (*(sptr - 1) << 23)) &
                    ((*(sptr) >> 8) | (*(sptr - 1) << 24)) &
                    ((*(sptr) >> 7) | (*(sptr - 1) << 25)) &
                    ((*(sptr) >> 6) | (*(sptr - 1) << 26)) &
                    ((*(sptr) >> 5) | (*(sptr - 1) << 27)) &
                    ((*(sptr) >> 4) | (*(sptr - 1) << 28)) &
                    ((*(sptr) >> 3) | (*(sptr - 1) << 29)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) &
                    ((*(sptr) << 3) | (*(sptr + 1) >> 29)) &
                    ((*(sptr) << 4) | (*(sptr + 1) >> 28)) &
                    ((*(sptr) << 5) | (*(sptr + 1) >> 27)) &
                    ((*(sptr) << 6) | (*(sptr + 1) >> 26)) &
                    ((*(sptr) << 7) | (*(sptr + 1) >> 25)) &
                    ((*(sptr) << 8) | (*(sptr + 1) >> 24)) &
                    ((*(sptr) << 9) | (*(sptr + 1) >> 23)) &
                    ((*(sptr) << 10) | (*(sptr + 1) >> 22)) &
                    ((*(sptr) << 11) | (*(sptr + 1) >> 21)) &
                    ((*(sptr) << 12) | (*(sptr + 1) >> 20)) &
                    ((*(sptr) << 13) | (*(sptr + 1) >> 19)) &
                    ((*(sptr) << 14) | (*(sptr + 1) >> 18)) &
                    ((*(sptr) << 15) | (*(sptr + 1) >> 17)) &
                    ((*(sptr) << 16) | (*(sptr + 1) >> 16)) &
                    ((*(sptr) << 17) | (*(sptr + 1) >> 15)) &
                    ((*(sptr) << 18) | (*(sptr + 1) >> 14)) &
                    ((*(sptr) << 19) | (*(sptr + 1) >> 13)) &
                    ((*(sptr) << 20) | (*(sptr + 1) >> 12)) &
                    ((*(sptr) << 21) | (*(sptr + 1) >> 11)) &
                    ((*(sptr) << 22) | (*(sptr + 1) >> 10)) &
                    ((*(sptr) << 23) | (*(sptr + 1) >> 9)) &
                    ((*(sptr) << 24) | (*(sptr + 1) >> 8)) &
                    ((*(sptr) << 25) | (*(sptr + 1) >> 7));
        }
    }
}

static void
fdilate_1_25(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls)) |
                    (*sptr);
        }
    }
}

static void
ferode_1_25(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls)) &
                    (*sptr);
        }
    }
}

static void
fdilate_1_26(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls));
        }
    }
}

static void
ferode_1_26(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls));
        }
    }
}

static void
fdilate_1_27(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2;

    wpls2 = 2 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls2)) |
                    (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls));
        }
    }
}

static void
ferode_1_27(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2;

    wpls2 = 2 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls2)) &
                    (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls));
        }
    }
}

static void
fdilate_1_28(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2;

    wpls2 = 2 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls2)) |
                    (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls)) |
                    (*(sptr - wpls2));
        }
    }
}

static void
ferode_1_28(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2;

    wpls2 = 2 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls2)) &
                    (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls)) &
                    (*(sptr + wpls2));
        }
    }
}

static void
fdilate_1_29(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls3)) |
                    (*(sptr + wpls2)) |
                    (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls)) |
                    (*(sptr - wpls2));
        }
    }
}

static void
ferode_1_29(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls3)) &
                    (*(sptr - wpls2)) &
                    (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls)) &
                    (*(sptr + wpls2));
        }
    }
}

static void
fdilate_1_30(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls3)) |
                    (*(sptr + wpls2)) |
                    (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls)) |
                    (*(sptr - wpls2)) |
                    (*(sptr - wpls3));
        }
    }
}

static void
ferode_1_30(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls3)) &
                    (*(sptr - wpls2)) &
                    (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls)) &
                    (*(sptr + wpls2)) &
                    (*(sptr + wpls3));
        }
    }
}

static void
fdilate_1_31(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls4)) |
                    (*(sptr + wpls3)) |
                    (*(sptr + wpls2)) |
                    (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls)) |
                    (*(sptr - wpls2)) |
                    (*(sptr - wpls3));
        }
    }
}

static void
ferode_1_31(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls4)) &
                    (*(sptr - wpls3)) &
                    (*(sptr - wpls2)) &
                    (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls)) &
                    (*(sptr + wpls2)) &
                    (*(sptr + wpls3));
        }
    }
}

static void
fdilate_1_32(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls4)) |
                    (*(sptr + wpls3)) |
                    (*(sptr + wpls2)) |
                    (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls)) |
                    (*(sptr - wpls2)) |
                    (*(sptr - wpls3)) |
                    (*(sptr - wpls4));
        }
    }
}

static void
ferode_1_32(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls4)) &
                    (*(sptr - wpls3)) &
                    (*(sptr - wpls2)) &
                    (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls)) &
                    (*(sptr + wpls2)) &
                    (*(sptr + wpls3)) &
                    (*(sptr + wpls4));
        }
    }
}

static void
fdilate_1_33(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls5)) |
                    (*(sptr + wpls4)) |
                    (*(sptr + wpls3)) |
                    (*(sptr + wpls2)) |
                    (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls)) |
                    (*(sptr - wpls2)) |
                    (*(sptr - wpls3)) |
                    (*(sptr - wpls4));
        }
    }
}

static void
ferode_1_33(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls5)) &
                    (*(sptr - wpls4)) &
                    (*(sptr - wpls3)) &
                    (*(sptr - wpls2)) &
                    (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls)) &
                    (*(sptr + wpls2)) &
                    (*(sptr + wpls3)) &
                    (*(sptr + wpls4));
        }
    }
}

static void
fdilate_1_34(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls5)) |
                    (*(sptr + wpls4)) |
                    (*(sptr + wpls3)) |
                    (*(sptr + wpls2)) |
                    (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls)) |
                    (*(sptr - wpls2)) |
                    (*(sptr - wpls3)) |
                    (*(sptr - wpls4)) |
                    (*(sptr - wpls5));
        }
    }
}

static void
ferode_1_34(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls5)) &
                    (*(sptr - wpls4)) &
                    (*(sptr - wpls3)) &
                    (*(sptr - wpls2)) &
                    (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls)) &
                    (*(sptr + wpls2)) &
                    (*(sptr + wpls3)) &
                    (*(sptr + wpls4)) &
                    (*(sptr + wpls5));
        }
    }
}

static void
fdilate_1_35(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls6)) |
                    (*(sptr + wpls5)) |
                    (*(sptr + wpls4)) |
                    (*(sptr + wpls3)) |
                    (*(sptr + wpls2)) |
                    (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls)) |
                    (*(sptr - wpls2)) |
                    (*(sptr - wpls3)) |
                    (*(sptr - wpls4)) |
                    (*(sptr - wpls5));
        }
    }
}

static void
ferode_1_35(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls6)) &
                    (*(sptr - wpls5)) &
                    (*(sptr - wpls4)) &
                    (*(sptr - wpls3)) &
                    (*(sptr - wpls2)) &
                    (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls)) &
                    (*(sptr + wpls2)) &
                    (*(sptr + wpls3)) &
                    (*(sptr + wpls4)) &
                    (*(sptr + wpls5));
        }
    }
}

static void
fdilate_1_36(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls6)) |
                    (*(sptr + wpls5)) |
                    (*(sptr + wpls4)) |
                    (*(sptr + wpls3)) |
                    (*(sptr + wpls2)) |
                    (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls)) |
                    (*(sptr - wpls2)) |
                    (*(sptr - wpls3)) |
                    (*(sptr - wpls4)) |
                    (*(sptr - wpls5)) |
                    (*(sptr - wpls6));
        }
    }
}

static void
ferode_1_36(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls6)) &
                    (*(sptr - wpls5)) &
                    (*(sptr - wpls4)) &
                    (*(sptr - wpls3)) &
                    (*(sptr - wpls2)) &
                    (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls)) &
                    (*(sptr + wpls2)) &
                    (*(sptr + wpls3)) &
                    (*(sptr + wpls4)) &
                    (*(sptr + wpls5)) &
                    (*(sptr + wpls6));
        }
    }
}

static void
fdilate_1_37(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls7)) |
                    (*(sptr + wpls6)) |
                    (*(sptr + wpls5)) |
                    (*(sptr + wpls4)) |
                    (*(sptr + wpls3)) |
                    (*(sptr + wpls2)) |
                    (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls)) |
                    (*(sptr - wpls2)) |
                    (*(sptr - wpls3)) |
                    (*(sptr - wpls4)) |
                    (*(sptr - wpls5)) |
                    (*(sptr - wpls6));
        }
    }
}

static void
ferode_1_37(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls7)) &
                    (*(sptr - wpls6)) &
                    (*(sptr - wpls5)) &
                    (*(sptr - wpls4)) &
                    (*(sptr - wpls3)) &
                    (*(sptr - wpls2)) &
                    (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls)) &
                    (*(sptr + wpls2)) &
                    (*(sptr + wpls3)) &
                    (*(sptr + wpls4)) &
                    (*(sptr + wpls5)) &
                    (*(sptr + wpls6));
        }
    }
}

static void
fdilate_1_38(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls7)) |
                    (*(sptr + wpls6)) |
                    (*(sptr + wpls5)) |
                    (*(sptr + wpls4)) |
                    (*(sptr + wpls3)) |
                    (*(sptr + wpls2)) |
                    (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls)) |
                    (*(sptr - wpls2)) |
                    (*(sptr - wpls3)) |
                    (*(sptr - wpls4)) |
                    (*(sptr - wpls5)) |
                    (*(sptr - wpls6)) |
                    (*(sptr - wpls7));
        }
    }
}

static void
ferode_1_38(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls7)) &
                    (*(sptr - wpls6)) &
                    (*(sptr - wpls5)) &
                    (*(sptr - wpls4)) &
                    (*(sptr - wpls3)) &
                    (*(sptr - wpls2)) &
                    (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls)) &
                    (*(sptr + wpls2)) &
                    (*(sptr + wpls3)) &
                    (*(sptr + wpls4)) &
                    (*(sptr + wpls5)) &
                    (*(sptr + wpls6)) &
                    (*(sptr + wpls7));
        }
    }
}

static void
fdilate_1_39(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7, wpls8;
l_int32             wpls9, wpls10;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    wpls8 = 8 * wpls;
    wpls9 = 9 * wpls;
    wpls10 = 10 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls10)) |
                    (*(sptr + wpls9)) |
                    (*(sptr + wpls8)) |
                    (*(sptr + wpls7)) |
                    (*(sptr + wpls6)) |
                    (*(sptr + wpls5)) |
                    (*(sptr + wpls4)) |
                    (*(sptr + wpls3)) |
                    (*(sptr + wpls2)) |
                    (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls)) |
                    (*(sptr - wpls2)) |
                    (*(sptr - wpls3)) |
                    (*(sptr - wpls4)) |
                    (*(sptr - wpls5)) |
                    (*(sptr - wpls6)) |
                    (*(sptr - wpls7)) |
                    (*(sptr - wpls8)) |
                    (*(sptr - wpls9));
        }
    }
}

static void
ferode_1_39(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7, wpls8;
l_int32             wpls9, wpls10;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    wpls8 = 8 * wpls;
    wpls9 = 9 * wpls;
    wpls10 = 10 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls10)) &
                    (*(sptr - wpls9)) &
                    (*(sptr - wpls8)) &
                    (*(sptr - wpls7)) &
                    (*(sptr - wpls6)) &
                    (*(sptr - wpls5)) &
                    (*(sptr - wpls4)) &
                    (*(sptr - wpls3)) &
                    (*(sptr - wpls2)) &
                    (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls)) &
                    (*(sptr + wpls2)) &
                    (*(sptr + wpls3)) &
                    (*(sptr + wpls4)) &
                    (*(sptr + wpls5)) &
                    (*(sptr + wpls6)) &
                    (*(sptr + wpls7)) &
                    (*(sptr + wpls8)) &
                    (*(sptr + wpls9));
        }
    }
}

static void
fdilate_1_40(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7, wpls8;
l_int32             wpls9, wpls10;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    wpls8 = 8 * wpls;
    wpls9 = 9 * wpls;
    wpls10 = 10 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls10)) |
                    (*(sptr + wpls9)) |
                    (*(sptr + wpls8)) |
                    (*(sptr + wpls7)) |
                    (*(sptr + wpls6)) |
                    (*(sptr + wpls5)) |
                    (*(sptr + wpls4)) |
                    (*(sptr + wpls3)) |
                    (*(sptr + wpls2)) |
                    (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls)) |
                    (*(sptr - wpls2)) |
                    (*(sptr - wpls3)) |
                    (*(sptr - wpls4)) |
                    (*(sptr - wpls5)) |
                    (*(sptr - wpls6)) |
                    (*(sptr - wpls7)) |
                    (*(sptr - wpls8)) |
                    (*(sptr - wpls9)) |
                    (*(sptr - wpls10));
        }
    }
}

static void
ferode_1_40(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7, wpls8;
l_int32             wpls9, wpls10;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    wpls8 = 8 * wpls;
    wpls9 = 9 * wpls;
    wpls10 = 10 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls10)) &
                    (*(sptr - wpls9)) &
                    (*(sptr - wpls8)) &
                    (*(sptr - wpls7)) &
                    (*(sptr - wpls6)) &
                    (*(sptr - wpls5)) &
                    (*(sptr - wpls4)) &
                    (*(sptr - wpls3)) &
                    (*(sptr - wpls2)) &
                    (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls)) &
                    (*(sptr + wpls2)) &
                    (*(sptr + wpls3)) &
                    (*(sptr + wpls4)) &
                    (*(sptr + wpls5)) &
                    (*(sptr + wpls6)) &
                    (*(sptr + wpls7)) &
                    (*(sptr + wpls8)) &
                    (*(sptr + wpls9)) &
                    (*(sptr + wpls10));
        }
    }
}

static void
fdilate_1_41(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7, wpls8;
l_int32             wpls9, wpls10, wpls11, wpls12;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    wpls8 = 8 * wpls;
    wpls9 = 9 * wpls;
    wpls10 = 10 * wpls;
    wpls11 = 11 * wpls;
    wpls12 = 12 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls12)) |
                    (*(sptr + wpls11)) |
                    (*(sptr + wpls10)) |
                    (*(sptr + wpls9)) |
                    (*(sptr + wpls8)) |
                    (*(sptr + wpls7)) |
                    (*(sptr + wpls6)) |
                    (*(sptr + wpls5)) |
                    (*(sptr + wpls4)) |
                    (*(sptr + wpls3)) |
                    (*(sptr + wpls2)) |
                    (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls)) |
                    (*(sptr - wpls2)) |
                    (*(sptr - wpls3)) |
                    (*(sptr - wpls4)) |
                    (*(sptr - wpls5)) |
                    (*(sptr - wpls6)) |
                    (*(sptr - wpls7)) |
                    (*(sptr - wpls8)) |
                    (*(sptr - wpls9)) |
                    (*(sptr - wpls10)) |
                    (*(sptr - wpls11)) |
                    (*(sptr - wpls12));
        }
    }
}

static void
ferode_1_41(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7, wpls8;
l_int32             wpls9, wpls10, wpls11, wpls12;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    wpls8 = 8 * wpls;
    wpls9 = 9 * wpls;
    wpls10 = 10 * wpls;
    wpls11 = 11 * wpls;
    wpls12 = 12 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls12)) &
                    (*(sptr - wpls11)) &
                    (*(sptr - wpls10)) &
                    (*(sptr - wpls9)) &
                    (*(sptr - wpls8)) &
                    (*(sptr - wpls7)) &
                    (*(sptr - wpls6)) &
                    (*(sptr - wpls5)) &
                    (*(sptr - wpls4)) &
                    (*(sptr - wpls3)) &
                    (*(sptr - wpls2)) &
                    (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls)) &
                    (*(sptr + wpls2)) &
                    (*(sptr + wpls3)) &
                    (*(sptr + wpls4)) &
                    (*(sptr + wpls5)) &
                    (*(sptr + wpls6)) &
                    (*(sptr + wpls7)) &
                    (*(sptr + wpls8)) &
                    (*(sptr + wpls9)) &
                    (*(sptr + wpls10)) &
                    (*(sptr + wpls11)) &
                    (*(sptr + wpls12));
        }
    }
}

static void
fdilate_1_42(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7, wpls8;
l_int32             wpls9, wpls10, wpls11, wpls12;
l_int32             wpls13, wpls14, wpls15;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    wpls8 = 8 * wpls;
    wpls9 = 9 * wpls;
    wpls10 = 10 * wpls;
    wpls11 = 11 * wpls;
    wpls12 = 12 * wpls;
    wpls13 = 13 * wpls;
    wpls14 = 14 * wpls;
    wpls15 = 15 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls15)) |
                    (*(sptr + wpls14)) |
                    (*(sptr + wpls13)) |
                    (*(sptr + wpls12)) |
                    (*(sptr + wpls11)) |
                    (*(sptr + wpls10)) |
                    (*(sptr + wpls9)) |
                    (*(sptr + wpls8)) |
                    (*(sptr + wpls7)) |
                    (*(sptr + wpls6)) |
                    (*(sptr + wpls5)) |
                    (*(sptr + wpls4)) |
                    (*(sptr + wpls3)) |
                    (*(sptr + wpls2)) |
                    (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls)) |
                    (*(sptr - wpls2)) |
                    (*(sptr - wpls3)) |
                    (*(sptr - wpls4)) |
                    (*(sptr - wpls5)) |
                    (*(sptr - wpls6)) |
                    (*(sptr - wpls7)) |
                    (*(sptr - wpls8)) |
                    (*(sptr - wpls9)) |
                    (*(sptr - wpls10)) |
                    (*(sptr - wpls11)) |
                    (*(sptr - wpls12)) |
                    (*(sptr - wpls13)) |
                    (*(sptr - wpls14));
        }
    }
}

static void
ferode_1_42(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7, wpls8;
l_int32             wpls9, wpls10, wpls11, wpls12;
l_int32             wpls13, wpls14, wpls15;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    wpls8 = 8 * wpls;
    wpls9 = 9 * wpls;
    wpls10 = 10 * wpls;
    wpls11 = 11 * wpls;
    wpls12 = 12 * wpls;
    wpls13 = 13 * wpls;
    wpls14 = 14 * wpls;
    wpls15 = 15 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls15)) &
                    (*(sptr - wpls14)) &
                    (*(sptr - wpls13)) &
                    (*(sptr - wpls12)) &
                    (*(sptr - wpls11)) &
                    (*(sptr - wpls10)) &
                    (*(sptr - wpls9)) &
                    (*(sptr - wpls8)) &
                    (*(sptr - wpls7)) &
                    (*(sptr - wpls6)) &
                    (*(sptr - wpls5)) &
                    (*(sptr - wpls4)) &
                    (*(sptr - wpls3)) &
                    (*(sptr - wpls2)) &
                    (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls)) &
                    (*(sptr + wpls2)) &
                    (*(sptr + wpls3)) &
                    (*(sptr + wpls4)) &
                    (*(sptr + wpls5)) &
                    (*(sptr + wpls6)) &
                    (*(sptr + wpls7)) &
                    (*(sptr + wpls8)) &
                    (*(sptr + wpls9)) &
                    (*(sptr + wpls10)) &
                    (*(sptr + wpls11)) &
                    (*(sptr + wpls12)) &
                    (*(sptr + wpls13)) &
                    (*(sptr + wpls14));
        }
    }
}

static void
fdilate_1_43(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7, wpls8;
l_int32             wpls9, wpls10, wpls11, wpls12;
l_int32             wpls13, wpls14, wpls15;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    wpls8 = 8 * wpls;
    wpls9 = 9 * wpls;
    wpls10 = 10 * wpls;
    wpls11 = 11 * wpls;
    wpls12 = 12 * wpls;
    wpls13 = 13 * wpls;
    wpls14 = 14 * wpls;
    wpls15 = 15 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls15)) |
                    (*(sptr + wpls14)) |
                    (*(sptr + wpls13)) |
                    (*(sptr + wpls12)) |
                    (*(sptr + wpls11)) |
                    (*(sptr + wpls10)) |
                    (*(sptr + wpls9)) |
                    (*(sptr + wpls8)) |
                    (*(sptr + wpls7)) |
                    (*(sptr + wpls6)) |
                    (*(sptr + wpls5)) |
                    (*(sptr + wpls4)) |
                    (*(sptr + wpls3)) |
                    (*(sptr + wpls2)) |
                    (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls)) |
                    (*(sptr - wpls2)) |
                    (*(sptr - wpls3)) |
                    (*(sptr - wpls4)) |
                    (*(sptr - wpls5)) |
                    (*(sptr - wpls6)) |
                    (*(sptr - wpls7)) |
                    (*(sptr - wpls8)) |
                    (*(sptr - wpls9)) |
                    (*(sptr - wpls10)) |
                    (*(sptr - wpls11)) |
                    (*(sptr - wpls12)) |
                    (*(sptr - wpls13)) |
                    (*(sptr - wpls14)) |
                    (*(sptr - wpls15));
        }
    }
}

static void
ferode_1_43(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7, wpls8;
l_int32             wpls9, wpls10, wpls11, wpls12;
l_int32             wpls13, wpls14, wpls15;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    wpls8 = 8 * wpls;
    wpls9 = 9 * wpls;
    wpls10 = 10 * wpls;
    wpls11 = 11 * wpls;
    wpls12 = 12 * wpls;
    wpls13 = 13 * wpls;
    wpls14 = 14 * wpls;
    wpls15 = 15 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls15)) &
                    (*(sptr - wpls14)) &
                    (*(sptr - wpls13)) &
                    (*(sptr - wpls12)) &
                    (*(sptr - wpls11)) &
                    (*(sptr - wpls10)) &
                    (*(sptr - wpls9)) &
                    (*(sptr - wpls8)) &
                    (*(sptr - wpls7)) &
                    (*(sptr - wpls6)) &
                    (*(sptr - wpls5)) &
                    (*(sptr - wpls4)) &
                    (*(sptr - wpls3)) &
                    (*(sptr - wpls2)) &
                    (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls)) &
                    (*(sptr + wpls2)) &
                    (*(sptr + wpls3)) &
                    (*(sptr + wpls4)) &
                    (*(sptr + wpls5)) &
                    (*(sptr + wpls6)) &
                    (*(sptr + wpls7)) &
                    (*(sptr + wpls8)) &
                    (*(sptr + wpls9)) &
                    (*(sptr + wpls10)) &
                    (*(sptr + wpls11)) &
                    (*(sptr + wpls12)) &
                    (*(sptr + wpls13)) &
                    (*(sptr + wpls14)) &
                    (*(sptr + wpls15));
        }
    }
}

static void
fdilate_1_44(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7, wpls8;
l_int32             wpls9, wpls10, wpls11, wpls12;
l_int32             wpls13, wpls14, wpls15, wpls16;
l_int32             wpls17;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    wpls8 = 8 * wpls;
    wpls9 = 9 * wpls;
    wpls10 = 10 * wpls;
    wpls11 = 11 * wpls;
    wpls12 = 12 * wpls;
    wpls13 = 13 * wpls;
    wpls14 = 14 * wpls;
    wpls15 = 15 * wpls;
    wpls16 = 16 * wpls;
    wpls17 = 17 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls17)) |
                    (*(sptr + wpls16)) |
                    (*(sptr + wpls15)) |
                    (*(sptr + wpls14)) |
                    (*(sptr + wpls13)) |
                    (*(sptr + wpls12)) |
                    (*(sptr + wpls11)) |
                    (*(sptr + wpls10)) |
                    (*(sptr + wpls9)) |
                    (*(sptr + wpls8)) |
                    (*(sptr + wpls7)) |
                    (*(sptr + wpls6)) |
                    (*(sptr + wpls5)) |
                    (*(sptr + wpls4)) |
                    (*(sptr + wpls3)) |
                    (*(sptr + wpls2)) |
                    (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls)) |
                    (*(sptr - wpls2)) |
                    (*(sptr - wpls3)) |
                    (*(sptr - wpls4)) |
                    (*(sptr - wpls5)) |
                    (*(sptr - wpls6)) |
                    (*(sptr - wpls7)) |
                    (*(sptr - wpls8)) |
                    (*(sptr - wpls9)) |
                    (*(sptr - wpls10)) |
                    (*(sptr - wpls11)) |
                    (*(sptr - wpls12)) |
                    (*(sptr - wpls13)) |
                    (*(sptr - wpls14)) |
                    (*(sptr - wpls15)) |
                    (*(sptr - wpls16)) |
                    (*(sptr - wpls17));
        }
    }
}

static void
ferode_1_44(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7, wpls8;
l_int32             wpls9, wpls10, wpls11, wpls12;
l_int32             wpls13, wpls14, wpls15, wpls16;
l_int32             wpls17;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    wpls8 = 8 * wpls;
    wpls9 = 9 * wpls;
    wpls10 = 10 * wpls;
    wpls11 = 11 * wpls;
    wpls12 = 12 * wpls;
    wpls13 = 13 * wpls;
    wpls14 = 14 * wpls;
    wpls15 = 15 * wpls;
    wpls16 = 16 * wpls;
    wpls17 = 17 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls17)) &
                    (*(sptr - wpls16)) &
                    (*(sptr - wpls15)) &
                    (*(sptr - wpls14)) &
                    (*(sptr - wpls13)) &
                    (*(sptr - wpls12)) &
                    (*(sptr - wpls11)) &
                    (*(sptr - wpls10)) &
                    (*(sptr - wpls9)) &
                    (*(sptr - wpls8)) &
                    (*(sptr - wpls7)) &
                    (*(sptr - wpls6)) &
                    (*(sptr - wpls5)) &
                    (*(sptr - wpls4)) &
                    (*(sptr - wpls3)) &
                    (*(sptr - wpls2)) &
                    (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls)) &
                    (*(sptr + wpls2)) &
                    (*(sptr + wpls3)) &
                    (*(sptr + wpls4)) &
                    (*(sptr + wpls5)) &
                    (*(sptr + wpls6)) &
                    (*(sptr + wpls7)) &
                    (*(sptr + wpls8)) &
                    (*(sptr + wpls9)) &
                    (*(sptr + wpls10)) &
                    (*(sptr + wpls11)) &
                    (*(sptr + wpls12)) &
                    (*(sptr + wpls13)) &
                    (*(sptr + wpls14)) &
                    (*(sptr + wpls15)) &
                    (*(sptr + wpls16)) &
                    (*(sptr + wpls17));
        }
    }
}

static void
fdilate_1_45(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7, wpls8;
l_int32             wpls9, wpls10, wpls11, wpls12;
l_int32             wpls13, wpls14, wpls15, wpls16;
l_int32             wpls17, wpls18, wpls19, wpls20;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    wpls8 = 8 * wpls;
    wpls9 = 9 * wpls;
    wpls10 = 10 * wpls;
    wpls11 = 11 * wpls;
    wpls12 = 12 * wpls;
    wpls13 = 13 * wpls;
    wpls14 = 14 * wpls;
    wpls15 = 15 * wpls;
    wpls16 = 16 * wpls;
    wpls17 = 17 * wpls;
    wpls18 = 18 * wpls;
    wpls19 = 19 * wpls;
    wpls20 = 20 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls20)) |
                    (*(sptr + wpls19)) |
                    (*(sptr + wpls18)) |
                    (*(sptr + wpls17)) |
                    (*(sptr + wpls16)) |
                    (*(sptr + wpls15)) |
                    (*(sptr + wpls14)) |
                    (*(sptr + wpls13)) |
                    (*(sptr + wpls12)) |
                    (*(sptr + wpls11)) |
                    (*(sptr + wpls10)) |
                    (*(sptr + wpls9)) |
                    (*(sptr + wpls8)) |
                    (*(sptr + wpls7)) |
                    (*(sptr + wpls6)) |
                    (*(sptr + wpls5)) |
                    (*(sptr + wpls4)) |
                    (*(sptr + wpls3)) |
                    (*(sptr + wpls2)) |
                    (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls)) |
                    (*(sptr - wpls2)) |
                    (*(sptr - wpls3)) |
                    (*(sptr - wpls4)) |
                    (*(sptr - wpls5)) |
                    (*(sptr - wpls6)) |
                    (*(sptr - wpls7)) |
                    (*(sptr - wpls8)) |
                    (*(sptr - wpls9)) |
                    (*(sptr - wpls10)) |
                    (*(sptr - wpls11)) |
                    (*(sptr - wpls12)) |
                    (*(sptr - wpls13)) |
                    (*(sptr - wpls14)) |
                    (*(sptr - wpls15)) |
                    (*(sptr - wpls16)) |
                    (*(sptr - wpls17)) |
                    (*(sptr - wpls18)) |
                    (*(sptr - wpls19));
        }
    }
}

static void
ferode_1_45(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7, wpls8;
l_int32             wpls9, wpls10, wpls11, wpls12;
l_int32             wpls13, wpls14, wpls15, wpls16;
l_int32             wpls17, wpls18, wpls19, wpls20;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    wpls8 = 8 * wpls;
    wpls9 = 9 * wpls;
    wpls10 = 10 * wpls;
    wpls11 = 11 * wpls;
    wpls12 = 12 * wpls;
    wpls13 = 13 * wpls;
    wpls14 = 14 * wpls;
    wpls15 = 15 * wpls;
    wpls16 = 16 * wpls;
    wpls17 = 17 * wpls;
    wpls18 = 18 * wpls;
    wpls19 = 19 * wpls;
    wpls20 = 20 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls20)) &
                    (*(sptr - wpls19)) &
                    (*(sptr - wpls18)) &
                    (*(sptr - wpls17)) &
                    (*(sptr - wpls16)) &
                    (*(sptr - wpls15)) &
                    (*(sptr - wpls14)) &
                    (*(sptr - wpls13)) &
                    (*(sptr - wpls12)) &
                    (*(sptr - wpls11)) &
                    (*(sptr - wpls10)) &
                    (*(sptr - wpls9)) &
                    (*(sptr - wpls8)) &
                    (*(sptr - wpls7)) &
                    (*(sptr - wpls6)) &
                    (*(sptr - wpls5)) &
                    (*(sptr - wpls4)) &
                    (*(sptr - wpls3)) &
                    (*(sptr - wpls2)) &
                    (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls)) &
                    (*(sptr + wpls2)) &
                    (*(sptr + wpls3)) &
                    (*(sptr + wpls4)) &
                    (*(sptr + wpls5)) &
                    (*(sptr + wpls6)) &
                    (*(sptr + wpls7)) &
                    (*(sptr + wpls8)) &
                    (*(sptr + wpls9)) &
                    (*(sptr + wpls10)) &
                    (*(sptr + wpls11)) &
                    (*(sptr + wpls12)) &
                    (*(sptr + wpls13)) &
                    (*(sptr + wpls14)) &
                    (*(sptr + wpls15)) &
                    (*(sptr + wpls16)) &
                    (*(sptr + wpls17)) &
                    (*(sptr + wpls18)) &
                    (*(sptr + wpls19));
        }
    }
}

static void
fdilate_1_46(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7, wpls8;
l_int32             wpls9, wpls10, wpls11, wpls12;
l_int32             wpls13, wpls14, wpls15, wpls16;
l_int32             wpls17, wpls18, wpls19, wpls20;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    wpls8 = 8 * wpls;
    wpls9 = 9 * wpls;
    wpls10 = 10 * wpls;
    wpls11 = 11 * wpls;
    wpls12 = 12 * wpls;
    wpls13 = 13 * wpls;
    wpls14 = 14 * wpls;
    wpls15 = 15 * wpls;
    wpls16 = 16 * wpls;
    wpls17 = 17 * wpls;
    wpls18 = 18 * wpls;
    wpls19 = 19 * wpls;
    wpls20 = 20 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls20)) |
                    (*(sptr + wpls19)) |
                    (*(sptr + wpls18)) |
                    (*(sptr + wpls17)) |
                    (*(sptr + wpls16)) |
                    (*(sptr + wpls15)) |
                    (*(sptr + wpls14)) |
                    (*(sptr + wpls13)) |
                    (*(sptr + wpls12)) |
                    (*(sptr + wpls11)) |
                    (*(sptr + wpls10)) |
                    (*(sptr + wpls9)) |
                    (*(sptr + wpls8)) |
                    (*(sptr + wpls7)) |
                    (*(sptr + wpls6)) |
                    (*(sptr + wpls5)) |
                    (*(sptr + wpls4)) |
                    (*(sptr + wpls3)) |
                    (*(sptr + wpls2)) |
                    (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls)) |
                    (*(sptr - wpls2)) |
                    (*(sptr - wpls3)) |
                    (*(sptr - wpls4)) |
                    (*(sptr - wpls5)) |
                    (*(sptr - wpls6)) |
                    (*(sptr - wpls7)) |
                    (*(sptr - wpls8)) |
                    (*(sptr - wpls9)) |
                    (*(sptr - wpls10)) |
                    (*(sptr - wpls11)) |
                    (*(sptr - wpls12)) |
                    (*(sptr - wpls13)) |
                    (*(sptr - wpls14)) |
                    (*(sptr - wpls15)) |
                    (*(sptr - wpls16)) |
                    (*(sptr - wpls17)) |
                    (*(sptr - wpls18)) |
                    (*(sptr - wpls19)) |
                    (*(sptr - wpls20));
        }
    }
}

static void
ferode_1_46(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7, wpls8;
l_int32             wpls9, wpls10, wpls11, wpls12;
l_int32             wpls13, wpls14, wpls15, wpls16;
l_int32             wpls17, wpls18, wpls19, wpls20;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    wpls8 = 8 * wpls;
    wpls9 = 9 * wpls;
    wpls10 = 10 * wpls;
    wpls11 = 11 * wpls;
    wpls12 = 12 * wpls;
    wpls13 = 13 * wpls;
    wpls14 = 14 * wpls;
    wpls15 = 15 * wpls;
    wpls16 = 16 * wpls;
    wpls17 = 17 * wpls;
    wpls18 = 18 * wpls;
    wpls19 = 19 * wpls;
    wpls20 = 20 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls20)) &
                    (*(sptr - wpls19)) &
                    (*(sptr - wpls18)) &
                    (*(sptr - wpls17)) &
                    (*(sptr - wpls16)) &
                    (*(sptr - wpls15)) &
                    (*(sptr - wpls14)) &
                    (*(sptr - wpls13)) &
                    (*(sptr - wpls12)) &
                    (*(sptr - wpls11)) &
                    (*(sptr - wpls10)) &
                    (*(sptr - wpls9)) &
                    (*(sptr - wpls8)) &
                    (*(sptr - wpls7)) &
                    (*(sptr - wpls6)) &
                    (*(sptr - wpls5)) &
                    (*(sptr - wpls4)) &
                    (*(sptr - wpls3)) &
                    (*(sptr - wpls2)) &
                    (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls)) &
                    (*(sptr + wpls2)) &
                    (*(sptr + wpls3)) &
                    (*(sptr + wpls4)) &
                    (*(sptr + wpls5)) &
                    (*(sptr + wpls6)) &
                    (*(sptr + wpls7)) &
                    (*(sptr + wpls8)) &
                    (*(sptr + wpls9)) &
                    (*(sptr + wpls10)) &
                    (*(sptr + wpls11)) &
                    (*(sptr + wpls12)) &
                    (*(sptr + wpls13)) &
                    (*(sptr + wpls14)) &
                    (*(sptr + wpls15)) &
                    (*(sptr + wpls16)) &
                    (*(sptr + wpls17)) &
                    (*(sptr + wpls18)) &
                    (*(sptr + wpls19)) &
                    (*(sptr + wpls20));
        }
    }
}

static void
fdilate_1_47(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7, wpls8;
l_int32             wpls9, wpls10, wpls11, wpls12;
l_int32             wpls13, wpls14, wpls15, wpls16;
l_int32             wpls17, wpls18, wpls19, wpls20;
l_int32             wpls21, wpls22;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    wpls8 = 8 * wpls;
    wpls9 = 9 * wpls;
    wpls10 = 10 * wpls;
    wpls11 = 11 * wpls;
    wpls12 = 12 * wpls;
    wpls13 = 13 * wpls;
    wpls14 = 14 * wpls;
    wpls15 = 15 * wpls;
    wpls16 = 16 * wpls;
    wpls17 = 17 * wpls;
    wpls18 = 18 * wpls;
    wpls19 = 19 * wpls;
    wpls20 = 20 * wpls;
    wpls21 = 21 * wpls;
    wpls22 = 22 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls22)) |
                    (*(sptr + wpls21)) |
                    (*(sptr + wpls20)) |
                    (*(sptr + wpls19)) |
                    (*(sptr + wpls18)) |
                    (*(sptr + wpls17)) |
                    (*(sptr + wpls16)) |
                    (*(sptr + wpls15)) |
                    (*(sptr + wpls14)) |
                    (*(sptr + wpls13)) |
                    (*(sptr + wpls12)) |
                    (*(sptr + wpls11)) |
                    (*(sptr + wpls10)) |
                    (*(sptr + wpls9)) |
                    (*(sptr + wpls8)) |
                    (*(sptr + wpls7)) |
                    (*(sptr + wpls6)) |
                    (*(sptr + wpls5)) |
                    (*(sptr + wpls4)) |
                    (*(sptr + wpls3)) |
                    (*(sptr + wpls2)) |
                    (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls)) |
                    (*(sptr - wpls2)) |
                    (*(sptr - wpls3)) |
                    (*(sptr - wpls4)) |
                    (*(sptr - wpls5)) |
                    (*(sptr - wpls6)) |
                    (*(sptr - wpls7)) |
                    (*(sptr - wpls8)) |
                    (*(sptr - wpls9)) |
                    (*(sptr - wpls10)) |
                    (*(sptr - wpls11)) |
                    (*(sptr - wpls12)) |
                    (*(sptr - wpls13)) |
                    (*(sptr - wpls14)) |
                    (*(sptr - wpls15)) |
                    (*(sptr - wpls16)) |
                    (*(sptr - wpls17)) |
                    (*(sptr - wpls18)) |
                    (*(sptr - wpls19)) |
                    (*(sptr - wpls20)) |
                    (*(sptr - wpls21)) |
                    (*(sptr - wpls22));
        }
    }
}

static void
ferode_1_47(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7, wpls8;
l_int32             wpls9, wpls10, wpls11, wpls12;
l_int32             wpls13, wpls14, wpls15, wpls16;
l_int32             wpls17, wpls18, wpls19, wpls20;
l_int32             wpls21, wpls22;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    wpls8 = 8 * wpls;
    wpls9 = 9 * wpls;
    wpls10 = 10 * wpls;
    wpls11 = 11 * wpls;
    wpls12 = 12 * wpls;
    wpls13 = 13 * wpls;
    wpls14 = 14 * wpls;
    wpls15 = 15 * wpls;
    wpls16 = 16 * wpls;
    wpls17 = 17 * wpls;
    wpls18 = 18 * wpls;
    wpls19 = 19 * wpls;
    wpls20 = 20 * wpls;
    wpls21 = 21 * wpls;
    wpls22 = 22 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls22)) &
                    (*(sptr - wpls21)) &
                    (*(sptr - wpls20)) &
                    (*(sptr - wpls19)) &
                    (*(sptr - wpls18)) &
                    (*(sptr - wpls17)) &
                    (*(sptr - wpls16)) &
                    (*(sptr - wpls15)) &
                    (*(sptr - wpls14)) &
                    (*(sptr - wpls13)) &
                    (*(sptr - wpls12)) &
                    (*(sptr - wpls11)) &
                    (*(sptr - wpls10)) &
                    (*(sptr - wpls9)) &
                    (*(sptr - wpls8)) &
                    (*(sptr - wpls7)) &
                    (*(sptr - wpls6)) &
                    (*(sptr - wpls5)) &
                    (*(sptr - wpls4)) &
                    (*(sptr - wpls3)) &
                    (*(sptr - wpls2)) &
                    (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls)) &
                    (*(sptr + wpls2)) &
                    (*(sptr + wpls3)) &
                    (*(sptr + wpls4)) &
                    (*(sptr + wpls5)) &
                    (*(sptr + wpls6)) &
                    (*(sptr + wpls7)) &
                    (*(sptr + wpls8)) &
                    (*(sptr + wpls9)) &
                    (*(sptr + wpls10)) &
                    (*(sptr + wpls11)) &
                    (*(sptr + wpls12)) &
                    (*(sptr + wpls13)) &
                    (*(sptr + wpls14)) &
                    (*(sptr + wpls15)) &
                    (*(sptr + wpls16)) &
                    (*(sptr + wpls17)) &
                    (*(sptr + wpls18)) &
                    (*(sptr + wpls19)) &
                    (*(sptr + wpls20)) &
                    (*(sptr + wpls21)) &
                    (*(sptr + wpls22));
        }
    }
}

static void
fdilate_1_48(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7, wpls8;
l_int32             wpls9, wpls10, wpls11, wpls12;
l_int32             wpls13, wpls14, wpls15, wpls16;
l_int32             wpls17, wpls18, wpls19, wpls20;
l_int32             wpls21, wpls22, wpls23, wpls24;
l_int32             wpls25;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    wpls8 = 8 * wpls;
    wpls9 = 9 * wpls;
    wpls10 = 10 * wpls;
    wpls11 = 11 * wpls;
    wpls12 = 12 * wpls;
    wpls13 = 13 * wpls;
    wpls14 = 14 * wpls;
    wpls15 = 15 * wpls;
    wpls16 = 16 * wpls;
    wpls17 = 17 * wpls;
    wpls18 = 18 * wpls;
    wpls19 = 19 * wpls;
    wpls20 = 20 * wpls;
    wpls21 = 21 * wpls;
    wpls22 = 22 * wpls;
    wpls23 = 23 * wpls;
    wpls24 = 24 * wpls;
    wpls25 = 25 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls25)) |
                    (*(sptr + wpls24)) |
                    (*(sptr + wpls23)) |
                    (*(sptr + wpls22)) |
                    (*(sptr + wpls21)) |
                    (*(sptr + wpls20)) |
                    (*(sptr + wpls19)) |
                    (*(sptr + wpls18)) |
                    (*(sptr + wpls17)) |
                    (*(sptr + wpls16)) |
                    (*(sptr + wpls15)) |
                    (*(sptr + wpls14)) |
                    (*(sptr + wpls13)) |
                    (*(sptr + wpls12)) |
                    (*(sptr + wpls11)) |
                    (*(sptr + wpls10)) |
                    (*(sptr + wpls9)) |
                    (*(sptr + wpls8)) |
                    (*(sptr + wpls7)) |
                    (*(sptr + wpls6)) |
                    (*(sptr + wpls5)) |
                    (*(sptr + wpls4)) |
                    (*(sptr + wpls3)) |
                    (*(sptr + wpls2)) |
                    (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls)) |
                    (*(sptr - wpls2)) |
                    (*(sptr - wpls3)) |
                    (*(sptr - wpls4)) |
                    (*(sptr - wpls5)) |
                    (*(sptr - wpls6)) |
                    (*(sptr - wpls7)) |
                    (*(sptr - wpls8)) |
                    (*(sptr - wpls9)) |
                    (*(sptr - wpls10)) |
                    (*(sptr - wpls11)) |
                    (*(sptr - wpls12)) |
                    (*(sptr - wpls13)) |
                    (*(sptr - wpls14)) |
                    (*(sptr - wpls15)) |
                    (*(sptr - wpls16)) |
                    (*(sptr - wpls17)) |
                    (*(sptr - wpls18)) |
                    (*(sptr - wpls19)) |
                    (*(sptr - wpls20)) |
                    (*(sptr - wpls21)) |
                    (*(sptr - wpls22)) |
                    (*(sptr - wpls23)) |
                    (*(sptr - wpls24));
        }
    }
}

static void
ferode_1_48(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7, wpls8;
l_int32             wpls9, wpls10, wpls11, wpls12;
l_int32             wpls13, wpls14, wpls15, wpls16;
l_int32             wpls17, wpls18, wpls19, wpls20;
l_int32             wpls21, wpls22, wpls23, wpls24;
l_int32             wpls25;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    wpls8 = 8 * wpls;
    wpls9 = 9 * wpls;
    wpls10 = 10 * wpls;
    wpls11 = 11 * wpls;
    wpls12 = 12 * wpls;
    wpls13 = 13 * wpls;
    wpls14 = 14 * wpls;
    wpls15 = 15 * wpls;
    wpls16 = 16 * wpls;
    wpls17 = 17 * wpls;
    wpls18 = 18 * wpls;
    wpls19 = 19 * wpls;
    wpls20 = 20 * wpls;
    wpls21 = 21 * wpls;
    wpls22 = 22 * wpls;
    wpls23 = 23 * wpls;
    wpls24 = 24 * wpls;
    wpls25 = 25 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls25)) &
                    (*(sptr - wpls24)) &
                    (*(sptr - wpls23)) &
                    (*(sptr - wpls22)) &
                    (*(sptr - wpls21)) &
                    (*(sptr - wpls20)) &
                    (*(sptr - wpls19)) &
                    (*(sptr - wpls18)) &
                    (*(sptr - wpls17)) &
                    (*(sptr - wpls16)) &
                    (*(sptr - wpls15)) &
                    (*(sptr - wpls14)) &
                    (*(sptr - wpls13)) &
                    (*(sptr - wpls12)) &
                    (*(sptr - wpls11)) &
                    (*(sptr - wpls10)) &
                    (*(sptr - wpls9)) &
                    (*(sptr - wpls8)) &
                    (*(sptr - wpls7)) &
                    (*(sptr - wpls6)) &
                    (*(sptr - wpls5)) &
                    (*(sptr - wpls4)) &
                    (*(sptr - wpls3)) &
                    (*(sptr - wpls2)) &
                    (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls)) &
                    (*(sptr + wpls2)) &
                    (*(sptr + wpls3)) &
                    (*(sptr + wpls4)) &
                    (*(sptr + wpls5)) &
                    (*(sptr + wpls6)) &
                    (*(sptr + wpls7)) &
                    (*(sptr + wpls8)) &
                    (*(sptr + wpls9)) &
                    (*(sptr + wpls10)) &
                    (*(sptr + wpls11)) &
                    (*(sptr + wpls12)) &
                    (*(sptr + wpls13)) &
                    (*(sptr + wpls14)) &
                    (*(sptr + wpls15)) &
                    (*(sptr + wpls16)) &
                    (*(sptr + wpls17)) &
                    (*(sptr + wpls18)) &
                    (*(sptr + wpls19)) &
                    (*(sptr + wpls20)) &
                    (*(sptr + wpls21)) &
                    (*(sptr + wpls22)) &
                    (*(sptr + wpls23)) &
                    (*(sptr + wpls24));
        }
    }
}

static void
fdilate_1_49(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7, wpls8;
l_int32             wpls9, wpls10, wpls11, wpls12;
l_int32             wpls13, wpls14, wpls15, wpls16;
l_int32             wpls17, wpls18, wpls19, wpls20;
l_int32             wpls21, wpls22, wpls23, wpls24;
l_int32             wpls25;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    wpls8 = 8 * wpls;
    wpls9 = 9 * wpls;
    wpls10 = 10 * wpls;
    wpls11 = 11 * wpls;
    wpls12 = 12 * wpls;
    wpls13 = 13 * wpls;
    wpls14 = 14 * wpls;
    wpls15 = 15 * wpls;
    wpls16 = 16 * wpls;
    wpls17 = 17 * wpls;
    wpls18 = 18 * wpls;
    wpls19 = 19 * wpls;
    wpls20 = 20 * wpls;
    wpls21 = 21 * wpls;
    wpls22 = 22 * wpls;
    wpls23 = 23 * wpls;
    wpls24 = 24 * wpls;
    wpls25 = 25 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr + wpls25)) |
                    (*(sptr + wpls24)) |
                    (*(sptr + wpls23)) |
                    (*(sptr + wpls22)) |
                    (*(sptr + wpls21)) |
                    (*(sptr + wpls20)) |
                    (*(sptr + wpls19)) |
                    (*(sptr + wpls18)) |
                    (*(sptr + wpls17)) |
                    (*(sptr + wpls16)) |
                    (*(sptr + wpls15)) |
                    (*(sptr + wpls14)) |
                    (*(sptr + wpls13)) |
                    (*(sptr + wpls12)) |
                    (*(sptr + wpls11)) |
                    (*(sptr + wpls10)) |
                    (*(sptr + wpls9)) |
                    (*(sptr + wpls8)) |
                    (*(sptr + wpls7)) |
                    (*(sptr + wpls6)) |
                    (*(sptr + wpls5)) |
                    (*(sptr + wpls4)) |
                    (*(sptr + wpls3)) |
                    (*(sptr + wpls2)) |
                    (*(sptr + wpls)) |
                    (*sptr) |
                    (*(sptr - wpls)) |
                    (*(sptr - wpls2)) |
                    (*(sptr - wpls3)) |
                    (*(sptr - wpls4)) |
                    (*(sptr - wpls5)) |
                    (*(sptr - wpls6)) |
                    (*(sptr - wpls7)) |
                    (*(sptr - wpls8)) |
                    (*(sptr - wpls9)) |
                    (*(sptr - wpls10)) |
                    (*(sptr - wpls11)) |
                    (*(sptr - wpls12)) |
                    (*(sptr - wpls13)) |
                    (*(sptr - wpls14)) |
                    (*(sptr - wpls15)) |
                    (*(sptr - wpls16)) |
                    (*(sptr - wpls17)) |
                    (*(sptr - wpls18)) |
                    (*(sptr - wpls19)) |
                    (*(sptr - wpls20)) |
                    (*(sptr - wpls21)) |
                    (*(sptr - wpls22)) |
                    (*(sptr - wpls23)) |
                    (*(sptr - wpls24)) |
                    (*(sptr - wpls25));
        }
    }
}

static void
ferode_1_49(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6, wpls7, wpls8;
l_int32             wpls9, wpls10, wpls11, wpls12;
l_int32             wpls13, wpls14, wpls15, wpls16;
l_int32             wpls17, wpls18, wpls19, wpls20;
l_int32             wpls21, wpls22, wpls23, wpls24;
l_int32             wpls25;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    wpls7 = 7 * wpls;
    wpls8 = 8 * wpls;
    wpls9 = 9 * wpls;
    wpls10 = 10 * wpls;
    wpls11 = 11 * wpls;
    wpls12 = 12 * wpls;
    wpls13 = 13 * wpls;
    wpls14 = 14 * wpls;
    wpls15 = 15 * wpls;
    wpls16 = 16 * wpls;
    wpls17 = 17 * wpls;
    wpls18 = 18 * wpls;
    wpls19 = 19 * wpls;
    wpls20 = 20 * wpls;
    wpls21 = 21 * wpls;
    wpls22 = 22 * wpls;
    wpls23 = 23 * wpls;
    wpls24 = 24 * wpls;
    wpls25 = 25 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls25)) &
                    (*(sptr - wpls24)) &
                    (*(sptr - wpls23)) &
                    (*(sptr - wpls22)) &
                    (*(sptr - wpls21)) &
                    (*(sptr - wpls20)) &
                    (*(sptr - wpls19)) &
                    (*(sptr - wpls18)) &
                    (*(sptr - wpls17)) &
                    (*(sptr - wpls16)) &
                    (*(sptr - wpls15)) &
                    (*(sptr - wpls14)) &
                    (*(sptr - wpls13)) &
                    (*(sptr - wpls12)) &
                    (*(sptr - wpls11)) &
                    (*(sptr - wpls10)) &
                    (*(sptr - wpls9)) &
                    (*(sptr - wpls8)) &
                    (*(sptr - wpls7)) &
                    (*(sptr - wpls6)) &
                    (*(sptr - wpls5)) &
                    (*(sptr - wpls4)) &
                    (*(sptr - wpls3)) &
                    (*(sptr - wpls2)) &
                    (*(sptr - wpls)) &
                    (*sptr) &
                    (*(sptr + wpls)) &
                    (*(sptr + wpls2)) &
                    (*(sptr + wpls3)) &
                    (*(sptr + wpls4)) &
                    (*(sptr + wpls5)) &
                    (*(sptr + wpls6)) &
                    (*(sptr + wpls7)) &
                    (*(sptr + wpls8)) &
                    (*(sptr + wpls9)) &
                    (*(sptr + wpls10)) &
                    (*(sptr + wpls11)) &
                    (*(sptr + wpls12)) &
                    (*(sptr + wpls13)) &
                    (*(sptr + wpls14)) &
                    (*(sptr + wpls15)) &
                    (*(sptr + wpls16)) &
                    (*(sptr + wpls17)) &
                    (*(sptr + wpls18)) &
                    (*(sptr + wpls19)) &
                    (*(sptr + wpls20)) &
                    (*(sptr + wpls21)) &
                    (*(sptr + wpls22)) &
                    (*(sptr + wpls23)) &
                    (*(sptr + wpls24)) &
                    (*(sptr + wpls25));
        }
    }
}

static void
fdilate_1_50(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr + wpls) << 1) | (*(sptr + wpls + 1) >> 31)) |
                    (*(sptr + wpls)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr);
        }
    }
}

static void
ferode_1_50(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr - wpls) >> 1) | (*(sptr - wpls - 1) << 31)) &
                    (*(sptr - wpls)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr);
        }
    }
}

static void
fdilate_1_51(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr + wpls) << 1) | (*(sptr + wpls + 1) >> 31)) |
                    (*(sptr + wpls)) |
                    ((*(sptr + wpls) >> 1) | (*(sptr + wpls - 1) << 31)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr - wpls) << 1) | (*(sptr - wpls + 1) >> 31)) |
                    (*(sptr - wpls)) |
                    ((*(sptr - wpls) >> 1) | (*(sptr - wpls - 1) << 31));
        }
    }
}

static void
ferode_1_51(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr - wpls) >> 1) | (*(sptr - wpls - 1) << 31)) &
                    (*(sptr - wpls)) &
                    ((*(sptr - wpls) << 1) | (*(sptr - wpls + 1) >> 31)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr + wpls) >> 1) | (*(sptr + wpls - 1) << 31)) &
                    (*(sptr + wpls)) &
                    ((*(sptr + wpls) << 1) | (*(sptr + wpls + 1) >> 31));
        }
    }
}

static void
fdilate_1_52(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2;

    wpls2 = 2 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr + wpls2) << 2) | (*(sptr + wpls2 + 1) >> 30)) |
                    ((*(sptr + wpls2) << 1) | (*(sptr + wpls2 + 1) >> 31)) |
                    (*(sptr + wpls2)) |
                    ((*(sptr + wpls2) >> 1) | (*(sptr + wpls2 - 1) << 31)) |
                    ((*(sptr + wpls) << 2) | (*(sptr + wpls + 1) >> 30)) |
                    ((*(sptr + wpls) << 1) | (*(sptr + wpls + 1) >> 31)) |
                    (*(sptr + wpls)) |
                    ((*(sptr + wpls) >> 1) | (*(sptr + wpls - 1) << 31)) |
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr - wpls) << 2) | (*(sptr - wpls + 1) >> 30)) |
                    ((*(sptr - wpls) << 1) | (*(sptr - wpls + 1) >> 31)) |
                    (*(sptr - wpls)) |
                    ((*(sptr - wpls) >> 1) | (*(sptr - wpls - 1) << 31));
        }
    }
}

static void
ferode_1_52(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2;

    wpls2 = 2 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr - wpls2) >> 2) | (*(sptr - wpls2 - 1) << 30)) &
                    ((*(sptr - wpls2) >> 1) | (*(sptr - wpls2 - 1) << 31)) &
                    (*(sptr - wpls2)) &
                    ((*(sptr - wpls2) << 1) | (*(sptr - wpls2 + 1) >> 31)) &
                    ((*(sptr - wpls) >> 2) | (*(sptr - wpls - 1) << 30)) &
                    ((*(sptr - wpls) >> 1) | (*(sptr - wpls - 1) << 31)) &
                    (*(sptr - wpls)) &
                    ((*(sptr - wpls) << 1) | (*(sptr - wpls + 1) >> 31)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr + wpls) >> 2) | (*(sptr + wpls - 1) << 30)) &
                    ((*(sptr + wpls) >> 1) | (*(sptr + wpls - 1) << 31)) &
                    (*(sptr + wpls)) &
                    ((*(sptr + wpls) << 1) | (*(sptr + wpls + 1) >> 31));
        }
    }
}

static void
fdilate_1_53(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2;

    wpls2 = 2 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr + wpls2) << 2) | (*(sptr + wpls2 + 1) >> 30)) |
                    ((*(sptr + wpls2) << 1) | (*(sptr + wpls2 + 1) >> 31)) |
                    (*(sptr + wpls2)) |
                    ((*(sptr + wpls2) >> 1) | (*(sptr + wpls2 - 1) << 31)) |
                    ((*(sptr + wpls2) >> 2) | (*(sptr + wpls2 - 1) << 30)) |
                    ((*(sptr + wpls) << 2) | (*(sptr + wpls + 1) >> 30)) |
                    ((*(sptr + wpls) << 1) | (*(sptr + wpls + 1) >> 31)) |
                    (*(sptr + wpls)) |
                    ((*(sptr + wpls) >> 1) | (*(sptr + wpls - 1) << 31)) |
                    ((*(sptr + wpls) >> 2) | (*(sptr + wpls - 1) << 30)) |
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) |
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) |
                    ((*(sptr - wpls) << 2) | (*(sptr - wpls + 1) >> 30)) |
                    ((*(sptr - wpls) << 1) | (*(sptr - wpls + 1) >> 31)) |
                    (*(sptr - wpls)) |
                    ((*(sptr - wpls) >> 1) | (*(sptr - wpls - 1) << 31)) |
                    ((*(sptr - wpls) >> 2) | (*(sptr - wpls - 1) << 30)) |
                    ((*(sptr - wpls2) << 2) | (*(sptr - wpls2 + 1) >> 30)) |
                    ((*(sptr - wpls2) << 1) | (*(sptr - wpls2 + 1) >> 31)) |
                    (*(sptr - wpls2)) |
                    ((*(sptr - wpls2) >> 1) | (*(sptr - wpls2 - 1) << 31)) |
                    ((*(sptr - wpls2) >> 2) | (*(sptr - wpls2 - 1) << 30));
        }
    }
}

static void
ferode_1_53(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2;

    wpls2 = 2 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr - wpls2) >> 2) | (*(sptr - wpls2 - 1) << 30)) &
                    ((*(sptr - wpls2) >> 1) | (*(sptr - wpls2 - 1) << 31)) &
                    (*(sptr - wpls2)) &
                    ((*(sptr - wpls2) << 1) | (*(sptr - wpls2 + 1) >> 31)) &
                    ((*(sptr - wpls2) << 2) | (*(sptr - wpls2 + 1) >> 30)) &
                    ((*(sptr - wpls) >> 2) | (*(sptr - wpls - 1) << 30)) &
                    ((*(sptr - wpls) >> 1) | (*(sptr - wpls - 1) << 31)) &
                    (*(sptr - wpls)) &
                    ((*(sptr - wpls) << 1) | (*(sptr - wpls + 1) >> 31)) &
                    ((*(sptr - wpls) << 2) | (*(sptr - wpls + 1) >> 30)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) &
                    ((*(sptr + wpls) >> 2) | (*(sptr + wpls - 1) << 30)) &
                    ((*(sptr + wpls) >> 1) | (*(sptr + wpls - 1) << 31)) &
                    (*(sptr + wpls)) &
                    ((*(sptr + wpls) << 1) | (*(sptr + wpls + 1) >> 31)) &
                    ((*(sptr + wpls) << 2) | (*(sptr + wpls + 1) >> 30)) &
                    ((*(sptr + wpls2) >> 2) | (*(sptr + wpls2 - 1) << 30)) &
                    ((*(sptr + wpls2) >> 1) | (*(sptr + wpls2 - 1) << 31)) &
                    (*(sptr + wpls2)) &
                    ((*(sptr + wpls2) << 1) | (*(sptr + wpls2 + 1) >> 31)) &
                    ((*(sptr + wpls2) << 2) | (*(sptr + wpls2 + 1) >> 30));
        }
    }
}

static void
fdilate_1_54(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 1) | (*(sptr - 1) << 31)) |
                    (*(sptr - wpls));
        }
    }
}

static void
ferode_1_54(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    (*(sptr + wpls));
        }
    }
}

static void
fdilate_1_55(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*sptr) |
                    ((*(sptr - wpls) >> 1) | (*(sptr - wpls - 1) << 31));
        }
    }
}

static void
ferode_1_55(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*sptr) &
                    ((*(sptr + wpls) << 1) | (*(sptr + wpls + 1) >> 31));
        }
    }
}

static void
fdilate_1_56(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2;

    wpls2 = 2 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr + wpls2) >> 2) | (*(sptr + wpls2 - 1) << 30)) |
                    ((*(sptr + wpls) >> 1) | (*(sptr + wpls - 1) << 31)) |
                    (*sptr) |
                    ((*(sptr - wpls) << 1) | (*(sptr - wpls + 1) >> 31)) |
                    ((*(sptr - wpls2) << 2) | (*(sptr - wpls2 + 1) >> 30));
        }
    }
}

static void
ferode_1_56(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2;

    wpls2 = 2 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr - wpls2) << 2) | (*(sptr - wpls2 + 1) >> 30)) &
                    ((*(sptr - wpls) << 1) | (*(sptr - wpls + 1) >> 31)) &
                    (*sptr) &
                    ((*(sptr + wpls) >> 1) | (*(sptr + wpls - 1) << 31)) &
                    ((*(sptr + wpls2) >> 2) | (*(sptr + wpls2 - 1) << 30));
        }
    }
}

static void
fdilate_1_57(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2;

    wpls2 = 2 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr + wpls2) << 2) | (*(sptr + wpls2 + 1) >> 30)) |
                    ((*(sptr + wpls) << 1) | (*(sptr + wpls + 1) >> 31)) |
                    (*sptr) |
                    ((*(sptr - wpls) >> 1) | (*(sptr - wpls - 1) << 31)) |
                    ((*(sptr - wpls2) >> 2) | (*(sptr - wpls2 - 1) << 30));
        }
    }
}

static void
ferode_1_57(l_uint32  *datad,
            l_int32    w,
            l_int32    h,
            l_int32    wpld,
            l_uint32  *datas,
            l_int32    wpls)
{
l_int32   i;
l_int32   j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2;

    wpls2 = 2 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr - wpls2) >> 2) | (*(sptr - wpls2 - 1) << 30)) &
                    ((*(sptr - wpls) >> 1) | (*(sptr - wpls - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr + wpls) << 1) | (*(sptr + wpls + 1) >> 31)) &
                    ((*(sptr + wpls2) << 2) | (*(sptr + wpls2 + 1) >> 30));
        }
    }
}

