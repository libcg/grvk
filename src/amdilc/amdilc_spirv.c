#include "amdilc_internal.h"
#include "amdilc_spirv.h"

#define BUFFER_ALLOC_THRESHOLD 64

static unsigned strlenw(
    const char* str)
{
    // String length in words, including \0
    return (strlen(str) + 4) / sizeof(IlcSpvWord);
}

static void putWord(
    IlcSpvBuffer* buffer,
    IlcSpvWord word)
{
    // Check if we need to resize the buffer
    if (buffer->wordCount % BUFFER_ALLOC_THRESHOLD == 0) {
        size_t size = sizeof(IlcSpvWord) * (buffer->wordCount + BUFFER_ALLOC_THRESHOLD);
        buffer->words = realloc(buffer->words, size);
    }

    buffer->words[buffer->wordCount++] = word;
}

static void putInstr(
    IlcSpvBuffer* buffer,
    uint16_t op,
    uint16_t wordCount)
{
    putWord(buffer, op | (wordCount << SpvWordCountShift));
}

static void putString(
    IlcSpvBuffer* buffer,
    const char* str)
{
    size_t len = strlen(str);
    IlcSpvWord word = 0;

    for (unsigned i = 0; i < len; i++) {
        word |= str[i] << (8 * (i % sizeof(word)));

        if (i % sizeof(word) == 3) {
            putWord(buffer, word);
            word = 0;
        }
    }

    putWord(buffer, word);
}

static void putBuffer(
    IlcSpvBuffer* buffer,
    IlcSpvBuffer* otherBuffer)
{
    for (unsigned i = 0; i < otherBuffer->wordCount; i++) {
        putWord(buffer, otherBuffer->words[i]);
    }
}

static void putHeader(
    IlcSpvModule* module)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_MAIN];

    putWord(buffer, SpvMagicNumber);
    putWord(buffer, SpvVersion);
    putWord(buffer, 0);
    putWord(buffer, module->currentId);
    putWord(buffer, 0);
}

static IlcSpvId putType(
    IlcSpvModule* module,
    SpvOp op,
    unsigned argCount,
    const IlcSpvWord* args,
    bool hasConstants)
{
    IlcSpvBuffer* buffer = &module->buffer[hasConstants ? ID_TYPES_WITH_CONSTANTS : ID_TYPES];

    // Check if the type is already present
    for (int i = 0; i < buffer->wordCount;) {
        SpvOp typeOp = buffer->words[i] & SpvOpCodeMask;
        unsigned typeWordCount = buffer->words[i] >> SpvWordCountShift;
        unsigned typeArgCount = typeWordCount - 2;

        // Got a potential match
        if (op == typeOp && argCount == typeArgCount) {
            bool match = true;
            IlcSpvId typeId = buffer->words[i + 1];

            // Check that the args also match
            for (unsigned j = 0; j < typeArgCount; j++) {
                if (args[j] != buffer->words[i + 2 + j]) {
                    match = false;
                    break;
                }
            }

            if (match) {
                return typeId;
            }
        }

        i += typeWordCount;
    }

    IlcSpvId id = ilcSpvAllocId(module);
    putInstr(buffer, op, 2 + argCount);
    putWord(buffer, id);
    for (int i = 0; i < argCount; i++) {
        putWord(buffer, args[i]);
    }

    return id;
}

