#include "amdilc_internal.h"

typedef struct {
    uint16_t opcode;
    uint8_t dstCount;
    uint8_t srcCount;
    uint8_t extraCount;
    bool hasIndexedResourceSampler;
} OpcodeInfo;

OpcodeInfo mOpcodeInfos[IL_OP_LAST] = {
    [IL_OP_ADD] = { IL_OP_ADD, 1, 2, 0, false },
    [IL_OP_END] = { IL_OP_END, 0, 0, 0, false },
    [IL_OP_MAD] = { IL_OP_MAD, 1, 3, 0, false },
    [IL_OP_MOV] = { IL_OP_MOV, 1, 1, 0, false },
    [IL_OP_MUL] = { IL_OP_MUL, 1, 2, 0, false },
    [IL_OP_RET_DYN] = { IL_OP_RET_DYN, 0, 0, 0, false },
    [IL_DCL_LITERAL] = { IL_DCL_LITERAL, 0, 1, 4, false },
    [IL_DCL_OUTPUT] = { IL_DCL_OUTPUT, 1, 0, 0, false },
    [IL_DCL_INPUT] = { IL_DCL_INPUT, 1, 0, 0, false },
    [IL_OP_LOAD] = { IL_OP_LOAD, 1, 1, 0, true },
    [IL_DCL_RESOURCE] = { IL_DCL_RESOURCE, 0, 0, 1, false },
    [IL_DCL_GLOBAL_FLAGS] = { IL_DCL_GLOBAL_FLAGS, 0, 0, 0, false },
};

static uint32_t decodeIlLang(
    Kernel* kernel,
    const Token* token)
{
    kernel->clientType = GET_BITS(token[0], 0, 7);
    return 1;
}

static uint32_t decodeIlVersion(
    Kernel* kernel,
    const Token* token)
{
    kernel->minorVersion = GET_BITS(token[0], 0, 7);
    kernel->majorVersion = GET_BITS(token[0], 8, 15);
    kernel->shaderType = GET_BITS(token[0], 16, 23);
    kernel->multipass = GET_BIT(token[0], 24);
    kernel->realtime = GET_BIT(token[0], 25);
    return 1;
}

static uint32_t decodeDestination(
    Destination* dst,
    const Token* token)
{
    uint32_t idx = 0;
    bool modifierPresent;
    uint8_t relativeAddress;
    uint8_t dimension;
    bool immediatePresent;
    bool extended;

    dst->registerNum = GET_BITS(token[idx], 0, 15);
    dst->registerType = GET_BITS(token[idx], 16, 21);
    modifierPresent = GET_BIT(token[idx], 22);
    relativeAddress = GET_BITS(token[idx], 23, 24);
    dimension = GET_BIT(token[idx], 25);
    immediatePresent = GET_BIT(token[idx], 26);
    extended = GET_BIT(token[idx], 31);
    idx++;

    if (modifierPresent) {
        dst->component[0] = GET_BITS(token[idx], 0, 1);
        dst->component[1] = GET_BITS(token[idx], 2, 3);
        dst->component[2] = GET_BITS(token[idx], 4, 5);
        dst->component[3] = GET_BITS(token[idx], 6, 7);
        dst->clamp = GET_BIT(token[idx], 8);
        dst->shiftScale = GET_BITS(token[idx], 9, 12);
        idx++;
    } else {
        dst->component[0] = IL_MODCOMP_WRITE;
        dst->component[1] = IL_MODCOMP_WRITE;
        dst->component[2] = IL_MODCOMP_WRITE;
        dst->component[3] = IL_MODCOMP_WRITE;
        dst->clamp = false;
        dst->shiftScale = IL_SHIFT_NONE;
    }

    if (relativeAddress != IL_ADDR_ABSOLUTE) {
        // TODO
        LOGW("unhandled addressing %d\n", relativeAddress);
    }
    if (dimension != 0) {
        // TODO
        LOGW("unhandled dimension %d\n", dimension);
    }
    if (immediatePresent) {
        // TODO
        LOGW("unhandled immediate value\n");
    }
    if (extended) {
        // TODO
        LOGW("unhandled extended register addressing\n");
    }

    return idx;
}

