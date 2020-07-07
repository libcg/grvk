#include <stdio.h>
#include <string.h>
#include "mantle/mantle.h"
#include "mantle/mantleDbg.h"
#include "mantle/mantleWsiWinExt.h"

// Initialization and Device Functions

GR_RESULT grGetGpuInfo(
    GR_PHYSICAL_GPU gpu,
    GR_ENUM infoType,
    GR_SIZE* pDataSize,
    GR_VOID* pData)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grDestroyDevice(
    GR_DEVICE device)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

// Extension Discovery Functions

GR_RESULT grGetExtensionSupport(
    GR_PHYSICAL_GPU gpu,
    const GR_CHAR* pExtName)
{
    printf("STUB: %s\n", __func__);

    if (strcmp(pExtName, "GR_WSI_WINDOWS") == 0) {
        return GR_SUCCESS;
    }

    return GR_UNSUPPORTED;
}

// Queue Functions

GR_RESULT grQueueWaitIdle(
    GR_QUEUE queue)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grDeviceWaitIdle(
    GR_DEVICE device)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grQueueSetGlobalMemReferences(
    GR_QUEUE queue,
    GR_UINT memRefCount,
    const GR_MEMORY_REF* pMemRefs)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

// Memory Management Functions

GR_RESULT grFreeMemory(
    GR_GPU_MEMORY mem)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grSetMemoryPriority(
    GR_GPU_MEMORY mem,
    GR_ENUM priority)
{
    printf("STUB: %s\n", __func__);
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
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grPinSystemMemory(
    GR_DEVICE device,
    const GR_VOID* pSysMem,
    GR_SIZE memSize,
    GR_GPU_MEMORY* pMem)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

// Generic API Object Management functions

GR_RESULT grDestroyObject(
    GR_OBJECT object)
{
    printf("STUB: %s\n", __func__);
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
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grCreateImage(
    GR_DEVICE device,
    const GR_IMAGE_CREATE_INFO* pCreateInfo,
    GR_IMAGE* pImage)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grGetImageSubresourceInfo(
    GR_IMAGE image,
    const GR_IMAGE_SUBRESOURCE* pSubresource,
    GR_ENUM infoType,
    GR_SIZE* pDataSize,
    GR_VOID* pData)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grCreateSampler(
    GR_DEVICE device,
    const GR_SAMPLER_CREATE_INFO* pCreateInfo,
    GR_SAMPLER* pSampler)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

// Image View Functions

GR_RESULT grCreateImageView(
    GR_DEVICE device,
    const GR_IMAGE_VIEW_CREATE_INFO* pCreateInfo,
    GR_IMAGE_VIEW* pView)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grCreateDepthStencilView(
    GR_DEVICE device,
    const GR_DEPTH_STENCIL_VIEW_CREATE_INFO* pCreateInfo,
    GR_DEPTH_STENCIL_VIEW* pView)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

// Shader and Pipeline Functions

GR_RESULT grCreateComputePipeline(
    GR_DEVICE device,
    const GR_COMPUTE_PIPELINE_CREATE_INFO* pCreateInfo,
    GR_PIPELINE* pPipeline)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grStorePipeline(
    GR_PIPELINE pipeline,
    GR_SIZE* pDataSize,
    GR_VOID* pData)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grLoadPipeline(
    GR_DEVICE device,
    GR_SIZE dataSize,
    const GR_VOID* pData,
    GR_PIPELINE* pPipeline)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

// Descriptor Set Functions

GR_VOID grBeginDescriptorSetUpdate(
    GR_DESCRIPTOR_SET descriptorSet)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grEndDescriptorSetUpdate(
    GR_DESCRIPTOR_SET descriptorSet)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grAttachSamplerDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_SAMPLER* pSamplers)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grAttachImageViewDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_IMAGE_VIEW_ATTACH_INFO* pImageViews)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grAttachMemoryViewDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_MEMORY_VIEW_ATTACH_INFO* pMemViews)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grAttachNestedDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_DESCRIPTOR_SET_ATTACH_INFO* pNestedDescriptorSets)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grClearDescriptorSetSlots(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount)
{
    printf("STUB: %s\n", __func__);
}

// Query and Synchronization Function

