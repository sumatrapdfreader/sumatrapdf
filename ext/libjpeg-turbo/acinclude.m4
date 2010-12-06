# AC_PROG_NASM
# --------------------------
# Check that NASM exists and determine flags
AC_DEFUN([AC_PROG_NASM],[

AC_CHECK_PROGS(NASM, [nasm nasmw])
test -z "$NASM" && AC_MSG_ERROR([no nasm (Netwide Assembler) found])

AC_MSG_CHECKING([for object file format of host system])
case "$host_os" in
  cygwin* | mingw* | pw32* | interix*)
    case "$host_cpu" in
      x86_64)
        objfmt='Win64-COFF'
        ;;
      *)
        objfmt='Win32-COFF'
        ;;
    esac
  ;;
  msdosdjgpp* | go32*)
    objfmt='COFF'
  ;;
  os2-emx*)			# not tested
    objfmt='MSOMF'		# obj
  ;;
  linux*coff* | linux*oldld*)
    objfmt='COFF'		# ???
  ;;
  linux*aout*)
    objfmt='a.out'
  ;;
  linux*)
    case "$host_cpu" in
      x86_64)
        objfmt='ELF64'
        ;;
      *)
        objfmt='ELF'
        ;;
    esac
  ;;
  freebsd* | netbsd* | openbsd*)
    if echo __ELF__ | $CC -E - | grep __ELF__ > /dev/null; then
      objfmt='BSD-a.out'
    else
      case "$host_cpu" in
        x86_64 | amd64)
          objfmt='ELF64'
          ;;
        *)
          objfmt='ELF'
          ;;
      esac
    fi
  ;;
  solaris* | sunos* | sysv* | sco*)
    case "$host_cpu" in
      x86_64)
        objfmt='ELF64'
        ;;
      *)
        objfmt='ELF'
        ;;
    esac
  ;;
  darwin* | rhapsody* | nextstep* | openstep* | macos*)
    case "$host_cpu" in
      x86_64)
        objfmt='Mach-O64'
        ;;
      *)
        objfmt='Mach-O'
        ;;
    esac
  ;;
  *)
    objfmt='ELF ?'
  ;;
esac

AC_MSG_RESULT([$objfmt])
if test "$objfmt" = 'ELF ?'; then
  objfmt='ELF'
  AC_MSG_WARN([unexpected host system. assumed that the format is $objfmt.])
fi

AC_MSG_CHECKING([for object file format specifier (NAFLAGS) ])
case "$objfmt" in
  MSOMF)      NAFLAGS='-fobj -DOBJ32';;
  Win32-COFF) NAFLAGS='-fwin32 -DWIN32';;
  Win64-COFF) NAFLAGS='-fwin64 -DWIN64 -D__x86_64__';;
  COFF)       NAFLAGS='-fcoff -DCOFF';;
  a.out)      NAFLAGS='-faout -DAOUT';;
  BSD-a.out)  NAFLAGS='-faoutb -DAOUT';;
  ELF)        NAFLAGS='-felf -DELF';;
  ELF64)      NAFLAGS='-felf64 -DELF -D__x86_64__';;
  RDF)        NAFLAGS='-frdf -DRDF';;
  Mach-O)     NAFLAGS='-fmacho -DMACHO';;
  Mach-O64)   NAFLAGS='-fmacho64 -DMACHO -D__x86_64__';;
esac
AC_MSG_RESULT([$NAFLAGS])
AC_SUBST([NAFLAGS])

AC_MSG_CHECKING([whether the assembler ($NASM $NAFLAGS) works])
cat > conftest.asm <<EOF
[%line __oline__ "configure"
        section .text
        global  _main,main
_main:
main:   xor     eax,eax
        ret
]EOF
try_nasm='$NASM $NAFLAGS -o conftest.o conftest.asm'
if AC_TRY_EVAL(try_nasm) && test -s conftest.o; then
  AC_MSG_RESULT(yes)
else
  echo "configure: failed program was:" >&AC_FD_CC
  cat conftest.asm >&AC_FD_CC
  rm -rf conftest*
  AC_MSG_RESULT(no)
  AC_MSG_ERROR([installation or configuration problem: assembler cannot create object files.])
fi

AC_MSG_CHECKING([whether the linker accepts assembler output])
try_nasm='${CC-cc} -o conftest${ac_exeext} $LDFLAGS conftest.o $LIBS 1>&AC_FD_CC'
if AC_TRY_EVAL(try_nasm) && test -s conftest${ac_exeext}; then
  rm -rf conftest*
  AC_MSG_RESULT(yes)
else
  rm -rf conftest*
  AC_MSG_RESULT(no)
  AC_MSG_ERROR([configuration problem: maybe object file format mismatch.])
fi

])
