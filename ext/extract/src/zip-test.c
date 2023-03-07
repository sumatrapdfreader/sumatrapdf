/* Crude programme to show detailed information about a zip file. */

#include "memento.h"
#include "outf.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static int s_native_little_endinesss(void)
{
    static const char   a[] = { 1, 2};
    uint16_t b = *(uint16_t*) a;
    if (b == 1 + 2*256) {
        /* Native little-endiness. */
        return 1;
    }
    else if (b == 2 + 1*256) {
        return 0;
    }
    abort();
}


static int s_show(const char* filename)
{
    outf("Looking at filename=%s", filename);
    assert(s_native_little_endinesss());
    FILE* f = fopen(filename, "r");
    assert(f);
    size_t  datasize = 10*1000*1000;
    char* data = extract_malloc(datasize);
    assert(data);
    size_t n = fread(data, 1, datasize, f);
    assert(n < datasize);
    datasize = n;
    outf("datasize=%zi", datasize);
    fclose(f);

    /* look for End of central directory (EOCD) record. */
    uint32_t magic = 0x06054b50;
    char* pos = data + datasize - 22;
    for(;;) {
        if (!memcmp(pos, &magic, sizeof(magic))) break;
        assert(pos > data);
        pos -= 1;
    }
    outf("found EOCD at offset=%li", pos-data);
    uint16_t disk_number = *(uint16_t*)(pos+4);
    uint16_t disk_cd = *(uint16_t*)(pos+6);
    uint16_t num_records_on_disk = *(uint16_t*)(pos+8);
    uint16_t num_records = *(uint16_t*)(pos+10);
    uint32_t size_cd = *(uint32_t*)(pos+12);
    uint32_t offset_cd = *(uint32_t*)(pos+16);
    uint16_t comment_length = *(uint16_t*)(pos+20);
    char* comment = extract_malloc(comment_length + 1);
    assert(comment);
    memcpy(comment, pos+22, comment_length);
    comment[comment_length] = 0;
    assert(strlen(comment) == comment_length);
    outf("    EOCD:");
    outf("        disk_number=%i", disk_number);
    outf("        disk_cd=%i", disk_cd);
    outf("        num_records_on_disk=%i", num_records_on_disk);
    outf("        num_records=%i", num_records);
    outf("        size_cd=%i", size_cd);
    outf("        offset_cd=%i", offset_cd);
    outf("        comment_length=%i", comment_length);
    outf("        comment=%s", comment);

    if (pos != data + datasize - 22 - comment_length) {
        outf("file does not end with EOCD. datasize=%zi pos-data=%li datasize-22-comment_length=%zi",
                datasize,
                pos-data,
                datasize-22-comment_length
                );
        /* I think this isn't actually an error according to the Zip standard,
        but zip files created by us should always pass this test. Note that
        Word doesn't like trailing data after the EOCD record, but will repair
        the file. */
        assert(0);
    }

    pos = data + offset_cd;
    int i;
    for (i=0; i<num_records_on_disk; ++i) {
        outf("    file %i: offset=%i", i, pos - data);
        magic = 0x02014b50;
        assert(!memcmp(pos, &magic, sizeof(magic)));
        uint16_t version_made_by = *(uint16_t*)(pos+4);
        uint16_t version_needed = *(uint16_t*)(pos+6);
        uint16_t general_bit_flag = *(uint16_t*)(pos+8);
        uint16_t compression_method = *(uint16_t*)(pos+10);
        uint16_t mtime = *(uint16_t*)(pos+12);
        uint16_t mdate = *(uint16_t*)(pos+14);
        uint32_t crc = *(uint32_t*)(pos+16);
        uint32_t size_compressed = *(uint32_t*)(pos+20);
        uint32_t size_uncompressed = *(uint32_t*)(pos+24);
        uint16_t filename_length = *(uint16_t*)(pos+28);
        uint16_t extrafield_length = *(uint16_t*)(pos+30);
        uint16_t filecomment_length = *(uint16_t*)(pos+32);
        uint16_t disk_number = *(uint16_t*)(pos+34);
        uint16_t internal_attributes = *(uint16_t*)(pos+36);
        uint32_t external_attributes = *(uint32_t*)(pos+38);
        uint32_t offset = *(uint32_t*)(pos+42);
        char* filename = extract_malloc(filename_length + 1);
        assert(filename);
        memcpy(filename, pos+46, filename_length);
        filename[filename_length] = 0;

        char* comment = extract_malloc(filecomment_length + 1);
        assert(comment);
        memcpy(comment, pos+46+filename_length+extrafield_length, filecomment_length);
        comment[filecomment_length] = 0;
        assert(strlen(comment) == filecomment_length);
        outf("        version_made_by=0x%x", version_made_by);
        outf("        version_needed=0x%x", version_needed);
        outf("        general_bit_flag=0x%x", general_bit_flag);
        outf("        compression_method=%i", compression_method);
        outf("        mtime=%i", mtime);
        outf("        mdate=%i", mdate);
        outf("        crc=%i", crc);
        outf("        size_compressed=%i", size_compressed);
        outf("        size_uncompressed=%i", size_uncompressed);
        outf("        filename_length=%i", filename_length);
        outf("        extrafield_length=%i", extrafield_length);
        outf("        filecomment_length=%i", filecomment_length);
        outf("        disk_number=%i", disk_number);
        outf("        internal_attributes=0x%x", internal_attributes);
        outf("        external_attributes=0x%x", external_attributes);
        outf("        offset=%i", offset);
        outf("        filename=%s", filename);

        if (extrafield_length) {
            outf( "        extra:");
            fprintf(stderr, "            ");
            char* extra = pos + 46+filename_length;
            int j;
            for (j=0; j<extrafield_length; ++j) {
                unsigned char c = extra[j];
                if (isprint(c) && c != '\\') fputc(c, stderr);
                else fprintf(stderr, "\\x%02x", c);
            }
            fputc('\n', stderr);
        }

        /* show local file header. */
        {
            char* local_pos = data + offset;
            outf("    local header offset=%i", i, local_pos - data);
            magic = 0x04034b50;
            assert(!memcmp(local_pos, &magic, sizeof(magic)));

            uint16_t version_needed = *(uint16_t*)(local_pos+4);
            uint16_t general_bit_flag = *(uint16_t*)(local_pos+6);
            uint16_t compression_method = *(uint16_t*)(local_pos+8);
            uint16_t mtime = *(uint16_t*)(local_pos+10);
            uint16_t mdate = *(uint16_t*)(local_pos+12);
            uint32_t crc = *(uint32_t*)(local_pos+14);
            uint32_t size_compressed = *(uint32_t*)(local_pos+18);
            uint32_t size_uncompressed = *(uint32_t*)(local_pos+22);
            uint16_t filename_length = *(uint16_t*)(local_pos+26);
            uint16_t extrafield_length = *(uint16_t*)(local_pos+28);

            char* filename = extract_malloc(filename_length + 1);
            assert(filename);
            memcpy(filename, local_pos+30, filename_length);
            filename[filename_length] = 0;

            outf("            version_needed=0x%x", version_needed);
            outf("            general_bit_flag=0x%x", general_bit_flag);
            outf("            compression_method=%i", compression_method);
            outf("            mtime=%i", mtime);
            outf("            mdate=%i", mdate);
            outf("            crc=%i", crc);
            outf("            size_compressed=%i", size_compressed);
            outf("            size_uncompressed=%i", size_uncompressed);
            outf("            filename_length=%i", filename_length);
            outf("            extrafield_length=%i", extrafield_length);
            outf("            filecomment_length=%i", filecomment_length);
            outf("            disk_number=%i", disk_number);
            outf("            internal_attributes=0x%x", internal_attributes);
            outf("            external_attributes=0x%x", external_attributes);
            outf("            offset=%i", offset);
            outf("            filename=%s", filename);

            if (extrafield_length) {
                outf( "            extra:");
                fprintf(stderr, "                ");
                char* extra = local_pos + 30 + filename_length;
                int j;
                for (j=0; j<extrafield_length; ++j) {
                    unsigned char c = extra[j];
                    if (isprint(c) && c != '\\') fputc(c, stderr);
                    else fprintf(stderr, "\\x%02x", c);
                }
                fputc('\n', stderr);
            }

        }

        outf("        comment=%s", comment);

        pos += 46 + filename_length + extrafield_length + filecomment_length;
    }

    outf("finished");
    extract_free(&data);

    return 0;
}

int main(int argc, char** argv)
{
    outf_level_set(1);
    int i;
    for (i=1; i<argc; ++i) {
        s_show(argv[i]);
    }
    return 0;
}
