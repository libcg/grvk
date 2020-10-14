#include <string.h>
#include "mantle/mantle.h"
#include "mantle/mantleDbg.h"
#include "mantle/mantleWsiWinExt.h"
#include "logger.h"

// Initialization and Device Functions

GR_RESULT grDestroyDevice(
    GR_DEVICE device)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

// Extension Discovery Functions

GR_RESULT grGetExtensionSupport(
    GR_PHYSICAL_GPU gpu,
    const GR_CHAR* pExtName)
{
    LOGW("STUB\n");

    if (strcmp(pExtName, "GR_WSI_WINDOWS") == 0) {
        return GR_SUCCESS;
    }

    return GR_UNSUPPORTED;
}

// Queue Functions

GR_RESULT grQueueSetGlobalMemReferences(
    GR_QUEUE queue,
    GR_UINT memRefCount,
    const GR_MEMORY_REF* pMemRefs)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

// Memory Management Functions

GR_RESULT grFreeMemory(
    GR_GPU_MEMORY mem)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grSetMemoryPriority(
    GR_GPU_MEMORY mem,
    GR_ENUM priority)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grRemapVirtualMemoryPages(
    GR_DEVICE device,
    GR_UINT rangeCount,
    const GR_VIRTUAL_MEMORY_REMAP_RANGE* pRanges,
    GR_UINT preWaitSemaphoreCount,
    const GR_QUEUE_SEMAPHORE* pPreWaitSemaphores,
    GR_UINT postSignalSemaphoreCount,
    const GR_QUEUE_SEMAPHORE* pPostSignalSemaphores)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grPinSystemMemory(
    GR_DEVICE device,
    const GR_VOID* pSysMem,
    GR_SIZE memSize,
    GR_GPU_MEMORY* pMem)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

// Generic API Object Management functions

GR_RESULT grDestroyObject(
    GR_OBJECT object)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

// Image and Sample Functions

GR_RESULT grGetFormatInfo(
    GR_DEVICE device,
    GR_FORMAT format,
    GR_ENUM infoType,
    GR_SIZE* pDataSize,
    GR_VOID* pData)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCreateImage(
    GR_DEVICE device,
    const GR_IMAGE_CREATE_INFO* pCreateInfo,
    GR_IMAGE* pImage)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grGetImageSubresourceInfo(
    GR_IMAGE image,
    const GR_IMAGE_SUBRESOURCE* pSubresource,
    GR_ENUM infoType,
    GR_SIZE* pDataSize,
    GR_VOID* pData)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

// Image View Functions

GR_RESULT grCreateImageView(
    GR_DEVICE device,
    const GR_IMAGE_VIEW_CREATE_INFO* pCreateInfo,
    GR_IMAGE_VIEW* pView)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCreateDepthStencilView(
    GR_DEVICE device,
    const GR_DEPTH_STENCIL_VIEW_CREATE_INFO* pCreateInfo,
    GR_DEPTH_STENCIL_VIEW* pView)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

// Shader and Pipeline Functions

GR_RESULT grCreateComputePipeline(
    GR_DEVICE device,
    const GR_COMPUTE_PIPELINE_CREATE_INFO* pCreateInfo,
    GR_PIPELINE* pPipeline)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grStorePipeline(
    GR_PIPELINE pipeline,
    GR_SIZE* pDataSize,
    GR_VOID* pData)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grLoadPipeline(
    GR_DEVICE device,
    GR_SIZE dataSize,
    const GR_VOID* pData,
    GR_PIPELINE* pPipeline)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

// Query and Synchronization Functions

GR_RESULT grCreateQueryPool(
    GR_DEVICE device,
    const GR_QUERY_POOL_CREATE_INFO* pCreateInfo,
    GR_QUERY_POOL* pQueryPool)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grGetQueryPoolResults(
    GR_QUERY_POOL queryPool,
    GR_UINT startQuery,
    GR_UINT queryCount,
    GR_SIZE* pDataSize,
    GR_VOID* pData)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

// Multi-Device Management Functions

