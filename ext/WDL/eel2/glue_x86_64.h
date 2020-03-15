#ifndef _NSEEL_GLUE_X86_64_H_
#define _NSEEL_GLUE_X86_64_H_

#define GLUE_MAX_FPSTACK_SIZE 8
#define GLUE_JMP_SET_OFFSET(endOfInstruction,offset) (((int *)(endOfInstruction))[-1] = (int) (offset))

#define GLUE_PREFER_NONFP_DV_ASSIGNS

static const unsigned char GLUE_JMP_NC[] = { 0xE9, 0,0,0,0, }; // jmp<offset>
static const unsigned char GLUE_JMP_IF_P1_Z[] = {0x85, 0xC0, 0x0F, 0x84, 0,0,0,0 }; // test eax, eax, jz
static const unsigned char GLUE_JMP_IF_P1_NZ[] = {0x85, 0xC0, 0x0F, 0x85, 0,0,0,0 }; // test eax, eax, jnz


#define GLUE_FUNC_ENTER_SIZE 0
#define GLUE_FUNC_LEAVE_SIZE 0
const static unsigned int GLUE_FUNC_ENTER[1];
const static unsigned int GLUE_FUNC_LEAVE[1];

  // on x86-64:
  //  stack is always 16 byte aligned
  //  pushing values to the stack (for eel functions) has alignment pushed first, then value (value is at the lower address)
  //  pushing pointers to the stack has the pointer pushed first, then the alignment (pointer is at the higher address)
  #define GLUE_MOV_PX_DIRECTVALUE_SIZE 10
  static void GLUE_MOV_PX_DIRECTVALUE_GEN(void *b, INT_PTR v, int wr) {   
    const static unsigned short tab[3] = 
    {
      0xB848 /* mov rax, dv*/, 
      0xBF48 /* mov rdi, dv */ , 
      0xB948 /* mov rcx, dv */ 
    };
    unsigned short *bb = (unsigned short *)b;
    *bb++ = tab[wr];  // mov rax, directvalue
    *(INT_PTR *)bb = v; 
  }

  const static unsigned char  GLUE_PUSH_P1[2]={	   0x50,0x50}; // push rax (pointer); push rax (alignment)

  #define GLUE_STORE_P1_TO_STACK_AT_OFFS_SIZE(x) 8
  static void GLUE_STORE_P1_TO_STACK_AT_OFFS(void *b, int offs)
  {
    ((unsigned char *)b)[0] = 0x48; // mov [rsp+offs], rax
    ((unsigned char *)b)[1] = 0x89; 
    ((unsigned char *)b)[2] = 0x84;
    ((unsigned char *)b)[3] = 0x24;
    *(int *)((unsigned char *)b+4) = offs;
  }

  #define GLUE_MOVE_PX_STACKPTR_SIZE 3
  static void GLUE_MOVE_PX_STACKPTR_GEN(void *b, int wv)
  {
    static const unsigned char tab[3][GLUE_MOVE_PX_STACKPTR_SIZE]=
    {
      { 0x48, 0x89, 0xe0 }, // mov rax, rsp
      { 0x48, 0x89, 0xe7 }, // mov rdi, rsp
      { 0x48, 0x89, 0xe1 }, // mov rcx, rsp
    };    
    memcpy(b,tab[wv],GLUE_MOVE_PX_STACKPTR_SIZE);
  }

  #define GLUE_MOVE_STACK_SIZE 7
  static void GLUE_MOVE_STACK(void *b, int amt)
  {
    ((unsigned char *)b)[0] = 0x48;
    ((unsigned char *)b)[1] = 0x81;
    if (amt < 0)
    {
      ((unsigned char *)b)[2] = 0xEC;
      *(int *)((char*)b+3) = -amt; // sub rsp, -amt32
    }
    else
    {
      ((unsigned char *)b)[2] = 0xc4;
      *(int *)((char*)b+3) = amt; // add rsp, amt32
    }
  }

  #define GLUE_POP_PX_SIZE 2
  static void GLUE_POP_PX(void *b, int wv)
  {
    static const unsigned char tab[3][GLUE_POP_PX_SIZE]=
    {
      {0x58,/*pop rax*/  0x58}, // pop alignment, then pop pointer
      {0x5F,/*pop rdi*/  0x5F}, 
      {0x59,/*pop rcx*/  0x59}, 
    };    
    memcpy(b,tab[wv],GLUE_POP_PX_SIZE);
  }

  static const unsigned char GLUE_PUSH_P1PTR_AS_VALUE[] = 
  {  
    0x50, /*push rax - for alignment */  
    0xff, 0x30, /* push qword [rax] */
  };

  static int GLUE_POP_VALUE_TO_ADDR(unsigned char *buf, void *destptr) // trashes P2 (rdi) and P3 (rcx)
  {    
    if (buf)
    {
      *buf++ = 0x48; *buf++ = 0xB9; *(void **) buf = destptr; buf+=8; // mov rcx, directvalue
      *buf++ = 0x8f; *buf++ = 0x01; // pop qword [rcx]      
      *buf++ = 0x5F ; // pop rdi (alignment, safe to trash rdi though)
    }
    return 1+10+2;
  }

  static int GLUE_COPY_VALUE_AT_P1_TO_PTR(unsigned char *buf, void *destptr) // trashes P2/P3
  {    
    if (buf)
    {
      *buf++ = 0x48; *buf++ = 0xB9; *(void **) buf = destptr; buf+=8; // mov rcx, directvalue
      *buf++ = 0x48; *buf++ = 0x8B; *buf++ = 0x38; // mov rdi, [rax]
      *buf++ = 0x48; *buf++ = 0x89; *buf++ = 0x39; // mov [rcx], rdi
    }

    return 3 + 10 + 3;
  }

  static int GLUE_POP_FPSTACK_TO_PTR(unsigned char *buf, void *destptr)
  {
    if (buf)
    {
      *buf++ = 0x48;
      *buf++ = 0xB8; 
      *(void **) buf = destptr; buf+=8; // mov rax, directvalue
      *buf++ = 0xDD; *buf++ = 0x18;  // fstp qword [rax]
    }
    return 2+8+2;
  }


  #define GLUE_SET_PX_FROM_P1_SIZE 3
  static void GLUE_SET_PX_FROM_P1(void *b, int wv)
  {
    static const unsigned char tab[3][GLUE_SET_PX_FROM_P1_SIZE]={
      {0x90,0x90,0x90}, // should never be used! (nopnop)
      {0x48,0x89,0xC7}, // mov rdi, rax
      {0x48,0x89,0xC1}, // mov rcx, rax
    };
    memcpy(b,tab[wv],GLUE_SET_PX_FROM_P1_SIZE);
  }


  #define GLUE_POP_FPSTACK_SIZE 2
  static const unsigned char GLUE_POP_FPSTACK[2] = { 0xDD, 0xD8 }; // fstp st0

  static const unsigned char GLUE_POP_FPSTACK_TOSTACK[] = {
    0x48, 0x81, 0xEC, 16, 0,0,0, // sub rsp, 16 
    0xDD, 0x1C, 0x24 // fstp qword (%rsp)  
  };

  static const unsigned char GLUE_POP_FPSTACK_TO_WTP[] = { 
      0xDD, 0x1E, /* fstp qword [rsi] */
      0x48, 0x81, 0xC6, 8, 0,0,0,/* add rsi, 8 */ 
  };

  #define GLUE_SET_PX_FROM_WTP_SIZE 3
  static void GLUE_SET_PX_FROM_WTP(void *b, int wv)
  {
    static const unsigned char tab[3][GLUE_SET_PX_FROM_WTP_SIZE]={
      {0x48, 0x89,0xF0}, // mov rax, rsi
      {0x48, 0x89,0xF7}, // mov rdi, rsi
      {0x48, 0x89,0xF1}, // mov rcx, rsi
    };
    memcpy(b,tab[wv],GLUE_SET_PX_FROM_WTP_SIZE);
  }

  #define GLUE_PUSH_VAL_AT_PX_TO_FPSTACK_SIZE 2
  static void GLUE_PUSH_VAL_AT_PX_TO_FPSTACK(void *b, int wv)
  {
    static const unsigned char tab[3][GLUE_PUSH_VAL_AT_PX_TO_FPSTACK_SIZE]={
      {0xDD,0x00}, // fld qword [rax]
      {0xDD,0x07}, // fld qword [rdi]
      {0xDD,0x01}, // fld qword [rcx]
    };
    memcpy(b,tab[wv],GLUE_PUSH_VAL_AT_PX_TO_FPSTACK_SIZE);
  }
  static unsigned char GLUE_POP_STACK_TO_FPSTACK[] = {
    0xDD, 0x04, 0x24, // fld qword (%rsp)
    0x48, 0x81, 0xC4, 16, 0,0,0, //  add rsp, 16
  };


