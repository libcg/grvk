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
#define NO_STRIDE_INDEX     (-1)

#define V_ARRAY_REG_ID 0xFEFFFFFF

typedef enum {
    RES_TYPE_GENERIC,
    RES_TYPE_LDS,
    RES_TYPE_ATOMIC_COUNTER,
    RES_TYPE_PUSH_CONSTANTS,
} IlcResourceType;

typedef enum {
    BLOCK_IF_ELSE,
    BLOCK_LOOP,
} IlcControlFlowBlockType;

typedef enum {
    CONTROL_POINT,
    FORK,
    JOIN
} IlcHullPhase;

typedef struct {
    IlcSpvId id;
    IlcSpvId typeId;
    IlcSpvId vecTypeId;
    IlcSpvId componentTypeId;
    unsigned componentCount;
    unsigned arrayItemCount;
    unsigned arrayItemOffset;//for use with arrayed registers like InnerTessLevel, which has offsets
    bool duped;
    uint32_t ilType; // ILRegType
    uint32_t ilNum;
    uint8_t ilImportUsage; // Input/output only
    uint8_t ilInterpMode; // Input only
} IlcRegister;

typedef struct {
    IlcResourceType resType;
    IlcSpvId id;
    IlcSpvId typeId;
    IlcSpvId texelTypeId;
    uint32_t ilId;
    uint8_t ilType;
    IlcSpvId strideId;
    bool structured;
} IlcResource;

typedef struct {
    IlcSpvId id;
    uint32_t ilId;
} IlcSampler;

typedef struct {
    IlcSpvId labelBeginId;
    IlcSpvId labelElseId;
    IlcSpvId labelEndId;
    bool hasElseBlock;
} IlcIfElseBlock;

typedef struct {
    IlcSpvId labelBeginId;
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
    IlcHullPhase type;
    unsigned phaseId;
    IlcSpvId functionId;
    IlcSpvId invocationId;
    unsigned invocationCount;
} IlcHullPhaseBlock;

typedef struct {
    const Kernel* kernel;
    IlcSpvModule* module;
    unsigned bindingCount;
    IlcBinding* bindings;
    unsigned atomicUavCount;
    uint16_t* atomicUavs;
    unsigned inputCount;
    IlcInput* inputs;
    unsigned outputCount;
    uint32_t* outputLocations;
    bool emitHullOutputFinalize;
    IlcSpvId entryPointId;
    IlcSpvId stageFunctionId;
    IlcSpvId uintId;
    IlcSpvId uint4Id;
    IlcSpvId intId;
    IlcSpvId int4Id;
    IlcSpvId floatId;
    IlcSpvId float4Id;
    IlcSpvId boolId;
    IlcSpvId bool4Id;
    unsigned currentStrideIndex;
    unsigned inputArraySize;
    IlcSpvWord inputControlPointSize;
    unsigned regCount;
    IlcRegister* regs;
    unsigned resourceCount;
    IlcResource* resources;
    unsigned samplerCount;
    IlcSampler* samplers;
    unsigned controlFlowBlockCount;
    IlcControlFlowBlock* controlFlowBlocks;
    IlcSpvId currentFunctionLabelId;
    unsigned forkPhaseCount;
    IlcHullPhaseBlock* forkPhases;
    unsigned joinPhaseCount;
    IlcHullPhaseBlock* joinPhases;
    IlcHullPhaseBlock controlPointPhase;
    struct {
        IlcHullPhase type;
        unsigned phaseId;
    } currentHullPhase;
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
    case IL_USAGE_PIXTEX_2DMSAA:
    case IL_USAGE_PIXTEX_1DARRAY:
        return 2;
    case IL_USAGE_PIXTEX_3D:
    case IL_USAGE_PIXTEX_CUBEMAP:
    case IL_USAGE_PIXTEX_2DARRAY:
        return 3;
    default:
        LOGE("unhandled resource type %d\n", ilType);
        assert(false);
    }

    return 0;
}

static SpvDim getSpvDimension(
    IlcCompiler* compiler,
    uint8_t ilType,
    bool isSampled)
{
    switch (ilType) {
    case IL_USAGE_PIXTEX_1D:
    case IL_USAGE_PIXTEX_1DARRAY:
        ilcSpvPutCapability(compiler->module,
                            isSampled ? SpvCapabilitySampled1D : SpvCapabilityImage1D);
        return SpvDim1D;
    case IL_USAGE_PIXTEX_2D:
    case IL_USAGE_PIXTEX_2DMSAA:
    case IL_USAGE_PIXTEX_2DARRAY:
        return SpvDim2D;
    case IL_USAGE_PIXTEX_3D:
        return SpvDim3D;
    case IL_USAGE_PIXTEX_CUBEMAP:
        return SpvDimCube;
    case IL_USAGE_PIXTEX_BUFFER:
        ilcSpvPutCapability(compiler->module,
                            isSampled ? SpvCapabilitySampledBuffer : SpvCapabilityImageBuffer);
        return SpvDimBuffer;
    default:
        LOGE("unhandled resource type %d\n", ilType);
        assert(false);
    }

    return 0;
}

static bool isArrayed(
    uint8_t ilType)
{
    return ilType == IL_USAGE_PIXTEX_1DARRAY ||
           ilType == IL_USAGE_PIXTEX_2DARRAY ||
           ilType == IL_USAGE_PIXTEX_2DARRAYMSAA;
}

static bool isMultisampled(
    uint8_t ilType)
{
    return ilType == IL_USAGE_PIXTEX_2DMSAA ||
           ilType == IL_USAGE_PIXTEX_2DARRAYMSAA;
}

static IlcSpvId emitVariable(
    IlcCompiler* compiler,
    IlcSpvId typeId,
    IlcSpvWord storageClass)
{
    IlcSpvId pointerId = ilcSpvPutPointerType(compiler->module, storageClass, typeId);

    return ilcSpvPutVariable(compiler->module, pointerId, storageClass);
}

static IlcSpvId emitZeroOneVector(
    IlcCompiler* compiler,
    IlcSpvId componentTypeId)
{
    IlcSpvId vec2Id = ilcSpvPutVectorType(compiler->module, componentTypeId, 2);

    const IlcSpvId consistuentIds[] = {
        ilcSpvPutConstant(compiler->module, componentTypeId, ZERO_LITERAL),
        ilcSpvPutConstant(compiler->module, componentTypeId, ONE_LITERAL),
    };
    return ilcSpvPutConstantComposite(compiler->module, vec2Id, 2, consistuentIds);
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
    return ilcSpvPutOp2(compiler->module, SpvOpBitwiseAnd, compiler->int4Id,
                        srcId, maskCompositeId);
}

