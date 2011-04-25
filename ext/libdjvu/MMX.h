//C-  -*- C++ -*-
//C- -------------------------------------------------------------------
//C- DjVuLibre-3.5
//C- Copyright (c) 2002  Leon Bottou and Yann Le Cun.
//C- Copyright (c) 2001  AT&T
//C-
//C- This software is subject to, and may be distributed under, the
//C- GNU General Public License, either Version 2 of the license,
//C- or (at your option) any later version. The license should have
//C- accompanied the software or you may obtain a copy of the license
//C- from the Free Software Foundation at http://www.fsf.org .
//C-
//C- This program is distributed in the hope that it will be useful,
//C- but WITHOUT ANY WARRANTY; without even the implied warranty of
//C- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//C- GNU General Public License for more details.
//C- 
//C- DjVuLibre-3.5 is derived from the DjVu(r) Reference Library from
//C- Lizardtech Software.  Lizardtech Software has authorized us to
//C- replace the original DjVu(r) Reference Library notice by the following
//C- text (see doc/lizard2002.djvu and doc/lizardtech2007.djvu):
//C-
//C-  ------------------------------------------------------------------
//C- | DjVu (r) Reference Library (v. 3.5)
//C- | Copyright (c) 1999-2001 LizardTech, Inc. All Rights Reserved.
//C- | The DjVu Reference Library is protected by U.S. Pat. No.
//C- | 6,058,214 and patents pending.
//C- |
//C- | This software is subject to, and may be distributed under, the
//C- | GNU General Public License, either Version 2 of the license,
//C- | or (at your option) any later version. The license should have
//C- | accompanied the software or you may obtain a copy of the license
//C- | from the Free Software Foundation at http://www.fsf.org .
//C- |
//C- | The computer code originally released by LizardTech under this
//C- | license and unmodified by other parties is deemed "the LIZARDTECH
//C- | ORIGINAL CODE."  Subject to any third party intellectual property
//C- | claims, LizardTech grants recipient a worldwide, royalty-free, 
//C- | non-exclusive license to make, use, sell, or otherwise dispose of 
//C- | the LIZARDTECH ORIGINAL CODE or of programs derived from the 
//C- | LIZARDTECH ORIGINAL CODE in compliance with the terms of the GNU 
//C- | General Public License.   This grant only confers the right to 
//C- | infringe patent claims underlying the LIZARDTECH ORIGINAL CODE to 
//C- | the extent such infringement is reasonably necessary to enable 
//C- | recipient to make, have made, practice, sell, or otherwise dispose 
//C- | of the LIZARDTECH ORIGINAL CODE (or portions thereof) and not to 
//C- | any greater extent that may be necessary to utilize further 
//C- | modifications or combinations.
//C- |
//C- | The LIZARDTECH ORIGINAL CODE is provided "AS IS" WITHOUT WARRANTY
//C- | OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
//C- | TO ANY WARRANTY OF NON-INFRINGEMENT, OR ANY IMPLIED WARRANTY OF
//C- | MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
//C- +------------------------------------------------------------------

#ifndef _MMX_H_
#define _MMX_H_
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif

#include "DjVuGlobal.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


/** @name MMX.h
    Files #"MMX.h"# and #"MMX.cpp"# implement basic routines for
    supporting the MMX instructions on x86.  Future instruction sets
    for other processors may be supported in this file as well.

    Macro #MMX# is defined if the compiler supports the X86-MMX instructions.
    It does not mean however that the processor supports the instruction set.
    Variable #MMXControl::mmxflag# must be used to decide whether MMX.
    instructions can be executed.  MMX instructions are entered in the middle
    of C++ code using the following macros.  Examples can be found in
    #"IWTransform.cpp"#.

    \begin{description}
    \item[MMXrr( insn, srcreg, dstreg)] 
       Encode a register to register MMX instruction 
       (e.g. #paddw# or #punpcklwd#).
    \item[MMXar( insn, addr, dstreg )]
       Encode a memory to register MMX instruction 
       (e.g. #moveq# from memory).
    \item[MMXra( insn, srcreg, addr )]
       Encode a register to memory MMX instruction 
       (e.g. #moveq# to memory).
    \item[MMXir( insn, imm, dstreg )]
       Encode a immediate to register MMX instruction 
       (e.g #psraw#).
    \item[MMXemms]
       Execute the #EMMS# instruction to reset the FPU state.
    \end{description}

    @memo
    Essential support for MMX.
    @author: 
    L\'eon Bottou <leonb@research.att.com> -- initial implementation */
//@{


/** MMX Control. 
    Class #MMXControl# encapsulates a few static functions for 
    globally enabling or disabling MMX support. */

class MMXControl
{
 public:
  // MMX DETECTION
  /** Detects and enable MMX or similar technologies.  This function checks
      whether the CPU supports a vectorial instruction set (such as Intel's
      MMX) and enables them.  Returns a boolean indicating whether such an
      instruction set is available.  Speedups factors may vary. */
  static int enable_mmx();
  /** Disables MMX or similar technologies.  The transforms will then be
      performed using the baseline code. */
  static int disable_mmx();
  /** Contains a value greater than zero if the CPU supports vectorial
      instructions. A negative value means that you must call \Ref{enable_mmx}
      and test the value again. Direct access to this member should only be
      used to transfer the instruction flow to the vectorial branch of the
      code. Never modify the value of this variable.  Use #enable_mmx# or
      #disable_mmx# instead. */
  static int mmxflag;  // readonly
};

//@}




// ----------------------------------------
// GCC MMX MACROS

#ifndef NO_MMX

#if defined(__GNUC__) && defined(__i386__)
#define MMXemms \
  __asm__ volatile("emms" : : : "memory" ) 
#define MMXrr(op,src,dst) \
  __asm__ volatile( #op " %%" #src ",%%" #dst : : : "memory") 
#define MMXir(op,imm,dst) \
  __asm__ volatile( #op " %0,%%" #dst : : "i" (imm) : "memory") 
#define MMXar(op,addr,dst) \
  __asm__ volatile( #op " %0,%%" #dst : : "m" (*(addr)) : "memory") 
#define MMXra(op,src,addr) \
  __asm__ volatile( #op " %%" #src ",%0" : : "m" (*(addr)) : "memory") 
#define MMX 1
#endif


// ----------------------------------------
// MSVC MMX MACROS

#if defined(_MSC_VER) && defined(_M_IX86)
// Compiler option /GM is required
#pragma warning( disable : 4799 )
#define MMXemms \
  __asm { emms }
#define MMXrr(op,src,dst) \
  __asm { op dst,src }
#define MMXir(op,imm,dst) \
  __asm { op dst,imm }
#define MMXar(op,addr,dst) \
  { register __int64 var=*(__int64*)(addr); __asm { op dst,var } }
#define MMXra(op,src,addr) \
  { register __int64 var; __asm { op [var],src };  *(__int64*)addr = var; } 
// Probably not as efficient as GCC macros
#define MMX 1
#endif

#endif

// -----------

#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