#define GLUE_POP_FPSTACK_TO_WTP_TO_PX_SIZE (GLUE_SET_PX_FROM_WTP_SIZE + sizeof(GLUE_POP_FPSTACK_TO_WTP))
static void GLUE_POP_FPSTACK_TO_WTP_TO_PX(unsigned char *buf, int wv)
{
  GLUE_SET_PX_FROM_WTP(buf,wv);
  memcpy(buf + GLUE_SET_PX_FROM_WTP_SIZE,GLUE_POP_FPSTACK_TO_WTP,sizeof(GLUE_POP_FPSTACK_TO_WTP));
};


const static unsigned char  GLUE_RET=0xC3;

static int GLUE_RESET_WTP(unsigned char *out, void *ptr)
{
  if (out)
  {
	  *out++ = 0x48;
    *out++ = 0xBE; // mov rsi, constant64
  	*(void **)out = ptr;
    out+=sizeof(void *);
  }
  return 2+sizeof(void *);
}

extern void eel_callcode64(INT_PTR code, INT_PTR ram_tab);
extern void eel_callcode64_fast(INT_PTR code, INT_PTR ram_tab);
#define GLUE_CALL_CODE(bp, cp, rt) do { \
  if (h->compile_flags&NSEEL_CODE_COMPILE_FLAG_NOFPSTATE) eel_callcode64_fast(cp, rt); \
  else eel_callcode64(cp, rt);\
  } while(0)
