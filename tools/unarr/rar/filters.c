/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: GPLv3 */

#include "rar.h"
#include "vm.h"

struct MemBitReader {
    const uint8_t *bytes;
    size_t length;
    size_t offset;
    uint64_t bits;
    int available;
};

static bool br_fill(struct MemBitReader *br, int bits)
{
    while (br->available < bits && br->offset < br->length) {
        br->bits = (br->bits << 8) | br->bytes[br->offset++];
        br->available += 8;
    }
    return bits <= br->available;
}

static inline bool br_check(struct MemBitReader *br, int bits)
{
    return bits <= br->available || br_fill(br, bits);
}

static inline uint32_t br_bits(struct MemBitReader *br, int bits)
{
    return (uint32_t)((br->bits >> (br->available -= bits)) & (((uint64_t)1 << bits) - 1));
}

static uint32_t br_next_rarvm_number(struct MemBitReader *br)
{
    if (!br_check(br, 2))
        return (uint32_t)-1;
    switch (br_bits(br, 2)) {
    case 0:
        return br_check(br, 4) ? br_bits(br, 4) : (uint32_t)-1;
    case 1:
        if (br_check(br, 8)) {
            uint32_t val = br_bits(br, 8);
            if (val >= 16)
                return val;
            if (br_check(br, 4))
                return 0xFFFFFF00 | (val << 4) | br_bits(br, 4);
        }
        return (uint32_t)-1;
    case 2:
        return br_check(br, 16) ? br_bits(br, 16) : (uint32_t)-1;
    default:
        return br_check(br, 32) ? br_bits(br, 32) : (uint32_t)-1;
    }
}

struct RARProgramCode {
    RARProgram *prog;
    uint8_t *staticdata;
    uint8_t *globalbackup;
    uint32_t fingerprint;
};

static void rar_delete_program(struct RARProgramCode *prog)
{
    RARDeleteProgram(prog->prog);
    free(prog->staticdata);
    free(prog->globalbackup);
    free(prog);
}

static bool rar_parse_operand(struct MemBitReader *br, uint8_t instruction, bool bytemode, uint32_t instrcount, uint8_t *addressmode, uint32_t *value)
{
    if (!br_check(br, 4))
        return false;
    else if (br_bits(br, 1)) {
        *addressmode = RARRegisterAddressingMode((uint8_t)br_bits(br, 3));
        *value = 0;
    }
    else if (br_bits(br, 1)) {
        if (br_bits(br, 1)) {
            if (br_bits(br, 1))
                *addressmode = RARAbsoluteAddressingMode;
            else if (br_check(br, 5))
                *addressmode = RARIndexedAbsoluteAddressingMode((uint8_t)br_bits(br, 3));
            else
                return false;
            *value = br_next_rarvm_number(br);
        }
        else if (br_check(br, 3)) {
            *addressmode = RARRegisterIndirectAddressingMode((uint8_t)br_bits(br, 3));
            *value = 0;
        }
        else
            return false;
    }
    else {
        *addressmode = RARImmediateAddressingMode;
        if (!bytemode)
            *value = br_next_rarvm_number(br);
        else if (br_check(br, 8))
            *value = br_bits(br, 8);
        else
            return false;
        if (instrcount != (uint32_t)-1 && RARInstructionIsRelativeJump(instruction)) {
            if (*value >= 256) // absolute address
                *value -= 256;
            else { // relative address
                if (*value >= 136)
                    *value -= 264;
                else if (*value >= 16)
                    *value -= 8;
                else if (*value >= 8)
                    *value -= 16;
                *value += instrcount;
            }
        }
    }
    return true;
}

static struct RARProgramCode *rar_compile_program(const uint8_t *bytes, size_t length)
{
    struct MemBitReader br;
    struct RARProgramCode *prog;
    uint32_t instrcount = 0;
    uint8_t xor;
    size_t i;

    xor = 0;
    for (i = 1; i < length; i++)
        xor ^= bytes[i];
    if (!length || xor != bytes[0])
        return NULL;

    br.bytes = bytes;
    br.length = length;
    br.offset = 1;
    br.available = 0;

    prog = calloc(1, sizeof(*prog));
    prog->prog = RARCreateProgram();
    if (!prog->prog) {
        rar_delete_program(prog);
        return NULL;
    }
    // XADRARVirtualMachine calculates a CRC32 fingerprint so that known,
    // often used programs can be run natively instead of in the VM
    prog->fingerprint = crc32(0, bytes, length);

    if (br_check(&br, 3) && br_bits(&br, 1)) {
        uint32_t staticlen = br_next_rarvm_number(&br) + 1;
        prog->staticdata = malloc(staticlen);
        if (!prog->staticdata) {
            rar_delete_program(prog);
            return NULL;
        }
        for (i = 0; i < staticlen; i++)
            prog->staticdata[i] = br_check(&br, 8) ? (uint8_t)br_bits(&br, 8) : 0;
    }

    while (br_check(&br, 8)) {
        bool ok = true;
        uint8_t instruction = (uint8_t)br_bits(&br, 4);
        bool bytemode = false;
        int numargs = 0;
        uint8_t addrmode1 = 0, addrmode2 = 0;
        uint32_t value1 = 0, value2 = 0;

        if ((instruction & 0x08))
            instruction = ((instruction << 2) | (uint8_t)br_bits(&br, 2)) - 24;
        if (RARInstructionHasByteMode(instruction))
            bytemode = br_bits(&br, 1) != 0;
        ok = RARProgramAddInstr(prog->prog, instruction, bytemode);
        numargs = NumberOfRARInstructionOperands(instruction);
        if (ok && numargs >= 1)
            ok = rar_parse_operand(&br, instruction, bytemode, instrcount, &addrmode1, &value1);
        if (ok && numargs == 2)
            ok = rar_parse_operand(&br, instruction, bytemode, (uint32_t)-1, &addrmode2, &value2);
        if (ok)
            ok = RARSetLastInstrOperands(prog->prog, addrmode1, value1, addrmode2, value2);
        if (!ok) {
            warn("Invalid RAR program instruction");
            rar_delete_program(prog);
            return NULL;
        }
        instrcount++;
    }

    if (!RARIsProgramTerminated(prog->prog)) {
        if (!RARProgramAddInstr(prog->prog, RARRetInstruction, false)) {
            rar_delete_program(prog);
            return NULL;
        }
    }

    return prog;
}
