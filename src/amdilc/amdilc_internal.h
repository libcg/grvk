#ifndef AMDILC_INTERNAL_H_
#define AMDILC_INTERNAL_H_

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "amdil/amdil.h"
#include "logger.h"
#include "amdilc.h"

#define GET_BITS(dword, firstBit, lastBit) \
    (((dword) >> (firstBit)) & (0xFFFFFFFF >> (32 - ((lastBit) - (firstBit) + 1))))

#define GET_BIT(dword, bit) \
    (GET_BITS(dword, bit, bit))

typedef uint32_t Token;
typedef struct _Source Source;

typedef struct {
    uint32_t registerNum;
    uint8_t registerType;
    uint8_t component[4];
    bool clamp;
    uint8_t shiftScale;
    Source* absoluteSrc;
    unsigned relativeSrcCount;
    Source* relativeSrcs;
    bool hasImmediate;
    Token immediate;
} Destination;

typedef struct _Source {
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
    unsigned srcCount;
    Source* srcs;
    bool hasImmediate;
    Token immediate;
} Source;

typedef struct {
    uint16_t opcode;
    uint16_t control;
    Token primModifier;
    Token secModifier;
    Token resourceFormat;
    Token addressOffset;
    unsigned dstCount;
    Destination* dsts;
    unsigned srcCount;
    Source* srcs;
    unsigned extraCount;
    Token* extras;
    uint8_t preciseMask;
} Instruction;

typedef struct {
    uint8_t clientType;
    uint8_t majorVersion;
    uint8_t minorVersion;
    uint8_t shaderType;
    bool multipass;
    bool realtime;
    unsigned instrCount;
    Instruction* instrs;
} Kernel;

extern const char* mIlShaderTypeNames[IL_SHADER_LAST];

Kernel* ilcDecodeStream(
    const Token* tokens,
    unsigned count);

void ilcDumpKernel(
    FILE* file,
    const Kernel* kernel);

IlcShader ilcCompileKernel(
    const Kernel* kernel,
    const char* name);

IlcRecompiledShader ilcRecompileKernel(
    const uint32_t* spirvWords,
    unsigned wordCount,
    const unsigned* inputPassthroughLocations,
    unsigned passthroughCount);

#endif // AMDILC_INTERNAL_H_
