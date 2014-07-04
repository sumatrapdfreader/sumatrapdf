/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

// adapted from https://code.google.com/p/theunarchiver/source/browse/XADMaster/RARVirtualMachine.h

#include "vm.h"

#include <stdlib.h>
#include <string.h>

typedef uint32_t (* RARGetterFunction)(RARVirtualMachine *vm, uint32_t value);
typedef void (* RARSetterFunction)(RARVirtualMachine *vm, uint32_t value, uint32_t data);

typedef struct RAROpcode_s RAROpcode;

struct RAROpcode_s {
    RARGetterFunction operand1getter;
    RARSetterFunction operand1setter;
    uint32_t value1;

    RARGetterFunction operand2getter;
    RARSetterFunction operand2setter;
    uint32_t value2;

    uint8_t instruction;
    uint8_t bytemode;
    uint8_t addressingmode1;
    uint8_t addressingmode2;
};

struct RARProgram_s {
    RAROpcode *opcodes;
    uint32_t length;
    uint32_t capacity;
};

static RARGetterFunction OperandGetters_32[RARNumberOfAddressingModes];
static RARGetterFunction OperandGetters_8[RARNumberOfAddressingModes];
static RARSetterFunction OperandSetters_32[RARNumberOfAddressingModes];
static RARSetterFunction OperandSetters_8[RARNumberOfAddressingModes];

// Program building

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
    if (prog->length + 1 >= prog->capacity) {
        uint32_t newCapacity = prog->capacity ? prog->capacity * 2 : 16;
        RAROpcode *newCodes = realloc(prog->opcodes, newCapacity * sizeof(*prog->opcodes));
        if (!newCodes)
            return false;
        prog->opcodes = newCodes;
        prog->capacity = newCapacity;
    }
    memset(&prog->opcodes[prog->length], 0, sizeof(prog->opcodes[prog->length]));
    prog->opcodes[prog->length].instruction = instruction;
    prog->opcodes[prog->length].bytemode = bytemode ? 1 : 0;
    prog->length++;
    return true;
}

bool RARSetLastInstrOperands(RARProgram *prog, uint8_t addressingmode1, uint32_t value1, uint8_t addressingmode2, uint32_t value2)
{
    RAROpcode *opcode = &prog->opcodes[prog->length - 1];
    RARGetterFunction *getterfunctions;
    RARSetterFunction *setterfunctions;
    int numoperands;

    if (addressingmode1 >= RARNumberOfAddressingModes || addressingmode2 >= RARNumberOfAddressingModes)
        return false;
    if (!prog->length || opcode->addressingmode1 || opcode->value1 || opcode->addressingmode2 || opcode->value2)
        return false;

    numoperands = NumberOfRARInstructionOperands(opcode->instruction);
    if (numoperands == 0)
        return true;

    if (opcode->instruction == RARMovsxInstruction || opcode->instruction == RARMovzxInstruction) {
        getterfunctions = OperandGetters_8;
        setterfunctions = OperandSetters_32;
    }
    else if (opcode->bytemode) {
        if (!RARInstructionHasByteMode(opcode->instruction))
            return false;
        getterfunctions = OperandGetters_8;
        setterfunctions = OperandSetters_8;
    }
    else {
        getterfunctions = OperandGetters_32;
        setterfunctions = OperandSetters_32;
    }

    opcode->operand1getter = getterfunctions[addressingmode1];
    opcode->operand1setter = setterfunctions[addressingmode1];
    if (addressingmode1 == RARImmediateAddressingMode) {
        if (RARInstructionWritesFirstOperand(opcode->instruction))
            return false;
    }
    else if (addressingmode1 == RARAbsoluteAddressingMode) {
        value1 &= RARProgramMemoryMask;
    }
    opcode->addressingmode1 = addressingmode1;
    opcode->value1 = value1;

    if (numoperands == 2) {
        opcode->operand2getter = getterfunctions[addressingmode2];
        opcode->operand2setter = setterfunctions[addressingmode2];
        if (addressingmode2 == RARImmediateAddressingMode) {
            if (RARInstructionWritesSecondOperand(opcode->instruction))
                return false;
        }
        else if (addressingmode2 == RARAbsoluteAddressingMode) {
            value2 &= RARProgramMemoryMask;
        }
        opcode->addressingmode2 = addressingmode2;
        opcode->value2 = value2;
    }

    return true;
}

bool RARIsProgramTerminated(RARProgram *prog)
{
    return prog->length > 0 && RARInstructionIsUnconditionalJump(prog->opcodes[prog->length - 1].instruction);
}

// Execution

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

