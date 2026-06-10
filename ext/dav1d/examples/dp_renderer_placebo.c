/*
 * Copyright © 2020, VideoLAN and dav1d authors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "dp_renderer.h"

#if HAVE_RENDERER_PLACEBO
#include <assert.h>

#include <libplacebo/renderer.h>
#include <libplacebo/utils/dav1d.h>

#if HAVE_PLACEBO_VULKAN
# include <libplacebo/vulkan.h>
# include <SDL_vulkan.h>
#endif
#if HAVE_PLACEBO_OPENGL
# include <libplacebo/opengl.h>
# include <SDL_opengl.h>
#endif


/**
 * Renderer context for libplacebo
 */
typedef struct renderer_priv_ctx
{
    // SDL window
    SDL_Window *win;
    // Placebo log
    pl_log log;
    // Placebo renderer
    pl_renderer renderer;
#if HAVE_PLACEBO_VULKAN
    // Placebo Vulkan handle
    pl_vulkan vk;
    // Placebo Vulkan instance
    pl_vk_inst vk_inst;
    // Vulkan surface
    VkSurfaceKHR surf;
#endif
#if HAVE_PLACEBO_OPENGL
    // Placebo OpenGL handle
    pl_opengl gl;
    // SDL OpenGL context
    SDL_GLContext gl_context;
#endif
    // Placebo GPU
    pl_gpu gpu;
    // Placebo swapchain
    pl_swapchain swapchain;
    // Lock protecting access to the texture
    SDL_mutex *lock;
    // Image to render, and planes backing them
    struct pl_frame image;
    pl_tex plane_tex[3];
} Dav1dPlayRendererPrivateContext;

static Dav1dPlayRendererPrivateContext*
    placebo_renderer_create_common(const Dav1dPlaySettings *settings, int window_flags)
{
    if (settings->fullscreen)
        window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

    // Create Window
    SDL_Window *sdlwin = dp_create_sdl_window(window_flags | SDL_WINDOW_RESIZABLE);
    if (sdlwin == NULL) {
        fprintf(stderr, "Creating SDL window failed: %s\n", SDL_GetError());
        return NULL;
    }

    SDL_ShowCursor(0);

    // Alloc
    Dav1dPlayRendererPrivateContext *const rd_priv_ctx =
        calloc(1, sizeof(Dav1dPlayRendererPrivateContext));
    if (rd_priv_ctx == NULL) {
        fprintf(stderr, "Out of memory!\n");
        return NULL;
    }
    rd_priv_ctx->win = sdlwin;

    // Init libplacebo
    rd_priv_ctx->log = pl_log_create(PL_API_VER, pl_log_params(
        .log_cb     = pl_log_color,
#ifndef NDEBUG
        .log_level  = PL_LOG_DEBUG,
#else
        .log_level  = PL_LOG_WARN,
#endif
    ));
    if (rd_priv_ctx->log == NULL) {
        fprintf(stderr, "pl_log_create failed!\n");
        free(rd_priv_ctx);
        return NULL;
    }

    // Create Mutex
    rd_priv_ctx->lock = SDL_CreateMutex();
    if (rd_priv_ctx->lock == NULL) {
        fprintf(stderr, "SDL_CreateMutex failed: %s\n", SDL_GetError());
        pl_log_destroy(&rd_priv_ctx->log);
        free(rd_priv_ctx);
        return NULL;
    }

    return rd_priv_ctx;
}

#if HAVE_PLACEBO_OPENGL
static void *placebo_renderer_create_gl(const Dav1dPlaySettings *settings)
{
    SDL_Window *sdlwin = NULL;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // Common init
    Dav1dPlayRendererPrivateContext *rd_priv_ctx =
        placebo_renderer_create_common(settings, SDL_WINDOW_OPENGL);

    if (rd_priv_ctx == NULL)
        return NULL;
    sdlwin = rd_priv_ctx->win;

    rd_priv_ctx->gl_context = SDL_GL_CreateContext(sdlwin);
    if (!rd_priv_ctx->gl_context) {
        fprintf(stderr, "Failed creating opengl context: %s\n", SDL_GetError());
        exit(2);
    }
    SDL_GL_MakeCurrent(sdlwin, rd_priv_ctx->gl_context);

    rd_priv_ctx->gl = pl_opengl_create(rd_priv_ctx->log, pl_opengl_params(
        .allow_software = true,
#ifndef NDEBUG
        .debug = true,
#endif
    ));
    if (!rd_priv_ctx->gl) {
        fprintf(stderr, "Failed creating opengl device!\n");
        exit(2);
    }

    rd_priv_ctx->swapchain = pl_opengl_create_swapchain(rd_priv_ctx->gl,
        pl_opengl_swapchain_params(
            .swap_buffers = (void (*)(void *)) SDL_GL_SwapWindow,
            .priv = sdlwin,
        ));

    if (!rd_priv_ctx->swapchain) {
        fprintf(stderr, "Failed creating opengl swapchain!\n");
        exit(2);
    }

    int w = WINDOW_WIDTH, h = WINDOW_HEIGHT;
    SDL_GL_GetDrawableSize(sdlwin, &w, &h);

    if (!pl_swapchain_resize(rd_priv_ctx->swapchain, &w, &h)) {
        fprintf(stderr, "Failed resizing vulkan swapchain!\n");
        exit(2);
    }

    rd_priv_ctx->gpu = rd_priv_ctx->gl->gpu;

    if (w != WINDOW_WIDTH || h != WINDOW_HEIGHT)
        printf("Note: window dimensions differ (got %dx%d)\n", w, h);

    return rd_priv_ctx;
}
#endif

