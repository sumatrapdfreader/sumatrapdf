What is ucrt?
-------------

ucrt (micro crt) is a BSD-licensed library for creating smaller
Windows executables. It does that by implementing a smaller crt
library than the one included with Visual Studio.

It's a work in progress. It works for some programs, it might not work
for others. It has only been tried with 32bit programs. I'm unclear if the
assembly code needs to be rewritten for 64bit or if 32bit version will compile
for 64bit.

How much can you save?
-------------------------

Using ucrt on an example program of mine shaved ~60kB from final
executable size. It might be different for you.

How to use it my program?
-------------------------

Crt stands for C runtime and implements the C standard library functions
like malloc(), printf() etc. as well as low-level support for C++, like
exception handling, i.e. http://msdn.microsoft.com/en-us/library/634ca0c2.aspx

When you compile a C/C++ program with Visual Studio, you have 2 choices:
a) dynamically link with its crt dll and ship the whole dll with your
  program (msvcr100.dll that comes with Visual Studio 10 is 756 KB)
b) statically link the crt code that you use

If you use option a), ucrt doesn't help. You have to statically link
ucrt code. What's more you have to tell the linker to not link the
Visual Studio's crt implementation.

TODO: more info

Theory of operation
-------------------

For more detailed explanations, read the following explanations:
* http://msdn.microsoft.com/library/bb985746.aspx
* http://drdobbs.com/windows/184416623
* http://kobyk.wordpress.com/2007/07/20/dynamically-linking-with-msvcrtdll-using-visual-c-2005/
* http://in4k.untergrund.net/various%20web%20articles/Creating_Small_Win32_Executables_-_Fast_Builds.htm
* http://blog.koroirc.com/2008/09/trimming-executables-part-1-getting-rid-of-the
-c-runtime/

We do what they describe i.e. re-implement crt functions. If equivalent
function already exists in msvcrt.dll or ntdll.dll, we just redirect
to it. If it doesn't, we do a minimal implementation.

How come our code is smaller than Visual Studio's? Because size is what
we focus on.

Other projects like this
------------------------

There are other projects like this:
* http://code.google.com/p/omaha/source/browse/#svn%2Ftrunk%2Fthird_party%2Fminicrt
  (minicrt, a part of Google's omaha updater for windows)
* http://synesis.com.au/software/cruntiny/
* http://f4b24.googlecode.com/svn/trunk/extra/smartvc9/

Why did we choose write another one?

The big difference is that everything is in source code. Others often
rely on having the original *.obj files.

Dev notes
---------

* other places that I might steal the code from:
 - http://llvm.org/svn/llvm-project/compiler-rt/trunk/
 - http://www.jbox.dk/sanos/source/
 - http://code.google.com/p/ontl/source/browse/trunk/ntl/rtl/eh.cpp
   implements __CxxFrameHandler3 and other exceptions support
 - http://codesearch.google.com/#XAzRy8oK4zA/libm/src/e_hypotf.c&ct=rc&cd=3&q=hypotf&l=24
   hypotf code from android's bionic C library, also might contain
   other useful code
 - Visual Studio crt code is included under Vc\crt\src directory. Useful
   for figuring out some things
 - http://www.openrce.org/articles/full_view/23 has some info that might be
   helpful figuring out what 'eh vector constructor iterator' is and how
   to implement it
 - http://code.google.com/p/wceccrtrtti, might help implementing
   "eh vector constructor iterator" 
   (http://code.google.com/p/wceccrtrtti/source/browse/trunk/wce_ccrtrtti/ehvec.cpp)
* http://msinilo.pl/blog/?p=719 info about vector constructor iterator, also
  http://stackoverflow.com/questions/1025313/c-will-an-empty-destructor-do-the-
same-thing-as-the-generated-destructor/2322314#2322314
* http://code.google.com/p/tinyrss/source/browse/tiny/tcrt.cpp - another minimal crt
* http://reactos-mirror.googlecode.com/svn/trunk/reactos/lib/sdk/crt/except/cppexcept.c
  http://reactos-mirror.googlecode.com/svn/trunk/reactos/lib/sdk/crt/except/except.c
  look but don't copy (GPL)
* https://github.com/7shi/minix-tools/tree/master/lib/c - maybe there's code to reuse

TODO
----
* implement tls support (needed by NoFreeAllocator.cpp)
* write a test program that calls all the functions we claim to
  implement and verifies they work as expected
* test on XP SP2 (maybe earlier). I develop on Win 7 and some functions
  present in msvcrt.dll in win 7 might not be available on XP (like
  e.g. __ftol2_sse). Thankfuly when that happens the error is obvious:
  the OS tells us that function foo is not present when we launch the
  executable
* a mystery: on my win7 64bit thinkpad, according to dumpbin /exports,
  ntdll.dll doesn't have _alldiv() and yet everything works. Why
  isn't it there (it is on my other win7 32bit vm)? What gets
  called in et.exe?
* implement _access, _waccess (was used in Sumatra)
