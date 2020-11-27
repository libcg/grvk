#ifndef AMDILC_SPIRV_H_
#define AMDILC_SPIRV_H_

#include <stdbool.h>
#include <stdint.h>
#include "spirv/GLSL.std.450.h"
#include "spirv/spirv.h"

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
    unsigned ptr;
    IlcSpvWord* words;
} IlcSpvBuffer;

typedef struct {
    IlcSpvId currentId;
    IlcSpvId glsl450ImportId;
    IlcSpvBuffer buffer[ID_MAX];
} IlcSpvModule;

typedef struct {
    IlcSpvId label;
    uint32_t literal;
} IlcSpvSwitchCase;

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
    const IlcSpvWord* interfaces);

void ilcSpvPutExecMode(
    IlcSpvModule* module,
    IlcSpvId id,
    SpvExecutionMode execMode);

void ilcSpvPutCapability(
    IlcSpvModule* module,
    IlcSpvWord capability);

/*gets current pointer position of the code buffer */
uint32_t ilcSpvGetInsertionPtr(const IlcSpvModule* module);

/*resets position of the code buffer */
void ilcSpvEndInsertion(IlcSpvModule* module);

/*sets pointer of the code buffer to specified position, allowing to insert commands in the middle of the buffer*/
bool ilcSpvBeginInsertion(IlcSpvModule* module, uint32_t newPtr);

unsigned getSpvTypeComponentCount(
    IlcSpvModule* module,
    IlcSpvId typeId,
    IlcSpvId *outScalarTypeId);

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

IlcSpvId ilcSpvPutSampledImageType(
    IlcSpvModule* module,
    IlcSpvId imageTypeId);

IlcSpvId ilcSpvPutSamplerType(
    IlcSpvModule* module);

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

IlcSpvId ilcSpvPutImageSample(
    IlcSpvModule* module,
    IlcSpvId resultType,
    IlcSpvId sampledImageId,
    IlcSpvId coordinateVariableId,
    IlcSpvId argMask,
    IlcSpvId* operands);

IlcSpvId ilcSpvPutImageSampleDref(
    IlcSpvModule* module,
    IlcSpvId resultType,
    IlcSpvId sampledImageId,
    IlcSpvId coordinateVariableId,
    IlcSpvId drefId,
    IlcSpvId argMask,
    IlcSpvId* operands);

IlcSpvId ilcSpvPutSampledImage(
    IlcSpvModule* module,
    IlcSpvId resultType,
    IlcSpvId imageResourceId,
    IlcSpvId samplerId);


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
    const IlcSpvWord* args);

IlcSpvId ilcSpvPutVectorExtractDynamic(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId vecId,
    IlcSpvId indexId);

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

IlcSpvId ilcSpvPutImageFetch(
    IlcSpvModule* module,
    IlcSpvId resultTypeId,
    IlcSpvId imageId,
    IlcSpvId coordinateId);

IlcSpvId ilcSpvPutAlu(
    IlcSpvModule* module,
    SpvOp op,
    IlcSpvId resultTypeId,
    unsigned idCount,
    const IlcSpvId* ids);

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

void ilcSpvPutSwitch(
    IlcSpvModule* module,
    IlcSpvId selectorId,
    IlcSpvId defaultLabelId,
    uint32_t caseSize,
    const IlcSpvSwitchCase* cases);

void ilcSpvPutReturn(
    IlcSpvModule* module);

IlcSpvId ilcSpvPutGLSLOp(
    IlcSpvModule* module,
    enum GLSLstd450 glslOp,
    IlcSpvId resultTypeId,
    unsigned idCount,
    const IlcSpvId* ids);

#endif // AMDILC_SPIRV_H_
