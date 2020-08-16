#include <stdio.h>
#include "spirv/spirv.h"
#include "amdilc_spirv.h"
#include "amdilc_internal.h"

typedef struct {
    IlcSpvId id;
    IlcSpvId typeId;
    uint32_t ilType; // ILRegType
    uint32_t ilNum;
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
    unsigned regCount;
    IlcRegister *regs;
    unsigned resourceCount;
    IlcResource *resources;
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

static IlcSpvId loadSource(
    IlcCompiler* compiler,
    const Source* src)
{
    const IlcRegister* reg = findRegister(compiler, src->registerType, src->registerNum);

    return ilcSpvPutLoad(compiler->module, reg->typeId, reg->id);
}

static void storeDestination(
    IlcCompiler* compiler,
    const Destination* dst,
    IlcSpvId varId)
{
    const IlcRegister* dstReg = findRegister(compiler, dst->registerType, dst->registerNum);

    if (dstReg == NULL && dst->registerType == IL_REGTYPE_TEMP) {
        // Create temporary register
        IlcSpvId floatId = ilcSpvPutFloatType(compiler->module);
        IlcSpvId vectorId = ilcSpvPutVectorType(compiler->module, floatId, 4);
        IlcSpvId pointerId = ilcSpvPutPointerType(compiler->module, SpvStorageClassPrivate,
                                                  vectorId);
        IlcSpvId tempId = ilcSpvPutVariable(compiler->module, pointerId, SpvStorageClassPrivate);

        const IlcRegister reg = {
            .id = tempId,
            .typeId = vectorId,
            .ilType = IL_REGTYPE_TEMP,
            .ilNum = dst->registerNum,
        };

        dstReg = addRegister(compiler, &reg, 'r');
    }

    ilcSpvPutStore(compiler->module, dstReg->id, varId);
}

static void emitGlobalFlags(
    IlcCompiler* compiler,
    Instruction* instr)
{
    bool refactoringAllowed = getBit(instr->control, 0);
    bool forceEarlyDepthStencil = getBit(instr->control, 1);
    bool enableRawStructuredBuffers = getBit(instr->control, 2);
    bool enableDoublePrecisionFloatOps = getBit(instr->control, 3);

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

static void emitOutput(
    IlcCompiler* compiler,
    Instruction* instr)
{
    uint8_t importUsage = getBits(instr->control, 0, 4);

    assert(instr->dstCount == 1 &&
           instr->srcCount == 0 &&
           instr->extraCount == 0);

    Destination* dst = &instr->dsts[0];

    assert(dst->registerType == IL_REGTYPE_OUTPUT &&
           !dst->clamp &&
           dst->shiftScale == IL_SHIFT_NONE);

    if (dst->component[0] != IL_MODCOMP_WRITE ||
        dst->component[1] != IL_MODCOMP_WRITE ||
        dst->component[2] != IL_MODCOMP_WRITE ||
        dst->component[3] != IL_MODCOMP_WRITE) {
        LOGW("unhandled component mod %d %d %d %d\n",
             dst->component[0], dst->component[1], dst->component[2], dst->component[3]);
    }

    IlcSpvId floatId = ilcSpvPutFloatType(compiler->module);
    IlcSpvId vectorId = ilcSpvPutVectorType(compiler->module, floatId, 4);
    IlcSpvId pointerId = ilcSpvPutPointerType(compiler->module, SpvStorageClassOutput, vectorId);
    IlcSpvId outputId = ilcSpvPutVariable(compiler->module, pointerId, SpvStorageClassOutput);

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
        .typeId = vectorId,
        .ilType = dst->registerType,
        .ilNum = dst->registerNum,
    };

    addRegister(compiler, &reg, 'o');
}

static void emitInput(
    IlcCompiler* compiler,
    Instruction* instr)
{
    uint8_t importUsage = getBits(instr->control, 0, 4);
    uint8_t interpMode = getBits(instr->control, 5, 7);
    IlcSpvId inputId = 0;
    IlcSpvId typeId = 0;

    assert(instr->dstCount == 1 &&
           instr->srcCount == 0 &&
           instr->extraCount == 0);

    Destination* dst = &instr->dsts[0];

    assert(dst->registerType == IL_REGTYPE_INPUT &&
           !dst->clamp &&
           dst->shiftScale == IL_SHIFT_NONE);

    if (importUsage == IL_IMPORTUSAGE_GENERIC) {
        if (dst->component[0] != IL_MODCOMP_WRITE ||
            dst->component[1] != IL_MODCOMP_WRITE ||
            dst->component[2] != IL_MODCOMP_WRITE ||
            dst->component[3] != IL_MODCOMP_WRITE) {
            LOGW("unhandled component mod %d %d %d %d\n",
                 dst->component[0], dst->component[1], dst->component[2], dst->component[3]);
        }

        IlcSpvId f32Id = ilcSpvPutFloatType(compiler->module);
        IlcSpvId v4f32Id = ilcSpvPutVectorType(compiler->module, f32Id, 4);
        IlcSpvId pv4f32Id = ilcSpvPutPointerType(compiler->module, SpvStorageClassInput, v4f32Id);
        inputId = ilcSpvPutVariable(compiler->module, pv4f32Id, SpvStorageClassInput);
        typeId = v4f32Id;

        IlcSpvWord locationIdx = dst->registerNum;
        ilcSpvPutDecoration(compiler->module, inputId, SpvDecorationLocation, 1, &locationIdx);
    } else if (importUsage == IL_IMPORTUSAGE_VERTEXID) {
        IlcSpvId u32Id = ilcSpvPutIntType(compiler->module, false);
        IlcSpvId pu32Id = ilcSpvPutPointerType(compiler->module, SpvStorageClassInput, u32Id);
        inputId = ilcSpvPutVariable(compiler->module, pu32Id, SpvStorageClassInput);
        typeId = u32Id;

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
        .typeId = typeId,
        .ilType = dst->registerType,
        .ilNum = dst->registerNum,
    };

    addRegister(compiler, &reg, 'v');
}

static void emitResource(
    IlcCompiler* compiler,
    Instruction* instr)
{
    assert(instr->dstCount == 0 &&
           instr->srcCount == 0 &&
           instr->extraCount == 1);

    uint8_t id = getBits(instr->control, 0, 7);
    uint8_t type = getBits(instr->control, 8, 11);
    bool unnorm = getBit(instr->control, 31);
    uint8_t fmtx = getBits(instr->extras[0], 20, 22);
    uint8_t fmty = getBits(instr->extras[0], 23, 25);
    uint8_t fmtz = getBits(instr->extras[0], 26, 28);
    uint8_t fmtw = getBits(instr->extras[0], 29, 31);

    if (type != IL_USAGE_PIXTEX_BUFFER || unnorm) {
        LOGE("unhandled resource type %d %d\n", type, unnorm);
        assert(false);
    }
    if (fmtx != IL_ELEMENTFORMAT_FLOAT || fmty != IL_ELEMENTFORMAT_FLOAT ||
        fmtz != IL_ELEMENTFORMAT_FLOAT || fmtw != IL_ELEMENTFORMAT_FLOAT) {
        LOGE("unhandled resource format %d %d %d %d\n", fmtx, fmty, fmtz, fmtw);
        assert(false);
    }

    IlcSpvId f32Id = ilcSpvPutFloatType(compiler->module);
    IlcSpvId imageId = ilcSpvPutImageType(compiler->module, f32Id, SpvDimBuffer, 0, 0, 0, 1,
                                          SpvImageFormatRgba32f);
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

static void emitMov(
    IlcCompiler* compiler,
    Instruction* instr)
{
    IlcSpvId srcId = loadSource(compiler, &instr->srcs[0]);
    storeDestination(compiler, &instr->dsts[0], srcId);
}

static void emitLoad(
    IlcCompiler* compiler,
    Instruction* instr)
{
    uint8_t ilResourceId = getBits(instr->control, 0, 7);
    const IlcResource* resource = findResource(compiler, ilResourceId);

    const Destination* dst = &instr->dsts[0];
    const IlcRegister* dstReg = findRegister(compiler, dst->registerType, dst->registerNum);

    IlcSpvId srcId = loadSource(compiler, &instr->srcs[0]);
    IlcSpvId resourceId = ilcSpvPutLoad(compiler->module, resource->typeId, resource->id);
    IlcSpvId fetchId = ilcSpvPutImageFetch(compiler->module, dstReg->typeId, resourceId, srcId);
    storeDestination(compiler, dst, fetchId);
}

static void emitInstr(
    IlcCompiler* compiler,
    Instruction* instr)
{
    switch (instr->opcode) {
    case IL_OP_END:
        ilcSpvPutFunctionEnd(compiler->module);
        break;
    case IL_OP_MOV:
        emitMov(compiler, instr);
        break;
    case IL_OP_RET_DYN:
        ilcSpvPutReturn(compiler->module);
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
        IlcRegister* reg = &compiler->regs[i];

        interfaces[i] = reg->id;
    }
    for (int i = 0; i < compiler->resourceCount; i++) {
        IlcResource* resource = &compiler->resources[i];

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

    IlcCompiler compiler = {
        .module = &module,
        .kernel = kernel,
        .entryPointId = ilcSpvAllocId(&module),
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
