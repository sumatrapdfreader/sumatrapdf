/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

extern "C" {
#include <mupdf/fitz.h>
}

#include "PdfDarkMode.h"
#include "PdfDarkModeInternal.h"

// Engine-level Smart Dark caches (Phase 6). Access only under EngineMupdf::renderLock.

static constexpr i64 kMaxProcessedCacheBytes = 64 * 1024 * 1024;
static constexpr int kMaxFeatureEntries = 256;
static constexpr int kMaxProcessedEntries = 48;

struct DarkModeImageFeatureEntry {
    fz_image* image = nullptr;
    int w = 0;
    int h = 0;
    DarkImageFeatures features{};
    PixelColor estimatedBackground{};
    u64 lastAccess = 0;
};

struct DarkModeProcessedImageEntry {
    fz_image* srcImage = nullptr;
    int srcW = 0;
    int srcH = 0;
    u32 profileHash = 0;
    u8 policy = 0;
    u8 kind = 0;
    fz_image* processed = nullptr;
    i64 pixelBytes = 0;
    u64 lastAccess = 0;
};

struct DarkModeEngineCache {
    Vec<DarkModeImageFeatureEntry> features;
    Vec<DarkModeProcessedImageEntry> processed;
    i64 processedBytes = 0;
    u64 accessCounter = 0;
};

static bool dm_image_key_match(fz_image* a, int aw, int ah, fz_image* b, int bw, int bh) {
    return a == b && aw == bw && ah == bh;
}

static i64 dm_estimate_image_bytes(int w, int h) {
    if (w <= 0 || h <= 0) {
        return 0;
    }
    return (i64)w * h * 4;
}

DarkModeEngineCache* PdfDarkModeEngineCacheCreate() {
    return new DarkModeEngineCache();
}

static void dm_free_processed_entry(fz_context* ctx, DarkModeProcessedImageEntry& entry) {
    if (ctx && entry.processed) {
        fz_drop_image(ctx, entry.processed);
    }
    entry.processed = nullptr;
    entry.pixelBytes = 0;
}

void PdfDarkModeEngineCacheClear(fz_context* ctx, DarkModeEngineCache* cache) {
    if (!cache) {
        return;
    }
    for (DarkModeImageFeatureEntry& e : cache->features) {
        if (ctx && e.image) {
            fz_drop_image(ctx, e.image);
        }
    }
    cache->features.Clear();
    for (DarkModeProcessedImageEntry& e : cache->processed) {
        dm_free_processed_entry(ctx, e);
    }
    cache->processed.Clear();
    cache->processedBytes = 0;
    cache->accessCounter = 0;
}

void PdfDarkModeEngineCacheFree(fz_context* ctx, DarkModeEngineCache* cache) {
    if (!cache) {
        return;
    }
    PdfDarkModeEngineCacheClear(ctx, cache);
    delete cache;
}

static u64 dm_next_access(DarkModeEngineCache* cache) {
    return ++cache->accessCounter;
}

static void dm_evict_oldest_feature(DarkModeEngineCache* cache, fz_context* ctx) {
    if (len(cache->features) == 0) {
        return;
    }
    int oldestIdx = 0;
    u64 oldest = cache->features[0].lastAccess;
    for (int i = 1; i < len(cache->features); i++) {
        if (cache->features[i].lastAccess < oldest) {
            oldest = cache->features[i].lastAccess;
            oldestIdx = i;
        }
    }
    DarkModeImageFeatureEntry& e = cache->features[oldestIdx];
    if (ctx && e.image) {
        fz_drop_image(ctx, e.image);
    }
    cache->features.RemoveAt(oldestIdx);
}

static void dm_evict_oldest_processed(fz_context* ctx, DarkModeEngineCache* cache) {
    if (len(cache->processed) == 0) {
        return;
    }
    int oldestIdx = 0;
    u64 oldest = cache->processed[0].lastAccess;
    for (int i = 1; i < len(cache->processed); i++) {
        if (cache->processed[i].lastAccess < oldest) {
            oldest = cache->processed[i].lastAccess;
            oldestIdx = i;
        }
    }
    DarkModeProcessedImageEntry& e = cache->processed[oldestIdx];
    cache->processedBytes -= e.pixelBytes;
    dm_free_processed_entry(ctx, e);
    cache->processed.RemoveAt(oldestIdx);
}

