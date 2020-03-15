#ifndef _NSEEL_GLUE_ARM_H_
#define _NSEEL_GLUE_ARM_H_

// r0=return value, first parm, r1-r2 parms
// r3+ should be reserved
// blx addr
// stmfd sp!, {register list, lr}
// ldmfd sp!, {register list, pc}

// let's make r8 = worktable
// let's make r7 = ramtable
// r6 = consttab
// r5 = worktable ptr

// r0=p1
// r1=p2
// r2=p3

// d0 is return value?

#define GLUE_MAX_FPSTACK_SIZE 0 // no stack support
#define GLUE_MAX_JMPSIZE ((1<<25) - 1024) // maximum relative jump size

// endOfInstruction is end of jump with relative offset, offset passed in is offset from end of dest instruction.
// TODO: verify, but offset probably from next instruction (PC is ahead)
#define GLUE_JMP_SET_OFFSET(endOfInstruction,offset) (((int *)(endOfInstruction))[-1] = (((int *)(endOfInstruction))[-1]&0xFF000000)|((((offset)>>2)-1)))

                                           // /=conditional=always = 0xE
                                           // |/= 101(L), so 8+2+0 = 10 = A
static const unsigned int GLUE_JMP_NC[] = { 0xEA000000 };

static const unsigned int GLUE_JMP_IF_P1_Z[]=
{
  0xe1100000, // tst r0, r0
  0x0A000000,  // branch if Z set
};
static const unsigned int GLUE_JMP_IF_P1_NZ[]=
{
  0xe1100000, // tst r0, r0
  0x1A000000,  // branch if Z clear
};

#define GLUE_MOV_PX_DIRECTVALUE_SIZE 8
static void GLUE_MOV_PX_DIRECTVALUE_GEN(void *b, INT_PTR v, unsigned int wv) 
{   
  // requires ARMv6thumb2 or later
  const unsigned int reg_add = wv << 12;
  static const unsigned int tab[2] = {
    0xe3000000, // movw r0, #0000
    0xe3400000, // movt r0, #0000
  };
  // 0xABAAA, B is register, A are bits of word
  unsigned int *p=(unsigned int *)b;
  p[0] = tab[0] | reg_add | (v&0xfff) | ((v&0xf000)<<4);
  p[1] = tab[1] | reg_add | ((v>>16)&0xfff) | ((v&0xf0000000)>>12);
}

const static unsigned int GLUE_FUNC_ENTER[1] = { 0xe92d4010 }; // push {r4, lr} 
#define GLUE_FUNC_ENTER_SIZE 4
const static unsigned int GLUE_FUNC_LEAVE[1] = { 0 }; // let GLUE_RET pop
#define GLUE_FUNC_LEAVE_SIZE 0
const static unsigned int GLUE_RET[]={  0xe8bd8010 }; // pop {r4, pc}

static int GLUE_RESET_WTP(unsigned char *out, void *ptr)
{
  const static unsigned int GLUE_SET_WTP_FROM_R8 = 0xe1a05008; // mov r5, r8
  if (out) memcpy(out,&GLUE_SET_WTP_FROM_R8,sizeof(GLUE_SET_WTP_FROM_R8));
  return sizeof(GLUE_SET_WTP_FROM_R8);
}


const static unsigned int GLUE_PUSH_P1[1]={ 0xe52d0008 }; // push {r0}, aligned to 8


static int arm_encode_constforalu(int amt)
{
  int nrot = 16;
  while (amt >= 0x100 && nrot > 1)
  {
    // ARM encodes integers for ALU operations as rotated right by third nibble*2
    amt = (amt + 3)>>2;
    nrot--;
  }
  return ((nrot&15) << 8) | amt;
}


#define GLUE_STORE_P1_TO_STACK_AT_OFFS_SIZE(x) ((x)>=4096 ? 8 : 4)
static void GLUE_STORE_P1_TO_STACK_AT_OFFS(void *b, int offs)
{
  if (offs >= 4096)
  {
    // add r2, sp, (offs&~4095)
    *(unsigned int *)b = 0xe28d2000 | arm_encode_constforalu(offs&~4095);
    // str r0, [r2, offs&4095]
    ((unsigned int *)b)[1] = 0xe5820000 + (offs&4095);
  }
  else
  {
    // str r0, [sp, #offs]
    *(unsigned int *)b = 0xe58d0000 + offs;
  }
}

