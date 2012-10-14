# The ARMv7 is significanly faster due to the use of the hardware FPU
APP_PLATFORM=android-8
APP_ABI := armeabi armeabi-v7a
APP_OPTIM := debug
ifdef V8_BUILD
APP_STL := stlport_static
endif

# Workaround bug in the ndk
NDK_TOOLCHAIN_VERSION=4.4.3
