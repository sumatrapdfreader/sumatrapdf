To build/debug android build.

1) Download the android sdk, and install it. On windows I unpacked it as:

   C:\Program Files\android-sdk-windows

on Macos as:

   /Library/android-sdk-mac_x86

On windows add: C:/Progra~1/android-sdk-windows/tools to your path.
On linux/macos add the equivalent.

2) Download the android ndk, and install in. On windows I unpacked it as:

   C:\Program Files\android-ndk-r4b

on Macos as:

   /Library/android-ndk-r4b

On windows add: C:/Progra~1/android-ndk-r4b to your patyh. On linux/macos
add the equivalent.

3) On windows, to use the ndk, you *must* be running under cygwin. This means
you need to install Cygwin 1.7 or greater now.

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
(but you've done that already, cos you're reading this, right?). Get the
thirdparty package from mupdf.com and unpack that into mupdf/thirdparty.
Also get the pregen package from the same place and unpack that into
mupdf/pregen.

7) Load local.properties into your favourite editor, and edit the sdk
path there as appropriate. This should be the only bit of localisation
you need to do.

8) Change into the android directory, and execute (in a Cygwin window on
Windows!):

       ndk-build

This should build the native code portion. Then execute:

       ant debug

This should build the java wrapper.

9) Now start the emulator by executing:

       emulator -avd FroyoEm

This will take a while to full start up (be patient).

10) We now need to give the demo file something to chew on, so let's copy
a file into the SD card image of the emulator (this should only need to be
done once). With the emulator running type:

       adb push ../../MyTests/pdf_reference17.pdf /mnt/sdcard/Download/test.pdf

(where obviously ../../MyTests/pdf_reference17.pdf is altered for your
machine).

11) With the emulator running (see step 9), execute

       ant install

and that will copy MuPDF into the emulator where you can run it from the
launchpad screen.

12) To see debug messages from the emulator (including stdout/stderr from
our app), execute:

       adb logcat

Good luck!