#define GetOperand1() (opcode->operand1getter(vm, opcode->value1))
#define GetOperand2() (opcode->operand2getter(vm, opcode->value2))
#define SetOperand1(data) opcode->operand1setter(vm, opcode->value1, data)
#define SetOperand2(data) opcode->operand2setter(vm, opcode->value2, data)

#define SetFlagsWithCarry(res, carry) EXTMACRO_BEGIN uint32_t result = (res); flags = (result == 0 ? ZeroFlag : (result & SignFlag)) | ((carry) ? 1 : 0); EXTMACRO_END
#define SetByteFlagsWithCarry(res, carry) EXTMACRO_BEGIN uint8_t result = (res); flags = (result == 0 ? ZeroFlag : (SignExtend(result) & SignFlag)) | ((carry) ? 1 : 0); EXTMACRO_END
#define SetFlags(res) SetFlagsWithCarry(res, 0)

#define SetOperand1AndFlagsWithCarry(res, carry) EXTMACRO_BEGIN uint32_t r = (res); SetFlagsWithCarry(r, carry); SetOperand1(r); EXTMACRO_END
#define SetOperand1AndByteFlagsWithCarry(res, carry) EXTMACRO_BEGIN uint8_t r = (res); SetByteFlagsWithCarry(r, carry); SetOperand1(r); EXTMACRO_END
#define SetOperand1AndFlags(res) EXTMACRO_BEGIN uint32_t r = (res); SetFlags(r); SetOperand1(r); EXTMACRO_END

#define NextInstruction() { opcode++; continue; }
#define Jump(offs) { uint32_t o = (offs); if (o >= prog->length) return false; opcode = &prog->opcodes[o]; continue; }

bool RARExecuteProgram(RARVirtualMachine *vm, RARProgram *prog)
{
    RAROpcode *opcode = prog->opcodes;
    uint32_t flags = vm->flags;
    uint32_t op1, op2, carry, i;
    uint32_t counter = 0;

    if (!RARIsProgramTerminated(prog))
        return false;

    vm->flags = 0; // ?

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
#if 0 // apparently not correctly implemented in the RAR VM
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
            RARVirtualMachineWrite32(vm, vm->registers[7], opcode - prog->opcodes + 1);
            Jump(GetOperand1());

        case RARRetInstruction:
            if (vm->registers[7] >= RARProgramMemorySize) {
                vm->flags = flags;
                return true;
            }
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
            for (i = 0; i < 8; i++) {
                RARVirtualMachineWrite32(vm, vm->registers[7] - 4 - i * 4, vm->registers[i]);
            }
            vm->registers[7] -= 32;
            NextInstruction();

        case RARPopaInstruction:
            for (i = 0; i < 8; i++) {
                vm->registers[i] = RARVirtualMachineRead32(vm, vm->registers[7] + 28 - i * 4);
            }
            // TODO: vm->registers[7] += 32; ?
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
                SetOperand1AndFlagsWithCarry((op1 + GetOperand2() + carry) & 0xFF, result < op1 || (result == op1 && carry)); // does not correctly set sign bit
            else
                SetOperand1AndFlagsWithCarry(op1 + GetOperand2() + carry, result < op1 || (result == op1 && carry));
            NextInstruction();

        case RARSbbInstruction:
            op1 = GetOperand1();
            carry = (flags & CarryFlag);
            if (opcode->bytemode)
                SetOperand1AndFlagsWithCarry((op1 - GetOperand2() - carry) & 0xFF, result > op1 || (result == op1 && carry)); // does not correctly set sign bit
            else
                SetOperand1AndFlagsWithCarry(op1 - GetOperand2() - carry, result > op1 || (result == op1 && carry));
            NextInstruction();

        case RARPrintInstruction:
            // TODO: ???
            NextInstruction();
        }
    }

    return false;
}

// Memory and register access

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

