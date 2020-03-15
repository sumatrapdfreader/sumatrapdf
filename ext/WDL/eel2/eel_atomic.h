#ifndef __EEL_ATOMIC_H__
#define __EEL_ATOMIC_H__

// requires these to be defined
//#define EEL_ATOMIC_SET_SCOPE(opaque) WDL_Mutex *mutex = (opaque?&((effectProcessor *)opaque)->m_atomic_mutex:&atomic_mutex);
//#define EEL_ATOMIC_ENTER mutex->Enter()
//#define EEL_ATOMIC_LEAVE mutex->Leave()

static EEL_F NSEEL_CGEN_CALL atomic_setifeq(void *opaque, EEL_F *a,  EEL_F *cmp, EEL_F *nd)
{
  EEL_F ret;
  EEL_ATOMIC_SET_SCOPE(opaque)
  EEL_ATOMIC_ENTER;
  ret = *a;
  if (fabs(ret - *cmp) < NSEEL_CLOSEFACTOR) *a = *nd;
  EEL_ATOMIC_LEAVE;
  return ret;
}

static EEL_F NSEEL_CGEN_CALL atomic_exch(void *opaque, EEL_F *a, EEL_F *b)
{
  EEL_F tmp;
  EEL_ATOMIC_SET_SCOPE(opaque)
  EEL_ATOMIC_ENTER;
  tmp = *b;
  *b = *a;
  *a = tmp;
  EEL_ATOMIC_LEAVE;
  return tmp;
}

static EEL_F NSEEL_CGEN_CALL atomic_add(void *opaque, EEL_F *a, EEL_F *b)
{
  EEL_F tmp;
  EEL_ATOMIC_SET_SCOPE(opaque)
  EEL_ATOMIC_ENTER;
  tmp = (*a += *b);
  EEL_ATOMIC_LEAVE;
  return tmp;
}

static EEL_F NSEEL_CGEN_CALL atomic_set(void *opaque, EEL_F *a, EEL_F *b)
{
  EEL_F tmp;
  EEL_ATOMIC_SET_SCOPE(opaque)
  EEL_ATOMIC_ENTER;
  tmp = *a = *b;
  EEL_ATOMIC_LEAVE;
  return tmp;
}

static EEL_F NSEEL_CGEN_CALL atomic_get(void *opaque, EEL_F *a)
{
  EEL_F tmp;
  EEL_ATOMIC_SET_SCOPE(opaque)
  EEL_ATOMIC_ENTER;
  tmp = *a;
  EEL_ATOMIC_LEAVE;
  return tmp;
}

static void EEL_atomic_register()
{
  NSEEL_addfunc_retval("atomic_setifequal",3, NSEEL_PProc_THIS, &atomic_setifeq);
  NSEEL_addfunc_retval("atomic_exch",2, NSEEL_PProc_THIS, &atomic_exch);
  NSEEL_addfunc_retval("atomic_add",2, NSEEL_PProc_THIS, &atomic_add);
  NSEEL_addfunc_retval("atomic_set",2, NSEEL_PProc_THIS, &atomic_set);
  NSEEL_addfunc_retval("atomic_get",1, NSEEL_PProc_THIS, &atomic_get);
}

#endif
