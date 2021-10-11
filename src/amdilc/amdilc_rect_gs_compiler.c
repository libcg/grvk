#include "amdilc_spirv.h"
#include "amdilc_internal.h"

// TODO cleanup

#define ONE_LITERAL         (0x3F800000)
#define MINUS_ONE_LITERAL   (0xBF800000)

static IlcSpvId interpolateToLastPoint(
    IlcSpvModule* module,
    IlcSpvId resTypeId,
    const IlcSpvId* barycentricCoords,
    const IlcSpvId* inputs)
{
    const IlcSpvId termIds[3] = {
        ilcSpvPutOp2(module, SpvOpVectorTimesScalar, resTypeId, inputs[0], barycentricCoords[0]),
        ilcSpvPutOp2(module, SpvOpVectorTimesScalar, resTypeId, inputs[1], barycentricCoords[1]),
        ilcSpvPutOp2(module, SpvOpVectorTimesScalar, resTypeId, inputs[2], barycentricCoords[2]),
    };

    IlcSpvId resId = ilcSpvPutOp2(module, SpvOpFAdd, resTypeId, termIds[0], termIds[1]);
    return ilcSpvPutOp2(module, SpvOpFAdd, resTypeId, resId, termIds[2]);
}

static IlcSpvId createEndPrimitiveFunction(
    IlcSpvModule* module,
    IlcSpvId vertexCounterVarId,
    IlcSpvId outputPositionVarId,
    IlcSpvId outputPositionBufferVarId,
    SpvStorageClass bufferStorageClass,
    unsigned outputCount,
    const IlcSpvId* outputIds,
    const uint32_t* outputInterpType,
    const IlcSpvId* outputBufferIds)
{
    IlcSpvId voidTypeId = ilcSpvPutVoidType(module);
    IlcSpvId funcTypeId = ilcSpvPutFunctionType(module, voidTypeId, 0, NULL);

    IlcSpvId funcId = ilcSpvAllocId(module);
    ilcSpvPutFunction(module, voidTypeId, funcId, SpvFunctionControlMaskNone, funcTypeId);
    ilcSpvPutLabel(module, 0);

    IlcSpvId uintTypeId = ilcSpvPutIntType(module, false);

    IlcSpvId floatId = ilcSpvPutFloatType(module);
    IlcSpvId float4Id = ilcSpvPutVectorType(module, floatId, 4);

    IlcSpvId float4PtrId = ilcSpvPutPointerType(module, bufferStorageClass, float4Id);

    //fill up the vertices first
    IlcSpvId counterIds[] = {
        ilcSpvPutConstant(module, uintTypeId, 0),
        ilcSpvPutConstant(module, uintTypeId, 1),
        ilcSpvPutConstant(module, uintTypeId, 2),
        ilcSpvPutConstant(module, uintTypeId, 3),
    };
    IlcSpvId positionElements[4];

    for (unsigned i = 0; i < 3; i++) {
        positionElements[i] = ilcSpvPutAccessChain(module, float4PtrId, outputPositionBufferVarId, 1, &counterIds[i]);
        positionElements[i] = ilcSpvPutLoad(module, float4Id, positionElements[i]);
    }
    IlcSpvId positionsX[3];
    IlcSpvId positionsY[3];
    for (unsigned i = 0; i < 3; i++) {
        unsigned xIndex = 0;
        unsigned yIndex = 1;
        positionsX[i] = ilcSpvPutCompositeExtract(module, floatId, positionElements[i], 1, &xIndex);
        positionsY[i] = ilcSpvPutCompositeExtract(module, floatId, positionElements[i], 1, &yIndex);
    }
    IlcSpvId pointCoordEqualX[3];
    IlcSpvId pointCoordEqualY[3];
    IlcSpvId isEdgeVertex[3];
    IlcSpvId barycentricCoords[3];
    IlcSpvId boolTypeId = ilcSpvPutBoolType(module);
    IlcSpvId fOneId = ilcSpvPutConstant(module, floatId, ONE_LITERAL);
    IlcSpvId fMinusOneId = ilcSpvPutConstant(module, floatId, MINUS_ONE_LITERAL);
    for (unsigned i = 0; i < 3; i++) {
        pointCoordEqualX[i] = ilcSpvPutOp2(module, SpvOpFOrdEqual, boolTypeId, positionsX[i], positionsX[(i + 1) % 3]);
        pointCoordEqualY[i] = ilcSpvPutOp2(module, SpvOpFOrdEqual, boolTypeId, positionsY[i], positionsY[(i + 1) % 3]);
    }
    for (unsigned i = 0; i < 3; i++) {
        IlcSpvId xyEqual = ilcSpvPutOp2(module, SpvOpLogicalAnd, boolTypeId, pointCoordEqualX[i], pointCoordEqualY[(i + 2) % 3]);
        IlcSpvId yxEqual = ilcSpvPutOp2(module, SpvOpLogicalAnd, boolTypeId, pointCoordEqualY[i], pointCoordEqualX[(i + 2) % 3]);
        isEdgeVertex[i] = ilcSpvPutOp2(module, SpvOpLogicalOr, boolTypeId, xyEqual, yxEqual);
        barycentricCoords[i] = ilcSpvPutSelect(module, floatId, isEdgeVertex[i], fMinusOneId, fOneId);
    }
    positionElements[3] = interpolateToLastPoint(module, float4Id, barycentricCoords, positionElements);
    // good now gotta select first vertex index dynamically (bs)
    IlcSpvId vertexIndexId = ilcSpvPutSelect(module, uintTypeId,
                                             isEdgeVertex[1],
                                             counterIds[1], counterIds[0]);
    vertexIndexId = ilcSpvPutSelect(module, uintTypeId,
                                    isEdgeVertex[2],
                                    counterIds[2], vertexIndexId);
    // idk about the order of vertices
    for (unsigned i = 0; i < 3; i++) {
        IlcSpvId posPtrId = ilcSpvPutAccessChain(module, float4PtrId, outputPositionBufferVarId, 1, &vertexIndexId);
        IlcSpvId posId = ilcSpvPutLoad(module, float4Id, posPtrId);
        ilcSpvPutStore(module, outputPositionVarId, posId);
        for (unsigned j = 0; j < outputCount; ++j) {
            IlcSpvId valueId = ilcSpvPutAccessChain(module, float4PtrId, outputBufferIds[j], 1, outputInterpType[j] == IL_INTERPMODE_CONSTANT ? &counterIds[0] : &vertexIndexId);
            valueId = ilcSpvPutLoad(module, float4Id, valueId);
            ilcSpvPutStore(module, outputIds[j], valueId);
        }
        ilcSpvPutEmitVertex(module);
        // increment the vertex counter with modulo
        vertexIndexId = ilcSpvPutOp2(module, SpvOpIAdd, uintTypeId, vertexIndexId, counterIds[1]);
        vertexIndexId = ilcSpvPutOp2(module, SpvOpUMod, uintTypeId, vertexIndexId, counterIds[3]);
    }

    ilcSpvPutStore(module, outputPositionVarId, positionElements[3]);
    for (unsigned i = 0; i < outputCount; i++) {
        IlcSpvId valueId;
        if (outputInterpType[i] == IL_INTERPMODE_CONSTANT) {
            valueId = ilcSpvPutAccessChain(module, float4PtrId, outputBufferIds[i], 1, &counterIds[0]);
            valueId = ilcSpvPutLoad(module, float4Id, valueId);
        } else {
            IlcSpvId outputs[] = {
                ilcSpvPutAccessChain(module, float4PtrId, outputBufferIds[i], 1, &counterIds[0]),
                ilcSpvPutAccessChain(module, float4PtrId, outputBufferIds[i], 1, &counterIds[1]),
                ilcSpvPutAccessChain(module, float4PtrId, outputBufferIds[i], 1, &counterIds[2]),
            };
            for (unsigned j = 0; j < 3; j++) {
                outputs[j] = ilcSpvPutLoad(module, float4Id, outputs[j]);
            }
            valueId = interpolateToLastPoint(module, float4Id, barycentricCoords, outputs);
        }
        ilcSpvPutStore(module, outputIds[i], valueId);
    }
    ilcSpvPutEmitVertex(module);
    ilcSpvPutEndPrimitive(module);
    if (vertexCounterVarId != 0) {
        ilcSpvPutStore(module, vertexCounterVarId, counterIds[0]);
    }
    ilcSpvPutReturn(module);
    ilcSpvPutFunctionEnd(module);
    return funcId;
}

