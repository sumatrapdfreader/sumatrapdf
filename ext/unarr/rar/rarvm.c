/* Copyright 2015 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

/* adapted from https://code.google.com/p/theunarchiver/source/browse/XADMaster/RARVirtualMachine.c */

#include "rarvm.h"
#include "../common/allocator.h"

#include <stdlib.h>
#include <string.h>

typedef struct RAROpcode_s RAROpcode;

struct RAROpcode_s {
    uint8_t instruction;
    uint8_t bytemode;
    uint8_t addressingmode1;
    uint8_t addressingmode2;
    uint32_t value1;
    uint32_t value2;
};

struct RARProgram_s {
    RAROpcode *opcodes;
    uint32_t length;
    uint32_t capacity;
};

/* Program building */

RARProgram *RARCreateProgram()
{
    return calloc(1, sizeof(RARProgram));
}

void RARDeleteProgram(RARProgram *prog)
{
    if (prog)
        free(prog->opcodes);
    free(prog);
}

bool RARProgramAddInstr(RARProgram *prog, uint8_t instruction, bool bytemode)
{
    if (instruction >= RARNumberOfInstructions)
        return false;
    if (bytemode && !RARInstructionHasByteMode(instruction))
        return false;
    if (prog->length + 1 >= prog->capacity) {
        /* in my small file sample, 16 is the value needed most often */
        uint32_t newCapacity = prog->capacity ? prog->capacity * 4 : 32;
        RAROpcode *newCodes = calloc(newCapacity, sizeof(*prog->opcodes));
        if (!newCodes)
            return false;
        memcpy(newCodes, prog->opcodes, prog->capacity * sizeof(*prog->opcodes));
        free(prog->opcodes);
        prog->opcodes = newCodes;
        prog->capacity = newCapacity;
    }
    memset(&prog->opcodes[prog->length], 0, sizeof(prog->opcodes[prog->length]));
    prog->opcodes[prog->length].instruction = instruction;
    if (instruction == RARMovzxInstruction || instruction == RARMovsxInstruction)
        prog->opcodes[prog->length].bytemode = 2; /* second argument only */
    else if (bytemode)
        prog->opcodes[prog->length].bytemode = (1 | 2);
    else
        prog->opcodes[prog->length].bytemode = 0;
    prog->length++;
    return true;
}

bool RARSetLastInstrOperands(RARProgram *prog, uint8_t addressingmode1, uint32_t value1, uint8_t addressingmode2, uint32_t value2)
{
    RAROpcode *opcode = &prog->opcodes[prog->length - 1];
    int numoperands;

    if (addressingmode1 >= RARNumberOfAddressingModes || addressingmode2 >= RARNumberOfAddressingModes)
        return false;
    if (!prog->length || opcode->addressingmode1 || opcode->value1 || opcode->addressingmode2 || opcode->value2)
        return false;

    numoperands = NumberOfRARInstructionOperands(opcode->instruction);
    if (numoperands == 0)
        return true;

    if (addressingmode1 == RARImmediateAddressingMode && RARInstructionWritesFirstOperand(opcode->instruction))
        return false;
    opcode->addressingmode1 = addressingmode1;
    opcode->value1 = value1;

    if (numoperands == 2) {
        if (addressingmode2 == RARImmediateAddressingMode && RARInstructionWritesSecondOperand(opcode->instruction))
            return false;
        opcode->addressingmode2 = addressingmode2;
        opcode->value2 = value2;
    }

    return true;
}

bool RARIsProgramTerminated(RARProgram *prog)
{
    return prog->length > 0 && RARInstructionIsUnconditionalJump(prog->opcodes[prog->length - 1].instruction);
}

/* Execution */

#define EXTMACRO_BEGIN do {
#ifdef _MSC_VER
#define EXTMACRO_END } __pragma(warning(push)) __pragma(warning(disable:4127)) while (0) __pragma(warning(pop))
#else
#define EXTMACRO_END } while (0)
#endif

#define CarryFlag 1
#define ZeroFlag 2
#define SignFlag 0x80000000