#define GLUE_MOVE_PX_STACKPTR_SIZE 4
static void GLUE_MOVE_PX_STACKPTR_GEN(void *b, int wv)
{
  // mov rX, sp
  *(unsigned int *)b = 0xe1a0000d + (wv<<12);
}

#define GLUE_MOVE_STACK_SIZE 4
static void GLUE_MOVE_STACK(void *b, int amt)
{
  unsigned int instr = 0xe28dd000;
  if (amt < 0) 
  {
    instr = 0xe24dd000;
    amt=-amt;
  }
  *(unsigned int*)b = instr | arm_encode_constforalu(amt);
}

#define GLUE_POP_PX_SIZE 4
static void GLUE_POP_PX(void *b, int wv)
{
  ((unsigned int *)b)[0] = 0xe49d0008 | (wv<<12); // pop {rX}, aligned to 8
}

#define GLUE_SET_PX_FROM_P1_SIZE 4
static void GLUE_SET_PX_FROM_P1(void *b, int wv)
{
  *(unsigned int *)b  = 0xe1a00000 | (wv<<12); // mov rX, r0
}


static const unsigned int GLUE_PUSH_P1PTR_AS_VALUE[] = 
{ 
  0xed907b00, // fldd d7, [r0]
  0xe24dd008, // sub sp, sp, #8
  0xed8d7b00, // fstd d7, [sp]
};

static int GLUE_POP_VALUE_TO_ADDR(unsigned char *buf, void *destptr)
{    
  if (buf)
  {
    unsigned int *bufptr = (unsigned int *)buf;
    *bufptr++ = 0xed9d7b00; // fldd d7, [sp]
    *bufptr++ = 0xe28dd008; // add sp, sp, #8
    GLUE_MOV_PX_DIRECTVALUE_GEN(bufptr, (INT_PTR)destptr,0);
    bufptr += GLUE_MOV_PX_DIRECTVALUE_SIZE/4;
    *bufptr++ = 0xed807b00; // fstd d7, [r0] 
  }
  return 3*4 + GLUE_MOV_PX_DIRECTVALUE_SIZE;
}

static int GLUE_COPY_VALUE_AT_P1_TO_PTR(unsigned char *buf, void *destptr)
{    
  if (buf)
  {
    unsigned int *bufptr = (unsigned int *)buf;
    *bufptr++ = 0xed907b00; // fldd d7, [r0]
    GLUE_MOV_PX_DIRECTVALUE_GEN(bufptr, (INT_PTR)destptr,0);
    bufptr += GLUE_MOV_PX_DIRECTVALUE_SIZE/4;
    *bufptr++ = 0xed807b00; // fstd d7, [r0] 
  }
  return 2*4 + GLUE_MOV_PX_DIRECTVALUE_SIZE;
}


#ifndef _MSC_VER
#define GLUE_CALL_CODE(bp, cp, rt) do { \
  unsigned int f; \
  if (!(h->compile_flags&NSEEL_CODE_COMPILE_FLAG_NOFPSTATE) && \
      !((f=glue_getscr())&(1<<24))) {  \
    glue_setscr(f|(1<<24)); \
    eel_callcode32(bp, cp, rt); \
    glue_setscr(f); \
  } else eel_callcode32(bp, cp, rt);\
  } while(0)

static const double __consttab[] = { 
    NSEEL_CLOSEFACTOR, 
    0.0,
    1.0,
    -1.0,
    -0.5, // for invsqrt
    1.5,
  };

static void eel_callcode32(INT_PTR bp, INT_PTR cp, INT_PTR rt) 
{
  __asm__ volatile(
          "mov r7, %2\n"
          "mov r6, %3\n"
          "mov r8, %1\n"
          "mov r0, %0\n"
          "mov r1, sp\n"
          "bic sp, sp, #7\n"
          "push {r1, lr}\n"
          "blx r0\n"
          "pop {r1, lr}\n"
          "mov sp, r1\n"
            ::"r" (cp), "r" (bp), "r" (rt), "r" (__consttab) : 
             "r5", "r6", "r7", "r8", "r10");
};
#endif

static unsigned char *EEL_GLUE_set_immediate(void *_p, INT_PTR newv)
{
  unsigned int *p=(unsigned int *)_p;
  while ((p[0]&0x000F0FFF) != 0x000d0ead && 
         (p[1]&0x000F0FFF) != 0x000b0eef) p++;
  p[0] = (p[0]&0xFFF0F000) | (newv&0xFFF) | ((newv << 4) & 0xF0000);
  p[1] = (p[1]&0xFFF0F000) | ((newv>>16)&0xFFF) | ((newv >> 12)&0xF0000);

  return (unsigned char *)(p+1);
}

