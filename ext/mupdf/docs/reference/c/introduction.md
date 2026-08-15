# Introduction

This is the MuPDF C API guide for developers. In this document we will guide
you through the public and stable bits of the MuPDF library. This is intended
to both provide an introduction to new developers wanting to use low level
features of MuPDF, and as a reference guide to the public interface.

We will be explaining the basics, enough to get you started on a single
threaded application. There are more functions and data structures used
internally, and there are also ways to use MuPDF from multiple threads, but
those are beyond the scope of this document.

The functions and structures documented in this document are what we consider
stable APIs. They will rarely change, and if they do, any such changes will be
noted in the "api-changes" document along with instructions for how to update
your code.

## Core

The core contains the runtime context, exception handling, and various string
manipulation, math, hash table, binary tree, and other useful functions.

## I/O

The I/O module contains structures and functions for buffers of data, reading
and writing to streams, compression, and encryption.

## Graphics

The graphics module contains graphic resource objects like colors, fonts,
shadings, and images.

## Device

The device interface is how we provide access to the contents of a document. A
device is a callback structure, that gets called for each piece of text, line
art, and image on a page. There are several device implementations. The most
important one renders the contents to a raster image, and another one gathers
all the text into a structure that can be used to select and copy and search
the text content on a page.

## Document

The document module handles reading and writing documents in various formats,
and ties together all the preceding modules to provide rendering, format
conversion, and search functionality for the document types we support.

## PDF

The PDF module provides access to the low level PDF structure, letting you
query, modify, and create PDF objects and streams. It allows you to create new
documents, modify existing documents, or examine features, extract data, or do
almost anything you could want at the PDF object and stream level that we don't
provide with the higher level APIs.