#if HAVE_PLACEBO_VULKAN
static void *placebo_renderer_create_vk(const Dav1dPlaySettings *settings)
{
    SDL_Window *sdlwin = NULL;

    // Common init
    Dav1dPlayRendererPrivateContext *rd_priv_ctx =
        placebo_renderer_create_common(settings, SDL_WINDOW_VULKAN);

    if (rd_priv_ctx == NULL)
        return NULL;
    sdlwin = rd_priv_ctx->win;

    // Init Vulkan
    unsigned num = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(sdlwin, &num, NULL)) {
        fprintf(stderr, "Failed enumerating Vulkan extensions: %s\n", SDL_GetError());
        exit(1);
    }

    const char **extensions = malloc(num * sizeof(const char *));
    assert(extensions);

    SDL_bool ok = SDL_Vulkan_GetInstanceExtensions(sdlwin, &num, extensions);
    if (!ok) {
        fprintf(stderr, "Failed getting Vk instance extensions\n");
        exit(1);
    }

    if (num > 0) {
        printf("Requesting %d additional Vulkan extensions:\n", num);
        for (unsigned i = 0; i < num; i++)
            printf("    %s\n", extensions[i]);
    }

    rd_priv_ctx->vk_inst = pl_vk_inst_create(rd_priv_ctx->log, pl_vk_inst_params(
        .extensions = extensions,
        .num_extensions = num,
    ));
    if (!rd_priv_ctx->vk_inst) {
        fprintf(stderr, "Failed creating Vulkan instance!\n");
        exit(1);
    }
    free(extensions);

    if (!SDL_Vulkan_CreateSurface(sdlwin, rd_priv_ctx->vk_inst->instance, &rd_priv_ctx->surf)) {
        fprintf(stderr, "Failed creating vulkan surface: %s\n", SDL_GetError());
        exit(1);
    }

    rd_priv_ctx->vk = pl_vulkan_create(rd_priv_ctx->log, pl_vulkan_params(
        .instance = rd_priv_ctx->vk_inst->instance,
        .surface = rd_priv_ctx->surf,
        .allow_software = true,
    ));
    if (!rd_priv_ctx->vk) {
        fprintf(stderr, "Failed creating vulkan device!\n");
        exit(2);
    }

    // Create swapchain
    rd_priv_ctx->swapchain = pl_vulkan_create_swapchain(rd_priv_ctx->vk,
        pl_vulkan_swapchain_params(
            .surface = rd_priv_ctx->surf,
            .present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR,
        ));

    if (!rd_priv_ctx->swapchain) {
        fprintf(stderr, "Failed creating vulkan swapchain!\n");
        exit(2);
    }

    int w = WINDOW_WIDTH, h = WINDOW_HEIGHT;
    if (!pl_swapchain_resize(rd_priv_ctx->swapchain, &w, &h)) {
        fprintf(stderr, "Failed resizing vulkan swapchain!\n");
        exit(2);
    }

    rd_priv_ctx->gpu = rd_priv_ctx->vk->gpu;

    if (w != WINDOW_WIDTH || h != WINDOW_HEIGHT)
        printf("Note: window dimensions differ (got %dx%d)\n", w, h);

    return rd_priv_ctx;
}
#endif

static void placebo_renderer_destroy(void *cookie)
{
    Dav1dPlayRendererPrivateContext *rd_priv_ctx = cookie;
    assert(rd_priv_ctx != NULL);

    pl_renderer_destroy(&(rd_priv_ctx->renderer));
    pl_swapchain_destroy(&(rd_priv_ctx->swapchain));
    for (int i = 0; i < 3; i++)
        pl_tex_destroy(rd_priv_ctx->gpu, &(rd_priv_ctx->plane_tex[i]));

#if HAVE_PLACEBO_VULKAN
    if (rd_priv_ctx->vk) {
        pl_vulkan_destroy(&(rd_priv_ctx->vk));
        vkDestroySurfaceKHR(rd_priv_ctx->vk_inst->instance, rd_priv_ctx->surf, NULL);
        pl_vk_inst_destroy(&(rd_priv_ctx->vk_inst));
    }
#endif
#if HAVE_PLACEBO_OPENGL
    if (rd_priv_ctx->gl)
        pl_opengl_destroy(&(rd_priv_ctx->gl));
    if (rd_priv_ctx->gl_context)
        SDL_GL_DeleteContext(rd_priv_ctx->gl_context);
#endif

    SDL_DestroyWindow(rd_priv_ctx->win);

    pl_log_destroy(&rd_priv_ctx->log);
}

