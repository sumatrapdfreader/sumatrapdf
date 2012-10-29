To build/debug android build.

1) Download the android sdk, and install it. These instructions have been
written with r14 (the latest version at time of writing) of the SDK in mind;
other versions may give problems. On windows r14 unpacked as:

   C:\Program Files (x86)\Android\android-sdk

on Macos an older version installed as:

   /Library/android-sdk-mac_x86

on Linux install it as:

   mkdir ~/android-sdk
   cd ~/android-sdk
   tar ~/Downloads/android-sdk_r20.0.3-linux.tgz

Whatever directory it unpacks to, ensure that both the 'tools' and
'platform-tools' directories inside it have been added to your PATH.

2) Download the android ndk, and unpack it. These instructions were written
with NDK r6b (the latest version at the time of writing) in mind, but the
build has now been tweaked to work with r8b. Other versions may give problems.
On windows I unpacked it as:

   C:\android-ndk-r8b

on Macos an older version unpacked as:

   /Library/android-ndk-r5

on Linux as:

   mkdir ~/android-ndk
   cd ~/android-ndk
   tar jxvf ~/Downloads/android-ndk-r8b-linux-x86.tar.bz2

It is very important that you should unpack it to a directory with no
spaces in the name! (Don't be tempted to put it in C:\Program Files etc)

Ensure that that directory is also added to your PATH.

3) On windows, to use the ndk, you *must* be running under cygwin. This means
you need to install Cygwin 1.7 or greater now.

[ In version r5 of the ndk, when running under cygwin, there were    ]
[ bugs to do with the automatic conversion of dependencies from DOS  ]
[ format paths to cygwin format paths. The 2 fixes can be found in:  ]
[                                                                    ]
[  <http://groups.google.com/group/android-ndk/msg/b385e47e1484c2d4> ]
[                                                                    ]
[ Use the latest version and there should not be a problem.          ]

4) If the SDK has not popped up a window already, bring up a shell, and run
'android' (or android.bat on cygwin/windows). You should now have a window
with a graphical gui for the sdk. From here you can install the different SDK
components for the different flavours of android. Download them all -
bandwidth and disk space are cheap, right? Make sure you get at least
the API level 11 as this is the current dependency for mupdf.

5) In new versions of the GUI there is a 'Tools' menu from which you can
select 'Manage AVDs...'. In old versions, go to the Virtual Devices entry
on the right hand side. You need to create yourself an emulator image to
use. Click 'New...' on the right hand side and a window will appear. Fill
in the entries as follows:

     Name: FroyoEm
     Target: Android 2.2 - API Level 8
     CPU/ABI: ARM (armeabi)     (If this option exists)
     SD card: Size: 1024MiB
     Skin: Resolution: 480x756  (756 just fits my macbook screen, but 800 may
     	   	       		 be 'more standard')

Click 'Create AVD' (on old versions you may have to wait for a minute or
so while it is prepared. Now you can exit the GUI.

6) You will need a copy of the JDK installed. See
<http://www.oracle.com/technetwork/java/javase/downloads/>. When this
installs, ensure that JAVA_HOME is set to point to the installation
directory.

7) You will need a copy of Apache ANT installed.
See <http://ant.apache.org/>. Ensure that ANT_HOME is set to point to
the top level directory, and that ANT_HOME/bin is on the PATH.

8) Now we are ready to build mupdf for Android. Check out a copy of MuPDF
(but you've done that already, cos you're reading this, right?).

9) You will also need a copy of mupdf's thirdparty libraries. If you are
using git, make sure to do a git submodule update --init from the top of
the build tree. Older versions packaged this source code in a .zip-file
(see the source code link on http://mupdf.com/). Unpack the contents of
this into a 'thirdparty' directory created within the mupdf directory
(i.e. at the same level as fitz, pdf, android etc).

10) Finally, you will need a copy of a 'generated' directory. This is not
currently available to download.

The normal mupdf build process involves running some code on the host
(the machine on which you are compiling), rather than the target (the
machine/device on which you eventually want to run mupdf). This code
repacks various bits of information (fonts, CMAPs etc) into a more
compact and usable form.

Unfortunately, the android SDK does not provide a compiler for the host
machine, so we cannot run this step automatically as part of the android
build. You will need to generate it by running a different build, such
as the windows or linux native builds.

We do not make a snapshot of the generated directory available to
download as the contents of this directory change frequently, and we'd
have to keep multiple versions on the website. We assume that anyone
capable of building for android is capable of doing a normal hosted
build.

On windows (where you are using cygwin), or on linux/macos, this can be
as simple as running 'make' in the top level directory. Even if the
make process fails, it should get far enough to generate you the required
'generated' directory, and you can continue through these instructions.

11) Change into mupdf's android directory. Copy the
android/local.properties.sample file to be android/local.properties and
change the sdk path there as appropriate. This should be the only bit of
localisation you need to do.

12) Change into the android directory (note, the android directory, NOT
the android/jni directory!), and execute (in a Cygwin window on Windows!):

       ndk-build

This should build the native code portion.

If this dies with an error in thirdparty/jbig2/os_types.h load this
file into an editor, and change line 43 from:

    #else

to

    #elif !defined(HAVE_STDINT_H)

and this should solve the problem.

13) Then execute:

       ant debug

or on windows under cygwin:

       ant.bat debug

This should build the java wrapper.

14) Now start the emulator by executing:

       emulator -avd FroyoEm

This will take a while to full start up (be patient).

15) We now need to give the demo file something to chew on, so let's copy
a file into the SD card image of the emulator (this should only need to be
done once). With the emulator running type:

       adb push ../../MyTests/pdf_reference17.pdf /mnt/sdcard/Download/test.pdf

(where obviously ../../MyTests/pdf_reference17.pdf is altered for your
machine, and  under Windows, should start c:/ even if invoked from cygwin)
(adb lives in <sdk>/platform-tools if it's not on your path).

16) With the emulator running (see step 14), execute

       ant debug install

('ant.bat debug install' on Windows) and that will copy MuPDF into the
emulator where you can run it from the launchpad screen.

17) To see debug messages from the emulator (including stdout/stderr from
our app), execute:

       adb logcat

Good luck!

Forms support
~~~~~~~~~~~~~

To build with PDF forms support, the only change is to the ndk-build stage.
Run:

	V8_BUILD=yes ndk-build

The build will need v8 headers and libraries to be present in the thirdparty
directory. The files assumed are:

	thirdparty/v8-3.9/android/libv8_base.a
	thirdparty/v8-3.9/android/libv8_snapshot.a
	thirdparty/v8-3.9/include/v8.h
	thirdparty/v8-3.9/include/v8stdint.h

