#include "spirv/spirv.h"
#include "amdilc_spirv.h"
#include "amdilc_internal.h"

typedef struct {
    IlcSpvModule* module;
    const Kernel* kernel;
    IlcSpvId entryPointId;
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
    };

    emitFunc(&compiler, compiler.entryPointId);
    for (int i = 0; i < kernel->instrCount; i++) {
        emitInstr(&compiler, &kernel->instrs[i]);
    }

    emitEntryPoint(&compiler);
    ilcSpvFinish(&module);

    *size = sizeof(IlcSpvWord) * module.buffer[ID_MAIN].wordCount;
    return module.buffer[ID_MAIN].words;
}
