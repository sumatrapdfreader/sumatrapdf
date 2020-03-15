/*
    WDL - rng.h
    Copyright (C) 2005 and later, Cockos Incorporated
   
    This software is provided 'as-is', without any express or implied
    warranty.  In no event will the authors be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
       claim that you wrote the original software. If you use this software
       in a product, an acknowledgment in the product documentation would be
       appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be
       misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.
    
*/

/*

  This header provides the interface to a decent random number generator, 
  that internally uses a 256-bit state, and SHA-1 to iterate. We wouldn't consider
  this RNG to be cryptographically secure, but it may be decent.

*/

#ifndef _WDL_RNG_H_
#define _WDL_RNG_H_


void WDL_RNG_addentropy(void *buf, int buflen); // add entropy to the RNG

int WDL_RNG_int32(); // get a random integer
void WDL_RNG_bytes(void *buf, int buflen); // get a random string of bytes


#endif