GR_RESULT grCreateQueryPool(
    GR_DEVICE device,
    const GR_QUERY_POOL_CREATE_INFO* pCreateInfo,
    GR_QUERY_POOL* pQueryPool)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grGetQueryPoolResults(
    GR_QUERY_POOL queryPool,
    GR_UINT startQuery,
    GR_UINT queryCount,
    GR_SIZE* pDataSize,
    GR_VOID* pData)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grCreateFence(
    GR_DEVICE device,
    const GR_FENCE_CREATE_INFO* pCreateInfo,
    GR_FENCE* pFence)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grGetFenceStatus(
    GR_FENCE fence)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grWaitForFences(
    GR_DEVICE device,
    GR_UINT fenceCount,
    const GR_FENCE* pFences,
    GR_BOOL waitAll,
    GR_FLOAT timeout)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grCreateQueueSemaphore(
    GR_DEVICE device,
    const GR_QUEUE_SEMAPHORE_CREATE_INFO* pCreateInfo,
    GR_QUEUE_SEMAPHORE* pSemaphore)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grSignalQueueSemaphore(
    GR_QUEUE queue,
    GR_QUEUE_SEMAPHORE semaphore)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grWaitQueueSemaphore(
    GR_QUEUE queue,
    GR_QUEUE_SEMAPHORE semaphore)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grCreateEvent(
    GR_DEVICE device,
    const GR_EVENT_CREATE_INFO* pCreateInfo,
    GR_EVENT* pEvent)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grGetEventStatus(
    GR_EVENT event)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grSetEvent(
    GR_EVENT event)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grResetEvent(
    GR_EVENT event)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

// Multi-Device Management Functions

GR_RESULT grGetMultiGpuCompatibility(
    GR_PHYSICAL_GPU gpu0,
    GR_PHYSICAL_GPU gpu1,
    GR_GPU_COMPATIBILITY_INFO* pInfo)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grOpenSharedMemory(
    GR_DEVICE device,
    const GR_MEMORY_OPEN_INFO* pOpenInfo,
    GR_GPU_MEMORY* pMem)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grOpenSharedQueueSemaphore(
    GR_DEVICE device,
    const GR_QUEUE_SEMAPHORE_OPEN_INFO* pOpenInfo,
    GR_QUEUE_SEMAPHORE* pSemaphore)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grOpenPeerMemory(
    GR_DEVICE device,
    const GR_PEER_MEMORY_OPEN_INFO* pOpenInfo,
    GR_GPU_MEMORY* pMem)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grOpenPeerImage(
    GR_DEVICE device,
    const GR_PEER_IMAGE_OPEN_INFO* pOpenInfo,
    GR_IMAGE* pImage,
    GR_GPU_MEMORY* pMem)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

// Command Buffer Management Functions

GR_RESULT grResetCommandBuffer(
    GR_CMD_BUFFER cmdBuffer)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

// Command Buffer Building Functions

