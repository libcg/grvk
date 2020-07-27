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

void ilcDumpShader(
    const void* code,
    uint32_t size)
{
    Kernel* kernel = ilcDecodeStream((Token*)code, size / sizeof(Token));

    ilcDumpKernel(kernel);
    freeKernel(kernel);
    free(kernel);
}
