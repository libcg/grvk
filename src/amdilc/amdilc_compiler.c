#include "amdilc_spirv.h"
#include "amdilc_internal.h"

#define MAX_SRC_COUNT       (8)
#define ZERO_LITERAL        (0x00000000)
#define ONE_LITERAL         (0x3F800000)
#define FALSE_LITERAL       (0x00000000)
#define TRUE_LITERAL        (0xFFFFFFFF)
#define SHIFT_MASK_LITERAL  (0x1F)
#define COMP_INDEX_X        (0)
#define COMP_INDEX_Y        (1)
#define COMP_INDEX_Z        (2)
#define COMP_INDEX_W        (3)
#define COMP_MASK_X         (1 << COMP_INDEX_X)
#define COMP_MASK_Y         (1 << COMP_INDEX_Y)
#define COMP_MASK_Z         (1 << COMP_INDEX_Z)
#define COMP_MASK_W         (1 << COMP_INDEX_W)
#define COMP_MASK_XY        (COMP_MASK_X | COMP_MASK_Y)
#define COMP_MASK_XYZ       (COMP_MASK_XY | COMP_MASK_Z)
#define COMP_MASK_XYZW      (COMP_MASK_XYZ | COMP_MASK_W)

typedef struct {
    IlcSpvId id;
    IlcSpvId typeId;
    IlcSpvId baseTypeId; // Base type for arrays
    uint32_t ilType; // ILRegType
    uint32_t ilNum;
    uint8_t ilImportUsage; // Input/output only
    uint8_t ilInterpMode; // Input only
} IlcRegister;

typedef struct {
    IlcSpvId id;
    IlcSpvId typeId;
    uint32_t ilId;
    uint8_t ilType;
    uint32_t strideId;
} IlcResource;

typedef struct {
    IlcSpvId id;
    uint32_t ilId;
} IlcSampler;

typedef enum {
    BLOCK_IF_ELSE,
    BLOCK_LOOP,
} IlcControlFlowBlockType;

typedef struct {
    IlcSpvId labelElseId;
    IlcSpvId labelEndId;
    bool hasElseBlock;
} IlcIfElseBlock;

typedef struct {
    IlcSpvId labelHeaderId;
    IlcSpvId labelContinueId;
    IlcSpvId labelBreakId;
} IlcLoopBlock;

typedef struct {
    IlcControlFlowBlockType type;
    union {
        IlcIfElseBlock ifElse;
        IlcLoopBlock loop;
    };
} IlcControlFlowBlock;

typedef struct {
    IlcSpvModule* module;
    const Kernel* kernel;
    IlcSpvId entryPointId;
    IlcSpvId uintId;
    IlcSpvId uint4Id;
    IlcSpvId intId;
    IlcSpvId int4Id;
    IlcSpvId floatId;
    IlcSpvId float4Id;
    IlcSpvId boolId;
    IlcSpvId bool4Id;
    unsigned regCount;
    IlcRegister* regs;
    unsigned resourceCount;
    IlcResource* resources;
    unsigned samplerCount;
    IlcSampler* samplers;
    unsigned controlFlowBlockCount;
    IlcControlFlowBlock* controlFlowBlocks;
    bool isInFunction;
    bool isAfterReturn;
} IlcCompiler;

static unsigned getResourceDimensionCount(
    uint8_t ilType)
{
    switch (ilType) {
    case IL_USAGE_PIXTEX_1D:
    case IL_USAGE_PIXTEX_BUFFER:
        return 1;
    case IL_USAGE_PIXTEX_2D:
    case IL_USAGE_PIXTEX_1DARRAY:
        return 2;
    case IL_USAGE_PIXTEX_3D:
    case IL_USAGE_PIXTEX_2DARRAY:
        return 3;
    default:
        LOGE("unhandled resource type %d\n", ilType);
        assert(false);
    }

    return 0;
}

static IlcSpvId emitVectorVariable(
    IlcCompiler* compiler,
    IlcSpvId* typeId,
    unsigned componentCount,
    IlcSpvId componentTypeId,
    IlcSpvWord storageClass)
{
    *typeId = ilcSpvPutVectorType(compiler->module, componentTypeId, componentCount);
    IlcSpvId pointerId = ilcSpvPutPointerType(compiler->module, storageClass, *typeId);

    return ilcSpvPutVariable(compiler->module, pointerId, storageClass);
}

static IlcSpvId emitZeroOneVector(
    IlcCompiler* compiler)
{
    IlcSpvId float2Id = ilcSpvPutVectorType(compiler->module, compiler->floatId, 2);

    const IlcSpvId consistuentIds[] = {
        ilcSpvPutConstant(compiler->module, compiler->floatId, ZERO_LITERAL),
        ilcSpvPutConstant(compiler->module, compiler->floatId, ONE_LITERAL),
    };
    return ilcSpvPutConstantComposite(compiler->module, float2Id, 2, consistuentIds);
}

static IlcSpvId emitShiftMask(
    IlcCompiler* compiler,
    IlcSpvId srcId)
{
    // Only keep the lower 5 bits of the shift value
    IlcSpvId maskId = ilcSpvPutConstant(compiler->module, compiler->intId, SHIFT_MASK_LITERAL);
    const IlcSpvId maskConsistuentIds[] = { maskId, maskId, maskId, maskId };
    IlcSpvId maskCompositeId = ilcSpvPutConstantComposite(compiler->module, compiler->int4Id,
                                                          4, maskConsistuentIds);
    const IlcSpvId andIds[] = { srcId, maskCompositeId };
    return ilcSpvPutAlu(compiler->module, SpvOpBitwiseAnd, compiler->int4Id, 2, andIds);
}

static IlcSpvId emitVectorTrim(
    IlcCompiler* compiler,
    IlcSpvId vecId,
    IlcSpvId typeId,
    unsigned offset,
    unsigned count)
{
    assert(1 <= (offset + count) && (offset + count) <= 4);

    IlcSpvId baseTypeId;
    if (typeId == compiler->float4Id) {
        baseTypeId = compiler->floatId;
    } else if (typeId == compiler->uint4Id) {
        baseTypeId = compiler->uintId;
    } else if (typeId == compiler->int4Id) {
        baseTypeId = compiler->intId;
    } else {
        assert(false);
    }

    const IlcSpvWord compIndex[] = {
        COMP_INDEX_X + offset, COMP_INDEX_Y + offset,
        COMP_INDEX_Z + offset, COMP_INDEX_W + offset
    };

    if (count == 1) {
        // Extract scalar
        return ilcSpvPutCompositeExtract(compiler->module, baseTypeId, vecId, 1, compIndex);
    } else {
        IlcSpvId vecTypeId = ilcSpvPutVectorType(compiler->module, baseTypeId, count);
        return ilcSpvPutVectorShuffle(compiler->module, vecTypeId, vecId, vecId, count, compIndex);
    }
}

static const IlcRegister* addRegister(
    IlcCompiler* compiler,
    const IlcRegister* reg,
    char prefix)
{
    char name[16];
    snprintf(name, 16, "%c%u", prefix, reg->ilNum);
    ilcSpvPutName(compiler->module, reg->id, name);

    compiler->regCount++;
    compiler->regs = realloc(compiler->regs, sizeof(IlcRegister) * compiler->regCount);
    compiler->regs[compiler->regCount - 1] = *reg;

    return &compiler->regs[compiler->regCount - 1];
}

static const IlcRegister* findRegister(
    IlcCompiler* compiler,
    uint32_t type,
    uint32_t num)
{
    for (int i = 0; i < compiler->regCount; i++) {
        const IlcRegister* reg = &compiler->regs[i];

        if (reg->ilType == type && reg->ilNum == num) {
            return reg;
        }
    }

    return NULL;
}

static const IlcRegister* findOrCreateRegister(
    IlcCompiler* compiler,
    uint32_t type,
    uint32_t num)
{
    const IlcRegister* reg = findRegister(compiler, type, num);

    if (reg == NULL && type == IL_REGTYPE_TEMP) {
        // Create temporary register
        IlcSpvId tempTypeId = 0;
        IlcSpvId tempId = emitVectorVariable(compiler, &tempTypeId, 4, compiler->floatId,
                                             SpvStorageClassPrivate);

        const IlcRegister tempReg = {
            .id = tempId,
            .typeId = tempTypeId,
            .baseTypeId = 0,
            .ilType = type,
            .ilNum = num,
            .ilImportUsage = 0,
            .ilInterpMode = 0,
        };

        reg = addRegister(compiler, &tempReg, 'r');
    }

    return reg;
}

static const IlcResource* findResource(
    IlcCompiler* compiler,
    uint32_t ilId)
{
    for (int i = 0; i < compiler->resourceCount; i++) {
        const IlcResource* resource = &compiler->resources[i];

        if (resource->ilId == ilId) {
            return resource;
        }
    }

    return NULL;
}

static void addResource(
    IlcCompiler* compiler,
    const IlcResource* resource)
{
    if (findResource(compiler, resource->ilId) != NULL) {
        LOGE("resource %d already present\n", resource->ilId);
        assert(false);
    }

    char name[32];
    snprintf(name, 32, "resource%u", resource->ilId);
    ilcSpvPutName(compiler->module, resource->id, name);

    compiler->resourceCount++;
    compiler->resources = realloc(compiler->resources,
                                  sizeof(IlcResource) * compiler->resourceCount);
    compiler->resources[compiler->resourceCount - 1] = *resource;
}

static const IlcSampler* findSampler(
    IlcCompiler* compiler,
    uint32_t ilId)
{
    for (int i = 0; i < compiler->samplerCount; i++) {
        const IlcSampler* sampler = &compiler->samplers[i];

        if (sampler->ilId == ilId) {
            return sampler;
        }
    }

    return NULL;
}

static const IlcSampler* addSampler(
    IlcCompiler* compiler,
    const IlcSampler* sampler)
{
    if (findSampler(compiler, sampler->ilId) != NULL) {
        LOGE("sampler %d already present\n", sampler->ilId);
        assert(false);
    }

    char name[32];
    snprintf(name, 32, "sampler%u", sampler->ilId);
    ilcSpvPutName(compiler->module, sampler->id, name);

    compiler->samplerCount++;
    compiler->samplers = realloc(compiler->samplers, sizeof(IlcSampler) * compiler->samplerCount);
    compiler->samplers[compiler->samplerCount - 1] = *sampler;

    return &compiler->samplers[compiler->samplerCount - 1];
}

