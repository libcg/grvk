#include "amdilc_internal.h"

typedef struct {
    uint16_t opcode;
    uint8_t dstCount;
    uint8_t srcCount;
    uint8_t extraCount;
} OpcodeInfo;

static const OpcodeInfo mOpcodeInfos[IL_OP_LAST] = {
    [IL_OP_ABS] = { IL_OP_ABS, 1, 1, 0 },
    [IL_OP_ACOS] = { IL_OP_ACOS, 1, 1, 0 },
    [IL_OP_ADD] = { IL_OP_ADD, 1, 2, 0 },
    [IL_OP_ASIN] = { IL_OP_ASIN, 1, 1, 0 },
    [IL_OP_ATAN] = { IL_OP_ATAN, 1, 1, 0 },
    [IL_OP_BREAK] = { IL_OP_BREAK, 0, 0, 0 },
    [IL_OP_BREAKC] = { IL_OP_BREAKC, 0, 2, 0 },
    [IL_OP_CONTINUE] = { IL_OP_CONTINUE, 0, 0, 0 },
    [IL_OP_DCLARRAY] = { IL_OP_DCLARRAY, 0, 2, 0 },
    [IL_OP_DIV] = { IL_OP_DIV, 1, 2, 0 },
    [IL_OP_DP3] = { IL_OP_DP3, 1, 2, 0 },
    [IL_OP_DP4] = { IL_OP_DP4, 1, 2, 0 },
    [IL_OP_DSX] = { IL_OP_DSX, 1, 1, 0 },
    [IL_OP_DSY] = { IL_OP_DSY, 1, 1, 0 },
    [IL_OP_ELSE] = { IL_OP_ELSE, 0, 0, 0 },
    [IL_OP_END] = { IL_OP_END, 0, 0, 0 },
    [IL_OP_ENDIF] = { IL_OP_ENDIF, 0, 0, 0 },
    [IL_OP_ENDLOOP] = { IL_OP_ENDLOOP, 0, 0, 0 },
    [IL_OP_ENDMAIN] = { IL_OP_ENDMAIN, 0, 0, 0 },
    [IL_OP_FRC] = { IL_OP_FRC, 1, 1, 0 },
    [IL_OP_MAD] = { IL_OP_MAD, 1, 3, 0 },
    [IL_OP_MAX] = { IL_OP_MAX, 1, 2, 0 },
    [IL_OP_MIN] = { IL_OP_MIN, 1, 2, 0 },
    [IL_OP_MOV] = { IL_OP_MOV, 1, 1, 0 },
    [IL_OP_MUL] = { IL_OP_MUL, 1, 2, 0 },
    [IL_OP_BREAK_LOGICALZ] = { IL_OP_BREAK_LOGICALZ, 0, 1, 0 },
    [IL_OP_BREAK_LOGICALNZ] = { IL_OP_BREAK_LOGICALNZ, 0, 1, 0 },
    [IL_OP_CASE] = { IL_OP_CASE, 0, 0, 1 },
    [IL_OP_CONTINUE_LOGICALZ] = { IL_OP_CONTINUE_LOGICALZ, 0, 1, 0},
    [IL_OP_CONTINUE_LOGICALNZ] = { IL_OP_CONTINUE_LOGICALNZ, 0, 1, 0},
    [IL_OP_DEFAULT] = { IL_OP_DEFAULT, 0, 0, 0 },
    [IL_OP_ENDSWITCH] = { IL_OP_ENDSWITCH, 0, 0, 0 },
    [IL_OP_IF_LOGICALZ] = { IL_OP_IF_LOGICALZ, 0, 1, 0 },
    [IL_OP_IF_LOGICALNZ] = { IL_OP_IF_LOGICALNZ, 0, 1, 0 },
    [IL_OP_WHILE] = { IL_OP_WHILE, 0, 0, 0 },
    [IL_OP_SWITCH] = { IL_OP_SWITCH, 0, 1, 0 },
    [IL_OP_RET_DYN] = { IL_OP_RET_DYN, 0, 0, 0 },
    [IL_DCL_CONST_BUFFER] = { IL_DCL_CONST_BUFFER, 0, 0, 0 },
    [IL_DCL_INDEXED_TEMP_ARRAY] = { IL_DCL_INDEXED_TEMP_ARRAY, 0, 1, 0 },
    [IL_DCL_LITERAL] = { IL_DCL_LITERAL, 0, 1, 4 },
    [IL_DCL_OUTPUT] = { IL_DCL_OUTPUT, 1, 0, 0 },
    [IL_DCL_INPUT] = { IL_DCL_INPUT, 1, 0, 0 },
    [IL_DCL_RESOURCE] = { IL_DCL_RESOURCE, 0, 0, 1 },
    [IL_OP_DISCARD_LOGICALZ] = { IL_OP_DISCARD_LOGICALZ, 0, 1, 0 },
    [IL_OP_DISCARD_LOGICALNZ] = { IL_OP_DISCARD_LOGICALNZ, 0, 1, 0 },
    [IL_OP_LOAD] = { IL_OP_LOAD, 1, 1, 0 },
    [IL_OP_RESINFO] = { IL_OP_RESINFO, 1, 1, 0 },
    [IL_OP_SAMPLE] = { IL_OP_SAMPLE, 1, 1, 0 },
    [IL_OP_SAMPLE_B] = { IL_OP_SAMPLE_B, 1, 2, 0 },
    [IL_OP_SAMPLE_G] = { IL_OP_SAMPLE_G, 1, 3, 0 },
    [IL_OP_SAMPLE_L] = { IL_OP_SAMPLE_L, 1, 2, 0 },
    [IL_OP_SAMPLE_C_LZ] = { IL_OP_SAMPLE_C_LZ, 1, 2, 0 },
    [IL_OP_I_NOT] = { IL_OP_I_NOT, 1, 1, 0 },
    [IL_OP_I_OR] = { IL_OP_I_OR, 1, 2, 0 },
    [IL_OP_I_XOR] = { IL_OP_I_XOR, 1, 2, 0 },
    [IL_OP_I_ADD] = { IL_OP_I_ADD, 1, 2, 0 },
    [IL_OP_I_MAD] = { IL_OP_I_MAD, 1, 3, 0 },
    [IL_OP_I_MAX] = { IL_OP_I_MAX, 1, 2, 0 },
    [IL_OP_I_MIN] = { IL_OP_I_MIN, 1, 2, 0 },
    [IL_OP_I_MUL] = { IL_OP_I_MUL, 1, 2, 0 },
    [IL_OP_I_EQ] = { IL_OP_I_EQ, 1, 2, 0 },
    [IL_OP_I_GE] = { IL_OP_I_GE, 1, 2, 0 },
    [IL_OP_I_LT] = { IL_OP_I_LT, 1, 2, 0 },
    [IL_OP_I_NEGATE] = { IL_OP_I_NEGATE, 1, 1, 0 },
    [IL_OP_I_NE] = { IL_OP_I_NE, 1, 2, 0 },
    [IL_OP_I_SHL] = { IL_OP_I_SHL, 1, 2, 0 },
    [IL_OP_I_SHR] = { IL_OP_I_SHR, 1, 2, 0 },
    [IL_OP_U_SHR] = { IL_OP_U_SHR, 1, 2, 0 },
    [IL_OP_U_DIV] = { IL_OP_U_DIV, 1, 2, 0 },
    [IL_OP_U_MOD] = { IL_OP_U_MOD, 1, 2, 0 },
    [IL_OP_U_MAX] = { IL_OP_U_MAX, 1, 2, 0 },
    [IL_OP_U_MIN] = { IL_OP_U_MIN, 1, 2, 0 },
    [IL_OP_U_LT] = { IL_OP_U_LT, 1, 2, 0 },
    [IL_OP_U_GE] = { IL_OP_U_GE, 1, 2, 0 },
    [IL_OP_FTOI] = { IL_OP_FTOI, 1, 1, 0 },
    [IL_OP_FTOU] = { IL_OP_FTOU, 1, 1, 0 },
    [IL_OP_ITOF] = { IL_OP_ITOF, 1, 1, 0 },
    [IL_OP_UTOF] = { IL_OP_UTOF, 1, 1, 0 },
    [IL_OP_AND] = { IL_OP_AND, 1, 2, 0 },
    [IL_OP_CMOV_LOGICAL] = { IL_OP_CMOV_LOGICAL, 1, 3, 0 },
    [IL_OP_EQ] = { IL_OP_EQ, 1, 2, 0 },
    [IL_OP_EXP_VEC] = { IL_OP_EXP_VEC, 1, 1, 0 },
    [IL_OP_GE] = { IL_OP_GE, 1, 2, 0 },
    [IL_OP_LOG_VEC] = { IL_OP_LOG_VEC, 1, 1, 0 },
    [IL_OP_LT] = { IL_OP_LT, 1, 2, 0 },
    [IL_OP_NE] = { IL_OP_NE, 1, 2, 0 },
    [IL_OP_ROUND_NEAR] = { IL_OP_ROUND_NEAR, 1, 1, 0 },
    [IL_OP_ROUND_NEG_INF] = { IL_OP_ROUND_NEG_INF, 1, 1, 0 },
    [IL_OP_ROUND_PLUS_INF] = { IL_OP_ROUND_PLUS_INF, 1, 1, 0 },
    [IL_OP_ROUND_ZERO] = { IL_OP_ROUND_ZERO, 1, 1, 0 },
    [IL_OP_RSQ_VEC] = { IL_OP_RSQ_VEC, 1, 1, 0 },
    [IL_OP_SIN_VEC] = { IL_OP_SIN_VEC, 1, 1, 0 },
    [IL_OP_COS_VEC] = { IL_OP_COS_VEC, 1, 1, 0 },
    [IL_OP_SQRT_VEC] = { IL_OP_SQRT_VEC, 1, 1, 0 },
    [IL_OP_DP2] = { IL_OP_DP2, 1, 2, 0 },
    [IL_OP_FETCH4] = { IL_OP_FETCH4, 1, 1, 0 },
    [IL_OP_DCL_NUM_THREAD_PER_GROUP] = { IL_OP_DCL_NUM_THREAD_PER_GROUP, 0, 0, 0 },
    [IL_OP_FENCE] = { IL_OP_FENCE, 0, 0, 0 },
    [IL_OP_LDS_LOAD_VEC] = { IL_OP_LDS_LOAD_VEC, 1, 2, 0 },
    [IL_OP_LDS_STORE_VEC] = { IL_OP_LDS_STORE_VEC, 1, 3, 0 },
    [IL_OP_DCL_UAV] = { IL_OP_DCL_UAV, 0, 0, 0 },
    [IL_OP_DCL_RAW_UAV] = { IL_OP_DCL_RAW_UAV, 0, 0, 0 },
    [IL_OP_UAV_LOAD] = { IL_OP_UAV_LOAD, 1, 1, 0 },
    [IL_OP_UAV_STRUCT_LOAD] = { IL_OP_UAV_STRUCT_LOAD, 1, 1, 0 },
    [IL_OP_UAV_STORE] = { IL_OP_UAV_STORE, 0, 2, 0 },
    [IL_OP_UAV_RAW_STORE] = { IL_OP_UAV_RAW_STORE, 1, 2, 0 },
    [IL_OP_UAV_STRUCT_STORE] = { IL_OP_UAV_STRUCT_STORE, 1, 2, 0 },
    [IL_OP_UAV_ADD] = { IL_OP_UAV_ADD, 0, 2, 0 },
    [IL_OP_UAV_READ_ADD] = { IL_OP_UAV_READ_ADD, 1, 2, 0 },
    [IL_OP_APPEND_BUF_ALLOC] = { IL_OP_APPEND_BUF_ALLOC, 1, 0, 0 },
    [IL_OP_DCL_RAW_SRV] = { IL_OP_DCL_RAW_SRV, 0, 0, 0 },
    [IL_OP_DCL_STRUCT_SRV] = { IL_OP_DCL_STRUCT_SRV, 0, 0, 1 },
    [IL_OP_SRV_STRUCT_LOAD] = { IL_OP_SRV_STRUCT_LOAD, 1, 1, 0 },
    [IL_DCL_LDS] = { IL_DCL_LDS, 0, 0, 1 },
    [IL_DCL_STRUCT_LDS] = { IL_DCL_STRUCT_LDS, 0, 0, 2 },
    [IL_OP_LDS_READ_ADD] = { IL_OP_LDS_READ_ADD, 1, 2, 0 },
    [IL_OP_I_FIRSTBIT] = { IL_OP_I_FIRSTBIT, 1, 1, 0 },
    [IL_OP_I_BIT_EXTRACT] = { IL_OP_I_BIT_EXTRACT, 1, 3, 0 },
    [IL_OP_U_BIT_EXTRACT] = { IL_OP_U_BIT_EXTRACT, 1, 3, 0 },
    [IL_DCL_NUM_ICP] = { IL_DCL_NUM_ICP, 0, 0, 1 },
    [IL_DCL_NUM_OCP] = { IL_DCL_NUM_OCP, 0, 0, 1 },
    [IL_OP_HS_FORK_PHASE] = { IL_OP_HS_FORK_PHASE, 0, 0, 0 },
    [IL_OP_HS_JOIN_PHASE] = { IL_OP_HS_JOIN_PHASE, 0, 0, 0 },
    [IL_OP_ENDPHASE] = { IL_OP_ENDPHASE, 0, 0, 0 },
    [IL_DCL_TS_DOMAIN] = { IL_DCL_TS_DOMAIN, 0, 0, 0 },
    [IL_DCL_TS_PARTITION] = { IL_DCL_TS_PARTITION, 0, 0, 0 },
    [IL_DCL_TS_OUTPUT_PRIMITIVE] = { IL_DCL_TS_OUTPUT_PRIMITIVE, 0, 0, 0 },
    [IL_DCL_MAX_TESSFACTOR] = { IL_DCL_MAX_TESSFACTOR, 0, 0, 1 },
    [IL_OP_U_BIT_INSERT] = { IL_OP_U_BIT_INSERT, 1, 4, 0 },
    [IL_OP_FETCH4_C] = { IL_OP_FETCH4_C, 1, 2, 0 },
    [IL_OP_FETCH4_PO] = { IL_OP_FETCH4_PO, 1, 2, 0 },
    [IL_OP_FETCH4_PO_C] = { IL_OP_FETCH4_PO_C, 1, 3, 0 },
    [IL_OP_F_2_F16] = { IL_OP_F_2_F16, 1, 1, 0 },
    [IL_OP_F16_2_F] = { IL_OP_F16_2_F, 1, 1, 0 },
    [IL_DCL_GLOBAL_FLAGS] = { IL_DCL_GLOBAL_FLAGS, 0, 0, 0 },
    [IL_OP_RCP_VEC] = { IL_OP_RCP_VEC, 1, 1, 0 }, // Undocumented
    [IL_OP_DCL_TYPED_UAV] = { IL_OP_DCL_TYPED_UAV, 0, 0, 1 }, // Undocumented
    [IL_OP_DCL_TYPELESS_UAV] = { IL_OP_DCL_TYPELESS_UAV, 0, 0, 2 }, // Undocumented
    [IL_UNK_660] = { IL_UNK_660, 1, 0, 0 }, // FIXME undocumented
};

