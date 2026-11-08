/*
 * strtest.c
 *
 * Copyright (C) 2022-2023, D. R. Commander.
 * For conditions of distribution and use, see the accompanying README.ijg
 * file.
 */

#include "jinclude.h"
#include <errno.h>


#define CHECK_VALUE(actual, expected, desc) \
  if (actual != expected) { \
    printf("ERROR in line %d: " desc " is %d, should be %d\n", \
           __LINE__, actual, expected); \
    return -1; \
  }

#define CHECK_ERRNO(errno_return, expected_errno) \
  CHECK_VALUE(errno_return, expected_errno, "Return value") \
  CHECK_VALUE(errno, expected_errno, "errno") \


#ifdef _MSC_VER

void invalid_parameter_handler(const wchar_t *expression,
                               const wchar_t *function, const wchar_t *file,
                               unsigned int line, uintptr_t pReserved)
{
}

#endif


int main(int argc, char **argv)
{
#if !defined(NO_GETENV) || !defined(NO_PUTENV)
  int err;
#endif
#ifndef NO_GETENV
  char env[3];
#endif

#ifdef _MSC_VER
  _set_invalid_parameter_handler(invalid_parameter_handler);
#endif

  /***************************************************************************/

#ifndef NO_PUTENV

  printf("PUTENV_S():\n");

  errno = 0;
  err = PUTENV_S(NULL, "12");
  CHECK_ERRNO(err, EINVAL);

  errno = 0;
  err = PUTENV_S("TESTENV", NULL);
  CHECK_ERRNO(err, EINVAL);

  errno = 0;
  err = PUTENV_S("TESTENV", "12");
  CHECK_ERRNO(err, 0);

  printf("SUCCESS!\n\n");

#endif

  /***************************************************************************/

#ifndef NO_GETENV

  printf("GETENV_S():\n");

  errno = 0;
  env[0] = 1;
  env[1] = 2;
  env[2] = 3;
  err = GETENV_S(env, 3, NULL);
  CHECK_ERRNO(err, 0);
  CHECK_VALUE(env[0], 0, "env[0]");
  CHECK_VALUE(env[1], 2, "env[1]");
  CHECK_VALUE(env[2], 3, "env[2]");

  errno = 0;
  env[0] = 1;
  env[1] = 2;
  env[2] = 3;
  err = GETENV_S(env, 3, "TESTENV2");
  CHECK_ERRNO(err, 0);
  CHECK_VALUE(env[0], 0, "env[0]");
  CHECK_VALUE(env[1], 2, "env[1]");
  CHECK_VALUE(env[2], 3, "env[2]");

  errno = 0;
  err = GETENV_S(NULL, 3, "TESTENV");
  CHECK_ERRNO(err, EINVAL);

  errno = 0;
  err = GETENV_S(NULL, 0, "TESTENV");
  CHECK_ERRNO(err, 0);

  errno = 0;
  env[0] = 1;
  err = GETENV_S(env, 0, "TESTENV");
  CHECK_ERRNO(err, EINVAL);
  CHECK_VALUE(env[0], 1, "env[0]");

  errno = 0;
  env[0] = 1;
  env[1] = 2;
  env[2] = 3;
  err = GETENV_S(env, 1, "TESTENV");
  CHECK_VALUE(err, ERANGE, "Return value");
  CHECK_VALUE(errno, 0, "errno");
  CHECK_VALUE(env[0], 0, "env[0]");
  CHECK_VALUE(env[1], 2, "env[1]");
  CHECK_VALUE(env[2], 3, "env[2]");

  errno = 0;
  env[0] = 1;
  env[1] = 2;
  env[2] = 3;
  err = GETENV_S(env, 2, "TESTENV");
  CHECK_VALUE(err, ERANGE, "Return value");
  CHECK_VALUE(errno, 0, "errno");
  CHECK_VALUE(env[0], 0, "env[0]");
  CHECK_VALUE(env[1], 2, "env[1]");
  CHECK_VALUE(env[2], 3, "env[2]");

  errno = 0;
  env[0] = 1;
  env[1] = 2;
  env[2] = 3;
  err = GETENV_S(env, 3, "TESTENV");
  CHECK_ERRNO(err, 0);
  CHECK_VALUE(env[0], '1', "env[0]");
  CHECK_VALUE(env[1], '2', "env[1]");
  CHECK_VALUE(env[2], 0, "env[2]");

  printf("SUCCESS!\n\n");

#endif

  /***************************************************************************/

  return 0;
}
