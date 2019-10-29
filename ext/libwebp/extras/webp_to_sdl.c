// Copyright 2017 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
//  Simple WebP-to-SDL wrapper. Useful for emscripten.
//
// Author: James Zern (jzern@google.com)

#ifdef HAVE_CONFIG_H
#include "src/webp/config.h"
#endif

#if defined(WEBP_HAVE_SDL)

#include "webp_to_sdl.h"

#include <stdio.h>
#include "src/webp/decode.h"

#if defined(WEBP_HAVE_JUST_SDL_H)
#include <SDL.h>
#else
#include <SDL/SDL.h>
#endif

static int init_ok = 0;
int WebpToSDL(const char* data, unsigned int data_size) {
  int ok = 0;
  VP8StatusCode status;
  WebPDecoderConfig config;
  WebPBitstreamFeatures* const input = &config.input;
  WebPDecBuffer* const output = &config.output;
  SDL_Surface* screen = NULL;
  SDL_Surface* surface = NULL;

  if (!WebPInitDecoderConfig(&config)) {
    fprintf(stderr, "Library version mismatch!\n");
    return 0;
  }

  if (!init_ok) {
    SDL_Init(SDL_INIT_VIDEO);
    init_ok = 1;
  }

  status = WebPGetFeatures((uint8_t*)data, (size_t)data_size, &config.input);
  if (status != VP8_STATUS_OK) goto Error;

  screen = SDL_SetVideoMode(input->width, input->height, 32, SDL_SWSURFACE);
  if (screen == NULL) {
    fprintf(stderr, "Unable to set video mode (32bpp %dx%d)!\n",
            input->width, input->height);
    goto Error;
  }

  surface = SDL_CreateRGBSurface(SDL_SWSURFACE,
                                 input->width, input->height, 32,
                                 0x000000ffu,   // R mask
                                 0x0000ff00u,   // G mask
                                 0x00ff0000u,   // B mask
                                 0xff000000u);  // A mask

  if (surface == NULL) {
    fprintf(stderr, "Unable to create %dx%d RGBA surface!\n",
            input->width, input->height);
    goto Error;
  }
  if (SDL_MUSTLOCK(surface)) SDL_LockSurface(surface);

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
  output->colorspace = MODE_BGRA;
#else
  output->colorspace = MODE_RGBA;
#endif
  output->width  = surface->w;
  output->height = surface->h;
  output->u.RGBA.rgba   = surface->pixels;
  output->u.RGBA.stride = surface->pitch;
  output->u.RGBA.size   = surface->pitch * surface->h;
  output->is_external_memory = 1;

  status = WebPDecode((const uint8_t*)data, (size_t)data_size, &config);
  if (status != VP8_STATUS_OK) {
    fprintf(stderr, "Error decoding image (%d)\n", status);
    goto Error;
  }

  if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
  if (SDL_BlitSurface(surface, NULL, screen, NULL) ||
      SDL_Flip(screen)) {
    goto Error;
  }

  ok = 1;

 Error:
  SDL_FreeSurface(surface);
  SDL_FreeSurface(screen);
  WebPFreeDecBuffer(output);
  return ok;
}

//------------------------------------------------------------------------------

#endif  // WEBP_HAVE_SDL