static bool hasIndexedResourceSampler(
    const Instruction* instr)
{
    return instr->opcode == IL_OP_LOAD ||
           instr->opcode == IL_OP_SAMPLE ||
           instr->opcode == IL_OP_SAMPLE_B ||
           instr->opcode == IL_OP_SAMPLE_G ||
           instr->opcode == IL_OP_SAMPLE_L ||
           instr->opcode == IL_OP_SAMPLE_C_LZ ||
           instr->opcode == IL_OP_FETCH4 ||
           instr->opcode == IL_OP_FETCH4_C ||
           instr->opcode == IL_OP_FETCH4_PO ||
           instr->opcode == IL_OP_FETCH4_PO_C;
}

static unsigned decodeSource(
    Source* src,
    const Token* token);

static unsigned getSourceCount(
    const Instruction* instr)
{
    const OpcodeInfo* info = &mOpcodeInfos[instr->opcode];
    bool indexedArgs = GET_BIT(instr->control, 12);
    bool priModifierPresent = GET_BIT(instr->control, 15);

    if (hasIndexedResourceSampler(instr) && indexedArgs) {
        // AMDIL spec, section 7.2.3: If the indexed_args bit is set to 1, there are two
        // additional source arguments, corresponding to resource index and sampler index.
        return info->srcCount + 2;
    } else if (instr->opcode == IL_OP_SRV_STRUCT_LOAD && indexedArgs) {
        // Extra indexed input
        return info->srcCount + 1;
    } else if (instr->opcode == IL_DCL_CONST_BUFFER && !priModifierPresent) {
        // Non-immediate constant buffer
        return info->srcCount + 1;
    }

    return info->srcCount;
}