static const IlcSampler* findOrCreateSampler(
    IlcCompiler* compiler,
    uint32_t ilId)
{
    const IlcSampler* sampler = findSampler(compiler, ilId);

    if (sampler == NULL) {
        // Create new sampler
        IlcSpvId samplerTypeId = ilcSpvPutSamplerType(compiler->module);
        IlcSpvId pointerId = ilcSpvPutPointerType(compiler->module, SpvStorageClassUniformConstant,
                                                  samplerTypeId);
        IlcSpvId samplerId = ilcSpvPutVariable(compiler->module, pointerId,
                                               SpvStorageClassUniformConstant);

        IlcSpvWord descriptorSetIdx = compiler->kernel->shaderType;
        ilcSpvPutDecoration(compiler->module, samplerId, SpvDecorationDescriptorSet,
                            1, &descriptorSetIdx);
        IlcSpvWord bindingIdx = ilId;
        ilcSpvPutDecoration(compiler->module, samplerId, SpvDecorationBinding, 1, &bindingIdx);

        const IlcSampler newSampler = {
            .id = samplerId,
            .ilId = ilId,
        };

        sampler = addSampler(compiler, &newSampler);
    }

    return sampler;
}

static void pushControlFlowBlock(
    IlcCompiler* compiler,
    const IlcControlFlowBlock* block)
{
    compiler->controlFlowBlockCount++;
    size_t size = sizeof(IlcControlFlowBlock) * compiler->controlFlowBlockCount;
    compiler->controlFlowBlocks = realloc(compiler->controlFlowBlocks, size);
    compiler->controlFlowBlocks[compiler->controlFlowBlockCount - 1] = *block;
}

static IlcControlFlowBlock popControlFlowBlock(
    IlcCompiler* compiler)
{
    assert(compiler->controlFlowBlockCount > 0);
    IlcControlFlowBlock block = compiler->controlFlowBlocks[compiler->controlFlowBlockCount - 1];

    compiler->controlFlowBlockCount--;
    size_t size = sizeof(IlcControlFlowBlock) * compiler->controlFlowBlockCount;
    compiler->controlFlowBlocks = realloc(compiler->controlFlowBlocks, size);

    return block;
}

static const IlcControlFlowBlock* findControlFlowBlock(
    IlcCompiler* compiler,
    IlcControlFlowBlockType type)
{
    for (int i = compiler->controlFlowBlockCount - 1; i >= 0; i--) {
        const IlcControlFlowBlock* block = &compiler->controlFlowBlocks[i];

        if (block->type == type) {
            return block;
        }
    }

    return NULL;
}

static IlcSpvId loadSource(
    IlcCompiler* compiler,
    const Source* src,
    uint8_t componentMask,
    IlcSpvId typeId)
{
    if (src->hasRelativeSrc) {
        LOGW("unhandled relative source\n");
    }

    const IlcRegister* reg = findRegister(compiler, src->registerType, src->registerNum);

    if (reg == NULL) {
        LOGE("source register %d %d not found\n", src->registerType, src->registerNum);
        return 0;
    }

    IlcSpvId ptrId = 0;
    if (src->registerType != IL_REGTYPE_ITEMP) {
        if (src->hasImmediate) {
            LOGW("unhandled immediate\n");
        }
        ptrId = reg->id;
    } else {
        IlcSpvId ptrTypeId = ilcSpvPutPointerType(compiler->module, SpvStorageClassPrivate,
                                                  reg->typeId);
        IlcSpvId indexId = ilcSpvPutConstant(compiler->module, compiler->intId,
                                             src->hasImmediate ? src->immediate : 0);
        // TODO handle relative source
        ptrId = ilcSpvPutAccessChain(compiler->module, ptrTypeId, reg->id, indexId);
    }

    IlcSpvId varId = ilcSpvPutLoad(compiler->module, reg->typeId, ptrId);

    if (reg->typeId == compiler->floatId || reg->typeId == compiler->intId) {
        // Convert scalar to float vector
        if (reg->typeId != compiler->floatId) {
            varId = ilcSpvPutBitcast(compiler->module, compiler->floatId, varId);
        }

        const IlcSpvWord constituents[] = { varId, varId, varId, varId };
        varId = ilcSpvPutCompositeConstruct(compiler->module, compiler->float4Id, 4, constituents);
    }

    const uint8_t swizzle[] = {
        componentMask & 1 ? src->swizzle[0] : IL_COMPSEL_0,
        componentMask & 2 ? src->swizzle[1] : IL_COMPSEL_0,
        componentMask & 4 ? src->swizzle[2] : IL_COMPSEL_0,
        componentMask & 8 ? src->swizzle[3] : IL_COMPSEL_0,
    };

    if (swizzle[0] != IL_COMPSEL_X_R || swizzle[1] != IL_COMPSEL_Y_G ||
        swizzle[2] != IL_COMPSEL_Z_B || swizzle[3] != IL_COMPSEL_W_A) {
        // Select components from {x, y, z, w, 0.f, 1.f}
        IlcSpvId zeroOneId = emitZeroOneVector(compiler);

        const IlcSpvWord components[] = { swizzle[0], swizzle[1], swizzle[2], swizzle[3] };
        varId = ilcSpvPutVectorShuffle(compiler->module, compiler->float4Id, varId, zeroOneId,
                                       4, components);
    }

    if (typeId != reg->typeId) {
        // Need to cast to the expected type
        varId = ilcSpvPutBitcast(compiler->module, typeId, varId);
    }

    // All following operations but `neg` are float only (AMDIL spec, table 2.10)

    if (src->invert) {
        LOGW("unhandled invert flag\n");
    }

    if (src->bias) {
        LOGW("unhandled bias flag\n");
    }

    if (src->x2) {
        LOGW("unhandled x2 flag\n");
    }

    if (src->sign) {
        LOGW("unhandled sign flag\n");
    }

    if (src->divComp != IL_DIVCOMP_NONE) {
        LOGW("unhandled divcomp %d\n", src->divComp);
    }

    if (src->abs) {
        varId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450FAbs, compiler->float4Id, 1, &varId);
    }

    if (src->negate[0] || src->negate[1] || src->negate[2] || src->negate[3]) {
        IlcSpvId negId = 0;

        if (typeId == compiler->float4Id) {
            negId = ilcSpvPutAlu(compiler->module, SpvOpFNegate, compiler->float4Id, 1, &varId);
        } else if (typeId == compiler->int4Id) {
            negId = ilcSpvPutAlu(compiler->module, SpvOpSNegate, compiler->int4Id, 1, &varId);
        } else {
            assert(false);
        }

        if (src->negate[0] && src->negate[1] && src->negate[2] && src->negate[3]) {
            varId = negId;
        } else {
            // Select components from {-x, -y, -z, -w, x, y, z, w}
            const IlcSpvWord components[] = {
                src->negate[0] ? 0 : 4,
                src->negate[1] ? 1 : 5,
                src->negate[2] ? 2 : 6,
                src->negate[3] ? 3 : 7,
            };

            varId = ilcSpvPutVectorShuffle(compiler->module, typeId, negId, varId, 4, components);
        }
    }

    if (src->clamp) {
        LOGW("unhandled clamp flag\n");
    }

    return varId;
}

static void storeDestination(
    IlcCompiler* compiler,
    const Destination* dst,
    IlcSpvId varId)
{
    const IlcRegister* reg = findOrCreateRegister(compiler, dst->registerType, dst->registerNum);

    if (reg == NULL) {
        LOGE("destination register %d %d not found\n", dst->registerType, dst->registerNum);
        return;
    }

    IlcSpvId ptrId = 0;
    if (dst->registerType != IL_REGTYPE_ITEMP) {
        if (dst->hasImmediate) {
            LOGW("unhandled immediate\n");
        }
        ptrId = reg->id;
    } else {
        assert(dst->hasImmediate);
        IlcSpvId ptrTypeId = ilcSpvPutPointerType(compiler->module, SpvStorageClassPrivate,
                                                  reg->typeId);
        IlcSpvId indexId = ilcSpvPutConstant(compiler->module, compiler->intId, dst->immediate);
        ptrId = ilcSpvPutAccessChain(compiler->module, ptrTypeId, reg->id, indexId);
    }

    if (dst->shiftScale != IL_SHIFT_NONE) {
        LOGW("unhandled shift scale %d\n", dst->shiftScale);
    }

    if (dst->clamp) {
        // Clamp to [0.f, 1.f]
        IlcSpvId zeroId = ilcSpvPutConstant(compiler->module, compiler->floatId, ZERO_LITERAL);
        IlcSpvId oneId = ilcSpvPutConstant(compiler->module, compiler->floatId, ONE_LITERAL);
        const IlcSpvId zeroConsistuentIds[] = { zeroId, zeroId, zeroId, zeroId };
        const IlcSpvId oneConsistuentIds[] = { oneId, oneId, oneId, oneId };
        IlcSpvId zeroCompositeId = ilcSpvPutConstantComposite(compiler->module, reg->typeId,
                                                              4, zeroConsistuentIds);
        IlcSpvId oneCompositeId = ilcSpvPutConstantComposite(compiler->module, reg->typeId,
                                                             4, oneConsistuentIds);

        const IlcSpvId paramIds[] = { varId, zeroCompositeId, oneCompositeId };
        varId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450FClamp, reg->typeId, 3, paramIds);
    }

    if (dst->component[0] == IL_MODCOMP_NOWRITE || dst->component[1] == IL_MODCOMP_NOWRITE ||
        dst->component[2] == IL_MODCOMP_NOWRITE || dst->component[3] == IL_MODCOMP_NOWRITE) {
        // Select components from {dst.x, dst.y, dst.z, dst.w, x, y, z, w}
        IlcSpvId origId = ilcSpvPutLoad(compiler->module, reg->typeId, ptrId);

        const IlcSpvWord components[] = {
            dst->component[0] == IL_MODCOMP_NOWRITE ? 0 : 4,
            dst->component[1] == IL_MODCOMP_NOWRITE ? 1 : 5,
            dst->component[2] == IL_MODCOMP_NOWRITE ? 2 : 6,
            dst->component[3] == IL_MODCOMP_NOWRITE ? 3 : 7,
        };
        varId = ilcSpvPutVectorShuffle(compiler->module, reg->typeId, origId, varId,
                                       4, components);
    }

    if ((dst->component[0] == IL_MODCOMP_0 || dst->component[0] == IL_MODCOMP_1) ||
        (dst->component[1] == IL_MODCOMP_0 || dst->component[1] == IL_MODCOMP_1) ||
        (dst->component[2] == IL_MODCOMP_0 || dst->component[2] == IL_MODCOMP_1) ||
        (dst->component[3] == IL_MODCOMP_0 || dst->component[3] == IL_MODCOMP_1)) {
        // Select components from {x, y, z, w, 0.f, 1.f}
        IlcSpvId zeroOneId = emitZeroOneVector(compiler);

        const IlcSpvWord components[] = {
            dst->component[0] == IL_MODCOMP_0 ? 4 : (dst->component[0] == IL_MODCOMP_1 ? 5 : 0),
            dst->component[1] == IL_MODCOMP_0 ? 4 : (dst->component[1] == IL_MODCOMP_1 ? 5 : 1),
            dst->component[2] == IL_MODCOMP_0 ? 4 : (dst->component[2] == IL_MODCOMP_1 ? 5 : 2),
            dst->component[3] == IL_MODCOMP_0 ? 4 : (dst->component[3] == IL_MODCOMP_1 ? 5 : 3),
        };
        varId = ilcSpvPutVectorShuffle(compiler->module, reg->typeId, varId, zeroOneId,
                                       4, components);
    }

    ilcSpvPutStore(compiler->module, ptrId, varId);
}

