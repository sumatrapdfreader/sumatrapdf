/* Copyright 2015 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

/* adapted from https://code.google.com/p/theunarchiver/source/browse/XADMaster/RARVirtualMachine.h */

#ifndef rar_vm_h
#define rar_vm_h

#include <stdint.h>
#include <stdbool.h>

#define RARProgramMemorySize 0x40000
#define RARProgramMemoryMask (RARProgramMemorySize - 1)
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

typedef struct RARVirtualMachine RARVirtualMachine;

struct RARVirtualMachine {
    uint32_t registers[8];
    uint8_t memory[RARProgramMemorySize + sizeof(uint32_t) /* overflow sentinel */];
};

typedef struct RARProgram_s RARProgram;

/* Program building */

enum {
    RARMovInstruction = 0,
    RARCmpInstruction = 1,
    RARAddInstruction = 2,
    RARSubInstruction = 3,
    RARJzInstruction = 4,
    RARJnzInstruction = 5,
    RARIncInstruction = 6,
    RARDecInstruction = 7,
    RARJmpInstruction = 8,
    RARXorInstruction = 9,
    RARAndInstruction = 10,
    RAROrInstruction = 11,
    RARTestInstruction = 12,
    RARJsInstruction = 13,
    RARJnsInstruction = 14,
    RARJbInstruction = 15,
    RARJbeInstruction = 16,
    RARJaInstruction = 17,
    RARJaeInstruction = 18,
    RARPushInstruction = 19,
    RARPopInstruction = 20,
    RARCallInstruction = 21,
    RARRetInstruction = 22,
    RARNotInstruction = 23,
    RARShlInstruction = 24,
    RARShrInstruction = 25,
    RARSarInstruction = 26,
    RARNegInstruction = 27,
    RARPushaInstruction = 28,
    RARPopaInstruction = 29,
    RARPushfInstruction = 30,
    RARPopfInstruction = 31,
    RARMovzxInstruction = 32,
    RARMovsxInstruction = 33,
    RARXchgInstruction = 34,
    RARMulInstruction = 35,
    RARDivInstruction = 36,
    RARAdcInstruction = 37,
    RARSbbInstruction = 38,
    RARPrintInstruction = 39,
    RARNumberOfInstructions = 40,
};

RARProgram *RARCreateProgram();
void RARDeleteProgram(RARProgram *prog);
bool RARProgramAddInstr(RARProgram *prog, uint8_t instruction, bool bytemode);
bool RARSetLastInstrOperands(RARProgram *prog, uint8_t addressingmode1, uint32_t value1, uint8_t addressingmode2, uint32_t value2);
bool RARIsProgramTerminated(RARProgram *prog);

/* Execution */

bool RARExecuteProgram(RARVirtualMachine *vm, RARProgram *prog);

/* Memory and register access (convenience) */

void RARSetVirtualMachineRegisters(RARVirtualMachine *vm, uint32_t registers[8]);
uint32_t RARVirtualMachineRead32(RARVirtualMachine *vm, uint32_t address);
void RARVirtualMachineWrite32(RARVirtualMachine *vm, uint32_t address, uint32_t val);
uint8_t RARVirtualMachineRead8(RARVirtualMachine *vm, uint32_t address);
void RARVirtualMachineWrite8(RARVirtualMachine *vm, uint32_t address, uint8_t val);

/* Instruction properties */

int NumberOfRARInstructionOperands(uint8_t instruction);
bool RARInstructionHasByteMode(uint8_t instruction);
bool RARInstructionIsUnconditionalJump(uint8_t instruction);
bool RARInstructionIsRelativeJump(uint8_t instruction);
bool RARInstructionWritesFirstOperand(uint8_t instruction);
bool RARInstructionWritesSecondOperand(uint8_t instruction);

/* Program debugging */

#ifndef NDEBUG
void RARPrintProgram(RARProgram *prog);
#endif

#endif
