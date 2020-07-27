#ifndef AMDILC_INTERNAL_H_
#define AMDILC_INTERNAL_H_

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "amdil/amdil.h"
#include "amdilc.h"

typedef uint32_t Token;

typedef struct {
    uint32_t registerNum;
    uint8_t registerType;
    uint8_t component[4];
    bool clamp;
    uint8_t shiftScale;
} Destination;

typedef struct {
    uint32_t registerNum;
    uint8_t registerType;
    uint8_t swizzle[4];
    bool negate[4];
    bool invert;
    bool bias;
    bool x2;
    bool sign;
    bool abs;
    uint8_t divComp;
    bool clamp;
} Source;

typedef struct {
    uint16_t opcode;
    uint16_t control;
    uint32_t dstCount;
    Destination* dsts;
    uint32_t srcCount;
    Source* srcs;
    uint32_t extraCount;
    Token* extras;
} Instruction;

typedef struct {
    uint8_t clientType;
    uint8_t majorVersion;
    uint8_t minorVersion;
    uint8_t shaderType;
    bool multipass;
    bool realtime;
    uint32_t instrCount;
    Instruction* instrs;
} Kernel;

uint32_t getBits(
    const uint32_t dword,
    const uint32_t firstBit,
    const uint32_t lastBit);

uint32_t getBit(
    const uint32_t dword,
    const uint32_t bit);

Kernel* ilcDecodeStream(
    const Token* tokens,
    const uint32_t count);

void ilcDumpKernel(
    const Kernel* kernel);

#endif // AMDILC_INTERNAL_H_
