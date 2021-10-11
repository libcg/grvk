#include "amdilc_spirv.h"
#include "amdilc_internal.h"

IlcSpvId createEmitVertexFunction(
    IlcSpvModule* module,
    IlcSpvId outputPositionVarId,
    unsigned outputCount,
    const IlcSpvId* outputVarIds,
    IlcSpvId* bufferedOutputVarIds,
    IlcSpvId* bufferedOutputPositionVarId,
    IlcSpvId* outVertexCounterId);

IlcSpvId createEndPrimitiveFunction(
    IlcSpvModule* module,
    IlcSpvId vertexCounterVarId,
    IlcSpvId outputPositionVarId,
    IlcSpvId outputPositionBufferVarId,
    unsigned outputCount,
    const IlcSpvId* outputIds,
    const uint32_t* outputInterpType,
    const IlcSpvId* outputBufferIds);
