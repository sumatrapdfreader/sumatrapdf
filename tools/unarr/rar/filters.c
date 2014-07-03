/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: GPLv3 */

#include "rar.h"
#include "vm.h"

// adapted from https://code.google.com/p/theunarchiver/source/browse/XADMaster/XADRARVirtualMachine.m

struct MemBitReader {
    const uint8_t *bytes;
    size_t length;
    size_t offset;
    uint64_t bits;
    int available;
    bool at_eof;
};

struct RARProgramCode {
    RARProgram *prog;
    uint8_t *staticdata;
    uint32_t staticdatalen;
    uint64_t fingerprint;
    struct RARProgramCode *next;
};

struct RARProgramInvocation {
    struct RARProgramCode *prog;
    uint32_t initialregisters[8];
    uint8_t *globaldata;
    uint32_t globaldatalen;
    uint8_t *globalbackup;
    uint32_t globalbackuplen;
};

struct RARFilter {
    struct RARProgramInvocation *invoc;
    size_t blockstartpos;
    size_t blocklength;
    uint32_t filteredblockaddress;
    uint32_t filteredblocklength;
    struct RARFilter *next;
};

static bool br_fill(struct MemBitReader *br, int bits)
{
    while (br->available < bits && br->offset < br->length) {
        br->bits = (br->bits << 8) | br->bytes[br->offset++];
        br->available += 8;
    }
    if (bits > br->available) {
        br->at_eof = true;
        return false;
    }
    return true;
}

static inline uint32_t br_bits(struct MemBitReader *br, int bits)
{
    if (bits > br->available && (br->at_eof || !br_fill(br, bits)))
        return 0;
    return (uint32_t)((br->bits >> (br->available -= bits)) & (((uint64_t)1 << bits) - 1));
}

static inline bool br_available(struct MemBitReader *br, int bits)
{
    return !br->at_eof && (bits > br->available || br_fill(br, bits));
}

static uint32_t br_next_rarvm_number(struct MemBitReader *br)
{
    uint32_t val;
    switch (br_bits(br, 2)) {
    case 0:
        return br_bits(br, 4);
    case 1:
        val = br_bits(br, 8);
        if (val >= 16)
            return val;
        return 0xFFFFFF00 | (val << 4) | br_bits(br, 4);
    case 2:
        return br_bits(br, 16);
    default:
        return br_bits(br, 32);
    }
}

static RARVirtualMachine *rar_create_vm()
{
    return calloc(1, sizeof(RARVirtualMachine));
}

static void rar_delete_program(struct RARProgramCode *prog)
{
    while (prog) {
        struct RARProgramCode *next = prog->next;
        RARDeleteProgram(prog->prog);
        free(prog->staticdata);
        free(prog);
        prog = next;
    }
}

void rar_clear_filters(struct ar_archive_rar_filters *filters)
{
    rar_delete_program(filters->progs);
    free(filters->vm);
}

