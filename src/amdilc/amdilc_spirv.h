#ifndef AMDILC_SPIRV_H_
#define AMDILC_SPIRV_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ID_MAIN,
    ID_CAPABILITIES,
    ID_EXT_INST_IMPORTS,
    ID_MEMORY_MODEL,
    ID_ENTRY_POINTS,
    ID_EXEC_MODES,
    ID_NAMES,
    ID_DECORATIONS,
    ID_TYPES,
    ID_CONSTANTS,
    ID_VARIABLES,
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

void ilcSpvPutCapability(
    IlcSpvModule* module,
    IlcSpvWord capability);

IlcSpvId ilcSpvPutVoidType(
    IlcSpvModule* module);

IlcSpvId ilcSpvPutIntType(
    IlcSpvModule* module,
    bool isSigned);

IlcSpvId ilcSpvPutFloatType(
    IlcSpvModule* module);

IlcSpvId ilcSpvPutVectorType(
    IlcSpvModule* module,
    IlcSpvId typeId,
    unsigned count);

IlcSpvId ilcSpvPutImageType(
    IlcSpvModule* module,
    IlcSpvId sampledTypeId,
    IlcSpvWord dim,
    IlcSpvWord depth,
    IlcSpvWord arrayed,
    IlcSpvWord ms,
    IlcSpvWord sampled,
    IlcSpvWord format);

IlcSpvId ilcSpvPutPointerType(
    IlcSpvModule* module,
    IlcSpvWord storageClass,
    IlcSpvId typeId);

IlcSpvId ilcSpvPutFunctionType(
    IlcSpvModule* module,
    IlcSpvId returnTypeId,
    unsigned argTypeIdCount,
    IlcSpvId* argTypeIds);

IlcSpvId ilcSpvPutConstant(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvWord literal);

IlcSpvId ilcSpvPutConstantComposite(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    unsigned consistuentCount,
    const IlcSpvId* consistuents);

void ilcSpvPutFunction(
    IlcSpvModule* module,
    IlcSpvId resultType,
    IlcSpvId result,
    SpvFunctionControlMask control,
    IlcSpvId type);

void ilcSpvPutFunctionEnd(
    IlcSpvModule* module);

IlcSpvId ilcSpvPutVariable(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvWord storageClass);

IlcSpvId ilcSpvPutLoad(
    IlcSpvModule* module,
    IlcSpvId typeId,
    IlcSpvId pointerId);

void ilcSpvPutStore(
    IlcSpvModule* module,
    IlcSpvId pointerId,
    IlcSpvId objectId);

void ilcSpvPutDecoration(
    IlcSpvModule* module,
    IlcSpvId target,
    IlcSpvWord decoration,
    unsigned argCount,
    IlcSpvWord* args);

IlcSpvId ilcSpvPutImageFetch(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId imageId,
    IlcSpvId coordinateId);

IlcSpvId ilcSpvPutLabel(
    IlcSpvModule* module);

void ilcSpvPutReturn(
    IlcSpvModule* module);

#endif // AMDILC_SPIRV_H_