static IlcSpvId emitVectorTrim(
    IlcCompiler* compiler,
    IlcSpvId vecId,
    IlcSpvId typeId,
    unsigned offset,
    unsigned count)
{
    assert(1 <= (offset + count) && (offset + count) <= 4);

    IlcSpvId baseTypeId = 0;
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

static IlcSpvId emitVectorGrow(
    IlcCompiler* compiler,
    IlcSpvId vecId,
    IlcSpvId componentTypeId,
    unsigned componentCount)
{
    assert(1 <= componentCount && componentCount <= 3);

    IlcSpvId vec4TypeId = ilcSpvPutVectorType(compiler->module, componentTypeId, 4);

    if (componentCount == 1) {
        // Convert scalar to vec4
        const IlcSpvWord constituents[] = { vecId, vecId, vecId, vecId };
        return ilcSpvPutCompositeConstruct(compiler->module, vec4TypeId, 4, constituents);
    } else {
        // Grow vector to vec4
        const IlcSpvWord components[] = {
            componentCount >= 1 ? 0 : 0,
            componentCount >= 2 ? 1 : 0,
            componentCount >= 3 ? 2 : 0,
            componentCount >= 4 ? 3 : 0,
        };
        return ilcSpvPutVectorShuffle(compiler->module, vec4TypeId, vecId, vecId, 4, components);
    }
}

static void emitBinding(
    IlcCompiler* compiler,
    IlcSpvId bindingId,
    IlcSpvWord ilId,
    VkDescriptorType vkDescriptorType,
    int strideIndex)
{
    unsigned descriptorSetIdx = 0;

    switch (compiler->kernel->shaderType) {
    case IL_SHADER_VERTEX:
    case IL_SHADER_COMPUTE:
        descriptorSetIdx = 0;
        break;
    case IL_SHADER_HULL:
        descriptorSetIdx = 1;
        break;
    case IL_SHADER_DOMAIN:
        descriptorSetIdx = 2;
        break;
    case IL_SHADER_GEOMETRY:
        descriptorSetIdx = 3;
        break;
    case IL_SHADER_PIXEL:
        descriptorSetIdx = 4;
        break;
    default:
        assert(0);
    }

    ilcSpvPutDecoration(compiler->module, bindingId, SpvDecorationDescriptorSet,
                        1, &descriptorSetIdx);
    ilcSpvPutDecoration(compiler->module, bindingId, SpvDecorationBinding, 1, &ilId);

    compiler->bindingCount++;
    compiler->bindings = realloc(compiler->bindings, compiler->bindingCount * sizeof(IlcBinding));
    compiler->bindings[compiler->bindingCount - 1] = (IlcBinding) {
        .index = ilId,
        .descriptorType = vkDescriptorType,
        .strideIndex = strideIndex,
    };
}

static const IlcRegister* addRegister(
    IlcCompiler* compiler,
    const IlcRegister* reg,
    const char* identifier)
{
    if (!reg->duped) {
        char name[32];
        snprintf(name, sizeof(name), "%s%u", identifier, reg->ilNum);
        ilcSpvPutName(compiler->module, reg->id, name);
    }
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

static const IlcRegister* findRegisterByType(
    IlcCompiler* compiler,
    uint32_t type,
    uint32_t importUsage)
{
    for (int i = 0; i < compiler->regCount; i++) {
        const IlcRegister* reg = &compiler->regs[i];

        if (reg->ilType == type && reg->ilImportUsage == importUsage) {
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
        IlcSpvId tempTypeId = compiler->float4Id;

        IlcSpvId pointerId = ilcSpvPutPointerType(compiler->module, SpvStorageClassPrivate, tempTypeId);
        IlcSpvId zeroId = ilcSpvPutConstant(compiler->module, compiler->floatId, ZERO_LITERAL);
        IlcSpvId consistuentIds[] = { zeroId, zeroId, zeroId, zeroId };
        IlcSpvId zeroVecId = ilcSpvPutConstantComposite(compiler->module, tempTypeId,
                                                          4, consistuentIds);
        IlcSpvId tempId = ilcSpvPutVariableWithInitializer(compiler->module, pointerId, SpvStorageClassPrivate, zeroVecId);
        //emitVariable(compiler, tempTypeId, SpvStorageClassPrivate);

        const IlcRegister tempReg = {
            .id = tempId,
            .typeId = tempTypeId,
            .vecTypeId = tempTypeId,
            .componentTypeId = compiler->floatId,
            .componentCount = 4,
            .arrayItemCount = 0,
            .ilType = type,
            .ilNum = num,
            .ilImportUsage = 0,
            .ilInterpMode = 0,
        };

        reg = addRegister(compiler, &tempReg, "r");
    }

    return reg;
}

static const IlcResource* findResource(
    IlcCompiler* compiler,
    IlcResourceType resType,
    uint32_t ilId)
{
    for (int i = 0; i < compiler->resourceCount; i++) {
        const IlcResource* resource = &compiler->resources[i];

        if (resource->resType == resType && resource->ilId == ilId) {
            return resource;
        }
    }

    return NULL;
}

static const IlcResource* addResource(
    IlcCompiler* compiler,
    const IlcResource* resource)
{
    if (findResource(compiler, resource->resType, resource->ilId) != NULL) {
        LOGE("resource%u.%u already present\n", resource->resType, resource->ilId);
        assert(false);
    }

    char name[32];
    snprintf(name, sizeof(name), "resource%u.%u", resource->resType, resource->ilId);
    ilcSpvPutName(compiler->module, resource->id, name);

    compiler->resourceCount++;
    compiler->resources = realloc(compiler->resources,
                                  sizeof(IlcResource) * compiler->resourceCount);
    compiler->resources[compiler->resourceCount - 1] = *resource;

    return &compiler->resources[compiler->resourceCount - 1];
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
        LOGE("sampler%u already present\n", sampler->ilId);
        assert(false);
    }

    char name[32];
    snprintf(name, sizeof(name), "sampler%u", sampler->ilId);
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

        emitBinding(compiler, samplerId, ILC_BASE_SAMPLER_ID + ilId, VK_DESCRIPTOR_TYPE_SAMPLER,
                    NO_STRIDE_INDEX);

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

static IlcSpvId getTopBlockLabel(
    const IlcCompiler* compiler)
{
    if (compiler->controlFlowBlockCount > 0) {
        const IlcControlFlowBlock* block = &compiler->controlFlowBlocks[compiler->controlFlowBlockCount - 1];
        if (block->type == BLOCK_LOOP) {
            return block->loop.labelBeginId;
        } else if (block->type == BLOCK_IF_ELSE) {
            return block->ifElse.hasElseBlock ? block->ifElse.labelElseId : block->ifElse.labelBeginId;
        } else {
            LOGE("invalid control flow block type %d\n", block->type);
            assert(false);
        }
    }
    return compiler->currentFunctionLabelId;
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
    const IlcRegister* reg;

    uint32_t regNum = src->registerNum;// = src->srcCount == 0 ? src->registerNum : src->srcs[src->srcCount - 1].registerNum;
    if (src->srcCount > 0) {
        //idk how to select register here
        for (unsigned i = 0; i < src->srcCount; ++i) {
            if (src->srcs[i].registerType == src->registerType) {
                regNum = src->srcs[i].registerNum;
            }
        }
    }
    if ((src->swizzle[0] == IL_COMPSEL_0 || src->swizzle[0] == IL_COMPSEL_1) &&
        (src->swizzle[1] == IL_COMPSEL_0 || src->swizzle[1] == IL_COMPSEL_1) &&
        (src->swizzle[2] == IL_COMPSEL_0 || src->swizzle[2] == IL_COMPSEL_1) &&
        (src->swizzle[3] == IL_COMPSEL_0 || src->swizzle[3] == IL_COMPSEL_1)) {
        // We're reading only 0 or 1s so it's safe to create a temporary register
        // Example: r4096.0001 as seen in 3DMark shader
        reg = findOrCreateRegister(compiler, src->registerType, regNum);
    } else {
        reg = findRegister(compiler, src->registerType, regNum);
    }

    if (reg == NULL) {
        LOGE("source register %d %d not found\n", src->registerType, regNum);
        return 0;
    }

    IlcSpvId ptrId = 0;
    if (src->registerType == IL_REGTYPE_ITEMP ||
        src->registerType == IL_REGTYPE_IMMED_CONST_BUFF) {
        // 1D arrays
        IlcSpvId ptrTypeId = ilcSpvPutPointerType(compiler->module, SpvStorageClassPrivate,
                                                  reg->vecTypeId);
        IlcSpvId indexId = ilcSpvPutConstant(compiler->module, compiler->intId,
                                             src->hasImmediate ? src->immediate : 0);
        if (src->srcCount > 0) {
            assert(src->srcCount == 1);
            IlcSpvId rel4Id = loadSource(compiler, &src->srcs[0], COMP_MASK_XYZW,
                                         compiler->int4Id);
            IlcSpvId relId = emitVectorTrim(compiler, rel4Id, compiler->int4Id, 0, 1);
            indexId = ilcSpvPutOp2(compiler->module, SpvOpIAdd, compiler->intId, indexId, relId);
        }
        ptrId = ilcSpvPutAccessChain(compiler->module, ptrTypeId, reg->id, 1, &indexId);
    } else if (src->registerType == IL_REGTYPE_INPUTCP) {
        IlcSpvId ptrTypeId = ilcSpvPutPointerType(compiler->module, SpvStorageClassInput,
                                                  reg->vecTypeId);
        IlcSpvId indexId;
        if (src->srcCount > 1) {//last source is always the attribute
            IlcSpvId rel4Id = loadSource(compiler, &src->srcs[0], COMP_MASK_XYZW,
                                         compiler->int4Id);
            indexId = emitVectorTrim(compiler, rel4Id, compiler->int4Id, 0, 1);
        } else if (src->srcCount == 1 && src->srcs[0].registerType == IL_REGTYPE_INPUTCP) {
            indexId = ilcSpvPutConstant(compiler->module, compiler->intId, src->registerNum);
        } else {
            LOGE("unhandled source count %d\n", src->srcCount);
            assert(false);
        }
        if (src->hasImmediate) {
            IlcSpvId immId = ilcSpvPutConstant(compiler->module, compiler->intId, src->immediate);
            indexId = ilcSpvPutOp2(compiler->module, SpvOpIAdd, compiler->intId, indexId, immId);
        }
        if (compiler->kernel->shaderType == IL_SHADER_DOMAIN) {
            IlcSpvId indexIds[] = { indexId, ilcSpvPutConstant(compiler->module, compiler->intId, reg->ilNum) };
            ptrId = ilcSpvPutAccessChain(compiler->module, ptrTypeId, reg->id, 2, indexIds);
        } else {
            //extract the first register num
            ptrId = ilcSpvPutAccessChain(compiler->module, ptrTypeId, reg->id, 1, &indexId);
        }
    } else {
        if (src->hasImmediate) {
            LOGW("unhandled immediate\n");
        }
        if (src->srcCount > 0) {
            LOGW("unhandled extra sources (%u)\n", src->srcCount);
        }
        ptrId = reg->id;
    }

    IlcSpvId varId = ilcSpvPutLoad(compiler->module, reg->vecTypeId, ptrId);
    IlcSpvId componentTypeId = 0;

    if (reg->componentTypeId == compiler->boolId) {
        // Promote to float scalar
        componentTypeId = compiler->floatId;
        IlcSpvId trueId = ilcSpvPutConstant(compiler->module, componentTypeId, TRUE_LITERAL);
        IlcSpvId falseId = ilcSpvPutConstant(compiler->module, componentTypeId, FALSE_LITERAL);
        varId = ilcSpvPutSelect(compiler->module, componentTypeId, varId, trueId, falseId);
    } else {
        componentTypeId = reg->componentTypeId;
    }

    IlcSpvId vec4TypeId = ilcSpvPutVectorType(compiler->module, componentTypeId, 4);
    if (reg->componentCount < 4) {
        varId = emitVectorGrow(compiler, varId, componentTypeId, reg->componentCount);
    }

    const uint8_t swizzle[] = {
        componentMask & COMP_MASK_X ? src->swizzle[0] : IL_COMPSEL_0,
        componentMask & COMP_MASK_Y ? src->swizzle[1] : IL_COMPSEL_0,
        componentMask & COMP_MASK_Z ? src->swizzle[2] : IL_COMPSEL_0,
        componentMask & COMP_MASK_W ? src->swizzle[3] : IL_COMPSEL_0,
    };

    if (swizzle[0] != IL_COMPSEL_X_R || swizzle[1] != IL_COMPSEL_Y_G ||
        swizzle[2] != IL_COMPSEL_Z_B || swizzle[3] != IL_COMPSEL_W_A) {
        // Select components from {x, y, z, w, 0.f, 1.f}
        IlcSpvId zeroOneId = emitZeroOneVector(compiler, componentTypeId);

        const IlcSpvWord components[] = { swizzle[0], swizzle[1], swizzle[2], swizzle[3] };
        varId = ilcSpvPutVectorShuffle(compiler->module, vec4TypeId, varId, zeroOneId,
                                       4, components);
    }

    if (typeId != vec4TypeId) {
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
            negId = ilcSpvPutOp1(compiler->module, SpvOpFNegate, compiler->float4Id, varId);
        } else if (typeId == compiler->int4Id) {
            negId = ilcSpvPutOp1(compiler->module, SpvOpSNegate, compiler->int4Id, varId);
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
    IlcSpvId varId,
    IlcSpvId typeId)
{
    const IlcRegister* reg = findOrCreateRegister(compiler, dst->registerType, dst->registerNum);

    if (reg == NULL) {
        LOGE("destination register %d %d not found\n", dst->registerType, dst->registerNum);
        return;
    }

    if (typeId != reg->vecTypeId && reg->componentCount == 4) {
        // Need to cast to the expected type
        varId = ilcSpvPutBitcast(compiler->module, reg->vecTypeId, varId);
    }
    //TODO: cast to other types

    IlcSpvId ptrId = 0;
    if (dst->registerType != IL_REGTYPE_ITEMP) {
        if (dst->hasImmediate) {
            LOGW("unhandled immediate\n");
        }
        if (dst->absoluteSrc != NULL) {
            LOGW("unhandled absolute source\n");
        }
        if (dst->relativeSrcCount > 0) {
            if (dst->relativeSrcCount == 1 && reg->arrayItemCount > 0) {
                IlcSpvId ptrTypeId = ilcSpvPutPointerType(compiler->module, SpvStorageClassOutput, reg->componentTypeId);

                IlcSpvId indexId = loadSource(compiler, &dst->relativeSrcs[0], COMP_MASK_XYZW, compiler->int4Id);
                indexId = emitVectorTrim(compiler, indexId, compiler->int4Id, COMP_INDEX_X, 1);

                ptrId = ilcSpvPutAccessChain(compiler->module, ptrTypeId, reg->id, 1, &indexId);
            } else {
                if (reg->arrayItemCount == 0) {
                    LOGW("expected to have arrayed type on register %d\n", dst->registerNum);
                }
                ptrId = reg->id;
                LOGW("unhandled relative source (%u) for type %u\n", dst->relativeSrcCount, dst->registerType);
            }
        } else if (reg->arrayItemCount > 0) {
            IlcSpvId ptrTypeId = ilcSpvPutPointerType(compiler->module, SpvStorageClassOutput, reg->vecTypeId);
            IlcSpvId indexId = ilcSpvPutConstant(compiler->module, compiler->intId, reg->arrayItemOffset);
            ptrId = ilcSpvPutAccessChain(compiler->module, ptrTypeId, reg->id, 1, &indexId);
        } else {
            ptrId = reg->id;
        }
    } else {
        assert(dst->hasImmediate);
        IlcSpvId ptrTypeId = ilcSpvPutPointerType(compiler->module, SpvStorageClassPrivate,
                                                  reg->typeId);
        IlcSpvId indexId = ilcSpvPutConstant(compiler->module, compiler->intId, dst->immediate);
        ptrId = ilcSpvPutAccessChain(compiler->module, ptrTypeId, reg->id, 1, &indexId);
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
        if (reg->componentCount == 1) {
            // Nothing to do
        } else if (reg->componentCount == 4) {
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
        } else {
            LOGW("unhandled write mask for component count %u\n", reg->componentCount);
        }
    }

    if ((dst->component[0] == IL_MODCOMP_0 || dst->component[0] == IL_MODCOMP_1) ||
        (dst->component[1] == IL_MODCOMP_0 || dst->component[1] == IL_MODCOMP_1) ||
        (dst->component[2] == IL_MODCOMP_0 || dst->component[2] == IL_MODCOMP_1) ||
        (dst->component[3] == IL_MODCOMP_0 || dst->component[3] == IL_MODCOMP_1)) {
        // Select components from {x, y, z, w, 0.f, 1.f}
        IlcSpvId zeroOneId = emitZeroOneVector(compiler, reg->componentTypeId);

        const IlcSpvWord components[] = {
            dst->component[0] == IL_MODCOMP_0 ? 4 : (dst->component[0] == IL_MODCOMP_1 ? 5 : 0),
            dst->component[1] == IL_MODCOMP_0 ? 4 : (dst->component[1] == IL_MODCOMP_1 ? 5 : 1),
            dst->component[2] == IL_MODCOMP_0 ? 4 : (dst->component[2] == IL_MODCOMP_1 ? 5 : 2),
            dst->component[3] == IL_MODCOMP_0 ? 4 : (dst->component[3] == IL_MODCOMP_1 ? 5 : 3),
        };
        varId = ilcSpvPutVectorShuffle(compiler->module, reg->typeId, varId, zeroOneId,
                                       4, components);
    }

    if (reg->componentCount < 4) {
        varId = emitVectorTrim(compiler, varId, typeId, 0, reg->componentCount);
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

static void emitConstBuffer(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    assert(instr->extraCount % 4 == 0);

    // Create immediate constant buffer
    unsigned arraySize = instr->extraCount / 4;
    IlcSpvId lengthId = ilcSpvPutConstant(compiler->module, compiler->uintId, arraySize);
    IlcSpvId typeId = compiler->float4Id;
    IlcSpvId arrayTypeId = ilcSpvPutArrayType(compiler->module, typeId, lengthId);
    IlcSpvId arrayId = emitVariable(compiler, arrayTypeId, SpvStorageClassPrivate);

    // Populate array
    IlcSpvId ptrTypeId = ilcSpvPutPointerType(compiler->module, SpvStorageClassPrivate, typeId);
    for (unsigned i = 0; i < arraySize; i++) {
        IlcSpvId consistuentIds[] = {
            ilcSpvPutConstant(compiler->module, compiler->floatId, instr->extras[4 * i + 0]),
            ilcSpvPutConstant(compiler->module, compiler->floatId, instr->extras[4 * i + 1]),
            ilcSpvPutConstant(compiler->module, compiler->floatId, instr->extras[4 * i + 2]),
            ilcSpvPutConstant(compiler->module, compiler->floatId, instr->extras[4 * i + 3]),
        };
        IlcSpvId compositeId = ilcSpvPutConstantComposite(compiler->module, typeId,
                                                          4, consistuentIds);

        IlcSpvId addrId = ilcSpvPutConstant(compiler->module, compiler->uintId, i);
        IlcSpvId ptrId = ilcSpvPutAccessChain(compiler->module, ptrTypeId, arrayId, 1, &addrId);
        ilcSpvPutStore(compiler->module, ptrId, compositeId);
    }

    const IlcRegister constBufferReg = {
        .id = arrayId,
        .typeId = arrayTypeId,
        .vecTypeId = typeId,
        .componentTypeId = compiler->floatId,
        .componentCount = 4,
        .arrayItemCount = arraySize,
        .ilType = IL_REGTYPE_IMMED_CONST_BUFF,
        .ilNum = 0,
        .ilImportUsage = 0,
        .ilInterpMode = 0,
    };

    addRegister(compiler, &constBufferReg, "icb");
}

static void emitIndexedTempArray(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    const Source* src = &instr->srcs[0];

    assert(src->registerType == IL_REGTYPE_ITEMP && src->hasImmediate);

    // Create temporary array register
    unsigned arraySize = src->immediate;
    IlcSpvId lengthId = ilcSpvPutConstant(compiler->module, compiler->uintId, arraySize);
    IlcSpvId arrayTypeId = ilcSpvPutArrayType(compiler->module, compiler->float4Id, lengthId);
    IlcSpvId arrayId = emitVariable(compiler, arrayTypeId, SpvStorageClassPrivate);

    const IlcRegister tempArrayReg = {
        .id = arrayId,
        .typeId = compiler->float4Id,
        .vecTypeId = compiler->float4Id,
        .componentTypeId = compiler->floatId,
        .componentCount = 4,
        .arrayItemCount = arraySize,
        .ilType = src->registerType,
        .ilNum = src->registerNum,
        .ilImportUsage = 0,
        .ilInterpMode = 0,
    };

    addRegister(compiler, &tempArrayReg, "x");
}

static void emitLiteral(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    const Source* src = &instr->srcs[0];

    assert(src->registerType == IL_REGTYPE_LITERAL);

    IlcSpvId literalTypeId = compiler->float4Id;
    IlcSpvId literalId = emitVariable(compiler, literalTypeId, SpvStorageClassPrivate);

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
        .vecTypeId = literalTypeId,
        .componentTypeId = compiler->floatId,
        .componentCount = 4,
        .arrayItemCount = 0,
        .ilType = src->registerType,
        .ilNum = src->registerNum,
        .ilImportUsage = 0,
        .ilInterpMode = 0,
    };

    addRegister(compiler, &reg, "l");
}

static void emitGenericInputInfo(
    IlcCompiler* compiler,
    uint32_t location,
    uint32_t interpolationType)
{
    // emit output info
    compiler->inputCount++;
    compiler->inputs = realloc(compiler->inputs, compiler->inputCount * sizeof(IlcInput));
    compiler->inputs[compiler->inputCount - 1] = (IlcInput){
        .locationIndex = location,
        .interpolationType = interpolationType,
    };
}

static void emitGenericOutputInfo(
    IlcCompiler* compiler,
    uint32_t location)
{
    // emit output info
    compiler->outputCount++;
    compiler->outputLocations = realloc(compiler->outputLocations, compiler->outputCount * sizeof(uint32_t));
    compiler->outputLocations[compiler->outputCount - 1] = location;
}

static void emitOutput(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint8_t importUsage = GET_BITS(instr->control, 0, 4);

    const Destination* dst = &instr->dsts[0];
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

    IlcSpvId outputId = 0;
    IlcSpvId componentTypeId = compiler->floatId;
    IlcSpvId vectorTypeId = 0;
    IlcSpvId outputTypeId = 0;
    unsigned outputComponentCount = 0;
    unsigned outputArrayItemCount = 0;
    unsigned outputArrayItemOffset = 0;
    const char* outputPrefix = NULL;

    if (dst->registerType == IL_REGTYPE_OUTPUT) {
        if (importUsage == IL_IMPORTUSAGE_EDGE_TESSFACTOR || importUsage == IL_IMPORTUSAGE_INSIDE_TESSFACTOR) {
            dupeReg = findRegisterByType(compiler, dst->registerType, importUsage);
            componentTypeId = compiler->floatId;
            outputArrayItemCount = importUsage == IL_IMPORTUSAGE_INSIDE_TESSFACTOR ? 2 : 4;
            IlcSpvId lengthId = ilcSpvPutConstant(compiler->module, compiler->intId, outputArrayItemCount);
            vectorTypeId = componentTypeId;
            outputTypeId = ilcSpvPutArrayType(compiler->module, componentTypeId, lengthId);
            if (dupeReg != NULL) {
                //just copy it over (TODO: and maybe set offset for later reads here)
                outputId = dupeReg->id;
                outputArrayItemOffset = dst->registerNum - dupeReg->ilNum;
            } else {
                outputId = emitVariable(compiler, outputTypeId, SpvStorageClassOutput);
            }
            outputComponentCount = 1;
        } else {
            outputTypeId = compiler->float4Id;
            outputId = emitVariable(compiler, outputTypeId, SpvStorageClassOutput);
            outputComponentCount = 4;
        }
        outputPrefix = "o";
        if (dupeReg != NULL) {
            goto register_create;
        }
        if (importUsage == IL_IMPORTUSAGE_POS) {
            IlcSpvWord builtInType = SpvBuiltInPosition;
            ilcSpvPutDecoration(compiler->module, outputId, SpvDecorationBuiltIn, 1, &builtInType);
        } else if (importUsage == IL_IMPORTUSAGE_GENERIC) {
            IlcSpvWord locationIdx = dst->registerNum;
            ilcSpvPutDecoration(compiler->module, outputId, SpvDecorationLocation, 1, &locationIdx);
            emitGenericOutputInfo(compiler, locationIdx);
        } else if (importUsage == IL_IMPORTUSAGE_EDGE_TESSFACTOR) {
            //glLevelOuter
            ilcSpvPutDecoration(compiler->module, outputId, SpvDecorationPatch, 0, NULL);
            IlcSpvWord builtInType = SpvBuiltInTessLevelOuter;
            ilcSpvPutDecoration(compiler->module, outputId, SpvDecorationBuiltIn, 1, &builtInType);
            outputPrefix = "oTessLevelOuter";
        } else if (importUsage == IL_IMPORTUSAGE_INSIDE_TESSFACTOR) {
            //glLevelOuter
            ilcSpvPutDecoration(compiler->module, outputId, SpvDecorationPatch, 0, NULL);
            IlcSpvWord builtInType = SpvBuiltInTessLevelInner;
            ilcSpvPutDecoration(compiler->module, outputId, SpvDecorationBuiltIn, 1, &builtInType);
            outputPrefix = "oTessLevelInner";
        } else {
            LOGW("unhandled import usage %d\n", importUsage);
        }
    } else if (dst->registerType == IL_REGTYPE_DEPTH) {
        outputTypeId = compiler->floatId;
        outputId = emitVariable(compiler, outputTypeId, SpvStorageClassOutput);
        outputComponentCount = 1;
        outputPrefix = "oDepth";

        IlcSpvWord builtInType = SpvBuiltInFragDepth;
        ilcSpvPutDecoration(compiler->module, outputId, SpvDecorationBuiltIn, 1, &builtInType);

        // TODO explore what Re-Z really means
        ilcSpvPutExecMode(compiler->module, compiler->entryPointId, SpvExecutionModeDepthReplacing,
                          0, NULL);
    } else if (dst->registerType == IL_REGTYPE_OMASK) {
        componentTypeId = compiler->intId;
        vectorTypeId = compiler->intId;
        IlcSpvId oneId = ilcSpvPutConstant(compiler->module, compiler->intId, 1);
        outputTypeId = ilcSpvPutArrayType(compiler->module, compiler->intId, oneId);
        outputComponentCount = 1;
        outputArrayItemCount = 1;

        outputId = emitVariable(compiler, outputTypeId, SpvStorageClassOutput);

        IlcSpvWord builtInType = SpvBuiltInSampleMask;
        ilcSpvPutDecoration(compiler->module, outputId, SpvDecorationBuiltIn, 1, &builtInType);

        outputPrefix = "oSampleMask";
    } else {
        LOGE("unhandled output register type %d %d\n", dst->registerType, importUsage);
        assert(false);
    }
register_create:
    const IlcRegister reg = {
        .id = outputId,
        .typeId = outputTypeId,
        .vecTypeId = vectorTypeId == 0 ? outputTypeId : vectorTypeId,
        .componentTypeId = componentTypeId,
        .componentCount = outputComponentCount,
        .arrayItemCount = outputArrayItemCount,
        .arrayItemOffset = outputArrayItemOffset,
        .duped = dupeReg != NULL,
        .ilType = dst->registerType,
        .ilNum = dst->absoluteSrc == NULL ? dst->registerNum : dst->absoluteSrc->registerNum,
        .ilImportUsage = importUsage,
        .ilInterpMode = 0,
    };

    addRegister(compiler, &reg, outputPrefix);
}

static void emitInput(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint8_t importUsage = GET_BITS(instr->control, 0, 4);
    uint8_t interpMode = GET_BITS(instr->control, 5, 7);
    IlcSpvId inputId = 0;
    IlcSpvId inputTypeId = 0;
    IlcSpvId inputComponentTypeId = 0;
    unsigned inputArrayItemCount = 0;
    unsigned inputComponentCount = 0;
    IlcSpvId inputVecTypeId = 0;

    assert(instr->dstCount == 1 &&
           instr->srcCount == 0 &&
           instr->extraCount == 0);

    const Destination* dst = &instr->dsts[0];

    if (!((dst->registerType == IL_REGTYPE_INPUT || dst->registerType == IL_REGTYPE_SHADER_INSTANCE_ID ||
           dst->registerType == IL_REGTYPE_INPUTCP || dst->registerType == IL_REGTYPE_PATCHCONST ||
           dst->registerType == IL_REGTYPE_DOMAINLOCATION) &&
          !dst->clamp &&
          dst->shiftScale == IL_SHIFT_NONE)) {
        LOGE("unhandled input data %d %d\n",dst->registerType, dst->shiftScale);
    }
    assert((dst->registerType == IL_REGTYPE_INPUT || dst->registerType == IL_REGTYPE_SHADER_INSTANCE_ID || dst->registerType == IL_REGTYPE_INPUTCP ||
            dst->registerType == IL_REGTYPE_PATCHCONST || dst->registerType == IL_REGTYPE_DOMAINLOCATION) &&
           !dst->clamp &&
           dst->shiftScale == IL_SHIFT_NONE);

    const IlcRegister* dupeReg = findRegister(compiler, dst->registerType, dst->absoluteSrc == NULL ? dst->registerNum : dst->absoluteSrc->registerNum);
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
    const char* name = "v";
    if (dst->registerType == IL_REGTYPE_SHADER_INSTANCE_ID) {
        inputTypeId = compiler->intId;
        inputComponentTypeId = compiler->intId;
        inputComponentCount = 1;
        inputId = emitVariable(compiler, inputTypeId, SpvStorageClassPrivate);
        IlcSpvId vInvocationId;
        switch (compiler->currentHullPhase.type) {
        case CONTROL_POINT:
            vInvocationId = compiler->controlPointPhase.invocationId;
            break;
        case FORK:
            vInvocationId = compiler->forkPhases[compiler->currentHullPhase.phaseId].invocationId;
            break;
        case JOIN:
            vInvocationId = compiler->joinPhases[compiler->currentHullPhase.phaseId].invocationId;
            break;
        }
        ilcSpvPutStore(compiler->module, inputId, vInvocationId);
    } else if (dst->registerType == IL_REGTYPE_INPUTCP) {
        name = "vicp";
        if (compiler->kernel->shaderType == IL_SHADER_DOMAIN) {
            dupeReg = findRegister(compiler, IL_REGTYPE_INPUTCP, V_ARRAY_REG_ID);
            if (dupeReg == NULL) {
                LOGE("failed to find vertex input array for domain shader\n");
                assert(false);
            }
            inputComponentTypeId = compiler->floatId;
            inputVecTypeId = compiler->float4Id;
            inputComponentCount = 4;
            inputArrayItemCount = dst->absoluteSrc == NULL ? 4 : dst->registerNum;
            IlcSpvId lengthId = ilcSpvPutConstant(compiler->module, compiler->intId, dupeReg->arrayItemCount);
            inputTypeId = ilcSpvPutArrayType(compiler->module, inputVecTypeId, lengthId);
            inputId = dupeReg->id;
        } else {
            inputComponentTypeId = compiler->floatId;
            inputVecTypeId = compiler->float4Id;
            inputComponentCount = 4;
            inputArrayItemCount = dst->absoluteSrc == NULL ? 4 : dst->registerNum;
            IlcSpvId lengthId = ilcSpvPutConstant(compiler->module, compiler->intId, inputArrayItemCount);
            inputTypeId = ilcSpvPutArrayType(compiler->module, inputVecTypeId, lengthId);
            inputId = emitVariable(compiler, inputTypeId, SpvStorageClassInput);
            if (importUsage == IL_IMPORTUSAGE_GENERIC) {
                IlcSpvWord locationIdx = (dst->absoluteSrc == NULL ? dst->registerNum : dst->absoluteSrc->registerNum);
                ilcSpvPutDecoration(compiler->module, inputId, SpvDecorationLocation, 1, &locationIdx);
            } else {
                LOGW("unhandled import usage for vicp: %d\n", importUsage);
            }
        }
    } else if (dst->registerType == IL_REGTYPE_PATCHCONST) {
        //TODO: just search it by output and then loadSource (unimplemented for tess level outer - and other arrayed types)
        //TODO: also implement support for this for domain (tess eval) sahder
        if (compiler->kernel->shaderType == IL_SHADER_DOMAIN) {
            LOGW("support for patch constants for domain shaders isn't implemented\n");
        } else if (compiler->kernel->shaderType != IL_SHADER_HULL) {
            LOGE("unexpected shader type for patchconst (vpc): %d\n", compiler->kernel->shaderType);
        }
        const IlcRegister* edgeReg = findRegisterByType(compiler, IL_REGTYPE_OUTPUT, IL_IMPORTUSAGE_EDGE_TESSFACTOR);
        if (edgeReg == NULL) {
            LOGW("failed to find vTessLevelOuter\n");
            assert(false);
        }
        // since patchconst is just tessfactor, let's just store it here
        //this is just tess level outer
        inputComponentTypeId = compiler->floatId;
        inputComponentCount = 4;
        inputTypeId = compiler->float4Id;
        inputId = emitVariable(compiler, inputTypeId, SpvStorageClassPrivate);
        name = "vpc";
        //TODO: change stuff here
        if (importUsage != IL_IMPORTUSAGE_GENERIC) {
            //there can be collisions
            //IlcSpvWord locationIdx = (dst->absoluteSrc == NULL ? dst->registerNum : dst->absoluteSrc->registerNum);
            //ilcSpvPutDecoration(compiler->module, inputId, SpvDecorationLocation, 1, &locationIdx);
            //} else {
            LOGW("unhandled import usage for vpc: %d\n", importUsage);
        }
        //TODO: use loadSource function
        IlcSpvId indexId = ilcSpvPutConstant(compiler->module, compiler->intId, dst->registerNum);
        IlcSpvId zeroFloatId = ilcSpvPutConstant(compiler->module, compiler->floatId, 0.0f);
        IlcSpvId refId = ilcSpvPutAccessChain(compiler->module, ilcSpvPutPointerType(compiler->module, SpvStorageClassOutput, edgeReg->componentTypeId), edgeReg->id, 1, &indexId);
        IlcSpvId valueId = ilcSpvPutLoad(compiler->module, edgeReg->componentTypeId, refId);
        const IlcSpvWord constituents[] = { valueId, zeroFloatId, zeroFloatId, zeroFloatId };
        IlcSpvId vecId = ilcSpvPutCompositeConstruct(compiler->module, compiler->float4Id, 4, constituents);
        ilcSpvPutStore(compiler->module, inputId, vecId);
    } else if (dst->registerType == IL_REGTYPE_DOMAINLOCATION) {
        inputComponentTypeId = compiler->floatId;
        inputComponentCount = 3;
        inputTypeId = inputVecTypeId = ilcSpvPutVectorType(compiler->module, inputComponentTypeId, inputComponentCount);
        inputId = emitVariable(compiler, inputTypeId, SpvStorageClassInput);
        name = "tessCoord";
        IlcSpvWord builtInType = SpvBuiltInTessCoord;
        ilcSpvPutDecoration(compiler->module, inputId, SpvDecorationBuiltIn, 1, &builtInType);
    } else if (importUsage == IL_IMPORTUSAGE_POS) {
        inputComponentTypeId = compiler->floatId;
        inputComponentCount = 4;
        inputTypeId = compiler->float4Id;
        inputId = emitVariable(compiler, inputTypeId, SpvStorageClassInput);

        IlcSpvWord builtInType = SpvBuiltInFragCoord;
        ilcSpvPutDecoration(compiler->module, inputId, SpvDecorationBuiltIn, 1, &builtInType);
    } else if (importUsage == IL_IMPORTUSAGE_GENERIC) {
        inputComponentTypeId = compiler->floatId;
        inputComponentCount = 4;
        inputTypeId = compiler->float4Id;
        inputId = emitVariable(compiler, inputTypeId, SpvStorageClassInput);

        IlcSpvWord locationIdx = dst->registerNum;
        ilcSpvPutDecoration(compiler->module, inputId, SpvDecorationLocation, 1, &locationIdx);
        emitGenericInputInfo(compiler, locationIdx, interpMode);
    } else if (importUsage == IL_IMPORTUSAGE_PRIMITIVEID ||
               importUsage == IL_IMPORTUSAGE_VERTEXID ||
               importUsage == IL_IMPORTUSAGE_INSTANCEID ||
               importUsage == IL_IMPORTUSAGE_SAMPLE_INDEX) {
        inputComponentTypeId = compiler->intId;
        inputComponentCount = 1;
        inputTypeId = compiler->intId;
        inputId = emitVariable(compiler, inputTypeId, SpvStorageClassInput);

        IlcSpvWord builtInType = 0;
        if (importUsage == IL_IMPORTUSAGE_PRIMITIVEID) {
            builtInType = SpvBuiltInPrimitiveId;
            ilcSpvPutCapability(compiler->module, SpvCapabilityGeometry);
        } else if (importUsage == IL_IMPORTUSAGE_VERTEXID) {
            builtInType = SpvBuiltInVertexIndex;
        } else if (importUsage == IL_IMPORTUSAGE_INSTANCEID) {
            builtInType = SpvBuiltInInstanceIndex;
        } else if (importUsage == IL_IMPORTUSAGE_SAMPLE_INDEX) {
            builtInType = SpvBuiltInSampleId;
            ilcSpvPutCapability(compiler->module, SpvCapabilitySampleRateShading);
        } else {
            assert(false);
        }
        ilcSpvPutDecoration(compiler->module, inputId, SpvDecorationBuiltIn, 1, &builtInType);
    } else if (importUsage == IL_IMPORTUSAGE_ISFRONTFACE) {
        inputComponentTypeId = compiler->boolId;
        inputComponentCount = 1;
        inputTypeId = compiler->boolId;
        inputId = emitVariable(compiler, inputTypeId, SpvStorageClassInput);

        IlcSpvWord builtInType = SpvBuiltInFrontFacing;
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
        .vecTypeId = inputVecTypeId == 0 ? inputTypeId : inputVecTypeId,
        .componentTypeId = inputComponentTypeId,
        .componentCount = inputComponentCount,
        .arrayItemCount = inputArrayItemCount,
        .arrayItemOffset = 0,
        .duped = dupeReg != NULL,
        .ilType = dst->registerType,
        .ilNum = dst->absoluteSrc == NULL ? dst->registerNum : dst->absoluteSrc->registerNum,
        .ilImportUsage = importUsage,
        .ilInterpMode = interpMode,
    };

    addRegister(compiler, &reg, name);
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

    SpvDim spvDim = getSpvDimension(compiler, type, true);

    if (unnorm) {
        LOGE("unhandled unnorm resource\n");
        assert(false);
    }

    IlcSpvId sampledTypeId = 0;
    IlcSpvId texelTypeId = 0;
    SpvImageFormat spvImageFormat = SpvImageFormatUnknown;
    if (fmtx == IL_ELEMENTFORMAT_FLOAT && fmty == IL_ELEMENTFORMAT_FLOAT &&
        fmtz == IL_ELEMENTFORMAT_FLOAT && fmtw == IL_ELEMENTFORMAT_FLOAT) {
        sampledTypeId = compiler->floatId;
        texelTypeId = compiler->float4Id;
        spvImageFormat = SpvImageFormatRgba32f;
    } else if (fmtx == IL_ELEMENTFORMAT_SINT && fmty == IL_ELEMENTFORMAT_SINT &&
               fmtz == IL_ELEMENTFORMAT_SINT && fmtw == IL_ELEMENTFORMAT_SINT) {
        sampledTypeId = compiler->intId;
        texelTypeId = compiler->int4Id;
        spvImageFormat = SpvImageFormatRgba32i;
    } else if (fmtx == IL_ELEMENTFORMAT_UINT && fmty == IL_ELEMENTFORMAT_UINT &&
               fmtz == IL_ELEMENTFORMAT_UINT && fmtw == IL_ELEMENTFORMAT_UINT) {
        sampledTypeId = compiler->uintId;
        texelTypeId = compiler->uint4Id;
        spvImageFormat = SpvImageFormatRgba32ui;
    } else {
        LOGE("unhandled resource format %d %d %d %d\n", fmtx, fmty, fmtz, fmtw);
        assert(false);
    }

    IlcSpvId imageId = ilcSpvPutImageType(compiler->module, sampledTypeId, spvDim,
                                          0, isArrayed(type), isMultisampled(type), 1,
                                          spvImageFormat);
    IlcSpvId pImageId = ilcSpvPutPointerType(compiler->module, SpvStorageClassUniformConstant,
                                             imageId);
    IlcSpvId resourceId = ilcSpvPutVariable(compiler->module, pImageId,
                                            SpvStorageClassUniformConstant);

    emitBinding(compiler, resourceId, ILC_BASE_RESOURCE_ID + id,
                spvDim == SpvDimBuffer ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER :
                                         VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                NO_STRIDE_INDEX);

    const IlcResource resource = {
        .resType = RES_TYPE_GENERIC,
        .id = resourceId,
        .typeId = imageId,
        .texelTypeId = texelTypeId,
        .ilId = id,
        .ilType = type,
        .strideId = 0,
        .structured = false,
    };

    addResource(compiler, &resource);
}

static void emitTypedUav(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint16_t id = 0;
    uint8_t fmtx = 0;
    uint8_t type = 0;

    if (instr->opcode == IL_OP_DCL_UAV) {
        id = GET_BITS(instr->control, 0, 3);
        fmtx = GET_BITS(instr->control, 4, 7);
        type = GET_BITS(instr->control, 8, 11);
    } else if (instr->opcode == IL_OP_DCL_TYPED_UAV) {
        // IL_OP_DCL_TYPED_UAV allows 14-bit IDs
        assert(instr->extraCount == 1);
        id = GET_BITS(instr->control, 0, 13);
        fmtx = GET_BITS(instr->extras[0], 0, 3);
        type = GET_BITS(instr->extras[0], 4, 7);
    } else {
        assert(false);
    }

    SpvDim spvDim = getSpvDimension(compiler, type, false);

    IlcSpvId sampledTypeId = 0;
    SpvImageFormat spvImageFormat = SpvImageFormatUnknown;
    // AMD IL doesn't specify the type of each component in UAV aside from X,
    // but there can be more components than 1, so just leave image format as Unknown
    if (fmtx == IL_ELEMENTFORMAT_SINT) {
        sampledTypeId = compiler->intId;
        spvImageFormat = SpvImageFormatR32i;
    } else if (fmtx == IL_ELEMENTFORMAT_UINT) {
        sampledTypeId = compiler->uintId;
        spvImageFormat = SpvImageFormatR32ui;
    } else if (fmtx == IL_ELEMENTFORMAT_FLOAT) {
        sampledTypeId = compiler->floatId;
        spvImageFormat = SpvImageFormatR32f;
    } else {
        LOGE("unhandled format %d\n", fmtx);
        assert(false);
    }

    bool isAtomicUav = false;
    for (unsigned i = 0; i < compiler->atomicUavCount; ++i) {
        if (id == compiler->atomicUavs[i]) {
            isAtomicUav = true;
            break;
        }
    }
    if (!isAtomicUav) {
        // set image format only for atomic images, since atomic operations are only being used,
        // if image contains single component
        spvImageFormat = SpvImageFormatUnknown;
    }
    ilcSpvPutCapability(compiler->module, SpvCapabilityStorageImageReadWithoutFormat);
    ilcSpvPutCapability(compiler->module, SpvCapabilityStorageImageWriteWithoutFormat);
    IlcSpvId imageId = ilcSpvPutImageType(compiler->module, sampledTypeId, spvDim,
                                          0, isArrayed(type), isMultisampled(type), 2,
                                          spvImageFormat);
    IlcSpvId pImageId = ilcSpvPutPointerType(compiler->module, SpvStorageClassUniformConstant,
                                             imageId);
    IlcSpvId resourceId = ilcSpvPutVariable(compiler->module, pImageId,
                                            SpvStorageClassUniformConstant);

    ilcSpvPutName(compiler->module, imageId, "typedUav");
    emitBinding(compiler, resourceId, ILC_BASE_RESOURCE_ID + id,
                spvDim == SpvDimBuffer ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER :
                                         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                NO_STRIDE_INDEX);

    const IlcResource resource = {
        .resType = RES_TYPE_GENERIC,
        .id = resourceId,
        .typeId = imageId,
        .texelTypeId = sampledTypeId,
        .ilId = id,
        .ilType = type,
        .strideId = 0,
        .structured = false,
    };

    addResource(compiler, &resource);
}

static void emitUav(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    bool isStructured = instr->opcode == IL_OP_DCL_STRUCT_UAV || instr->opcode == IL_OP_DCL_TYPELESS_UAV;
    uint16_t id = GET_BITS(instr->control, 0, 13);

    IlcSpvId arrayId = ilcSpvPutRuntimeArrayType(compiler->module, compiler->floatId, true);
    IlcSpvId structId = ilcSpvPutStructType(compiler->module, 1, &arrayId);
    IlcSpvId pointerId = ilcSpvPutPointerType(compiler->module, SpvStorageClassStorageBuffer,
                                              structId);
    IlcSpvId resourceId = ilcSpvPutVariable(compiler->module, pointerId,
                                            SpvStorageClassStorageBuffer);

    IlcSpvWord arrayStride = sizeof(float);
    IlcSpvWord memberOffset = 0;
    ilcSpvPutDecoration(compiler->module, arrayId, SpvDecorationArrayStride, 1, &arrayStride);
    ilcSpvPutDecoration(compiler->module, structId, SpvDecorationBlock, 0, NULL);
    ilcSpvPutMemberDecoration(compiler->module, structId, 0, SpvDecorationOffset, 1, &memberOffset);

    ilcSpvPutName(compiler->module, arrayId, isStructured ? "structUav" : "rawUav");
    emitBinding(compiler, resourceId, ILC_BASE_RESOURCE_ID + id, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                NO_STRIDE_INDEX);

    const IlcResource resource = {
        .resType = RES_TYPE_GENERIC,
        .id = resourceId,
        .typeId = arrayId,
        .texelTypeId = compiler->floatId,
        .ilId = id,
        .ilType = IL_USAGE_PIXTEX_UNKNOWN,
        .strideId = ilcSpvPutConstant(compiler->module, compiler->intId,
                                      isStructured ? instr->extras[0] : 4),
        .structured = isStructured,
    };

    addResource(compiler, &resource);
}

static void emitSrv(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    bool isStructured = instr->opcode == IL_OP_DCL_STRUCT_SRV;
    uint16_t id = GET_BITS(instr->control, 0, 13);

    IlcSpvId arrayId = ilcSpvPutRuntimeArrayType(compiler->module, compiler->floatId, true);
    IlcSpvId structId = ilcSpvPutStructType(compiler->module, 1, &arrayId);
    IlcSpvId pointerId = ilcSpvPutPointerType(compiler->module, SpvStorageClassStorageBuffer,
                                              structId);
    IlcSpvId resourceId = ilcSpvPutVariable(compiler->module, pointerId,
                                            SpvStorageClassStorageBuffer);

    IlcSpvWord arrayStride = sizeof(float);
    IlcSpvWord memberOffset = 0;
    ilcSpvPutDecoration(compiler->module, arrayId, SpvDecorationArrayStride, 1, &arrayStride);
    ilcSpvPutDecoration(compiler->module, structId, SpvDecorationBlock, 0, NULL);
    ilcSpvPutMemberDecoration(compiler->module, structId, 0, SpvDecorationOffset, 1, &memberOffset);
    ilcSpvPutDecoration(compiler->module, resourceId, SpvDecorationNonWritable, 0, NULL);

    ilcSpvPutName(compiler->module, arrayId, isStructured ? "structSrv" : "rawSrv");
    emitBinding(compiler, resourceId, ILC_BASE_RESOURCE_ID + id, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                isStructured ? NO_STRIDE_INDEX : compiler->currentStrideIndex);

    IlcSpvId strideId = 0;

    if (isStructured) {
        strideId = ilcSpvPutConstant(compiler->module, compiler->intId, instr->extras[0]);
    } else {
        if (compiler->kernel->shaderType != IL_SHADER_VERTEX) {
            LOGE("unhandled raw SRVs for shader type %u\n", compiler->kernel->shaderType);
            assert(false);
        }
        if (compiler->currentStrideIndex >= ILC_MAX_STRIDE_CONSTANTS) {
            LOGE("too many SRV strides\n");
            assert(false);
        }

        const IlcResource* pcResource = findResource(compiler, RES_TYPE_PUSH_CONSTANTS, 0);

        if (pcResource == NULL) {
            // Lazily create push constants resource
            IlcSpvId lengthId = ilcSpvPutConstant(compiler->module, compiler->intId,
                                                  ILC_MAX_STRIDE_CONSTANTS);
            IlcSpvId arrayId = ilcSpvPutArrayType(compiler->module, compiler->intId, lengthId);
            IlcSpvId structId = ilcSpvPutStructType(compiler->module, 1, &arrayId);
            IlcSpvId pcId = emitVariable(compiler, structId, SpvStorageClassPushConstant);

            IlcSpvWord arrayStride = sizeof(uint32_t);
            IlcSpvWord memberOffset = 0;
            ilcSpvPutDecoration(compiler->module, arrayId, SpvDecorationArrayStride,
                                1, &arrayStride);
            ilcSpvPutDecoration(compiler->module, structId, SpvDecorationBlock, 0, NULL);
            ilcSpvPutMemberDecoration(compiler->module, structId, 0, SpvDecorationOffset,
                                      1, &memberOffset);

            const IlcResource pushConstantsResource = {
                .resType = RES_TYPE_PUSH_CONSTANTS,
                .id = pcId,
                .typeId = 0,
                .texelTypeId = 0,
                .ilId = 0,
                .ilType = IL_USAGE_PIXTEX_UNKNOWN,
                .strideId = 0,
            };

            pcResource = addResource(compiler, &pushConstantsResource);
        }

        IlcSpvId ptrTypeId = ilcSpvPutPointerType(compiler->module, SpvStorageClassPushConstant,
                                                  compiler->intId);
        IlcSpvId indexesId[] = {
            ilcSpvPutConstant(compiler->module, compiler->intId, 0),
            ilcSpvPutConstant(compiler->module, compiler->intId, compiler->currentStrideIndex),
        };
        IlcSpvId ptrId = ilcSpvPutAccessChain(compiler->module, ptrTypeId, pcResource->id,
                                              2, indexesId);
        strideId = ilcSpvPutLoad(compiler->module, compiler->intId, ptrId);

        compiler->currentStrideIndex++;
    }

    const IlcResource resource = {
        .resType = RES_TYPE_GENERIC,
        .id = resourceId,
        .typeId = arrayId,
        .texelTypeId = compiler->floatId,
        .ilId = id,
        .ilType = IL_USAGE_PIXTEX_UNKNOWN,
        .strideId = strideId,
        .structured = isStructured,
    };

    addResource(compiler, &resource);
}

static void emitLds(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint16_t id = GET_BITS(instr->control, 0, 13);
    unsigned length = instr->extras[0];

    IlcSpvId lengthId = ilcSpvPutConstant(compiler->module, compiler->uintId, length);
    IlcSpvId arrayId = ilcSpvPutArrayType(compiler->module, compiler->uintId, lengthId);
    IlcSpvId pArrayId = ilcSpvPutPointerType(compiler->module, SpvStorageClassWorkgroup, arrayId);
    IlcSpvId resourceId = ilcSpvPutVariable(compiler->module, pArrayId, SpvStorageClassWorkgroup);

    ilcSpvPutName(compiler->module, arrayId, "rawLds");

    const IlcResource resource = {
        .resType = RES_TYPE_LDS,
        .id = resourceId,
        .typeId = arrayId,
        .texelTypeId = compiler->uintId,
        .ilId = id,
        .ilType = IL_USAGE_PIXTEX_UNKNOWN,
        .strideId = ilcSpvPutConstant(compiler->module, compiler->intId, 4),
        .structured = false,
    };

    addResource(compiler, &resource);
}

static void emitStructuredLds(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint16_t id = GET_BITS(instr->control, 0, 13);
    unsigned stride = instr->extras[0];
    unsigned length = instr->extras[1];

    IlcSpvId lengthId = ilcSpvPutConstant(compiler->module, compiler->uintId, stride * length);
    IlcSpvId arrayId = ilcSpvPutArrayType(compiler->module, compiler->uintId, lengthId);
    IlcSpvId pArrayId = ilcSpvPutPointerType(compiler->module, SpvStorageClassWorkgroup, arrayId);
    IlcSpvId resourceId = ilcSpvPutVariable(compiler->module, pArrayId, SpvStorageClassWorkgroup);

    ilcSpvPutName(compiler->module, arrayId, "structLds");

    const IlcResource resource = {
        .resType = RES_TYPE_LDS,
        .id = resourceId,
        .typeId = arrayId,
        .texelTypeId = compiler->uintId,
        .ilId = id,
        .ilType = IL_USAGE_PIXTEX_UNKNOWN,
        .strideId = ilcSpvPutConstant(compiler->module, compiler->intId, stride),
        .structured = true,
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
    compiler->currentFunctionLabelId = ilcSpvPutLabel(compiler->module, 0);
}

static IlcHullPhaseBlock emitHullForkJoinFunction(
    IlcCompiler* compiler)
{
    IlcSpvId funcId = ilcSpvAllocId(compiler->module);
    IlcSpvId voidTypeId = ilcSpvPutVoidType(compiler->module);
    IlcSpvId funcTypeId = ilcSpvPutFunctionType(compiler->module, voidTypeId, 1, &compiler->intId);
    ilcSpvPutFunction(compiler->module, voidTypeId, funcId, SpvFunctionControlMaskNone, funcTypeId);
    IlcSpvId paramId = ilcSpvPutFunctionParameter(compiler->module, compiler->intId);
    compiler->currentFunctionLabelId = ilcSpvPutLabel(compiler->module, 0);//TODO: handle IL_REGTYPE_SHADER_INSTANCE_ID
    compiler->isInFunction = true;
    return (IlcHullPhaseBlock) {
        .functionId = funcId,
        .invocationId = paramId,
    };
}

static void emitHullShaderPhase(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    switch (instr->opcode) {
    case IL_OP_HS_FORK_PHASE: {
        compiler->forkPhaseCount++;
        compiler->forkPhases = realloc(compiler->forkPhases, compiler->forkPhaseCount * sizeof(IlcHullPhaseBlock));
        compiler->forkPhases[compiler->forkPhaseCount - 1] = emitHullForkJoinFunction(compiler);
        compiler->forkPhases[compiler->forkPhaseCount - 1].type = FORK;
        compiler->forkPhases[compiler->forkPhaseCount - 1].phaseId = compiler->forkPhaseCount - 1;
        compiler->forkPhases[compiler->forkPhaseCount - 1].invocationCount = GET_BITS(instr->control, 0, 15);
        if (compiler->forkPhases[compiler->forkPhaseCount - 1].invocationCount == 0) {
            compiler->forkPhases[compiler->forkPhaseCount - 1].invocationCount = 1;
        }

        char name[32];
        snprintf(name, sizeof(name), "hs_fork%u", compiler->forkPhaseCount - 1);
        ilcSpvPutName(compiler->module, compiler->forkPhases[compiler->forkPhaseCount - 1].functionId, name);
        compiler->isInFunction = true;
        compiler->currentHullPhase.type = FORK;
        compiler->currentHullPhase.phaseId = compiler->forkPhaseCount - 1;
    } break;
    case IL_OP_HS_JOIN_PHASE: {
        compiler->joinPhaseCount++;
        compiler->joinPhases = realloc(compiler->joinPhases, compiler->joinPhaseCount * sizeof(IlcHullPhaseBlock));
        compiler->joinPhases[compiler->joinPhaseCount - 1] = emitHullForkJoinFunction(compiler);
        compiler->joinPhases[compiler->joinPhaseCount - 1].type = JOIN;
        compiler->joinPhases[compiler->joinPhaseCount - 1].phaseId = compiler->joinPhaseCount - 1;
        compiler->joinPhases[compiler->joinPhaseCount - 1].invocationCount = GET_BITS(instr->control, 0, 15);
        if (compiler->joinPhases[compiler->joinPhaseCount - 1].invocationCount == 0) {
            compiler->joinPhases[compiler->joinPhaseCount - 1].invocationCount = 1;
        }
        char name[32];
        snprintf(name, sizeof(name), "hs_join%u", compiler->forkPhaseCount - 1);
        ilcSpvPutName(compiler->module, compiler->joinPhases[compiler->joinPhaseCount - 1].functionId, name);

        compiler->isInFunction = true;
        compiler->currentHullPhase.type = JOIN;
        compiler->currentHullPhase.phaseId = compiler->joinPhaseCount - 1;
    } break;
    case IL_OP_HS_CP_PHASE: {
        IlcSpvId functionId = ilcSpvAllocId(compiler->module);
        IlcSpvId voidTypeId = ilcSpvPutVoidType(compiler->module);
        IlcSpvId funcTypeId = ilcSpvPutFunctionType(compiler->module, voidTypeId, 0, NULL);
        ilcSpvPutFunction(compiler->module, voidTypeId, functionId, SpvFunctionControlMaskNone, funcTypeId);
        compiler->currentFunctionLabelId = ilcSpvPutLabel(compiler->module, 0);
        compiler->isInFunction = true;
        compiler->controlPointPhase = (IlcHullPhaseBlock){
            .type = CONTROL_POINT,
            .phaseId = 0,
            .functionId = functionId,
            .invocationId = 0,
        };

        compiler->currentHullPhase.type = CONTROL_POINT;
        compiler->currentHullPhase.phaseId = 0;

    } break;
    }
}

static void emitDomainShaderInit(
    IlcCompiler* compiler)
{
    IlcSpvId inputComponentTypeId = compiler->floatId;
    IlcSpvId inputVecTypeId = compiler->float4Id;
    unsigned inputComponentCount = 4;
    //TODO: handle OCP
    IlcSpvId inputArrayItemCount = compiler->inputControlPointSize;
    //TODO: just use runtime arrays per vertex
    // vertex count
    IlcSpvId vertexCountId = ilcSpvPutConstant(compiler->module, compiler->intId, inputArrayItemCount);
    // length per vertex
    IlcSpvId inputLengthId = ilcSpvPutConstant(compiler->module, compiler->intId, compiler->inputArraySize);

    IlcSpvId arrayTypeId = ilcSpvPutArrayType(compiler->module, inputVecTypeId, inputLengthId);

    IlcSpvId inputTypeId = ilcSpvPutArrayType(compiler->module, arrayTypeId, vertexCountId);
    IlcSpvId inputId = emitVariable(compiler, inputTypeId, SpvStorageClassInput);
    IlcSpvWord locationIdx = 0;//TODO: handle patches
    ilcSpvPutDecoration(compiler->module, inputId, SpvDecorationLocation, 1, &locationIdx);
    const IlcRegister reg = {
        .id = inputId,
        .typeId = inputTypeId,
        .vecTypeId = inputVecTypeId == 0 ? inputTypeId : inputVecTypeId,
        .componentTypeId = inputComponentTypeId,
        .componentCount = inputComponentCount,
        .arrayItemCount = inputArrayItemCount,
        .arrayItemOffset = 0,
        .duped = false,
        .ilType = IL_REGTYPE_INPUTCP,
        .ilNum = V_ARRAY_REG_ID,
        .ilImportUsage = IL_IMPORTUSAGE_GENERIC,
        .ilInterpMode = 0,
    };

    const char* name = "vIArray";
    addRegister(compiler, &reg, name);
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
        resId = ilcSpvPutOp2(compiler->module, SpvOpFAdd, compiler->float4Id, srcIds[0], srcIds[1]);
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
        resId = ilcSpvPutOp2(compiler->module, SpvOpFDiv, compiler->float4Id, srcIds[0], srcIds[1]);
        break;
    case IL_OP_DP2:
    case IL_OP_DP3:
    case IL_OP_DP4: {
        bool ieee = GET_BIT(instr->control, 0);
        if (!ieee) {
            LOGW("unhandled non-IEEE dot product\n");
        }
        IlcSpvId dotId = ilcSpvPutOp2(compiler->module, SpvOpDot, compiler->floatId,
                                      srcIds[0], srcIds[1]);
        // Replicate dot product on all components
        resId = emitVectorGrow(compiler, dotId, compiler->floatId, 1);
    }   break;
    case IL_OP_DSX:
    case IL_OP_DSY: {
        bool fine = GET_BIT(instr->control, 7);
        IlcSpvWord op = instr->opcode == IL_OP_DSX ? (fine ? SpvOpDPdxFine : SpvOpDPdxCoarse)
                                                   : (fine ? SpvOpDPdyFine : SpvOpDPdyCoarse);
        ilcSpvPutCapability(compiler->module, SpvCapabilityDerivativeControl);
        resId = ilcSpvPutOp1(compiler->module, op, compiler->float4Id, srcIds[0]);
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
        resId = ilcSpvPutOp2(compiler->module, SpvOpFMul, compiler->float4Id, srcIds[0], srcIds[1]);
    }   break;
    case IL_OP_FTOI:
        resId = ilcSpvPutOp1(compiler->module, SpvOpConvertFToS, compiler->int4Id, srcIds[0]);
        resId = ilcSpvPutBitcast(compiler->module, compiler->float4Id, resId);
        break;
    case IL_OP_FTOU:
        resId = ilcSpvPutOp1(compiler->module, SpvOpConvertFToU, compiler->uint4Id, srcIds[0]);
        resId = ilcSpvPutBitcast(compiler->module, compiler->float4Id, resId);
        break;
    case IL_OP_ITOF:
        resId = ilcSpvPutBitcast(compiler->module, compiler->int4Id, srcIds[0]);
        resId = ilcSpvPutOp1(compiler->module, SpvOpConvertSToF, compiler->float4Id, resId);
        break;
    case IL_OP_UTOF:
        resId = ilcSpvPutBitcast(compiler->module, compiler->uint4Id, srcIds[0]);
        resId = ilcSpvPutOp1(compiler->module, SpvOpConvertUToF, compiler->float4Id, resId);
        break;
    case IL_OP_ROUND_NEAR:
        resId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450Round, compiler->float4Id,
                                instr->srcCount, srcIds);
        break;
    case IL_OP_ROUND_NEG_INF:
        resId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450Floor, compiler->float4Id,
                                instr->srcCount, srcIds);
        break;
    case IL_OP_ROUND_PLUS_INF:
        resId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450Ceil, compiler->float4Id,
                                instr->srcCount, srcIds);
        break;
    case IL_OP_ROUND_ZERO:
        resId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450Trunc, compiler->float4Id,
                                instr->srcCount, srcIds);
        break;
    case IL_OP_EXP_VEC:
        resId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450Exp2, compiler->float4Id,
                                instr->srcCount, srcIds);
        break;
    case IL_OP_LOG_VEC:
        // FIXME handle log(0)
        resId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450Log2, compiler->float4Id,
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
    case IL_OP_F_2_F16: {
        IlcSpvId firstHalfId = emitVectorTrim(compiler, srcIds[0], compiler->float4Id, 0, 2);
        IlcSpvId secondHalfId = emitVectorTrim(compiler, srcIds[0], compiler->float4Id, 2, 2);
        IlcSpvId lsbId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450PackHalf2x16, compiler->intId,
                                         1, &firstHalfId);
        IlcSpvId msbId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450PackHalf2x16, compiler->intId,
                                         1, &secondHalfId);
        IlcSpvId zeroId = ilcSpvPutConstant(compiler->module, compiler->intId, 0);
        IlcSpvId sixteenId = ilcSpvPutConstant(compiler->module, compiler->intId, 16);
        const IlcSpvId componentIds[] = {
            ilcSpvPutOp3(compiler->module, SpvOpBitFieldUExtract, compiler->intId,
                         lsbId, zeroId, sixteenId),
            ilcSpvPutOp3(compiler->module, SpvOpBitFieldUExtract, compiler->intId,
                         lsbId, sixteenId, sixteenId),
            ilcSpvPutOp3(compiler->module, SpvOpBitFieldUExtract, compiler->intId,
                         msbId, zeroId, sixteenId),
            ilcSpvPutOp3(compiler->module, SpvOpBitFieldUExtract, compiler->intId,
                         msbId, sixteenId, sixteenId),
        };
        resId = ilcSpvPutCompositeConstruct(compiler->module, compiler->int4Id, 4, componentIds);
        resId = ilcSpvPutBitcast(compiler->module, compiler->float4Id, resId);
    }   break;
    case IL_OP_F16_2_F: {
        IlcSpvId intSrcId = ilcSpvPutBitcast(compiler->module, compiler->int4Id, srcIds[0]);
        IlcSpvId intIds[] = {
            emitVectorTrim(compiler, intSrcId, compiler->int4Id, 0, 1),
            emitVectorTrim(compiler, intSrcId, compiler->int4Id, 1, 1),
            emitVectorTrim(compiler, intSrcId, compiler->int4Id, 2, 1),
            emitVectorTrim(compiler, intSrcId, compiler->int4Id, 3, 1),
        };
        IlcSpvId sixteenId = ilcSpvPutConstant(compiler->module, compiler->intId, 16);
        intIds[0] = ilcSpvPutOp4(compiler->module, SpvOpBitFieldInsert, compiler->intId,
                                 intIds[0], intIds[1], sixteenId, sixteenId);
        intIds[2] = ilcSpvPutOp4(compiler->module, SpvOpBitFieldInsert, compiler->intId,
                                 intIds[2], intIds[3], sixteenId, sixteenId);
        IlcSpvId float2Id = ilcSpvPutVectorType(compiler->module, compiler->floatId, 2);
        IlcSpvId firstHalfId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450UnpackHalf2x16,
                                               float2Id, 1, &intIds[0]);
        IlcSpvId secondHalfId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450UnpackHalf2x16,
                                                float2Id, 1, &intIds[2]);
        const IlcSpvWord components[] = { 0, 1, 2, 3 };
        resId = ilcSpvPutVectorShuffle(compiler->module, compiler->float4Id,
                                       firstHalfId, secondHalfId, 4, components);
    }   break;
    default:
        assert(false);
        break;
    }

    if (instr->preciseMask != 0) {
        ilcSpvPutDecoration(compiler->module, resId, SpvDecorationNoContraction, 0, NULL);
    }

    storeDestination(compiler, &instr->dsts[0], resId, compiler->float4Id);
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

    IlcSpvId condId = ilcSpvPutOp2(compiler->module, compOp, compiler->bool4Id,
                                   srcIds[0], srcIds[1]);
    IlcSpvId trueId = ilcSpvPutConstant(compiler->module, compiler->floatId, TRUE_LITERAL);
    IlcSpvId falseId = ilcSpvPutConstant(compiler->module, compiler->floatId, FALSE_LITERAL);
    IlcSpvId true4Id = emitVectorGrow(compiler, trueId, compiler->floatId, 1);
    IlcSpvId false4Id = emitVectorGrow(compiler, falseId, compiler->floatId, 1);
    IlcSpvId resId = ilcSpvPutSelect(compiler->module, compiler->float4Id,
                                     condId, true4Id, false4Id);

    storeDestination(compiler, &instr->dsts[0], resId, compiler->float4Id);
}