#define SignExtend(a) ((uint32_t)((int8_t)(a)))

static uint32_t _RARGetOperand(RARVirtualMachine *vm, uint8_t addressingmode, uint32_t value, bool bytemode);
static void _RARSetOperand(RARVirtualMachine *vm, uint8_t addressingmode, uint32_t value, bool bytemode, uint32_t data);

#define GetOperand1() _RARGetOperand(vm, opcode->addressingmode1, opcode->value1, opcode->bytemode & 1)
#define GetOperand2() _RARGetOperand(vm, opcode->addressingmode2, opcode->value2, opcode->bytemode & 2)
#define SetOperand1(data) _RARSetOperand(vm, opcode->addressingmode1, opcode->value1, opcode->bytemode & 1, data)
#define SetOperand2(data) _RARSetOperand(vm, opcode->addressingmode2, opcode->value2, opcode->bytemode & 2, data)

#define SetFlagsWithCarry(res, carry) EXTMACRO_BEGIN uint32_t result = (res); flags = (result == 0 ? ZeroFlag : (result & SignFlag)) | ((carry) ? CarryFlag : 0); EXTMACRO_END
#define SetByteFlagsWithCarry(res, carry) EXTMACRO_BEGIN uint8_t result = (res); flags = (result == 0 ? ZeroFlag : (SignExtend(result) & SignFlag)) | ((carry) ? CarryFlag : 0); EXTMACRO_END
#define SetFlags(res) SetFlagsWithCarry(res, 0)

#define SetOperand1AndFlagsWithCarry(res, carry) EXTMACRO_BEGIN uint32_t r = (res); SetFlagsWithCarry(r, carry); SetOperand1(r); EXTMACRO_END
#define SetOperand1AndByteFlagsWithCarry(res, carry) EXTMACRO_BEGIN uint8_t r = (res); SetByteFlagsWithCarry(r, carry); SetOperand1(r); EXTMACRO_END
#define SetOperand1AndFlags(res) EXTMACRO_BEGIN uint32_t r = (res); SetFlags(r); SetOperand1(r); EXTMACRO_END

#define NextInstruction() { opcode++; continue; }
#define Jump(offs) { uint32_t o = (offs); if (o >= prog->length) return false; opcode = &prog->opcodes[o]; continue; }

