/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: GPLv3 */

// adapted from https://code.google.com/p/theunarchiver/source/browse/XADMaster/RARVirtualMachine.h

#include "vm.h"

#define RARProgramMemorySize 0x40000
#define RARProgramMemoryMask (RARProgramMemorySize-1)
#define RARProgramWorkSize 0x3c000
#define RARProgramGlobalSize 0x2000
#define RARProgramSystemGlobalAddress RARProgramWorkSize
#define RARProgramSystemGlobalSize 64
#define RARProgramUserGlobalAddress (RARProgramSystemGlobalAddress + RARProgramSystemGlobalSize)
#define RARProgramUserGlobalSize (RARProgramGlobalSize - RARProgramSystemGlobalSize)
#define RARRuntimeMaxInstructions 250000000

#define RARRegisterAddressingMode(n) (0 + (n))
#define RARRegisterIndirectAddressingMode(n) (8 + (n))
#define RARIndexedAbsoluteAddressingMode(n) (16 + (n))
#define RARAbsoluteAddressingMode 24
#define RARImmediateAddressingMode 25
#define RARNumberOfAddressingModes 26

typedef uint32_t (* RARGetterFunction)(RARVirtualMachine *self, uint32_t value);
typedef void (* RARSetterFunction)(RARVirtualMachine *self, uint32_t value, uint32_t data);

struct RARVirtualMachine_s {
    uint32_t registers[8];
    uint32_t flags;
    uint8_t memory[RARProgramMemorySize + sizeof(uint32_t) /* overflow sentinel */];
};

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

#if UINTPTR_MAX==UINT64_MAX
    uint8_t padding[12]; // 64-bit machine, pad to 64 bytes
#endif
};

static RARGetterFunction OperandGetters_32[RARNumberOfAddressingModes];
static RARGetterFunction OperandGetters_8[RARNumberOfAddressingModes];
static RARSetterFunction OperandSetters_32[RARNumberOfAddressingModes];
static RARSetterFunction OperandSetters_8[RARNumberOfAddressingModes];

// Setup

void InitializeRARVirtualMachine(RARVirtualMachine *self)
{
    // TODO: memset(self, 0, sizeof(*self)); ?
    memset(self->registers, 0, sizeof(self->registers));
}

// Program building

void SetRAROpcodeInstruction(RAROpcode *opcode, uint8_t instruction, bool bytemode)
{
    opcode->instruction = instruction;
    opcode->bytemode = bytemode ? 1 : 0;
}

void SetRAROpcodeOperand1(RAROpcode *opcode, uint8_t addressingmode, uint32_t value)
{
    opcode->addressingmode1 = addressingmode;
    opcode->value1 = value;
}

void SetRAROpcodeOperand2(RAROpcode *opcode, uint8_t addressingmode, uint32_t value)
{
    opcode->addressingmode2 = addressingmode;
    opcode->value2 = value;
}

bool IsProgramTerminated(RAROpcode *opcodes, uint32_t numopcodes)
{
    return RARInstructionIsUnconditionalJump(opcodes[numopcodes - 1].instruction);
}

