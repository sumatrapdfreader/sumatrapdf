# The ARMv7 is significanly faster due to the use of the hardware FPU
APP_PLATFORM=android-8
APP_ABI := armeabi armeabi-v7a
ifdef NDK_PROFILER
# The profiler doesn't seem to receive ticks when run on release code.
# Accordingly, we need to build as debug - but this turns optimisations
# off, which is less than ideal. We COULD force them back on by using
# APP_CFLAGS = -O2, but this then triggers bugs in the compiler when it
# builds a couple of our source files. Accordingly, we have moved
# those files into Core2, and we have some flag hackery to make just that
# module without optimisation.
APP_OPTIM := debug
APP_CFLAGS :=
else
APP_OPTIM := release
endif
ifdef V8_BUILD
APP_STL := stlport_static
endif

# If the ndk is r8b then workaround bug by uncommenting the following line
#NDK_TOOLCHAIN_VERSION=4.4.3

# If the ndk is newer than r8c, try using clang.
#NDK_TOOLCHAIN_VERSION=clang3.1
