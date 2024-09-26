/* Copyright 2015 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#include "rar.h"
#include "rarvm.h"

/* adapted from https://code.google.com/p/theunarchiver/source/browse/XADMaster/XADRARVirtualMachine.m */
/* adapted from https://code.google.com/p/theunarchiver/source/browse/XADMaster/XADRAR30Filter.m */

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
    uint8_t *globalbackup;
    uint32_t globalbackuplen;
    uint64_t fingerprint;
    uint32_t usagecount;
    uint32_t oldfilterlength;
    struct RARProgramCode *next;
};

struct RARFilter {
    struct RARProgramCode *prog;
    uint32_t initialregisters[8];
    uint8_t *globaldata;
    uint32_t globaldatalen;
    size_t blockstartpos;
    uint32_t blocklength;
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
    return !br->at_eof && (bits <= br->available || br_fill(br, bits));
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

static void bw_write32le(uint8_t *dst, uint32_t value)
{
    dst[0] = value & 0xFF;
    dst[1] = (value >> 8) & 0xFF;
    dst[2] = (value >> 16) & 0xFF;
    dst[3] = (value >> 24) & 0xFF;
}

static void rar_delete_program(struct RARProgramCode *prog)
{
    while (prog) {
        struct RARProgramCode *next = prog->next;
        RARDeleteProgram(prog->prog);
        free(prog->staticdata);
        free(prog->globalbackup);
        free(prog);
        prog = next;
    }
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
            if (*value >= 256) /* absolute address */
                *value -= 256;
            else { /* relative address */
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
    if (!prog)
        return NULL;
    prog->prog = RARCreateProgram();
    if (!prog->prog) {
        rar_delete_program(prog);
        return NULL;
    }
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

static bool rar_execute_filter_prog(struct RARFilter *filter, RARVirtualMachine *vm)
{
    uint32_t newgloballength;
    uint32_t globallength = filter->globaldatalen;
    if (globallength > RARProgramSystemGlobalSize)
        globallength = RARProgramSystemGlobalSize;
    memcpy(&vm->memory[RARProgramSystemGlobalAddress], filter->globaldata, globallength);
    if (filter->prog->staticdata) {
        uint32_t staticlength = filter->prog->staticdatalen;
        if (staticlength > RARProgramUserGlobalSize - globallength)
            staticlength = RARProgramUserGlobalSize - globallength;
        memcpy(&vm->memory[RARProgramUserGlobalAddress], filter->prog->staticdata, staticlength);
    }
    RARSetVirtualMachineRegisters(vm, filter->initialregisters);

    if (!RARExecuteProgram(vm, filter->prog->prog)) {
        warn("Error while executing program in RAR VM");
        return false;
    }

    newgloballength = RARVirtualMachineRead32(vm, RARProgramSystemGlobalAddress + 0x30);
    if (newgloballength > RARProgramUserGlobalSize)
        newgloballength = RARProgramUserGlobalSize;
    if (newgloballength > 0) {
        uint32_t newglobaldatalength = RARProgramSystemGlobalSize + newgloballength;
        if (newglobaldatalength > filter->globaldatalen) {
            uint8_t *newglobaldata = malloc(newglobaldatalength);
            if (!newglobaldata)
                return false;
            free(filter->globaldata);
            filter->globaldata = newglobaldata;
        }
        filter->globaldatalen = newglobaldatalength;
        memcpy(filter->globaldata, &vm->memory[RARProgramSystemGlobalAddress], filter->globaldatalen);
    }
    else
        filter->globaldatalen = 0;

    return true;
}

static struct RARFilter *rar_create_filter(struct RARProgramCode *prog, const uint8_t *globaldata, uint32_t globaldatalen, uint32_t registers[8], size_t startpos, uint32_t length)
{
    struct RARFilter *filter;

    filter = calloc(1, sizeof(*filter));
    if (!filter)
        return NULL;
    filter->prog = prog;
    filter->globaldatalen = globaldatalen > RARProgramSystemGlobalSize ? globaldatalen : RARProgramSystemGlobalSize;
    filter->globaldata = calloc(1, filter->globaldatalen);
    if (!filter->globaldata)
        return NULL;
    if (globaldata)
        memcpy(filter->globaldata, globaldata, globaldatalen);
    if (registers)
        memcpy(filter->initialregisters, registers, sizeof(filter->initialregisters));
    filter->blockstartpos = startpos;
    filter->blocklength = length;

    return filter;
}

static void rar_delete_filter(struct RARFilter *filter)
{
    while (filter) {
        struct RARFilter *next = filter->next;
        free(filter->globaldata);
        free(filter);
        filter = next;
    }
}

static bool rar_execute_filter_delta(struct RARFilter *filter, RARVirtualMachine *vm)
{
    uint32_t length = filter->initialregisters[4];
    uint32_t numchannels = filter->initialregisters[0];
    uint8_t *src, *dst;
    uint32_t i, idx;

    if (length > RARProgramWorkSize / 2)
        return false;

    src = &vm->memory[0];
    dst = &vm->memory[length];
    for (i = 0; i < numchannels; i++) {
        uint8_t lastbyte = 0;
        for (idx = i; idx < length; idx += numchannels)
            lastbyte = dst[idx] = lastbyte - *src++;
    }

    filter->filteredblockaddress = length;
    filter->filteredblocklength = length;

    return true;
}

static bool rar_execute_filter_e8(struct RARFilter *filter, RARVirtualMachine *vm, size_t pos, bool e9also)
{
    uint32_t length = filter->initialregisters[4];
    uint32_t filesize = 0x1000000;
    uint32_t i;

    if (length > RARProgramWorkSize || length <= 4)
        return false;

    for (i = 0; i <= length - 5; i++) {
        if (vm->memory[i] == 0xE8 || (e9also && vm->memory[i] == 0xE9)) {
            uint32_t currpos = (uint32_t)pos + i + 1;
            int32_t address = (int32_t)RARVirtualMachineRead32(vm, i + 1);
            if (address < 0 && currpos >= (uint32_t)-address)
                RARVirtualMachineWrite32(vm, i + 1, address + filesize);
            else if (address >= 0 && (uint32_t)address < filesize)
                RARVirtualMachineWrite32(vm, i + 1, address - currpos);
            i += 4;
        }
    }

    filter->filteredblockaddress = 0;
    filter->filteredblocklength = length;

    return true;
}

static bool rar_execute_filter_rgb(struct RARFilter *filter, RARVirtualMachine *vm)
{
    uint32_t stride = filter->initialregisters[0];
    uint32_t byteoffset = filter->initialregisters[1];
    uint32_t blocklength = filter->initialregisters[4];
    uint8_t *src, *dst;
    uint32_t i, j;

    if (blocklength > RARProgramWorkSize / 2 || stride > blocklength)
        return false;

    src = &vm->memory[0];
    dst = &vm->memory[blocklength];
    for (i = 0; i < 3; i++) {
        uint8_t byte = 0;
        uint8_t *prev = dst + i - stride;
        for (j = i; j < blocklength; j += 3) {
            if (prev >= dst) {
                uint32_t delta1 = abs(prev[3] - prev[0]);
                uint32_t delta2 = abs(byte - prev[0]);
                uint32_t delta3 = abs(prev[3] - prev[0] + byte - prev[0]);
                if (delta1 > delta2 || delta1 > delta3)
                    byte = delta2 <= delta3 ? prev[3] : prev[0];
            }
            byte -= *src++;
            dst[j] = byte;
            prev += 3;
        }
    }
    for (i = byteoffset; i < blocklength - 2; i += 3) {
        dst[i] += dst[i + 1];
        dst[i + 2] += dst[i + 1];
    }

    filter->filteredblockaddress = blocklength;
    filter->filteredblocklength = blocklength;

    return true;
}

static bool rar_execute_filter_audio(struct RARFilter *filter, RARVirtualMachine *vm)
{
    uint32_t length = filter->initialregisters[4];
    uint32_t numchannels = filter->initialregisters[0];
    uint8_t *src, *dst;
    uint32_t i, j;

    if (length > RARProgramWorkSize / 2)
        return false;

    src = &vm->memory[0];
    dst = &vm->memory[length];
    for (i = 0; i < numchannels; i++) {
        struct AudioState state;
        memset(&state, 0, sizeof(state));
        for (j = i; j < length; j += numchannels) {
            int8_t delta = (int8_t)*src++;
            uint8_t predbyte, byte;
            int prederror;
            state.delta[2] = state.delta[1];
            state.delta[1] = state.lastdelta - state.delta[0];
            state.delta[0] = state.lastdelta;
            predbyte = ((8 * state.lastbyte + state.weight[0] * state.delta[0] + state.weight[1] * state.delta[1] + state.weight[2] * state.delta[2]) >> 3) & 0xFF;
            byte = (predbyte - delta) & 0xFF;
            prederror = delta << 3;
            state.error[0] += abs(prederror);
            state.error[1] += abs(prederror - state.delta[0]); state.error[2] += abs(prederror + state.delta[0]);
            state.error[3] += abs(prederror - state.delta[1]); state.error[4] += abs(prederror + state.delta[1]);
            state.error[5] += abs(prederror - state.delta[2]); state.error[6] += abs(prederror + state.delta[2]);
            state.lastdelta = (int8_t)(byte - state.lastbyte);
            dst[j] = state.lastbyte = byte;
            if (!(state.count++ & 0x1F)) {
                uint8_t k, idx = 0;
                for (k = 1; k < 7; k++) {
                    if (state.error[k] < state.error[idx])
                        idx = k;
                }
                memset(state.error, 0, sizeof(state.error));
                switch (idx) {
                case 1: if (state.weight[0] >= -16) state.weight[0]--; break;
                case 2: if (state.weight[0] < 16) state.weight[0]++; break;
                case 3: if (state.weight[1] >= -16) state.weight[1]--; break;
                case 4: if (state.weight[1] < 16) state.weight[1]++; break;
                case 5: if (state.weight[2] >= -16) state.weight[2]--; break;
                case 6: if (state.weight[2] < 16) state.weight[2]++; break;
                }
            }
        }
    }

    filter->filteredblockaddress = length;
    filter->filteredblocklength = length;

    return true;
}

static bool rar_execute_filter(struct RARFilter *filter, RARVirtualMachine *vm, size_t pos)
{
    if (filter->prog->fingerprint == 0x1D0E06077D)
        return rar_execute_filter_delta(filter, vm);
    if (filter->prog->fingerprint == 0x35AD576887)
        return rar_execute_filter_e8(filter, vm, pos, false);
    if (filter->prog->fingerprint == 0x393CD7E57E)
        return rar_execute_filter_e8(filter, vm, pos, true);
    if (filter->prog->fingerprint == 0x951C2C5DC8)
        return rar_execute_filter_rgb(filter, vm);
    if (filter->prog->fingerprint == 0xD8BC85E701)
        return rar_execute_filter_audio(filter, vm);
    log("Unknown parsing filter 0x%x%08x", (uint32_t)(filter->prog->fingerprint >> 32), (uint32_t)filter->prog->fingerprint);

    /* XADRAR30Filter.m @executeOnVirtualMachine claims that this is required */
    if (filter->prog->globalbackuplen > RARProgramSystemGlobalSize) {
        uint8_t *newglobaldata = malloc(filter->prog->globalbackuplen);
        if (newglobaldata) {
            free(filter->globaldata);
            filter->globaldata = newglobaldata;
            filter->globaldatalen = filter->prog->globalbackuplen;
            memcpy(filter->globaldata, filter->prog->globalbackup, filter->prog->globalbackuplen);
        }
    }

    filter->initialregisters[6] = (uint32_t)pos;
    bw_write32le(&filter->globaldata[0x24], (uint32_t)pos);
    bw_write32le(&filter->globaldata[0x28], (uint32_t)((uint64_t)pos >> 32));

    if (!rar_execute_filter_prog(filter, vm))
        return false;

    filter->filteredblockaddress = RARVirtualMachineRead32(vm, RARProgramSystemGlobalAddress + 0x20) & RARProgramMemoryMask;
    filter->filteredblocklength = RARVirtualMachineRead32(vm, RARProgramSystemGlobalAddress + 0x1C) & RARProgramMemoryMask;
    if (filter->filteredblockaddress + filter->filteredblocklength >= RARProgramMemorySize) {
        filter->filteredblockaddress = filter->filteredblocklength = 0;
        return false;
    }

    if (filter->globaldatalen > RARProgramSystemGlobalSize) {
        uint8_t *newglobalbackup = malloc(filter->globaldatalen);
        if (newglobalbackup) {
            free(filter->prog->globalbackup);
            filter->prog->globalbackup = newglobalbackup;
            filter->prog->globalbackuplen = filter->globaldatalen;
            memcpy(filter->prog->globalbackup, filter->globaldata, filter->globaldatalen);
        }
    }
    else
        filter->prog->globalbackuplen = 0;

    return true;
}

bool rar_parse_filter(ar_archive_rar *rar, const uint8_t *bytes, uint16_t length, uint8_t flags)
{
    struct ar_archive_rar_uncomp_v3 *uncomp = &rar->uncomp.state.v3;
    struct ar_archive_rar_filters *filters = &uncomp->filters;

    struct MemBitReader br = { 0 };
    struct RARProgramCode *prog;
    struct RARFilter *filter, **nextfilter;

    uint32_t numprogs, num, blocklength, globaldatalen;
    uint8_t *globaldata;
    size_t blockstartpos;
    uint32_t registers[8] = { 0 };
    uint32_t i;

    br.bytes = bytes;
    br.length = length;

    numprogs = 0;
    for (prog = filters->progs; prog; prog = prog->next)
        numprogs++;

    if ((flags & 0x80)) {
        num = br_next_rarvm_number(&br);
        if (num == 0) {
            rar_delete_filter(filters->stack);
            filters->stack = NULL;
            rar_delete_program(filters->progs);
            filters->progs = NULL;
        }
        else
            num--;
        if (num > numprogs) {
            warn("Invalid program number");
            return false;
        }
        filters->lastfilternum = num;
    }
    else
        num = filters->lastfilternum;

    prog = filters->progs;
    for (i = 0; i < num; i++)
        prog = prog->next;
    if (prog)
        prog->usagecount++;

    blockstartpos = br_next_rarvm_number(&br) + (size_t)lzss_position(&rar->uncomp.lzss);
    if ((flags & 0x40))
        blockstartpos += 258;
    if ((flags & 0x20))
        blocklength = br_next_rarvm_number(&br);
    else
        blocklength = prog ? prog->oldfilterlength : 0;

    registers[3] = RARProgramSystemGlobalAddress;
    registers[4] = blocklength;
    registers[5] = prog ? prog->usagecount : 0;
    registers[7] = RARProgramMemorySize;

    if ((flags & 0x10)) {
        uint8_t mask = (uint8_t)br_bits(&br, 7);
        for (i = 0; i < 7; i++) {
            if ((mask & (1 << i)))
                registers[i] = br_next_rarvm_number(&br);
        }
    }

    if (!prog) {
        uint32_t len = br_next_rarvm_number(&br);
        uint8_t *bytecode;
        struct RARProgramCode **next;

        if (len == 0 || len > 0x10000) {
            warn("Invalid RARVM bytecode length");
            return false;
        }
        bytecode = malloc(len);
        if (!bytecode)
            return false;
        for (i = 0; i < len; i++)
            bytecode[i] = (uint8_t)br_bits(&br, 8);
        prog = rar_compile_program(bytecode, len);
        if (!prog) {
            free(bytecode);
            return false;
        }
        free(bytecode);
        next = &filters->progs;
        while (*next)
            next = &(*next)->next;
        *next = prog;
    }
    prog->oldfilterlength = blocklength;

    globaldata = NULL;
    globaldatalen = 0;
    if ((flags & 0x08)) {
        globaldatalen = br_next_rarvm_number(&br);
        if (globaldatalen > RARProgramUserGlobalSize) {
            warn("Invalid RARVM data length");
            return false;
        }
        globaldata = malloc(globaldatalen + RARProgramSystemGlobalSize);
        if (!globaldata)
            return false;
        for (i = 0; i < globaldatalen; i++)
            globaldata[i + RARProgramSystemGlobalSize] = (uint8_t)br_bits(&br, 8);
    }

    if (br.at_eof) {
        free(globaldata);
        return false;
    }

    filter = rar_create_filter(prog, globaldata, globaldatalen, registers, blockstartpos, blocklength);
    free(globaldata);
    if (!filter)
        return false;

    for (i = 0; i < 7; i++)
        bw_write32le(&filter->globaldata[i * 4], registers[i]);
    bw_write32le(&filter->globaldata[0x1C], blocklength);
    bw_write32le(&filter->globaldata[0x20], 0);
    bw_write32le(&filter->globaldata[0x2C], prog->usagecount);

    nextfilter = &filters->stack;
    while (*nextfilter)
        nextfilter = &(*nextfilter)->next;
    *nextfilter = filter;

    if (!filters->stack->next)
        filters->filterstart = blockstartpos;

    return true;
}

bool rar_run_filters(ar_archive_rar *rar)
{
    struct ar_archive_rar_filters *filters = &rar->uncomp.state.v3.filters;
    struct RARFilter *filter = filters->stack;
    size_t start = filters->filterstart;
    size_t end = start + filter->blocklength;
    uint32_t lastfilteraddress;
    uint32_t lastfilterlength;

    filters->filterstart = SIZE_MAX;
    end = (size_t)rar_expand(rar, end);
    if (end != start + filter->blocklength) {
        warn("Failed to expand the expected amout of bytes");
        return false;
    }

    if (!filters->vm) {
        filters->vm = calloc(1, sizeof(*filters->vm));
        if (!filters->vm)
            return false;
    }

    lzss_copy_bytes_from_window(&rar->uncomp.lzss, filters->vm->memory, start, filter->blocklength);
    if (!rar_execute_filter(filter, filters->vm, rar->progress.bytes_done)) {
        warn("Failed to execute parsing filter");
        return false;
    }

    lastfilteraddress = filter->filteredblockaddress;
    lastfilterlength = filter->filteredblocklength;
    filters->stack = filter->next;
    filter->next = NULL;
    rar_delete_filter(filter);

    while ((filter = filters->stack) != NULL && filter->blockstartpos == filters->filterstart && filter->blocklength == lastfilterlength) {
        memmove(&filters->vm->memory[0], &filters->vm->memory[lastfilteraddress], lastfilterlength);
        if (!rar_execute_filter(filter, filters->vm, rar->progress.bytes_done)) {
            warn("Failed to execute parsing filter");
            return false;
        }

        lastfilteraddress = filter->filteredblockaddress;
        lastfilterlength = filter->filteredblocklength;
        filters->stack = filter->next;
        filter->next = NULL;
        rar_delete_filter(filter);
    }

    if (filters->stack) {
        if (filters->stack->blockstartpos < end) {
            warn("Bad filter order");
            return false;
        }
        filters->filterstart = filters->stack->blockstartpos;
    }

    filters->lastend = end;
    filters->bytes = &filters->vm->memory[lastfilteraddress];
    filters->bytes_ready = lastfilterlength;

    return true;
}

void rar_clear_filters(struct ar_archive_rar_filters *filters)
{
    rar_delete_filter(filters->stack);
    rar_delete_program(filters->progs);
    free(filters->vm);
}