static uint32_t RegisterGetter0_32(RARVirtualMachine *vm, uint32_t value) { return vm->registers[0]; }
static uint32_t RegisterGetter1_32(RARVirtualMachine *vm, uint32_t value) { return vm->registers[1]; }
static uint32_t RegisterGetter2_32(RARVirtualMachine *vm, uint32_t value) { return vm->registers[2]; }
static uint32_t RegisterGetter3_32(RARVirtualMachine *vm, uint32_t value) { return vm->registers[3]; }
static uint32_t RegisterGetter4_32(RARVirtualMachine *vm, uint32_t value) { return vm->registers[4]; }
static uint32_t RegisterGetter5_32(RARVirtualMachine *vm, uint32_t value) { return vm->registers[5]; }
static uint32_t RegisterGetter6_32(RARVirtualMachine *vm, uint32_t value) { return vm->registers[6]; }
static uint32_t RegisterGetter7_32(RARVirtualMachine *vm, uint32_t value) { return vm->registers[7]; }
static uint32_t RegisterGetter0_8(RARVirtualMachine *vm, uint32_t value) { return vm->registers[0] & 0xFF; }
static uint32_t RegisterGetter1_8(RARVirtualMachine *vm, uint32_t value) { return vm->registers[1] & 0xFF; }
static uint32_t RegisterGetter2_8(RARVirtualMachine *vm, uint32_t value) { return vm->registers[2] & 0xFF; }
static uint32_t RegisterGetter3_8(RARVirtualMachine *vm, uint32_t value) { return vm->registers[3] & 0xFF; }
static uint32_t RegisterGetter4_8(RARVirtualMachine *vm, uint32_t value) { return vm->registers[4] & 0xFF; }
static uint32_t RegisterGetter5_8(RARVirtualMachine *vm, uint32_t value) { return vm->registers[5] & 0xFF; }
static uint32_t RegisterGetter6_8(RARVirtualMachine *vm, uint32_t value) { return vm->registers[6] & 0xFF; }
static uint32_t RegisterGetter7_8(RARVirtualMachine *vm, uint32_t value) { return vm->registers[7] & 0xFF; }

static uint32_t RegisterIndirectGetter0_32(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead32(vm, vm->registers[0]); }
static uint32_t RegisterIndirectGetter1_32(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead32(vm, vm->registers[1]); }
static uint32_t RegisterIndirectGetter2_32(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead32(vm, vm->registers[2]); }
static uint32_t RegisterIndirectGetter3_32(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead32(vm, vm->registers[3]); }
static uint32_t RegisterIndirectGetter4_32(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead32(vm, vm->registers[4]); }
static uint32_t RegisterIndirectGetter5_32(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead32(vm, vm->registers[5]); }
static uint32_t RegisterIndirectGetter6_32(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead32(vm, vm->registers[6]); }
static uint32_t RegisterIndirectGetter7_32(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead32(vm, vm->registers[7]); }
static uint32_t RegisterIndirectGetter0_8(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead8(vm, vm->registers[0]); }
static uint32_t RegisterIndirectGetter1_8(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead8(vm, vm->registers[1]); }
static uint32_t RegisterIndirectGetter2_8(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead8(vm, vm->registers[2]); }
static uint32_t RegisterIndirectGetter3_8(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead8(vm, vm->registers[3]); }
static uint32_t RegisterIndirectGetter4_8(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead8(vm, vm->registers[4]); }
static uint32_t RegisterIndirectGetter5_8(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead8(vm, vm->registers[5]); }
static uint32_t RegisterIndirectGetter6_8(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead8(vm, vm->registers[6]); }
static uint32_t RegisterIndirectGetter7_8(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead8(vm, vm->registers[7]); }

static uint32_t IndexedAbsoluteGetter0_32(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead32(vm, value + vm->registers[0]); }
static uint32_t IndexedAbsoluteGetter1_32(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead32(vm, value + vm->registers[1]); }
static uint32_t IndexedAbsoluteGetter2_32(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead32(vm, value + vm->registers[2]); }
static uint32_t IndexedAbsoluteGetter3_32(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead32(vm, value + vm->registers[3]); }
static uint32_t IndexedAbsoluteGetter4_32(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead32(vm, value + vm->registers[4]); }
static uint32_t IndexedAbsoluteGetter5_32(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead32(vm, value + vm->registers[5]); }
static uint32_t IndexedAbsoluteGetter6_32(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead32(vm, value + vm->registers[6]); }
static uint32_t IndexedAbsoluteGetter7_32(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead32(vm, value + vm->registers[7]); }
static uint32_t IndexedAbsoluteGetter0_8(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead8(vm, value + vm->registers[0]); }
static uint32_t IndexedAbsoluteGetter1_8(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead8(vm, value + vm->registers[1]); }
static uint32_t IndexedAbsoluteGetter2_8(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead8(vm, value + vm->registers[2]); }
static uint32_t IndexedAbsoluteGetter3_8(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead8(vm, value + vm->registers[3]); }
static uint32_t IndexedAbsoluteGetter4_8(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead8(vm, value + vm->registers[4]); }
static uint32_t IndexedAbsoluteGetter5_8(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead8(vm, value + vm->registers[5]); }
static uint32_t IndexedAbsoluteGetter6_8(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead8(vm, value + vm->registers[6]); }
static uint32_t IndexedAbsoluteGetter7_8(RARVirtualMachine *vm, uint32_t value) { return RARVirtualMachineRead8(vm, value + vm->registers[7]); }