bool PdfDarkModeEngineCacheLookupFeatures(DarkModeEngineCache* cache, fz_image* image, DarkImageFeatures* outFeatures,
                                          PixelColor* outBackground) {
    if (!cache || !image || !outFeatures || !outBackground) {
        return false;
    }
    int w = image->w;
    int h = image->h;
    for (DarkModeImageFeatureEntry& e : cache->features) {
        if (dm_image_key_match(e.image, e.w, e.h, image, w, h)) {
            e.lastAccess = dm_next_access(cache);
            *outFeatures = e.features;
            *outBackground = e.estimatedBackground;
            return true;
        }
    }
    return false;
}

void PdfDarkModeEngineCacheStoreFeatures(fz_context* ctx, DarkModeEngineCache* cache, fz_image* image,
                                         const DarkImageFeatures& features, const PixelColor& background) {
    if (!cache || !ctx || !image) {
        return;
    }
    int w = image->w;
    int h = image->h;
    for (DarkModeImageFeatureEntry& e : cache->features) {
        if (dm_image_key_match(e.image, e.w, e.h, image, w, h)) {
            e.features = features;
            e.estimatedBackground = background;
            e.lastAccess = dm_next_access(cache);
            return;
        }
    }
    while (len(cache->features) >= kMaxFeatureEntries) {
        dm_evict_oldest_feature(cache, ctx);
    }
    DarkModeImageFeatureEntry entry;
    entry.image = fz_keep_image(ctx, image);
    entry.w = w;
    entry.h = h;
    entry.features = features;
    entry.estimatedBackground = background;
    entry.lastAccess = dm_next_access(cache);
    cache->features.Append(entry);
}

fz_image* PdfDarkModeEngineCacheLookupProcessed(fz_context* ctx, DarkModeEngineCache* cache, fz_image* src,
                                                u32 profileHash, DarkImagePolicy policy, DarkImageKind kind) {
    if (!cache || !ctx || !src) {
        return nullptr;
    }
    int w = src->w;
    int h = src->h;
    u8 policyByte = (u8)policy;
    u8 kindByte = (u8)kind;
    for (DarkModeProcessedImageEntry& e : cache->processed) {
        if (e.profileHash == profileHash && e.policy == policyByte && e.kind == kindByte &&
            dm_image_key_match(e.srcImage, e.srcW, e.srcH, src, w, h) && e.processed) {
            e.lastAccess = dm_next_access(cache);
            return fz_keep_image(ctx, e.processed);
        }
    }
    return nullptr;
}

void PdfDarkModeEngineCacheStoreProcessed(fz_context* ctx, DarkModeEngineCache* cache, fz_image* src, u32 profileHash,
                                          DarkImagePolicy policy, DarkImageKind kind, fz_image* processed) {
    if (!cache || !ctx || !src || !processed) {
        return;
    }
    int w = src->w;
    int h = src->h;
    u8 policyByte = (u8)policy;
    u8 kindByte = (u8)kind;
    i64 bytes = dm_estimate_image_bytes(w, h);

    for (DarkModeProcessedImageEntry& e : cache->processed) {
        if (e.profileHash == profileHash && e.policy == policyByte && e.kind == kindByte &&
            dm_image_key_match(e.srcImage, e.srcW, e.srcH, src, w, h)) {
            cache->processedBytes -= e.pixelBytes;
            dm_free_processed_entry(ctx, e);
            e.processed = fz_keep_image(ctx, processed);
            e.pixelBytes = bytes;
            e.lastAccess = dm_next_access(cache);
            cache->processedBytes += bytes;
            return;
        }
    }

    while (cache->processedBytes + bytes > kMaxProcessedCacheBytes && len(cache->processed) > 0) {
        dm_evict_oldest_processed(ctx, cache);
    }
    while (len(cache->processed) >= kMaxProcessedEntries) {
        dm_evict_oldest_processed(ctx, cache);
    }

    DarkModeProcessedImageEntry entry;
    entry.srcImage = src;
    entry.srcW = w;
    entry.srcH = h;
    entry.profileHash = profileHash;
    entry.policy = policyByte;
    entry.kind = kindByte;
    entry.processed = fz_keep_image(ctx, processed);
    entry.pixelBytes = bytes;
    entry.lastAccess = dm_next_access(cache);
    cache->processed.Append(entry);
    cache->processedBytes += bytes;
}
