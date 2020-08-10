#include "spirv/spirv.h"
#include "amdilc_internal.h"
#include "amdilc_spirv.h"

#define BUFFER_ALLOC_THRESHOLD 64

static uint32_t strlenw(
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
    IlcSpvWord* args)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_TYPES];

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
            for (int j = 0; j < typeArgCount; j++) {
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
    IlcSpvWord* interfaces)
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
    SpvExecutionMode execMode)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_EXEC_MODES];

    putInstr(buffer, SpvOpExecutionMode, 3);
    putWord(buffer, id);
    putWord(buffer, execMode);
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
    return putType(module, SpvOpTypeVoid, 0, NULL);
}

IlcSpvId ilcSpvPutFloatType(
    IlcSpvModule* module)
{
    IlcSpvWord width = 32;

    return putType(module, SpvOpTypeFloat, 1, &width);
}

IlcSpvId ilcSpvPutVectorType(
    IlcSpvModule* module,
    IlcSpvId typeId,
    unsigned count)
{
    IlcSpvWord args[] = { typeId, count };

    return putType(module, SpvOpTypeVector, 2, args);
}

IlcSpvId ilcSpvPutPointerType(
    IlcSpvModule* module,
    IlcSpvWord storageClass,
    IlcSpvId typeId)
{
    IlcSpvWord args[] = { storageClass, typeId };

    return putType(module, SpvOpTypePointer, 2, args);
}

IlcSpvId ilcSpvPutFunctionType(
    IlcSpvModule* module,
    IlcSpvId returnTypeId,
    unsigned argTypeIdCount,
    IlcSpvId* argTypeIds)
{
    unsigned argCount = 1 + argTypeIdCount;
    IlcSpvWord* args = malloc(sizeof(args[0]) * argCount);

    args[0] = returnTypeId;
    memcpy(&args[1], argTypeIds, sizeof(args[0]) * argTypeIdCount);

    IlcSpvId id = putType(module, SpvOpTypeFunction, argCount, args);
    free(args);

    return id;
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

void ilcSpvPutDecoration(
    IlcSpvModule* module,
    IlcSpvId target,
    IlcSpvWord decoration,
    unsigned argCount,
    IlcSpvWord* args)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_DECORATIONS];

    putInstr(buffer, SpvOpDecorate, 3 + argCount);
    putWord(buffer, target);
    putWord(buffer, decoration);
    for (int i = 0; i < argCount; i++) {
        putWord(buffer, args[i]);
    }
}

IlcSpvId ilcSpvPutLabel(
    IlcSpvModule* module)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    IlcSpvId id = ilcSpvAllocId(module);
    putInstr(buffer, SpvOpLabel, 2);
    putWord(buffer, id);
    return id;
}

void ilcSpvPutReturn(
    IlcSpvModule* module)
{
    IlcSpvBuffer* buffer = &module->buffer[ID_CODE];

    putInstr(buffer, SpvOpReturn, 1);
}