static void emitIntegerOp(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    IlcSpvId srcIds[MAX_SRC_COUNT] = { 0 };
    IlcSpvId typeId = 0;
    IlcSpvId resId = 0;

    if (instr->opcode == IL_OP_U_DIV ||
        instr->opcode == IL_OP_U_MOD) {
        typeId = compiler->uint4Id;
    } else {
        typeId = compiler->int4Id;
    }

    for (int i = 0; i < instr->srcCount; i++) {
        srcIds[i] = loadSource(compiler, &instr->srcs[i], COMP_MASK_XYZW, typeId);
    }

    switch (instr->opcode) {
    case IL_OP_I_NOT:
        resId = ilcSpvPutOp1(compiler->module, SpvOpNot, compiler->int4Id, srcIds[0]);
        break;
    case IL_OP_I_OR:
        resId = ilcSpvPutOp2(compiler->module, SpvOpBitwiseOr, compiler->int4Id,
                             srcIds[0], srcIds[1]);
        break;
    case IL_OP_I_XOR:
        resId = ilcSpvPutOp2(compiler->module, SpvOpBitwiseXor, compiler->int4Id,
                             srcIds[0], srcIds[1]);
        break;
    case IL_OP_I_ADD:
        resId = ilcSpvPutOp2(compiler->module, SpvOpIAdd, compiler->int4Id,
                             srcIds[0], srcIds[1]);
        break;
    case IL_OP_I_MAD: {
        IlcSpvId mulId = ilcSpvPutOp2(compiler->module, SpvOpIMul, compiler->int4Id,
                                      srcIds[0], srcIds[1]);
        resId = ilcSpvPutOp2(compiler->module, SpvOpIAdd, compiler->int4Id, mulId, srcIds[2]);
    } break;
    case IL_OP_I_MAX:
        resId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450SMax, compiler->int4Id,
                                instr->srcCount, srcIds);
        break;
    case IL_OP_I_MIN:
        resId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450SMin, compiler->int4Id,
                                instr->srcCount, srcIds);
        break;
    case IL_OP_I_MUL:
        resId = ilcSpvPutOp2(compiler->module, SpvOpIMul, compiler->int4Id, srcIds[0], srcIds[1]);
        break;
    case IL_OP_I_NEGATE:
        resId = ilcSpvPutOp1(compiler->module, SpvOpSNegate, compiler->int4Id, srcIds[0]);
        break;
    case IL_OP_U_DIV:
        resId = ilcSpvPutOp2(compiler->module, SpvOpUDiv, compiler->uint4Id, srcIds[0], srcIds[1]);
        break;
    case IL_OP_U_MOD:
        resId = ilcSpvPutOp2(compiler->module, SpvOpUMod, compiler->uint4Id, srcIds[0], srcIds[1]);
        break;
    case IL_OP_U_MIN:
        resId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450UMin, compiler->uint4Id, 2, srcIds);
        break;
    case IL_OP_AND:
        resId = ilcSpvPutOp2(compiler->module, SpvOpBitwiseAnd, compiler->int4Id,
                             srcIds[0], srcIds[1]);
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
        resId = ilcSpvPutOp2(compiler->module, op, compiler->int4Id,
                             srcIds[0], emitShiftMask(compiler, srcIds[1]));
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
            bfId[i] = ilcSpvPutOp3(compiler->module, SpvOpBitFieldUExtract, compiler->intId,
                                   baseId, offsetId, widthId);
        }
        resId = ilcSpvPutCompositeConstruct(compiler->module, compiler->int4Id, 4, bfId);
    }   break;
    case IL_OP_U_BIT_INSERT: {
        IlcSpvId widthsId = emitShiftMask(compiler, srcIds[0]);
        IlcSpvId offsetsId = emitShiftMask(compiler, srcIds[1]);
        IlcSpvId bfId[4];
        for (unsigned i = 0; i < 4; i++) {
            IlcSpvId widthId = emitVectorTrim(compiler, widthsId, compiler->int4Id, i, 1);
            IlcSpvId offsetId = emitVectorTrim(compiler, offsetsId, compiler->int4Id, i, 1);
            IlcSpvId insertId = emitVectorTrim(compiler, srcIds[2], compiler->int4Id, i, 1);
            IlcSpvId baseId = emitVectorTrim(compiler, srcIds[3], compiler->int4Id, i, 1);
            bfId[i] = ilcSpvPutOp4(compiler->module, SpvOpBitFieldInsert, compiler->intId,
                                   baseId, insertId, offsetId, widthId);
        }
        resId = ilcSpvPutCompositeConstruct(compiler->module, compiler->int4Id, 4, bfId);
    }   break;
    default:
        assert(false);
        break;
    }

    storeDestination(compiler, &instr->dsts[0], resId, typeId);
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

    IlcSpvId condId = ilcSpvPutOp2(compiler->module, compOp, compiler->bool4Id,
                                   srcIds[0], srcIds[1]);
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

    storeDestination(compiler, &instr->dsts[0], resId, compiler->float4Id);
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
    IlcSpvId condId = ilcSpvPutOp2(compiler->module, SpvOpINotEqual, compiler->bool4Id,
                                   castId, falseCompositeId);
    IlcSpvId resId = ilcSpvPutSelect(compiler->module, compiler->float4Id, condId,
                                     srcIds[1], srcIds[2]);

    storeDestination(compiler, &instr->dsts[0], resId, compiler->float4Id);
}