static void emitGlobalFlags(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    bool refactoringAllowed = GET_BIT(instr->control, 0);
    bool forceEarlyDepthStencil = GET_BIT(instr->control, 1);
    bool enableRawStructuredBuffers = GET_BIT(instr->control, 2);
    bool enableDoublePrecisionFloatOps = GET_BIT(instr->control, 3);

    if (!refactoringAllowed) {
        LOGW("unhandled !refactoringAllowed flag\n");
    }
    if (forceEarlyDepthStencil) {
        LOGW("unhandled forceEarlyDepthStencil flag\n");
    }
    if (enableRawStructuredBuffers) {
        LOGW("unhandled enableRawStructuredBuffers flag\n");
    }
    if (enableDoublePrecisionFloatOps) {
        LOGW("unhandled enableDoublePrecisionFloatOps flag\n");
    }
}

static void emitIndexedTempArray(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    const Source* src = &instr->srcs[0];

    assert(src->registerType == IL_REGTYPE_ITEMP && src->hasImmediate);

    // Create temporary array register
    unsigned arraySize = src->immediate;
    IlcSpvId arrayTypeId = ilcSpvPutMatrixType(compiler->module, compiler->float4Id, arraySize);
    IlcSpvId pointerId = ilcSpvPutPointerType(compiler->module, SpvStorageClassPrivate,
                                              arrayTypeId);
    IlcSpvId arrayId = ilcSpvPutVariable(compiler->module, pointerId, SpvStorageClassPrivate);

    const IlcRegister tempArrayReg = {
        .id = arrayId,
        .typeId = compiler->float4Id,
        .baseTypeId = arrayTypeId,
        .ilType = src->registerType,
        .ilNum = src->registerNum,
        .ilImportUsage = 0,
        .ilInterpMode = 0,
    };

    addRegister(compiler, &tempArrayReg, 'x');
}

static void emitLiteral(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    const Source* src = &instr->srcs[0];

    assert(src->registerType == IL_REGTYPE_LITERAL);

    IlcSpvId literalTypeId = 0;
    IlcSpvId literalId = emitVectorVariable(compiler, &literalTypeId, 4, compiler->floatId,
                                            SpvStorageClassPrivate);

    IlcSpvId consistuentIds[] = {
        ilcSpvPutConstant(compiler->module, compiler->floatId, instr->extras[0]),
        ilcSpvPutConstant(compiler->module, compiler->floatId, instr->extras[1]),
        ilcSpvPutConstant(compiler->module, compiler->floatId, instr->extras[2]),
        ilcSpvPutConstant(compiler->module, compiler->floatId, instr->extras[3]),
    };
    IlcSpvId compositeId = ilcSpvPutConstantComposite(compiler->module, literalTypeId,
                                                      4, consistuentIds);

    ilcSpvPutStore(compiler->module, literalId, compositeId);

    const IlcRegister reg = {
        .id = literalId,
        .typeId = literalTypeId,
        .baseTypeId = 0,
        .ilType = src->registerType,
        .ilNum = src->registerNum,
        .ilImportUsage = 0,
        .ilInterpMode = 0,
    };

    addRegister(compiler, &reg, 'l');
}

static void emitOutput(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint8_t importUsage = GET_BITS(instr->control, 0, 4);

    assert(instr->dstCount == 1 &&
           instr->srcCount == 0 &&
           instr->extraCount == 0);

    const Destination* dst = &instr->dsts[0];

    assert(dst->registerType == IL_REGTYPE_OUTPUT &&
           !dst->clamp &&
           dst->shiftScale == IL_SHIFT_NONE);

    const IlcRegister* dupeReg = findRegister(compiler, dst->registerType, dst->registerNum);
    if (dupeReg != NULL) {
        // Outputs are allowed to be redeclared with different components.
        // Can be safely ignored as long as the import usage is equivalent.
        if (dupeReg->ilImportUsage == importUsage) {
            return;
        } else {
            LOGE("unhandled o%d redeclaration with different import usage %d (was %d)\n",
                 dst->registerNum, importUsage, dupeReg->ilImportUsage);
            assert(false);
        }
    }

    IlcSpvId outputTypeId = 0;
    IlcSpvId outputId = emitVectorVariable(compiler, &outputTypeId, 4, compiler->floatId,
                                           SpvStorageClassOutput);

    if (importUsage == IL_IMPORTUSAGE_POS) {
        IlcSpvWord builtInType = SpvBuiltInPosition;
        ilcSpvPutDecoration(compiler->module, outputId, SpvDecorationBuiltIn, 1, &builtInType);
    } else if (importUsage == IL_IMPORTUSAGE_GENERIC) {
        IlcSpvWord locationIdx = dst->registerNum;
        ilcSpvPutDecoration(compiler->module, outputId, SpvDecorationLocation, 1, &locationIdx);
    } else {
        LOGW("unhandled import usage %d\n", importUsage);
    }

    const IlcRegister reg = {
        .id = outputId,
        .typeId = outputTypeId,
        .baseTypeId = 0,
        .ilType = dst->registerType,
        .ilNum = dst->registerNum,
        .ilImportUsage = importUsage,
        .ilInterpMode = 0,
    };

    addRegister(compiler, &reg, 'o');
}

static void emitInput(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint8_t importUsage = GET_BITS(instr->control, 0, 4);
    uint8_t interpMode = GET_BITS(instr->control, 5, 7);
    IlcSpvId inputId = 0;
    IlcSpvId inputTypeId = 0;

    assert(instr->dstCount == 1 &&
           instr->srcCount == 0 &&
           instr->extraCount == 0);

    const Destination* dst = &instr->dsts[0];

    assert(dst->registerType == IL_REGTYPE_INPUT &&
           !dst->clamp &&
           dst->shiftScale == IL_SHIFT_NONE);

    const IlcRegister* dupeReg = findRegister(compiler, dst->registerType, dst->registerNum);
    if (dupeReg != NULL) {
        // Inputs are allowed to be redeclared with different components.
        // Can be safely ignored as long as the import usage and interp mode are equivalent.
        if (dupeReg->ilImportUsage == importUsage && dupeReg->ilInterpMode == interpMode) {
            return;
        } else {
            LOGE("unhandled v%d redeclaration with different import usage %d (was %d) or "
                 "interp mode %d (was %d)\n",
                 dst->registerNum, importUsage, dupeReg->ilImportUsage,
                 interpMode, dupeReg->ilInterpMode);
            assert(false);
        }
    }

    if (importUsage == IL_IMPORTUSAGE_POS) {
        inputId = emitVectorVariable(compiler, &inputTypeId, 4, compiler->floatId,
                                     SpvStorageClassInput);

        IlcSpvWord builtInType = SpvBuiltInFragCoord;
        ilcSpvPutDecoration(compiler->module, inputId, SpvDecorationBuiltIn, 1, &builtInType);
    } else if (importUsage == IL_IMPORTUSAGE_GENERIC) {
        inputId = emitVectorVariable(compiler, &inputTypeId, 4, compiler->floatId,
                                     SpvStorageClassInput);

        IlcSpvWord locationIdx = dst->registerNum;
        ilcSpvPutDecoration(compiler->module, inputId, SpvDecorationLocation, 1, &locationIdx);
    } else if (importUsage == IL_IMPORTUSAGE_VERTEXID ||
               importUsage == IL_IMPORTUSAGE_INSTANCEID) {
        IlcSpvId pointerId = ilcSpvPutPointerType(compiler->module, SpvStorageClassInput,
                                                  compiler->intId);
        inputId = ilcSpvPutVariable(compiler->module, pointerId, SpvStorageClassInput);
        inputTypeId = compiler->intId;

        IlcSpvWord builtInType = importUsage == IL_IMPORTUSAGE_VERTEXID ?
                                 SpvBuiltInVertexIndex : SpvBuiltInInstanceIndex;
        ilcSpvPutDecoration(compiler->module, inputId, SpvDecorationBuiltIn, 1, &builtInType);
    } else {
        LOGW("unhandled import usage %d\n", importUsage);
    }

    // Handle interpolation modes in pixel shaders
    if (interpMode == IL_INTERPMODE_CONSTANT) {
        ilcSpvPutDecoration(compiler->module, inputId, SpvDecorationFlat, 0, NULL);
    }
    if (interpMode == IL_INTERPMODE_LINEAR_CENTROID ||
        interpMode == IL_INTERPMODE_LINEAR_NOPERSPECTIVE_CENTROID) {
        ilcSpvPutDecoration(compiler->module, inputId, SpvDecorationCentroid, 0, NULL);
    }
    if (interpMode == IL_INTERPMODE_LINEAR_NOPERSPECTIVE ||
        interpMode == IL_INTERPMODE_LINEAR_NOPERSPECTIVE_CENTROID ||
        interpMode == IL_INTERPMODE_LINEAR_NOPERSPECTIVE_SAMPLE) {
        ilcSpvPutDecoration(compiler->module, inputId, SpvDecorationNoPerspective, 0, NULL);
    }
    if (interpMode == IL_INTERPMODE_LINEAR_SAMPLE ||
        interpMode == IL_INTERPMODE_LINEAR_NOPERSPECTIVE_SAMPLE) {
        ilcSpvPutCapability(compiler->module, SpvCapabilitySampleRateShading);
        ilcSpvPutDecoration(compiler->module, inputId, SpvDecorationSample, 0, NULL);
    }

    const IlcRegister reg = {
        .id = inputId,
        .typeId = inputTypeId,
        .baseTypeId = 0,
        .ilType = dst->registerType,
        .ilNum = dst->registerNum,
        .ilImportUsage = importUsage,
        .ilInterpMode = interpMode,
    };

    addRegister(compiler, &reg, 'v');
}

