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

#define MAX(a, b) \
    ((a) > (b) ? (a) : (b))

#define COUNT_OF(array) \
    (sizeof(array) / sizeof((array)[0]))

#define STACK_ARRAY(type, name, stackCount, count) \
    type _stack_##name[stackCount]; \
    type* name = (count) <= (stackCount) ? _stack_##name : malloc((count) * sizeof(type))

#define STACK_ARRAY_FINISH(name) \
   if (name != _stack_##name) free(name)

#define MAX_SRC_COUNT (8)
#define MAX_DST_COUNT (1)

typedef uint32_t Token;
typedef struct _Source Source;

typedef struct {
    uint32_t registerNum;
    uint8_t registerType;
    uint8_t component[4];
    bool clamp;
    uint8_t shiftScale;
    bool hasAbsoluteSrc;
    unsigned absoluteSrc;
    unsigned relativeSrcCount;
    unsigned relativeSrcs[2];
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
    unsigned srcs[2];
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
    unsigned dsts[MAX_DST_COUNT];
    unsigned srcCount;
    unsigned srcs[MAX_SRC_COUNT];
    unsigned extraCount;
    unsigned extrasStartIndex;
    uint8_t preciseMask;
} Instruction;

typedef struct {
    uint8_t clientType;
    uint8_t majorVersion;
    uint8_t minorVersion;
    uint8_t shaderType;
    bool multipass;
    bool realtime;
    unsigned dstCount;
    unsigned dstSize;
    Destination* dstBuffer;
    unsigned srcCount;
    unsigned srcSize;
    Source* srcBuffer;
    unsigned extraCount;
    unsigned extraSize;
    Token* extrasBuffer;
    unsigned instrSize;
    unsigned instrCount;
    Instruction* instrs;
} Kernel;

extern const char* mIlShaderTypeNames[IL_SHADER_LAST];

void ilcDecodeStream(
    Kernel* kernel,
    const Token* tokens,
    unsigned count);

void ilcDumpKernel(
    FILE* file,
    const Kernel* kernel);

IlcShader ilcCompileKernel(
    const Kernel* kernel,
    const char* name);

#endif // AMDILC_INTERNAL_H_