bool RARExecuteProgram(RARVirtualMachine *vm, RARProgram *prog)
{
    RAROpcode *opcode = prog->opcodes;
    uint32_t flags = 0;
    uint32_t op1, op2, carry, i;
    uint32_t counter = 0;

    if (!RARIsProgramTerminated(prog))
        return false;

    while ((uint32_t)(opcode - prog->opcodes) < prog->length && counter++ < RARRuntimeMaxInstructions) {
        switch (opcode->instruction) {
        case RARMovInstruction:
            SetOperand1(GetOperand2());
            NextInstruction();

        case RARCmpInstruction:
            op1 = GetOperand1();
            SetFlagsWithCarry(op1 - GetOperand2(), result > op1);
            NextInstruction();

        case RARAddInstruction:
            op1 = GetOperand1();
            if (opcode->bytemode)
                SetOperand1AndByteFlagsWithCarry((op1 + GetOperand2()) & 0xFF, result < op1);
            else
                SetOperand1AndFlagsWithCarry(op1 + GetOperand2(), result < op1);
            NextInstruction();

        case RARSubInstruction:
            op1 = GetOperand1();
#if 0 /* apparently not correctly implemented in the RAR VM */
            if (opcode->bytemode)
                SetOperand1AndByteFlagsWithCarry((op1 - GetOperand2()) & 0xFF, result > op1);
            else
#endif
            SetOperand1AndFlagsWithCarry(op1 - GetOperand2(), result > op1);
            NextInstruction();

        case RARJzInstruction:
            if ((flags & ZeroFlag))
                Jump(GetOperand1());
            NextInstruction();

        case RARJnzInstruction:
            if (!(flags & ZeroFlag))
                Jump(GetOperand1());
            NextInstruction();

        case RARIncInstruction:
            if (opcode->bytemode)
                SetOperand1AndFlags((GetOperand1() + 1) & 0xFF);
            else
                SetOperand1AndFlags(GetOperand1() + 1);
            NextInstruction();

        case RARDecInstruction:
            if (opcode->bytemode)
                SetOperand1AndFlags((GetOperand1() - 1) & 0xFF);
            else
                SetOperand1AndFlags(GetOperand1() - 1);
            NextInstruction();

        case RARJmpInstruction:
            Jump(GetOperand1());

        case RARXorInstruction:
            SetOperand1AndFlags(GetOperand1() ^ GetOperand2());
            NextInstruction();

        case RARAndInstruction:
            SetOperand1AndFlags(GetOperand1() & GetOperand2());
            NextInstruction();

        case RAROrInstruction:
            SetOperand1AndFlags(GetOperand1() | GetOperand2());
            NextInstruction();

        case RARTestInstruction:
            SetFlags(GetOperand1() & GetOperand2());
            NextInstruction();

        case RARJsInstruction:
            if ((flags & SignFlag))
                Jump(GetOperand1());
            NextInstruction();

        case RARJnsInstruction:
            if (!(flags & SignFlag))
                Jump(GetOperand1());
            NextInstruction();

        case RARJbInstruction:
            if ((flags & CarryFlag))
                Jump(GetOperand1());
            NextInstruction();

        case RARJbeInstruction:
            if ((flags & (CarryFlag | ZeroFlag)))
                Jump(GetOperand1());
            NextInstruction();

        case RARJaInstruction:
            if (!(flags & (CarryFlag | ZeroFlag)))
                Jump(GetOperand1());
            NextInstruction();

        case RARJaeInstruction:
            if (!(flags & CarryFlag))
                Jump(GetOperand1());
            NextInstruction();

        case RARPushInstruction:
            vm->registers[7] -= 4;
            RARVirtualMachineWrite32(vm, vm->registers[7], GetOperand1());
            NextInstruction();

        case RARPopInstruction:
            SetOperand1(RARVirtualMachineRead32(vm, vm->registers[7]));
            vm->registers[7] += 4;
            NextInstruction();

        case RARCallInstruction:
            vm->registers[7] -= 4;
            RARVirtualMachineWrite32(vm, vm->registers[7], (uint32_t)(opcode - prog->opcodes + 1));
            Jump(GetOperand1());

        case RARRetInstruction:
            if (vm->registers[7] >= RARProgramMemorySize)
                return true;
            i = RARVirtualMachineRead32(vm, vm->registers[7]);
            vm->registers[7] += 4;
            Jump(i);

        case RARNotInstruction:
            SetOperand1(~GetOperand1());
            NextInstruction();

        case RARShlInstruction:
            op1 = GetOperand1();
            op2 = GetOperand2();
            SetOperand1AndFlagsWithCarry(op1 << op2, ((op1 << (op2 - 1)) & 0x80000000) != 0);
            NextInstruction();

        case RARShrInstruction:
            op1 = GetOperand1();
            op2 = GetOperand2();
            SetOperand1AndFlagsWithCarry(op1 >> op2, ((op1 >> (op2 - 1)) & 1) != 0);
            NextInstruction();

        case RARSarInstruction:
            op1 = GetOperand1();
            op2 = GetOperand2();
            SetOperand1AndFlagsWithCarry(((int32_t)op1) >> op2, ((op1 >> (op2 - 1)) & 1) != 0);
            NextInstruction();

        case RARNegInstruction:
            SetOperand1AndFlagsWithCarry(-(int32_t)GetOperand1(), result != 0);
            NextInstruction();

        case RARPushaInstruction:
            vm->registers[7] -= 32;
            for (i = 0; i < 8; i++)
                RARVirtualMachineWrite32(vm, vm->registers[7] + (7 - i) * 4, vm->registers[i]);
            NextInstruction();

        case RARPopaInstruction:
            for (i = 0; i < 8; i++)
                vm->registers[i] = RARVirtualMachineRead32(vm, vm->registers[7] + (7 - i) * 4);
            vm->registers[7] += 32;
            NextInstruction();

        case RARPushfInstruction:
            vm->registers[7] -= 4;
            RARVirtualMachineWrite32(vm, vm->registers[7], flags);
            NextInstruction();

        case RARPopfInstruction:
            flags = RARVirtualMachineRead32(vm, vm->registers[7]);
            vm->registers[7] += 4;
            NextInstruction();

        case RARMovzxInstruction:
            SetOperand1(GetOperand2());
            NextInstruction();

        case RARMovsxInstruction:
            SetOperand1(SignExtend(GetOperand2()));
            NextInstruction();

        case RARXchgInstruction:
            op1 = GetOperand1();
            op2 = GetOperand2();
            SetOperand1(op2);
            SetOperand2(op1);
            NextInstruction();

        case RARMulInstruction:
            SetOperand1(GetOperand1() * GetOperand2());
            NextInstruction();

        case RARDivInstruction:
            op2 = GetOperand2();
            if (op2 != 0)
                SetOperand1(GetOperand1() / op2);
            NextInstruction();

        case RARAdcInstruction:
            op1 = GetOperand1();
            carry = (flags & CarryFlag);
            if (opcode->bytemode)
                SetOperand1AndFlagsWithCarry((op1 + GetOperand2() + carry) & 0xFF, result < op1 || (result == op1 && carry)); /* does not correctly set sign bit */
            else
                SetOperand1AndFlagsWithCarry(op1 + GetOperand2() + carry, result < op1 || (result == op1 && carry));
            NextInstruction();

        case RARSbbInstruction:
            op1 = GetOperand1();
            carry = (flags & CarryFlag);
            if (opcode->bytemode)
                SetOperand1AndFlagsWithCarry((op1 - GetOperand2() - carry) & 0xFF, result > op1 || (result == op1 && carry)); /* does not correctly set sign bit */
            else
                SetOperand1AndFlagsWithCarry(op1 - GetOperand2() - carry, result > op1 || (result == op1 && carry));
            NextInstruction();

        case RARPrintInstruction:
            /* TODO: ??? */
            NextInstruction();
        }
    }

    return false;
}

