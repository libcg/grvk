#include "amdilc_internal.h"

const char* mIlShaderTypeNames[IL_SHADER_LAST] = {
    "vs",
    "ps",
    "gs",
    "cs",
    "hs",
    "ds",
};

const char* mIlLanguageTypeNames[IL_LANG_LAST] = {
    "generic",
    "opengl",
    "dx8_ps",
    "dx8_vs",
    "dx9_ps",
    "dx9_vs",
    "dx10_ps",
    "dx10_vs",
    "dx10_gs",
    "dx11_ps",
    "dx11_vs",
    "dx11_gs",
    "dx11_cs",
    "dx11_hs",
    "dx11_ds",
};

const char* mIlRegTypeNames[IL_REGTYPE_LAST] = {
    "?",
    "?",
    "?",
    "?",
    "r",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "v",
    "o",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
    "?",
};

const char* mIlDivCompNames[IL_DIVCOMP_LAST] = {
    "",
    "_divComp(y)",
    "_divComp(z)",
    "_divComp(w)",
    "_divComp(unknown)",
};

const char* mIlModDstComponentNames[IL_MODCOMP_LAST] = {
    "_",
    "?", // Replaced with x, y, z or w
    "0",
    "1",
};

const char* mIlComponentSelectNames[IL_COMPSEL_LAST] = {
    "x",
    "y",
    "z",
    "w",
    "0",
    "1",
};

const char* mIlShiftScaleNames[IL_SHIFT_LAST] = {
    "",
    "_x2"
    "_x4"
    "_x8"
    "_d2"
    "_d4"
    "_d8"
};

const char* mIlImportUsageNames[IL_IMPORTUSAGE_LAST] = {
    "position",
    "pointsize",
    "color",
    "backcolor",
    "fog",
    "pixelSampleCoverage",
    "generic",
    "clipdistance",
    "culldistance",
    "primitiveid",
    "vertexid",
    "instanceid",
    "isfrontface",
    "lod",
    "coloring",
    "nodeColoring",
    "normal",
    "rendertargetArrayIndex",
    "viewportArrayIndex",
    "undefined",
    "sampleIndex",
    "edgeTessfactor",
    "insideTessfactor",
    "detailTessfactor",
    "densityTessfactor",
};

const char* mIlPixTexUsageNames[IL_USAGE_PIXTEX_LAST] = {
    "unknown",
    "1d",
    "2d",
    "3d",
    "cubemap",
    "2dmsaa",
    "4comp",
    "buffer",
    "1darray",
    "2darray",
    "2darraymsaa",
    "2dPlusW",
    "cubemapPlusW",
    "cubemapArray",
};

const char* mIlElementFormatNames[IL_ELEMENTFORMAT_LAST] = {
    "unknown",
    "snorm",
    "unorm",
    "sint",
    "uint",
    "float",
    "srgb",
    "mixed",
};

const char* mIlInterpModeNames[IL_INTERPMODE_LAST] = {
    "",
    "_interp(constant)",
    "_interp(linear)",
    "_interp(linearCentroid)",
    "_interp(linearNoperspective)",
    "_interp(linearNoperspectiveCentroid)",
    "_interp(linearSample)",
    "_interp(linearNoperspectiveSample)",
};

static const char* getComponentName(
    const char* name,
    uint8_t mask)
{
    return mask == 1 ? name : mIlModDstComponentNames[mask];
}

static void dumpDestination(
    const Destination* dst)
{
    logPrintRaw("%s%s %s%u",
                mIlShiftScaleNames[dst->shiftScale],
                dst->clamp ? "_sat" : "",
                mIlRegTypeNames[dst->registerType],
                dst->registerNum);

    if (dst->component[0] != IL_MODCOMP_WRITE ||
        dst->component[1] != IL_MODCOMP_WRITE ||
        dst->component[2] != IL_MODCOMP_WRITE ||
        dst->component[3] != IL_MODCOMP_WRITE) {
        logPrintRaw(".%s%s%s%s",
                    getComponentName("x", dst->component[0]),
                    getComponentName("y", dst->component[1]),
                    getComponentName("z", dst->component[2]),
                    getComponentName("w", dst->component[3]));
    }
}