#define GLUE_SET_PX_FROM_WTP_SIZE sizeof(int)
static void GLUE_SET_PX_FROM_WTP(void *b, int wv)
{
  *(unsigned int *)b = 0xe1a00005 + (wv<<12); // mov rX, r5
}

static int GLUE_POP_FPSTACK_TO_PTR(unsigned char *buf, void *destptr)
{
  if (buf)
  {
    unsigned int *bufptr = (unsigned int *)buf;
    GLUE_MOV_PX_DIRECTVALUE_GEN(bufptr, (INT_PTR)destptr,0);
    bufptr += GLUE_MOV_PX_DIRECTVALUE_SIZE/4;

    *bufptr++ = 0xed800b00; // fstd d0, [r0] 
  }
  return GLUE_MOV_PX_DIRECTVALUE_SIZE + sizeof(int);
}

#define GLUE_POP_FPSTACK_SIZE 0
static const unsigned int GLUE_POP_FPSTACK[1] = { 0 }; // no need to pop, not a stack

static const unsigned int GLUE_POP_FPSTACK_TOSTACK[] = {
  0xe24dd008, // sub sp, sp, #8
  0xed8d0b00, // fstd d0, [sp]
};

static const unsigned int GLUE_POP_FPSTACK_TO_WTP[] = { 
  0xed850b00, // fstd d0, [r5]
  0xe2855008, // add r5, r5, #8
};

#define GLUE_PUSH_VAL_AT_PX_TO_FPSTACK_SIZE 4
static void GLUE_PUSH_VAL_AT_PX_TO_FPSTACK(void *b, int wv)
{
  *(unsigned int *)b = 0xed900b00 + (wv<<16); // fldd d0, [rX]
}

#define GLUE_POP_FPSTACK_TO_WTP_TO_PX_SIZE (sizeof(GLUE_POP_FPSTACK_TO_WTP) + GLUE_SET_PX_FROM_WTP_SIZE)
static void GLUE_POP_FPSTACK_TO_WTP_TO_PX(unsigned char *buf, int wv)
{
  GLUE_SET_PX_FROM_WTP(buf,wv); 
  memcpy(buf + GLUE_SET_PX_FROM_WTP_SIZE,GLUE_POP_FPSTACK_TO_WTP,sizeof(GLUE_POP_FPSTACK_TO_WTP));
};

static unsigned int GLUE_POP_STACK_TO_FPSTACK[2] = 
{ 
  0xed9d0b00, // fldd d0, [sp]
  0xe28dd008, // add sp, sp, #8
};


static const unsigned int GLUE_SET_P1_Z[] =  { 0xe3a00000 }; // mov r0, #0
static const unsigned int GLUE_SET_P1_NZ[] = { 0xe3a00001 }; // mov r0, #1


static void *GLUE_realAddress(void *fn, void *fn_e, int *size)
{
  static const unsigned int sig[3] = { 0xe1a00000, 0xe1a01001, 0xe1a02002 };
  unsigned char *p = (unsigned char *)fn;

  while (memcmp(p,sig,sizeof(sig))) p+=4;
  p+=sizeof(sig);
  fn = p;

  while (memcmp(p,sig,sizeof(sig))) p+=4;
  *size = p - (unsigned char *)fn;
  return fn;
}

static unsigned int __attribute__((unused)) glue_getscr()
{
  unsigned int rv;
  asm volatile ( "fmrx %0, fpscr" : "=r" (rv));
  return rv;
}
static void  __attribute__((unused)) glue_setscr(unsigned int v)
{
  asm volatile ( "fmxr fpscr, %0" :: "r"(v));
}

void eel_setfp_round() 
{ 
  // glue_setscr(glue_getscr()|(3<<22));
}
void eel_setfp_trunc() 
{ 
  // glue_setscr(glue_getscr()&~(3<<22));
}
void eel_enterfp(int s[2]) 
{
  s[0] = glue_getscr();
  glue_setscr(s[0] | (1<<24)); // could also do 3<<22 for RTZ
}
void eel_leavefp(int s[2]) 
{
  glue_setscr(s[0]);
}


#endif