GR_VOID grCmdBindPipeline(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_PIPELINE pipeline)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdBindStateObject(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM stateBindPoint,
    GR_STATE_OBJECT state)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdBindDescriptorSet(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_UINT index,
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT slotOffset)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdBindDynamicMemoryView(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    const GR_MEMORY_VIEW_ATTACH_INFO* pMemView)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdBindIndexData(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset,
    GR_ENUM indexType)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdBindTargets(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT colorTargetCount,
    const GR_COLOR_TARGET_BIND_INFO* pColorTargets,
    const GR_DEPTH_STENCIL_BIND_INFO* pDepthTarget)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdDraw(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT firstVertex,
    GR_UINT vertexCount,
    GR_UINT firstInstance,
    GR_UINT instanceCount)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdDrawIndexed(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT firstIndex,
    GR_UINT indexCount,
    GR_INT vertexOffset,
    GR_UINT firstInstance,
    GR_UINT instanceCount)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdDrawIndirect(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdDrawIndexedIndirect(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdDispatch(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT x,
    GR_UINT y,
    GR_UINT z)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdDispatchIndirect(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdCopyMemory(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY srcMem,
    GR_GPU_MEMORY destMem,
    GR_UINT regionCount,
    const GR_MEMORY_COPY* pRegions)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdCopyImage(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE srcImage,
    GR_IMAGE destImage,
    GR_UINT regionCount,
    const GR_IMAGE_COPY* pRegions)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdCopyMemoryToImage(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY srcMem,
    GR_IMAGE destImage,
    GR_UINT regionCount,
    const GR_MEMORY_IMAGE_COPY* pRegions)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdCopyImageToMemory(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE srcImage,
    GR_GPU_MEMORY destMem,
    GR_UINT regionCount,
    const GR_MEMORY_IMAGE_COPY* pRegions)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdResolveImage(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE srcImage,
    GR_IMAGE destImage,
    GR_UINT regionCount,
    const GR_IMAGE_RESOLVE* pRegions)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdCloneImageData(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE srcImage,
    GR_ENUM srcImageState,
    GR_IMAGE destImage,
    GR_ENUM destImageState)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdUpdateMemory(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY destMem,
    GR_GPU_SIZE destOffset,
    GR_GPU_SIZE dataSize,
    const GR_UINT32* pData)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdFillMemory(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY destMem,
    GR_GPU_SIZE destOffset,
    GR_GPU_SIZE fillSize,
    GR_UINT32 data)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdClearColorImage(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE image,
    const GR_FLOAT color[4],
    GR_UINT rangeCount,
    const GR_IMAGE_SUBRESOURCE_RANGE* pRanges)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdClearColorImageRaw(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE image,
    const GR_UINT32 color[4],
    GR_UINT rangeCount,
    const GR_IMAGE_SUBRESOURCE_RANGE* pRanges)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdClearDepthStencil(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE image,
    GR_FLOAT depth,
    GR_UINT8 stencil,
    GR_UINT rangeCount,
    const GR_IMAGE_SUBRESOURCE_RANGE* pRanges)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdSetEvent(
    GR_CMD_BUFFER cmdBuffer,
    GR_EVENT event)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdResetEvent(
    GR_CMD_BUFFER cmdBuffer,
    GR_EVENT event)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdMemoryAtomic(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY destMem,
    GR_GPU_SIZE destOffset,
    GR_UINT64 srcData,
    GR_ENUM atomicOp)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdBeginQuery(
    GR_CMD_BUFFER cmdBuffer,
    GR_QUERY_POOL queryPool,
    GR_UINT slot,
    GR_FLAGS flags)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdEndQuery(
    GR_CMD_BUFFER cmdBuffer,
    GR_QUERY_POOL queryPool,
    GR_UINT slot)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdResetQueryPool(
    GR_CMD_BUFFER cmdBuffer,
    GR_QUERY_POOL queryPool,
    GR_UINT startQuery,
    GR_UINT queryCount)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdWriteTimestamp(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM timestampType,
    GR_GPU_MEMORY destMem,
    GR_GPU_SIZE destOffset)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdInitAtomicCounters(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_UINT startCounter,
    GR_UINT counterCount,
    const GR_UINT32* pData)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdLoadAtomicCounters(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_UINT startCounter,
    GR_UINT counterCount,
    GR_GPU_MEMORY srcMem,
    GR_GPU_SIZE srcOffset)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID grCmdSaveAtomicCounters(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_UINT startCounter,
    GR_UINT counterCount,
    GR_GPU_MEMORY destMem,
    GR_GPU_SIZE destOffset)
{
    printf("STUB: %s\n", __func__);
}

// Debug Functions

GR_RESULT GR_STDCALL grDbgSetValidationLevel(
    GR_DEVICE device,
    GR_ENUM validationLevel)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grDbgRegisterMsgCallback(
    GR_DBG_MSG_CALLBACK_FUNCTION pfnMsgCallback,
    GR_VOID* pUserData)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grDbgUnregisterMsgCallback(
    GR_DBG_MSG_CALLBACK_FUNCTION pfnMsgCallback)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grDbgSetMessageFilter(
    GR_DEVICE device,
    GR_ENUM msgCode,
    GR_ENUM filter)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grDbgSetObjectTag(
    GR_BASE_OBJECT object,
    GR_SIZE tagSize,
    const GR_VOID* pTag)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grDbgSetGlobalOption(
    GR_DBG_GLOBAL_OPTION dbgOption,
    GR_SIZE dataSize,
    const GR_VOID* pData)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grDbgSetDeviceOption(
    GR_DEVICE device,
    GR_DBG_DEVICE_OPTION dbgOption,
    GR_SIZE dataSize,
    const GR_VOID* pData)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_VOID GR_STDCALL grCmdDbgMarkerBegin(
    GR_CMD_BUFFER cmdBuffer,
    const GR_CHAR* pMarker)
{
    printf("STUB: %s\n", __func__);
}

GR_VOID GR_STDCALL grCmdDbgMarkerEnd(
    GR_CMD_BUFFER cmdBuffer)
{
    printf("STUB: %s\n", __func__);
}

// WSI Functions

GR_RESULT grWsiWinGetDisplays(
    GR_DEVICE device,
    GR_UINT* pDisplayCount,
    GR_WSI_WIN_DISPLAY* pDisplayList)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grWsiWinGetDisplayModeList(
    GR_WSI_WIN_DISPLAY display,
    GR_UINT* pDisplayModeCount,
    GR_WSI_WIN_DISPLAY_MODE* pDisplayModeList)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grWsiWinTakeFullscreenOwnership(
    GR_WSI_WIN_DISPLAY display,
    GR_IMAGE image)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grWsiWinReleaseFullscreenOwnership(
    GR_WSI_WIN_DISPLAY display)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grWsiWinSetGammaRamp(
    GR_WSI_WIN_DISPLAY display,
    const GR_WSI_WIN_GAMMA_RAMP* pGammaRamp)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grWsiWinWaitForVerticalBlank(
    GR_WSI_WIN_DISPLAY display)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grWsiWinGetScanLine(
    GR_WSI_WIN_DISPLAY display,
    GR_INT* pScanLine)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grWsiWinQueuePresent(
    GR_QUEUE queue,
    const GR_WSI_WIN_PRESENT_INFO* pPresentInfo)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}

GR_RESULT grWsiWinSetMaxQueuedFrames(
    GR_DEVICE device,
    GR_UINT maxFrames)
{
    printf("STUB: %s\n", __func__);
    return GR_UNSUPPORTED;
}