bool PrepareRAROpcodes(RAROpcode *opcodes, uint32_t numopcodes)
{
    uint32_t i;
    for (i = 0; i < numopcodes; i++) {
        RARGetterFunction *getterfunctions = OperandGetters_32;
        RARSetterFunction *setterfunctions = OperandSetters_32;
        int numoperands = NumberOfRARInstructionOperands(opcodes[i].instruction);;

        if (opcodes[i].instruction >= RARNumberOfInstructions)
            return false;

        if (opcodes[i].instruction == RARMovsxInstruction || opcodes[i].instruction == RARMovzxInstruction) {
            getterfunctions = OperandGetters_8;
        }
        else if (opcodes[i].bytemode) {
            if (!RARInstructionHasByteMode(opcodes[i].instruction))
                return false;
            getterfunctions = OperandGetters_8;
            setterfunctions = OperandSetters_8;
        }

        if (numoperands >= 1) {
            if (opcodes[i].addressingmode1 >= RARNumberOfAddressingModes)
                return false;
            opcodes[i].operand1getter = getterfunctions[opcodes[i].addressingmode1];
            opcodes[i].operand1setter = setterfunctions[opcodes[i].addressingmode1];

            if (opcodes[i].addressingmode1 == RARImmediateAddressingMode) {
                if (RARInstructionWritesFirstOperand(opcodes[i].instruction))
                    return false;
            }
            else if (opcodes[i].addressingmode1 == RARAbsoluteAddressingMode)
                opcodes[i].value1 &= RARProgramMemoryMask;
        }
        if (numoperands==2) {
            if (opcodes[i].addressingmode2 >= RARNumberOfAddressingModes)
                return false;
            opcodes[i].operand2getter = getterfunctions[opcodes[i].addressingmode2];
            opcodes[i].operand2setter = setterfunctions[opcodes[i].addressingmode2];

            if (opcodes[i].addressingmode2 == RARImmediateAddressingMode) {
                if (RARInstructionWritesSecondOperand(opcodes[i].instruction))
                    return false;
            }
            else if (opcodes[i].addressingmode2 == RARAbsoluteAddressingMode)
                opcodes[i].value2 &= RARProgramMemoryMask;
        }
    }

    return true;
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

#define GetOperand1() (opcode->operand1getter(self, opcode->value1))
#define GetOperand2() (opcode->operand2getter(self, opcode->value2))
#define SetOperand1(data) opcode->operand1setter(self, opcode->value1, data)
#define SetOperand2(data) opcode->operand2setter(self, opcode->value2, data)

#define SetFlagsWithCarry(res, carry) EXTMACRO_BEGIN uint32_t result = (res); flags = (result == 0 ? ZeroFlag : (result & SignFlag)) | ((carry) ? 1 : 0); EXTMACRO_END
#define SetByteFlagsWithCarry(res, carry) EXTMACRO_BEGIN uint8_t result = (res); flags = (result == 0 ? ZeroFlag : (SignExtend(result) & SignFlag)) | ((carry) ? 1 : 0); EXTMACRO_END
#define SetFlags(res) SetFlagsWithCarry(res, 0)

#define SetOperand1AndFlagsWithCarry(res, carry) EXTMACRO_BEGIN uint32_t r = (res); SetFlagsWithCarry(r, carry); SetOperand1(r); EXTMACRO_END
#define SetOperand1AndByteFlagsWithCarry(res, carry) EXTMACRO_BEGIN uint8_t r = (res); SetByteFlagsWithCarry(r, carry); SetOperand1(r); EXTMACRO_END
#define SetOperand1AndFlags(res) EXTMACRO_BEGIN uint32_t r = (res); SetFlags(r); SetOperand1(r); EXTMACRO_END

#define NextInstruction() { opcode++; continue; }
#define Jump(offs) { uint32_t o = (offs); if (o >= numopcodes) return false; opcode = &opcodes[o]; continue; }

bool ExecuteRARCode(RARVirtualMachine *self, RAROpcode *opcodes, uint32_t numopcodes)
{
    RAROpcode *opcode = opcodes;
    uint32_t flags = self->flags;
    uint32_t op1, op2, carry, i;
    uint32_t counter = 0;

    if (!IsProgramTerminated(opcodes, numopcodes))
        return false;

    self->flags = 0; // ?

    while ((uint32_t)(opcode - opcodes) < numopcodes && counter++ < RARRuntimeMaxInstructions) {
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
            self->registers[7] -= 4;
            RARVirtualMachineWrite32(self, self->registers[7], GetOperand1());
            NextInstruction();

        case RARPopInstruction:
            SetOperand1(RARVirtualMachineRead32(self, self->registers[7]));
            self->registers[7] += 4;
            NextInstruction();

        case RARCallInstruction:
            self->registers[7] -= 4;
            RARVirtualMachineWrite32(self, self->registers[7], opcode - opcodes + 1);
            Jump(GetOperand1());

        case RARRetInstruction:
            if (self->registers[7] >= RARProgramMemorySize) {
                self->flags = flags;
                return true;
            }
            i = RARVirtualMachineRead32(self, self->registers[7]);
            self->registers[7] += 4;
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
                RARVirtualMachineWrite32(self, self->registers[7] - 4 - i * 4, self->registers[i]);
            }
            self->registers[7] -= 32;
            NextInstruction();

        case RARPopaInstruction:
            for (i = 0; i < 8; i++) {
                self->registers[i] = RARVirtualMachineRead32(self, self->registers[7] + 28 - i * 4);
            }
            // TODO: self->registers[7] += 32; ?
            NextInstruction();

        case RARPushfInstruction:
            self->registers[7] -= 4;
            RARVirtualMachineWrite32(self, self->registers[7], flags);
            NextInstruction();

        case RARPopfInstruction:
            flags = RARVirtualMachineRead32(self, self->registers[7]);
            self->registers[7] += 4;
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

void SetRARVirtualMachineRegisters(RARVirtualMachine *self, uint32_t registers[8])
{
    memcpy(self->registers, registers, sizeof(self->registers));
}

uint32_t RARVirtualMachineRead32(RARVirtualMachine *self, uint32_t address)
{
    return _RARRead32(&self->memory[address & RARProgramMemoryMask]);
}

void RARVirtualMachineWrite32(RARVirtualMachine *self, uint32_t address, uint32_t val)
{
    _RARWrite32(&self->memory[address & RARProgramMemoryMask], val);
}

uint8_t RARVirtualMachineRead8(RARVirtualMachine *self, uint32_t address)
{
    return self->memory[address & RARProgramMemoryMask];
}

void RARVirtualMachineWrite8(RARVirtualMachine *self, uint32_t address, uint8_t val)
{
    self->memory[address & RARProgramMemoryMask] = val;
}

static uint32_t RegisterGetter0_32(RARVirtualMachine *self, uint32_t value) { return self->registers[0]; }
static uint32_t RegisterGetter1_32(RARVirtualMachine *self, uint32_t value) { return self->registers[1]; }
static uint32_t RegisterGetter2_32(RARVirtualMachine *self, uint32_t value) { return self->registers[2]; }
static uint32_t RegisterGetter3_32(RARVirtualMachine *self, uint32_t value) { return self->registers[3]; }
static uint32_t RegisterGetter4_32(RARVirtualMachine *self, uint32_t value) { return self->registers[4]; }
static uint32_t RegisterGetter5_32(RARVirtualMachine *self, uint32_t value) { return self->registers[5]; }
static uint32_t RegisterGetter6_32(RARVirtualMachine *self, uint32_t value) { return self->registers[6]; }
static uint32_t RegisterGetter7_32(RARVirtualMachine *self, uint32_t value) { return self->registers[7]; }
static uint32_t RegisterGetter0_8(RARVirtualMachine *self, uint32_t value) { return self->registers[0] & 0xFF; }
static uint32_t RegisterGetter1_8(RARVirtualMachine *self, uint32_t value) { return self->registers[1] & 0xFF; }
static uint32_t RegisterGetter2_8(RARVirtualMachine *self, uint32_t value) { return self->registers[2] & 0xFF; }
static uint32_t RegisterGetter3_8(RARVirtualMachine *self, uint32_t value) { return self->registers[3] & 0xFF; }
static uint32_t RegisterGetter4_8(RARVirtualMachine *self, uint32_t value) { return self->registers[4] & 0xFF; }
static uint32_t RegisterGetter5_8(RARVirtualMachine *self, uint32_t value) { return self->registers[5] & 0xFF; }
static uint32_t RegisterGetter6_8(RARVirtualMachine *self, uint32_t value) { return self->registers[6] & 0xFF; }
static uint32_t RegisterGetter7_8(RARVirtualMachine *self, uint32_t value) { return self->registers[7] & 0xFF; }

static uint32_t RegisterIndirectGetter0_32(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead32(self, self->registers[0]); }
static uint32_t RegisterIndirectGetter1_32(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead32(self, self->registers[1]); }
static uint32_t RegisterIndirectGetter2_32(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead32(self, self->registers[2]); }
static uint32_t RegisterIndirectGetter3_32(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead32(self, self->registers[3]); }
static uint32_t RegisterIndirectGetter4_32(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead32(self, self->registers[4]); }
static uint32_t RegisterIndirectGetter5_32(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead32(self, self->registers[5]); }
static uint32_t RegisterIndirectGetter6_32(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead32(self, self->registers[6]); }
static uint32_t RegisterIndirectGetter7_32(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead32(self, self->registers[7]); }
static uint32_t RegisterIndirectGetter0_8(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead8(self, self->registers[0]); }
static uint32_t RegisterIndirectGetter1_8(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead8(self, self->registers[1]); }
static uint32_t RegisterIndirectGetter2_8(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead8(self, self->registers[2]); }
static uint32_t RegisterIndirectGetter3_8(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead8(self, self->registers[3]); }
static uint32_t RegisterIndirectGetter4_8(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead8(self, self->registers[4]); }
static uint32_t RegisterIndirectGetter5_8(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead8(self, self->registers[5]); }
static uint32_t RegisterIndirectGetter6_8(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead8(self, self->registers[6]); }
static uint32_t RegisterIndirectGetter7_8(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead8(self, self->registers[7]); }

static uint32_t IndexedAbsoluteGetter0_32(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead32(self, value + self->registers[0]); }
static uint32_t IndexedAbsoluteGetter1_32(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead32(self, value + self->registers[1]); }
static uint32_t IndexedAbsoluteGetter2_32(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead32(self, value + self->registers[2]); }
static uint32_t IndexedAbsoluteGetter3_32(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead32(self, value + self->registers[3]); }
static uint32_t IndexedAbsoluteGetter4_32(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead32(self, value + self->registers[4]); }
static uint32_t IndexedAbsoluteGetter5_32(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead32(self, value + self->registers[5]); }
static uint32_t IndexedAbsoluteGetter6_32(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead32(self, value + self->registers[6]); }
static uint32_t IndexedAbsoluteGetter7_32(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead32(self, value + self->registers[7]); }
static uint32_t IndexedAbsoluteGetter0_8(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead8(self, value + self->registers[0]); }
static uint32_t IndexedAbsoluteGetter1_8(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead8(self, value + self->registers[1]); }
static uint32_t IndexedAbsoluteGetter2_8(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead8(self, value + self->registers[2]); }
static uint32_t IndexedAbsoluteGetter3_8(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead8(self, value + self->registers[3]); }
static uint32_t IndexedAbsoluteGetter4_8(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead8(self, value + self->registers[4]); }
static uint32_t IndexedAbsoluteGetter5_8(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead8(self, value + self->registers[5]); }
static uint32_t IndexedAbsoluteGetter6_8(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead8(self, value + self->registers[6]); }
static uint32_t IndexedAbsoluteGetter7_8(RARVirtualMachine *self, uint32_t value) { return RARVirtualMachineRead8(self, value + self->registers[7]); }

// Note: Absolute addressing is pre-masked in PrepareRAROpcodes.
static uint32_t AbsoluteGetter_32(RARVirtualMachine *self, uint32_t value) { return _RARRead32(&self->memory[value]); }
static uint32_t AbsoluteGetter_8(RARVirtualMachine *self, uint32_t value) { return self->memory[value]; }
static uint32_t ImmediateGetter(RARVirtualMachine *self, uint32_t value) { return value; }

static void RegisterSetter0_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { self->registers[0] = data; }
static void RegisterSetter1_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { self->registers[1] = data; }
static void RegisterSetter2_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { self->registers[2] = data; }
static void RegisterSetter3_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { self->registers[3] = data; }
static void RegisterSetter4_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { self->registers[4] = data; }
static void RegisterSetter5_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { self->registers[5] = data; }
static void RegisterSetter6_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { self->registers[6] = data; }
static void RegisterSetter7_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { self->registers[7] = data; }
static void RegisterSetter0_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { self->registers[0] = data & 0xFF; }
static void RegisterSetter1_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { self->registers[1] = data & 0xFF; }
static void RegisterSetter2_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { self->registers[2] = data & 0xFF; }
static void RegisterSetter3_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { self->registers[3] = data & 0xFF; }
static void RegisterSetter4_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { self->registers[4] = data & 0xFF; }
static void RegisterSetter5_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { self->registers[5] = data & 0xFF; }
static void RegisterSetter6_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { self->registers[6] = data & 0xFF; }
static void RegisterSetter7_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { self->registers[7] = data & 0xFF; }

static void RegisterIndirectSetter0_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(self, self->registers[0], data); }
static void RegisterIndirectSetter1_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(self, self->registers[1], data); }
static void RegisterIndirectSetter2_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(self, self->registers[2], data); }
static void RegisterIndirectSetter3_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(self, self->registers[3], data); }
static void RegisterIndirectSetter4_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(self, self->registers[4], data); }
static void RegisterIndirectSetter5_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(self, self->registers[5], data); }
static void RegisterIndirectSetter6_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(self, self->registers[6], data); }
static void RegisterIndirectSetter7_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(self, self->registers[7], data); }
static void RegisterIndirectSetter0_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(self, self->registers[0], (uint8_t)data); }
static void RegisterIndirectSetter1_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(self, self->registers[1], (uint8_t)data); }
static void RegisterIndirectSetter2_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(self, self->registers[2], (uint8_t)data); }
static void RegisterIndirectSetter3_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(self, self->registers[3], (uint8_t)data); }
static void RegisterIndirectSetter4_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(self, self->registers[4], (uint8_t)data); }
static void RegisterIndirectSetter5_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(self, self->registers[5], (uint8_t)data); }
static void RegisterIndirectSetter6_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(self, self->registers[6], (uint8_t)data); }
static void RegisterIndirectSetter7_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(self, self->registers[7], (uint8_t)data); }