static uint32_t decodeSource(
    Source* src,
    const Token* token)
{
    uint32_t idx = 0;
    bool modifierPresent;
    uint8_t relativeAddress;
    uint8_t dimension;
    bool immediatePresent;
    bool extended;

    src->registerNum = GET_BITS(token[idx], 0, 15);
    src->registerType = GET_BITS(token[idx], 16, 21);
    modifierPresent = GET_BIT(token[idx], 22);
    relativeAddress = GET_BITS(token[idx], 23, 24);
    dimension = GET_BIT(token[idx], 25);
    immediatePresent = GET_BIT(token[idx], 26);
    extended = GET_BIT(token[idx], 31);
    idx++;

    if (modifierPresent) {
        src->swizzle[0] = GET_BITS(token[idx], 0, 2);
        src->swizzle[1] = GET_BITS(token[idx], 4, 6);
        src->swizzle[2] = GET_BITS(token[idx], 8, 10);
        src->swizzle[3] = GET_BITS(token[idx], 12, 14);
        src->negate[0] = GET_BIT(token[idx], 3);
        src->negate[1] = GET_BIT(token[idx], 7);
        src->negate[2] = GET_BIT(token[idx], 11);
        src->negate[3] = GET_BIT(token[idx], 15);
        src->invert = GET_BIT(token[idx], 16);
        src->bias = GET_BIT(token[idx], 17);
        src->x2 = GET_BIT(token[idx], 18);
        src->sign = GET_BIT(token[idx], 19);
        src->abs = GET_BIT(token[idx], 20);
        src->divComp = GET_BITS(token[idx], 21, 23);
        src->clamp = GET_BIT(token[idx], 24);
        idx++;
    } else {
        src->swizzle[0] = IL_COMPSEL_X_R;
        src->swizzle[1] = IL_COMPSEL_Y_G;
        src->swizzle[2] = IL_COMPSEL_Z_B;
        src->swizzle[3] = IL_COMPSEL_W_A;
        src->negate[0] = false;
        src->negate[1] = false;
        src->negate[2] = false;
        src->negate[3] = false;
        src->invert = false;
        src->bias = false;
        src->x2 = false;
        src->sign = false;
        src->abs = false;
        src->divComp = IL_DIVCOMP_NONE;
        src->clamp = false;
    }

    if (relativeAddress != IL_ADDR_ABSOLUTE) {
        // TODO
        LOGW("unhandled addressing %d\n", relativeAddress);
    }
    if (dimension != 0) {
        // TODO
        LOGW("unhandled dimension %d\n", dimension);
    }
    if (immediatePresent) {
        // TODO
        LOGW("unhandled immediate value\n");
    }
    if (extended) {
        // TODO
        LOGW("unhandled extended register addressing\n");
    }

    return idx;
}

static uint32_t decodeInstruction(
    Instruction* instr,
    const Token* token)
{
    uint32_t idx = 0;

    memset(instr, 0, sizeof(*instr));

    instr->opcode = GET_BITS(token[idx], 0, 15);
    instr->control = GET_BITS(token[idx], 16, 31);
    idx++;

    if (instr->opcode >= IL_OP_LAST) {
        LOGE("invalid opcode\n");
        return idx;
    }

    OpcodeInfo* info = &mOpcodeInfos[instr->opcode];

    if (info->opcode == IL_OP_UNKNOWN) {
        LOGW("unhandled opcode %d\n", instr->opcode);
        return idx;
    }
    assert(instr->opcode == info->opcode);

    instr->dstCount = info->dstCount;
    instr->dsts = malloc(sizeof(Destination) * instr->dstCount);
    for (int i = 0; i < info->dstCount; i++) {
        idx += decodeDestination(&instr->dsts[i], &token[idx]);
    }

    instr->srcCount = info->srcCount;
    instr->srcs = malloc(sizeof(Source) * instr->srcCount);
    for (int i = 0; i < info->srcCount; i++) {
        idx += decodeSource(&instr->srcs[i], &token[idx]);
    }

    if (info->hasIndexedResourceSampler) {
        if (GET_BIT(instr->control, 12)) {
            LOGW("unhandled indexed args\n");
        }
        if (GET_BIT(instr->control, 13)) {
            LOGW("unhandled immediate address offset\n");
        }
    }

    instr->extraCount = info->extraCount;
    instr->extras = malloc(sizeof(Token) * instr->extraCount);
    memcpy(instr->extras, &token[idx], sizeof(Token) * instr->extraCount);
    idx += info->extraCount;

    return idx;
}

Kernel* ilcDecodeStream(
    const Token* tokens,
    uint32_t count)
{
    Kernel* kernel = malloc(sizeof(Kernel));
    uint32_t idx = 0;

    idx += decodeIlLang(kernel, &tokens[idx]);
    idx += decodeIlVersion(kernel, &tokens[idx]);

    kernel->instrCount = 0;
    kernel->instrs = NULL;
    while (idx < count) {
        kernel->instrCount++;
        kernel->instrs = realloc(kernel->instrs, sizeof(Instruction) * kernel->instrCount);
        idx += decodeInstruction(&kernel->instrs[kernel->instrCount - 1], &tokens[idx]);
    }

    return kernel;
}
