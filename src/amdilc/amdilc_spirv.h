#ifndef AMDILC_SPIRV_H_
#define AMDILC_SPIRV_H_

#include <stdint.h>

typedef enum {
    ID_MAIN,
    ID_CAPABILITIES,
    ID_EXT_INST_IMPORTS,
    ID_MEMORY_MODEL,
    ID_ENTRY_POINTS,
    ID_EXEC_MODES,
    ID_NAMES,
    ID_TYPES,
    ID_CODE,
    ID_MAX,
} IlcSpvBufferId;

typedef uint32_t IlcSpvWord;
typedef IlcSpvWord IlcSpvId;

typedef struct {
    unsigned wordCount;
    IlcSpvWord* words;
} IlcSpvBuffer;

typedef struct {
    IlcSpvId currentId;
    IlcSpvId glsl450ImportId;
    IlcSpvBuffer buffer[ID_MAX];
} IlcSpvModule;

void ilcSpvInit(
    IlcSpvModule* module);

void ilcSpvFinish(
    IlcSpvModule* module);

uint32_t ilcSpvAllocId(
    IlcSpvModule* module);

void ilcSpvPutName(
    IlcSpvModule* module,
    IlcSpvId target,
    const char* name);

void ilcSpvPutEntryPoint(
    IlcSpvModule* module,
    SpvExecutionModel execModel,
    IlcSpvId id,
    const char* name,
    unsigned interfaceCount,
    IlcSpvWord* interfaces);

void ilcSpvPutExecMode(
    IlcSpvModule* module,
    IlcSpvId id,
    SpvExecutionMode execMode);

IlcSpvId ilcSpvPutVoidType(
    IlcSpvModule* module);

IlcSpvId ilcSpvPutFunctionType(
    IlcSpvModule* module,
    IlcSpvId returnTypeId,
    unsigned argTypeIdCount,
    IlcSpvId* argTypeIds);

void ilcSpvPutFunction(
    IlcSpvModule* module,
    IlcSpvId resultType,
    IlcSpvId result,
    SpvFunctionControlMask control,
    IlcSpvId type);

void ilcSpvPutFunctionEnd(
    IlcSpvModule* module);

IlcSpvId ilcSpvPutLabel(
    IlcSpvModule* module);

void ilcSpvPutReturn(
    IlcSpvModule* module);

#endif // AMDILC_SPIRV_H_
