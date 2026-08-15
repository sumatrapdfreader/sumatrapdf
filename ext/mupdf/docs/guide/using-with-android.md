# Using with Android

## Introduction

This document outlines the steps necessary to use the MuPDF Android Library in various ways.

First, we show you how to embed MuPDF in your app. Then, we explain how to
customize the viewer if you need to change how it looks or behaves. Finally, we
tell you where to go if you need to do work on the library itself.

Embedding the viewer in your app provides an activity that you can start to
view PDF documents from within your app. This should be enough for most use
cases.

## Acquire a valid license

### Open Source license

If your software is open source, you may use MuPDF under the terms of the
[GNU Affero General Public License](https://www.gnu.org/licenses/agpl-3.0.html).

This means that *all of the source code* for your *complete* app must be
released under a compatible open source license!

It also means that you may *not* use any proprietary closed source libraries or
components in your app. This includes (but is not limited to):

- Google Play Services
- Google Mobile Services
- AdMob by Google
- Crashlytics
- Answers
- *etc.*

Just because a library ships with Android or is made by Google does *not* make it AGPL compatible!

If you cannot or do not want to comply with these restrictions, you **must** acquire a commercial license instead.

### Commercial license

If your software is not open source, Artifex Software can sell you a license to use MuPDF in closed source software.

Fill out the
[MuPDF product inquiry form](https://artifex.com/contact/mupdf-inquiry.php?utm_source=rtd-mupdf&utm_medium=rtd&utm_content=inline-link)
for commercial licensing terms and pricing.

## Add the MuPDF Library to your project

The MuPDF library uses the Gradle build system.
In order to include MuPDF in your app, you also need to use Gradle.
The Eclipse and Ant build systems are not supported.

The MuPDF library needs Android version 4.1 or newer.
Make sure that the `minSdkVersion` in your app's `build.gradle` is at least 16.

	android {
		defaultConfig {
			minSdkVersion 16
			...
		}
		...
	}

The MuPDF library can be retrieved as a pre-built artifact from our Maven repository.
In your project's top `build.gradle`, add the line to the repositories section:

	allprojects {
		repositories {
			jcenter()
				maven { url 'http://maven.ghostscript.com' }
			...
		}
	}

Then add the MuPDF viewer library to your app's dependencies.
In your app's `build.gradle`, add the line to the dependencies section:

	dependencies {
		api 'com.artifex.mupdf:viewer:1.15.+'
		...
	}

## Invoke the document viewer activity

Once this has been done, you have access to the MuPDF viewer activity.
You can now open a document viewing activity by launching an `Intent`,
passing the URI of the document you wish to view.

	import com.artifex.mupdf.viewer.DocumentActivity;

	public void startMuPDFActivity(Uri documentUri) {
		Intent intent = new Intent(this, DocumentActivity.class);
		intent.setAction(Intent.ACTION_VIEW);
		intent.setData(documentUri);
		startActivity(intent);
	}

## How to customize the viewer

If you've already tried embedding the viewer in your app, but are unhappy with some
aspect of the look or behavior and want to modify it in some way, this section should
point you in the right direction.

### Decide which viewer to base your customizations on

In order to customize the viewer UI, you will need to modify the existing Android viewer activity.
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

### Check out the chosen project

When you have decided which project to base your modifications on, you should check out
the corresponding git repository:

	$ git clone git://git.ghostscript.com/mupdf-android-viewer.git
	$ git clone git://git.ghostscript.com/mupdf-android-viewer-mini.git

Inside the checked out project you will find two modules: `app` and `lib`.
The `app` module is a file chooser activity that lets the user open files from the external storage.
The `lib` module is the viewer activity, which provides the `com.artifex.mupdf:viewer`
package that you're already using.

The `lib` module is the one you want; ignore everything else in this project.

### Copy the viewer library module into your project

Copy the 'lib' directory to your project, renaming it to something appropriate.
The following instructions assume you called the directory 'mupdf-lib'.

Don't forget to include the module in the `settings.gradle` file:

	include ':app'
	include ':mupdf-lib'
	...

You'll also want to change your app's dependencies to now depend on your local
copy rather than the official MuPDF viewer package. In your app `build.gradle`:

	dependencies {
		api project(':mupdf-lib')
		...
	}

The `lib` module depends on the JNI library `com.artifex.mupdf:fitz`, so do
*not* remove the Maven repository from your top `build.gradle`.

### Edit the viewer activity

If all has gone well, you can now build your project with the local viewer library,
and access the MuPDF viewer activity just as you used to.

You're now free to customize the resources in `mupdf-lib/src/main/res` and behavior in
`mupdf-lib/src/main/java` as you desire.

## Working on the MuPDF Library

If you want to work on the library itself, rather than just use it, you will need
to check out the following git repositories.

- `mupdf.git`
	This repository contains the low-level "fitz" C library and the JNI bindings.

- `mupdf-android-fitz.git`
	This repository contains an Android project to build the C library and JNI bindings.
	It uses `mupdf.git` as a Git submodule.

- `mupdf-android-viewer.git`
	This repository contains the Android viewer library and app.
	It uses `mupdf-android-fitz.git` as either a Maven artifact or Git submodule.

- `mupdf-android-viewer-mini.git`
	This repository contains the minimalist Android viewer library and app.
	It uses `mupdf-android-fitz.git` as either a Maven artifact or Git submodule.

These repositories are set up with Git submodules. If you're a Git expert,
you can clone one of the viewer repositories recursively and get the fitz
and mupdf git repositories all at once.

If you only want to work with one of the viewer repositories, you can clone it
non-recursively and use the Maven artifact for the JNI bindings library and not
worry about the `mupdf.git` and `mupdf-android-fitz.git` repositories.
