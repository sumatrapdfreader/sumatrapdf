/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// needs ofd-seal.h

void fz_register_ofd_handler(fz_context* ctx);

typedef enum {
    OFD_SIG_SIGNATURE, // a Signature of Signatures.xml
    OFD_SIG_INTEGRITY  // the package's integrity protection (OFDEntries.xml, GM/T 0099)
} ofd_sig_kind;

// one signature of an OFD package, as ofd_check_signatures() found it
typedef struct ofd_signature_info {
    struct ofd_signature_info* next;
    ofd_sig_kind kind;
    int page;     // 0-based across the package's documents; -1 without a stamp
    fz_rect rect; // the stamp, in page points
    int files_total;
    int files_ok;       // files whose digest still matches
    char bad_file[256]; // a file that changed, or is missing from the package
    int files_extra;    // integrity: files the protection doesn't list
    char extra_file[256];
    ofd_sig_check check;
} ofd_signature_info;

fz_outline* ofd_load_attachments(fz_context* ctx, fz_document* doc);
fz_buffer* ofd_load_attachment(fz_context* ctx, fz_document* doc, int no);

int ofd_count_signatures(fz_context* ctx, fz_document* doc);
ofd_signature_info* ofd_check_signatures(fz_context* ctx, fz_document* doc);
void ofd_drop_signature_infos(fz_context* ctx, ofd_signature_info* info);