/* Memory and register access */

static uint32_t _RARRead32(const uint8_t *b)
{
    return ((uint32_t)b[3] << 24) | ((uint32_t)b[2] << 16) | ((uint32_t)b[1] << 8) | (uint32_t)b[0];
}

static void _RARWrite32(uint8_t *b, uint32_t n)
{
    b[3] = (n >> 24) & 0xFF;
    b[2] = (n >> 16) & 0xFF;
    b[1] = (n >> 8) & 0xFF;
    b[0] = n & 0xFF;
}

void RARSetVirtualMachineRegisters(RARVirtualMachine *vm, uint32_t registers[8])
{
    if (registers)
        memcpy(vm->registers, registers, sizeof(vm->registers));
    else
        memset(vm->registers, 0, sizeof(vm->registers));
}

uint32_t RARVirtualMachineRead32(RARVirtualMachine *vm, uint32_t address)
{
    return _RARRead32(&vm->memory[address & RARProgramMemoryMask]);
}

void RARVirtualMachineWrite32(RARVirtualMachine *vm, uint32_t address, uint32_t val)
{
    _RARWrite32(&vm->memory[address & RARProgramMemoryMask], val);
}

uint8_t RARVirtualMachineRead8(RARVirtualMachine *vm, uint32_t address)
{
    return vm->memory[address & RARProgramMemoryMask];
}

void RARVirtualMachineWrite8(RARVirtualMachine *vm, uint32_t address, uint8_t val)
{
    vm->memory[address & RARProgramMemoryMask] = val;
}

