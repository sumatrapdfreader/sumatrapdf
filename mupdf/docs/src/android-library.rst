.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js

.. include:: header.rst

.. meta::
   :description: MuPDF documentation
   :keywords: MuPDF, pdf, epub


Android Library
===============================





Introduction
-------------------------------------

This document outlines the steps necessary to use the :title:`MuPDF Android Library` in various ways.

First, we show you how to embed :title:`MuPDF` in your app. Then, we explain how to
customize the viewer if you need to change how it looks or behaves. Finally, we
tell you where to go if you need to do work on the library itself.

Embedding the viewer in your app provides an activity that you can start to
view :title:`PDF` documents from within your app. This should be enough for most use
cases.


Acquire a valid license
-------------------------



Open Source license
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


If your software is open source, you may use :title:`MuPDF` under the terms of the `GNU Affero General Public License`_.


This means that *all of the source code* for your *complete* app must be
released under a compatible open source license!

It also means that you may *not* use any proprietary closed source libraries or
components in your app. This includes (but is not limited to):

- Google Play Services
- Google Mobile Services
- AdMob by Google
- Crashlytics
- Answers
*etc.*

Just because a library ships with :title:`Android` or is made by :title:`Google` does *not* make it :title:`AGPL` compatible!


If you cannot or do not want to comply with these restrictions,
you **must** acquire a commercial license instead.


Commercial license
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If your software is not open source, :title:`Artifex Software` can sell you a license to use :title:`MuPDF` in closed source software.

Fill out the `MuPDF product inquiry form`_ for commercial licensing terms and pricing.


Add the :title:`MuPDF` Library to your project
---------------------------------------------------------------------------

The :title:`MuPDF` library uses the :title:`Gradle` build system.
In order to include :title:`MuPDF` in your app, you also need to use :title:`Gradle`.
The :title:`Eclipse` and :title:`Ant` build systems are not supported.


The :title:`MuPDF` library needs :title:`Android` version 4.1 or newer.
Make sure that the `minSdkVersion` in your app's `build.gradle` is at least 16.


.. code-block::

   android {
      defaultConfig {
         minSdkVersion 16
         ...
      }
      ...
   }


The :title:`MuPDF` library can be retrieved as a pre-built artifact from our :title:`Maven` repository.
In your project's top `build.gradle`, add the line to the repositories section:


.. code-block::

   allprojects {
      repositories {
         jcenter()
         maven { url 'http://maven.ghostscript.com' }
         ...
      }
   }


Then add the :title:`MuPDF` viewer library to your app's dependencies.
In your app's `build.gradle`, add the line to the dependencies section:

.. code-block::

   dependencies {
      api 'com.artifex.mupdf:viewer:1.15.+'
      ...
   }


Invoke the document viewer activity
-------------------------------------


Once this has been done, you have access to the :title:`MuPDF` viewer activity.
You can now open a document viewing activity by launching an `Intent`,
passing the `URI` of the document you wish to view.


.. code-block:: java

   import com.artifex.mupdf.viewer.DocumentActivity;

   public void startMuPDFActivity(Uri documentUri) {
      Intent intent = new Intent(this, DocumentActivity.class);
      intent.setAction(Intent.ACTION_VIEW);
      intent.setData(documentUri);
      startActivity(intent);
   }


How to customize the viewer
----------------------------

If you've already tried embedding the viewer in your app, but are unhappy with some
aspect of the look or behavior and want to modify it in some way, this section should
point you in the right direction.


Decide which viewer to base your customizations on
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In order to customize the viewer UI, you will need to modify the existing :title:`Android` viewer activity.
There are two separate code bases you can start with:

- `mupdf-android-viewer`
   The main viewer app. This code is difficult to work with, but has the most
   features and pre-renders neighboring pages into a page cache for faster page
   turning performance.

- `mupdf-android-viewer-mini`
   This is a minimalist viewer which has fewer features but is designed to be
   easy to understand and modify. It does not (currently) have high-resolution
   zooming, and it does not use the swipe gesture to flip pages (it requires the
   user to tap on the side of the screen to flip pages).

If all you want to do is brand the UI with your own colors and icons, you are
welcome to use whichever code base you prefer. However, if you want to do
extensive modifications, we suggest you base your code on the mini viewer.


Check out the chosen project
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


When you have decided which project to base your modifications on, you should check out
the corresponding :title:`git` repository:


.. code-block:: bash

   $ git clone git://git.ghostscript.com/mupdf-android-viewer.git
   $ git clone git://git.ghostscript.com/mupdf-android-viewer-mini.git



Inside the checked out project you will find two modules: `app` and `lib`.
The `app` module is a file chooser activity that lets the user open files from the external storage.
The `lib` module is the viewer activity, which provides the `com.artifex.mupdf:viewer`
package that you're already using.

The `lib` module is the one you want; ignore everything else in this project.


Copy the viewer library module into your project
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Copy the 'lib' directory to your project, renaming it to something appropriate.
The following instructions assume you called the directory 'mupdf-lib'.

Don't forget to include the module in the `settings.gradle` file:


.. code-block::

   include ':app'
   include ':mupdf-lib'
   ...



You'll also want to change your app's dependencies to now depend on your local
copy rather than the official :title:`MuPDF` viewer package. In your app `build.gradle`:


.. code-block::

   dependencies {
      api project(':mupdf-lib')
      ...
   }


The `lib` module depends on the :title:`JNI` library `com.artifex.mupdf:fitz`, so do
*not* remove the :title:`Maven` repository from your top `build.gradle`.




Edit the viewer activity
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If all has gone well, you can now build your project with the local viewer library,
and access the :title:`MuPDF` viewer activity just as you used to.

You're now free to customize the resources in `mupdf-lib/src/main/res` and behavior in
`mupdf-lib/src/main/java` as you desire.



Working on the MuPDF Library
-----------------------------------


If you want to work on the library itself, rather than just use it, you will need
to check out the following :title:`git` repositories.



- `mupdf.git`
   This repository contains the low-level "fitz" :title:`C` library and the :title:`JNI` bindings.

- `mupdf-android-fitz.git`
   This repository contains an :title:`Android` project to build the :title:`C` library and :title:`JNI` bindings.
   It uses `mupdf.git` as a :title:`Git` submodule.

- `mupdf-android-viewer.git`
   This repository contains the :title:`Android` viewer library and app.
   It uses `mupdf-android-fitz.git` as either a :title:`Maven` artifact or :title:`Git` submodule.

- `mupdf-android-viewer-mini.git`
   This repository contains the minimalist :title:`Android` viewer library and app.
   It uses `mupdf-android-fitz.git` as either a :title:`Maven` artifact or :title:`Git` submodule.

- `mupdf-android-viewer-old.git`
   This repository contains the old :title:`Android` viewer. It has its own :title:`JNI`
   bindings and uses `mupdf.git` as a submodule directly. It is only listed here for
   history.



Since these repositories are set up as :title:`Git` submodules, if you're a :title:`Git` expert,
you can clone one of the viewer repositories recursively and get all of them at
once. However, working with :title:`Git` submodules can be fraught with danger, so it
may be easier to check them out individually.


If you only want to work with one of the viewer repositories, you can use the
:title:`Maven` artifact for the :title:`JNI` bindings library and not worry about the `mupdf.git`
and `mupdf-android-fitz.git` repositories.




.. include:: footer.rst



.. External links

.. _GNU Affero General Public License: https://www.gnu.org/licenses/agpl-3.0.html

.. _MuPDF product inquiry form: https://artifex.com/contact/mupdf-inquiry.php?utm_source=rtd-mupdf&utm_medium=rtd&utm_content=inline-link
