#ifndef AMDILC_SPIRV_H_
#define AMDILC_SPIRV_H_

#include <stdbool.h>
#include <stdint.h>
#include "spirv/GLSL.std.450.h"
#include "spirv/spirv.h"

typedef enum {
    ID_MAIN,
    ID_CAPABILITIES,
    ID_EXTENSIONS,
    ID_EXT_INST_IMPORTS,
    ID_MEMORY_MODEL,
    ID_ENTRY_POINTS,
    ID_EXEC_MODES,
    ID_DEBUG,
    ID_DECORATIONS,
    ID_TYPES,
    ID_CONSTANTS,
    ID_TYPES_WITH_CONSTANTS,
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

void ilcSpvPutExtension(
    IlcSpvModule* module,
    const char* name);

void ilcSpvPutSource(
    IlcSpvModule* module,
    IlcSpvId nameId);

void ilcSpvPutName(
    IlcSpvModule* module,
    IlcSpvId target,
    const char* name);

IlcSpvId ilcSpvPutString(
    IlcSpvModule* module,
    const char* string);

void ilcSpvPutEntryPoint(
    IlcSpvModule* module,
    SpvExecutionModel execModel,
    IlcSpvId id,
    const char* name,
    unsigned interfaceCount,
    const IlcSpvWord* interfaces);

void ilcSpvPutExecMode(
    IlcSpvModule* module,
    IlcSpvId id,
    SpvExecutionMode execMode,
    unsigned operandCount,
    const IlcSpvWord* operands);

void ilcSpvPutCapability(
    IlcSpvModule* module,
    IlcSpvWord capability);

IlcSpvId ilcSpvPutVoidType(
    IlcSpvModule* module);

IlcSpvId ilcSpvPutBoolType(
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

IlcSpvId ilcSpvPutSamplerType(
    IlcSpvModule* module);

IlcSpvId ilcSpvPutSampledImageType(
    IlcSpvModule* module,
    IlcSpvId imageTypeId);

IlcSpvId ilcSpvPutArrayType(
    IlcSpvModule* module,
    IlcSpvId typeId,
    IlcSpvId lengthId);

IlcSpvId ilcSpvPutRuntimeArrayType(
    IlcSpvModule* module,
    IlcSpvId typeId,
    bool unique);

IlcSpvId ilcSpvPutStructType(
    IlcSpvModule* module,
    unsigned memberTypeIdCount,
    const IlcSpvId* memberTypeId);

IlcSpvId ilcSpvPutPointerType(
    IlcSpvModule* module,
    IlcSpvWord storageClass,
    IlcSpvId typeId);

IlcSpvId ilcSpvPutFunctionType(
    IlcSpvModule* module,
    IlcSpvId returnTypeId,
    unsigned argTypeIdCount,
    const IlcSpvId* argTypeIds);

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

IlcSpvId ilcSpvPutImageTexelPointer(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId imageId,
    IlcSpvId coordinateId,
    IlcSpvId sampleId);

IlcSpvId ilcSpvPutLoad(
    IlcSpvModule* module,
    IlcSpvId typeId,
    IlcSpvId pointerId);

void ilcSpvPutStore(
    IlcSpvModule* module,
    IlcSpvId pointerId,
    IlcSpvId objectId);

IlcSpvId ilcSpvPutAccessChain(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId baseId,
    unsigned indexIdCount,
    const IlcSpvId* indexIds);

void ilcSpvPutDecoration(
    IlcSpvModule* module,
    IlcSpvId target,
    IlcSpvWord decoration,
    unsigned argCount,
    const IlcSpvWord* args);

void ilcSpvPutMemberDecoration(
    IlcSpvModule* module,
    IlcSpvId structureTypeId,
    IlcSpvWord member,
    IlcSpvWord decoration,
    unsigned argCount,
    const IlcSpvWord* args);

IlcSpvId ilcSpvPutVectorShuffle(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId vec1Id,
    IlcSpvId vec2Id,
    unsigned componentCount,
    const IlcSpvWord* components);

IlcSpvId ilcSpvPutCompositeConstruct(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    unsigned consistuentCount,
    const IlcSpvId* consistuents);

IlcSpvId ilcSpvPutCompositeExtract(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId compositeId,
    unsigned indexCount,
    const IlcSpvId* indexes);

IlcSpvId ilcSpvPutSampledImage(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId imageId,
    IlcSpvId samplerId);

IlcSpvId ilcSpvPutImageSample(
    IlcSpvModule* module,
    IlcSpvWord sampleOp,
    IlcSpvId resultTypeId,
    IlcSpvId sampledImageId,
    IlcSpvId coordinateId,
    IlcSpvId argId,
    SpvImageOperandsMask mask,
    unsigned operandIdCount,
    const IlcSpvId* operandIds);

IlcSpvId ilcSpvPutImageFetch(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId imageId,
    IlcSpvId coordinateId,
    SpvImageOperandsMask mask,
    unsigned operandIdCount,
    const IlcSpvId* operandIds);

IlcSpvId ilcSpvPutImageRead(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId imageId,
    IlcSpvId coordinateId);

void ilcSpvPutImageWrite(
    IlcSpvModule* module,
    IlcSpvId imageId,
    IlcSpvId coordinateId,
    IlcSpvId texelId);

IlcSpvId ilcSpvPutImageQuerySizeLod(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId imageId,
    IlcSpvId lodId);

IlcSpvId ilcSpvPutImageQueryLevels(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId imageId);

IlcSpvId ilcSpvPutOp1(
    IlcSpvModule* module,
    SpvOp op,
    IlcSpvId resultTypeId,
    IlcSpvId arg0Id);

IlcSpvId ilcSpvPutOp2(
    IlcSpvModule* module,
    SpvOp op,
    IlcSpvId resultTypeId,
    IlcSpvId arg0Id,
    IlcSpvId arg1Id);

IlcSpvId ilcSpvPutOp3(
    IlcSpvModule* module,
    SpvOp op,
    IlcSpvId resultTypeId,
    IlcSpvId arg0Id,
    IlcSpvId arg1Id,
    IlcSpvId arg2Id);

IlcSpvId ilcSpvPutOp4(
    IlcSpvModule* module,
    SpvOp op,
    IlcSpvId resultTypeId,
    IlcSpvId arg0Id,
    IlcSpvId arg1Id,
    IlcSpvId arg2Id,
    IlcSpvId arg3Id);

IlcSpvId ilcSpvPutAtomicOp(
    IlcSpvModule* module,
    SpvOp op,
    IlcSpvId resultTypeId,
    IlcSpvId pointerId,
    IlcSpvWord memoryId,
    IlcSpvWord semanticsId,
    IlcSpvId valueId);

IlcSpvId ilcSpvPutBitcast(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId operandId);

IlcSpvId ilcSpvPutSelect(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId conditionId,
    IlcSpvId obj1Id,
    IlcSpvId obj2Id);

void ilcSpvPutControlBarrier(
    IlcSpvModule* module,
    IlcSpvId executionId,
    IlcSpvId memoryId,
    IlcSpvId semanticsId);

void ilcSpvPutMemoryBarrier(
    IlcSpvModule* module,
    IlcSpvId memoryId,
    IlcSpvId semanticsId);

void ilcSpvPutLoopMerge(
    IlcSpvModule* module,
    IlcSpvId mergeBlockId,
    IlcSpvId continueTargetId);

void ilcSpvPutSelectionMerge(
    IlcSpvModule* module,
    IlcSpvId mergeBlockId);

IlcSpvId ilcSpvPutLabel(
    IlcSpvModule* module,
    IlcSpvId labelId);

void ilcSpvPutBranch(
    IlcSpvModule* module,
    IlcSpvId labelId);

void ilcSpvPutBranchConditional(
    IlcSpvModule* module,
    IlcSpvId conditionId,
    IlcSpvId trueLabelId,
    IlcSpvId falseLabelId);

void ilcSpvPutReturn(
    IlcSpvModule* module);

IlcSpvId ilcSpvPutGLSLOp(
    IlcSpvModule* module,
    enum GLSLstd450 glslOp,
    IlcSpvId resultTypeId,
    unsigned idCount,
    const IlcSpvId* ids);

void ilcSpvPutDemoteToHelperInvocation(
    IlcSpvModule* module);

#endif // AMDILC_SPIRV_H_
