/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Password-encrypted OFD packages (GM/T 0099). Encryptions.xml names a decryptseed.dat
// with one UserInfo per user and an entriesmap.dat. A password user's EncryptedWK is the
// file key encrypted with KDF_SM3(password); entriesmap.dat (encrypted with the file key,
// or plain) maps each original member to its SM4-CBC encrypted copy.
//
//   Encryptions.xml -> decryptseed.dat: UserInfo { IVValue, EncryptedWK }
//                   -> entriesmap.dat:  EncryptEntry Path="/OFD.xml" EPath="/OFD.dat"

#include "mupdf/fitz.h"
#include "ofd-crypto.h"
#include "ofd-decrypt.h"

enum {
    OFD_DECRYPT_MAX_LAYERS = 4,
    OFD_DECRYPT_PATH = 4096
};

//--- an in-memory archive that can list its members

typedef struct {
    fz_archive super;
    int count;
    int cap;
    char** names;
    fz_buffer** bufs;
} ofd_mem_archive;

static int mem_find(ofd_mem_archive* arch, const char* name) {
    for (int i = 0; i < arch->count; ++i) {
        if (!strcmp(arch->names[i], name)) return i;
    }
    return -1;
}

static int mem_count(fz_context* ctx, fz_archive* arch) {
    return ((ofd_mem_archive*)arch)->count;
}

static const char* mem_list(fz_context* ctx, fz_archive* arch_, int idx) {
    ofd_mem_archive* arch = (ofd_mem_archive*)arch_;
    return idx >= 0 && idx < arch->count ? arch->names[idx] : NULL;
}

static int mem_has(fz_context* ctx, fz_archive* arch, const char* name) {
    return mem_find((ofd_mem_archive*)arch, name) >= 0;
}

static fz_buffer* mem_read(fz_context* ctx, fz_archive* arch_, const char* name) {
    ofd_mem_archive* arch = (ofd_mem_archive*)arch_;
    int i = mem_find(arch, name);
    if (i < 0) fz_throw(ctx, FZ_ERROR_FORMAT, "cannot find entry %s", name);
    return fz_keep_buffer(ctx, arch->bufs[i]);
}

static fz_stream* mem_open(fz_context* ctx, fz_archive* arch_, const char* name) {
    ofd_mem_archive* arch = (ofd_mem_archive*)arch_;
    int i = mem_find(arch, name);
    return i < 0 ? NULL : fz_open_buffer(ctx, arch->bufs[i]);
}

static void mem_drop(fz_context* ctx, fz_archive* arch_) {
    ofd_mem_archive* arch = (ofd_mem_archive*)arch_;
    for (int i = 0; i < arch->count; ++i) {
        fz_free(ctx, arch->names[i]);
        fz_drop_buffer(ctx, arch->bufs[i]);
    }
    fz_free(ctx, arch->names);
    fz_free(ctx, arch->bufs);
}

static ofd_mem_archive* mem_new(fz_context* ctx) {
    ofd_mem_archive* arch = fz_new_derived_archive(ctx, NULL, ofd_mem_archive);
    arch->super.format = "ofd-decrypted";
    arch->super.count_entries = mem_count;
    arch->super.list_entry = mem_list;
    arch->super.has_entry = mem_has;
    arch->super.read_entry = mem_read;
    arch->super.open_entry = mem_open;
    arch->super.drop_archive = mem_drop;
    return arch;
}

// takes a reference to buf
static void mem_add(fz_context* ctx, ofd_mem_archive* arch, const char* name, fz_buffer* buf) {
    if (mem_find(arch, name) >= 0) fz_throw(ctx, FZ_ERROR_FORMAT, "duplicate OFD entry %s", name);
    if (arch->count == arch->cap) {
        int cap = arch->cap ? arch->cap * 2 : 32;
        arch->names = fz_realloc_array(ctx, arch->names, cap, char*);
        arch->bufs = fz_realloc_array(ctx, arch->bufs, cap, fz_buffer*);
        arch->cap = cap;
    }
    arch->names[arch->count] = fz_strdup(ctx, name);
    arch->bufs[arch->count] = fz_keep_buffer(ctx, buf);
    arch->count++;
}

//--- XML helpers

static const char* child_text(fz_xml* node, const char* name) {
    const char* text = fz_xml_text(fz_xml_down(ofd_child(node, name)));
    return text ? text : "";
}

// "/Doc_0/OFD.dat" or "Doc_0\OFD.dat" -> "Doc_0/OFD.dat"
static void member_name(char* out, const char* loc) {
    while (*loc == '/' || *loc == '\\') ++loc;
    fz_strlcpy(out, loc, OFD_DECRYPT_PATH);
    for (char* p = out; *p; ++p) {
        if (*p == '\\') *p = '/';
    }
    fz_cleanname(out);
}

