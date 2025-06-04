QT -= gui

TEMPLATE = lib
CONFIG += staticlib

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += ../../../include ../../../src

HEADERS += \
    ../../../include/lcms2.h \
    ../../../include/lcms2_plugin.h \
    ../../../src/lcms2_internal.h

SOURCES += \
    ../../../src/cmsalpha.c \
    ../../../src/cmscam02.c \
    ../../../src/cmscgats.c \
    ../../../src/cmscnvrt.c \
    ../../../src/cmserr.c \
    ../../../src/cmsgamma.c \
    ../../../src/cmsgmt.c \
    ../../../src/cmshalf.c \
    ../../../src/cmsintrp.c \
    ../../../src/cmsio0.c \
    ../../../src/cmsio1.c \
    ../../../src/cmslut.c \
    ../../../src/cmsmd5.c \
    ../../../src/cmsmtrx.c \
    ../../../src/cmsnamed.c \
    ../../../src/cmsopt.c \
    ../../../src/cmspack.c \
    ../../../src/cmspcs.c \
    ../../../src/cmsplugin.c \
    ../../../src/cmsps2.c \
    ../../../src/cmssamp.c \
    ../../../src/cmssm.c \
    ../../../src/cmstypes.c \
    ../../../src/cmsvirt.c \
    ../../../src/cmswtpnt.c \
    ../../../src/cmsxform.c 



# Default rules for deployment.
unix {
    target.path = $$[QT_INSTALL_PLUGINS]/generic
}
!isEmpty(target.path): INSTALLS += target