static void emitNumThreadPerGroup(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    IlcSpvWord sizes[] = {
        instr->extraCount >= 1 ? instr->extras[0] : 1,
        instr->extraCount >= 2 ? instr->extras[1] : 1,
        instr->extraCount >= 3 ? instr->extras[2] : 1,
    };
    ilcSpvPutExecMode(compiler->module, compiler->entryPointId, SpvExecutionModeLocalSize,
                      3, sizes);
}

static void emitTessDomain(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    enum ILTsDomain domainType = (enum ILTsDomain)GET_BITS(instr->control, 0, 15);
    IlcSpvWord spvDomainType;
    switch (domainType) {
    case IL_TS_DOMAIN_ISOLINE:
        spvDomainType = SpvExecutionModeIsolines;
        break;
    case IL_TS_DOMAIN_TRI:
        spvDomainType = SpvExecutionModeTriangles;
        break;
    case IL_TS_DOMAIN_QUAD:
        spvDomainType = SpvExecutionModeQuads;
        break;
    default:
        LOGE("undefined domain type %d\n", domainType);
        break;
    }
    ilcSpvPutExecMode(compiler->module, compiler->entryPointId, spvDomainType,
                      0, NULL);
}

static void emitTessPartition(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    enum ILTsPartition partType = (enum ILTsPartition)GET_BITS(instr->control, 0, 15);
    IlcSpvWord spvPartType;
    switch (partType) {
    case IL_TS_PARTITION_INTEGER:
        spvPartType = SpvExecutionModeSpacingEqual;
        break;
    case IL_TS_PARTITION_POW2:
        LOGE("unhandled partition type pow2\n");
        break;
    case IL_TS_PARTITION_FRACTIONAL_ODD:
        spvPartType = SpvExecutionModeSpacingFractionalOdd;
        break;
    case IL_TS_PARTITION_FRACTIONAL_EVEN:
        spvPartType = SpvExecutionModeSpacingFractionalEven;
        break;
    default:
        LOGE("undefined partition type %d\n", partType);
        break;
    }
    ilcSpvPutExecMode(compiler->module, compiler->entryPointId, spvPartType, 0, NULL);
}

