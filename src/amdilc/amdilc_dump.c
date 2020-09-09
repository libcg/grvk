#include "amdilc_internal.h"

const char* mIlShaderTypeNames[IL_SHADER_LAST] = {
    "vs",
    "ps",
    "gs",
    "cs",
    "hs",
    "ds",
};

static const char* mIlLanguageTypeNames[IL_LANG_LAST] = {
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

static const char* mIlRegTypeNames[IL_REGTYPE_LAST] = {
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
    "l",
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

static const char* mIlDivCompNames[IL_DIVCOMP_LAST] = {
    "",
    "_divComp(y)",
    "_divComp(z)",
    "_divComp(w)",
    "_divComp(unknown)",
};

static const char* mIlZeroOpNames[IL_ZEROOP_LAST] = {
    "fltmax",
    "zero",
    "infinity",
    "inf_else_max",
};

static const char* mIlModDstComponentNames[IL_MODCOMP_LAST] = {
    "_",
    "?", // Replaced with x, y, z or w
    "0",
    "1",
};

static const char* mIlComponentSelectNames[IL_COMPSEL_LAST] = {
    "x",
    "y",
    "z",
    "w",
    "0",
    "1",
};

static const char* mIlShiftScaleNames[IL_SHIFT_LAST] = {
    "",
    "_x2"
    "_x4"
    "_x8"
    "_d2"
    "_d4"
    "_d8"
};

static const char* mIlImportUsageNames[IL_IMPORTUSAGE_LAST] = {
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

static const char* mIlPixTexUsageNames[IL_USAGE_PIXTEX_LAST] = {
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

static const char* mIlElementFormatNames[IL_ELEMENTFORMAT_LAST] = {
    "unknown",
    "snorm",
    "unorm",
    "sint",
    "uint",
    "float",
    "srgb",
    "mixed",
};

static const char* mIlInterpModeNames[IL_INTERPMODE_LAST] = {
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

static void dumpGlobalFlags(
    const uint16_t flags)
{
    const char* flagNames[] = {
        "refactoringAllowed",
        "forceEarlyDepthStencil",
        "enableRawStructuredBuffers",
        "enableDoublePrecisionFloatOps",
    };

    bool first = true;
    for (int i = 0; i < sizeof(flagNames) / sizeof(flagNames[0]); i++) {
        if (GET_BIT(flags, i)) {
            logPrintRaw("%s%s",
                        first ? " " : "|",
                        flagNames[i]);

            first = false;
        }
    }
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
    logPrintRaw(" %s%u", mIlRegTypeNames[src->registerType], src->registerNum);

    if (src->swizzle[0] != IL_COMPSEL_X_R ||
        src->swizzle[1] != IL_COMPSEL_Y_G ||
        src->swizzle[2] != IL_COMPSEL_Z_B ||
        src->swizzle[3] != IL_COMPSEL_W_A) {
        if (src->swizzle[0] == src->swizzle[1] &&
            src->swizzle[1] == src->swizzle[2] &&
            src->swizzle[2] == src->swizzle[3]) {
            logPrintRaw(".%s", mIlComponentSelectNames[src->swizzle[0]]);
        } else {
            logPrintRaw(".%s%s%s%s",
                        mIlComponentSelectNames[src->swizzle[0]],
                        mIlComponentSelectNames[src->swizzle[1]],
                        mIlComponentSelectNames[src->swizzle[2]],
                        mIlComponentSelectNames[src->swizzle[3]]);
        }
    }

    if (src->negate[0] || src->negate[1] || src->negate[2] || src->negate[3]) {
        logPrintRaw("_neg(%s%s%s%s)",
                    src->negate[0] ? "x" : "",
                    src->negate[1] ? "y" : "",
                    src->negate[2] ? "z" : "",
                    src->negate[3] ? "w" : "");
    }

    logPrintRaw("%s%s%s%s%s%s%s",
                src->invert ? "_invert" : "",
                src->bias && !src->x2 ? "_bias" : "",
                !src->bias && src->x2 ? "_x2" : "",
                src->bias && src->x2 ? "_bx2" : "",
                src->sign ? "_sign" : "",
                mIlDivCompNames[src->divComp],
                src->abs ? "_abs" : "");

    logPrintRaw("%s", src->clamp ? "_sat" : "");
}

static void dumpInstruction(
    const Instruction* instr)
{
    static int indentLevel = 0; // TODO move to context

    switch (instr->opcode) {
    case IL_OP_ELSE:
    case IL_OP_ENDIF:
    case IL_OP_ENDLOOP:
        indentLevel--;
        break;
    }

    for (int i = 0; i < indentLevel; i++) {
        logPrintRaw("    ");
    }

    switch (instr->opcode) {
    case IL_OP_ABS:
        logPrintRaw("abs");
        break;
    case IL_OP_ACOS:
        logPrintRaw("acos");
        break;
    case IL_OP_ADD:
        logPrintRaw("add");
        break;
    case IL_OP_ASIN:
        logPrintRaw("asin");
        break;
    case IL_OP_ATAN:
        logPrintRaw("atan");
        break;
    case IL_OP_BREAK:
        logPrintRaw("break");
        break;
    case IL_OP_CONTINUE:
        logPrintRaw("continue");
        break;
    case IL_OP_DIV:
        logPrintRaw("div_zeroop(%s)", mIlZeroOpNames[instr->control]);
        break;
    case IL_OP_DP3:
        logPrintRaw("dp3%s", GET_BIT(instr->control, 0) ? "_ieee" : "");
        break;
    case IL_OP_DP4:
        logPrintRaw("dp4%s", GET_BIT(instr->control, 0) ? "_ieee" : "");
        break;
    case IL_OP_ELSE:
        logPrintRaw("else");
        indentLevel++;
        break;
    case IL_OP_END:
        logPrintRaw("end");
        break;
    case IL_OP_ENDIF:
        logPrintRaw("endif");
        break;
    case IL_OP_ENDLOOP:
        logPrintRaw("endloop");
        break;
    case IL_OP_FRC:
        logPrintRaw("frc");
        break;
    case IL_OP_MAD:
        logPrintRaw("mad%s", GET_BIT(instr->control, 0) ? "_ieee" : "");
        break;
    case IL_OP_MAX:
        logPrintRaw("max%s", GET_BIT(instr->control, 0) ? "_ieee" : "");
        break;
    case IL_OP_MIN:
        logPrintRaw("min%s", GET_BIT(instr->control, 0) ? "_ieee" : "");
        break;
    case IL_OP_MOV:
        logPrintRaw("mov");
        break;
    case IL_OP_MUL:
        logPrintRaw("mul%s", GET_BIT(instr->control, 0) ? "_ieee" : "");
        break;
    case IL_OP_BREAK_LOGICALZ:
        logPrintRaw("break_logicalz");
        break;
    case IL_OP_BREAK_LOGICALNZ:
        logPrintRaw("break_logicalnz");
        break;
    case IL_OP_IF_LOGICALZ:
        logPrintRaw("if_logicalz");
        indentLevel++;
        break;
    case IL_OP_IF_LOGICALNZ:
        logPrintRaw("if_logicalnz");
        indentLevel++;
        break;
    case IL_OP_WHILE:
        logPrintRaw("whileloop");
        indentLevel++;
        break;
    case IL_OP_RET_DYN:
        logPrintRaw("ret_dyn");
        break;
    case IL_DCL_LITERAL:
        logPrintRaw("dcl_literal");
        break;
    case IL_DCL_OUTPUT:
        logPrintRaw("dcl_output_%s", mIlImportUsageNames[instr->control]);
        break;
    case IL_DCL_INPUT:
        logPrintRaw("dcl_input_%s%s",
                    mIlImportUsageNames[GET_BITS(instr->control, 0, 4)],
                    mIlInterpModeNames[GET_BITS(instr->control, 5, 7)]);
        break;
    case IL_DCL_RESOURCE:
        logPrintRaw("dcl_resource_id(%d)_type(%s%s)_fmtx(%s)_fmty(%s)_fmtz(%s)_fmtw(%s)",
                    GET_BITS(instr->control, 0, 7),
                    mIlPixTexUsageNames[GET_BITS(instr->control, 8, 11)],
                    GET_BIT(instr->control, 31) ? ",unnorm" : "",
                    mIlElementFormatNames[GET_BITS(instr->extras[0], 20, 22)],
                    mIlElementFormatNames[GET_BITS(instr->extras[0], 23, 25)],
                    mIlElementFormatNames[GET_BITS(instr->extras[0], 26, 28)],
                    mIlElementFormatNames[GET_BITS(instr->extras[0], 29, 31)]);
        break;
    case IL_OP_LOAD:
        // Sampler ID is ignored
        logPrintRaw("load_resource(%d)", GET_BITS(instr->control, 0, 7));
        break;
    case IL_OP_I_NOT:
        logPrintRaw("inot");
        break;
    case IL_OP_I_OR:
        logPrintRaw("ior");
        break;
    case IL_OP_I_ADD:
        logPrintRaw("iadd");
        break;
    case IL_OP_I_EQ:
        logPrintRaw("ieq");
        break;
    case IL_OP_I_GE:
        logPrintRaw("ige");
        break;
    case IL_OP_I_LT:
        logPrintRaw("ilt");
        break;
    case IL_OP_FTOI:
        logPrintRaw("ftoi");
        break;
    case IL_OP_ITOF:
        logPrintRaw("itof");
        break;
    case IL_OP_AND:
        logPrintRaw("iand"); // IL_OP_I_AND doesn't exist
        break;
    case IL_OP_CMOV_LOGICAL:
        logPrintRaw("cmov_logical");
        break;
    case IL_OP_EQ:
        logPrintRaw("eq");
        break;
    case IL_OP_EXP_VEC:
        logPrintRaw("exp_vec");
        break;
    case IL_OP_GE:
        logPrintRaw("ge");
        break;
    case IL_OP_LOG_VEC:
        logPrintRaw("log_vec");
        break;
    case IL_OP_LT:
        logPrintRaw("lt");
        break;
    case IL_OP_NE:
        logPrintRaw("ne");
        break;
    case IL_OP_ROUND_NEG_INF:
        logPrintRaw("round_neginf");
        break;
    case IL_OP_RSQ_VEC:
        logPrintRaw("rsq_vec");
        break;
    case IL_OP_SIN_VEC:
        logPrintRaw("sin_vec");
        break;
    case IL_OP_COS_VEC:
        logPrintRaw("cos_vec");
        break;
    case IL_OP_SQRT_VEC:
        logPrintRaw("sqrt_vec");
        break;
    case IL_OP_DP2:
        logPrintRaw("dp2%s", GET_BIT(instr->control, 0) ? "_ieee" : "");
        break;
    case IL_OP_U_BIT_EXTRACT:
        logPrintRaw("ubit_extract");
        break;
    case IL_DCL_GLOBAL_FLAGS:
        logPrintRaw("dcl_global_flags");
        break;
    default:
        logPrintRaw("%d?\n", instr->opcode);
        return;
    }

    assert(instr->dstCount <= 1);
    for (int i = 0; i < instr->dstCount; i++) {
        dumpDestination(&instr->dsts[i]);
    }

    for (int i = 0; i < instr->srcCount; i++) {
        if (i > 0 || (i == 0 && instr->dstCount > 0)) {
            logPrintRaw(",");
        }

        dumpSource(&instr->srcs[i]);
    }

    if (instr->opcode == IL_DCL_LITERAL) {
        for (int i = 0; i < instr->extraCount; i++) {
            logPrintRaw(", 0x%08X", instr->extras[i]);
        }
    } else if (instr->opcode == IL_DCL_GLOBAL_FLAGS) {
        dumpGlobalFlags(instr->control);
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