static unsigned getExtraCount(
    const Instruction* instr)
{
    const OpcodeInfo* info = &mOpcodeInfos[instr->opcode];
    bool priModifierPresent = GET_BIT(instr->control, 15);

    if (instr->opcode == IL_DCL_CONST_BUFFER && priModifierPresent) {
        // Immediate constant buffer
        return info->extraCount + instr->primModifier;
    } else if (instr->opcode == IL_OP_DCL_NUM_THREAD_PER_GROUP) {
        // Variable dimensions
        return info->extraCount + GET_BITS(instr->control, 0, 13);
    }

    return info->extraCount;
}

static unsigned decodeIlLang(
    Kernel* kernel,
    const Token* token)
{
    kernel->clientType = GET_BITS(token[0], 0, 7);
    return 1;
}

static unsigned decodeIlVersion(
    Kernel* kernel,
    const Token* token)
{
    kernel->minorVersion = GET_BITS(token[0], 0, 7);
    kernel->majorVersion = GET_BITS(token[0], 8, 15);
    kernel->shaderType = GET_BITS(token[0], 16, 23);
    kernel->multipass = GET_BIT(token[0], 24);
    kernel->realtime = GET_BIT(token[0], 25);
    return 1;
}

static unsigned decodeDestination(
    Destination* dst,
    const Token* token)
{
    unsigned idx = 0;
    bool modifierPresent;
    uint8_t relativeAddress;
    bool dimension;
    bool extended;

    memset(dst, 0, sizeof(*dst));

    dst->registerNum = GET_BITS(token[idx], 0, 15);
    dst->registerType = GET_BITS(token[idx], 16, 21);
    modifierPresent = GET_BIT(token[idx], 22);
    relativeAddress = GET_BITS(token[idx], 23, 24);
    dimension = GET_BIT(token[idx], 25);
    dst->hasImmediate = GET_BIT(token[idx], 26);
    extended = GET_BIT(token[idx], 31);
    idx++;

    if (modifierPresent) {
        dst->component[0] = GET_BITS(token[idx], 0, 1);
        dst->component[1] = GET_BITS(token[idx], 2, 3);
        dst->component[2] = GET_BITS(token[idx], 4, 5);
        dst->component[3] = GET_BITS(token[idx], 6, 7);
        dst->clamp = GET_BIT(token[idx], 8);
        dst->shiftScale = GET_BITS(token[idx], 9, 12);
        idx++;
    } else {
        dst->component[0] = IL_MODCOMP_WRITE;
        dst->component[1] = IL_MODCOMP_WRITE;
        dst->component[2] = IL_MODCOMP_WRITE;
        dst->component[3] = IL_MODCOMP_WRITE;
        dst->clamp = false;
        dst->shiftScale = IL_SHIFT_NONE;
    }

    if (relativeAddress == IL_ADDR_ABSOLUTE) {
        if (dimension) {
            dst->absoluteSrc = malloc(sizeof(Source));
            idx += decodeSource(dst->absoluteSrc, &token[idx]);
        }
    } else if (relativeAddress == IL_ADDR_RELATIVE) {
        // TODO
        LOGW("unhandled relative addressing\n");
        assert(!dimension);
    } else if (relativeAddress == IL_ADDR_REG_RELATIVE) {
        dst->relativeSrcCount = dimension ? 2 : 1;
        dst->relativeSrcs = malloc(dst->relativeSrcCount * sizeof(Source));
        idx += decodeSource(&dst->relativeSrcs[0], &token[idx]);
        // the immediate after the first addr reg
        if (dst->hasImmediate) {
            dst->immediate = token[idx];
            idx++;
        }
        if (dst->relativeSrcCount > 1) {
            idx += decodeSource(&dst->relativeSrcs[1], &token[idx]);
        }
    } else {
        assert(false);
    }

    if (dst->hasImmediate && relativeAddress != IL_ADDR_REG_RELATIVE) {
        dst->immediate = token[idx];
        idx++;
    }

    if (extended) {
        // TODO
        LOGW("unhandled extended register addressing\n");
    }

    return idx;
}