static void emitResource(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    assert(instr->dstCount == 0 &&
           instr->srcCount == 0 &&
           instr->extraCount == 1);

    uint8_t id = GET_BITS(instr->control, 0, 7);
    uint8_t type = GET_BITS(instr->control, 8, 11);
    bool unnorm = GET_BIT(instr->control, 31);
    uint8_t fmtx = GET_BITS(instr->extras[0], 20, 22);
    uint8_t fmty = GET_BITS(instr->extras[0], 23, 25);
    uint8_t fmtz = GET_BITS(instr->extras[0], 26, 28);
    uint8_t fmtw = GET_BITS(instr->extras[0], 29, 31);

    SpvDim spvDim = 0;
    switch (type) {
    case IL_USAGE_PIXTEX_1D:
    case IL_USAGE_PIXTEX_1DARRAY:
        spvDim = SpvDim1D;
        ilcSpvPutCapability(compiler->module, SpvCapabilityImage1D);
        break;
    case IL_USAGE_PIXTEX_2D:
    case IL_USAGE_PIXTEX_2DARRAY:
        spvDim = SpvDim2D;
        break;
    case IL_USAGE_PIXTEX_3D:
        spvDim = SpvDim3D;
        break;
    case IL_USAGE_PIXTEX_BUFFER:
        spvDim = SpvDimBuffer;
        ilcSpvPutCapability(compiler->module, SpvCapabilityImageBuffer);
        break;
    default:
        LOGE("unhandled resource type %d\n", type);
        assert(false);
    }

    if (unnorm) {
        LOGE("unhandled unnorm resource\n");
        assert(false);
    }

    SpvImageFormat spvImageFormat;
    if (fmtx == IL_ELEMENTFORMAT_FLOAT && fmty == IL_ELEMENTFORMAT_FLOAT &&
        fmtz == IL_ELEMENTFORMAT_FLOAT && fmtw == IL_ELEMENTFORMAT_FLOAT) {
        spvImageFormat = SpvImageFormatRgba32f;
    } else if (fmtx == IL_ELEMENTFORMAT_SINT && fmty == IL_ELEMENTFORMAT_SINT &&
               fmtz == IL_ELEMENTFORMAT_SINT && fmtw == IL_ELEMENTFORMAT_SINT) {
        spvImageFormat = SpvImageFormatRgba32i;
    } else if (fmtx == IL_ELEMENTFORMAT_UINT && fmty == IL_ELEMENTFORMAT_UINT &&
               fmtz == IL_ELEMENTFORMAT_UINT && fmtw == IL_ELEMENTFORMAT_UINT) {
        spvImageFormat = SpvImageFormatRgba32ui;
    } else {
        LOGE("unhandled resource format %d %d %d %d\n", fmtx, fmty, fmtz, fmtw);
        assert(false);
    }

    IlcSpvId imageId = ilcSpvPutImageType(compiler->module, compiler->floatId, spvDim,
                                          0, 0, 0, 1, spvImageFormat);
    IlcSpvId pImageId = ilcSpvPutPointerType(compiler->module, SpvStorageClassUniformConstant,
                                             imageId);
    IlcSpvId resourceId = ilcSpvPutVariable(compiler->module, pImageId,
                                            SpvStorageClassUniformConstant);

    IlcSpvWord descriptorSetIdx = compiler->kernel->shaderType;
    ilcSpvPutDecoration(compiler->module, resourceId, SpvDecorationDescriptorSet,
                        1, &descriptorSetIdx);
    IlcSpvWord bindingIdx = id;
    ilcSpvPutDecoration(compiler->module, resourceId, SpvDecorationBinding, 1, &bindingIdx);

    const IlcResource resource = {
        .id = resourceId,
        .typeId = imageId,
        .ilId = id,
        .ilType = type,
        .strideId = 0,
    };

    addResource(compiler, &resource);
}

static void emitStructuredSrv(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint16_t id = GET_BITS(instr->control, 0, 13);

    IlcSpvId imageId = ilcSpvPutImageType(compiler->module, compiler->intId, SpvDimBuffer,
                                          0, 0, 0, 1, SpvImageFormatR32i);
    IlcSpvId pImageId = ilcSpvPutPointerType(compiler->module, SpvStorageClassUniformConstant,
                                             imageId);
    IlcSpvId resourceId = ilcSpvPutVariable(compiler->module, pImageId,
                                            SpvStorageClassUniformConstant);

    ilcSpvPutCapability(compiler->module, SpvCapabilitySampledBuffer);
    ilcSpvPutName(compiler->module, imageId, "structSrv");

    IlcSpvWord descriptorSetIdx = compiler->kernel->shaderType;
    ilcSpvPutDecoration(compiler->module, resourceId, SpvDecorationDescriptorSet,
                        1, &descriptorSetIdx);
    IlcSpvWord bindingIdx = id;
    ilcSpvPutDecoration(compiler->module, resourceId, SpvDecorationBinding, 1, &bindingIdx);

    const IlcResource resource = {
        .id = resourceId,
        .typeId = imageId,
        .ilId = id,
        .ilType = IL_USAGE_PIXTEX_UNKNOWN,
        .strideId = ilcSpvPutConstant(compiler->module, compiler->intId, instr->extras[0]),
    };

    addResource(compiler, &resource);
}

static void emitFunc(
    IlcCompiler* compiler,
    IlcSpvId id)
{
    IlcSpvId voidTypeId = ilcSpvPutVoidType(compiler->module);
    IlcSpvId funcTypeId = ilcSpvPutFunctionType(compiler->module, voidTypeId, 0, NULL);
    ilcSpvPutFunction(compiler->module, voidTypeId, id, SpvFunctionControlMaskNone, funcTypeId);
    ilcSpvPutLabel(compiler->module, 0);
}