#define GLUE_TABPTR_IGNORED

static unsigned char *EEL_GLUE_set_immediate(void *_p, INT_PTR newv)
{
  char *p=(char*)_p;
  INT_PTR scan = 0xFEFEFEFEFEFEFEFE;
  while (*(INT_PTR *)p != scan) p++;
  *(INT_PTR *)p = newv;
  return (unsigned char *) (((INT_PTR*)p)+1);
}

#define INT_TO_LECHARS(x) ((x)&0xff),(((x)>>8)&0xff), (((x)>>16)&0xff), (((x)>>24)&0xff)

#define GLUE_INLINE_LOOPS

static const unsigned char GLUE_LOOP_LOADCNT[]={
        0xDD, 0x0E,           //fistTp qword [rsi]
  0x48, 0x8B, 0x0E,           // mov rcx, [rsi]
  0x48, 0x81, 0xf9, 1,0,0,0,  // cmp rcx, 1
        0x0F, 0x8C, 0,0,0,0,  // JL <skipptr>
};

#if NSEEL_LOOPFUNC_SUPPORT_MAXLEN > 0
#define GLUE_LOOP_CLAMPCNT_SIZE sizeof(GLUE_LOOP_CLAMPCNT)
static const unsigned char GLUE_LOOP_CLAMPCNT[]={
  0x48, 0x81, 0xf9, INT_TO_LECHARS(NSEEL_LOOPFUNC_SUPPORT_MAXLEN), // cmp rcx, NSEEL_LOOPFUNC_SUPPORT_MAXLEN
        0x0F, 0x8C, 10,0,0,0,  // JL over-the-mov
  0x48, 0xB9, INT_TO_LECHARS(NSEEL_LOOPFUNC_SUPPORT_MAXLEN), 0,0,0,0, // mov rcx, NSEEL_LOOPFUNC_SUPPORT_MAXLEN
};
#else
#define GLUE_LOOP_CLAMPCNT_SIZE 0
#define GLUE_LOOP_CLAMPCNT ""
#endif