static unsigned decodeSource(
    Source* src,
    const Token* token)
{
    unsigned idx = 0;
    bool modifierPresent;
    uint8_t relativeAddress;
    bool dimension;
    bool extended;

    memset(src, 0, sizeof(*src));

    src->registerNum = GET_BITS(token[idx], 0, 15);
    src->registerType = GET_BITS(token[idx], 16, 21);
    modifierPresent = GET_BIT(token[idx], 22);
    relativeAddress = GET_BITS(token[idx], 23, 24);
    dimension = GET_BIT(token[idx], 25);
    src->hasImmediate = GET_BIT(token[idx], 26);
    extended = GET_BIT(token[idx], 31);
    idx++;

    if (modifierPresent) {
        src->swizzle[0] = GET_BITS(token[idx], 0, 2);
        src->swizzle[1] = GET_BITS(token[idx], 4, 6);
        src->swizzle[2] = GET_BITS(token[idx], 8, 10);
        src->swizzle[3] = GET_BITS(token[idx], 12, 14);
        src->negate[0] = GET_BIT(token[idx], 3);
        src->negate[1] = GET_BIT(token[idx], 7);
        src->negate[2] = GET_BIT(token[idx], 11);
        src->negate[3] = GET_BIT(token[idx], 15);
        src->invert = GET_BIT(token[idx], 16);
        src->bias = GET_BIT(token[idx], 17);
        src->x2 = GET_BIT(token[idx], 18);
        src->sign = GET_BIT(token[idx], 19);
        src->abs = GET_BIT(token[idx], 20);
        src->divComp = GET_BITS(token[idx], 21, 23);
        src->clamp = GET_BIT(token[idx], 24);
        idx++;
    } else {
        src->swizzle[0] = IL_COMPSEL_X_R;
        src->swizzle[1] = IL_COMPSEL_Y_G;
        src->swizzle[2] = IL_COMPSEL_Z_B;
        src->swizzle[3] = IL_COMPSEL_W_A;
    }

    if (relativeAddress == IL_ADDR_ABSOLUTE) {
        if (dimension) {
            src->srcCount = 1;
            src->srcs = malloc(sizeof(Source));
            idx += decodeSource(&src->srcs[0], &token[idx]);
        }
    } else if (relativeAddress == IL_ADDR_RELATIVE) {
        // TODO
        LOGW("unhandled relative addressing\n");
        assert(!dimension);
    } else if (relativeAddress == IL_ADDR_REG_RELATIVE) {
        src->srcCount = dimension ? 2 : 1;
        src->srcs = malloc(src->srcCount * sizeof(Source));
        idx += decodeSource(&src->srcs[0], &token[idx]);
        // the immediate after the first addr reg
        if (src->hasImmediate) {
            src->immediate = token[idx];
            idx++;
        }
        if (src->srcCount > 1) {
            idx += decodeSource(&src->srcs[1], &token[idx]);
        }
    } else {
        assert(false);
    }

    if (src->hasImmediate && relativeAddress != IL_ADDR_REG_RELATIVE) {
        src->immediate = token[idx];
        idx++;
    }

    if (extended) {
        // TODO
        LOGW("unhandled extended register addressing\n");
    }

    return idx;
}