static void emitTessOutputPrimitive(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    enum ILTsOutputPrimitive primType = (enum ILTsOutputPrimitive)GET_BITS(instr->control, 0, 15);
    IlcSpvWord spvPrimType;
    switch (primType) {
    case IL_TS_OUTPUT_TRIANGLE_CW:
        spvPrimType = SpvExecutionModeVertexOrderCw;
        break;
    case IL_TS_OUTPUT_TRIANGLE_CCW:
        spvPrimType = SpvExecutionModeVertexOrderCcw;
        break;
    case IL_TS_OUTPUT_POINT:
        spvPrimType = SpvExecutionModePointMode;
        break;
    case IL_TS_OUTPUT_LINE:
        LOGE("unhandled line output primitive type\n", primType);
        break;
    default:
        LOGE("undefined primitive type %d\n", primType);
    }
    ilcSpvPutExecMode(compiler->module, compiler->entryPointId, spvPrimType, 0, NULL);
}

static void emitNumInputControlPoints(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    IlcSpvWord inputPoints = instr->extras[0];
    LOGW("unhandled number input control points %d\n", inputPoints);
}

static void emitNumOutputControlPoints(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    IlcSpvWord outputPoints = instr->extras[0];
    ilcSpvPutExecMode(compiler->module, compiler->entryPointId, SpvExecutionModeOutputVertices, 1, &outputPoints);
}

static IlcSpvId emitConditionCheck(
    IlcCompiler* compiler,
    IlcSpvId srcId,
    bool notZero)
{
    IlcSpvId xId = emitVectorTrim(compiler, srcId, compiler->int4Id, COMP_INDEX_X, 1);
    IlcSpvId falseId = ilcSpvPutConstant(compiler->module, compiler->intId, FALSE_LITERAL);
    SpvOp comparisonOp = notZero ? SpvOpINotEqual : SpvOpIEqual;
    return ilcSpvPutOp2(compiler->module, comparisonOp, compiler->boolId, xId, falseId);
}

static void emitIf(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    const IlcIfElseBlock ifElseBlock = {
        .labelBeginId = ilcSpvAllocId(compiler->module),
        .labelElseId = ilcSpvAllocId(compiler->module),
        .labelEndId = ilcSpvAllocId(compiler->module),
        .hasElseBlock = false,
    };

    IlcSpvId srcId = loadSource(compiler, &instr->srcs[0], COMP_MASK_XYZW, compiler->int4Id);
    IlcSpvId condId = emitConditionCheck(compiler, srcId, instr->opcode == IL_OP_IF_LOGICALNZ);
    ilcSpvPutSelectionMerge(compiler->module, ifElseBlock.labelEndId);
    ilcSpvPutBranchConditional(compiler->module, condId, ifElseBlock.labelBeginId, ifElseBlock.labelElseId);
    ilcSpvPutLabel(compiler->module, ifElseBlock.labelBeginId);

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

    if (compiler->isAfterReturn) {
        // Declare unreachable block
        ilcSpvPutLabel(compiler->module, ilcSpvAllocId(compiler->module));
        compiler->isAfterReturn = false;
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
        .labelBeginId = ilcSpvAllocId(compiler->module),
        .labelHeaderId = ilcSpvAllocId(compiler->module),
        .labelContinueId = ilcSpvAllocId(compiler->module),
        .labelBreakId = ilcSpvAllocId(compiler->module),
    };

    ilcSpvPutBranch(compiler->module, loopBlock.labelHeaderId);
    ilcSpvPutLabel(compiler->module, loopBlock.labelHeaderId);

    ilcSpvPutLoopMerge(compiler->module, loopBlock.labelBreakId, loopBlock.labelContinueId);

    ilcSpvPutBranch(compiler->module, loopBlock.labelBeginId);
    ilcSpvPutLabel(compiler->module, loopBlock.labelBeginId);

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
        // Declare unreachable block
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

    if (instr->opcode == IL_OP_CONTINUE) {
        ilcSpvPutBranch(compiler->module, block->loop.labelContinueId);
    } else if (instr->opcode == IL_OP_CONTINUE_LOGICALZ ||
               instr->opcode == IL_OP_CONTINUE_LOGICALNZ) {
        IlcSpvId srcId = loadSource(compiler, &instr->srcs[0], COMP_MASK_XYZW, compiler->int4Id);
        IlcSpvId condId = emitConditionCheck(compiler, srcId,
                                             instr->opcode == IL_OP_CONTINUE_LOGICALNZ);
        ilcSpvPutBranchConditional(compiler->module, condId, block->loop.labelContinueId, labelId);
    } else {
        assert(false);
    }

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

static void emitFence(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    bool threads = GET_BIT(instr->control, 0);
    bool lds = GET_BIT(instr->control, 1);
    bool memory = GET_BIT(instr->control, 2);
    bool sr = GET_BIT(instr->control, 3);
    bool mem_write_only = GET_BIT(instr->control, 4);
    bool mem_read_only = GET_BIT(instr->control, 5);
    bool gds = GET_BIT(instr->control, 6);

    SpvScope memoryScope = SpvScopeInvocation;
    SpvMemorySemanticsMask semanticsMask = SpvMemorySemanticsMaskNone;

    if (lds) {
        memoryScope = SpvScopeWorkgroup;
        semanticsMask |= SpvMemorySemanticsWorkgroupMemoryMask |
                         SpvMemorySemanticsAcquireReleaseMask;
    }
    if (!lds || memory || sr || mem_write_only || mem_read_only || gds) {
        LOGW("unhandled fence type %d %d %d %d %d %d %d",
             threads, lds, memory, sr, mem_write_only, mem_read_only, gds);
    }

    IlcSpvId memoryId = ilcSpvPutConstant(compiler->module, compiler->uintId, memoryScope);
    IlcSpvId semanticsId = ilcSpvPutConstant(compiler->module, compiler->uintId, semanticsMask);

    if (!threads) {
        ilcSpvPutMemoryBarrier(compiler->module, memoryId, semanticsId);
    } else {
        IlcSpvId executionId = ilcSpvPutConstant(compiler->module, compiler->uintId,
                                                 SpvScopeWorkgroup);
        ilcSpvPutControlBarrier(compiler->module, executionId, memoryId, semanticsId);
    }
}

static void emitLoad(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint8_t ilResourceId = GET_BITS(instr->control, 0, 7);

    const IlcResource* resource = findResource(compiler, RES_TYPE_GENERIC, ilResourceId);
    const Destination* dst = &instr->dsts[0];

    if (resource == NULL) {
        LOGE("resource %d not found\n", ilResourceId);
        return;
    }

    IlcSpvId srcId = loadSource(compiler, &instr->srcs[0], COMP_MASK_XYZW, compiler->int4Id);

    SpvImageOperandsMask operandsMask = 0;
    unsigned operandIdCount = 0;
    IlcSpvId operandIds[2];

    if (resource->ilType == IL_USAGE_PIXTEX_2DMSAA) {
        // Sample number is stored in src.w
        operandsMask |= SpvImageOperandsSampleMask;
        operandIds[0] = emitVectorTrim(compiler, srcId, compiler->int4Id, COMP_INDEX_W, 1);
        operandIdCount++;
    } else if (resource->ilType != IL_USAGE_PIXTEX_BUFFER && resource->ilType != IL_USAGE_PIXTEX_UNKNOWN) {
        // otherwise it is LOD
        operandsMask |= SpvImageOperandsLodMask;
        operandIds[0] = emitVectorTrim(compiler, srcId, compiler->int4Id, COMP_INDEX_W, 1);
        operandIdCount++;
    }

    IlcSpvId resourceId = ilcSpvPutLoad(compiler->module, resource->typeId, resource->id);
    IlcSpvId fetchId = ilcSpvPutImageFetch(compiler->module, resource->texelTypeId, resourceId,
                                           srcId, operandsMask, operandIdCount, operandIds);
    storeDestination(compiler, dst, fetchId, resource->texelTypeId);
}

static void emitResinfo(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint8_t ilResourceId = GET_BITS(instr->control, 0, 7);
    bool ilReturnType = GET_BIT(instr->control, 8);

    const IlcResource* resource = findResource(compiler, RES_TYPE_GENERIC, ilResourceId);
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
    IlcSpvId sizesId = ilcSpvPutImageQuerySizeLod(compiler->module, vecTypeId, resourceId, lodId);
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
    if (ilReturnType) {
        storeDestination(compiler, dst, infoId, compiler->int4Id);
    } else {
        IlcSpvId fInfoId = ilcSpvPutOp1(compiler->module, SpvOpConvertSToF, compiler->float4Id,
                                        infoId);
        storeDestination(compiler, dst, fInfoId, compiler->float4Id);
    }

}

static void emitSample(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint8_t ilResourceId = GET_BITS(instr->control, 0, 7);
    uint8_t ilSamplerId = GET_BITS(instr->control, 8, 11);
    // TODO handle indexed args and aoffimmi

    const IlcResource* resource = findResource(compiler, RES_TYPE_GENERIC, ilResourceId);
    const IlcSampler* sampler = findOrCreateSampler(compiler, ilSamplerId);
    const Destination* dst = &instr->dsts[0];

    if (resource == NULL) {
        LOGE("resource %d not found\n", ilResourceId);
        return;
    }

    unsigned dimCount = getResourceDimensionCount(resource->ilType);
    IlcSpvWord sampleOp = 0;
    IlcSpvId coordinateId = loadSource(compiler, &instr->srcs[0], COMP_MASK_XYZW,
                                       compiler->float4Id);
    IlcSpvId drefId = 0;
    SpvImageOperandsMask operandsMask = 0;
    unsigned operandIdCount = 0;
    IlcSpvId operandIds[3];

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
    } else if (instr->opcode == IL_OP_SAMPLE_C_LZ) {
        sampleOp = SpvOpImageSampleDrefExplicitLod;
        drefId = loadSource(compiler, &instr->srcs[1], COMP_MASK_XYZW, compiler->float4Id);
        drefId = emitVectorTrim(compiler, drefId, compiler->float4Id, COMP_INDEX_X, 1);
        operandsMask |= SpvImageOperandsLodMask;

        operandIds[0] = ilcSpvPutConstant(compiler->module, compiler->floatId, 0);
        operandIdCount++;
    } else {
        assert(false);
    }

    if (instr->addressOffset != 0) {
        float u = (int8_t)GET_BITS(instr->addressOffset, 0, 7) / 2.f;
        float v = (int8_t)GET_BITS(instr->addressOffset, 8, 15) / 2.f;
        float w = (int8_t)GET_BITS(instr->addressOffset, 16, 23) / 2.f;
        if (u != (int)u || v != (int)v || w != (int)w) {
            LOGW("non-integer offsets %g %g %g\n", u, v, w);
        }
        operandsMask |= SpvImageOperandsConstOffsetMask;
        const IlcSpvId constantIds[] = {
            ilcSpvPutConstant(compiler->module, compiler->intId, u),
            ilcSpvPutConstant(compiler->module, compiler->intId, v),
            ilcSpvPutConstant(compiler->module, compiler->intId, w),
        };
        IlcSpvId offsetsTypeId = ilcSpvPutVectorType(compiler->module, compiler->intId, dimCount);
        IlcSpvId offsetsId = ilcSpvPutConstantComposite(compiler->module, offsetsTypeId,
                                                        dimCount, constantIds);
        operandIds[operandIdCount] = offsetsId;
        operandIdCount++;
    }

    IlcSpvId resourceId = ilcSpvPutLoad(compiler->module, resource->typeId, resource->id);
    IlcSpvId samplerTypeId = ilcSpvPutSamplerType(compiler->module);
    IlcSpvId samplerId = ilcSpvPutLoad(compiler->module, samplerTypeId, sampler->id);
    IlcSpvId sampledImageTypeId = ilcSpvPutSampledImageType(compiler->module, resource->typeId);
    IlcSpvId sampledImageId = ilcSpvPutSampledImage(compiler->module, sampledImageTypeId,
                                                    resourceId, samplerId);
    if (drefId == 0) {
        IlcSpvId sampleId = ilcSpvPutImageSample(compiler->module, sampleOp, resource->texelTypeId,
                                                 sampledImageId, coordinateId, drefId, operandsMask,
                                                 operandIdCount, operandIds);
        storeDestination(compiler, dst, sampleId, resource->texelTypeId);
    } else {
        // Store the scalar result in dst.x
        IlcSpvId sampleId = ilcSpvPutImageSample(compiler->module, sampleOp, compiler->floatId,
                                                 sampledImageId, coordinateId, drefId, operandsMask,
                                                 operandIdCount, operandIds);
        sampleId = emitVectorGrow(compiler, sampleId, compiler->floatId, 1);
        storeDestination(compiler, dst, sampleId, compiler->float4Id);
    }
}

static void emitFetch4(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint8_t ilResourceId = GET_BITS(instr->control, 0, 7);
    uint8_t ilSamplerId = GET_BITS(instr->control, 8, 11);
    // TODO handle indexed args

    const IlcResource* resource = findResource(compiler, RES_TYPE_GENERIC, ilResourceId);
    const IlcSampler* sampler = findOrCreateSampler(compiler, ilSamplerId);
    const Destination* dst = &instr->dsts[0];

    if (resource == NULL) {
        LOGE("resource %d not found\n", ilResourceId);
        return;
    }

    unsigned dimCount = getResourceDimensionCount(resource->ilType);
    IlcSpvWord sampleOp = 0;
    IlcSpvId coordinateId = loadSource(compiler, &instr->srcs[0], COMP_MASK_XYZW,
                                       compiler->float4Id);
    IlcSpvId argId = 0;
    SpvImageOperandsMask operandsMask = 0;
    unsigned operandIdCount = 0;
    IlcSpvId operandIds[2];

    if (instr->opcode == IL_OP_FETCH4) {
        sampleOp = SpvOpImageGather;
        // Component index is stored in optional primary modifier
        argId = ilcSpvPutConstant(compiler->module, compiler->intId, instr->primModifier);
    } else if (instr->opcode == IL_OP_FETCH4_C) {
        sampleOp = SpvOpImageDrefGather;
        IlcSpvId drefId = loadSource(compiler, &instr->srcs[1], COMP_MASK_XYZW, compiler->float4Id);
        argId = emitVectorTrim(compiler, drefId, compiler->float4Id, COMP_INDEX_X, 1);
    } else {
        assert(false);
    }

    if (instr->addressOffset != 0) {
        float u = (int8_t)GET_BITS(instr->addressOffset, 0, 7) / 2.f;
        float v = (int8_t)GET_BITS(instr->addressOffset, 8, 15) / 2.f;
        float w = (int8_t)GET_BITS(instr->addressOffset, 16, 23) / 2.f;
        if (u != (int)u || v != (int)v || w != (int)w) {
            LOGW("non-integer offsets %g %g %g\n", u, v, w);
        }
        operandsMask |= SpvImageOperandsConstOffsetMask;
        const IlcSpvId constantIds[] = {
            ilcSpvPutConstant(compiler->module, compiler->intId, u),
            ilcSpvPutConstant(compiler->module, compiler->intId, v),
            ilcSpvPutConstant(compiler->module, compiler->intId, w),
        };
        IlcSpvId offsetsTypeId = ilcSpvPutVectorType(compiler->module, compiler->intId, dimCount);
        IlcSpvId offsetsId = ilcSpvPutConstantComposite(compiler->module, offsetsTypeId,
                                                        dimCount, constantIds);
        operandIds[0] = offsetsId;
        operandIdCount++;
    }

    IlcSpvId resourceId = ilcSpvPutLoad(compiler->module, resource->typeId, resource->id);
    IlcSpvId samplerTypeId = ilcSpvPutSamplerType(compiler->module);
    IlcSpvId samplerId = ilcSpvPutLoad(compiler->module, samplerTypeId, sampler->id);
    IlcSpvId sampledImageTypeId = ilcSpvPutSampledImageType(compiler->module, resource->typeId);
    IlcSpvId sampledImageId = ilcSpvPutSampledImage(compiler->module, sampledImageTypeId,
                                                    resourceId, samplerId);
    IlcSpvId sampleId = ilcSpvPutImageSample(compiler->module, sampleOp, resource->texelTypeId,
                                             sampledImageId, coordinateId, argId, operandsMask,
                                             operandIdCount, operandIds);
    storeDestination(compiler, dst, sampleId, resource->texelTypeId);
}