static IlcSpvId putConstant(
    IlcSpvModule* module,
    SpvOp op,
    IlcSpvId resultTypeId,
    unsigned argCount,
    const IlcSpvWord* args)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CONSTANTS];

    // Check if the constant is already present
    for (int i = 0; i < buffer->wordCount;) {
        SpvOp constantOp = buffer->words[i] & SpvOpCodeMask;
        unsigned constantWordCount = buffer->words[i] >> SpvWordCountShift;
        unsigned constantArgCount = constantWordCount - 3;
        IlcSpvId constantResultTypeId = buffer->words[i + 1];

        // Got a potential match
        if (op == constantOp &&
            resultTypeId == constantResultTypeId &&
            argCount == constantArgCount) {
            bool match = true;
            IlcSpvId constantId = buffer->words[i + 2];

            // Check that the args also match
            for (unsigned j = 0; j < constantArgCount; j++) {
                if (args[j] != buffer->words[i + 3 + j]) {
                    match = false;
                    break;
                }
            }

            if (match) {
                return constantId;
            }
        }

        i += constantWordCount;
    }

    IlcSpvId id = ilcSpvAllocId(module);
    putInstr(buffer, op, 3 + argCount);
    putWord(buffer, resultTypeId);
    putWord(buffer, id);
    for (int i = 0; i < argCount; i++) {
        putWord(buffer, args[i]);
    }

    return id;
}

static void putExtInstImport(
    IlcSpvModule* module,
    IlcSpvId id,
    const char* name)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_EXT_INST_IMPORTS];

    putInstr(buffer, SpvOpExtInstImport, 2 + strlenw(name));
    putWord(buffer, module->glsl450ImportId);
    putString(buffer, name);
}

static void putMemoryModel(
    IlcSpvModule* module,
    IlcSpvWord addressing,
    IlcSpvWord memory)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_MEMORY_MODEL];

    putInstr(buffer, SpvOpMemoryModel, 3);
    putWord(buffer, addressing);
    putWord(buffer, memory);
}

void ilcSpvInit(
    IlcSpvModule* module)
{
    module->currentId = 1;
    module->glsl450ImportId = ilcSpvAllocId(module);
    for (int i = 0; i < ID_MAX; i++) {
        module->buffer[i] = (IlcSpvBuffer) { 0, NULL };
    }

    ilcSpvPutCapability(module, SpvCapabilityShader);
    putExtInstImport(module, module->glsl450ImportId, "GLSL.std.450");
    putMemoryModel(module, SpvAddressingModelLogical, SpvMemoryModelGLSL450);
}

void ilcSpvFinish(
    IlcSpvModule* module)
{
    putHeader(module);

    // Merge buffers into one
    for (int i = ID_MAIN + 1; i < ID_MAX; i++) {
        putBuffer(&module->buffer[ID_MAIN], &module->buffer[i]);
        free(module->buffer[i].words);
    }
}

uint32_t ilcSpvAllocId(
    IlcSpvModule* module)
{
    return module->currentId++;
}

void ilcSpvPutExtension(
    IlcSpvModule* module,
    const char* name)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_EXTENSIONS];

    putInstr(buffer, SpvOpExtension, 1 + strlenw(name));
    putString(buffer, name);
}

void ilcSpvPutName(
    IlcSpvModule* module,
    IlcSpvId target,
    const char* name)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_NAMES];

    putInstr(buffer, SpvOpName, 2 + strlenw(name));
    putWord(buffer, target);
    putString(buffer, name);
}

void ilcSpvPutEntryPoint(
    IlcSpvModule* module,
    SpvExecutionModel execModel,
    IlcSpvId id,
    const char* name,
    unsigned interfaceCount,
    const IlcSpvWord* interfaces)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_ENTRY_POINTS];

    putInstr(buffer, SpvOpEntryPoint, 3 + strlenw(name) + interfaceCount);
    putWord(buffer, id);
    putWord(buffer, execModel);
    putString(buffer, name);
    for (unsigned i = 0; i < interfaceCount; i++) {
        putWord(buffer, interfaces[i]);
    }
}

void ilcSpvPutExecMode(
    IlcSpvModule* module,
    IlcSpvId id,
    SpvExecutionMode execMode,
    unsigned operandCount,
    const IlcSpvWord* operands)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_EXEC_MODES];

    putInstr(buffer, SpvOpExecutionMode, 3 + operandCount);
    putWord(buffer, id);
    putWord(buffer, execMode);
    for (unsigned i = 0; i < operandCount; i++) {
        putWord(buffer, operands[i]);
    }
}