// Note: Absolute addressing is pre-masked in RARSetLastInstrOperands.
static uint32_t AbsoluteGetter_32(RARVirtualMachine *vm, uint32_t value) { return _RARRead32(&vm->memory[value]); }
static uint32_t AbsoluteGetter_8(RARVirtualMachine *vm, uint32_t value) { return vm->memory[value]; }
static uint32_t ImmediateGetter(RARVirtualMachine *vm, uint32_t value) { return value; }

static void RegisterSetter0_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { vm->registers[0] = data; }
static void RegisterSetter1_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { vm->registers[1] = data; }
static void RegisterSetter2_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { vm->registers[2] = data; }
static void RegisterSetter3_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { vm->registers[3] = data; }
static void RegisterSetter4_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { vm->registers[4] = data; }
static void RegisterSetter5_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { vm->registers[5] = data; }
static void RegisterSetter6_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { vm->registers[6] = data; }
static void RegisterSetter7_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { vm->registers[7] = data; }
static void RegisterSetter0_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { vm->registers[0] = data & 0xFF; }
static void RegisterSetter1_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { vm->registers[1] = data & 0xFF; }
static void RegisterSetter2_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { vm->registers[2] = data & 0xFF; }
static void RegisterSetter3_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { vm->registers[3] = data & 0xFF; }
static void RegisterSetter4_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { vm->registers[4] = data & 0xFF; }
static void RegisterSetter5_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { vm->registers[5] = data & 0xFF; }
static void RegisterSetter6_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { vm->registers[6] = data & 0xFF; }
static void RegisterSetter7_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { vm->registers[7] = data & 0xFF; }

static void RegisterIndirectSetter0_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(vm, vm->registers[0], data); }
static void RegisterIndirectSetter1_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(vm, vm->registers[1], data); }
static void RegisterIndirectSetter2_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(vm, vm->registers[2], data); }
static void RegisterIndirectSetter3_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(vm, vm->registers[3], data); }
static void RegisterIndirectSetter4_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(vm, vm->registers[4], data); }
static void RegisterIndirectSetter5_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(vm, vm->registers[5], data); }
static void RegisterIndirectSetter6_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(vm, vm->registers[6], data); }
static void RegisterIndirectSetter7_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(vm, vm->registers[7], data); }
static void RegisterIndirectSetter0_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(vm, vm->registers[0], (uint8_t)data); }
static void RegisterIndirectSetter1_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(vm, vm->registers[1], (uint8_t)data); }
static void RegisterIndirectSetter2_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(vm, vm->registers[2], (uint8_t)data); }
static void RegisterIndirectSetter3_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(vm, vm->registers[3], (uint8_t)data); }
static void RegisterIndirectSetter4_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(vm, vm->registers[4], (uint8_t)data); }
static void RegisterIndirectSetter5_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(vm, vm->registers[5], (uint8_t)data); }
static void RegisterIndirectSetter6_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(vm, vm->registers[6], (uint8_t)data); }
static void RegisterIndirectSetter7_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(vm, vm->registers[7], (uint8_t)data); }

static void IndexedAbsoluteSetter0_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(vm, value + vm->registers[0], data); }
static void IndexedAbsoluteSetter1_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(vm, value + vm->registers[1], data); }
static void IndexedAbsoluteSetter2_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(vm, value + vm->registers[2], data); }
static void IndexedAbsoluteSetter3_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(vm, value + vm->registers[3], data); }
static void IndexedAbsoluteSetter4_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(vm, value + vm->registers[4], data); }
static void IndexedAbsoluteSetter5_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(vm, value + vm->registers[5], data); }
static void IndexedAbsoluteSetter6_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(vm, value + vm->registers[6], data); }
static void IndexedAbsoluteSetter7_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(vm, value + vm->registers[7], data); }
static void IndexedAbsoluteSetter0_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(vm, value + vm->registers[0], (uint8_t)data); }
static void IndexedAbsoluteSetter1_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(vm, value + vm->registers[1], (uint8_t)data); }
static void IndexedAbsoluteSetter2_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(vm, value + vm->registers[2], (uint8_t)data); }
static void IndexedAbsoluteSetter3_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(vm, value + vm->registers[3], (uint8_t)data); }
static void IndexedAbsoluteSetter4_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(vm, value + vm->registers[4], (uint8_t)data); }
static void IndexedAbsoluteSetter5_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(vm, value + vm->registers[5], (uint8_t)data); }
static void IndexedAbsoluteSetter6_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(vm, value + vm->registers[6], (uint8_t)data); }
static void IndexedAbsoluteSetter7_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(vm, value + vm->registers[7], (uint8_t)data); }