static uint32_t _RARGetOperand(RARVirtualMachine *vm, uint8_t addressingmode, uint32_t value, bool bytemode)
{
    if (RARRegisterAddressingMode(0) <= addressingmode && addressingmode <= RARRegisterAddressingMode(7)) {
        uint32_t result = vm->registers[addressingmode % 8];
        if (bytemode)
            result = result & 0xFF;
        return result;
    }
    if (RARRegisterIndirectAddressingMode(0) <= addressingmode && addressingmode <= RARRegisterIndirectAddressingMode(7)) {
        if (bytemode)
            return RARVirtualMachineRead8(vm, vm->registers[addressingmode % 8]);
        return RARVirtualMachineRead32(vm, vm->registers[addressingmode % 8]);
    }
    if (RARIndexedAbsoluteAddressingMode(0) <= addressingmode && addressingmode <= RARIndexedAbsoluteAddressingMode(7)) {
        if (bytemode)
            return RARVirtualMachineRead8(vm, value + vm->registers[addressingmode % 8]);
        return RARVirtualMachineRead32(vm, value + vm->registers[addressingmode % 8]);
    }
    if (addressingmode == RARAbsoluteAddressingMode) {
        if (bytemode)
            return RARVirtualMachineRead8(vm, value);
        return RARVirtualMachineRead32(vm, value);
    }
    /* if (addressingmode == RARImmediateAddressingMode) */
    return value;
}

static void _RARSetOperand(RARVirtualMachine *vm, uint8_t addressingmode, uint32_t value, bool bytemode, uint32_t data)
{
    if (RARRegisterAddressingMode(0) <= addressingmode && addressingmode <= RARRegisterAddressingMode(7)) {
        if (bytemode)
            data = data & 0xFF;
        vm->registers[addressingmode % 8] = data;
    }
    else if (RARRegisterIndirectAddressingMode(0) <= addressingmode && addressingmode <= RARRegisterIndirectAddressingMode(7)) {
        if (bytemode)
            RARVirtualMachineWrite8(vm, vm->registers[addressingmode % 8], (uint8_t)data);
        else
            RARVirtualMachineWrite32(vm, vm->registers[addressingmode % 8], data);
    }
    else if (RARIndexedAbsoluteAddressingMode(0) <= addressingmode && addressingmode <= RARIndexedAbsoluteAddressingMode(7)) {
        if (bytemode)
            RARVirtualMachineWrite8(vm, value + vm->registers[addressingmode % 8], (uint8_t)data);
        else
            RARVirtualMachineWrite32(vm, value + vm->registers[addressingmode % 8], data);
    }
    else if (addressingmode == RARAbsoluteAddressingMode) {
        if (bytemode)
            RARVirtualMachineWrite8(vm, value, (uint8_t)data);
        else
            RARVirtualMachineWrite32(vm, value, data);
    }
}

/* Instruction properties */

#define RAR0OperandsFlag 0
#define RAR1OperandFlag 1
#define RAR2OperandsFlag 2
#define RAROperandsFlag 3
#define RARHasByteModeFlag 4
#define RARIsUnconditionalJumpFlag 8
#define RARIsRelativeJumpFlag 16
#define RARWritesFirstOperandFlag 32
#define RARWritesSecondOperandFlag 64
#define RARReadsStatusFlag 128
#define RARWritesStatusFlag 256