void ilcSpvPutCapability(
    IlcSpvModule* module,
    IlcSpvWord capability)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CAPABILITIES];

    // Check if the capability is already present
    for (unsigned i = 0; i < buffer->wordCount; i += 2) {
        if (buffer->words[i + 1] == capability) {
            return;
        }
    }

    putInstr(buffer, SpvOpCapability, 2);
    putWord(buffer, capability);
}

IlcSpvId ilcSpvPutVoidType(
    IlcSpvModule* module)
{
    return putType(module, SpvOpTypeVoid, 0, NULL, false);
}

IlcSpvId ilcSpvPutBoolType(
    IlcSpvModule* module)
{
    return putType(module, SpvOpTypeBool, 0, NULL, false);
}

IlcSpvId ilcSpvPutIntType(
    IlcSpvModule* module,
    bool isSigned)
{
    const IlcSpvWord args[2] = { 32, isSigned };

    return putType(module, SpvOpTypeInt, 2, args, false);
}

IlcSpvId ilcSpvPutFloatType(
    IlcSpvModule* module)
{
    IlcSpvWord width = 32;

    return putType(module, SpvOpTypeFloat, 1, &width, false);
}

IlcSpvId ilcSpvPutVectorType(
    IlcSpvModule* module,
    IlcSpvId typeId,
    unsigned count)
{
    const IlcSpvWord args[] = { typeId, count };

    return putType(module, SpvOpTypeVector, 2, args, false);
}

IlcSpvId ilcSpvPutImageType(
    IlcSpvModule* module,
    IlcSpvId sampledTypeId,
    IlcSpvWord dim,
    IlcSpvWord depth,
    IlcSpvWord arrayed,
    IlcSpvWord ms,
    IlcSpvWord sampled,
    IlcSpvWord format)
{
    const IlcSpvWord args[] = {
        sampledTypeId,
        dim,
        depth,
        arrayed,
        ms,
        sampled,
        format,
    };

    return putType(module, SpvOpTypeImage, 7, args, false);
}

IlcSpvId ilcSpvPutSamplerType(
    IlcSpvModule* module)
{
    return putType(module, SpvOpTypeSampler, 0, NULL, false);
}

IlcSpvId ilcSpvPutSampledImageType(
    IlcSpvModule* module,
    IlcSpvId imageTypeId)
{
    return putType(module, SpvOpTypeSampledImage, 1, &imageTypeId, false);
}

IlcSpvId ilcSpvPutArrayType(
    IlcSpvModule* module,
    IlcSpvId typeId,
    IlcSpvId lengthId)
{
    const IlcSpvWord args[] = { typeId, lengthId };

    return putType(module, SpvOpTypeArray, 2, args, true);
}

IlcSpvId ilcSpvPutPointerType(
    IlcSpvModule* module,
    IlcSpvWord storageClass,
    IlcSpvId typeId)
{
    const IlcSpvWord args[] = { storageClass, typeId };

    return putType(module, SpvOpTypePointer, 2, args, true);
}

IlcSpvId ilcSpvPutFunctionType(
    IlcSpvModule* module,
    IlcSpvId returnTypeId,
    unsigned argTypeIdCount,
    const IlcSpvId* argTypeIds)
{
    unsigned argCount = 1 + argTypeIdCount;
    IlcSpvWord* args = malloc(sizeof(args[0]) * argCount);

    args[0] = returnTypeId;
    memcpy(&args[1], argTypeIds, sizeof(args[0]) * argTypeIdCount);

    IlcSpvId id = putType(module, SpvOpTypeFunction, argCount, args, false);
    free(args);

    return id;
}

IlcSpvId ilcSpvPutConstant(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvWord literal)
{
    return putConstant(module, SpvOpConstant, resultTypeId, 1, &literal);
}

IlcSpvId ilcSpvPutConstantComposite(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    unsigned consistuentCount,
    const IlcSpvId* consistuents)
{
    return putConstant(module, SpvOpConstantComposite, resultTypeId,
                       consistuentCount, consistuents);
}