static void dumpSource(
    const Source* src)
{
    logPrintRaw(" %s%u%s%s%s%s%s%s%s",
                mIlRegTypeNames[src->registerType],
                src->registerNum,
                src->invert ? "_invert" : "",
                src->bias && !src->x2 ? "_bias" : "",
                !src->bias && src->x2 ? "_x2" : "",
                src->bias && src->x2 ? "_bx2" : "",
                src->sign ? "_sign" : "",
                mIlDivCompNames[src->divComp],
                src->abs ? "_abs" : "");

    if (src->negate[0] || src->negate[1] || src->negate[2] || src->negate[3]) {
        logPrintRaw("_neg(%s%s%s%s)",
                    src->negate[0] ? "x" : "",
                    src->negate[1] ? "y" : "",
                    src->negate[2] ? "z" : "",
                    src->negate[3] ? "w" : "");
    }

    logPrintRaw("%s", src->clamp ? "_sat" : "");

    if (src->swizzle[0] != IL_COMPSEL_X_R ||
        src->swizzle[1] != IL_COMPSEL_Y_G ||
        src->swizzle[2] != IL_COMPSEL_Z_B ||
        src->swizzle[3] != IL_COMPSEL_W_A) {
        logPrintRaw(".%s%s%s%s",
                    mIlComponentSelectNames[src->swizzle[0]],
                    mIlComponentSelectNames[src->swizzle[1]],
                    mIlComponentSelectNames[src->swizzle[2]],
                    mIlComponentSelectNames[src->swizzle[3]]);
    }
}

static void dumpInstruction(
    const Instruction* instr)
{
    switch (instr->opcode) {
    case IL_OP_END:
        logPrintRaw("end");
        break;
    case IL_OP_MOV:
        logPrintRaw("mov");
        break;
    case IL_OP_RET_DYN:
        logPrintRaw("ret_dyn");
        break;
    case IL_DCL_OUTPUT:
        logPrintRaw("dcl_output_%s",
                    mIlImportUsageNames[instr->control]);
        break;
    case IL_DCL_INPUT:
        logPrintRaw("dcl_input_%s%s",
                    mIlImportUsageNames[getBits(instr->control, 0, 4)],
                    mIlInterpModeNames[getBits(instr->control, 5, 7)]);
        break;
    case IL_DCL_RESOURCE:
        logPrintRaw("dcl_resource_id(%d)_type(%s%s)_fmtx(%s)_fmty(%s)_fmtz(%s)_fmtw(%s)",
                    getBits(instr->control, 0, 7),
                    mIlPixTexUsageNames[getBits(instr->control, 8, 11)],
                    getBit(instr->control, 31) ? ",unnorm" : "",
                    mIlElementFormatNames[getBits(instr->extras[0], 20, 22)],
                    mIlElementFormatNames[getBits(instr->extras[0], 23, 25)],
                    mIlElementFormatNames[getBits(instr->extras[0], 26, 28)],
                    mIlElementFormatNames[getBits(instr->extras[0], 29, 31)]);
        break;
    case IL_OP_LOAD:
        // Sampler ID is ignored
        logPrintRaw("load_resource(%d)",
                    getBits(instr->control, 0, 7));
        break;
    case IL_DCL_GLOBAL_FLAGS:
        logPrintRaw("dcl_global_flags %s%s%s%s0",
                    getBit(instr->control, 0) ? "refactoringAllowed|" : "",
                    getBit(instr->control, 1) ? "forceEarlyDepthStencil|" : "",
                    getBit(instr->control, 2) ? "enableRawStructuredBuffers|" : "",
                    getBit(instr->control, 3) ? "enableDoublePrecisionFloatOps|" : "");
        break;
    default:
        logPrintRaw("%d?", instr->opcode);
        break;
    }

    assert(instr->dstCount <= 1);
    for (int i = 0; i < instr->dstCount; i++) {
        dumpDestination(&instr->dsts[i]);
    }

    for (int i = 0; i < instr->srcCount; i++) {
        dumpSource(&instr->srcs[i]);
    }

    logPrintRaw("\n");
}

void ilcDumpKernel(
    const Kernel* kernel)
{
    logPrintRaw("%s\nil_%s_%d_%d%s%s\n",
                mIlLanguageTypeNames[kernel->clientType],
                mIlShaderTypeNames[kernel->shaderType],
                kernel->majorVersion, kernel->minorVersion,
                kernel->multipass ? "_mp" : "", kernel->realtime ? "_rt" : "");

    for (int i = 0; i < kernel->instrCount; i++) {
        dumpInstruction(&kernel->instrs[i]);
    }
}