GR_RESULT grGetMultiGpuCompatibility(
    GR_PHYSICAL_GPU gpu0,
    GR_PHYSICAL_GPU gpu1,
    GR_GPU_COMPATIBILITY_INFO* pInfo)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grOpenSharedMemory(
    GR_DEVICE device,
    const GR_MEMORY_OPEN_INFO* pOpenInfo,
    GR_GPU_MEMORY* pMem)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grOpenSharedQueueSemaphore(
    GR_DEVICE device,
    const GR_QUEUE_SEMAPHORE_OPEN_INFO* pOpenInfo,
    GR_QUEUE_SEMAPHORE* pSemaphore)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grOpenPeerMemory(
    GR_DEVICE device,
    const GR_PEER_MEMORY_OPEN_INFO* pOpenInfo,
    GR_GPU_MEMORY* pMem)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grOpenPeerImage(
    GR_DEVICE device,
    const GR_PEER_IMAGE_OPEN_INFO* pOpenInfo,
    GR_IMAGE* pImage,
    GR_GPU_MEMORY* pMem)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

// Command Buffer Management Functions

GR_RESULT grResetCommandBuffer(
    GR_CMD_BUFFER cmdBuffer)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

// Command Buffer Building Functions

GR_VOID grCmdBindDynamicMemoryView(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    const GR_MEMORY_VIEW_ATTACH_INFO* pMemView)
{
    LOGW("STUB\n");
}

GR_VOID grCmdBindIndexData(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset,
    GR_ENUM indexType)
{
    LOGW("STUB\n");
}

GR_VOID grCmdDrawIndirect(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset)
{
    LOGW("STUB\n");
}

GR_VOID grCmdDrawIndexedIndirect(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset)
{
    LOGW("STUB\n");
}

GR_VOID grCmdDispatch(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT x,
    GR_UINT y,
    GR_UINT z)
{
    LOGW("STUB\n");
}

GR_VOID grCmdDispatchIndirect(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset)
{
    LOGW("STUB\n");
}

GR_VOID grCmdCopyMemory(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY srcMem,
    GR_GPU_MEMORY destMem,
    GR_UINT regionCount,
    const GR_MEMORY_COPY* pRegions)
{
    LOGW("STUB\n");
}

GR_VOID grCmdCopyImage(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE srcImage,
    GR_IMAGE destImage,
    GR_UINT regionCount,
    const GR_IMAGE_COPY* pRegions)
{
    LOGW("STUB\n");
}

GR_VOID grCmdCopyMemoryToImage(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY srcMem,
    GR_IMAGE destImage,
    GR_UINT regionCount,
    const GR_MEMORY_IMAGE_COPY* pRegions)
{
    LOGW("STUB\n");
}

GR_VOID grCmdCopyImageToMemory(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE srcImage,
    GR_GPU_MEMORY destMem,
    GR_UINT regionCount,
    const GR_MEMORY_IMAGE_COPY* pRegions)
{
    LOGW("STUB\n");
}

GR_VOID grCmdResolveImage(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE srcImage,
    GR_IMAGE destImage,
    GR_UINT regionCount,
    const GR_IMAGE_RESOLVE* pRegions)
{
    LOGW("STUB\n");
}

GR_VOID grCmdCloneImageData(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE srcImage,
    GR_ENUM srcImageState,
    GR_IMAGE destImage,
    GR_ENUM destImageState)
{
    LOGW("STUB\n");
}

GR_VOID grCmdUpdateMemory(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY destMem,
    GR_GPU_SIZE destOffset,
    GR_GPU_SIZE dataSize,
    const GR_UINT32* pData)
{
    LOGW("STUB\n");
}

GR_VOID grCmdFillMemory(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY destMem,
    GR_GPU_SIZE destOffset,
    GR_GPU_SIZE fillSize,
    GR_UINT32 data)
{
    LOGW("STUB\n");
}

GR_VOID grCmdClearDepthStencil(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE image,
    GR_FLOAT depth,
    GR_UINT8 stencil,
    GR_UINT rangeCount,
    const GR_IMAGE_SUBRESOURCE_RANGE* pRanges)
{
    LOGW("STUB\n");
}

GR_VOID grCmdMemoryAtomic(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY destMem,
    GR_GPU_SIZE destOffset,
    GR_UINT64 srcData,
    GR_ENUM atomicOp)
{
    LOGW("STUB\n");
}