static void emitFloatOp(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    IlcSpvId srcIds[MAX_SRC_COUNT] = { 0 };
    IlcSpvId resId = 0;
    uint8_t componentMask = 0;

    switch (instr->opcode) {
    case IL_OP_DP2:
        componentMask = COMP_MASK_XY;
        break;
    case IL_OP_DP3:
        componentMask = COMP_MASK_XYZ;
        break;
    default:
        componentMask = COMP_MASK_XYZW;
        break;
    }

    for (int i = 0; i < instr->srcCount; i++) {
        srcIds[i] = loadSource(compiler, &instr->srcs[i], componentMask, compiler->float4Id);
    }

    switch (instr->opcode) {
    case IL_OP_ABS:
        resId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450FAbs, compiler->float4Id,
                                instr->srcCount, srcIds);
        break;
    case IL_OP_ACOS: {
        IlcSpvId acosId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450Acos, compiler->float4Id,
                                          instr->srcCount, srcIds);
        // Replicate .w on all components
        const IlcSpvWord components[] = { COMP_INDEX_W, COMP_INDEX_W, COMP_INDEX_W, COMP_INDEX_W };
        resId = ilcSpvPutVectorShuffle(compiler->module, compiler->float4Id, acosId, acosId,
                                       4, components);
    }   break;
    case IL_OP_ADD:
        resId = ilcSpvPutAlu(compiler->module, SpvOpFAdd, compiler->float4Id,
                             instr->srcCount, srcIds);
        break;
    case IL_OP_ASIN: {
        IlcSpvId asinId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450Asin, compiler->float4Id,
                                          instr->srcCount, srcIds);
        // Replicate .w on all components
        const IlcSpvWord components[] = { COMP_INDEX_W, COMP_INDEX_W, COMP_INDEX_W, COMP_INDEX_W };
        resId = ilcSpvPutVectorShuffle(compiler->module, compiler->float4Id, asinId, asinId,
                                       4, components);
    }   break;
    case IL_OP_ATAN: {
        IlcSpvId atanId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450Atan, compiler->float4Id,
                                          instr->srcCount, srcIds);
        // Replicate .w on all components
        const IlcSpvWord components[] = { COMP_INDEX_W, COMP_INDEX_W, COMP_INDEX_W, COMP_INDEX_W };
        resId = ilcSpvPutVectorShuffle(compiler->module, compiler->float4Id, atanId, atanId,
                                       4, components);
    }   break;
    case IL_OP_DIV:
        if (instr->control != IL_ZEROOP_INFINITY) {
            LOGW("unhandled div zero op %d\n", instr->control);
        }
        // FIXME SPIR-V has undefined division by zero
        resId = ilcSpvPutAlu(compiler->module, SpvOpFDiv, compiler->float4Id,
                             instr->srcCount, srcIds);
        break;
    case IL_OP_DP2:
    case IL_OP_DP3:
    case IL_OP_DP4: {
        bool ieee = GET_BIT(instr->control, 0);
        if (!ieee) {
            LOGW("unhandled non-IEEE dot product\n");
        }
        IlcSpvId dotId = ilcSpvPutAlu(compiler->module, SpvOpDot, compiler->floatId,
                                      instr->srcCount, srcIds);
        // Replicate dot product on all components
        const IlcSpvWord constituents[] = { dotId, dotId, dotId, dotId };
        resId = ilcSpvPutCompositeConstruct(compiler->module, compiler->float4Id, 4, constituents);
    }   break;
    case IL_OP_DSX:
    case IL_OP_DSY: {
        bool fine = GET_BIT(instr->control, 7);
        IlcSpvWord op = instr->opcode == IL_OP_DSX ? (fine ? SpvOpDPdxFine : SpvOpDPdxCoarse)
                                                   : (fine ? SpvOpDPdyFine : SpvOpDPdyCoarse);
        ilcSpvPutCapability(compiler->module, SpvCapabilityDerivativeControl);
        resId = ilcSpvPutAlu(compiler->module, op, compiler->float4Id, instr->srcCount, srcIds);
    }   break;
    case IL_OP_FRC:
        resId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450Fract, compiler->float4Id,
                                instr->srcCount, srcIds);
        break;
    case IL_OP_MAD: {
        bool ieee = GET_BIT(instr->control, 0);
        if (!ieee) {
            LOGW("unhandled non-IEEE mad\n");
        }
        resId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450Fma, compiler->float4Id,
                                instr->srcCount, srcIds);
    }   break;
    case IL_OP_MAX: {
        bool ieee = GET_BIT(instr->control, 0);
        if (!ieee) {
            LOGW("unhandled non-IEEE max\n");
        }
        resId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450NMax, compiler->float4Id,
                                instr->srcCount, srcIds);
    }   break;
    case IL_OP_MIN: {
        bool ieee = GET_BIT(instr->control, 0);
        if (!ieee) {
            LOGW("unhandled non-IEEE min\n");
        }
        resId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450NMin, compiler->float4Id,
                                instr->srcCount, srcIds);
    }   break;
    case IL_OP_MOV:
        resId = srcIds[0];
        break;
    case IL_OP_MUL: {
        bool ieee = GET_BIT(instr->control, 0);
        if (!ieee) {
            LOGW("unhandled non-IEEE mul\n");
        }
        resId = ilcSpvPutAlu(compiler->module, SpvOpFMul, compiler->float4Id,
                             instr->srcCount, srcIds);
    }   break;
    case IL_OP_FTOI:
        resId = ilcSpvPutAlu(compiler->module, SpvOpConvertFToS, compiler->int4Id,
                             instr->srcCount, srcIds);
        resId = ilcSpvPutBitcast(compiler->module, compiler->float4Id, resId);
        break;
    case IL_OP_FTOU:
        resId = ilcSpvPutAlu(compiler->module, SpvOpConvertFToU, compiler->uint4Id,
                             instr->srcCount, srcIds);
        resId = ilcSpvPutBitcast(compiler->module, compiler->float4Id, resId);
        break;
    case IL_OP_ITOF:
        resId = ilcSpvPutBitcast(compiler->module, compiler->int4Id, srcIds[0]);
        resId = ilcSpvPutAlu(compiler->module, SpvOpConvertSToF, compiler->float4Id, 1, &resId);
        break;
    case IL_OP_UTOF:
        resId = ilcSpvPutBitcast(compiler->module, compiler->uint4Id, srcIds[0]);
        resId = ilcSpvPutAlu(compiler->module, SpvOpConvertUToF, compiler->float4Id, 1, &resId);
        break;
    case IL_OP_ROUND_NEG_INF:
        resId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450Floor, compiler->float4Id,
                                instr->srcCount, srcIds);
        break;
    case IL_OP_ROUND_PLUS_INF:
        resId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450Ceil, compiler->float4Id,
                                instr->srcCount, srcIds);
        break;
    case IL_OP_EXP_VEC:
        resId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450Exp, compiler->float4Id,
                                instr->srcCount, srcIds);
        break;
    case IL_OP_LOG_VEC:
        // FIXME handle log(0)
        resId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450Log, compiler->float4Id,
                                instr->srcCount, srcIds);
        break;
    case IL_OP_RSQ_VEC:
        resId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450InverseSqrt, compiler->float4Id,
                                instr->srcCount, srcIds);
        break;
    case IL_OP_SIN_VEC:
        resId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450Sin, compiler->float4Id,
                                instr->srcCount, srcIds);
        break;
    case IL_OP_COS_VEC:
        resId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450Cos, compiler->float4Id,
                                instr->srcCount, srcIds);
        break;
    case IL_OP_SQRT_VEC:
        resId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450Sqrt, compiler->float4Id,
                                instr->srcCount, srcIds);
        break;
    default:
        assert(false);
        break;
    }

    storeDestination(compiler, &instr->dsts[0], resId);
}

static void emitFloatComparisonOp(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    IlcSpvId srcIds[MAX_SRC_COUNT] = { 0 };
    SpvOp compOp = 0;

    for (int i = 0; i < instr->srcCount; i++) {
        srcIds[i] = loadSource(compiler, &instr->srcs[i], COMP_MASK_XYZW, compiler->float4Id);
    }

    switch (instr->opcode) {
    case IL_OP_EQ:
        compOp = SpvOpFOrdEqual;
        break;
    case IL_OP_GE:
        compOp = SpvOpFOrdGreaterThanEqual;
        break;
    case IL_OP_LT:
        compOp = SpvOpFOrdLessThan;
        break;
    case IL_OP_NE:
        compOp = SpvOpFOrdNotEqual;
        break;
    default:
        assert(false);
        break;
    }

    IlcSpvId condId = ilcSpvPutAlu(compiler->module, compOp, compiler->bool4Id,
                                   instr->srcCount, srcIds);
    IlcSpvId trueId = ilcSpvPutConstant(compiler->module, compiler->floatId, TRUE_LITERAL);
    IlcSpvId falseId = ilcSpvPutConstant(compiler->module, compiler->floatId, FALSE_LITERAL);
    const IlcSpvId trueConsistuentIds[] = { trueId, trueId, trueId, trueId };
    const IlcSpvId falseConsistuentIds[] = { falseId, falseId, falseId, falseId };
    IlcSpvId trueCompositeId = ilcSpvPutConstantComposite(compiler->module, compiler->float4Id,
                                                          4, trueConsistuentIds);
    IlcSpvId falseCompositeId = ilcSpvPutConstantComposite(compiler->module, compiler->float4Id,
                                                           4, falseConsistuentIds);
    IlcSpvId resId = ilcSpvPutSelect(compiler->module, compiler->float4Id, condId,
                                     trueCompositeId, falseCompositeId);

    storeDestination(compiler, &instr->dsts[0], resId);
}

static void emitIntegerOp(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    IlcSpvId srcIds[MAX_SRC_COUNT] = { 0 };
    IlcSpvId srcTypeId = 0;
    IlcSpvId resId = 0;

    if (instr->opcode == IL_OP_U_DIV ||
        instr->opcode == IL_OP_U_MOD) {
        srcTypeId = compiler->uint4Id;
    } else {
        srcTypeId = compiler->int4Id;
    }

    for (int i = 0; i < instr->srcCount; i++) {
        srcIds[i] = loadSource(compiler, &instr->srcs[i], COMP_MASK_XYZW, srcTypeId);
    }

    switch (instr->opcode) {
    case IL_OP_I_NOT:
        resId = ilcSpvPutAlu(compiler->module, SpvOpNot, compiler->int4Id,
                             instr->srcCount, srcIds);
        break;
    case IL_OP_I_OR:
        resId = ilcSpvPutAlu(compiler->module, SpvOpBitwiseOr, compiler->int4Id,
                             instr->srcCount, srcIds);
        break;
    case IL_OP_I_ADD:
        resId = ilcSpvPutAlu(compiler->module, SpvOpIAdd, compiler->int4Id,
                             instr->srcCount, srcIds);
        break;
    case IL_OP_I_MAD: {
        IlcSpvId mulId = ilcSpvPutAlu(compiler->module, SpvOpIMul, compiler->int4Id, 2, srcIds);
        IlcSpvId addIds[] = { mulId, srcIds[2] };
        resId = ilcSpvPutAlu(compiler->module, SpvOpIAdd, compiler->int4Id, 2, addIds);
    } break;
    case IL_OP_I_MUL:
        resId = ilcSpvPutAlu(compiler->module, SpvOpIMul, compiler->int4Id,
                             instr->srcCount, srcIds);
        break;
    case IL_OP_I_NEGATE:
        resId = ilcSpvPutAlu(compiler->module, SpvOpSNegate, compiler->int4Id,
                             instr->srcCount, srcIds);
        break;
    case IL_OP_U_DIV:
        resId = ilcSpvPutAlu(compiler->module, SpvOpUDiv, compiler->uint4Id,
                             instr->srcCount, srcIds);
        break;
    case IL_OP_U_MOD:
        resId = ilcSpvPutAlu(compiler->module, SpvOpUMod, compiler->uint4Id,
                             instr->srcCount, srcIds);
        break;
    case IL_OP_AND:
        resId = ilcSpvPutAlu(compiler->module, SpvOpBitwiseAnd, compiler->int4Id,
                             instr->srcCount, srcIds);
        break;
    case IL_OP_I_SHL:
    case IL_OP_I_SHR:
    case IL_OP_U_SHR: {
        SpvOp op;
        if (instr->opcode == IL_OP_I_SHL) {
            op = SpvOpShiftLeftLogical;
        } else if (instr->opcode == IL_OP_I_SHR) {
            op = SpvOpShiftRightArithmetic;
        } else {
            op = SpvOpShiftRightLogical;
        }
        const IlcSpvId argIds[] = { srcIds[0], emitShiftMask(compiler, srcIds[1]) };
        resId = ilcSpvPutAlu(compiler->module, op, compiler->int4Id, 2, argIds);
    }   break;
    case IL_OP_U_BIT_EXTRACT: {
        IlcSpvId widthsId = emitShiftMask(compiler, srcIds[0]);
        IlcSpvId offsetsId = emitShiftMask(compiler, srcIds[1]);
        IlcSpvId bfId[4];
        for (unsigned i = 0; i < 4; i++) {
            // FIXME handle width + offset >= 32
            IlcSpvId widthId = emitVectorTrim(compiler, widthsId, compiler->int4Id, i, 1);
            IlcSpvId offsetId = emitVectorTrim(compiler, offsetsId, compiler->int4Id, i, 1);
            IlcSpvId baseId = emitVectorTrim(compiler, srcIds[2], compiler->int4Id, i, 1);
            const IlcSpvId argIds[] = { baseId, offsetId, widthId };
            bfId[i] = ilcSpvPutAlu(compiler->module, SpvOpBitFieldUExtract, compiler->intId,
                                   3, argIds);
        }
        resId = ilcSpvPutCompositeConstruct(compiler->module, compiler->int4Id, 4, bfId);
        break;
    }
    default:
        assert(false);
        break;
    }

    storeDestination(compiler, &instr->dsts[0],
                     ilcSpvPutBitcast(compiler->module, compiler->float4Id, resId));
}