#define GLUE_LOOP_BEGIN_SIZE sizeof(GLUE_LOOP_BEGIN)
static const unsigned char GLUE_LOOP_BEGIN[]={ 
  0x56, //push rsi
  0x51, // push rcx
};
static const unsigned char GLUE_LOOP_END[]={ 
  0x59, //pop rcx
  0x5E, // pop rsi
  0xff, 0xc9, // dec rcx
  0x0f, 0x85, 0,0,0,0, // jnz ...
};



#if NSEEL_LOOPFUNC_SUPPORT_MAXLEN > 0
static const unsigned char GLUE_WHILE_SETUP[]={
  0x48, 0xB9, INT_TO_LECHARS(NSEEL_LOOPFUNC_SUPPORT_MAXLEN), 0,0,0,0, // mov rcx, NSEEL_LOOPFUNC_SUPPORT_MAXLEN
};
#define GLUE_WHILE_SETUP_SIZE sizeof(GLUE_WHILE_SETUP)

static const unsigned char GLUE_WHILE_BEGIN[]={ 
  0x56, //push rsi
  0x51, // push rcx
};
static const unsigned char GLUE_WHILE_END[]={ 
  0x59, //pop rcx
  0x5E, // pop rsi

  0xff, 0xc9, // dec rcx
  0x0f, 0x84,  0,0,0,0, // jz endpt
};


#else
#define GLUE_WHILE_SETUP ""
#define GLUE_WHILE_SETUP_SIZE 0
#define GLUE_WHILE_END_NOJUMP

static const unsigned char GLUE_WHILE_BEGIN[]={ 
  0x56, //push rsi
  0x51, // push rcx
};
static const unsigned char GLUE_WHILE_END[]={ 
  0x59, //pop rcx
  0x5E, // pop rsi
};

#endif


static const unsigned char GLUE_WHILE_CHECK_RV[] = {
  0x85, 0xC0, // test eax, eax
  0x0F, 0x85, 0,0,0,0 // jnz  looppt
};

static const unsigned char GLUE_SET_P1_Z[] = { 0x48, 0x29, 0xC0 }; // sub rax, rax
static const unsigned char GLUE_SET_P1_NZ[] = { 0xb0, 0x01 }; // mov al, 1


#define GLUE_HAS_FXCH
static const unsigned char GLUE_FXCH[] = {0xd9, 0xc9};

#define GLUE_HAS_FLDZ
static const unsigned char GLUE_FLDZ[] = {0xd9, 0xee};
#define GLUE_HAS_FLD1
static const unsigned char GLUE_FLD1[] = {0xd9, 0xe8};


static EEL_F negativezeropointfive=-0.5f;
static EEL_F onepointfive=1.5f;
#define GLUE_INVSQRT_NEEDREPL &negativezeropointfive, &onepointfive,

#define GLUE_HAS_NATIVE_TRIGSQRTLOG


static void *GLUE_realAddress(void *fn, void *fn_e, int *size)
{
  static const unsigned char sig[12] = { 0x89, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
  unsigned char *p = (unsigned char *)fn;

  while (memcmp(p,sig,sizeof(sig))) p++;
  p+=sizeof(sig);
  fn = p;

  while (memcmp(p,sig,sizeof(sig))) p++;
  *size = (int) (p - (unsigned char *)fn);
  return fn;
}

// end of x86-64

#endif
