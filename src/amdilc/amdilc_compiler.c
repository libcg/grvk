#include <stdio.h>
#include "spirv/spirv.h"
#include "amdilc_spirv.h"
#include "amdilc_internal.h"

typedef struct {
    IlcSpvId id;
    uint32_t registerNum;
} IlcInput;

typedef struct {
    IlcSpvModule* module;
    const Kernel* kernel;
    IlcSpvId entryPointId;
    unsigned inputCount;
    IlcInput *inputs;
} IlcCompiler;

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

static void emitInput(
    IlcCompiler* compiler,
    Instruction* instr)
{
    uint8_t importUsage = getBits(instr->control, 0, 4);
    uint8_t interpMode = getBits(instr->control, 5, 7);

    if (importUsage != IL_IMPORTUSAGE_GENERIC) {
        LOGW("unhandled import usage %d\n", importUsage);
    }

    assert(instr->dstCount == 1 &&
           instr->srcCount == 0 &&
           instr->extraCount == 0);

    Destination* dst = &instr->dsts[0];

    assert(dst->registerType == IL_REGTYPE_INPUT &&
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
    IlcSpvId pointerId = ilcSpvPutPointerType(compiler->module, SpvStorageClassInput, vectorId);
    IlcSpvId inputId = ilcSpvPutVariable(compiler->module, pointerId, SpvStorageClassInput);

    char name[16];
    snprintf(name, 16, "v%u", dst->registerNum);
    ilcSpvPutName(compiler->module, inputId, name);

    IlcSpvWord locationIdx = 0; // FIXME hardcoded to match glslc VS slot
    ilcSpvPutDecoration(compiler->module, inputId, SpvDecorationLocation, 1, &locationIdx);

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

    compiler->inputCount++;
    compiler->inputs = realloc(compiler->inputs, sizeof(IlcInput) * compiler->inputCount);
    compiler->inputs[compiler->inputCount - 1] = (IlcInput) {
        .id = inputId,
        .registerNum = dst->registerNum
    };
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

static void emitInstr(
    IlcCompiler* compiler,
    Instruction* instr)
{
    switch (instr->opcode) {
    case IL_OP_END:
        ilcSpvPutFunctionEnd(compiler->module);
        break;
    case IL_OP_RET_DYN:
        ilcSpvPutReturn(compiler->module);
        break;
    case IL_DCL_INPUT:
        emitInput(compiler, instr);
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
    const char* name = NULL;
    unsigned inputCount = 0;
    IlcSpvWord* inputs = NULL;

    switch (compiler->kernel->shaderType) {
    case IL_SHADER_VERTEX:
        execution = SpvExecutionModelVertex;
        name = "VShader";
        break;
    case IL_SHADER_PIXEL:
        execution = SpvExecutionModelFragment;
        name = "PShader";
        break;
    case IL_SHADER_GEOMETRY:
        execution = SpvExecutionModelGeometry;
        name = "GShader";
        break;
    case IL_SHADER_COMPUTE:
        execution = SpvExecutionModelGLCompute;
        name = "CShader";
        break;
    case IL_SHADER_HULL:
        execution = SpvExecutionModelTessellationControl;
        name = "HShader";
        break;
    case IL_SHADER_DOMAIN:
        execution = SpvExecutionModelTessellationEvaluation;
        name = "DShader";
        break;
    }

    // FIXME gather inputs/outputs

    ilcSpvPutEntryPoint(compiler->module, compiler->entryPointId, execution, name,
                        inputCount, inputs);
    ilcSpvPutName(compiler->module, compiler->entryPointId, name);

    switch (compiler->kernel->shaderType) {
    case IL_SHADER_PIXEL:
        ilcSpvPutExecMode(compiler->module, compiler->entryPointId,
                          SpvExecutionModeOriginUpperLeft);
        break;
    }
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
        .inputCount = 0,
        .inputs = NULL,
    };

    emitFunc(&compiler, compiler.entryPointId);
    for (int i = 0; i < kernel->instrCount; i++) {
        emitInstr(&compiler, &kernel->instrs[i]);
    }

    emitEntryPoint(&compiler);

    free(compiler.inputs);
    ilcSpvFinish(&module);

    *size = sizeof(IlcSpvWord) * module.buffer[ID_MAIN].wordCount;
    return module.buffer[ID_MAIN].words;
}