static void emitIntegerComparisonOp(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    IlcSpvId srcIds[MAX_SRC_COUNT] = { 0 };
    SpvOp compOp = 0;

    for (int i = 0; i < instr->srcCount; i++) {
        srcIds[i] = loadSource(compiler, &instr->srcs[i], COMP_MASK_XYZW, compiler->int4Id);
    }

    switch (instr->opcode) {
    case IL_OP_I_EQ:
        compOp = SpvOpIEqual;
        break;
    case IL_OP_I_GE:
        compOp = SpvOpSGreaterThanEqual;
        break;
    case IL_OP_I_LT:
        compOp = SpvOpSLessThan;
        break;
    case IL_OP_I_NE:
        compOp = SpvOpINotEqual;
        break;
    case IL_OP_U_LT:
        compOp = SpvOpULessThan;
        break;
    case IL_OP_U_GE:
        compOp = SpvOpUGreaterThanEqual;
        break;
    default:
        assert(false);
        break;
    }

    IlcSpvId condId = ilcSpvPutAlu(compiler->module, compOp, compiler->bool4Id,
                                   instr->srcCount, srcIds);
    IlcSpvId trueId = ilcSpvPutConstant(compiler->module, compiler->floatId, TRUE_LITERAL);
    IlcSpvId falseId = ilcSpvPutConstant(compiler->module, compiler->floatId, FALSE_LITERAL);
    const IlcSpvId trueConsistuentIds[] = { trueId, trueId, trueId, trueId };
    const IlcSpvId falseConsistuentIds[] = { falseId, falseId, falseId, falseId };
    IlcSpvId trueCompositeId = ilcSpvPutConstantComposite(compiler->module, compiler->float4Id,
                                                          4, trueConsistuentIds);
    IlcSpvId falseCompositeId = ilcSpvPutConstantComposite(compiler->module, compiler->float4Id,
                                                           4, falseConsistuentIds);
    IlcSpvId resId = ilcSpvPutSelect(compiler->module, compiler->float4Id, condId,
                                     trueCompositeId, falseCompositeId);

    storeDestination(compiler, &instr->dsts[0], resId);
}

static void emitCmovLogical(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    IlcSpvId srcIds[MAX_SRC_COUNT] = { 0 };

    for (int i = 0; i < instr->srcCount; i++) {
        srcIds[i] = loadSource(compiler, &instr->srcs[i], COMP_MASK_XYZW, compiler->float4Id);
    }

    // For each component, select src1 if src0 has any bit set, otherwise select src2
    IlcSpvId falseId = ilcSpvPutConstant(compiler->module, compiler->intId, FALSE_LITERAL);
    const IlcSpvId falseConsistuentIds[] = { falseId, falseId, falseId, falseId };
    IlcSpvId falseCompositeId = ilcSpvPutConstantComposite(compiler->module, compiler->int4Id,
                                                           4, falseConsistuentIds);
    IlcSpvId castId = ilcSpvPutBitcast(compiler->module, compiler->int4Id, srcIds[0]);
    const IlcSpvId compIds[] = { castId, falseCompositeId };
    IlcSpvId condId = ilcSpvPutAlu(compiler->module, SpvOpINotEqual, compiler->bool4Id, 2, compIds);
    IlcSpvId resId = ilcSpvPutSelect(compiler->module, compiler->float4Id, condId,
                                     srcIds[1], srcIds[2]);

    storeDestination(compiler, &instr->dsts[0], resId);
}

static IlcSpvId emitConditionCheck(
    IlcCompiler* compiler,
    IlcSpvId srcId,
    bool notZero)
{
    IlcSpvId xId = emitVectorTrim(compiler, srcId, compiler->int4Id, COMP_INDEX_X, 1);
    IlcSpvId falseId = ilcSpvPutConstant(compiler->module, compiler->intId, FALSE_LITERAL);
    const IlcSpvId compIds[] = { xId, falseId };
    return ilcSpvPutAlu(compiler->module, notZero ? SpvOpINotEqual : SpvOpIEqual, compiler->boolId,
                        2, compIds);
}

static void emitIf(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    const IlcIfElseBlock ifElseBlock = {
        .labelElseId = ilcSpvAllocId(compiler->module),
        .labelEndId = ilcSpvAllocId(compiler->module),
        .hasElseBlock = false,
    };

    IlcSpvId srcId = loadSource(compiler, &instr->srcs[0], COMP_MASK_XYZW, compiler->int4Id);
    IlcSpvId labelBeginId = ilcSpvAllocId(compiler->module);
    IlcSpvId condId = emitConditionCheck(compiler, srcId, instr->opcode == IL_OP_IF_LOGICALNZ);
    ilcSpvPutSelectionMerge(compiler->module, ifElseBlock.labelEndId);
    ilcSpvPutBranchConditional(compiler->module, condId, labelBeginId, ifElseBlock.labelElseId);
    ilcSpvPutLabel(compiler->module, labelBeginId);

    const IlcControlFlowBlock block = {
        .type = BLOCK_IF_ELSE,
        .ifElse = ifElseBlock,
    };
    pushControlFlowBlock(compiler, &block);
}

static void emitElse(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    IlcControlFlowBlock block = popControlFlowBlock(compiler);
    if (block.type != BLOCK_IF_ELSE) {
        LOGE("no matching if/else block was found\n");
        assert(false);
    }

    ilcSpvPutBranch(compiler->module, block.ifElse.labelEndId);
    ilcSpvPutLabel(compiler->module, block.ifElse.labelElseId);
    block.ifElse.hasElseBlock = true;

    pushControlFlowBlock(compiler, &block);
}

static void emitWhile(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    const IlcLoopBlock loopBlock = {
        .labelHeaderId = ilcSpvAllocId(compiler->module),
        .labelContinueId = ilcSpvAllocId(compiler->module),
        .labelBreakId = ilcSpvAllocId(compiler->module),
    };

    ilcSpvPutBranch(compiler->module, loopBlock.labelHeaderId);
    ilcSpvPutLabel(compiler->module, loopBlock.labelHeaderId);

    ilcSpvPutLoopMerge(compiler->module, loopBlock.labelBreakId, loopBlock.labelContinueId);

    IlcSpvId labelBeginId = ilcSpvAllocId(compiler->module);
    ilcSpvPutBranch(compiler->module, labelBeginId);
    ilcSpvPutLabel(compiler->module, labelBeginId);

    const IlcControlFlowBlock block = {
        .type = BLOCK_LOOP,
        .loop = loopBlock,
    };
    pushControlFlowBlock(compiler, &block);
}

static void emitEndIf(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    const IlcControlFlowBlock block = popControlFlowBlock(compiler);
    if (block.type != BLOCK_IF_ELSE) {
        LOGE("no matching if/else block was found\n");
        assert(false);
    }

    if (compiler->isAfterReturn) {
        // Declare a new block
        ilcSpvPutLabel(compiler->module, ilcSpvAllocId(compiler->module));
        compiler->isAfterReturn = false;
    }

    if (!block.ifElse.hasElseBlock) {
        // If no else block was declared, insert a dummy one
        ilcSpvPutBranch(compiler->module, block.ifElse.labelEndId);
        ilcSpvPutLabel(compiler->module, block.ifElse.labelElseId);
    }

    ilcSpvPutBranch(compiler->module, block.ifElse.labelEndId);
    ilcSpvPutLabel(compiler->module, block.ifElse.labelEndId);
}

static void emitEndLoop(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    const IlcControlFlowBlock block = popControlFlowBlock(compiler);
    if (block.type != BLOCK_LOOP) {
        LOGE("no matching loop block was found\n");
        assert(false);
    }

    ilcSpvPutBranch(compiler->module, block.loop.labelContinueId);
    ilcSpvPutLabel(compiler->module, block.loop.labelContinueId);

    ilcSpvPutBranch(compiler->module, block.loop.labelHeaderId);
    ilcSpvPutLabel(compiler->module, block.loop.labelBreakId);
}

static void emitBreak(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    const IlcControlFlowBlock* block = findControlFlowBlock(compiler, BLOCK_LOOP);
    if (block == NULL) {
        LOGE("no matching loop block was found\n");
        assert(false);
    }

    IlcSpvId labelId = ilcSpvAllocId(compiler->module);

    if (instr->opcode == IL_OP_BREAK) {
        ilcSpvPutBranch(compiler->module, block->loop.labelBreakId);
    } else if (instr->opcode == IL_OP_BREAK_LOGICALZ ||
               instr->opcode == IL_OP_BREAK_LOGICALNZ) {
        IlcSpvId srcId = loadSource(compiler, &instr->srcs[0], COMP_MASK_XYZW, compiler->int4Id);
        IlcSpvId condId = emitConditionCheck(compiler, srcId,
                                             instr->opcode == IL_OP_BREAK_LOGICALNZ);
        ilcSpvPutBranchConditional(compiler->module, condId, block->loop.labelBreakId, labelId);
    } else {
        assert(false);
    }

    ilcSpvPutLabel(compiler->module, labelId);
}