static void IndexedAbsoluteSetter0_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(self, value + self->registers[0], data); }
static void IndexedAbsoluteSetter1_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(self, value + self->registers[1], data); }
static void IndexedAbsoluteSetter2_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(self, value + self->registers[2], data); }
static void IndexedAbsoluteSetter3_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(self, value + self->registers[3], data); }
static void IndexedAbsoluteSetter4_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(self, value + self->registers[4], data); }
static void IndexedAbsoluteSetter5_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(self, value + self->registers[5], data); }
static void IndexedAbsoluteSetter6_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(self, value + self->registers[6], data); }
static void IndexedAbsoluteSetter7_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite32(self, value + self->registers[7], data); }
static void IndexedAbsoluteSetter0_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(self, value + self->registers[0], (uint8_t)data); }
static void IndexedAbsoluteSetter1_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(self, value + self->registers[1], (uint8_t)data); }
static void IndexedAbsoluteSetter2_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(self, value + self->registers[2], (uint8_t)data); }
static void IndexedAbsoluteSetter3_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(self, value + self->registers[3], (uint8_t)data); }
static void IndexedAbsoluteSetter4_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(self, value + self->registers[4], (uint8_t)data); }
static void IndexedAbsoluteSetter5_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(self, value + self->registers[5], (uint8_t)data); }
static void IndexedAbsoluteSetter6_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(self, value + self->registers[6], (uint8_t)data); }
static void IndexedAbsoluteSetter7_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { RARVirtualMachineWrite8(self, value + self->registers[7], (uint8_t)data); }