static bool rar_parse_operand(struct MemBitReader *br, uint8_t instruction, bool bytemode, uint32_t instrcount, uint8_t *addressmode, uint32_t *value)
{
    if (br_bits(br, 1)) {
        *addressmode = RARRegisterAddressingMode((uint8_t)br_bits(br, 3));
        *value = 0;
    }
    else if (br_bits(br, 1)) {
        if (br_bits(br, 1)) {
            if (br_bits(br, 1))
                *addressmode = RARAbsoluteAddressingMode;
            else
                *addressmode = RARIndexedAbsoluteAddressingMode((uint8_t)br_bits(br, 3));
            *value = br_next_rarvm_number(br);
        }
        else {
            *addressmode = RARRegisterIndirectAddressingMode((uint8_t)br_bits(br, 3));
            *value = 0;
        }
    }
    else {
        *addressmode = RARImmediateAddressingMode;
        if (!bytemode)
            *value = br_next_rarvm_number(br);
        else
            *value = br_bits(br, 8);
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
    return !br->at_eof;
}

static struct RARProgramCode *rar_compile_program(const uint8_t *bytes, size_t length)
{
    struct MemBitReader br = { 0 };
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

    prog = calloc(1, sizeof(*prog));
    prog->prog = RARCreateProgram();
    if (!prog->prog) {
        rar_delete_program(prog);
        return NULL;
    }
    // XADRARVirtualMachine calculates a fingerprint so that known,
    // often used programs can be run natively instead of in the VM
    prog->fingerprint = ar_crc32(0, bytes, length) | ((uint64_t)length << 32);

    if (br_bits(&br, 1)) {
        prog->staticdatalen = br_next_rarvm_number(&br) + 1;
        prog->staticdata = malloc(prog->staticdatalen);
        if (!prog->staticdata) {
            rar_delete_program(prog);
            return NULL;
        }
        for (i = 0; i < prog->staticdatalen; i++)
            prog->staticdata[i] = (uint8_t)br_bits(&br, 8);
    }

    while (br_available(&br, 8)) {
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

static struct RARProgramInvocation *rar_create_program_invocation(struct RARProgramCode *prog, const uint8_t *globaldata, size_t globaldatalen, uint32_t registers[8])
{
    struct RARProgramInvocation *invoc;
    
    invoc = calloc(1, sizeof(*invoc));
    if (!invoc)
        return NULL;
    invoc->prog = prog;
    invoc->globaldatalen = globaldatalen > RARProgramSystemGlobalSize ? globaldatalen : RARProgramSystemGlobalSize;
    invoc->globaldata = malloc(globaldatalen);
    if (!invoc->globaldata)
        return NULL;
    if (globaldata)
        memcpy(invoc->globaldata, globaldata, invoc->globaldatalen);
    if (registers)
        memcpy(invoc->initialregisters, registers, sizeof(invoc->initialregisters));

    return invoc;
}

static void rar_free_program_invocation(struct RARProgramInvocation *invoc)
{
    free(invoc->globaldata);
    free(invoc->globalbackup);
    free(invoc);
}

static bool rar_execute_invocation(struct RARProgramInvocation *invoc, RARVirtualMachine *vm)
{
    uint32_t newgloballength;
    uint32_t globallength = invoc->globaldatalen;
    if (globallength > RARProgramSystemGlobalSize)
        globallength = RARProgramSystemGlobalSize;
    memcpy(&vm->memory[RARProgramSystemGlobalAddress], invoc->globaldata, globallength);
    if (invoc->prog->staticdata) {
        uint32_t staticlength = invoc->prog->staticdatalen;
        if (staticlength > RARProgramUserGlobalSize - globallength)
            staticlength = RARProgramUserGlobalSize - globallength;
        memcpy(&vm->memory[RARProgramUserGlobalAddress], invoc->prog->staticdata, staticlength);
    }
    memcpy(vm->registers, invoc->initialregisters, sizeof(vm->registers));

    if (!RARExecuteProgram(vm, invoc->prog->prog)) {
        warn("Error while executing program in RAR VM");
        return false;
    }

    newgloballength = RARVirtualMachineRead32(vm, RARProgramSystemGlobalAddress + 0x30);
    if (newgloballength > RARProgramUserGlobalSize)
        newgloballength = RARProgramUserGlobalSize;
    if (newgloballength > 0) {
        uint32_t newglobaldatalength = RARProgramSystemGlobalSize + newgloballength;
        uint8_t *newglobaldata = realloc(invoc->globaldata, newglobaldatalength);
        if (!newglobaldata)
            return false;
        memcpy(newglobaldata, &vm->memory[RARProgramSystemGlobalAddress], newglobaldatalength);
        invoc->globaldata = newglobaldata;
        invoc->globaldatalen = newglobaldatalength;
    }
    else
        invoc->globaldatalen = 0;

    return true;
}

static struct RARFilter *rar_create_filter(struct RARProgramInvocation *invoc, size_t startpos, size_t length)
{
    struct RARFilter *filter;

    filter = calloc(1, sizeof(*filter));
    if (!filter)
        return NULL;
    filter->invoc = invoc;
    filter->blockstartpos = startpos;
    filter->blocklength = length;

    return filter;
}

static void rar_delete_filter(struct RARFilter *filter)
{
    while (filter) {
        struct RARFilter *next = filter->next;
        free(filter);
        filter = next;
    }
}

static bool rar_execute_filter_delta(struct RARFilter *filter, RARVirtualMachine *vm, size_t pos)
{
    uint32_t length = filter->invoc->initialregisters[4];
    uint32_t numchannels = filter->invoc->initialregisters[0];
    uint8_t *src, *dest;
    uint32_t i, idx;

    if (length > RARProgramWorkSize / 2)
        return false;

    filter->filteredblockaddress = length;
    filter->filteredblocklength = length;

    src = &vm->memory[0];
    dest = &vm->memory[filter->filteredblockaddress];
    for (i = 0; i < numchannels; i++) {
        uint8_t lastbyte = 0;
        for (idx = i; idx < length; idx += numchannels)
            lastbyte = dest[idx] = lastbyte - *src++;
    }

    return true;
}

static bool rar_execute_filter(struct RARFilter *filter, RARVirtualMachine *vm, size_t pos)
{
    int i;
    // TODO: verify fingerprint
    if (filter->invoc->prog->fingerprint == 0x1D0E06077D)
        return rar_execute_filter_delta(filter, vm, pos);

    // TODO: XADRAR30Filter.m @executeOnVirtualMachine claims that this is required
    if (filter->invoc->globalbackuplen > RARProgramSystemGlobalSize) {
        uint8_t *newglobaldata = malloc(filter->invoc->globalbackuplen);
        if (newglobaldata) {
            free(filter->invoc->globaldata);
            filter->invoc->globaldata = newglobaldata;
            filter->invoc->globaldatalen = filter->invoc->globalbackuplen;
        }
    }

    filter->invoc->initialregisters[6] = (uint32_t)pos;
    for (i = 0; i < 8; i++)
        filter->invoc->globaldata[0x24 + i] = (pos >> (i * 8)) & 0xFF;
    if (!rar_execute_invocation(filter->invoc, vm))
        return false;
    filter->filteredblockaddress = RARVirtualMachineRead32(vm, RARProgramSystemGlobalAddress + 0x20) & RARProgramMemoryMask;
    filter->filteredblocklength = RARVirtualMachineRead32(vm, RARProgramSystemGlobalAddress + 0x1C) & RARProgramMemoryMask;
    if (filter->filteredblockaddress + filter->filteredblocklength >= RARProgramMemorySize) {
        filter->filteredblockaddress = filter->filteredblocklength = 0;
        return false;
    }

    if (filter->invoc->globaldatalen > RARProgramSystemGlobalSize) {
        uint8_t *newglobalbackup = malloc(filter->invoc->globaldatalen);
        if (newglobalbackup) {
            free(filter->invoc->globalbackup);
            filter->invoc->globalbackup = newglobalbackup;
            filter->invoc->globalbackuplen = filter->invoc->globaldatalen;
        }
    }
    else
        filter->invoc->globalbackuplen = 0;

    return true;
}