static fz_buffer* base64(fz_context* ctx, const char* text) {
    while (*text && strchr(" \t\r\n", *text)) ++text;
    return fz_new_buffer_from_base64(ctx, text, 0);
}

// a copy of data decrypted with key/iv; NULL if the padding shows it's the wrong key
static fz_buffer* decrypt_copy(fz_context* ctx, fz_buffer* data, const unsigned char* key, const unsigned char* iv) {
    fz_buffer* out = fz_new_buffer_from_copied_data(ctx, data->data, data->len);
    size_t n = ofd_sm4_cbc_decrypt(key, iv, out->data, out->len);
    if (!n) {
        fz_drop_buffer(ctx, out);
        return NULL;
    }
    out->len = n;
    return out;
}

// the entries map as XML, encrypted with the file key or plain; NULL if it isn't one
static fz_xml* parse_entries_map(fz_context* ctx, fz_buffer* map, const unsigned char* fek, const unsigned char* iv) {
    fz_buffer* plain = decrypt_copy(ctx, map, fek, iv);
    fz_xml* xml = NULL;
    for (int attempt = 0; attempt < 2 && !xml; ++attempt) {
        fz_buffer* buf = attempt == 0 ? plain : map;
        if (!buf) continue;
        fz_try(ctx) {
            xml = fz_parse_xml(ctx, buf, 0);
            fz_xml* root = fz_xml_root(xml);
            if (!ofd_is(root, "EncryptEntries") && !ofd_is(root, "Entries")) {
                fz_drop_xml(ctx, xml);
                xml = NULL;
            }
        }
        fz_catch(ctx) {
            fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
            fz_ignore_error(ctx);
            xml = NULL;
        }
    }
    fz_drop_buffer(ctx, plain);
    return xml;
}

// The file key from the first password user that the password opens; the right key is
// one that decrypts the entries map. Returns 0 when no user matches.
static int recover_key(fz_context* ctx, fz_xml* seed, fz_buffer* map, const char* password, unsigned char fek[16],
                       unsigned char iv[16], fz_xml** map_xml) {
    unsigned char kek[OFD_SM4_BLOCK];
    ofd_sm3_kdf((const unsigned char*)password, strlen(password), kek, sizeof(kek));
    // UserInfo elements are children of the seed, or grouped one level below it
    fz_xml* users[64];
    int nusers = 0;
    for (fz_xml* node = fz_xml_down(seed); node && nusers < 64; node = fz_xml_next(node)) {
        if (ofd_is(node, "UserInfo")) users[nusers++] = node;
        for (fz_xml* sub = fz_xml_down(node); sub && nusers < 64; sub = fz_xml_next(sub)) {
            if (ofd_is(sub, "UserInfo")) users[nusers++] = sub;
        }
    }
    for (int u = 0; u < nusers; ++u) {
        fz_xml* user = users[u];
        if (ofd_child(user, "UserCert")) continue;
        fz_buffer* ivb = base64(ctx, child_text(user, "IVValue"));
        fz_buffer* wk = NULL;
        fz_buffer* key = NULL;
        int ok = 0;
        fz_var(wk);
        fz_var(key);
        fz_try(ctx) {
            wk = base64(ctx, child_text(user, "EncryptedWK"));
            if (ivb->len == OFD_SM4_BLOCK) key = decrypt_copy(ctx, wk, kek, ivb->data);
            if (key && key->len == OFD_SM4_BLOCK) {
                *map_xml = parse_entries_map(ctx, map, key->data, ivb->data);
                ok = *map_xml != NULL;
            }
            if (ok) {
                memcpy(fek, key->data, OFD_SM4_BLOCK);
                memcpy(iv, ivb->data, OFD_SM4_BLOCK);
            }
        }
        fz_always(ctx) {
            fz_drop_buffer(ctx, key);
            fz_drop_buffer(ctx, wk);
            fz_drop_buffer(ctx, ivb);
        }
        fz_catch(ctx) {
            fz_rethrow(ctx);
        }
        if (ok) return 1;
    }
    return 0;
}

int ofd_is_encrypted(fz_context* ctx, fz_archive* archive) {
    return fz_has_archive_entry(ctx, archive, "Encryptions.xml");
}