// Note: Absolute addressing is pre-masked in PrepareRAROpcodes.
static void AbsoluteSetter_32(RARVirtualMachine *self, uint32_t value, uint32_t data) { _RARWrite32(&self->memory[value], data); }
static void AbsoluteSetter_8(RARVirtualMachine *self, uint32_t value, uint32_t data) { self->memory[value] = (uint8_t)data; }

static RARGetterFunction OperandGetters_32[RARNumberOfAddressingModes] = {
    RegisterGetter0_32,
    RegisterGetter1_32,
    RegisterGetter2_32,
    RegisterGetter3_32,
    RegisterGetter4_32,
    RegisterGetter5_32,
    RegisterGetter6_32,
    RegisterGetter7_32,
    RegisterIndirectGetter0_32,
    RegisterIndirectGetter1_32,
    RegisterIndirectGetter2_32,
    RegisterIndirectGetter3_32,
    RegisterIndirectGetter4_32,
    RegisterIndirectGetter5_32,
    RegisterIndirectGetter6_32,
    RegisterIndirectGetter7_32,
    IndexedAbsoluteGetter0_32,
    IndexedAbsoluteGetter1_32,
    IndexedAbsoluteGetter2_32,
    IndexedAbsoluteGetter3_32,
    IndexedAbsoluteGetter4_32,
    IndexedAbsoluteGetter5_32,
    IndexedAbsoluteGetter6_32,
    IndexedAbsoluteGetter7_32,
    AbsoluteGetter_32,
    ImmediateGetter,
};