static void emitLdsLoadVec(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint8_t ilResourceId = GET_BITS(instr->control, 0, 14);

    const IlcResource* resource = findResource(compiler, RES_TYPE_LDS, ilResourceId);
    const Destination* dst = &instr->dsts[0];

    if (resource == NULL) {
        LOGE("resource %d not found\n", ilResourceId);
        return;
    }

    IlcSpvId index4Id = loadSource(compiler, &instr->srcs[0], COMP_MASK_XYZW, compiler->int4Id);
    IlcSpvId offset4Id = loadSource(compiler, &instr->srcs[1], COMP_MASK_XYZW, compiler->int4Id);
    IlcSpvId indexId = emitVectorTrim(compiler, index4Id, compiler->int4Id, COMP_INDEX_X, 1);
    IlcSpvId offsetId = emitVectorTrim(compiler, offset4Id, compiler->int4Id, COMP_INDEX_X, 1);

    // addr = (index * stride + offset) / 4
    IlcSpvId baseId = ilcSpvPutOp2(compiler->module, SpvOpIMul, compiler->intId,
                                   indexId, resource->strideId);
    IlcSpvId byteAddrId = ilcSpvPutOp2(compiler->module, SpvOpIAdd, compiler->intId,
                                       baseId, offsetId);
    IlcSpvId fourId = ilcSpvPutConstant(compiler->module, compiler->intId, 4);
    IlcSpvId wordAddrId = ilcSpvPutOp2(compiler->module, SpvOpSDiv, compiler->intId,
                                       byteAddrId, fourId);

    IlcSpvId oneId = ilcSpvPutConstant(compiler->module, compiler->intId, 1);
    IlcSpvId ptrTypeId = ilcSpvPutPointerType(compiler->module, SpvStorageClassWorkgroup,
                                              resource->texelTypeId);

    IlcSpvId componentIds[4];
    for (unsigned i = 0; i < 4; i++) {
        if (i > 0) {
            // Increment address
            wordAddrId = ilcSpvPutOp2(compiler->module, SpvOpIAdd, compiler->intId,
                                      wordAddrId, oneId);
        }

        IlcSpvId ptrId = ilcSpvPutAccessChain(compiler->module, ptrTypeId, resource->id,
                                              1, &wordAddrId);
        componentIds[i] = ilcSpvPutLoad(compiler->module, resource->texelTypeId, ptrId);
    }

    IlcSpvId resTypeId = ilcSpvPutVectorType(compiler->module, resource->texelTypeId, 4);
    IlcSpvId resId = ilcSpvPutCompositeConstruct(compiler->module, resTypeId, 4, componentIds);
    storeDestination(compiler, dst, resId, resTypeId);
}

static void emitLdsStoreVec(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint8_t ilResourceId = GET_BITS(instr->control, 0, 14);

    const IlcResource* resource = findResource(compiler, RES_TYPE_LDS, ilResourceId);
    const Destination* dst = &instr->dsts[0];

    if (resource == NULL) {
        LOGE("resource %d not found\n", ilResourceId);
        return;
    }

    IlcSpvId index4Id = loadSource(compiler, &instr->srcs[0], COMP_MASK_XYZW, compiler->int4Id);
    IlcSpvId offset4Id = loadSource(compiler, &instr->srcs[1], COMP_MASK_XYZW, compiler->int4Id);
    IlcSpvId dataId = loadSource(compiler, &instr->srcs[2], COMP_MASK_XYZW, compiler->uint4Id);
    IlcSpvId indexId = emitVectorTrim(compiler, index4Id, compiler->int4Id, COMP_INDEX_X, 1);
    IlcSpvId offsetId = emitVectorTrim(compiler, offset4Id, compiler->int4Id, COMP_INDEX_X, 1);

    // addr = (index * stride + offset) / 4
    IlcSpvId baseId = ilcSpvPutOp2(compiler->module, SpvOpIMul, compiler->intId,
                                   indexId, resource->strideId);
    IlcSpvId byteAddrId = ilcSpvPutOp2(compiler->module, SpvOpIAdd, compiler->intId,
                                       baseId, offsetId);
    IlcSpvId fourId = ilcSpvPutConstant(compiler->module, compiler->intId, 4);
    IlcSpvId wordAddrId = ilcSpvPutOp2(compiler->module, SpvOpSDiv, compiler->intId,
                                       byteAddrId, fourId);

    IlcSpvId ptrTypeId = ilcSpvPutPointerType(compiler->module, SpvStorageClassWorkgroup,
                                              resource->texelTypeId);

    // Write up to four components based on the destination mask
    for (unsigned i = 0; i < 4; i++) {
        if (dst->component[i] == IL_MODCOMP_NOWRITE) {
            continue;
        }

        IlcSpvId addrId;
        if (i > 0) {
            // Increment address
            IlcSpvId offsetId = ilcSpvPutConstant(compiler->module, compiler->intId, i);
            addrId = ilcSpvPutOp2(compiler->module, SpvOpIAdd, compiler->intId,
                                  wordAddrId, offsetId);
        } else {
            addrId = wordAddrId;
        }

        IlcSpvId ptrId = ilcSpvPutAccessChain(compiler->module, ptrTypeId, resource->id,
                                              1, &addrId);
        IlcSpvId componentId = emitVectorTrim(compiler, dataId, compiler->uint4Id, i, 1);
        ilcSpvPutStore(compiler->module, ptrId, componentId);
    }
}

static void emitUavLoad(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint8_t ilResourceId = GET_BITS(instr->control, 0, 14);

    const IlcResource* resource = findResource(compiler, RES_TYPE_GENERIC, ilResourceId);
    const Destination* dst = &instr->dsts[0];

    if (resource == NULL) {
        LOGE("resource %d not found\n", ilResourceId);
        return;
    }

    // Vulkan spec: "The Result Type operand of OpImageRead must be a vector of four components."
    IlcSpvId texel4TypeId = ilcSpvPutVectorType(compiler->module, resource->texelTypeId, 4);
    IlcSpvId resourceId = ilcSpvPutLoad(compiler->module, resource->typeId, resource->id);
    IlcSpvId addressId = loadSource(compiler, &instr->srcs[0], COMP_MASK_XYZW, compiler->int4Id);
    IlcSpvId readId = ilcSpvPutImageRead(compiler->module, texel4TypeId, resourceId, addressId);
    storeDestination(compiler, dst, readId, texel4TypeId);
}

static void emitUavStore(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint8_t ilResourceId = GET_BITS(instr->control, 0, 14);

    const IlcResource* resource = findResource(compiler, RES_TYPE_GENERIC, ilResourceId);

    if (resource == NULL) {
        LOGE("resource %d not found\n", ilResourceId);
        return;
    }

    IlcSpvId resourceId = ilcSpvPutLoad(compiler->module, resource->typeId, resource->id);
    IlcSpvId addressId = loadSource(compiler, &instr->srcs[0], COMP_MASK_XYZW, compiler->int4Id);
    IlcSpvId elementTypeId = ilcSpvPutVectorType(compiler->module, resource->texelTypeId, 4);
    IlcSpvId elementId = loadSource(compiler, &instr->srcs[1], COMP_MASK_XYZW, elementTypeId);

    ilcSpvPutImageWrite(compiler->module, resourceId, addressId, elementId);
}

static void emitStructUavStore(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint8_t ilResourceId = GET_BITS(instr->control, 0, 14);

    const IlcResource* resource = findResource(compiler, RES_TYPE_GENERIC, ilResourceId);
    const Destination* dst = &instr->dsts[0];

    if (resource == NULL) {
        LOGE("resource %d not found\n", ilResourceId);
        return;
    }

    //IlcSpvId resourceId = ilcSpvPutLoad(compiler->module, resource->typeId, resource->id);
    IlcSpvId srcId = loadSource(compiler, &instr->srcs[0], COMP_MASK_XYZW, compiler->int4Id);
    IlcSpvId indexId = emitVectorTrim(compiler, srcId, compiler->int4Id, COMP_INDEX_X, 1);
    IlcSpvId offsetId = emitVectorTrim(compiler, srcId, compiler->int4Id, COMP_INDEX_Y, 1);

    IlcSpvId elementTypeId = ilcSpvPutVectorType(compiler->module, resource->texelTypeId, 4);
    IlcSpvId elementId = loadSource(compiler, &instr->srcs[1], COMP_MASK_XYZW, elementTypeId);

    // addr = (index * stride + offset) / 4
    IlcSpvId baseId = ilcSpvPutOp2(compiler->module, SpvOpIMul, compiler->intId, indexId, resource->strideId);

    IlcSpvId byteAddrId = ilcSpvPutOp2(compiler->module, SpvOpIAdd, compiler->intId, baseId, offsetId);
    IlcSpvId wordAddrId = ilcSpvPutOp2(compiler->module, SpvOpSDiv, compiler->intId, byteAddrId, ilcSpvPutConstant(compiler->module, compiler->intId, 4));

    IlcSpvId zeroId = ilcSpvPutConstant(compiler->module, compiler->intId, 0);
    IlcSpvId ptrTypeId = ilcSpvPutPointerType(compiler->module, SpvStorageClassStorageBuffer,
                                              resource->texelTypeId);
    // Write up to four components based on the destination mask
    for (unsigned i = 0; i < 4; i++) {
        if (dst->component[i] == IL_MODCOMP_NOWRITE) {
            continue;
        }

        IlcSpvId addrId;
        if (i > 0) {
            // calculate address
            IlcSpvId offsetId = ilcSpvPutConstant(compiler->module, compiler->intId, i);
            addrId = ilcSpvPutOp2(compiler->module, SpvOpIAdd, compiler->intId,
                                  wordAddrId, offsetId);
        } else {
            addrId = wordAddrId;
        }

        IlcSpvId indexIds[] = { zeroId, addrId };
        IlcSpvId ptrId = ilcSpvPutAccessChain(compiler->module, ptrTypeId, resource->id,
                                              2, indexIds);
        IlcSpvId componentId = emitVectorTrim(compiler, elementId, elementTypeId, i, 1);
        ilcSpvPutStore(compiler->module, ptrId, componentId);
    }
}

static void emitLdsAtomicOp(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint8_t ilResourceId = GET_BITS(instr->control, 0, 4);

    const IlcResource* resource = findResource(compiler, RES_TYPE_LDS, ilResourceId);

    if (resource == NULL) {
        LOGE("resource %d not found\n", ilResourceId);
        return;
    }

    IlcSpvId pointerTypeId = ilcSpvPutPointerType(compiler->module, SpvStorageClassWorkgroup,
                                                  resource->texelTypeId);
    IlcSpvId addressId = loadSource(compiler, &instr->srcs[0], COMP_MASK_XYZW, compiler->int4Id);
    IlcSpvId byteAddrId;
    if (resource->structured) {
        IlcSpvId indexId = emitVectorTrim(compiler, addressId, compiler->int4Id, COMP_INDEX_X, 1);
        IlcSpvId offsetId = emitVectorTrim(compiler, addressId, compiler->int4Id, COMP_INDEX_Y, 1);
        // addr = (index * stride + offset) / 4
        IlcSpvId baseId = ilcSpvPutOp2(compiler->module, SpvOpIMul, compiler->intId,
                                       indexId, resource->strideId);
        byteAddrId = ilcSpvPutOp2(compiler->module, SpvOpIAdd, compiler->intId,
            baseId, offsetId);
    } else {
        byteAddrId = emitVectorTrim(compiler, addressId, compiler->int4Id, COMP_INDEX_X, 1);
    }
    IlcSpvId wordAddrId = ilcSpvPutOp2(compiler->module, SpvOpSDiv, compiler->intId,
                                       byteAddrId, ilcSpvPutConstant(compiler->module, compiler->intId, 4));
    IlcSpvId bufferPtrId = ilcSpvPutAccessChain(compiler->module, pointerTypeId, resource->id,
                                      1, &wordAddrId);
    IlcSpvId readId = 0;
    IlcSpvId vecTypeId = ilcSpvPutVectorType(compiler->module, resource->texelTypeId, 4);
    IlcSpvId scopeId = ilcSpvPutConstant(compiler->module, compiler->intId, SpvScopeWorkgroup);
    IlcSpvId semanticsId = ilcSpvPutConstant(compiler->module, compiler->intId,
                                             SpvMemorySemanticsAcquireReleaseMask |
                                             SpvMemorySemanticsSubgroupMemoryMask);
    IlcSpvId src1Id = loadSource(compiler, &instr->srcs[1], COMP_MASK_XYZW, vecTypeId);
    IlcSpvId valueId = emitVectorTrim(compiler, src1Id, vecTypeId, COMP_INDEX_X, 1);

    if (instr->opcode == IL_OP_LDS_ADD || instr->opcode == IL_OP_LDS_READ_ADD) {
        readId = ilcSpvPutAtomicOp(compiler->module, SpvOpAtomicIAdd, resource->texelTypeId,
                                   bufferPtrId, scopeId, semanticsId, valueId);
    } else if (instr->opcode == IL_OP_LDS_UMAX || instr->opcode == IL_OP_LDS_READ_UMAX) {
        readId = ilcSpvPutAtomicOp(compiler->module, SpvOpAtomicUMax, resource->texelTypeId,
                                   bufferPtrId, scopeId, semanticsId, valueId);
    } else {
        assert(false);
    }

    if (instr->dstCount > 0) {
        IlcSpvId resId = emitVectorGrow(compiler, readId, resource->texelTypeId, 1);
        storeDestination(compiler, &instr->dsts[0], resId, vecTypeId);
    }
}

static void emitUavAtomicOp(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint8_t ilResourceId = GET_BITS(instr->control, 0, 14);

    const IlcResource* resource = findResource(compiler, RES_TYPE_GENERIC, ilResourceId);

    if (resource == NULL) {
        LOGE("resource %d not found\n", ilResourceId);
        return;
    }

    IlcSpvId texelPtrId ;

    IlcSpvId semanticsId;
    if (resource->strideId == 0) {
        IlcSpvId pointerTypeId = ilcSpvPutPointerType(compiler->module, SpvStorageClassImage,
                                                      resource->texelTypeId);
        IlcSpvId addressId = loadSource(compiler, &instr->srcs[0], COMP_MASK_XYZW, compiler->int4Id);
        IlcSpvId trimAddressId = emitVectorTrim(compiler, addressId, compiler->int4Id, COMP_INDEX_X,
                                                getResourceDimensionCount(resource->ilType));
        IlcSpvId zeroId = ilcSpvPutConstant(compiler->module, compiler->intId, ZERO_LITERAL);
        texelPtrId = ilcSpvPutImageTexelPointer(compiler->module, pointerTypeId, resource->id,
                                                trimAddressId, zeroId);
        semanticsId = ilcSpvPutConstant(compiler->module, compiler->intId,
                                        SpvMemorySemanticsAcquireReleaseMask |
                                        SpvMemorySemanticsImageMemoryMask);
    } else {
        IlcSpvId pointerTypeId = ilcSpvPutPointerType(compiler->module, SpvStorageClassStorageBuffer,
                                                      resource->texelTypeId);
        IlcSpvId addressId = loadSource(compiler, &instr->srcs[0], COMP_MASK_XYZW, compiler->int4Id);
        IlcSpvId byteAddrId;
        if (resource->structured) {
            IlcSpvId indexId = emitVectorTrim(compiler, addressId, compiler->int4Id, COMP_INDEX_X, 1);
            IlcSpvId offsetId = emitVectorTrim(compiler, addressId, compiler->int4Id, COMP_INDEX_Y, 1);
            // addr = (index * stride + offset) / 4
            IlcSpvId baseId = ilcSpvPutOp2(compiler->module, SpvOpIMul, compiler->intId, indexId, resource->strideId);
            byteAddrId = ilcSpvPutOp2(compiler->module, SpvOpIAdd, compiler->intId, baseId, offsetId);
        } else {
            byteAddrId = emitVectorTrim(compiler, addressId, compiler->int4Id, COMP_INDEX_X, 1);
        }
        IlcSpvId zeroId = ilcSpvPutConstant(compiler->module, compiler->intId, ZERO_LITERAL);
        IlcSpvId wordAddrId = ilcSpvPutOp2(compiler->module, SpvOpSDiv, compiler->intId,
                                           byteAddrId, ilcSpvPutConstant(compiler->module, compiler->intId, 4));
        const IlcSpvId indexIds[] = { zeroId, wordAddrId };
        texelPtrId = ilcSpvPutAccessChain(compiler->module, pointerTypeId, resource->id,
                                          2, indexIds);
        semanticsId = ilcSpvPutConstant(compiler->module, compiler->intId,
                                        SpvMemorySemanticsAcquireReleaseMask |
                                        SpvMemorySemanticsUniformMemoryMask);
    }
    IlcSpvId readId = 0;
    IlcSpvId vecTypeId = ilcSpvPutVectorType(compiler->module, resource->texelTypeId, 4);
    IlcSpvId scopeId = ilcSpvPutConstant(compiler->module, compiler->intId, SpvScopeDevice);
    IlcSpvId src1Id = loadSource(compiler, &instr->srcs[1], COMP_MASK_XYZW, vecTypeId);
    IlcSpvId valueId = emitVectorTrim(compiler, src1Id, vecTypeId, COMP_INDEX_X, 1);

    if (instr->opcode == IL_OP_UAV_ADD || instr->opcode == IL_OP_UAV_READ_ADD) {
        readId = ilcSpvPutAtomicOp(compiler->module, SpvOpAtomicIAdd, resource->texelTypeId,
                                   texelPtrId, scopeId, semanticsId, valueId);
    } else if (instr->opcode == IL_OP_UAV_UMAX || instr->opcode == IL_OP_UAV_READ_UMAX) {
        readId = ilcSpvPutAtomicOp(compiler->module, SpvOpAtomicUMax, resource->texelTypeId,
                                   texelPtrId, scopeId, semanticsId, valueId);
    } else {
        assert(false);
    }

    if (instr->dstCount > 0) {
        IlcSpvId resId = emitVectorGrow(compiler, readId, resource->texelTypeId, 1);
        storeDestination(compiler, &instr->dsts[0], resId, vecTypeId);
    }
}