static unsigned decodeInstruction(
    Instruction* instr,
    const Token* token,
    uint16_t prefixControl)
{
    unsigned idx = 0;

    memset(instr, 0, sizeof(*instr));

    instr->opcode = GET_BITS(token[idx], 0, 15);
    instr->control = GET_BITS(token[idx], 16, 31);
    idx++;

    if (instr->opcode == IL_OP_PREFIX) {
        // Pass prefix info to the next instruction
        return idx + decodeInstruction(instr, &token[idx], instr->control);
    }

    if (instr->opcode >= IL_OP_LAST) {
        LOGE("invalid opcode %d\n", instr->opcode);
        return idx;
    }

    const OpcodeInfo* info = &mOpcodeInfos[instr->opcode];

    if (info->opcode != instr->opcode) {
        LOGW("unhandled opcode %d\n", instr->opcode);
        return idx;
    }

    if (instr->opcode != IL_DCL_RESOURCE) {
        if (GET_BIT(instr->control, 15)) {
            instr->primModifier = token[idx];
            idx++;
        }
    }

    if (GET_BIT(instr->control, 14)) {
        instr->secModifier = token[idx];
        idx++;
    }

    if (hasIndexedResourceSampler(instr)) {
        if (GET_BIT(instr->control, 12)) {
            instr->resourceFormat = token[idx];
            idx++;
        }

        if (GET_BIT(instr->control, 13)) {
            instr->addressOffset = token[idx];
            idx++;
        }
    }

    instr->dstCount = info->dstCount;
    instr->dsts = malloc(sizeof(Destination) * instr->dstCount);
    for (int i = 0; i < instr->dstCount; i++) {
        idx += decodeDestination(&instr->dsts[i], &token[idx]);
    }

    instr->srcCount = getSourceCount(instr);
    instr->srcs = malloc(sizeof(Source) * instr->srcCount);
    for (int i = 0; i < instr->srcCount; i++) {
        idx += decodeSource(&instr->srcs[i], &token[idx]);
    }

    instr->extraCount = getExtraCount(instr);
    instr->extras = malloc(sizeof(Token) * instr->extraCount);
    memcpy(instr->extras, &token[idx], sizeof(Token) * instr->extraCount);
    idx += instr->extraCount;

    instr->preciseMask = GET_BITS(prefixControl, 0, 3);

    return idx;
}

Kernel* ilcDecodeStream(
    const Token* tokens,
    unsigned count)
{
    Kernel* kernel = malloc(sizeof(Kernel));
    unsigned idx = 0;

    idx += decodeIlLang(kernel, &tokens[idx]);
    idx += decodeIlVersion(kernel, &tokens[idx]);

    kernel->instrCount = 0;
    kernel->instrs = NULL;
    while (idx < count) {
        kernel->instrCount++;
        kernel->instrs = realloc(kernel->instrs, sizeof(Instruction) * kernel->instrCount);
        idx += decodeInstruction(&kernel->instrs[kernel->instrCount - 1], &tokens[idx], 0);
    }

    return kernel;
}