static RARGetterFunction OperandGetters_8[RARNumberOfAddressingModes] = {
    RegisterGetter0_8,
    RegisterGetter1_8,
    RegisterGetter2_8,
    RegisterGetter3_8,
    RegisterGetter4_8,
    RegisterGetter5_8,
    RegisterGetter6_8,
    RegisterGetter7_8,
    RegisterIndirectGetter0_8,
    RegisterIndirectGetter1_8,
    RegisterIndirectGetter2_8,
    RegisterIndirectGetter3_8,
    RegisterIndirectGetter4_8,
    RegisterIndirectGetter5_8,
    RegisterIndirectGetter6_8,
    RegisterIndirectGetter7_8,
    IndexedAbsoluteGetter0_8,
    IndexedAbsoluteGetter1_8,
    IndexedAbsoluteGetter2_8,
    IndexedAbsoluteGetter3_8,
    IndexedAbsoluteGetter4_8,
    IndexedAbsoluteGetter5_8,
    IndexedAbsoluteGetter6_8,
    IndexedAbsoluteGetter7_8,
    AbsoluteGetter_8,
    ImmediateGetter,
};


static RARSetterFunction OperandSetters_32[RARNumberOfAddressingModes] = {
    RegisterSetter0_32,
    RegisterSetter1_32,
    RegisterSetter2_32,
    RegisterSetter3_32,
    RegisterSetter4_32,
    RegisterSetter5_32,
    RegisterSetter6_32,
    RegisterSetter7_32,
    RegisterIndirectSetter0_32,
    RegisterIndirectSetter1_32,
    RegisterIndirectSetter2_32,
    RegisterIndirectSetter3_32,
    RegisterIndirectSetter4_32,
    RegisterIndirectSetter5_32,
    RegisterIndirectSetter6_32,
    RegisterIndirectSetter7_32,
    IndexedAbsoluteSetter0_32,
    IndexedAbsoluteSetter1_32,
    IndexedAbsoluteSetter2_32,
    IndexedAbsoluteSetter3_32,
    IndexedAbsoluteSetter4_32,
    IndexedAbsoluteSetter5_32,
    IndexedAbsoluteSetter6_32,
    IndexedAbsoluteSetter7_32,
    AbsoluteSetter_32,
    NULL,
};

static RARSetterFunction OperandSetters_8[RARNumberOfAddressingModes] = {
    RegisterSetter0_8,
    RegisterSetter1_8,
    RegisterSetter2_8,
    RegisterSetter3_8,
    RegisterSetter4_8,
    RegisterSetter5_8,
    RegisterSetter6_8,
    RegisterSetter7_8,
    RegisterIndirectSetter0_8,
    RegisterIndirectSetter1_8,
    RegisterIndirectSetter2_8,
    RegisterIndirectSetter3_8,
    RegisterIndirectSetter4_8,
    RegisterIndirectSetter5_8,
    RegisterIndirectSetter6_8,
    RegisterIndirectSetter7_8,
    IndexedAbsoluteSetter0_8,
    IndexedAbsoluteSetter1_8,
    IndexedAbsoluteSetter2_8,
    IndexedAbsoluteSetter3_8,
    IndexedAbsoluteSetter4_8,
    IndexedAbsoluteSetter5_8,
    IndexedAbsoluteSetter6_8,
    IndexedAbsoluteSetter7_8,
    AbsoluteSetter_8,
    NULL,
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
    return (InstructionFlags[instruction]&RARHasByteModeFlag)!=0;
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