GR_VOID grCmdBeginQuery(
    GR_CMD_BUFFER cmdBuffer,
    GR_QUERY_POOL queryPool,
    GR_UINT slot,
    GR_FLAGS flags)
{
    LOGW("STUB\n");
}

GR_VOID grCmdEndQuery(
    GR_CMD_BUFFER cmdBuffer,
    GR_QUERY_POOL queryPool,
    GR_UINT slot)
{
    LOGW("STUB\n");
}

GR_VOID grCmdResetQueryPool(
    GR_CMD_BUFFER cmdBuffer,
    GR_QUERY_POOL queryPool,
    GR_UINT startQuery,
    GR_UINT queryCount)
{
    LOGW("STUB\n");
}

GR_VOID grCmdWriteTimestamp(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM timestampType,
    GR_GPU_MEMORY destMem,
    GR_GPU_SIZE destOffset)
{
    LOGW("STUB\n");
}

GR_VOID grCmdInitAtomicCounters(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_UINT startCounter,
    GR_UINT counterCount,
    const GR_UINT32* pData)
{
    LOGW("STUB\n");
}

GR_VOID grCmdLoadAtomicCounters(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_UINT startCounter,
    GR_UINT counterCount,
    GR_GPU_MEMORY srcMem,
    GR_GPU_SIZE srcOffset)
{
    LOGW("STUB\n");
}

GR_VOID grCmdSaveAtomicCounters(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_UINT startCounter,
    GR_UINT counterCount,
    GR_GPU_MEMORY destMem,
    GR_GPU_SIZE destOffset)
{
    LOGW("STUB\n");
}

// Debug Functions

GR_RESULT GR_STDCALL grDbgSetValidationLevel(
    GR_DEVICE device,
    GR_ENUM validationLevel)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grDbgRegisterMsgCallback(
    GR_DBG_MSG_CALLBACK_FUNCTION pfnMsgCallback,
    GR_VOID* pUserData)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grDbgUnregisterMsgCallback(
    GR_DBG_MSG_CALLBACK_FUNCTION pfnMsgCallback)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grDbgSetMessageFilter(
    GR_DEVICE device,
    GR_ENUM msgCode,
    GR_ENUM filter)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grDbgSetObjectTag(
    GR_BASE_OBJECT object,
    GR_SIZE tagSize,
    const GR_VOID* pTag)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grDbgSetGlobalOption(
    GR_DBG_GLOBAL_OPTION dbgOption,
    GR_SIZE dataSize,
    const GR_VOID* pData)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grDbgSetDeviceOption(
    GR_DEVICE device,
    GR_DBG_DEVICE_OPTION dbgOption,
    GR_SIZE dataSize,
    const GR_VOID* pData)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_VOID GR_STDCALL grCmdDbgMarkerBegin(
    GR_CMD_BUFFER cmdBuffer,
    const GR_CHAR* pMarker)
{
    LOGW("STUB\n");
}

GR_VOID GR_STDCALL grCmdDbgMarkerEnd(
    GR_CMD_BUFFER cmdBuffer)
{
    LOGW("STUB\n");
}

// WSI Functions

GR_RESULT grWsiWinGetDisplays(
    GR_DEVICE device,
    GR_UINT* pDisplayCount,
    GR_WSI_WIN_DISPLAY* pDisplayList)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grWsiWinGetDisplayModeList(
    GR_WSI_WIN_DISPLAY display,
    GR_UINT* pDisplayModeCount,
    GR_WSI_WIN_DISPLAY_MODE* pDisplayModeList)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grWsiWinTakeFullscreenOwnership(
    GR_WSI_WIN_DISPLAY display,
    GR_IMAGE image)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grWsiWinReleaseFullscreenOwnership(
    GR_WSI_WIN_DISPLAY display)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grWsiWinSetGammaRamp(
    GR_WSI_WIN_DISPLAY display,
    const GR_WSI_WIN_GAMMA_RAMP* pGammaRamp)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grWsiWinWaitForVerticalBlank(
    GR_WSI_WIN_DISPLAY display)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grWsiWinGetScanLine(
    GR_WSI_WIN_DISPLAY display,
    GR_INT* pScanLine)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grWsiWinSetMaxQueuedFrames(
    GR_DEVICE device,
    GR_UINT maxFrames)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}