// One encryption layer: the decrypted package, or NULL if the password doesn't open it.
static fz_archive* decrypt_layer(fz_context* ctx, fz_archive* src, const char* password) {
    fz_xml* encryptions = NULL;
    fz_xml* seed = NULL;
    fz_xml* map_xml = NULL;
    fz_buffer* map = NULL;
    ofd_mem_archive* out = NULL;
    fz_buffer* data = NULL;
    fz_buffer* plain = NULL;
    fz_var(encryptions);
    fz_var(seed);
    fz_var(map_xml);
    fz_var(map);
    fz_var(out);
    fz_var(data);
    fz_var(plain);
    fz_try(ctx) {
        encryptions = fz_parse_xml_archive_entry(ctx, src, "Encryptions.xml", 0);
        fz_xml* info = ofd_child(fz_xml_root(encryptions), "EncryptInfo");
        if (!info) fz_throw(ctx, FZ_ERROR_FORMAT, "OFD Encryptions.xml has no EncryptInfo");
        char seed_path[OFD_DECRYPT_PATH], map_path[OFD_DECRYPT_PATH];
        const char* seed_loc = child_text(info, "DecryptSeedLoc");
        const char* map_loc = child_text(info, "EntriesMapLoc");
        member_name(seed_path, *seed_loc ? seed_loc : "decryptseed.dat");
        member_name(map_path, *map_loc ? map_loc : "entriesmap.dat");
        seed = fz_parse_xml_archive_entry(ctx, src, seed_path, 0);
        map = fz_read_archive_entry(ctx, src, map_path);

        unsigned char fek[OFD_SM4_BLOCK], iv[OFD_SM4_BLOCK];
        if (!recover_key(ctx, fz_xml_root(seed), map, password, fek, iv, &map_xml)) break;

        out = mem_new(ctx);
        fz_xml* entries = fz_xml_root(map_xml);
        // decrypted members under their original names
        for (fz_xml* e = fz_xml_down(entries); e; e = fz_xml_next(e)) {
            if (!ofd_is(e, "EncryptEntry") && !ofd_is(e, "Entry")) continue;
            const char* path = fz_xml_att(e, "Path");
            const char* epath = fz_xml_att(e, "EPath");
            if (!path || !epath) fz_throw(ctx, FZ_ERROR_FORMAT, "OFD entries map entry without Path or EPath");
            char name[OFD_DECRYPT_PATH], ename[OFD_DECRYPT_PATH];
            member_name(name, path);
            member_name(ename, epath);
            data = fz_read_archive_entry(ctx, src, ename);
            plain = decrypt_copy(ctx, data, fek, iv);
            if (!plain) fz_throw(ctx, FZ_ERROR_FORMAT, "Cannot decrypt OFD entry %s", ename);
            mem_add(ctx, out, name, plain);
            fz_drop_buffer(ctx, plain);
            fz_drop_buffer(ctx, data);
            plain = data = NULL;
        }
        // members that weren't encrypted, minus this layer's metadata and ciphertexts
        int n = fz_count_archive_entries(ctx, src);
        for (int i = 0; i < n; ++i) {
            const char* name = fz_list_archive_entry(ctx, src, i);
            if (!name || !strcmp(name, "Encryptions.xml") || !strcmp(name, seed_path) || !strcmp(name, map_path))
                continue;
            int is_cipher = 0;
            for (fz_xml* e = fz_xml_down(entries); e && !is_cipher; e = fz_xml_next(e)) {
                char ename[OFD_DECRYPT_PATH];
                const char* epath = fz_xml_att(e, "EPath");
                if (!epath) continue;
                member_name(ename, epath);
                is_cipher = !strcmp(ename, name);
            }
            if (is_cipher || mem_has(ctx, &out->super, name)) continue;
            data = fz_read_archive_entry(ctx, src, name);
            mem_add(ctx, out, name, data);
            fz_drop_buffer(ctx, data);
            data = NULL;
        }
    }
    fz_always(ctx) {
        fz_drop_buffer(ctx, plain);
        fz_drop_buffer(ctx, data);
        fz_drop_buffer(ctx, map);
        fz_drop_xml(ctx, map_xml);
        fz_drop_xml(ctx, seed);
        fz_drop_xml(ctx, encryptions);
    }
    fz_catch(ctx) {
        fz_drop_archive(ctx, out ? &out->super : NULL);
        fz_rethrow(ctx);
    }
    return out ? &out->super : NULL;
}

// The decrypted package, all layers removed; NULL if the password doesn't open it.
fz_archive* ofd_decrypt_package(fz_context* ctx, fz_archive* archive, const char* password) {
    if (!password || !*password) return NULL;
    fz_archive* current = fz_keep_archive(ctx, archive);
    for (int layer = 0; layer < OFD_DECRYPT_MAX_LAYERS && ofd_is_encrypted(ctx, current); ++layer) {
        fz_archive* next = NULL;
        fz_try(ctx) {
            next = decrypt_layer(ctx, current, password);
        }
        fz_always(ctx) {
            fz_drop_archive(ctx, current);
        }
        fz_catch(ctx) {
            fz_rethrow(ctx);
        }
        current = next;
        if (!current) return NULL;
    }
    if (ofd_is_encrypted(ctx, current)) {
        fz_drop_archive(ctx, current);
        fz_throw(ctx, FZ_ERROR_LIMIT, "Too many OFD encryption layers");
    }
    return current;
}