static void emitAppendBufOp(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint16_t ilResourceId = GET_BITS(instr->control, 0, 14);

    const IlcResource* resource = findResource(compiler, RES_TYPE_ATOMIC_COUNTER, 0);
    const Destination* dst = &instr->dsts[0];

    if (resource == NULL) {
        // Lazily declare atomic counter buffer
        IlcSpvId arrayId = ilcSpvPutRuntimeArrayType(compiler->module, compiler->uintId, true);
        IlcSpvId structId = ilcSpvPutStructType(compiler->module, 1, &arrayId);
        IlcSpvId pointerId = ilcSpvPutPointerType(compiler->module, SpvStorageClassStorageBuffer,
                                                  structId);
        IlcSpvId resourceId = ilcSpvPutVariable(compiler->module, pointerId,
                                                SpvStorageClassStorageBuffer);

        IlcSpvWord arrayStride = sizeof(uint32_t);
        IlcSpvWord memberOffset = 0;
        ilcSpvPutDecoration(compiler->module, arrayId, SpvDecorationArrayStride, 1, &arrayStride);
        ilcSpvPutDecoration(compiler->module, structId, SpvDecorationBlock, 0, NULL);
        ilcSpvPutMemberDecoration(compiler->module, structId, 0, SpvDecorationOffset,
                                  1, &memberOffset);

        ilcSpvPutName(compiler->module, arrayId, "atomicCounter");
        emitBinding(compiler, resourceId, ILC_ATOMIC_COUNTER_ID, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    NO_STRIDE_INDEX);

        const IlcResource atomicCounterResource = {
            .resType = RES_TYPE_ATOMIC_COUNTER,
            .id = resourceId,
            .typeId = 0,
            .texelTypeId = 0,
            .ilId = 0,
            .ilType = IL_USAGE_PIXTEX_UNKNOWN,
            .strideId = 0,
        };

        resource = addResource(compiler, &atomicCounterResource);
    }

    bool useSubgroupOps = compiler->kernel->shaderType == IL_SHADER_COMPUTE;
    IlcSpvId electBlockBeginId;
    IlcSpvId electBlockEndId;
    IlcSpvId preElectBlockLabelId;
    IlcSpvId workgroupScopeId;
    IlcSpvId laneCountId, laneIndexId;
    if (useSubgroupOps) {
        preElectBlockLabelId = getTopBlockLabel(compiler);
        electBlockBeginId = ilcSpvAllocId(compiler->module);
        electBlockEndId = ilcSpvAllocId(compiler->module);

        ilcSpvPutCapability(compiler->module, SpvCapabilityGroupNonUniform);
        ilcSpvPutCapability(compiler->module, SpvCapabilityGroupNonUniformBallot);
        workgroupScopeId = ilcSpvPutConstant(compiler->module, compiler->intId, SpvScopeWorkgroup);

        IlcSpvId ballotId = ilcSpvPutGroupNonUniformBallot(compiler->module, compiler->uint4Id, workgroupScopeId, ilcSpvPutConstantTrue(compiler->module, compiler->boolId));
        laneCountId = ilcSpvPutGroupNonUniformBallotBitCount(compiler->module, compiler->uintId, workgroupScopeId, SpvGroupOperationReduce, ballotId);
        laneIndexId = ilcSpvPutGroupNonUniformBallotBitCount(compiler->module, compiler->uintId, workgroupScopeId, SpvGroupOperationExclusiveScan, ballotId);
        IlcSpvId electionCondId = ilcSpvPutGroupNonUniformElect(compiler->module, compiler->boolId, workgroupScopeId);
        ilcSpvPutSelectionMerge(compiler->module, electBlockEndId);
        ilcSpvPutBranchConditional(compiler->module, electionCondId, electBlockBeginId, electBlockEndId);
        ilcSpvPutLabel(compiler->module, electBlockBeginId);
    } else {
        laneCountId = ilcSpvPutConstant(compiler->module, compiler->uintId, 1u);
    }
    IlcSpvId ptrTypeId = ilcSpvPutPointerType(compiler->module, SpvStorageClassStorageBuffer,
                                              compiler->uintId);
    IlcSpvId zeroId = ilcSpvPutConstant(compiler->module, compiler->intId, ZERO_LITERAL);
    IlcSpvId counterIndexId = ilcSpvPutConstant(compiler->module, compiler->intId, ilResourceId);
    IlcSpvId indexIds[] = { zeroId, counterIndexId };
    IlcSpvId ptrId = ilcSpvPutAccessChain(compiler->module, ptrTypeId, resource->id,
                                          2, indexIds);
    IlcSpvId scopeId = ilcSpvPutConstant(compiler->module, compiler->intId, SpvScopeDevice);
    IlcSpvId semanticsId = ilcSpvPutConstant(compiler->module, compiler->intId,
                                             SpvMemorySemanticsAcquireReleaseMask |
                                             SpvMemorySemanticsUniformMemoryMask);
    IlcSpvId readId;
    SpvOp op = instr->opcode == IL_OP_APPEND_BUF_ALLOC ? SpvOpAtomicIAdd : SpvOpAtomicISub;

    readId = ilcSpvPutAtomicOp(compiler->module, op, compiler->uintId, ptrId,
                               scopeId, semanticsId, laneCountId);

    if (useSubgroupOps) {
        ilcSpvPutBranch(compiler->module, electBlockEndId);
        ilcSpvPutLabel(compiler->module, electBlockEndId);

        IlcSpvId constUndefId = ilcSpvPutConstantUndef(compiler->module, compiler->uintId);
        IlcSpvId phiLabels[4] = {
            readId, electBlockBeginId,
            constUndefId, preElectBlockLabelId,
        };
        readId = ilcSpvPutPhi(compiler->module, compiler->uintId, 4, phiLabels);
        readId = ilcSpvPutGroupNonUniformBroadcastFirst(compiler->module, compiler->uintId, workgroupScopeId, readId);
        readId = ilcSpvPutOp2(compiler->module, instr->opcode == IL_OP_APPEND_BUF_ALLOC ? SpvOpIAdd : SpvOpISub, compiler->uintId, readId, laneIndexId);
    }

    IlcSpvId resId = emitVectorGrow(compiler, readId, compiler->uintId, 1);
    storeDestination(compiler, dst, resId, compiler->uint4Id);
}

static void emitStructuredSrvLoad(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint8_t ilResourceId = GET_BITS(instr->control, 0, 7);
    bool indexedResourceId = GET_BIT(instr->control, 12);
    bool primModifierPresent = GET_BIT(instr->control, 15);

    if (indexedResourceId) {
        LOGW("unhandled indexed resource ID\n");
    }

    unsigned wordCount = 4;
    unsigned elemCount = 1; // Elements per word
    uint8_t numFormat = IL_BUF_NUM_FMT_FLOAT;

    if (primModifierPresent) {
        uint8_t dfmt = GET_BITS(instr->primModifier, 0, 3);
        uint8_t nfmt = GET_BITS(instr->primModifier, 4, 7);

        switch (dfmt) {
        case IL_BUF_DATA_FMT_32:            wordCount = 1; elemCount = 1; break;
        case IL_BUF_DATA_FMT_16_16:         wordCount = 1; elemCount = 2; break;
        case IL_BUF_DATA_FMT_8_8_8_8:       wordCount = 1; elemCount = 4; break;
        case IL_BUF_DATA_FMT_32_32:         wordCount = 2; elemCount = 1; break;
        case IL_BUF_DATA_FMT_16_16_16_16:   wordCount = 2; elemCount = 2; break;
        case IL_BUF_DATA_FMT_32_32_32:      wordCount = 3; elemCount = 1; break;
        case IL_BUF_DATA_FMT_32_32_32_32:   wordCount = 4; elemCount = 1; break;
        default:
            LOGE("unhandled SRV data format %u %u\n", dfmt, nfmt);
            assert(false);
        }

        numFormat = nfmt;
    }

    const IlcResource* resource = findResource(compiler, RES_TYPE_GENERIC, ilResourceId);
    const Destination* dst = &instr->dsts[0];

    if (resource == NULL) {
        LOGE("resource %d not found\n", ilResourceId);
        return;
    }

    IlcSpvId srcId = loadSource(compiler, &instr->srcs[0], COMP_MASK_XYZW, compiler->int4Id);
    IlcSpvId indexId = emitVectorTrim(compiler, srcId, compiler->int4Id, COMP_INDEX_X, 1);
    IlcSpvId offsetId = emitVectorTrim(compiler, srcId, compiler->int4Id, COMP_INDEX_Y, 1);

    // addr = (index * stride + offset) / 4
    IlcSpvId baseId = ilcSpvPutOp2(compiler->module, SpvOpIMul, compiler->intId,
                                   indexId, resource->strideId);
    IlcSpvId byteAddrId = ilcSpvPutOp2(compiler->module, SpvOpIAdd, compiler->intId,
                                       baseId, offsetId);
    IlcSpvId fourId = ilcSpvPutConstant(compiler->module, compiler->intId, 4);
    IlcSpvId wordAddrId = ilcSpvPutOp2(compiler->module, SpvOpSDiv, compiler->intId,
                                       byteAddrId, fourId);

    // Read data words as 32-bit floats
    // TODO redeclare resources based on type to avoid type conversions
    IlcSpvId zeroId = ilcSpvPutConstant(compiler->module, compiler->intId, ZERO_LITERAL);
    IlcSpvId ptrTypeId = ilcSpvPutPointerType(compiler->module, SpvStorageClassStorageBuffer,
                                              resource->texelTypeId);
    IlcSpvId fZeroId = ilcSpvPutConstant(compiler->module, compiler->floatId, ZERO_LITERAL);
    IlcSpvId fWordIds[] = { fZeroId, fZeroId, fZeroId, fZeroId };

    for (unsigned i = 0; i < wordCount; i++) {
        IlcSpvId addrId;

        if (elemCount == 1 && dst->component[i] == IL_MODCOMP_NOWRITE) {
            // Skip read if each element is a whole word and this component is not written to dst
            continue;
        }

        if (i == 0) {
            addrId = wordAddrId;
        } else {
            IlcSpvId offsetId = ilcSpvPutConstant(compiler->module, compiler->intId, i);
            addrId = ilcSpvPutOp2(compiler->module, SpvOpIAdd, compiler->intId,
                                  wordAddrId, offsetId);
        }

        const IlcSpvId indexIds[] = { zeroId, addrId };
        IlcSpvId ptrId = ilcSpvPutAccessChain(compiler->module, ptrTypeId, resource->id,
                                              2, indexIds);
        fWordIds[i] = ilcSpvPutLoad(compiler->module, resource->texelTypeId, ptrId);
    }

    IlcSpvId resId = 0;

    if ((numFormat == IL_BUF_NUM_FMT_UNORM && elemCount > 1) ||
        (numFormat == IL_BUF_NUM_FMT_SNORM && elemCount > 1) ||
        (numFormat == IL_BUF_NUM_FMT_FLOAT && elemCount > 1)) {
        // Unpack words into 2 or 4 float values each
        IlcSpvWord op = 0;
        IlcSpvId opResTypeId = ilcSpvPutVectorType(compiler->module, compiler->floatId, elemCount);
        IlcSpvId opResIds[2] = { 0 };

        if (numFormat == IL_BUF_NUM_FMT_UNORM) {
            op = elemCount == 4 ? GLSLstd450UnpackUnorm4x8 : GLSLstd450UnpackUnorm2x16;
        } else if (numFormat == IL_BUF_NUM_FMT_SNORM) {
            op = elemCount == 4 ? GLSLstd450UnpackSnorm4x8 : GLSLstd450UnpackSnorm2x16;
        } else if (numFormat == IL_BUF_NUM_FMT_FLOAT && elemCount == 2) {
            op = GLSLstd450UnpackHalf2x16;
        } else {
            assert(false);
        }

        for (unsigned i = 0; i < wordCount; i++) {
            IlcSpvId wordId = ilcSpvPutBitcast(compiler->module, compiler->intId, fWordIds[i]);
            opResIds[i] = ilcSpvPutGLSLOp(compiler->module, op, opResTypeId, 1, &wordId);
        }

        if (wordCount == 1 && elemCount == 4) {
            // 1x4, copy
            resId = opResIds[0];
        } else if (wordCount == 1 && elemCount < 4) {
            // 1x2, grow to 1x4
            resId = emitVectorGrow(compiler, opResIds[0], compiler->floatId, elemCount);
        } else {
            // 2x2, merge into 1x4
            const IlcSpvWord components[] = { 0, 1, 2, 3 };
            resId = ilcSpvPutVectorShuffle(compiler->module, compiler->float4Id,
                                           opResIds[0], opResIds[1], 4, components);
        }
    } else if ((numFormat == IL_BUF_NUM_FMT_UINT && elemCount > 1) ||
               (numFormat == IL_BUF_NUM_FMT_SINT && elemCount > 1)) {
        // Extract words into 2 or 4 integer values each
        unsigned elemWidth = 32 / elemCount;
        bool isSigned = numFormat == IL_BUF_NUM_FMT_SINT;
        IlcSpvWord op = isSigned ? SpvOpBitFieldSExtract : SpvOpBitFieldUExtract;
        IlcSpvId widthId = ilcSpvPutConstant(compiler->module, compiler->intId, elemWidth);
        IlcSpvId compIds[4] = { 0 };

        unsigned compCount = 0;
        for (unsigned i = 0; i < wordCount; i++) {
            IlcSpvId wordId = ilcSpvPutBitcast(compiler->module, compiler->intId, fWordIds[i]);

            for (unsigned j = 0; j < elemCount; j++) {
                IlcSpvId offsetId = ilcSpvPutConstant(compiler->module, compiler->intId,
                                                      j * elemWidth);
                compIds[compCount] = ilcSpvPutOp3(compiler->module, op, compiler->intId,
                                                  wordId, offsetId, widthId);
                compCount++;
            }
        }

        IlcSpvId typeId = ilcSpvPutVectorType(compiler->module, compiler->intId, compCount);
        resId = ilcSpvPutCompositeConstruct(compiler->module, typeId, compCount, compIds);

        if (compCount < 4) {
            resId = emitVectorGrow(compiler, resId, compiler->intId, compCount);
        }

        resId = ilcSpvPutBitcast(compiler->module, compiler->float4Id, resId);
    } else if (numFormat == IL_BUF_NUM_FMT_UINT ||
               numFormat == IL_BUF_NUM_FMT_SINT ||
               numFormat == IL_BUF_NUM_FMT_FLOAT) {
        // Each word is a component of float4
        resId = ilcSpvPutCompositeConstruct(compiler->module, compiler->float4Id, 4, fWordIds);
    } else {
        LOGE("unhandled SRV format %ux%u %u\n", wordCount, elemCount, numFormat);
        assert(false);
    }

    storeDestination(compiler, dst, resId, compiler->float4Id);
}

static void finalizeVertexStage(
    IlcCompiler* compiler)
{
    const IlcRegister* posReg = findRegisterByType(compiler, IL_REGTYPE_OUTPUT, IL_IMPORTUSAGE_POS);
    if (posReg != NULL) {
        IlcSpvId outputId = emitVariable(compiler, posReg->typeId, SpvStorageClassOutput);
        IlcSpvId locationIdx = posReg->ilNum;
        ilcSpvPutDecoration(compiler->module, outputId, SpvDecorationLocation, 1, &locationIdx);
        ilcSpvPutDecoration(compiler->module, outputId, SpvDecorationInvariant, 0, NULL);
        IlcSpvId loadedPosId = ilcSpvPutLoad(compiler->module, posReg->typeId, posReg->id);
        ilcSpvPutStore(compiler->module, outputId, loadedPosId);
        const IlcRegister reg = {
            .id = outputId,
            .typeId = posReg->typeId,
            .vecTypeId = posReg->vecTypeId,
            .componentTypeId = posReg->componentTypeId,
            .componentCount = posReg->componentCount,
            .arrayItemCount = 0,
            .arrayItemOffset = 0,
            .duped = false,
            .ilType = IL_REGTYPE_OUTPUT,
            .ilNum = posReg->ilNum,//idk what to place here (not needed :) )
            .ilImportUsage = IL_IMPORTUSAGE_GENERIC,
            .ilInterpMode = 0,
        };

        addRegister(compiler, &reg, "oPos");
        // store output location
        emitGenericOutputInfo(compiler, locationIdx);
    }
}

static IlcSpvId createInvocationId(
    IlcCompiler* compiler)
{
    const IlcRegister* existingInvocationIdReg = findRegister(compiler, IL_REGTYPE_OUTPUTCP, 0);
    if (existingInvocationIdReg != NULL) {
        return existingInvocationIdReg->id;
    }
    //just for the moment there is no invocationId
    IlcSpvId invocationId = emitVariable(compiler, compiler->intId, SpvStorageClassInput);

    IlcSpvWord builtInType = SpvBuiltInInvocationId;
    ilcSpvPutDecoration(compiler->module, invocationId, SpvDecorationBuiltIn, 1, &builtInType);
    const IlcRegister invocationIdReg = {
        .id = invocationId,
        .typeId = compiler->intId,
        .vecTypeId = compiler->intId,
        .componentTypeId = compiler->intId,
        .componentCount = 1,
        .arrayItemCount = 0,
        .arrayItemOffset = 0,
        .duped = false,
        .ilType = IL_REGTYPE_OUTPUTCP,
        .ilNum = 0,//idk what to place here (not needed :) )
        .ilImportUsage = IL_IMPORTUSAGE_GENERIC,
        .ilInterpMode = 0,
    };

    addRegister(compiler, &invocationIdReg, "invocationId");
    return invocationId;
}

static void finalizeHullShader(
    IlcCompiler* compiler,
    IlcSpvId invocationId)
{
    if (!compiler->emitHullOutputFinalize) {
        return;
    }
    //TODO: also handle patches
    int maxVertRegIndex = -1;
    unsigned maxVertexDimension = compiler->inputControlPointSize;
    if (maxVertexDimension == 0) {
        LOGW("unhandled input control count\n");
        maxVertexDimension = 3;
    }
    IlcSpvId componentTypeId = compiler->floatId;
    unsigned componentCount = 4;
    IlcSpvId vecTypeId = compiler->float4Id;

    for (unsigned i = 0; i < compiler->regCount; ++i) {
        if (compiler->regs[i].ilType == IL_REGTYPE_INPUTCP && (int)compiler->regs[i].ilNum > maxVertRegIndex) {
            if (compiler->regs[i].id == 0) {
                LOGW("got empty reg %d %d %d\n", compiler->regs[i].ilNum, compiler->regs[i].ilType, compiler->regs[i].ilImportUsage);
            }
            maxVertRegIndex = compiler->regs[i].ilNum;
        }
    }

    if (maxVertRegIndex >= 0) {
        IlcSpvId lengthId = ilcSpvPutConstant(compiler->module, compiler->intId, maxVertRegIndex + 1);
        IlcSpvId vertexDimId = ilcSpvPutConstant(compiler->module, compiler->intId, maxVertexDimension);
        IlcSpvId arrayTypeId = ilcSpvPutArrayType(compiler->module, vecTypeId, lengthId);
        IlcSpvId arrayWrapperId = ilcSpvPutArrayType(compiler->module, arrayTypeId, vertexDimId);

        IlcSpvId outputId = emitVariable(compiler, arrayWrapperId, SpvStorageClassOutput);
        IlcSpvId locationIdx = 0;
        ilcSpvPutDecoration(compiler->module, outputId, SpvDecorationLocation, 1, &locationIdx);

        IlcSpvId inputPtrTypeId = ilcSpvPutPointerType(compiler->module, SpvStorageClassInput, vecTypeId);
        IlcSpvId outputPtrTypeId = ilcSpvPutPointerType(compiler->module, SpvStorageClassOutput, vecTypeId);
        for (unsigned i = 0; i < compiler->regCount; ++i) {
            if (compiler->regs[i].ilType != IL_REGTYPE_INPUTCP) {
                continue;
            }
            IlcSpvId inputIndexId = ilcSpvPutConstant(compiler->module, compiler->intId, i);
            IlcSpvId invocationValueId = ilcSpvPutLoad(compiler->module, compiler->intId, invocationId);

            IlcSpvId inputPtrId = ilcSpvPutAccessChain(compiler->module, inputPtrTypeId, compiler->regs[i].id, 1, &invocationValueId);
            IlcSpvId loadedInputId = ilcSpvPutLoad(compiler->module, vecTypeId, inputPtrId);
            IlcSpvId indexesId[] = {
                invocationValueId, inputIndexId
            };
            IlcSpvId dstId = ilcSpvPutAccessChain(compiler->module, outputPtrTypeId, outputId, 2, indexesId );
            ilcSpvPutStore(compiler->module, dstId, loadedInputId);
        }

        const IlcRegister reg = {
            .id = outputId,
            .typeId = arrayWrapperId,
            .vecTypeId = vecTypeId,
            .componentTypeId = componentTypeId,
            .componentCount = componentCount,
            .arrayItemCount = maxVertexDimension,
            .arrayItemOffset = 0,
            .duped = false,
            .ilType = IL_REGTYPE_OUTPUT,
            .ilNum = 0,//idk what to place here (not needed :) )
            .ilImportUsage = IL_IMPORTUSAGE_GENERIC,
            .ilInterpMode = 0,
        };

        addRegister(compiler, &reg, "vert_out");
    }
}

