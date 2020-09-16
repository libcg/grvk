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
    "x",
    "cb",
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
    "_interp(linear_centroid)",
    "_interp(linear_noperspective)",
    "_interp(linear_noperspective_centroid)",
    "_interp(linear_sample)",
    "_interp(linear_noperspective_sample)",
};

static const char* getComponentName(
    const char* name,
    uint8_t mask)
{
    return mask == 1 ? name : mIlModDstComponentNames[mask];
}

static void dumpGlobalFlags(
    FILE* file,
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
            fprintf(file, "%s%s", first ? " " : "|", flagNames[i]);
            first = false;
        }
    }
}

static void dumpDestination(
    FILE* file,
    const Destination* dst)
{
    fprintf(file, "%s%s %s%u",
            mIlShiftScaleNames[dst->shiftScale],
            dst->clamp ? "_sat" : "",
            mIlRegTypeNames[dst->registerType],
            dst->registerNum);

    if (dst->component[0] != IL_MODCOMP_WRITE ||
        dst->component[1] != IL_MODCOMP_WRITE ||
        dst->component[2] != IL_MODCOMP_WRITE ||
        dst->component[3] != IL_MODCOMP_WRITE) {
        fprintf(file, ".%s%s%s%s",
                getComponentName("x", dst->component[0]),
                getComponentName("y", dst->component[1]),
                getComponentName("z", dst->component[2]),
                getComponentName("w", dst->component[3]));
    }
}

static void dumpSource(
    FILE* file,
    const Source* src)
{
    fprintf(file, "%s%u", mIlRegTypeNames[src->registerType], src->registerNum);

    if (src->registerType == IL_REGTYPE_ITEMP ||
        src->registerType == IL_REGTYPE_CONST_BUFF) {
        bool indexed = src->hasImmediate || src->hasRelativeSrc;

        if (indexed) {
            fprintf(file, "[");
        }
        if (src->hasRelativeSrc) {
            dumpSource(file, src->relativeSrc);
        }
        if (src->hasImmediate && src->hasRelativeSrc) {
            fprintf(file, "+");
        }
        if (src->hasImmediate) {
            fprintf(file, "%u", src->immediate);
        }
        if (indexed) {
            fprintf(file, "]");
        }
    } else {
        if (src->hasImmediate) {
            LOGW("unhandled immediate value\n");
        }
        if (src->hasRelativeSrc) {
            LOGW("unhandled relative source\n");
        }
    }

    if (src->swizzle[0] != IL_COMPSEL_X_R ||
        src->swizzle[1] != IL_COMPSEL_Y_G ||
        src->swizzle[2] != IL_COMPSEL_Z_B ||
        src->swizzle[3] != IL_COMPSEL_W_A) {
        if (src->swizzle[0] == src->swizzle[1] &&
            src->swizzle[1] == src->swizzle[2] &&
            src->swizzle[2] == src->swizzle[3]) {
            fprintf(file, ".%s", mIlComponentSelectNames[src->swizzle[0]]);
        } else {
            fprintf(file, ".%s%s%s%s",
                    mIlComponentSelectNames[src->swizzle[0]],
                    mIlComponentSelectNames[src->swizzle[1]],
                    mIlComponentSelectNames[src->swizzle[2]],
                    mIlComponentSelectNames[src->swizzle[3]]);
        }
    }

    if (src->negate[0] || src->negate[1] || src->negate[2] || src->negate[3]) {
        fprintf(file, "_neg(%s%s%s%s)",
                src->negate[0] ? "x" : "",
                src->negate[1] ? "y" : "",
                src->negate[2] ? "z" : "",
                src->negate[3] ? "w" : "");
    }

    fprintf(file, "%s%s%s%s%s%s%s",
            src->invert ? "_invert" : "",
            src->bias && !src->x2 ? "_bias" : "",
            !src->bias && src->x2 ? "_x2" : "",
            src->bias && src->x2 ? "_bx2" : "",
            src->sign ? "_sign" : "",
            mIlDivCompNames[src->divComp],
            src->abs ? "_abs" : "");

    fprintf(file, "%s", src->clamp ? "_sat" : "");
}