static void placebo_render(void *cookie, const Dav1dPlaySettings *settings)
{
    Dav1dPlayRendererPrivateContext *rd_priv_ctx = cookie;
    assert(rd_priv_ctx != NULL);

    SDL_LockMutex(rd_priv_ctx->lock);
    if (!rd_priv_ctx->image.num_planes) {
        SDL_UnlockMutex(rd_priv_ctx->lock);
        return;
    }

    // Prepare rendering
    if (rd_priv_ctx->renderer == NULL) {
        rd_priv_ctx->renderer = pl_renderer_create(rd_priv_ctx->log, rd_priv_ctx->gpu);
    }

    struct pl_swapchain_frame frame;
    bool ok = pl_swapchain_start_frame(rd_priv_ctx->swapchain, &frame);
    if (!ok) {
        SDL_UnlockMutex(rd_priv_ctx->lock);
        return;
    }

    struct pl_frame target;
    pl_frame_from_swapchain(&target, &frame);
    pl_rect2df_aspect_copy(&target.crop, &rd_priv_ctx->image.crop, 0.0);
    if (pl_frame_is_cropped(&target))
        pl_tex_clear(rd_priv_ctx->gpu, frame.fbo, (float[4]){ 0.0 });

    if (!pl_render_image(rd_priv_ctx->renderer, &rd_priv_ctx->image, &target,
                         settings->highquality ? &pl_render_default_params
                                               : &pl_render_fast_params))
    {
        fprintf(stderr, "Failed rendering frame!\n");
        pl_tex_clear(rd_priv_ctx->gpu, frame.fbo, (float[4]){ 1.0 });
    }

    ok = pl_swapchain_submit_frame(rd_priv_ctx->swapchain);
    if (!ok) {
        fprintf(stderr, "Failed submitting frame!\n");
        SDL_UnlockMutex(rd_priv_ctx->lock);
        return;
    }

    pl_swapchain_swap_buffers(rd_priv_ctx->swapchain);
    SDL_UnlockMutex(rd_priv_ctx->lock);
}

static int placebo_upload_image(void *cookie, Dav1dPicture *dav1d_pic,
                                const Dav1dPlaySettings *settings)
{
    Dav1dPlayRendererPrivateContext *p = cookie;
    assert(p != NULL);
    int ret = 0;

    if (!dav1d_pic)
        return ret;

    SDL_LockMutex(p->lock);
    if (!pl_upload_dav1dpicture(p->gpu, &p->image, p->plane_tex, pl_dav1d_upload_params(
        .picture = dav1d_pic,
        .film_grain = settings->gpugrain,
        .gpu_allocated = settings->zerocopy,
        .asynchronous = true,
    )))
    {
        fprintf(stderr, "Failed uploading planes!\n");
        p->image = (struct pl_frame) {0};
        ret = -1;
    }
    SDL_UnlockMutex(p->lock);
    return ret;
}

static int placebo_alloc_pic(Dav1dPicture *const pic, void *cookie)
{
    Dav1dPlayRendererPrivateContext *rd_priv_ctx = cookie;
    assert(rd_priv_ctx != NULL);

    SDL_LockMutex(rd_priv_ctx->lock);
    int ret = pl_allocate_dav1dpicture(pic, (void *) rd_priv_ctx->gpu);
    SDL_UnlockMutex(rd_priv_ctx->lock);
    return ret;
}

static void placebo_release_pic(Dav1dPicture *pic, void *cookie)
{
    Dav1dPlayRendererPrivateContext *rd_priv_ctx = cookie;
    assert(rd_priv_ctx != NULL);

    SDL_LockMutex(rd_priv_ctx->lock);
    pl_release_dav1dpicture(pic, (void *) rd_priv_ctx->gpu);
    SDL_UnlockMutex(rd_priv_ctx->lock);
}

#if HAVE_PLACEBO_VULKAN
const Dav1dPlayRenderInfo rdr_placebo_vk = {
    .name = "placebo-vk",
    .create_renderer = placebo_renderer_create_vk,
    .destroy_renderer = placebo_renderer_destroy,
    .render = placebo_render,
    .update_frame = placebo_upload_image,
    .alloc_pic = placebo_alloc_pic,
    .release_pic = placebo_release_pic,
    .supports_gpu_grain = 1,
};
#else
const Dav1dPlayRenderInfo rdr_placebo_vk = { NULL };
#endif

#if HAVE_PLACEBO_OPENGL
const Dav1dPlayRenderInfo rdr_placebo_gl = {
    .name = "placebo-gl",
    .create_renderer = placebo_renderer_create_gl,
    .destroy_renderer = placebo_renderer_destroy,
    .render = placebo_render,
    .update_frame = placebo_upload_image,
    .supports_gpu_grain = 1,
};
#else
const Dav1dPlayRenderInfo rdr_placebo_gl = { NULL };
#endif

#else
const Dav1dPlayRenderInfo rdr_placebo_vk = { NULL };
const Dav1dPlayRenderInfo rdr_placebo_gl = { NULL };
#endif