static const int InstructionFlags[RARNumberOfInstructions] = {
    /*RARMovInstruction*/ RAR2OperandsFlag | RARHasByteModeFlag | RARWritesFirstOperandFlag,
    /*RARCmpInstruction*/ RAR2OperandsFlag | RARHasByteModeFlag | RARWritesStatusFlag,
    /*RARAddInstruction*/ RAR2OperandsFlag | RARHasByteModeFlag | RARWritesFirstOperandFlag | RARWritesStatusFlag,
    /*RARSubInstruction*/ RAR2OperandsFlag | RARHasByteModeFlag | RARWritesFirstOperandFlag | RARWritesStatusFlag,
    /*RARJzInstruction*/ RAR1OperandFlag | RARIsUnconditionalJumpFlag | RARIsRelativeJumpFlag | RARReadsStatusFlag,
    /*RARJnzInstruction*/ RAR1OperandFlag | RARIsRelativeJumpFlag | RARReadsStatusFlag,
    /*RARIncInstruction*/ RAR1OperandFlag | RARHasByteModeFlag | RARWritesFirstOperandFlag | RARWritesStatusFlag,
    /*RARDecInstruction*/ RAR1OperandFlag | RARHasByteModeFlag | RARWritesFirstOperandFlag | RARWritesStatusFlag,
    /*RARJmpInstruction*/ RAR1OperandFlag | RARIsRelativeJumpFlag,
    /*RARXorInstruction*/ RAR2OperandsFlag | RARHasByteModeFlag | RARWritesFirstOperandFlag | RARWritesStatusFlag,
    /*RARAndInstruction*/ RAR2OperandsFlag | RARHasByteModeFlag | RARWritesFirstOperandFlag | RARWritesStatusFlag,
    /*RAROrInstruction*/ RAR2OperandsFlag | RARHasByteModeFlag | RARWritesFirstOperandFlag | RARWritesStatusFlag,
    /*RARTestInstruction*/ RAR2OperandsFlag | RARHasByteModeFlag | RARWritesStatusFlag,
    /*RARJsInstruction*/ RAR1OperandFlag | RARIsRelativeJumpFlag | RARReadsStatusFlag,
    /*RARJnsInstruction*/ RAR1OperandFlag | RARIsRelativeJumpFlag | RARReadsStatusFlag,
    /*RARJbInstruction*/ RAR1OperandFlag | RARIsRelativeJumpFlag | RARReadsStatusFlag,
    /*RARJbeInstruction*/ RAR1OperandFlag | RARIsRelativeJumpFlag | RARReadsStatusFlag,
    /*RARJaInstruction*/ RAR1OperandFlag | RARIsRelativeJumpFlag | RARReadsStatusFlag,
    /*RARJaeInstruction*/ RAR1OperandFlag | RARIsRelativeJumpFlag | RARReadsStatusFlag,
    /*RARPushInstruction*/ RAR1OperandFlag,
    /*RARPopInstruction*/ RAR1OperandFlag,
    /*RARCallInstruction*/ RAR1OperandFlag | RARIsRelativeJumpFlag,
    /*RARRetInstruction*/ RAR0OperandsFlag | RARIsUnconditionalJumpFlag,
    /*RARNotInstruction*/ RAR1OperandFlag | RARHasByteModeFlag | RARWritesFirstOperandFlag,
    /*RARShlInstruction*/ RAR2OperandsFlag | RARHasByteModeFlag | RARWritesFirstOperandFlag | RARWritesStatusFlag,
    /*RARShrInstruction*/ RAR2OperandsFlag | RARHasByteModeFlag | RARWritesFirstOperandFlag | RARWritesStatusFlag,
    /*RARSarInstruction*/ RAR2OperandsFlag | RARHasByteModeFlag | RARWritesFirstOperandFlag | RARWritesStatusFlag,
    /*RARNegInstruction*/ RAR1OperandFlag | RARHasByteModeFlag | RARWritesFirstOperandFlag | RARWritesStatusFlag,
    /*RARPushaInstruction*/ RAR0OperandsFlag,
    /*RARPopaInstruction*/ RAR0OperandsFlag,
    /*RARPushfInstruction*/ RAR0OperandsFlag | RARReadsStatusFlag,
    /*RARPopfInstruction*/ RAR0OperandsFlag | RARWritesStatusFlag,
    /*RARMovzxInstruction*/ RAR2OperandsFlag | RARWritesFirstOperandFlag,
    /*RARMovsxInstruction*/ RAR2OperandsFlag | RARWritesFirstOperandFlag,
    /*RARXchgInstruction*/ RAR2OperandsFlag | RARWritesFirstOperandFlag | RARWritesSecondOperandFlag | RARHasByteModeFlag,
    /*RARMulInstruction*/ RAR2OperandsFlag | RARHasByteModeFlag | RARWritesFirstOperandFlag,
    /*RARDivInstruction*/ RAR2OperandsFlag | RARHasByteModeFlag | RARWritesFirstOperandFlag,
    /*RARAdcInstruction*/ RAR2OperandsFlag | RARHasByteModeFlag | RARWritesFirstOperandFlag | RARReadsStatusFlag | RARWritesStatusFlag,
    /*RARSbbInstruction*/ RAR2OperandsFlag | RARHasByteModeFlag | RARWritesFirstOperandFlag | RARReadsStatusFlag | RARWritesStatusFlag,
    /*RARPrintInstruction*/ RAR0OperandsFlag
};