static void dumpInstruction(
    FILE* file,
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
        fprintf(file, "    ");
    }

    switch (instr->opcode) {
    case IL_OP_ABS:
        fprintf(file, "abs");
        break;
    case IL_OP_ACOS:
        fprintf(file, "acos");
        break;
    case IL_OP_ADD:
        fprintf(file, "add");
        break;
    case IL_OP_ASIN:
        fprintf(file, "asin");
        break;
    case IL_OP_ATAN:
        fprintf(file, "atan");
        break;
    case IL_OP_BREAK:
        fprintf(file, "break");
        break;
    case IL_OP_CONTINUE:
        fprintf(file, "continue");
        break;
    case IL_OP_DIV:
        fprintf(file, "div_zeroop(%s)", mIlZeroOpNames[instr->control]);
        break;
    case IL_OP_DP3:
        fprintf(file, "dp3%s", GET_BIT(instr->control, 0) ? "_ieee" : "");
        break;
    case IL_OP_DP4:
        fprintf(file, "dp4%s", GET_BIT(instr->control, 0) ? "_ieee" : "");
        break;
    case IL_OP_DSX:
        fprintf(file, "dsx%s", GET_BIT(instr->control, 7) ? "_fine" : "");
        break;
    case IL_OP_DSY:
        fprintf(file, "dsy%s", GET_BIT(instr->control, 7) ? "_fine" : "");
        break;
    case IL_OP_ELSE:
        fprintf(file, "else");
        indentLevel++;
        break;
    case IL_OP_END:
        fprintf(file, "end");
        break;
    case IL_OP_ENDIF:
        fprintf(file, "endif");
        break;
    case IL_OP_ENDLOOP:
        fprintf(file, "endloop");
        break;
    case IL_OP_ENDMAIN:
        fprintf(file, "endmain");
        break;
    case IL_OP_FRC:
        fprintf(file, "frc");
        break;
    case IL_OP_MAD:
        fprintf(file, "mad%s", GET_BIT(instr->control, 0) ? "_ieee" : "");
        break;
    case IL_OP_MAX:
        fprintf(file, "max%s", GET_BIT(instr->control, 0) ? "_ieee" : "");
        break;
    case IL_OP_MIN:
        fprintf(file, "min%s", GET_BIT(instr->control, 0) ? "_ieee" : "");
        break;
    case IL_OP_MOV:
        fprintf(file, "mov");
        break;
    case IL_OP_MUL:
        fprintf(file, "mul%s", GET_BIT(instr->control, 0) ? "_ieee" : "");
        break;
    case IL_OP_BREAK_LOGICALZ:
        fprintf(file, "break_logicalz");
        break;
    case IL_OP_BREAK_LOGICALNZ:
        fprintf(file, "break_logicalnz");
        break;
    case IL_OP_IF_LOGICALZ:
        fprintf(file, "if_logicalz");
        indentLevel++;
        break;
    case IL_OP_IF_LOGICALNZ:
        fprintf(file, "if_logicalnz");
        indentLevel++;
        break;
    case IL_OP_WHILE:
        fprintf(file, "whileloop");
        indentLevel++;
        break;
    case IL_OP_RET_DYN:
        fprintf(file, "ret_dyn");
        break;
    case IL_DCL_CONST_BUFFER:
        if (GET_BIT(instr->control, 15)) {
            LOGW("unhandled immediate constant buffer\n");
        }
        fprintf(file, "dcl_cb");
        break;
    case IL_DCL_INDEXED_TEMP_ARRAY:
        fprintf(file, "dcl_indexed_temp_array");
        break;
    case IL_DCL_LITERAL:
        fprintf(file, "dcl_literal");
        break;
    case IL_DCL_OUTPUT:
        fprintf(file, "dcl_output_%s", mIlImportUsageNames[instr->control]);
        break;
    case IL_DCL_INPUT:
        fprintf(file, "dcl_input_%s%s",
                mIlImportUsageNames[GET_BITS(instr->control, 0, 4)],
                mIlInterpModeNames[GET_BITS(instr->control, 5, 7)]);
        break;
    case IL_DCL_RESOURCE:
        fprintf(file, "dcl_resource_id(%u)_type(%s%s)_fmtx(%s)_fmty(%s)_fmtz(%s)_fmtw(%s)",
                GET_BITS(instr->control, 0, 7),
                mIlPixTexUsageNames[GET_BITS(instr->control, 8, 11)],
                GET_BIT(instr->control, 31) ? ",unnorm" : "",
                mIlElementFormatNames[GET_BITS(instr->extras[0], 20, 22)],
                mIlElementFormatNames[GET_BITS(instr->extras[0], 23, 25)],
                mIlElementFormatNames[GET_BITS(instr->extras[0], 26, 28)],
                mIlElementFormatNames[GET_BITS(instr->extras[0], 29, 31)]);
        break;
    case IL_OP_DISCARD_LOGICALNZ:
        fprintf(file, "discard_logicalnz");
        break;
    case IL_OP_LOAD:
        // Sampler ID is ignored
        fprintf(file, "load_resource(%u)", GET_BITS(instr->control, 0, 7));
        break;
    case IL_OP_RESINFO:
        if (GET_BITS(instr->control, 9, 15)) {
            LOGW("unhandled resinfo flags 0x%X\n", instr->control);
        }
        fprintf(file, "resinfo_resource(%u)%s",
                GET_BITS(instr->control, 0, 7),
                GET_BIT(instr->control, 8) ? "_uint" : "");
        break;
    case IL_OP_SAMPLE:
        if (GET_BITS(instr->control, 12, 15)) {
            LOGW("unhandled sample flags 0x%X\n", instr->control);
        }
        fprintf(file, "sample_resource(%u)_sampler(%u)",
                GET_BITS(instr->control, 0, 7), GET_BITS(instr->control, 8, 11));
        break;
    case IL_OP_SAMPLE_B:
        if (GET_BITS(instr->control, 12, 15)) {
            LOGW("unhandled sample_b flags 0x%X\n", instr->control);
        }
        fprintf(file, "sample_b_resource(%u)_sampler(%u)",
                GET_BITS(instr->control, 0, 7), GET_BITS(instr->control, 8, 11));
        break;
    case IL_OP_SAMPLE_G:
        if (GET_BITS(instr->control, 12, 15)) {
            LOGW("unhandled sample_g flags 0x%X\n", instr->control);
        }
        fprintf(file, "sample_g_resource(%u)_sampler(%u)",
                GET_BITS(instr->control, 0, 7), GET_BITS(instr->control, 8, 11));
        break;
    case IL_OP_SAMPLE_L:
        if (GET_BITS(instr->control, 12, 15)) {
            LOGW("unhandled sample_l flags 0x%X\n", instr->control);
        }
        fprintf(file, "sample_l_resource(%u)_sampler(%u)",
                GET_BITS(instr->control, 0, 7), GET_BITS(instr->control, 8, 11));
        break;
    case IL_OP_SAMPLE_C_LZ:
        if (GET_BITS(instr->control, 12, 15)) {
            LOGW("unhandled sample_c_lz flags 0x%X\n", instr->control);
        }
        fprintf(file, "sample_c_lz_resource(%u)_sampler(%u)",
                GET_BITS(instr->control, 0, 7), GET_BITS(instr->control, 8, 11));
        break;
    case IL_OP_I_NOT:
        fprintf(file, "inot");
        break;
    case IL_OP_I_OR:
        fprintf(file, "ior");
        break;
    case IL_OP_I_ADD:
        fprintf(file, "iadd");
        break;
    case IL_OP_I_MAD:
        fprintf(file, "imad");
        break;
    case IL_OP_I_MUL:
        fprintf(file, "imul");
        break;
    case IL_OP_I_EQ:
        fprintf(file, "ieq");
        break;
    case IL_OP_I_GE:
        fprintf(file, "ige");
        break;
    case IL_OP_I_LT:
        fprintf(file, "ilt");
        break;
    case IL_OP_I_NEGATE:
        fprintf(file, "inegate");
        break;
    case IL_OP_I_NE:
        fprintf(file, "ine");
        break;
    case IL_OP_I_SHL:
        fprintf(file, "ishl");
        break;
    case IL_OP_U_SHR:
        fprintf(file, "ushr");
        break;
    case IL_OP_U_DIV:
        fprintf(file, "udiv");
        break;
    case IL_OP_U_MOD:
        fprintf(file, "umod");
        break;
    case IL_OP_U_LT:
        fprintf(file, "ult");
        break;
    case IL_OP_FTOI:
        fprintf(file, "ftoi");
        break;
    case IL_OP_FTOU:
        fprintf(file, "ftou");
        break;
    case IL_OP_ITOF:
        fprintf(file, "itof");
        break;
    case IL_OP_UTOF:
        fprintf(file, "utof");
        break;
    case IL_OP_AND:
        fprintf(file, "iand"); // IL_OP_I_AND doesn't exist
        break;
    case IL_OP_CMOV_LOGICAL:
        fprintf(file, "cmov_logical");
        break;
    case IL_OP_EQ:
        fprintf(file, "eq");
        break;
    case IL_OP_EXP_VEC:
        fprintf(file, "exp_vec");
        break;
    case IL_OP_GE:
        fprintf(file, "ge");
        break;
    case IL_OP_LOG_VEC:
        fprintf(file, "log_vec");
        break;
    case IL_OP_LT:
        fprintf(file, "lt");
        break;
    case IL_OP_NE:
        fprintf(file, "ne");
        break;
    case IL_OP_ROUND_NEG_INF:
        fprintf(file, "round_neginf");
        break;
    case IL_OP_ROUND_PLUS_INF:
        fprintf(file, "round_plusinf");
        break;
    case IL_OP_ROUND_ZERO:
        fprintf(file, "round_z");
        break;
    case IL_OP_RSQ_VEC:
        fprintf(file, "rsq_vec");
        break;
    case IL_OP_SIN_VEC:
        fprintf(file, "sin_vec");
        break;
    case IL_OP_COS_VEC:
        fprintf(file, "cos_vec");
        break;
    case IL_OP_SQRT_VEC:
        fprintf(file, "sqrt_vec");
        break;
    case IL_OP_DP2:
        fprintf(file, "dp2%s", GET_BIT(instr->control, 0) ? "_ieee" : "");
        break;
    case IL_OP_DCL_UAV:
        fprintf(file, "dcl_uav_id(%u)_type(%s)_fmtx(%s)",
                GET_BITS(instr->control, 0, 3),
                mIlPixTexUsageNames[GET_BITS(instr->control, 8, 11)],
                mIlElementFormatNames[GET_BITS(instr->control, 4, 7)]);
        break;
    case IL_OP_UAV_LOAD:
        fprintf(file, "uav_load_id(%u)", GET_BITS(instr->control, 0, 13));
        break;
    case IL_OP_UAV_STORE:
        fprintf(file, "uav_store_id(%u)", GET_BITS(instr->control, 0, 13));
        break;
    case IL_OP_UAV_ADD:
        fprintf(file, "uav_add_id(%u)", GET_BITS(instr->control, 0, 13));
        break;
    case IL_OP_UAV_READ_ADD:
        fprintf(file, "uav_read_add_id(%u)", GET_BITS(instr->control, 0, 13));
        break;
    case IL_OP_DCL_STRUCT_SRV:
        fprintf(file, "dcl_struct_srv_id(%u) %u",
                GET_BITS(instr->control, 0, 13), instr->extras[0]);
        break;
    case IL_OP_SRV_STRUCT_LOAD:
        fprintf(file, "srv_struct_load%s_id(%u)",
                GET_BIT(instr->control, 12) ? "_ext" : "", GET_BITS(instr->control, 0, 7));
        break;
    case IL_OP_U_BIT_EXTRACT:
        fprintf(file, "ubit_extract");
        break;
    case IL_DCL_GLOBAL_FLAGS:
        fprintf(file, "dcl_global_flags");
        break;
    case IL_OP_DCL_TYPED_UAV:
        // FIXME guessed from IL_OP_DCL_UAV
        fprintf(file, "dcl_typed_uav_id(%u)_type(%s)_fmtx(%s)",
                GET_BITS(instr->control, 0, 13),
                mIlPixTexUsageNames[GET_BITS(instr->extras[0], 0, 3)],
                mIlElementFormatNames[GET_BITS(instr->extras[0], 4, 9)]);
        break;
    case IL_UNK_660:
        fprintf(file, "unk_%u", instr->opcode);
        break;
    default:
        fprintf(file, "%u?\n", instr->opcode);
        return;
    }

    assert(instr->dstCount <= 1);
    for (int i = 0; i < instr->dstCount; i++) {
        dumpDestination(file, &instr->dsts[i]);
    }

    for (int i = 0; i < instr->srcCount; i++) {
        if (i > 0 || (i == 0 && instr->dstCount > 0)) {
            fprintf(file, ",");
        }

        fprintf(file, " ");
        dumpSource(file, &instr->srcs[i]);
    }

    if (instr->opcode == IL_DCL_LITERAL) {
        for (int i = 0; i < instr->extraCount; i++) {
            fprintf(file, ", 0x%08X", instr->extras[i]);
        }
    } else if (instr->opcode == IL_DCL_GLOBAL_FLAGS) {
        dumpGlobalFlags(file, instr->control);
    }

    fprintf(file, "\n");
}

void ilcDumpKernel(
    FILE* file,
    const Kernel* kernel)
{
    fprintf(file, "%s\nil_%s_%d_%d%s%s\n",
            mIlLanguageTypeNames[kernel->clientType],
            mIlShaderTypeNames[kernel->shaderType],
            kernel->majorVersion, kernel->minorVersion,
            kernel->multipass ? "_mp" : "", kernel->realtime ? "_rt" : "");

    for (int i = 0; i < kernel->instrCount; i++) {
        dumpInstruction(file, &kernel->instrs[i]);
    }
}
