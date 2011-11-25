/** \file crengine.h
    \brief CREngine main include file

    Include this file to use CR engine.

    (c) Vadim Lopatin, 2000-2008

    This source code is distributed under the terms of
    GNU General Public License.

    See LICENSE file for details.
*/

#ifndef CRENGINE_H_INCLUDED
#define CRENGINE_H_INCLUDED

/**
    \mainpage CoolReader Engine Library
    \author Vadim Lopatin
    \date 2000-2008

    \section main_intro Introduction

    CoolReader Engine is a XML/CSS based 
    visualization library for writing e-book readers.

    The goal is to write fast, compact and portable library
    which allows to create e-book readers for different platform
    including handheld devices with limited resources.
    
    This library is partially based on CoolReader2 e-book reader, 
    but most parts are rewritten from scratch.

    \b Features:

    - Different font engines support
        - grayscale bitmap font engine
        - Win32 font support
        - TTF fonts support via Freetype library
    - Text formatter with support of different paragraph and font styles, 
        which allows to prepare text to be drawed
    - XML parser with support of unicode (UTF-8 and UTF-16) and 8-bit encodings
    - 2 different DOM tree implementations
        - Tiny DOM - compact readonly DOM tree
            - doesn't use RAM to store document text but reads it from file by demand
            - doesn't store formatted text in memory, but generates it on the fly
            - parsed tree can be saved to file to allow fast re-opening of documents
            - compact tree structure requires minimum amount of RAM
            - optimized element names, attribute names and values string storing
        - Fast DOM (in progress, not included to distribution) - fast but compact read/write tree
            - editable document tree
            - faster implementation
            - optimized element names, attribute names and values string storing
    - Styles: CSS2 subset implementation
        - only simple selectors ( element-name or universal selector * )
        - definition for properties
            - display
            - white-space
            - text-align
            - vertical-align
            - font-family
            - font-size
            - font-style
            - font-weight
            - text-indent
            - line-height
            - width
            - height
            - margin-left
            - margin-right
            - margin-top
            - margin-bottom
            - margin
            - page-break-before
            - page-break-after
            - page-break-inside
    - DOM/CSS formatter allows to prepare document for drawing
    - Sample applications
        - FB2 e-book reader for Windows
        - FB2 e-book reader for X (Linux)
    - Tools
        - TrueType to grayscale bitmap font convertor



    \section main_authors Authors

    - Vadim Lopatin (http://www.coolreader.org/) - most source code
    - Alan (http://alreader.kms.ru/) - hyphenation sypport code
    

    \section main_install Installation

    - download source code from CoolReader homepage http://www.coolreader.org/
    - unpack archieve into some folder
    - change options in crsetup.h file if necessary
    - build library
    - build sample applications located in /tools folder

    \note current version supports only build under Win32 with MS VC++ 6.0


    \section getting_started Getting started


    Please see Tools/Fb2Test/Fb2Test.cpp source code for sample code.

    Library implements \a LVDocView class which can read XML document from 
    file and draw it in grayscale buffer.

    Before loading of document, you have to initialize font manager:

    \b InitFontManager( lString8() );

    Please register fonts you want to make available using call of RegisterFont method. 
    For bitmap font manager, parameter is filename of bitmap font.

    \b fontMan->RegisterFont( lString8(fn) );

    Typical usage of LVDocView:
    - Load document using LVDocView::LoadDocument() method. 
    - Call LVDocView::setStyleSheet() to set stylesheet for document.
    - Set draw buffer dimensions using LVDocView::Resize(dx, dy).
    - LVDocView::Draw() draws document into gray buffer. 
    - \a DrawBuf2DC() can be used to draw gray bitmap in Windows device context.
    - LVDocView::GetPos() and LVDocView::SetPos() can be used to scroll throuh document.



    \section main_license License


    This source code is distributed under the terms of
    GNU General Public License.

    See LICENSE file for details.
    

*/

#include "crsetup.h"
#include "lvtypes.h"
#include "lvref.h"
#include "lvstring.h"
#include "lvarray.h"
#include "lvbmpbuf.h"
#include "lvfntman.h"
#include "lvstyles.h"
#include "lvdocview.h"
#include "lvstsheet.h"
#include "lvdrawbuf.h"
#include "props.h"
#include "w32utils.h"



#endif//CRENGINE_H_INCLUDED