static void emitImplicitInput(
    IlcCompiler* compiler,
    SpvBuiltIn spvBuiltIn,
    uint32_t ilType,
    unsigned componentCount,
    const char* name)
{
    IlcSpvId componentTypeId = compiler->uintId;
    IlcSpvId inputTypeId;
    if (componentCount == 1) {
        inputTypeId = componentTypeId;
    } else {
        inputTypeId = ilcSpvPutVectorType(compiler->module, componentTypeId, componentCount);
    }
    IlcSpvId inputId = emitVariable(compiler, inputTypeId, SpvStorageClassInput);

    IlcSpvWord builtInType = spvBuiltIn;
    ilcSpvPutDecoration(compiler->module, inputId, SpvDecorationBuiltIn, 1, &builtInType);

    const IlcRegister reg = {
        .id = inputId,
        .typeId = inputTypeId,
        .vecTypeId = inputTypeId,
        .componentTypeId = componentTypeId,
        .componentCount = componentCount,
        .arrayItemCount = 0,
        .ilType = ilType,
        .ilNum = 0,
        .ilImportUsage = 0,
        .ilInterpMode = 0,
    };

    addRegister(compiler, &reg, name);
}

static void emitImplicitInputs(
    IlcCompiler* compiler)
{
    if (compiler->kernel->shaderType == IL_SHADER_COMPUTE) {
        // TODO declare on-demand
        emitImplicitInput(compiler, SpvBuiltInLocalInvocationId, IL_REGTYPE_THREAD_ID_IN_GROUP,
                          3, "vTidInGrp");
        emitImplicitInput(compiler, SpvBuiltInLocalInvocationIndex,
                          IL_REGTYPE_THREAD_ID_IN_GROUP_FLAT, 1, "vTidInGrpFlat");
        emitImplicitInput(compiler, SpvBuiltInGlobalInvocationId, IL_REGTYPE_ABSOLUTE_THREAD_ID,
                          3, "vAbsTid");
        emitImplicitInput(compiler, SpvBuiltInWorkgroupId, IL_REGTYPE_THREAD_GROUP_ID,
                          3, "vThreadGrpId");

    }
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
    case IL_OP_ROUND_NEAR:
    case IL_OP_ROUND_NEG_INF:
    case IL_OP_ROUND_PLUS_INF:
    case IL_OP_ROUND_ZERO:
    case IL_OP_EXP_VEC:
    case IL_OP_LOG_VEC:
    case IL_OP_RSQ_VEC:
    case IL_OP_SIN_VEC:
    case IL_OP_COS_VEC:
    case IL_OP_SQRT_VEC:
    case IL_OP_DP2:
    case IL_OP_F_2_F16:
    case IL_OP_F16_2_F:
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
    case IL_OP_I_XOR:
    case IL_OP_I_ADD:
    case IL_OP_I_MAD:
    case IL_OP_I_MAX:
    case IL_OP_I_MIN:
    case IL_OP_I_MUL:
    case IL_OP_I_NEGATE:
    case IL_OP_I_SHL:
    case IL_OP_I_SHR:
    case IL_OP_U_SHR:
    case IL_OP_U_DIV:
    case IL_OP_U_MOD:
    case IL_OP_U_MIN:
    case IL_OP_AND:
    case IL_OP_U_BIT_EXTRACT:
    case IL_OP_U_BIT_INSERT:
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
    case IL_OP_CONTINUE_LOGICALZ:
    case IL_OP_CONTINUE_LOGICALNZ:
        emitContinue(compiler, instr);
        break;
    case IL_OP_ELSE:
        emitElse(compiler, instr);
        break;
    case IL_OP_HS_CP_PHASE:
    case IL_OP_HS_FORK_PHASE:
    case IL_OP_HS_JOIN_PHASE:
        emitHullShaderPhase(compiler, instr);
        break;
    case IL_OP_ENDPHASE:
    case IL_OP_END:
    case IL_OP_ENDMAIN:
        if (compiler->isInFunction) {
            ilcSpvPutFunctionEnd(compiler->module);
            compiler->isInFunction = false;
            compiler->isAfterReturn = false;
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
    case IL_DCL_CONST_BUFFER:
        emitConstBuffer(compiler, instr);
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
    case IL_OP_SAMPLE_C_LZ:
        emitSample(compiler, instr);
        break;
    case IL_OP_FETCH4:
    case IL_OP_FETCH4_C:
        emitFetch4(compiler, instr);
        break;
    case IL_OP_CMOV_LOGICAL:
        emitCmovLogical(compiler, instr);
        break;
    case IL_OP_DCL_NUM_THREAD_PER_GROUP:
        emitNumThreadPerGroup(compiler, instr);
        break;
    case IL_DCL_TS_DOMAIN:
        emitTessDomain(compiler, instr);
        break;
    case IL_DCL_TS_OUTPUT_PRIMITIVE:
        emitTessOutputPrimitive(compiler, instr);
        break;
    case IL_DCL_TS_PARTITION:
        emitTessPartition(compiler, instr);
        break;
    case IL_DCL_NUM_ICP:
        emitNumInputControlPoints(compiler, instr);
        break;
    case IL_DCL_NUM_OCP:
        emitNumOutputControlPoints(compiler, instr);
        break;
    case IL_OP_FENCE:
        emitFence(compiler, instr);
        break;
    case IL_OP_LDS_LOAD_VEC:
        emitLdsLoadVec(compiler, instr);
        break;
    case IL_OP_LDS_STORE_VEC:
        emitLdsStoreVec(compiler, instr);
        break;
    case IL_OP_DCL_UAV:
    case IL_OP_DCL_TYPED_UAV:
        emitTypedUav(compiler, instr);
        break;
    case IL_OP_DCL_STRUCT_UAV:
    case IL_OP_DCL_TYPELESS_UAV:
    case IL_OP_DCL_RAW_UAV:
        emitUav(compiler, instr);
        break;
    case IL_OP_UAV_LOAD:
        emitUavLoad(compiler, instr);
        break;
    case IL_OP_UAV_STORE:
        emitUavStore(compiler, instr);
        break;
    case IL_OP_UAV_STRUCT_STORE:
        emitStructUavStore(compiler, instr);
        break;
    case IL_OP_UAV_ADD:
    case IL_OP_UAV_READ_ADD:
    case IL_OP_UAV_UMAX:
    case IL_OP_UAV_READ_UMAX:
        emitUavAtomicOp(compiler, instr);
        break;
    case IL_OP_APPEND_BUF_ALLOC:
    case IL_OP_APPEND_BUF_CONSUME:
        emitAppendBufOp(compiler, instr);
        break;
    case IL_OP_LDS_ADD:
    case IL_OP_LDS_READ_ADD:
    case IL_OP_LDS_UMAX:
    case IL_OP_LDS_READ_UMAX:
        emitLdsAtomicOp(compiler, instr);
        break;
    case IL_OP_DCL_RAW_SRV:
    case IL_OP_DCL_STRUCT_SRV:
        emitSrv(compiler, instr);
        break;
    case IL_OP_SRV_STRUCT_LOAD:
        emitStructuredSrvLoad(compiler, instr);
        break;
    case IL_DCL_STRUCT_LDS:
        emitStructuredLds(compiler, instr);
        break;
    case IL_DCL_LDS:
        emitLds(compiler, instr);
        break;
    case IL_DCL_GLOBAL_FLAGS:
        emitGlobalFlags(compiler, instr);
        break;
    case IL_UNK_660: {
        // FIXME seems to be some sort of vertex ID offset (as seen in 3DMark), store 0 for now
        IlcSpvId zeroId = ilcSpvPutConstant(compiler->module, compiler->intId, ZERO_LITERAL);
        IlcSpvId zero4Id = emitVectorGrow(compiler, zeroId, compiler->intId, 1);
        storeDestination(compiler, &instr->dsts[0], zero4Id, compiler->int4Id);
    }   break;
    default:
        LOGW("unhandled instruction %d\n", instr->opcode);
    }
}

bool isUavAtomicOperation(
    const Instruction* instr)
{
    return instr->opcode == IL_OP_UAV_ADD ||
        instr->opcode ==IL_OP_UAV_READ_ADD ||
        instr->opcode == IL_OP_UAV_UMAX ||
        instr->opcode == IL_OP_UAV_READ_UMAX;
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
        ilcSpvPutCapability(compiler->module, SpvCapabilityTessellation);
        break;
    case IL_SHADER_DOMAIN:
        execution = SpvExecutionModelTessellationEvaluation;
        ilcSpvPutCapability(compiler->module, SpvCapabilityTessellation);
        break;
    }

    unsigned interfaceCount = compiler->regCount +
                              compiler->resourceCount +
                              compiler->samplerCount;
    IlcSpvWord* interfaces = malloc(sizeof(IlcSpvWord) * interfaceCount);
    unsigned interfaceIndex = 0;

    for (int i = 0; i < compiler->regCount; i++) {
        const IlcRegister* reg = &compiler->regs[i];
        if (reg->duped) {
            continue;
        }
        interfaces[interfaceIndex] = reg->id;
        interfaceIndex++;
        if (compiler->regs[i].id == 0) {
            LOGW("got empty reg %d %d %d\n", compiler->regs[i].ilNum, compiler->regs[i].ilType, compiler->regs[i].ilImportUsage);
        }

    }
    for (int i = 0; i < compiler->resourceCount; i++) {
        const IlcResource* resource = &compiler->resources[i];

        interfaces[interfaceIndex] = resource->id;
        if (compiler->resources[i].id == 0) {
            LOGW("got empty sampler %d\n", resource->id);
        }

        interfaceIndex++;
    }
    for (int i = 0; i < compiler->samplerCount; i++) {
        const IlcSampler* sampler = &compiler->samplers[i];

        interfaces[interfaceIndex] = sampler->id;
        if (compiler->regs[i].id == 0) {
            LOGW("got empty sampler %d\n", sampler->id);
        }
        interfaceIndex++;
    }

    ilcSpvPutEntryPoint(compiler->module, compiler->entryPointId, execution, name,
                        interfaceIndex, interfaces);
    ilcSpvPutName(compiler->module, compiler->entryPointId, name);

    switch (compiler->kernel->shaderType) {
    case IL_SHADER_PIXEL:
        ilcSpvPutExtension(compiler->module, "SPV_EXT_demote_to_helper_invocation");
        ilcSpvPutExecMode(compiler->module, compiler->entryPointId,
                          SpvExecutionModeOriginUpperLeft, 0, NULL);
        break;
    }

    free(interfaces);
}

IlcShader ilcCompileKernel(
    const Kernel* kernel,
    const char* name)
{
    IlcSpvModule module;

    ilcSpvInit(&module);

    IlcSpvId nameId = ilcSpvPutString(&module, name);
    ilcSpvPutSource(&module, nameId);

    IlcSpvId uintId = ilcSpvPutIntType(&module, false);
    IlcSpvId intId = ilcSpvPutIntType(&module, true);
    IlcSpvId floatId = ilcSpvPutFloatType(&module);
    IlcSpvId boolId = ilcSpvPutBoolType(&module);

    IlcCompiler compiler = {
        .kernel = kernel,
        .module = &module,
        .bindingCount = 0,
        .bindings = NULL,
        .atomicUavCount = 0,
        .atomicUavs = NULL,
        .inputCount = 0,
        .inputs = NULL,
        .outputCount = 0,
        .outputLocations = NULL,
        .emitHullOutputFinalize = 0,
        .entryPointId = ilcSpvAllocId(&module),
        .stageFunctionId = compiler.kernel->shaderType != IL_SHADER_HULL ? ilcSpvAllocId(&module) : 0,
        .uintId = uintId,
        .uint4Id = ilcSpvPutVectorType(&module, uintId, 4),
        .intId = intId,
        .int4Id = ilcSpvPutVectorType(&module, intId, 4),
        .floatId = floatId,
        .float4Id = ilcSpvPutVectorType(&module, floatId, 4),
        .boolId = boolId,
        .bool4Id = ilcSpvPutVectorType(&module, boolId, 4),
        .currentStrideIndex = 0,
        .inputArraySize = 0,
        .inputControlPointSize = 0,
        .regCount = 0,
        .regs = NULL,
        .resourceCount = 0,
        .resources = NULL,
        .samplerCount = 0,
        .samplers = NULL,
        .controlFlowBlockCount = 0,
        .controlFlowBlocks = NULL,
        .currentFunctionLabelId = 0,
        .forkPhaseCount = 0,
        .forkPhases = NULL,
        .forkPhaseCount = 0,
        .joinPhases = NULL,
        .joinPhaseCount = 0,
        .controlPointPhase = {},
        .currentHullPhase = {},
        .isInFunction = false,
        .isAfterReturn = false,
    };

    // analyze shader
    for (int i = 0; i < kernel->instrCount; i++) {
        if (compiler.kernel->shaderType == IL_SHADER_DOMAIN &&
            kernel->instrs[i].opcode == IL_DCL_INPUT &&
            kernel->instrs[i].dstCount > 0 &&
            kernel->instrs[i].dsts[0].registerType == IL_REGTYPE_INPUTCP) {
            unsigned regNum = (kernel->instrs[i].dsts[0].absoluteSrc != NULL ? kernel->instrs[i].dsts[0].absoluteSrc->registerNum : kernel->instrs[i].dsts[0].registerNum) + 1;
            compiler.inputArraySize = compiler.inputArraySize < regNum ? regNum : compiler.inputArraySize;
        } else if (kernel->instrs[i].opcode == IL_DCL_NUM_ICP &&
                   kernel->instrs[i].extraCount > 0) {
            compiler.inputControlPointSize = kernel->instrs[i].extras[0];
        } else if (isUavAtomicOperation(&kernel->instrs[i])) {
            uint16_t ilResourceId = GET_BITS(kernel->instrs[i].control, 0, 14);
            bool resourceFound = false;
            for (unsigned j = 0; j < compiler.atomicUavCount; j++) {
                if (compiler.atomicUavs[j] == ilResourceId) {
                    resourceFound = true;
                    break;
                }
            }
            if (!resourceFound) {
                compiler.atomicUavCount++;
                compiler.atomicUavs = realloc(compiler.atomicUavs, compiler.atomicUavCount * sizeof(uint16_t));
                compiler.atomicUavs[compiler.atomicUavCount - 1] = ilResourceId;
            }
        }
    }

    emitImplicitInputs(&compiler);
    if (compiler.kernel->shaderType != IL_SHADER_HULL) {
        compiler.isInFunction = true;
        emitFunc(&compiler, compiler.stageFunctionId);
        const char* stageFunctionName = "stage_main";
        switch (compiler.kernel->shaderType) {
        case IL_SHADER_VERTEX:
            stageFunctionName = "vs_main";
            break;
        case IL_SHADER_DOMAIN:
            stageFunctionName = "ds_main";
            break;
        case IL_SHADER_GEOMETRY:
            stageFunctionName = "gs_main";
            break;
        case IL_SHADER_PIXEL:
            stageFunctionName = "ps_main";
            break;
        case IL_SHADER_COMPUTE:
            stageFunctionName = "cs_main";
            break;
        default:
            break;
        }
        ilcSpvPutName(compiler.module, compiler.stageFunctionId, stageFunctionName);
    }
    if (compiler.kernel->shaderType == IL_SHADER_DOMAIN) {
        emitDomainShaderInit(&compiler);
    }
    for (int i = 0; i < kernel->instrCount; i++) {
        emitInstr(&compiler, &kernel->instrs[i]);
    }

    compiler.isInFunction = true;
    compiler.isAfterReturn = false;
    emitFunc(&compiler, compiler.entryPointId);
    IlcSpvId voidTypeId = ilcSpvPutVoidType(compiler.module);
    if (compiler.kernel->shaderType == IL_SHADER_HULL) {
        IlcSpvId invocationVarId = createInvocationId(&compiler);
        IlcSpvId invocationId = ilcSpvPutLoad(compiler.module, compiler.intId, invocationVarId);

        IlcSpvId oneId = ilcSpvPutConstant(compiler.module, compiler.intId, 1);
        IlcSpvId condId = ilcSpvPutOp2(compiler.module, SpvOpULessThan, compiler.boolId, invocationId, oneId);

        IlcSpvId invocationBlockBegin = ilcSpvAllocId(compiler.module);
        IlcSpvId invocationBlockEnd = ilcSpvAllocId(compiler.module);
        ilcSpvPutSelectionMerge(compiler.module, invocationBlockEnd);
        ilcSpvPutBranchConditional(compiler.module, condId, invocationBlockBegin, invocationBlockEnd);
        ilcSpvPutLabel(compiler.module, invocationBlockBegin);
        //emit fork join stuff
        for (int i = 0; i < compiler.forkPhaseCount; ++i) {
            for (int j = 0; j < compiler.forkPhases[i].invocationCount;++j) {
                IlcSpvId invocationId = ilcSpvPutConstant(compiler.module, compiler.intId, j);

                ilcSpvPutFunctionCall(compiler.module, voidTypeId, compiler.forkPhases[i].functionId, 1, &invocationId);
            }
        }
        for (int i = 0; i < compiler.joinPhaseCount; ++i) {
            for (int j = 0; j < compiler.joinPhases[i].invocationCount;++j) {
                IlcSpvId invocationId = ilcSpvPutConstant(compiler.module, compiler.intId, j);

                ilcSpvPutFunctionCall(compiler.module, voidTypeId, compiler.joinPhases[i].functionId, 1, &invocationId);
            }
        }
        ilcSpvPutBranch(compiler.module, invocationBlockEnd);
        ilcSpvPutLabel(compiler.module, invocationBlockEnd);

        finalizeHullShader(&compiler, invocationVarId);
    } else {
        // call stage main
        ilcSpvPutFunctionCall(compiler.module, voidTypeId, compiler.stageFunctionId, 0, NULL);
        // finalize
        if (compiler.kernel->shaderType == IL_SHADER_DOMAIN || compiler.kernel->shaderType == IL_SHADER_VERTEX) {
            finalizeVertexStage(&compiler);
        }
    }

    // close stage main function if not yet ended
    if (compiler.isInFunction) {
        if (!compiler.isAfterReturn) {
            ilcSpvPutReturn(compiler.module);
            compiler.isAfterReturn = true;
        }
        ilcSpvPutFunctionEnd(compiler.module);
        compiler.isInFunction = false;
    }

    emitEntryPoint(&compiler);

    free(compiler.regs);
    free(compiler.resources);
    free(compiler.samplers);
    free(compiler.controlFlowBlocks);
    free(compiler.forkPhases);
    free(compiler.joinPhases);
    free(compiler.atomicUavs);
    ilcSpvFinish(&module);

    return (IlcShader) {
        .codeSize = sizeof(IlcSpvWord) * module.buffer[ID_MAIN].wordCount,
        .code = module.buffer[ID_MAIN].words,
        .bindingCount = compiler.bindingCount,
        .bindings = compiler.bindings,
        .inputCount = compiler.inputCount,
        .inputs = compiler.inputs,
        .outputCount = compiler.outputCount,
        .outputLocations = compiler.outputLocations,
        .name = strdup(name),
    };
}
