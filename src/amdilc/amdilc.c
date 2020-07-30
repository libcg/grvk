#include "amdilc_internal.h"

static void freeInstruction(
    Instruction* instr)
{
    free(instr->dsts);
    free(instr->srcs);
    free(instr->extras);
}

static void freeKernel(
    Kernel* kernel)
{
    for (int i = 0; i < kernel->instrCount; i++) {
        freeInstruction(&kernel->instrs[i]);
    }
    free(kernel->instrs);
}

uint32_t* ilcCompileShader(
    uint32_t* compiledSize,
    const void* code,
    uint32_t size,
    bool dump)
{
    uint32_t* compiledCode;
    Kernel* kernel = ilcDecodeStream((Token*)code, size / sizeof(Token));

    if (dump) {
        ilcDumpKernel(kernel);
    }
    compiledCode = ilcCompileKernel(compiledSize, kernel);

    freeKernel(kernel);
    free(kernel);
    return compiledCode;
}
