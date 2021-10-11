#include "amdilc_spirv.h"
#include "amdilc_internal.h"

typedef struct {
    IlcSpvId varId;
    IlcSpvWord location;
} IlcInputRegister;

typedef struct {
    IlcSpvBuffer sourceBuffer;
    IlcSpvModule* module;
    IlcSpvId entryPointId;
    SpvExecutionModel execModel;
    const char* entryPointName;
    IlcSpvId* interfaces;
    unsigned interfaceCount;
    IlcInputRegister* existingInputRegisters;
    unsigned existingInputCount;
    unsigned outputPointsCount;
    IlcSpvId invocationVarId;
    IlcSpvId floatId;
    IlcSpvId float4Id;
    IlcSpvId intId;
    bool isInFunction;
    bool isAfterReturn;
} IlcRecompiler;

IlcRecompiledShader ilcRecompileKernel(
    const IlcSpvWord* spirvWords,
    unsigned wordCount,
    const unsigned* inputPassthroughLocations,
    unsigned passthroughCount)
{
    IlcSpvModule module;
    module.currentId = 0;
    for (int i = 0; i < ID_MAX; i++) {
        module.buffer[i] = (IlcSpvBuffer) { 0, NULL };
    }

    IlcRecompiler recompiler = (IlcRecompiler){
        .module = &module,
        .entryPointId = 0,
        .execModel = 0,
        .entryPointName = NULL,
        .interfaces = NULL,
        .interfaceCount = 0,
        .existingInputRegisters = NULL,
        .existingInputCount = 0,
        .outputPointsCount = 0,
        .invocationVarId = 0,
        .floatId = 0,
        .float4Id = 0,
        .intId = 0,
        .isInFunction = false,
        .isAfterReturn = false,
    };
    //header will be inserted at finish
    unsigned bufferIndex = ID_CAPABILITIES;
    unsigned bufferStart = 5;
    for (unsigned i = 5; i < wordCount;) {
        SpvOp opCode = spirvWords[i] & SpvOpCodeMask;
        unsigned instrWordCount = spirvWords[i] >> SpvWordCountShift;
        unsigned newBufferIndex = getBufferIndex(opCode);
        if (newBufferIndex != bufferIndex) {
            if (bufferIndex != ID_ENTRY_POINTS && bufferIndex != ID_CODE) {
                // skip the entry point as it will be rewritten
                ilcSpvUnwrapBuffer(&module.buffer[bufferIndex], &spirvWords[bufferStart], i - bufferStart);
            }
            bufferIndex = newBufferIndex;
            bufferStart = i;
        }
        bool finishProcessing = false;
        switch (bufferIndex) {
        case ID_TYPES:
        case ID_TYPES_WITH_CONSTANTS:
            if (opCode == SpvOpTypeFloat && spirvWords[i + 2] == 32) {
                recompiler.floatId = spirvWords[i + 1];
            } else if (opCode == SpvOpTypeVector && spirvWords[i + 2] == recompiler.floatId && spirvWords[i + 3] == 4) {
                recompiler.float4Id = spirvWords[i + 1];
            } else if (opCode == SpvOpTypeInt && spirvWords[i + 2] == 32 && spirvWords[i + 3]) {
                recompiler.intId = spirvWords[i + 1];
            }
            module.currentId = module.currentId < spirvWords[i + 1] ? spirvWords[i + 1] : module.currentId;
            break;
        case ID_ENTRY_POINTS:
            if (opCode == SpvOpEntryPoint) {
                recompiler.execModel = spirvWords[i + 1];
                recompiler.entryPointId = spirvWords[i + 2];
                recompiler.entryPointName = (const char*)&spirvWords[i + 3];
                unsigned nameLength = (strlen(recompiler.entryPointName) + 4) / sizeof(IlcSpvWord);
                recompiler.interfaceCount = instrWordCount - nameLength - 3;
                recompiler.interfaces = malloc(recompiler.interfaceCount * sizeof(IlcSpvWord));
                memcpy(recompiler.interfaces, &spirvWords[i + 3 + nameLength], recompiler.interfaceCount * sizeof(IlcSpvWord));
            }
            break;
        case ID_EXEC_MODES:
            if (opCode == SpvOpExecutionMode && spirvWords[i + 2] == SpvExecutionModeOutputVertices) {
                recompiler.outputPointsCount = spirvWords[i + 3];
            }
            break;
        case ID_VARIABLES:
            if (opCode == SpvOpVariable && spirvWords[i + 3] == SpvStorageClassInput) {
                bool foundLocation = false;
                IlcSpvWord locationIdx = 0;
                IlcSpvId varId = spirvWords[i + 2];
                for (unsigned j = 0; !foundLocation && j < module.buffer[ID_DECORATIONS].wordCount;) {
                    SpvOp decorOpCode = module.buffer[ID_DECORATIONS].words[j] & SpvOpCodeMask;
                    unsigned decorInstrWordCount = module.buffer[ID_DECORATIONS].words[j] >> SpvWordCountShift;
                    if (decorOpCode == SpvOpDecorate && module.buffer[ID_DECORATIONS].words[j + 1] == varId &&
                         module.buffer[ID_DECORATIONS].words[j + 2] == SpvDecorationLocation) {
                        locationIdx = module.buffer[ID_DECORATIONS].words[j + 3];
                        foundLocation = true;
                    } else if (decorOpCode == SpvOpDecorate && module.buffer[ID_DECORATIONS].words[j + 1] == varId &&
                               module.buffer[ID_DECORATIONS].words[j + 2] == SpvDecorationBuiltIn &&
                               module.buffer[ID_DECORATIONS].words[j + 3] == SpvBuiltInInvocationId) {
                        recompiler.invocationVarId = varId;
                    }
                    j += decorInstrWordCount;
                }
                if (foundLocation) {
                    recompiler.existingInputRegisters = realloc(recompiler.existingInputRegisters, (1 + recompiler.existingInputCount) * sizeof(IlcInputRegister));
                    recompiler.existingInputRegisters[recompiler.existingInputCount] = (IlcInputRegister) {
                        .varId = varId,
                        .location = locationIdx,
                    };
                    recompiler.existingInputCount++;
                }
            }
            break;
        case ID_CODE:
            if (opCode == SpvOpFunction && spirvWords[i + 2] == recompiler.entryPointId) {
                recompiler.isInFunction = true;
            } else if (opCode == SpvOpStore) {
                module.currentId = module.currentId < spirvWords[i + 1] ? spirvWords[i + 1] : module.currentId;
            } else if (opCode == SpvOpLoad) {
                module.currentId = module.currentId < spirvWords[i + 2] ? spirvWords[i + 2] : module.currentId;
            }
            if (opCode == SpvOpReturn && recompiler.isInFunction) {
                finishProcessing = true;
            } else {
                // copy the code over
                ilcSpvUnwrapBuffer(&module.buffer[ID_CODE], &spirvWords[i], instrWordCount);
            }
            break;
        }
        if (finishProcessing) {
            break;
        }
        i += instrWordCount;
    }
    // HACK: just add offset to avoid collision
    module.currentId += 65536;
    //TODO: handle outputs checking
    IlcSpvId float4InputPtrTypeId = ilcSpvPutPointerType(&module, SpvStorageClassInput, recompiler.float4Id);
    IlcSpvId float4OutputPtrTypeId = ilcSpvPutPointerType(&module, SpvStorageClassOutput, recompiler.float4Id);
    if (recompiler.execModel == SpvExecutionModelTessellationControl) {
        if (recompiler.invocationVarId == 0) {
            IlcSpvId intPtrInputId = ilcSpvPutPointerType(&module, SpvStorageClassInput, recompiler.intId);
            recompiler.invocationVarId  = ilcSpvPutVariable(&module, intPtrInputId, SpvStorageClassInput);
            IlcSpvWord builtInType = SpvBuiltInInvocationId;
            ilcSpvPutDecoration(&module, recompiler.invocationVarId, SpvDecorationBuiltIn, 1, &builtInType);
            ilcSpvPutName(&module, recompiler.invocationVarId, "invocationId");
            recompiler.interfaces = realloc(recompiler.interfaces, (recompiler.interfaceCount + 1) * sizeof(IlcSpvId));
            recompiler.interfaces[recompiler.interfaceCount] = recompiler.invocationVarId;
            recompiler.interfaceCount++;
        }
        int maxArraySize = -1;
        for (unsigned i = 0; i < passthroughCount; ++i) {
            maxArraySize = maxArraySize < (int)inputPassthroughLocations[i] ? inputPassthroughLocations[i] : maxArraySize;
        }
        for (unsigned i = 0; i < recompiler.existingInputCount; ++i) {
            maxArraySize = maxArraySize < (int)recompiler.existingInputRegisters[i].location ? recompiler.existingInputRegisters[i].location : maxArraySize;
        }
        maxArraySize++;
        if (maxArraySize <= 0) {
            goto finish;
        }
        //vertex count
        if (recompiler.outputPointsCount == 0) {
            LOGW("didn't handle output control point count\n");
            recompiler.outputPointsCount = 3;
        }
        IlcSpvId vertexLengthId = ilcSpvPutConstant(&module, recompiler.intId, recompiler.outputPointsCount);

        //TODO: check input/output vertex count
        IlcSpvId inputArrTypeId = ilcSpvPutArrayType(&module, recompiler.float4Id, vertexLengthId);
        IlcSpvId inputVarTypeId = ilcSpvPutPointerType(&module, SpvStorageClassInput, inputArrTypeId);

        IlcSpvId outputLengthId = ilcSpvPutConstant(&module, recompiler.intId, maxArraySize);
        // array of registers per vertex
        IlcSpvId outputArrTypeId = ilcSpvPutArrayType(&module, recompiler.float4Id, outputLengthId);
        // array of registers per primitive
        IlcSpvId outputVArrTypeId = ilcSpvPutArrayType(&module, outputArrTypeId, vertexLengthId);
        IlcSpvId outputVArrPtrTypeId = ilcSpvPutPointerType(&module, SpvStorageClassOutput, outputVArrTypeId);
        IlcSpvId outputVArrId = ilcSpvPutVariable(&module, outputVArrPtrTypeId, SpvStorageClassOutput);
        ilcSpvPutName(&module, outputVArrId, "vertex_out");

        recompiler.interfaces = realloc(recompiler.interfaces, (recompiler.interfaceCount + 1) * sizeof(IlcSpvId));
        recompiler.interfaces[recompiler.interfaceCount] = outputVArrId;
        recompiler.interfaceCount++;
        IlcSpvWord outputLocationIdx = 0;
        ilcSpvPutDecoration(&module, outputVArrId, SpvDecorationLocation, 1, &outputLocationIdx);
        for (unsigned i = 0; i < passthroughCount; ++i) {
            bool includesLocation = false;
            IlcSpvId inputVariableId = 0;
            for (unsigned j = 0; j < recompiler.existingInputCount; ++j) {
                if (recompiler.existingInputRegisters[j].location == inputPassthroughLocations[i]) {
                    includesLocation = true;
                    inputVariableId = recompiler.existingInputRegisters[j].varId;
                }
            }
            if (!includesLocation) {
                char name[32];
                snprintf(name, sizeof(name), "vicp_patched%u", inputPassthroughLocations[i]);
                inputVariableId = ilcSpvPutVariable(&module, inputVarTypeId, SpvStorageClassInput);
                ilcSpvPutName(&module, inputVariableId, name);
                ilcSpvPutDecoration(&module, inputVariableId, SpvDecorationLocation, 1, &inputPassthroughLocations[i]);

                recompiler.interfaces = realloc(recompiler.interfaces, (recompiler.interfaceCount + 1) * sizeof(IlcSpvId));
                recompiler.interfaces[recompiler.interfaceCount] = inputVariableId;
                recompiler.interfaceCount++;
            }
            IlcSpvId inputIndexId = ilcSpvPutConstant(&module, recompiler.intId, inputPassthroughLocations[i]);
            IlcSpvId invocationValueId = ilcSpvPutLoad(&module, recompiler.intId, recompiler.invocationVarId);

            IlcSpvId inputPtrId = ilcSpvPutAccessChain(&module, float4InputPtrTypeId, inputVariableId, 1, &invocationValueId);
            IlcSpvId loadedInputId = ilcSpvPutLoad(&module, recompiler.float4Id, inputPtrId);
            IlcSpvId indexesId[] = {
                invocationValueId,
                inputIndexId,
            };
            IlcSpvId dstId = ilcSpvPutAccessChain(&module, float4OutputPtrTypeId, outputVArrId, 2, indexesId );
            ilcSpvPutStore(&module, dstId, loadedInputId);
        }
    } else {
        for (unsigned i = 0; i < passthroughCount; ++i) {
            bool includesLocation = false;
            IlcSpvId inputVariableId = 0;
            for (unsigned j = 0; j < recompiler.existingInputCount; ++j) {
                if (recompiler.existingInputRegisters[j].location == inputPassthroughLocations[i]) {
                    includesLocation = true;
                    inputVariableId = recompiler.existingInputRegisters[j].varId;
                }
            }
            if (includesLocation) {
                // no need to passthrough
                continue;
            }
            inputVariableId = ilcSpvPutVariable(&module, float4InputPtrTypeId, SpvStorageClassInput);
            IlcSpvId outputVariableId = ilcSpvPutVariable(&module, float4OutputPtrTypeId, SpvStorageClassOutput);
            ilcSpvPutDecoration(&module, inputVariableId,  SpvDecorationLocation, 1, &inputPassthroughLocations[i]);
            ilcSpvPutDecoration(&module, outputVariableId, SpvDecorationLocation, 1, &inputPassthroughLocations[i]);

            IlcSpvId valueId = ilcSpvPutLoad(&module, recompiler.float4Id, inputVariableId);
            ilcSpvPutStore(&module, outputVariableId, valueId);

            recompiler.interfaces = realloc(recompiler.interfaces, (recompiler.interfaceCount + 2) * sizeof(IlcSpvId));
            recompiler.interfaces[recompiler.interfaceCount] = outputVariableId;
            recompiler.interfaces[recompiler.interfaceCount + 1] = inputVariableId;
            recompiler.interfaceCount += 2;
        }
    }
finish:
    ilcSpvPutReturn(&module);
    ilcSpvPutFunctionEnd(&module);
    recompiler.isInFunction = false;
    ilcSpvPutEntryPoint(&module, recompiler.entryPointId, recompiler.execModel, recompiler.entryPointName,
                        recompiler.interfaceCount, recompiler.interfaces);
    //inject some code
    ilcSpvFinish(&module);
    free(recompiler.existingInputRegisters);
    free(recompiler.interfaces);

    return (IlcRecompiledShader) {
        .codeSize = sizeof(IlcSpvWord) * module.buffer[ID_MAIN].wordCount,
        .code = module.buffer[ID_MAIN].words,
    };
}
