To build/debug android build.

1) Download the android sdk, and install it. These instructions have been
written with r14 (the latest version at time of writing) of the SDK in mind;
other versions may give problems. On windows r14 unpacked as:

   C:\Program Files (x86)\Android\android-sdk

on Macos an older version installed as:

   /Library/android-sdk-mac_x86

Whatever directory it unpacks to, ensure that both the 'tools' and
'platform-tools' directories inside it have been added to your PATH.

2) Download the android ndk, and unpack it. These instructions were written
with NDK r6b (the latest version at the time of writing) in mind, but the
build has now been tweaked to work with r8b. Other versions may give problems.
On windows I unpacked it as:

   C:\android-ndk-r8b

on Macos an older version unpacked as:

   /Library/android-ndk-r5

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
bandwidth and disk space are cheap, right?

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

9) Copy the android/local.properties.sample file to be
android/local.properties and edit the contents to match your setup.

10) You will also need a copy of mupdf-thirdparty.zip (see the source code
link on http://mupdf.com/). Unpack the contents of this into a 'thirdparty'
directory created within the mupdf directory (i.e. at the same level as
fitz, pdf, android etc).

11) Finally, you will need a copy of a 'generated' directory. This is not
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

12) Change into the android directory, and edit local.properties into your
favourite editor. Change the sdk path there as appropriate. This should be
the only bit of localisation you need to do.

13) Change into the android directory (note, the android directory, NOT
the android/jni directory!), and execute (in a Cygwin window on Windows!):

       ndk-build

This should build the native code portion.

If this dies with an error in thirdparty/jbig2/os_types.h load this
file into an editor, and change line 43 from:

    #else

to

    #elif !defined(HAVE_STDINT_H)

and this should solve the problem.

14) Then execute:

       ant debug

or on windows under cygwin:

       ant.bat debug

This should build the java wrapper.

15) Now start the emulator by executing:

       emulator -avd FroyoEm

This will take a while to full start up (be patient).

16) We now need to give the demo file something to chew on, so let's copy
a file into the SD card image of the emulator (this should only need to be
done once). With the emulator running type:

       adb push ../../MyTests/pdf_reference17.pdf /mnt/sdcard/Download/test.pdf

(where obviously ../../MyTests/pdf_reference17.pdf is altered for your
machine, and  under Windows, should start c:/ even if invoked from cygwin)
(adb lives in <sdk>/platform-tools if it's not on your path).

17) With the emulator running (see step 14), execute

       ant debug install

('ant.bat debug install' on Windows) and that will copy MuPDF into the
emulator where you can run it from the launchpad screen.

18) To see debug messages from the emulator (including stdout/stderr from
our app), execute:

       adb logcat

Good luck!
