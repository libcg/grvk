#include <stdio.h>
#include "amdilc_spirv.h"
#include "amdilc_internal.h"

#define MAX_SRC_COUNT       (8)
#define ZERO_LITERAL        (0x00000000)
#define ONE_LITERAL         (0x3F800000)
#define FALSE_LITERAL       (0x00000000)
#define TRUE_LITERAL        (0xFFFFFFFF)

typedef struct {
    IlcSpvId id;
    IlcSpvId typeId;
    uint32_t ilType; // ILRegType
    uint32_t ilNum;
    bool isScalar;
} IlcRegister;

typedef struct {
    IlcSpvId id;
    IlcSpvId typeId;
    uint32_t ilId;
} IlcResource;

typedef struct {
    IlcSpvModule* module;
    const Kernel* kernel;
    IlcSpvId entryPointId;
    IlcSpvId intId;
    IlcSpvId int4Id;
    IlcSpvId floatId;
    IlcSpvId float4Id;
    IlcSpvId bool4Id;
    unsigned regCount;
    IlcRegister* regs;
    unsigned resourceCount;
    IlcResource* resources;
} IlcCompiler;

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

static const IlcResource* addResource(
    IlcCompiler* compiler,
    const IlcResource* resource)
{
    char name[32];
    snprintf(name, 32, "resource%u", resource->ilId);
    ilcSpvPutName(compiler->module, resource->id, name);

    compiler->resourceCount++;
    compiler->resources = realloc(compiler->resources,
                                  sizeof(IlcResource) * compiler->resourceCount);
    compiler->resources[compiler->resourceCount - 1] = *resource;

    return &compiler->resources[compiler->resourceCount - 1];
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

static IlcSpvId loadSource(
    IlcCompiler* compiler,
    const Source* src,
    uint8_t componentMask)
{
    const IlcRegister* reg = findRegister(compiler, src->registerType, src->registerNum);

    if (reg == NULL) {
        LOGE("register %d %d not found\n", src->registerType, src->registerNum);
        return 0;
    }

    IlcSpvId varId = ilcSpvPutLoad(compiler->module, reg->typeId, reg->id);

    const uint8_t swizzle[] = {
        componentMask & 1 ? src->swizzle[0] : IL_COMPSEL_0,
        componentMask & 2 ? src->swizzle[1] : IL_COMPSEL_0,
        componentMask & 4 ? src->swizzle[2] : IL_COMPSEL_0,
        componentMask & 8 ? src->swizzle[3] : IL_COMPSEL_0,
    };

    if (!reg->isScalar &&
        (swizzle[0] != IL_COMPSEL_X_R || swizzle[1] != IL_COMPSEL_Y_G ||
         swizzle[2] != IL_COMPSEL_Z_B || swizzle[3] != IL_COMPSEL_W_A)) {
        // Select components from {x, y, z, w, 0.f, 1.f}
        IlcSpvId float2Id = ilcSpvPutVectorType(compiler->module, compiler->floatId, 2);
        const IlcSpvId consistuentIds[] = {
            ilcSpvPutConstant(compiler->module, compiler->floatId, ZERO_LITERAL),
            ilcSpvPutConstant(compiler->module, compiler->floatId, ONE_LITERAL),
        };
        IlcSpvId compositeId = ilcSpvPutConstantComposite(compiler->module, float2Id,
                                                          2, consistuentIds);

        const IlcSpvWord components[] = { swizzle[0], swizzle[1], swizzle[2], swizzle[3] };
        varId = ilcSpvPutVectorShuffle(compiler->module, reg->typeId, varId, compositeId,
                                       4, components);
    }

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
        varId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450FAbs, reg->typeId, 1, &varId);
    }

    if (src->negate[0] || src->negate[1] || src->negate[2] || src->negate[3]) {
        IlcSpvId negId = ilcSpvPutAlu(compiler->module, SpvOpFNegate, reg->typeId, 1, &varId);

        if (src->negate[0] && src->negate[1] && src->negate[2] && src->negate[3]) {
            varId = negId;
        } else {
            // Select components from {x, y, z, w, -x, -y, -z, -w}
            const IlcSpvWord components[] = {
                src->negate[0] ? 0 : 4,
                src->negate[1] ? 1 : 5,
                src->negate[2] ? 2 : 6,
                src->negate[3] ? 3 : 7,
            };

            varId = ilcSpvPutVectorShuffle(compiler->module, reg->typeId, negId, varId,
                                           4, components);
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
    const IlcRegister* dstReg = findRegister(compiler, dst->registerType, dst->registerNum);

    if (dstReg == NULL && dst->registerType == IL_REGTYPE_TEMP) {
        // Create temporary register
        IlcSpvId tempTypeId = 0;
        IlcSpvId tempId = emitVectorVariable(compiler, &tempTypeId, 4, compiler->floatId,
                                             SpvStorageClassPrivate);

        const IlcRegister reg = {
            .id = tempId,
            .typeId = tempTypeId,
            .ilType = IL_REGTYPE_TEMP,
            .ilNum = dst->registerNum,
            .isScalar = false,
        };

        dstReg = addRegister(compiler, &reg, 'r');
    }

    if (dstReg == NULL) {
        LOGE("register %d %d not found\n", dst->registerType, dst->registerNum);
        return;
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
        IlcSpvId zeroCompositeId = ilcSpvPutConstantComposite(compiler->module, dstReg->typeId,
                                                              4, zeroConsistuentIds);
        IlcSpvId oneCompositeId = ilcSpvPutConstantComposite(compiler->module, dstReg->typeId,
                                                             4, oneConsistuentIds);

        const IlcSpvId paramIds[] = { varId, zeroCompositeId, oneCompositeId };
        varId = ilcSpvPutGLSLOp(compiler->module, GLSLstd450FClamp, dstReg->typeId, 3, paramIds);
    }

    if (dst->component[0] == IL_MODCOMP_NOWRITE || dst->component[1] == IL_MODCOMP_NOWRITE ||
        dst->component[2] == IL_MODCOMP_NOWRITE || dst->component[3] == IL_MODCOMP_NOWRITE) {
        // Select components from {dst.x, dst.y, dst.z, dst.w, x, y, z, w}
        IlcSpvId origId = ilcSpvPutLoad(compiler->module, dstReg->typeId, dstReg->id);

        const IlcSpvWord components[] = {
            dst->component[0] == IL_MODCOMP_NOWRITE ? 0 : 4,
            dst->component[1] == IL_MODCOMP_NOWRITE ? 1 : 5,
            dst->component[2] == IL_MODCOMP_NOWRITE ? 2 : 6,
            dst->component[3] == IL_MODCOMP_NOWRITE ? 3 : 7,
        };
        varId = ilcSpvPutVectorShuffle(compiler->module, dstReg->typeId, origId, varId,
                                       4, components);
    }

    if ((dst->component[0] == IL_MODCOMP_0 || dst->component[0] == IL_MODCOMP_1) ||
        (dst->component[1] == IL_MODCOMP_0 || dst->component[1] == IL_MODCOMP_1) ||
        (dst->component[2] == IL_MODCOMP_0 || dst->component[2] == IL_MODCOMP_1) ||
        (dst->component[3] == IL_MODCOMP_0 || dst->component[3] == IL_MODCOMP_1)) {
        // Select components from {x, y, z, w, 0.f, 1.f}
        IlcSpvId float2Id = ilcSpvPutVectorType(compiler->module, compiler->floatId, 2);
        const IlcSpvId consistuentIds[] = {
            ilcSpvPutConstant(compiler->module, compiler->floatId, ZERO_LITERAL),
            ilcSpvPutConstant(compiler->module, compiler->floatId, ONE_LITERAL),
        };
        IlcSpvId compositeId = ilcSpvPutConstantComposite(compiler->module, float2Id,
                                                          2, consistuentIds);

        const IlcSpvWord components[] = {
            dst->component[0] == IL_MODCOMP_0 ? 4 : (dst->component[0] == IL_MODCOMP_1 ? 5 : 0),
            dst->component[1] == IL_MODCOMP_0 ? 4 : (dst->component[1] == IL_MODCOMP_1 ? 5 : 1),
            dst->component[2] == IL_MODCOMP_0 ? 4 : (dst->component[2] == IL_MODCOMP_1 ? 5 : 2),
            dst->component[3] == IL_MODCOMP_0 ? 4 : (dst->component[3] == IL_MODCOMP_1 ? 5 : 3),
        };
        varId = ilcSpvPutVectorShuffle(compiler->module, dstReg->typeId, varId, compositeId,
                                       4, components);
    }

    ilcSpvPutStore(compiler->module, dstReg->id, varId);
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
        .ilType = src->registerType,
        .ilNum = src->registerNum,
        .isScalar = false,
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
        .ilType = dst->registerType,
        .ilNum = dst->registerNum,
        .isScalar = false,
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
    bool isScalar = false;

    assert(instr->dstCount == 1 &&
           instr->srcCount == 0 &&
           instr->extraCount == 0);

    const Destination* dst = &instr->dsts[0];

    assert(dst->registerType == IL_REGTYPE_INPUT &&
           !dst->clamp &&
           dst->shiftScale == IL_SHIFT_NONE);

    if (importUsage == IL_IMPORTUSAGE_GENERIC) {
        inputId = emitVectorVariable(compiler, &inputTypeId, 4, compiler->floatId,
                                     SpvStorageClassInput);
        isScalar = false;

        IlcSpvWord locationIdx = dst->registerNum;
        ilcSpvPutDecoration(compiler->module, inputId, SpvDecorationLocation, 1, &locationIdx);
    } else if (importUsage == IL_IMPORTUSAGE_VERTEXID) {
        IlcSpvId uintId = ilcSpvPutIntType(compiler->module, false);
        IlcSpvId pointerId = ilcSpvPutPointerType(compiler->module, SpvStorageClassInput, uintId);
        inputId = ilcSpvPutVariable(compiler->module, pointerId, SpvStorageClassInput);
        inputTypeId = uintId;
        isScalar = true;

        IlcSpvWord builtInType = SpvBuiltInVertexIndex;
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
        .ilType = dst->registerType,
        .ilNum = dst->registerNum,
        .isScalar = isScalar,
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

    if (type != IL_USAGE_PIXTEX_BUFFER || unnorm) {
        LOGE("unhandled resource type %d %d\n", type, unnorm);
        assert(false);
    }
    if (fmtx != IL_ELEMENTFORMAT_FLOAT || fmty != IL_ELEMENTFORMAT_FLOAT ||
        fmtz != IL_ELEMENTFORMAT_FLOAT || fmtw != IL_ELEMENTFORMAT_FLOAT) {
        LOGE("unhandled resource format %d %d %d %d\n", fmtx, fmty, fmtz, fmtw);
        assert(false);
    }

    IlcSpvId imageId = ilcSpvPutImageType(compiler->module, compiler->floatId, SpvDimBuffer,
                                          0, 0, 0, 1, SpvImageFormatRgba32f);
    IlcSpvId pImageId = ilcSpvPutPointerType(compiler->module, SpvStorageClassUniformConstant,
                                             imageId);
    IlcSpvId resourceId = ilcSpvPutVariable(compiler->module, pImageId,
                                            SpvStorageClassUniformConstant);

    ilcSpvPutCapability(compiler->module, SpvCapabilitySampledBuffer);
    ilcSpvPutName(compiler->module, imageId, "float4Buffer");

    IlcSpvWord descriptorSetIdx = compiler->kernel->shaderType;
    ilcSpvPutDecoration(compiler->module, resourceId, SpvDecorationDescriptorSet,
                        1, &descriptorSetIdx);
    IlcSpvWord bindingIdx = id;
    ilcSpvPutDecoration(compiler->module, resourceId, SpvDecorationBinding, 1, &bindingIdx);

    const IlcResource resource = {
        .id = resourceId,
        .typeId = imageId,
        .ilId = id,
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
    ilcSpvPutLabel(compiler->module);
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
        componentMask = 0x3;
        break;
    case IL_OP_DP3:
        componentMask = 0x7;
        break;
    default:
        componentMask = 0xF;
        break;
    }

    for (int i = 0; i < instr->srcCount; i++) {
        srcIds[i] = loadSource(compiler, &instr->srcs[i], componentMask);
    }

    switch (instr->opcode) {
    case IL_OP_ADD:
        resId = ilcSpvPutAlu(compiler->module, SpvOpFAdd, compiler->float4Id,
                             instr->srcCount, srcIds);
        break;
    case IL_OP_DIV:
        if (instr->control != IL_ZEROOP_INFINITY) {
            LOGW("unhandled div zero op %d\n", instr->control);
        }
        // FIXME SPIR-V has undefined division by zero
        resId = ilcSpvPutAlu(compiler->module, SpvOpFDiv, compiler->float4Id,
                             instr->srcCount, srcIds);
        break;
    case IL_OP_DP2:
    case IL_OP_DP3: {
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
        srcIds[i] = loadSource(compiler, &instr->srcs[i], 0xF);
    }

    switch (instr->opcode) {
    case IL_OP_GE:
        compOp = SpvOpFOrdGreaterThanEqual;
        break;
    default:
        assert(false);
        break;
    }

    IlcSpvId condId = ilcSpvPutAlu(compiler->module, compOp, compiler->bool4Id,
                                   instr->srcCount, srcIds);
    IlcSpvId trueId = ilcSpvPutConstant(compiler->module, compiler->floatId, TRUE_LITERAL);
    IlcSpvId falseId = ilcSpvPutConstant(compiler->module, compiler->floatId, FALSE_LITERAL);
    IlcSpvId trueConsistuentIds[] = { trueId, trueId, trueId, trueId };
    IlcSpvId falseConsistuentIds[] = { falseId, falseId, falseId, falseId };
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
    IlcSpvId resId = 0;

    for (int i = 0; i < instr->srcCount; i++) {
        srcIds[i] = ilcSpvPutBitcast(compiler->module, compiler->int4Id,
                                     loadSource(compiler, &instr->srcs[i], 0xF));
    }

    switch (instr->opcode) {
    case IL_OP_I_ADD:
        resId = ilcSpvPutAlu(compiler->module, SpvOpIAdd, compiler->int4Id,
                             instr->srcCount, srcIds);
        break;
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
        srcIds[i] = ilcSpvPutBitcast(compiler->module, compiler->int4Id,
                                     loadSource(compiler, &instr->srcs[i], 0xF));
    }

    switch (instr->opcode) {
    case IL_OP_I_GE:
        compOp = SpvOpSGreaterThanEqual;
        break;
    case IL_OP_I_LT:
        compOp = SpvOpSLessThan;
        break;
    default:
        assert(false);
        break;
    }

    IlcSpvId condId = ilcSpvPutAlu(compiler->module, compOp, compiler->bool4Id,
                                   instr->srcCount, srcIds);
    IlcSpvId trueId = ilcSpvPutConstant(compiler->module, compiler->floatId, TRUE_LITERAL);
    IlcSpvId falseId = ilcSpvPutConstant(compiler->module, compiler->floatId, FALSE_LITERAL);
    IlcSpvId trueConsistuentIds[] = { trueId, trueId, trueId, trueId };
    IlcSpvId falseConsistuentIds[] = { falseId, falseId, falseId, falseId };
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
        srcIds[i] = loadSource(compiler, &instr->srcs[i], 0xF);
    }

    // For each component, select src1 if src0 has any bit set, otherwise select src2
    IlcSpvId falseId = ilcSpvPutConstant(compiler->module, compiler->intId, FALSE_LITERAL);
    IlcSpvId falseConsistuentIds[] = { falseId, falseId, falseId, falseId };
    IlcSpvId falseCompositeId = ilcSpvPutConstantComposite(compiler->module, compiler->int4Id,
                                                           4, falseConsistuentIds);
    IlcSpvId castId = ilcSpvPutBitcast(compiler->module, compiler->int4Id, srcIds[0]);
    const IlcSpvId compIds[] = { castId, falseCompositeId };
    IlcSpvId condId = ilcSpvPutAlu(compiler->module, SpvOpINotEqual, compiler->bool4Id, 2, compIds);
    IlcSpvId resId = ilcSpvPutSelect(compiler->module, compiler->float4Id, condId,
                                     srcIds[1], srcIds[2]);

    storeDestination(compiler, &instr->dsts[0], resId);
}

static void emitLoad(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    uint8_t ilResourceId = GET_BITS(instr->control, 0, 7);
    const IlcResource* resource = findResource(compiler, ilResourceId);

    const Destination* dst = &instr->dsts[0];
    const IlcRegister* dstReg = findRegister(compiler, dst->registerType, dst->registerNum);

    IlcSpvId srcId = loadSource(compiler, &instr->srcs[0], 0x1);
    IlcSpvId resourceId = ilcSpvPutLoad(compiler->module, resource->typeId, resource->id);
    IlcSpvId fetchId = ilcSpvPutImageFetch(compiler->module, dstReg->typeId, resourceId, srcId);
    storeDestination(compiler, dst, fetchId);
}

static void emitInstr(
    IlcCompiler* compiler,
    const Instruction* instr)
{
    switch (instr->opcode) {
    case IL_OP_ADD:
    case IL_OP_DIV:
    case IL_OP_DP3:
    case IL_OP_FRC:
    case IL_OP_MAD:
    case IL_OP_MAX:
    case IL_OP_MOV:
    case IL_OP_MUL:
    case IL_OP_SQRT_VEC:
    case IL_OP_DP2:
        emitFloatOp(compiler, instr);
        break;
    case IL_OP_GE:
        emitFloatComparisonOp(compiler, instr);
        break;
    case IL_OP_I_ADD:
        emitIntegerOp(compiler, instr);
        break;
    case IL_OP_I_GE:
    case IL_OP_I_LT:
        emitIntegerComparisonOp(compiler, instr);
        break;
    case IL_OP_END:
        ilcSpvPutFunctionEnd(compiler->module);
        break;
    case IL_OP_RET_DYN:
        ilcSpvPutReturn(compiler->module);
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
    case IL_OP_LOAD:
        emitLoad(compiler, instr);
        break;
    case IL_OP_CMOV_LOGICAL:
        emitCmovLogical(compiler, instr);
        break;
    case IL_DCL_GLOBAL_FLAGS:
        emitGlobalFlags(compiler, instr);
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

    unsigned interfaceCount = compiler->regCount + compiler->resourceCount;
    IlcSpvWord* interfaces = malloc(sizeof(IlcSpvWord) * interfaceCount);
    for (int i = 0; i < compiler->regCount; i++) {
        const IlcRegister* reg = &compiler->regs[i];

        interfaces[i] = reg->id;
    }
    for (int i = 0; i < compiler->resourceCount; i++) {
        const IlcResource* resource = &compiler->resources[i];

        interfaces[compiler->regCount + i] = resource->id;
    }

    ilcSpvPutEntryPoint(compiler->module, compiler->entryPointId, execution, name,
                        interfaceCount, interfaces);
    ilcSpvPutName(compiler->module, compiler->entryPointId, name);

    switch (compiler->kernel->shaderType) {
    case IL_SHADER_PIXEL:
        ilcSpvPutExecMode(compiler->module, compiler->entryPointId,
                          SpvExecutionModeOriginUpperLeft);
        break;
    }

    free(interfaces);
}

uint32_t* ilcCompileKernel(
    uint32_t* size,
    const Kernel* kernel)
{
    IlcSpvModule module;

    ilcSpvInit(&module);

    IlcSpvId intId = ilcSpvPutIntType(&module, true);
    IlcSpvId floatId = ilcSpvPutFloatType(&module);
    IlcSpvId boolId = ilcSpvPutBoolType(&module);

    IlcCompiler compiler = {
        .module = &module,
        .kernel = kernel,
        .entryPointId = ilcSpvAllocId(&module),
        .intId = intId,
        .int4Id = ilcSpvPutVectorType(&module, intId, 4),
        .floatId = floatId,
        .float4Id = ilcSpvPutVectorType(&module, floatId, 4),
        .bool4Id = ilcSpvPutVectorType(&module, boolId, 4),
        .regCount = 0,
        .regs = NULL,
    };

    emitFunc(&compiler, compiler.entryPointId);
    for (int i = 0; i < kernel->instrCount; i++) {
        emitInstr(&compiler, &kernel->instrs[i]);
    }

    emitEntryPoint(&compiler);

    free(compiler.regs);
    free(compiler.resources);
    ilcSpvFinish(&module);

    *size = sizeof(IlcSpvWord) * module.buffer[ID_MAIN].wordCount;
    return module.buffer[ID_MAIN].words;
}