void ilcSpvPutFunction(
    IlcSpvModule* module,
    IlcSpvId resultType,
    IlcSpvId id,
    SpvFunctionControlMask control,
    IlcSpvId type)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    putInstr(buffer, SpvOpFunction, 5);
    putWord(buffer, resultType);
    putWord(buffer, id);
    putWord(buffer, control);
    putWord(buffer, type);
}

void ilcSpvPutFunctionEnd(
    IlcSpvModule* module)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    putInstr(buffer, SpvOpFunctionEnd, 1);
}

IlcSpvId ilcSpvPutVariable(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvWord storageClass)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_VARIABLES];

    IlcSpvId id = ilcSpvAllocId(module);
    putInstr(buffer, SpvOpVariable, 4);
    putWord(buffer, resultTypeId);
    putWord(buffer, id);
    putWord(buffer, storageClass);
    return id;
}

IlcSpvId ilcSpvPutImageTexelPointer(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId imageId,
    IlcSpvId coordinateId,
    IlcSpvId sampleId)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    IlcSpvId id = ilcSpvAllocId(module);
    putInstr(buffer, SpvOpImageTexelPointer, 6);
    putWord(buffer, resultTypeId);
    putWord(buffer, id);
    putWord(buffer, imageId);
    putWord(buffer, coordinateId);
    putWord(buffer, sampleId);
    return id;
}

IlcSpvId ilcSpvPutLoad(
    IlcSpvModule* module,
    IlcSpvId typeId,
    IlcSpvId pointerId)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    IlcSpvId id = ilcSpvAllocId(module);
    putInstr(buffer, SpvOpLoad, 4);
    putWord(buffer, typeId);
    putWord(buffer, id);
    putWord(buffer, pointerId);
    return id;
}

void ilcSpvPutStore(
    IlcSpvModule* module,
    IlcSpvId pointerId,
    IlcSpvId objectId)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    putInstr(buffer, SpvOpStore, 3);
    putWord(buffer, pointerId);
    putWord(buffer, objectId);
}

IlcSpvId ilcSpvPutAccessChain(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId baseId,
    IlcSpvId indexId)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    IlcSpvId id = ilcSpvAllocId(module);
    putInstr(buffer, SpvOpAccessChain, 5);
    putWord(buffer, resultTypeId);
    putWord(buffer, id);
    putWord(buffer, baseId);
    putWord(buffer, indexId);
    return id;
}

void ilcSpvPutDecoration(
    IlcSpvModule* module,
    IlcSpvId target,
    IlcSpvWord decoration,
    unsigned argCount,
    const IlcSpvWord* args)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_DECORATIONS];

    putInstr(buffer, SpvOpDecorate, 3 + argCount);
    putWord(buffer, target);
    putWord(buffer, decoration);
    for (int i = 0; i < argCount; i++) {
        putWord(buffer, args[i]);
    }
}

IlcSpvId ilcSpvPutVectorShuffle(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId vec1Id,
    IlcSpvId vec2Id,
    unsigned componentCount,
    const IlcSpvWord* components)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    IlcSpvId id = ilcSpvAllocId(module);
    putInstr(buffer, SpvOpVectorShuffle, 5 + componentCount);
    putWord(buffer, resultTypeId);
    putWord(buffer, id);
    putWord(buffer, vec1Id);
    putWord(buffer, vec2Id);
    for (int i = 0; i < componentCount; i++) {
        putWord(buffer, components[i]);
    }
    return id;
}

IlcSpvId ilcSpvPutCompositeConstruct(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    unsigned consistuentCount,
    const IlcSpvId* consistuents)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    IlcSpvId id = ilcSpvAllocId(module);
    putInstr(buffer, SpvOpCompositeConstruct, 3 + consistuentCount);
    putWord(buffer, resultTypeId);
    putWord(buffer, id);
    for (int i = 0; i < consistuentCount; i++) {
        putWord(buffer, consistuents[i]);
    }
    return id;
}