int NumberOfRARInstructionOperands(uint8_t instruction)
{
    if (instruction >= RARNumberOfInstructions)
        return 0;
    return InstructionFlags[instruction] & RAROperandsFlag;
}

bool RARInstructionHasByteMode(uint8_t instruction)
{
    if (instruction >= RARNumberOfInstructions)
        return false;
    return (InstructionFlags[instruction] & RARHasByteModeFlag)!=0;
}

bool RARInstructionIsUnconditionalJump(uint8_t instruction)
{
    if (instruction >= RARNumberOfInstructions)
        return false;
    return (InstructionFlags[instruction] & RARIsUnconditionalJumpFlag) != 0;
}

bool RARInstructionIsRelativeJump(uint8_t instruction)
{
    if (instruction >= RARNumberOfInstructions)
        return false;
    return (InstructionFlags[instruction] & RARIsRelativeJumpFlag) != 0;
}

bool RARInstructionWritesFirstOperand(uint8_t instruction)
{
    if (instruction >= RARNumberOfInstructions)
        return false;
    return (InstructionFlags[instruction] & RARWritesFirstOperandFlag) != 0;
}

bool RARInstructionWritesSecondOperand(uint8_t instruction)
{
    if (instruction >= RARNumberOfInstructions)
        return false;
    return (InstructionFlags[instruction] & RARWritesSecondOperandFlag) != 0;
}

/* Program debugging */

#ifndef NDEBUG
#include <stdio.h>

static void RARPrintOperand(uint8_t addressingmode, uint32_t value)
{
    if (RARRegisterAddressingMode(0) <= addressingmode && addressingmode <= RARRegisterAddressingMode(7))
        printf("r%d", addressingmode % 8);
    else if (RARRegisterIndirectAddressingMode(0) <= addressingmode && addressingmode <= RARRegisterIndirectAddressingMode(7))
        printf("@(r%d)", addressingmode % 8);
    else if (RARIndexedAbsoluteAddressingMode(0) <= addressingmode && addressingmode <= RARIndexedAbsoluteAddressingMode(7))
        printf("@(r%d+$%02x)", addressingmode % 8, value);
    else if (addressingmode == RARAbsoluteAddressingMode)
        printf("@($%02x)", value);
    else if (addressingmode == RARImmediateAddressingMode)
        printf("$%02x", value);
}

void RARPrintProgram(RARProgram *prog)
{
    static const char *instructionNames[RARNumberOfInstructions] = {
        "Mov", "Cmp", "Add", "Sub", "Jz", "Jnz", "Inc", "Dec", "Jmp", "Xor",
        "And", "Or", "Test", "Js", "Jns", "Jb", "Jbe", "Ja", "Jae", "Push",
        "Pop", "Call", "Ret", "Not", "Shl", "Shr", "Sar", "Neg", "Pusha", "Popa",
        "Pushf", "Popf", "Movzx", "Movsx", "Xchg", "Mul", "Div", "Adc", "Sbb", "Print",
    };

    uint32_t i;
    for (i = 0; i < prog->length; i++) {
        RAROpcode *opcode = &prog->opcodes[i];
        int numoperands = NumberOfRARInstructionOperands(opcode->instruction);
        printf("  %02x: %s", i, instructionNames[opcode->instruction]);
        if (opcode->bytemode)
            printf("B");
        if (numoperands >= 1) {
            printf(" ");
            RARPrintOperand(opcode->addressingmode1, opcode->value1);
        }
        if (numoperands == 2) {
            printf(", ");
            RARPrintOperand(opcode->addressingmode2, opcode->value2);
        }
        printf("\n");
    }
}
#endif