// Note: Absolute addressing is pre-masked in RARSetLastInstrOperands.
static void AbsoluteSetter_32(RARVirtualMachine *vm, uint32_t value, uint32_t data) { _RARWrite32(&vm->memory[value], data); }
static void AbsoluteSetter_8(RARVirtualMachine *vm, uint32_t value, uint32_t data) { vm->memory[value] = (uint8_t)data; }

static RARGetterFunction OperandGetters_32[RARNumberOfAddressingModes] = {
    RegisterGetter0_32, RegisterGetter1_32, RegisterGetter2_32, RegisterGetter3_32,
    RegisterGetter4_32, RegisterGetter5_32, RegisterGetter6_32, RegisterGetter7_32,
    RegisterIndirectGetter0_32, RegisterIndirectGetter1_32, RegisterIndirectGetter2_32, RegisterIndirectGetter3_32,
    RegisterIndirectGetter4_32, RegisterIndirectGetter5_32, RegisterIndirectGetter6_32, RegisterIndirectGetter7_32,
    IndexedAbsoluteGetter0_32, IndexedAbsoluteGetter1_32, IndexedAbsoluteGetter2_32, IndexedAbsoluteGetter3_32,
    IndexedAbsoluteGetter4_32, IndexedAbsoluteGetter5_32, IndexedAbsoluteGetter6_32, IndexedAbsoluteGetter7_32,
    AbsoluteGetter_32, ImmediateGetter,
};

static RARGetterFunction OperandGetters_8[RARNumberOfAddressingModes] = {
    RegisterGetter0_8, RegisterGetter1_8, RegisterGetter2_8, RegisterGetter3_8,
    RegisterGetter4_8, RegisterGetter5_8, RegisterGetter6_8, RegisterGetter7_8,
    RegisterIndirectGetter0_8, RegisterIndirectGetter1_8, RegisterIndirectGetter2_8, RegisterIndirectGetter3_8,
    RegisterIndirectGetter4_8, RegisterIndirectGetter5_8, RegisterIndirectGetter6_8, RegisterIndirectGetter7_8,
    IndexedAbsoluteGetter0_8, IndexedAbsoluteGetter1_8, IndexedAbsoluteGetter2_8, IndexedAbsoluteGetter3_8,
    IndexedAbsoluteGetter4_8, IndexedAbsoluteGetter5_8, IndexedAbsoluteGetter6_8, IndexedAbsoluteGetter7_8,
    AbsoluteGetter_8, ImmediateGetter,
};

static RARSetterFunction OperandSetters_32[RARNumberOfAddressingModes] = {
    RegisterSetter0_32, RegisterSetter1_32, RegisterSetter2_32, RegisterSetter3_32,
    RegisterSetter4_32, RegisterSetter5_32, RegisterSetter6_32, RegisterSetter7_32,
    RegisterIndirectSetter0_32, RegisterIndirectSetter1_32, RegisterIndirectSetter2_32, RegisterIndirectSetter3_32,
    RegisterIndirectSetter4_32, RegisterIndirectSetter5_32, RegisterIndirectSetter6_32, RegisterIndirectSetter7_32,
    IndexedAbsoluteSetter0_32, IndexedAbsoluteSetter1_32, IndexedAbsoluteSetter2_32, IndexedAbsoluteSetter3_32,
    IndexedAbsoluteSetter4_32, IndexedAbsoluteSetter5_32, IndexedAbsoluteSetter6_32, IndexedAbsoluteSetter7_32,
    AbsoluteSetter_32, NULL,
};

static RARSetterFunction OperandSetters_8[RARNumberOfAddressingModes] = {
    RegisterSetter0_8, RegisterSetter1_8, RegisterSetter2_8, RegisterSetter3_8,
    RegisterSetter4_8, RegisterSetter5_8, RegisterSetter6_8, RegisterSetter7_8,
    RegisterIndirectSetter0_8, RegisterIndirectSetter1_8, RegisterIndirectSetter2_8, RegisterIndirectSetter3_8,
    RegisterIndirectSetter4_8, RegisterIndirectSetter5_8, RegisterIndirectSetter6_8, RegisterIndirectSetter7_8,
    IndexedAbsoluteSetter0_8, IndexedAbsoluteSetter1_8, IndexedAbsoluteSetter2_8, IndexedAbsoluteSetter3_8,
    IndexedAbsoluteSetter4_8, IndexedAbsoluteSetter5_8, IndexedAbsoluteSetter6_8, IndexedAbsoluteSetter7_8,
    AbsoluteSetter_8, NULL,
};

// Instruction properties

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