IlcShader ilcCompileRectangleGeometryShader(
    unsigned psInputCount,
    const IlcInput* psInputs)
{
    IlcSpvModule module;
    unsigned interfaceCount = psInputCount * 2 + 2;
    IlcSpvId* interfaces = malloc(interfaceCount * sizeof(IlcSpvId));

    ilcSpvInit(&module);

    IlcSpvId floatId = ilcSpvPutFloatType(&module);
    IlcSpvId float4Id = ilcSpvPutVectorType(&module, floatId, 4);
    IlcSpvId intId = ilcSpvPutIntType(&module, true);

    IlcSpvId lengthId = ilcSpvPutConstant(&module, intId, 3);
    IlcSpvId float4ArrId = ilcSpvPutArrayType(&module, float4Id, lengthId);

    IlcSpvId float4OutPtrId = ilcSpvPutPointerType(&module, SpvStorageClassOutput, float4Id);
    IlcSpvId float4ArrPtrId = ilcSpvPutPointerType(&module, SpvStorageClassInput, float4ArrId);

    IlcSpvId* genericInputs = interfaces;
    IlcSpvId* genericOutputs = &interfaces[psInputCount];

    uint32_t* interpolationTypes = malloc(sizeof(uint32_t) * psInputCount);

    for (unsigned i = 0; i < psInputCount; i++) {
        genericInputs[i]  = ilcSpvPutVariable(&module, float4ArrPtrId, SpvStorageClassInput);
        ilcSpvPutDecoration(&module, genericInputs[i], SpvDecorationLocation, 1, &psInputs[i].locationIndex);
        genericOutputs[i] = ilcSpvPutVariable(&module, float4OutPtrId, SpvStorageClassOutput);
        ilcSpvPutDecoration(&module, genericOutputs[i], SpvDecorationLocation, 1, &psInputs[i].locationIndex);

        interpolationTypes[i] = psInputs[i].interpMode;
    }
    IlcSpvId inputPositionVarId  = ilcSpvPutVariable(&module, float4ArrPtrId, SpvStorageClassInput);
    ilcSpvPutName(&module, inputPositionVarId, "vInPos");
    IlcSpvId outputPositionVarId = ilcSpvPutVariable(&module, float4OutPtrId, SpvStorageClassOutput);
    ilcSpvPutName(&module, outputPositionVarId, "vOutPos");
    IlcSpvWord builtInPosType = SpvBuiltInPosition;
    ilcSpvPutDecoration(&module, inputPositionVarId , SpvDecorationBuiltIn, 1, &builtInPosType);
    ilcSpvPutDecoration(&module, outputPositionVarId, SpvDecorationBuiltIn, 1, &builtInPosType);

    IlcSpvId entryPointId = createEndPrimitiveFunction(
        &module,
        0,
        outputPositionVarId,
        inputPositionVarId,
        SpvStorageClassInput,
        psInputCount,
        genericOutputs,
        interpolationTypes,
        genericInputs);
    interfaces[psInputCount * 2] = inputPositionVarId;
    interfaces[psInputCount * 2 + 1] = outputPositionVarId;
    ilcSpvPutName(&module, entryPointId, "main");
    // insert capability
    ilcSpvPutCapability(&module, SpvCapabilityGeometry);
    // insert exec modes
    IlcSpvWord outputVertCount = 4;
    ilcSpvPutExecMode(&module, entryPointId, SpvExecutionModeOutputVertices, 1, &outputVertCount);
    IlcSpvWord invocationCount = 1;
    ilcSpvPutExecMode(&module, entryPointId, SpvExecutionModeInvocations, 1, &invocationCount);
    ilcSpvPutExecMode(&module, entryPointId, SpvExecutionModeOutputTriangleStrip, 0, NULL);
    ilcSpvPutExecMode(&module, entryPointId, SpvExecutionModeTriangles, 0, NULL);
    ilcSpvPutEntryPoint(&module, entryPointId, SpvExecutionModelGeometry, "main",
                        interfaceCount, interfaces);
    ilcSpvFinish(&module);

    free(interfaces);
    free(interpolationTypes);

    return (IlcShader) {
        .codeSize = sizeof(IlcSpvWord) * module.buffer[ID_MAIN].wordCount,
        .code = module.buffer[ID_MAIN].words,
        .bindingCount = 0,
        .bindings = NULL,
        .inputCount = 0,
        .inputs = NULL,
        .name = NULL,
    };
 }