static void emitContinue(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    const IlcControlFlowBlock* block = findControlFlowBlock(compiler, BLOCK_LOOP);
    if (block == NULL) {
        LOGE("no matching loop block was found\n");
        assert(false);
    }

    IlcSpvId labelId = ilcSpvAllocId(compiler->module);
    ilcSpvPutBranch(compiler->module, block->loop.labelContinueId);
    ilcSpvPutLabel(compiler->module, labelId);
}

static void emitDiscard(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    IlcSpvId labelBeginId = ilcSpvAllocId(compiler->module);
    IlcSpvId labelEndId = ilcSpvAllocId(compiler->module);

    IlcSpvId srcId = loadSource(compiler, &instr->srcs[0], COMP_MASK_XYZW, compiler->int4Id);
    IlcSpvId condId = emitConditionCheck(compiler, srcId, instr->opcode == IL_OP_DISCARD_LOGICALNZ);
    ilcSpvPutSelectionMerge(compiler->module, labelEndId);
    ilcSpvPutBranchConditional(compiler->module, condId, labelBeginId, labelEndId);
    ilcSpvPutLabel(compiler->module, labelBeginId);

    ilcSpvPutCapability(compiler->module, SpvCapabilityDemoteToHelperInvocationEXT);
    ilcSpvPutDemoteToHelperInvocation(compiler->module); // Direct3D discard

    ilcSpvPutBranch(compiler->module, labelEndId);
    ilcSpvPutLabel(compiler->module, labelEndId);
}

static void emitLoad(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint8_t ilResourceId = GET_BITS(instr->control, 0, 7);

    const IlcResource* resource = findResource(compiler, ilResourceId);
    const Destination* dst = &instr->dsts[0];
    const IlcRegister* dstReg = findOrCreateRegister(compiler, dst->registerType, dst->registerNum);

    if (resource == NULL) {
        LOGE("resource %d not found\n", ilResourceId);
        return;
    } else if (dstReg == NULL) {
        LOGE("destination register %d %d not found\n", dst->registerType, dst->registerNum);
        return;
    }

    IlcSpvId srcId = loadSource(compiler, &instr->srcs[0], COMP_MASK_XYZW, compiler->int4Id);
    IlcSpvId resourceId = ilcSpvPutLoad(compiler->module, resource->typeId, resource->id);
    IlcSpvId fetchId = ilcSpvPutImageFetch(compiler->module, dstReg->typeId, resourceId, srcId);
    storeDestination(compiler, dst, fetchId);
}

static void emitResinfo(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint8_t ilResourceId = GET_BITS(instr->control, 0, 7);
    bool ilReturnType = GET_BIT(instr->control, 8);

    const IlcResource* resource = findResource(compiler, ilResourceId);
    const Destination* dst = &instr->dsts[0];

    if (resource == NULL) {
        LOGE("resource %d not found\n", ilResourceId);
        return;
    }

    unsigned dimCount = getResourceDimensionCount(resource->ilType);

    IlcSpvId vecTypeId = dimCount == 1 ? compiler->intId :
                         ilcSpvPutVectorType(compiler->module, compiler->intId, dimCount);
    IlcSpvId resourceId = ilcSpvPutLoad(compiler->module, resource->typeId, resource->id);
    IlcSpvId srcId = loadSource(compiler, &instr->srcs[0], COMP_MASK_XYZW, compiler->int4Id);
    IlcSpvId lodId = emitVectorTrim(compiler, srcId, compiler->int4Id, COMP_INDEX_X, 1);
    ilcSpvPutCapability(compiler->module, SpvCapabilityImageQuery);
    IlcSpvId sizesId = ilcSpvPutImageQuerySizeLod(compiler->module, vecTypeId, resourceId,
                                                 lodId);
    IlcSpvId levelsId = ilcSpvPutImageQueryLevels(compiler->module, compiler->intId, resourceId);

    // Put everything together: zero-padded sizes (index 0 to 2) and mipmap count (index 3)
    // TODO handle out of bounds LOD
    IlcSpvId zeroId = ilcSpvPutConstant(compiler->module, compiler->intId, ZERO_LITERAL);
    const IlcSpvWord constituents[] = { zeroId, zeroId, zeroId, levelsId };
    IlcSpvId zeroLevelsId = ilcSpvPutCompositeConstruct(compiler->module, compiler->int4Id,
                                                        4, constituents);
    const IlcSpvWord components[] = {
        dimCount >= 1 ? 4 : 0, // Width
        dimCount >= 2 ? 5 : 1, // Height
        dimCount >= 3 ? 6 : 2, // Depth/slices
        3, // Mip count
    };
    IlcSpvId infoId = ilcSpvPutVectorShuffle(compiler->module, compiler->int4Id,
                                             zeroLevelsId, sizesId, 4, components);
    IlcSpvId resId = ilReturnType ? ilcSpvPutBitcast(compiler->module, compiler->float4Id, infoId)
                                  : ilcSpvPutAlu(compiler->module, SpvOpConvertSToF,
                                                 compiler->float4Id, 1, &infoId);
    storeDestination(compiler, dst, resId);
}

static void emitSample(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint8_t ilResourceId = GET_BITS(instr->control, 0, 7);
    uint8_t ilSamplerId = GET_BITS(instr->control, 8, 11);
    // TODO handle indexed args and aoffimmi

    const IlcResource* resource = findResource(compiler, ilResourceId);
    const IlcSampler* sampler = findOrCreateSampler(compiler, ilSamplerId);
    const Destination* dst = &instr->dsts[0];
    const IlcRegister* dstReg = findOrCreateRegister(compiler, dst->registerType, dst->registerNum);

    if (resource == NULL) {
        LOGE("resource %d not found\n", ilResourceId);
        return;
    } else if (dstReg == NULL) {
        LOGE("destination register %d %d not found\n", dst->registerType, dst->registerNum);
        return;
    }

    unsigned dimCount = getResourceDimensionCount(resource->ilType);
    IlcSpvWord sampleOp;
    IlcSpvId coordinateId = loadSource(compiler, &instr->srcs[0], COMP_MASK_XYZW,
                                       compiler->float4Id);
    SpvImageOperandsMask operandsMask = 0;
    unsigned operandIdCount = 0;
    IlcSpvId operandIds[2];

    if (instr->opcode == IL_OP_SAMPLE) {
        sampleOp = SpvOpImageSampleImplicitLod;
    } else if (instr->opcode == IL_OP_SAMPLE_B) {
        sampleOp = SpvOpImageSampleImplicitLod;
        operandsMask |= SpvImageOperandsBiasMask;

        IlcSpvId biasId = loadSource(compiler, &instr->srcs[1], COMP_MASK_XYZW, compiler->float4Id);
        operandIds[0] = emitVectorTrim(compiler, biasId, compiler->float4Id, COMP_INDEX_X, 1);
        operandIdCount++;
    } else if (instr->opcode == IL_OP_SAMPLE_G) {
        sampleOp = SpvOpImageSampleExplicitLod;
        operandsMask |= SpvImageOperandsGradMask;

        IlcSpvId xGradId = loadSource(compiler, &instr->srcs[1], COMP_MASK_XYZW,
                                      compiler->float4Id);
        operandIds[0] = emitVectorTrim(compiler, xGradId, compiler->float4Id,
                                       COMP_INDEX_X, dimCount);
        IlcSpvId yGradId = loadSource(compiler, &instr->srcs[2], COMP_MASK_XYZW,
                                      compiler->float4Id);
        operandIds[1] = emitVectorTrim(compiler, yGradId, compiler->float4Id,
                                       COMP_INDEX_X, dimCount);
        operandIdCount += 2;
    } else if (instr->opcode == IL_OP_SAMPLE_L) {
        sampleOp = SpvOpImageSampleExplicitLod;
        operandsMask |= SpvImageOperandsLodMask;

        IlcSpvId lodId = loadSource(compiler, &instr->srcs[1], COMP_MASK_XYZW, compiler->float4Id);
        operandIds[0] = emitVectorTrim(compiler, lodId, compiler->float4Id, COMP_INDEX_X, 1);
        operandIdCount++;
    } else {
        assert(false);
    }

    IlcSpvId resourceId = ilcSpvPutLoad(compiler->module, resource->typeId, resource->id);
    IlcSpvId samplerTypeId = ilcSpvPutSamplerType(compiler->module);
    IlcSpvId samplerId = ilcSpvPutLoad(compiler->module, samplerTypeId, sampler->id);
    IlcSpvId sampledImageTypeId = ilcSpvPutSampledImageType(compiler->module, resource->typeId);
    IlcSpvId sampledImageId = ilcSpvPutSampledImage(compiler->module, sampledImageTypeId,
                                                    resourceId, samplerId);
    IlcSpvId sampleId = ilcSpvPutImageSample(compiler->module, sampleOp, dstReg->typeId,
                                             sampledImageId, coordinateId, operandsMask,
                                             operandIdCount, operandIds);
    storeDestination(compiler, dst, sampleId);
}

static void emitStructuredSrvLoad(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint8_t ilResourceId = GET_BITS(instr->control, 0, 7);
    bool indexedResourceId = GET_BIT(instr->control, 12);

    if (indexedResourceId) {
        LOGW("unhandled indexed resource ID\n");
    }

    const IlcResource* resource = findResource(compiler, ilResourceId);
    const Destination* dst = &instr->dsts[0];
    const IlcRegister* dstReg = findOrCreateRegister(compiler, dst->registerType, dst->registerNum);

    if (resource == NULL) {
        LOGE("resource %d not found\n", ilResourceId);
        return;
    } else if (dstReg == NULL) {
        LOGE("destination register %d %d not found\n", dst->registerType, dst->registerNum);
        return;
    }

    IlcSpvId srcId = loadSource(compiler, &instr->srcs[0], COMP_MASK_XYZW, compiler->int4Id);
    IlcSpvId indexId = emitVectorTrim(compiler, srcId, compiler->int4Id, COMP_INDEX_X, 1);
    IlcSpvId offsetId = emitVectorTrim(compiler, srcId, compiler->int4Id, COMP_INDEX_Y, 1);

    // addr = (index * stride + offset) / 4
    const IlcSpvId mulIds[] = { indexId, resource->strideId };
    IlcSpvId baseId = ilcSpvPutAlu(compiler->module, SpvOpIMul, compiler->intId, 2, mulIds);
    const IlcSpvId addIds[] = { baseId, offsetId };
    IlcSpvId byteAddrId = ilcSpvPutAlu(compiler->module, SpvOpIAdd, compiler->intId, 2, addIds);
    const IlcSpvId divIds[] = {
        byteAddrId, ilcSpvPutConstant(compiler->module, compiler->intId, 4)
    };
    IlcSpvId wordAddrId = ilcSpvPutAlu(compiler->module, SpvOpSDiv, compiler->intId, 2, divIds);

    IlcSpvId resourceId = ilcSpvPutLoad(compiler->module, resource->typeId, resource->id);
    IlcSpvId fetchId = ilcSpvPutImageFetch(compiler->module, compiler->int4Id, resourceId,
                                           wordAddrId);
    storeDestination(compiler, dst,
                     ilcSpvPutBitcast(compiler->module, compiler->float4Id, fetchId));
}