IlcSpvId ilcSpvPutCompositeExtract(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId compositeId,
    unsigned indexCount,
    const IlcSpvId* indexes)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    IlcSpvId id = ilcSpvAllocId(module);
    putInstr(buffer, SpvOpCompositeExtract, 4 + indexCount);
    putWord(buffer, resultTypeId);
    putWord(buffer, id);
    putWord(buffer, compositeId);
    for (int i = 0; i < indexCount; i++) {
        putWord(buffer, indexes[i]);
    }
    return id;
}

IlcSpvId ilcSpvPutSampledImage(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId imageId,
    IlcSpvId samplerId)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    IlcSpvId id = ilcSpvAllocId(module);
    putInstr(buffer, SpvOpSampledImage, 5);
    putWord(buffer, resultTypeId);
    putWord(buffer, id);
    putWord(buffer, imageId);
    putWord(buffer, samplerId);
    return id;
}

IlcSpvId ilcSpvPutImageSample(
    IlcSpvModule* module,
    IlcSpvWord sampleOp,
    IlcSpvId resultTypeId,
    IlcSpvId sampledImageId,
    IlcSpvId coordinateId,
    SpvImageOperandsMask mask,
    unsigned operandIdCount,
    const IlcSpvId* operandIds)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];
    unsigned length = 5 + (mask != 0) + operandIdCount;

    IlcSpvId id = ilcSpvAllocId(module);
    putInstr(buffer, sampleOp, length);
    putWord(buffer, resultTypeId);
    putWord(buffer, id);
    putWord(buffer, sampledImageId);
    putWord(buffer, coordinateId);
    if (mask != 0) {
        putWord(buffer, mask);
    }
    for (unsigned i = 0; i < operandIdCount; i++) {
        putWord(buffer, operandIds[i]);
    }
    return id;
}

IlcSpvId ilcSpvPutImageFetch(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId imageId,
    IlcSpvId coordinateId)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    IlcSpvId id = ilcSpvAllocId(module);
    putInstr(buffer, SpvOpImageFetch, 5);
    putWord(buffer, resultTypeId);
    putWord(buffer, id);
    putWord(buffer, imageId);
    putWord(buffer, coordinateId);
    return id;
}

void ilcSpvPutImageWrite(
    IlcSpvModule* module,
    IlcSpvId imageId,
    IlcSpvId coordinateId,
    IlcSpvId texelId)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    putInstr(buffer, SpvOpImageWrite, 4);
    putWord(buffer, imageId);
    putWord(buffer, coordinateId);
    putWord(buffer, texelId);
}

IlcSpvId ilcSpvPutImageQuerySizeLod(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId imageId,
    IlcSpvId lodId)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    IlcSpvId id = ilcSpvAllocId(module);
    putInstr(buffer, SpvOpImageQuerySizeLod, 5);
    putWord(buffer, resultTypeId);
    putWord(buffer, id);
    putWord(buffer, imageId);
    putWord(buffer, lodId);
    return id;
}

IlcSpvId ilcSpvPutImageQueryLevels(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId imageId)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    IlcSpvId id = ilcSpvAllocId(module);
    putInstr(buffer, SpvOpImageQueryLevels, 4);
    putWord(buffer, resultTypeId);
    putWord(buffer, id);
    putWord(buffer, imageId);
    return id;
}

IlcSpvId ilcSpvPutAlu(
    IlcSpvModule* module,
    SpvOp op,
    IlcSpvId resultTypeId,
    unsigned idCount,
    const IlcSpvId* ids)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    IlcSpvId id = ilcSpvAllocId(module);
    putInstr(buffer, op, 3 + idCount);
    putWord(buffer, resultTypeId);
    putWord(buffer, id);
    for (int i = 0; i < idCount; i++) {
        putWord(buffer, ids[i]);
    }
    return id;
}

