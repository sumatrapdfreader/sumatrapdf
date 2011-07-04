To build/debug android build.

1) Download the android sdk, and install it. On windows I unpacked it as:

   C:\Program Files\android-sdk-windows

on Macos as:

   /Library/android-sdk-mac_x86

On windows add: C:/Progra~1/android-sdk-windows/tools to your path.
On linux/macos add the equivalent.

2) Download the android ndk, and install in. On windows I unpacked it as:

   C:\Program Files\android-ndk-r5

on Macos as:

   /Library/android-ndk-r5

On windows add: C:/Progra~1/android-ndk-r5 to your path. On linux/macos
add the equivalent.

3) On windows, to use the ndk, you *must* be running under cygwin. This means
you need to install Cygwin 1.7 or greater now.

In the current release of the ndk (r5), when running under cygwin, there are
bugs to do with the automatic conversion of dependencies from DOS format
paths to cygwin format paths. The 2 fixes can be found in:

 <http://groups.google.com/group/android-ndk/msg/b385e47e1484c2d4>

4) Bring up a shell, and run 'android'. This will bring up a graphical
gui for the sdk. From here you can install the different SDK components
for the different flavours of android. Download them all - bandwidth and disk
space are cheap, right?

5) Now go to the Virtual Devices entry on the right hand side. You need to
create yourself an emulator image to use. Click 'New...' on the right hand
side and a window will appear. Fill in the entries as follows:

     Name: FroyoEm
     Target: Android 2.2 - API Level 8
     SD card: Size: 1024MiB
     Skin: Resolution: 480x756  (756 just fits my macbook screen, but 800 may
     	   	       		 be 'more standard')

Click 'Create AVD' and wait for a minute or so while it is prepared. Now
you can exit the GUI.

6) Now we are ready to build mupdf for Android. Check out a copy of MuPDF
(but you've done that already, cos you're reading this, right?).

7) You will also need a copy of mupdf-thirdparty.zip (see the source code
link on http://mupdf.com/). Unpack the contents of this into a 'thirdparty'
directory created within the mupdf directory (i.e. at the same level as
fitz, pdf, android etc).

8) Finally, you will need a copy of a 'generated' directory. This is not
currently available to download. The easiest way to obtain this is to do
a standard windows or linux build of mupdf, which generates the required
files as part of the build process.

9) Change into the android directory, and edit local.properties into your
favourite editor. Change the sdk path there as appropriate. This should be
the only bit of localisation you need to do.

10) Change into the android directory (note, the android directory, NOT
the android/jni directory!), and execute (in a Cygwin window on Windows!):

       ndk-build

This should build the native code portion. Then execute:

       ant debug

or on windows under cygwin:

       ant.bat debug

This should build the java wrapper.

11) Now start the emulator by executing:

       emulator -avd FroyoEm

This will take a while to full start up (be patient).

12) We now need to give the demo file something to chew on, so let's copy
a file into the SD card image of the emulator (this should only need to be
done once). With the emulator running type:

       adb push ../../MyTests/pdf_reference17.pdf /mnt/sdcard/Download/test.pdf

(where obviously ../../MyTests/pdf_reference17.pdf is altered for your
machine). (adb lives in <sdk>/platform-tools if it's not on your path).

13) With the emulator running (see step 11), execute

       ant install

('ant.bat install' on Windows) and that will copy MuPDF into the emulator
where you can run it from the launchpad screen.

14) To see debug messages from the emulator (including stdout/stderr from
our app), execute:

       adb logcat

Good luck!