static void emitInstr(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    switch (instr->opcode) {
    case IL_OP_ABS:
    case IL_OP_ACOS:
    case IL_OP_ADD:
    case IL_OP_ASIN:
    case IL_OP_ATAN:
    case IL_OP_DIV:
    case IL_OP_DP3:
    case IL_OP_DP4:
    case IL_OP_DSX:
    case IL_OP_DSY:
    case IL_OP_FRC:
    case IL_OP_MAD:
    case IL_OP_MAX:
    case IL_OP_MIN:
    case IL_OP_MOV:
    case IL_OP_MUL:
    case IL_OP_FTOI:
    case IL_OP_FTOU:
    case IL_OP_ITOF:
    case IL_OP_UTOF:
    case IL_OP_ROUND_NEG_INF:
    case IL_OP_ROUND_PLUS_INF:
    case IL_OP_EXP_VEC:
    case IL_OP_LOG_VEC:
    case IL_OP_RSQ_VEC:
    case IL_OP_SIN_VEC:
    case IL_OP_COS_VEC:
    case IL_OP_SQRT_VEC:
    case IL_OP_DP2:
        emitFloatOp(compiler, instr);
        break;
    case IL_OP_EQ:
    case IL_OP_GE:
    case IL_OP_LT:
    case IL_OP_NE:
        emitFloatComparisonOp(compiler, instr);
        break;
    case IL_OP_I_NOT:
    case IL_OP_I_OR:
    case IL_OP_I_ADD:
    case IL_OP_I_MAD:
    case IL_OP_I_MUL:
    case IL_OP_I_NEGATE:
    case IL_OP_I_SHL:
    case IL_OP_I_SHR:
    case IL_OP_U_SHR:
    case IL_OP_U_DIV:
    case IL_OP_U_MOD:
    case IL_OP_AND:
    case IL_OP_U_BIT_EXTRACT:
        emitIntegerOp(compiler, instr);
        break;
    case IL_OP_I_EQ:
    case IL_OP_I_GE:
    case IL_OP_I_LT:
    case IL_OP_I_NE:
    case IL_OP_U_LT:
    case IL_OP_U_GE:
        emitIntegerComparisonOp(compiler, instr);
        break;
    case IL_OP_CONTINUE:
        emitContinue(compiler, instr);
        break;
    case IL_OP_ELSE:
        emitElse(compiler, instr);
        break;
    case IL_OP_END:
    case IL_OP_ENDMAIN:
        if (compiler->isInFunction) {
            ilcSpvPutFunctionEnd(compiler->module);
            compiler->isInFunction = false;
        }
        break;
    case IL_OP_ENDIF:
        emitEndIf(compiler, instr);
        break;
    case IL_OP_ENDLOOP:
        emitEndLoop(compiler, instr);
        break;
    case IL_OP_BREAK:
    case IL_OP_BREAK_LOGICALZ:
    case IL_OP_BREAK_LOGICALNZ:
        emitBreak(compiler, instr);
        break;
    case IL_OP_IF_LOGICALZ:
    case IL_OP_IF_LOGICALNZ:
        emitIf(compiler, instr);
        break;
    case IL_OP_WHILE:
        emitWhile(compiler, instr);
        break;
    case IL_OP_RET_DYN:
        ilcSpvPutReturn(compiler->module);
        compiler->isAfterReturn = true;
        break;
    case IL_DCL_INDEXED_TEMP_ARRAY:
        emitIndexedTempArray(compiler, instr);
        break;
    case IL_DCL_LITERAL:
        emitLiteral(compiler, instr);
        break;
    case IL_DCL_OUTPUT:
        emitOutput(compiler, instr);
        break;
    case IL_DCL_INPUT:
        emitInput(compiler, instr);
        break;
    case IL_DCL_RESOURCE:
        emitResource(compiler, instr);
        break;
    case IL_OP_DISCARD_LOGICALZ:
    case IL_OP_DISCARD_LOGICALNZ:
        emitDiscard(compiler, instr);
        break;
    case IL_OP_LOAD:
        emitLoad(compiler, instr);
        break;
    case IL_OP_RESINFO:
        emitResinfo(compiler, instr);
        break;
    case IL_OP_SAMPLE:
    case IL_OP_SAMPLE_B:
    case IL_OP_SAMPLE_G:
    case IL_OP_SAMPLE_L:
        emitSample(compiler, instr);
        break;
    case IL_OP_CMOV_LOGICAL:
        emitCmovLogical(compiler, instr);
        break;
    case IL_OP_DCL_STRUCT_SRV:
        emitStructuredSrv(compiler, instr);
        break;
    case IL_OP_SRV_STRUCT_LOAD:
        emitStructuredSrvLoad(compiler, instr);
        break;
    case IL_DCL_GLOBAL_FLAGS:
        emitGlobalFlags(compiler, instr);
        break;
    case IL_UNK_660:
        // FIXME seems to be a no-op
        break;
    default:
        LOGW("unhandled instruction %d\n", instr->opcode);
    }
}

static void emitEntryPoint(
    IlcCompiler* compiler)
{
    IlcSpvWord execution = 0;
    const char* name = "main";

    switch (compiler->kernel->shaderType) {
    case IL_SHADER_VERTEX:
        execution = SpvExecutionModelVertex;
        break;
    case IL_SHADER_PIXEL:
        execution = SpvExecutionModelFragment;
        break;
    case IL_SHADER_GEOMETRY:
        execution = SpvExecutionModelGeometry;
        break;
    case IL_SHADER_COMPUTE:
        execution = SpvExecutionModelGLCompute;
        break;
    case IL_SHADER_HULL:
        execution = SpvExecutionModelTessellationControl;
        break;
    case IL_SHADER_DOMAIN:
        execution = SpvExecutionModelTessellationEvaluation;
        break;
    }

    unsigned interfaceCount = compiler->regCount + compiler->resourceCount + compiler->samplerCount;
    IlcSpvWord* interfaces = malloc(sizeof(IlcSpvWord) * interfaceCount);
    unsigned interfaceIndex = 0;
    for (int i = 0; i < compiler->regCount; i++) {
        const IlcRegister* reg = &compiler->regs[i];

        interfaces[interfaceIndex] = reg->id;
        interfaceIndex++;
    }
    for (int i = 0; i < compiler->resourceCount; i++) {
        const IlcResource* resource = &compiler->resources[i];

        interfaces[interfaceIndex] = resource->id;
        interfaceIndex++;
    }
    for (int i = 0; i < compiler->samplerCount; i++) {
        const IlcSampler* sampler = &compiler->samplers[i];

        interfaces[interfaceIndex] = sampler->id;
        interfaceIndex++;
    }

    ilcSpvPutEntryPoint(compiler->module, compiler->entryPointId, execution, name,
                        interfaceCount, interfaces);
    ilcSpvPutName(compiler->module, compiler->entryPointId, name);

    switch (compiler->kernel->shaderType) {
    case IL_SHADER_PIXEL:
        ilcSpvPutExtension(compiler->module, "SPV_EXT_demote_to_helper_invocation");
        ilcSpvPutExecMode(compiler->module, compiler->entryPointId,
                          SpvExecutionModeOriginUpperLeft);
        break;
    }

    free(interfaces);
}

uint32_t* ilcCompileKernel(
    unsigned* size,
    const Kernel* kernel)
{
    IlcSpvModule module;

    ilcSpvInit(&module);

    IlcSpvId uintId = ilcSpvPutIntType(&module, false);
    IlcSpvId intId = ilcSpvPutIntType(&module, true);
    IlcSpvId floatId = ilcSpvPutFloatType(&module);
    IlcSpvId boolId = ilcSpvPutBoolType(&module);

    IlcCompiler compiler = {
        .module = &module,
        .kernel = kernel,
        .entryPointId = ilcSpvAllocId(&module),
        .uintId = uintId,
        .uint4Id = ilcSpvPutVectorType(&module, uintId, 4),
        .intId = intId,
        .int4Id = ilcSpvPutVectorType(&module, intId, 4),
        .floatId = floatId,
        .float4Id = ilcSpvPutVectorType(&module, floatId, 4),
        .boolId = boolId,
        .bool4Id = ilcSpvPutVectorType(&module, boolId, 4),
        .regCount = 0,
        .regs = NULL,
        .resourceCount = 0,
        .resources = NULL,
        .samplerCount = 0,
        .samplers = NULL,
        .controlFlowBlockCount = 0,
        .controlFlowBlocks = NULL,
        .isInFunction = true,
        .isAfterReturn = false,
    };

    emitFunc(&compiler, compiler.entryPointId);
    for (int i = 0; i < kernel->instrCount; i++) {
        emitInstr(&compiler, &kernel->instrs[i]);
    }

    emitEntryPoint(&compiler);

    free(compiler.regs);
    free(compiler.resources);
    free(compiler.controlFlowBlocks);
    ilcSpvFinish(&module);

    *size = sizeof(IlcSpvWord) * module.buffer[ID_MAIN].wordCount;
    return module.buffer[ID_MAIN].words;
}