IlcSpvId ilcSpvPutAtomicOp(
    IlcSpvModule* module,
    SpvOp op,
    IlcSpvId resultTypeId,
    IlcSpvId pointerId,
    IlcSpvWord memoryId,
    IlcSpvWord semanticsId,
    IlcSpvId valueId)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    IlcSpvId id = ilcSpvAllocId(module);
    putInstr(buffer, op, 7);
    putWord(buffer, resultTypeId);
    putWord(buffer, id);
    putWord(buffer, pointerId);
    putWord(buffer, memoryId);
    putWord(buffer, semanticsId);
    putWord(buffer, valueId);
    return id;
}

IlcSpvId ilcSpvPutBitcast(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId operandId)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    IlcSpvId id = ilcSpvAllocId(module);
    putInstr(buffer, SpvOpBitcast, 4);
    putWord(buffer, resultTypeId);
    putWord(buffer, id);
    putWord(buffer, operandId);
    return id;
}

IlcSpvId ilcSpvPutSelect(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId conditionId,
    IlcSpvId obj1Id,
    IlcSpvId obj2Id)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    IlcSpvId id = ilcSpvAllocId(module);
    putInstr(buffer, SpvOpSelect, 6);
    putWord(buffer, resultTypeId);
    putWord(buffer, id);
    putWord(buffer, conditionId);
    putWord(buffer, obj1Id);
    putWord(buffer, obj2Id);
    return id;
}

void ilcSpvPutLoopMerge(
    IlcSpvModule* module,
    IlcSpvId mergeBlockId,
    IlcSpvId continueTargetId)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    putInstr(buffer, SpvOpLoopMerge, 4);
    putWord(buffer, mergeBlockId);
    putWord(buffer, continueTargetId);
    putWord(buffer, SpvLoopControlMaskNone);
}

void ilcSpvPutSelectionMerge(
    IlcSpvModule* module,
    IlcSpvId mergeBlockId)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    putInstr(buffer, SpvOpSelectionMerge, 3);
    putWord(buffer, mergeBlockId);
    putWord(buffer, SpvSelectionControlMaskNone);
}

IlcSpvId ilcSpvPutLabel(
    IlcSpvModule* module,
    IlcSpvId labelId)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    IlcSpvId id = labelId != 0 ? labelId : ilcSpvAllocId(module);
    putInstr(buffer, SpvOpLabel, 2);
    putWord(buffer, id);
    return id;
}

void ilcSpvPutBranch(
    IlcSpvModule* module,
    IlcSpvId labelId)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    putInstr(buffer, SpvOpBranch, 2);
    putWord(buffer, labelId);
}

void ilcSpvPutBranchConditional(
    IlcSpvModule* module,
    IlcSpvId conditionId,
    IlcSpvId trueLabelId,
    IlcSpvId falseLabelId)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    putInstr(buffer, SpvOpBranchConditional, 4);
    putWord(buffer, conditionId);
    putWord(buffer, trueLabelId);
    putWord(buffer, falseLabelId);
}

void ilcSpvPutReturn(
    IlcSpvModule* module)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    putInstr(buffer, SpvOpReturn, 1);
}

IlcSpvId ilcSpvPutGLSLOp(
    IlcSpvModule* module,
    enum GLSLstd450 glslOp,
    IlcSpvId resultTypeId,
    unsigned idCount,
    const IlcSpvId* ids)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    IlcSpvId id = ilcSpvAllocId(module);
    putInstr(buffer, SpvOpExtInst, 5 + idCount);
    putWord(buffer, resultTypeId);
    putWord(buffer, id);
    putWord(buffer, module->glsl450ImportId);
    putWord(buffer, glslOp);
    for (int i = 0; i < idCount; i++) {
        putWord(buffer, ids[i]);
    }
    return id;
}

void ilcSpvPutDemoteToHelperInvocation(
    IlcSpvModule* module)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    putInstr(buffer, SpvOpDemoteToHelperInvocationEXT, 1);
}
