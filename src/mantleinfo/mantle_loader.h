#ifndef MANTLE_LOADER_H_
#define MANTLE_LOADER_H_

#include <stdbool.h>
#include "mantle/mantle.h"

// Initialization and Device Functions

typedef GR_RESULT (GR_STDCALL *_grInitAndEnumerateGpus)(
    const GR_APPLICATION_INFO* pAppInfo,
    const GR_ALLOC_CALLBACKS* pAllocCb,
    GR_UINT* pGpuCount,
    GR_PHYSICAL_GPU gpus[GR_MAX_PHYSICAL_GPUS]);

typedef GR_RESULT (GR_STDCALL *_grGetGpuInfo)(
    GR_PHYSICAL_GPU gpu,
    GR_ENUM infoType,
    GR_SIZE* pDataSize,
    GR_VOID* pData);

typedef GR_RESULT (GR_STDCALL *_grCreateDevice)(
    GR_PHYSICAL_GPU gpu,
    const GR_DEVICE_CREATE_INFO* pCreateInfo,
    GR_DEVICE* pDevice);

typedef GR_RESULT (GR_STDCALL *_grDestroyDevice)(
    GR_DEVICE device);

// Extension Discovery Functions

typedef GR_RESULT (GR_STDCALL *_grGetExtensionSupport)(
    GR_PHYSICAL_GPU gpu,
    const GR_CHAR* pExtName);

// Queue Functions

typedef GR_RESULT (GR_STDCALL *_grGetDeviceQueue)(
    GR_DEVICE device,
    GR_ENUM queueType,
    GR_UINT queueId,
    GR_QUEUE* pQueue);

typedef GR_RESULT (GR_STDCALL *_grQueueWaitIdle)(
    GR_QUEUE queue);

typedef GR_RESULT (GR_STDCALL *_grDeviceWaitIdle)(
    GR_DEVICE device);

typedef GR_RESULT (GR_STDCALL *_grQueueSubmit)(
    GR_QUEUE queue,
    GR_UINT cmdBufferCount,
    const GR_CMD_BUFFER* pCmdBuffers,
    GR_UINT memRefCount,
    const GR_MEMORY_REF* pMemRefs,
    GR_FENCE fence);

typedef GR_RESULT (GR_STDCALL *_grQueueSetGlobalMemReferences)(
    GR_QUEUE queue,
    GR_UINT memRefCount,
    const GR_MEMORY_REF* pMemRefs);

// Memory Management Functions

typedef GR_RESULT (GR_STDCALL *_grGetMemoryHeapCount)(
    GR_DEVICE device,
    GR_UINT* pCount);

typedef GR_RESULT (GR_STDCALL *_grGetMemoryHeapInfo)(
    GR_DEVICE device,
    GR_UINT heapId,
    GR_ENUM infoType,
    GR_SIZE* pDataSize,
    GR_VOID* pData);

typedef GR_RESULT (GR_STDCALL *_grAllocMemory)(
    GR_DEVICE device,
    const GR_MEMORY_ALLOC_INFO* pAllocInfo,
    GR_GPU_MEMORY* pMem);

typedef GR_RESULT (GR_STDCALL *_grFreeMemory)(
    GR_GPU_MEMORY mem);

typedef GR_RESULT (GR_STDCALL *_grSetMemoryPriority)(
    GR_GPU_MEMORY mem,
    GR_ENUM priority);

typedef GR_RESULT (GR_STDCALL *_grMapMemory)(
    GR_GPU_MEMORY mem,
    GR_FLAGS flags,
    GR_VOID** ppData);

typedef GR_RESULT (GR_STDCALL *_grUnmapMemory)(
    GR_GPU_MEMORY mem);

typedef GR_RESULT (GR_STDCALL *_grRemapVirtualMemoryPages)(
    GR_DEVICE device,
    GR_UINT rangeCount,
    const GR_VIRTUAL_MEMORY_REMAP_RANGE* pRanges,
    GR_UINT preWaitSemaphoreCount,
    const GR_QUEUE_SEMAPHORE* pPreWaitSemaphores,
    GR_UINT postSignalSemaphoreCount,
    const GR_QUEUE_SEMAPHORE* pPostSignalSemaphores);

typedef GR_RESULT (GR_STDCALL *_grPinSystemMemory)(
    GR_DEVICE device,
    const GR_VOID* pSysMem,
    GR_SIZE memSize,
    GR_GPU_MEMORY* pMem);

// Generic API Object Management functions

typedef GR_RESULT (GR_STDCALL *_grDestroyObject)(
    GR_OBJECT object);

typedef GR_RESULT (GR_STDCALL *_grGetObjectInfo)(
    GR_BASE_OBJECT object,
    GR_ENUM infoType,
    GR_SIZE* pDataSize,
    GR_VOID* pData);

typedef GR_RESULT (GR_STDCALL *_grBindObjectMemory)(
    GR_OBJECT object,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset);

// Image and Sample Functions

typedef GR_RESULT (GR_STDCALL *_grGetFormatInfo)(
    GR_DEVICE device,
    GR_FORMAT format,
    GR_ENUM infoType,
    GR_SIZE* pDataSize,
    GR_VOID* pData);

typedef GR_RESULT (GR_STDCALL *_grCreateImage)(
    GR_DEVICE device,
    const GR_IMAGE_CREATE_INFO* pCreateInfo,
    GR_IMAGE* pImage);

typedef GR_RESULT (GR_STDCALL *_grGetImageSubresourceInfo)(
    GR_IMAGE image,
    const GR_IMAGE_SUBRESOURCE* pSubresource,
    GR_ENUM infoType,
    GR_SIZE* pDataSize,
    GR_VOID* pData);

typedef GR_RESULT (GR_STDCALL *_grCreateSampler)(
    GR_DEVICE device,
    const GR_SAMPLER_CREATE_INFO* pCreateInfo,
    GR_SAMPLER* pSampler);

// Image View Functions

typedef GR_RESULT (GR_STDCALL *_grCreateImageView)(
    GR_DEVICE device,
    const GR_IMAGE_VIEW_CREATE_INFO* pCreateInfo,
    GR_IMAGE_VIEW* pView);

typedef GR_RESULT (GR_STDCALL *_grCreateColorTargetView)(
    GR_DEVICE device,
    const GR_COLOR_TARGET_VIEW_CREATE_INFO* pCreateInfo,
    GR_COLOR_TARGET_VIEW* pView);

typedef GR_RESULT (GR_STDCALL *_grCreateDepthStencilView)(
    GR_DEVICE device,
    const GR_DEPTH_STENCIL_VIEW_CREATE_INFO* pCreateInfo,
    GR_DEPTH_STENCIL_VIEW* pView);

// Shader and Pipeline Functions

typedef GR_RESULT (GR_STDCALL *_grCreateShader)(
    GR_DEVICE device,
    const GR_SHADER_CREATE_INFO* pCreateInfo,
    GR_SHADER* pShader);

typedef GR_RESULT (GR_STDCALL *_grCreateGraphicsPipeline)(
    GR_DEVICE device,
    const GR_GRAPHICS_PIPELINE_CREATE_INFO* pCreateInfo,
    GR_PIPELINE* pPipeline);

typedef GR_RESULT (GR_STDCALL *_grCreateComputePipeline)(
    GR_DEVICE device,
    const GR_COMPUTE_PIPELINE_CREATE_INFO* pCreateInfo,
    GR_PIPELINE* pPipeline);

typedef GR_RESULT (GR_STDCALL *_grStorePipeline)(
    GR_PIPELINE pipeline,
    GR_SIZE* pDataSize,
    GR_VOID* pData);

typedef GR_RESULT (GR_STDCALL *_grLoadPipeline)(
    GR_DEVICE device,
    GR_SIZE dataSize,
    const GR_VOID* pData,
    GR_PIPELINE* pPipeline);

// Descriptor Set Functions

typedef GR_RESULT (GR_STDCALL *_grCreateDescriptorSet)(
    GR_DEVICE device,
    const GR_DESCRIPTOR_SET_CREATE_INFO* pCreateInfo,
    GR_DESCRIPTOR_SET* pDescriptorSet);

typedef GR_VOID (GR_STDCALL *_grBeginDescriptorSetUpdate)(
    GR_DESCRIPTOR_SET descriptorSet);

typedef GR_VOID (GR_STDCALL *_grEndDescriptorSetUpdate)(
    GR_DESCRIPTOR_SET descriptorSet);

typedef GR_VOID (GR_STDCALL *_grAttachSamplerDescriptors)(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_SAMPLER* pSamplers);

typedef GR_VOID (GR_STDCALL *_grAttachImageViewDescriptors)(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_IMAGE_VIEW_ATTACH_INFO* pImageViews);

typedef GR_VOID (GR_STDCALL *_grAttachMemoryViewDescriptors)(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_MEMORY_VIEW_ATTACH_INFO* pMemViews);

typedef GR_VOID (GR_STDCALL *_grAttachNestedDescriptors)(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_DESCRIPTOR_SET_ATTACH_INFO* pNestedDescriptorSets);

typedef GR_VOID (GR_STDCALL *_grClearDescriptorSetSlots)(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount);

// State Object Functions

typedef GR_RESULT (GR_STDCALL *_grCreateViewportState)(
    GR_DEVICE device,
    const GR_VIEWPORT_STATE_CREATE_INFO* pCreateInfo,
    GR_VIEWPORT_STATE_OBJECT* pState);

typedef GR_RESULT (GR_STDCALL *_grCreateRasterState)(
    GR_DEVICE device,
    const GR_RASTER_STATE_CREATE_INFO* pCreateInfo,
    GR_RASTER_STATE_OBJECT* pState);

typedef GR_RESULT (GR_STDCALL *_grCreateColorBlendState)(
    GR_DEVICE device,
    const GR_COLOR_BLEND_STATE_CREATE_INFO* pCreateInfo,
    GR_COLOR_BLEND_STATE_OBJECT* pState);

typedef GR_RESULT (GR_STDCALL *_grCreateDepthStencilState)(
    GR_DEVICE device,
    const GR_DEPTH_STENCIL_STATE_CREATE_INFO* pCreateInfo,
    GR_DEPTH_STENCIL_STATE_OBJECT* pState);

typedef GR_RESULT (GR_STDCALL *_grCreateMsaaState)(
    GR_DEVICE device,
    const GR_MSAA_STATE_CREATE_INFO* pCreateInfo,
    GR_MSAA_STATE_OBJECT* pState);

// Query and Synchronization Functions

typedef GR_RESULT (GR_STDCALL *_grCreateQueryPool)(
    GR_DEVICE device,
    const GR_QUERY_POOL_CREATE_INFO* pCreateInfo,
    GR_QUERY_POOL* pQueryPool);

typedef GR_RESULT (GR_STDCALL *_grGetQueryPoolResults)(
    GR_QUERY_POOL queryPool,
    GR_UINT startQuery,
    GR_UINT queryCount,
    GR_SIZE* pDataSize,
    GR_VOID* pData);

typedef GR_RESULT (GR_STDCALL *_grCreateFence)(
    GR_DEVICE device,
    const GR_FENCE_CREATE_INFO* pCreateInfo,
    GR_FENCE* pFence);

typedef GR_RESULT (GR_STDCALL *_grGetFenceStatus)(
    GR_FENCE fence);

typedef GR_RESULT (GR_STDCALL *_grWaitForFences)(
    GR_DEVICE device,
    GR_UINT fenceCount,
    const GR_FENCE* pFences,
    GR_BOOL waitAll,
    GR_FLOAT timeout);

typedef GR_RESULT (GR_STDCALL *_grCreateQueueSemaphore)(
    GR_DEVICE device,
    const GR_QUEUE_SEMAPHORE_CREATE_INFO* pCreateInfo,
    GR_QUEUE_SEMAPHORE* pSemaphore);

typedef GR_RESULT (GR_STDCALL *_grSignalQueueSemaphore)(
    GR_QUEUE queue,
    GR_QUEUE_SEMAPHORE semaphore);

typedef GR_RESULT (GR_STDCALL *_grWaitQueueSemaphore)(
    GR_QUEUE queue,
    GR_QUEUE_SEMAPHORE semaphore);

typedef GR_RESULT (GR_STDCALL *_grCreateEvent)(
    GR_DEVICE device,
    const GR_EVENT_CREATE_INFO* pCreateInfo,
    GR_EVENT* pEvent);

typedef GR_RESULT (GR_STDCALL *_grGetEventStatus)(
    GR_EVENT event);

typedef GR_RESULT (GR_STDCALL *_grSetEvent)(
    GR_EVENT event);

typedef GR_RESULT (GR_STDCALL *_grResetEvent)(
    GR_EVENT event);

// Multi-Device Management Functions

typedef GR_RESULT (GR_STDCALL *_grGetMultiGpuCompatibility)(
    GR_PHYSICAL_GPU gpu0,
    GR_PHYSICAL_GPU gpu1,
    GR_GPU_COMPATIBILITY_INFO* pInfo);

typedef GR_RESULT (GR_STDCALL *_grOpenSharedMemory)(
    GR_DEVICE device,
    const GR_MEMORY_OPEN_INFO* pOpenInfo,
    GR_GPU_MEMORY* pMem);

typedef GR_RESULT (GR_STDCALL *_grOpenSharedQueueSemaphore)(
    GR_DEVICE device,
    const GR_QUEUE_SEMAPHORE_OPEN_INFO* pOpenInfo,
    GR_QUEUE_SEMAPHORE* pSemaphore);

typedef GR_RESULT (GR_STDCALL *_grOpenPeerMemory)(
    GR_DEVICE device,
    const GR_PEER_MEMORY_OPEN_INFO* pOpenInfo,
    GR_GPU_MEMORY* pMem);

typedef GR_RESULT (GR_STDCALL *_grOpenPeerImage)(
    GR_DEVICE device,
    const GR_PEER_IMAGE_OPEN_INFO* pOpenInfo,
    GR_IMAGE* pImage,
    GR_GPU_MEMORY* pMem);

// Command Buffer Management Functions

typedef GR_RESULT (GR_STDCALL *_grCreateCommandBuffer)(
    GR_DEVICE device,
    const GR_CMD_BUFFER_CREATE_INFO* pCreateInfo,
    GR_CMD_BUFFER* pCmdBuffer);

typedef GR_RESULT (GR_STDCALL *_grBeginCommandBuffer)(
    GR_CMD_BUFFER cmdBuffer,
    GR_FLAGS flags);

typedef GR_RESULT (GR_STDCALL *_grEndCommandBuffer)(
    GR_CMD_BUFFER cmdBuffer);

typedef GR_RESULT (GR_STDCALL *_grResetCommandBuffer)(
    GR_CMD_BUFFER cmdBuffer);

// Command Buffer Building Functions

typedef GR_VOID (GR_STDCALL *_grCmdBindPipeline)(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_PIPELINE pipeline);

typedef GR_VOID (GR_STDCALL *_grCmdBindStateObject)(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM stateBindPoint,
    GR_STATE_OBJECT state);

typedef GR_VOID (GR_STDCALL *_grCmdBindDescriptorSet)(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_UINT index,
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT slotOffset);

typedef GR_VOID (GR_STDCALL *_grCmdBindDynamicMemoryView)(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    const GR_MEMORY_VIEW_ATTACH_INFO* pMemView);

typedef GR_VOID (GR_STDCALL *_grCmdBindIndexData)(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset,
    GR_ENUM indexType);

typedef GR_VOID (GR_STDCALL *_grCmdBindTargets)(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT colorTargetCount,
    const GR_COLOR_TARGET_BIND_INFO* pColorTargets,
    const GR_DEPTH_STENCIL_BIND_INFO* pDepthTarget);

typedef GR_VOID (GR_STDCALL *_grCmdPrepareMemoryRegions)(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT transitionCount,
    const GR_MEMORY_STATE_TRANSITION* pStateTransitions);

typedef GR_VOID (GR_STDCALL *_grCmdPrepareImages)(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT transitionCount,
    const GR_IMAGE_STATE_TRANSITION* pStateTransitions);

typedef GR_VOID (GR_STDCALL *_grCmdDraw)(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT firstVertex,
    GR_UINT vertexCount,
    GR_UINT firstInstance,
    GR_UINT instanceCount);

typedef GR_VOID (GR_STDCALL *_grCmdDrawIndexed)(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT firstIndex,
    GR_UINT indexCount,
    GR_INT vertexOffset,
    GR_UINT firstInstance,
    GR_UINT instanceCount);

typedef GR_VOID (GR_STDCALL *_grCmdDrawIndirect)(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset);

typedef GR_VOID (GR_STDCALL *_grCmdDrawIndexedIndirect)(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset);

typedef GR_VOID (GR_STDCALL *_grCmdDispatch)(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT x,
    GR_UINT y,
    GR_UINT z);

typedef GR_VOID (GR_STDCALL *_grCmdDispatchIndirect)(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset);

typedef GR_VOID (GR_STDCALL *_grCmdCopyMemory)(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY srcMem,
    GR_GPU_MEMORY destMem,
    GR_UINT regionCount,
    const GR_MEMORY_COPY* pRegions);

typedef GR_VOID (GR_STDCALL *_grCmdCopyImage)(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE srcImage,
    GR_IMAGE destImage,
    GR_UINT regionCount,
    const GR_IMAGE_COPY* pRegions);

typedef GR_VOID (GR_STDCALL *_grCmdCopyMemoryToImage)(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY srcMem,
    GR_IMAGE destImage,
    GR_UINT regionCount,
    const GR_MEMORY_IMAGE_COPY* pRegions);

typedef GR_VOID (GR_STDCALL *_grCmdCopyImageToMemory)(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE srcImage,
    GR_GPU_MEMORY destMem,
    GR_UINT regionCount,
    const GR_MEMORY_IMAGE_COPY* pRegions);

typedef GR_VOID (GR_STDCALL *_grCmdResolveImage)(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE srcImage,
    GR_IMAGE destImage,
    GR_UINT regionCount,
    const GR_IMAGE_RESOLVE* pRegions);

typedef GR_VOID (GR_STDCALL *_grCmdCloneImageData)(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE srcImage,
    GR_ENUM srcImageState,
    GR_IMAGE destImage,
    GR_ENUM destImageState);

typedef GR_VOID (GR_STDCALL *_grCmdUpdateMemory)(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY destMem,
    GR_GPU_SIZE destOffset,
    GR_GPU_SIZE dataSize,
    const GR_UINT32* pData);

typedef GR_VOID (GR_STDCALL *_grCmdFillMemory)(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY destMem,
    GR_GPU_SIZE destOffset,
    GR_GPU_SIZE fillSize,
    GR_UINT32 data);

typedef GR_VOID (GR_STDCALL *_grCmdClearColorImage)(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE image,
    const GR_FLOAT color[4],
    GR_UINT rangeCount,
    const GR_IMAGE_SUBRESOURCE_RANGE* pRanges);

typedef GR_VOID (GR_STDCALL *_grCmdClearColorImageRaw)(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE image,
    const GR_UINT32 color[4],
    GR_UINT rangeCount,
    const GR_IMAGE_SUBRESOURCE_RANGE* pRanges);

typedef GR_VOID (GR_STDCALL *_grCmdClearDepthStencil)(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE image,
    GR_FLOAT depth,
    GR_UINT8 stencil,
    GR_UINT rangeCount,
    const GR_IMAGE_SUBRESOURCE_RANGE* pRanges);

typedef GR_VOID (GR_STDCALL *_grCmdSetEvent)(
    GR_CMD_BUFFER cmdBuffer,
    GR_EVENT event);

typedef GR_VOID (GR_STDCALL *_grCmdResetEvent)(
    GR_CMD_BUFFER cmdBuffer,
    GR_EVENT event);

typedef GR_VOID (GR_STDCALL *_grCmdMemoryAtomic)(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY destMem,
    GR_GPU_SIZE destOffset,
    GR_UINT64 srcData,
    GR_ENUM atomicOp);

typedef GR_VOID (GR_STDCALL *_grCmdBeginQuery)(
    GR_CMD_BUFFER cmdBuffer,
    GR_QUERY_POOL queryPool,
    GR_UINT slot,
    GR_FLAGS flags);

typedef GR_VOID (GR_STDCALL *_grCmdEndQuery)(
    GR_CMD_BUFFER cmdBuffer,
    GR_QUERY_POOL queryPool,
    GR_UINT slot);

typedef GR_VOID (GR_STDCALL *_grCmdResetQueryPool)(
    GR_CMD_BUFFER cmdBuffer,
    GR_QUERY_POOL queryPool,
    GR_UINT startQuery,
    GR_UINT queryCount);

typedef GR_VOID (GR_STDCALL *_grCmdWriteTimestamp)(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM timestampType,
    GR_GPU_MEMORY destMem,
    GR_GPU_SIZE destOffset);

typedef GR_VOID (GR_STDCALL *_grCmdInitAtomicCounters)(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_UINT startCounter,
    GR_UINT counterCount,
    const GR_UINT32* pData);

typedef GR_VOID (GR_STDCALL *_grCmdLoadAtomicCounters)(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_UINT startCounter,
    GR_UINT counterCount,
    GR_GPU_MEMORY srcMem,
    GR_GPU_SIZE srcOffset);

typedef GR_VOID (GR_STDCALL *_grCmdSaveAtomicCounters)(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_UINT startCounter,
    GR_UINT counterCount,
    GR_GPU_MEMORY destMem,
    GR_GPU_SIZE destOffset);

typedef struct _GrInstance {
    _grInitAndEnumerateGpus grInitAndEnumerateGpus;
    _grGetGpuInfo grGetGpuInfo;
    _grCreateDevice grCreateDevice;
    _grDestroyDevice grDestroyDevice;

    _grGetExtensionSupport grGetExtensionSupport;

    _grGetDeviceQueue grGetDeviceQueue;
    _grQueueWaitIdle grQueueWaitIdle;
    _grDeviceWaitIdle grDeviceWaitIdle;
    _grQueueSubmit grQueueSubmit;
    _grQueueSetGlobalMemReferences grQueueSetGlobalMemReferences;

    _grGetMemoryHeapCount grGetMemoryHeapCount;
    _grGetMemoryHeapInfo grGetMemoryHeapInfo;
    _grAllocMemory grAllocMemory;
    _grFreeMemory grFreeMemory;
    _grSetMemoryPriority grSetMemoryPriority;
    _grMapMemory grMapMemory;
    _grUnmapMemory grUnmapMemory;
    _grRemapVirtualMemoryPages grRemapVirtualMemoryPages;
    _grPinSystemMemory grPinSystemMemory;

    _grDestroyObject grDestroyObject;
    _grGetObjectInfo grGetObjectInfo;
    _grBindObjectMemory grBindObjectMemory;

    _grGetFormatInfo grGetFormatInfo;
    _grCreateImage grCreateImage;
    _grGetImageSubresourceInfo grGetImageSubresourceInfo;
    _grCreateSampler grCreateSampler;

    _grCreateImageView grCreateImageView;
    _grCreateColorTargetView grCreateColorTargetView;
    _grCreateDepthStencilView grCreateDepthStencilView;

    _grCreateShader grCreateShader;
    _grCreateGraphicsPipeline grCreateGraphicsPipeline;
    _grCreateComputePipeline grCreateComputePipeline;
    _grStorePipeline grStorePipeline;
    _grLoadPipeline grLoadPipeline;

    _grCreateDescriptorSet grCreateDescriptorSet;
    _grBeginDescriptorSetUpdate grBeginDescriptorSetUpdate;
    _grEndDescriptorSetUpdate grEndDescriptorSetUpdate;
    _grAttachSamplerDescriptors grAttachSamplerDescriptors;
    _grAttachImageViewDescriptors grAttachImageViewDescriptors;
    _grAttachMemoryViewDescriptors grAttachMemoryViewDescriptors;
    _grAttachNestedDescriptors grAttachNestedDescriptors;
    _grClearDescriptorSetSlots grClearDescriptorSetSlots;

    _grCreateViewportState grCreateViewportState;
    _grCreateRasterState grCreateRasterState;
    _grCreateColorBlendState grCreateColorBlendState;
    _grCreateDepthStencilState grCreateDepthStencilState;
    _grCreateMsaaState grCreateMsaaState;

    _grCreateQueryPool grCreateQueryPool;
    _grGetQueryPoolResults grGetQueryPoolResults;
    _grCreateFence grCreateFence;
    _grGetFenceStatus grGetFenceStatus;
    _grWaitForFences grWaitForFences;
    _grCreateQueueSemaphore grCreateQueueSemaphore;
    _grSignalQueueSemaphore grSignalQueueSemaphore;
    _grWaitQueueSemaphore grWaitQueueSemaphore;
    _grCreateEvent grCreateEvent;
    _grGetEventStatus grGetEventStatus;
    _grSetEvent grSetEvent;
    _grResetEvent grResetEvent;

    _grGetMultiGpuCompatibility grGetMultiGpuCompatibility;
    _grOpenSharedMemory grOpenSharedMemory;
    _grOpenSharedQueueSemaphore grOpenSharedQueueSemaphore;
    _grOpenPeerMemory grOpenPeerMemory;
    _grOpenPeerImage grOpenPeerImage;

    _grCreateCommandBuffer grCreateCommandBuffer;
    _grBeginCommandBuffer grBeginCommandBuffer;
    _grEndCommandBuffer grEndCommandBuffer;
    _grResetCommandBuffer grResetCommandBuffer;

    _grCmdBindPipeline grCmdBindPipeline;
    _grCmdBindStateObject grCmdBindStateObject;
    _grCmdBindDescriptorSet grCmdBindDescriptorSet;
    _grCmdBindDynamicMemoryView grCmdBindDynamicMemoryView;
    _grCmdBindIndexData grCmdBindIndexData;
    _grCmdBindTargets grCmdBindTargets;
    _grCmdPrepareMemoryRegions grCmdPrepareMemoryRegions;
    _grCmdPrepareImages grCmdPrepareImages;
    _grCmdDraw grCmdDraw;
    _grCmdDrawIndexed grCmdDrawIndexed;
    _grCmdDrawIndirect grCmdDrawIndirect;
    _grCmdDrawIndexedIndirect grCmdDrawIndexedIndirect;
    _grCmdDispatch grCmdDispatch;
    _grCmdDispatchIndirect grCmdDispatchIndirect;
    _grCmdCopyMemory grCmdCopyMemory;
    _grCmdCopyImage grCmdCopyImage;
    _grCmdCopyMemoryToImage grCmdCopyMemoryToImage;
    _grCmdCopyImageToMemory grCmdCopyImageToMemory;
    _grCmdResolveImage grCmdResolveImage;
    _grCmdCloneImageData grCmdCloneImageData;
    _grCmdUpdateMemory grCmdUpdateMemory;
    _grCmdFillMemory grCmdFillMemory;
    _grCmdClearColorImage grCmdClearColorImage;
    _grCmdClearColorImageRaw grCmdClearColorImageRaw;
    _grCmdClearDepthStencil grCmdClearDepthStencil;
    _grCmdSetEvent grCmdSetEvent;
    _grCmdResetEvent grCmdResetEvent;
    _grCmdMemoryAtomic grCmdMemoryAtomic;
    _grCmdBeginQuery grCmdBeginQuery;
    _grCmdEndQuery grCmdEndQuery;
    _grCmdResetQueryPool grCmdResetQueryPool;
    _grCmdWriteTimestamp grCmdWriteTimestamp;
    _grCmdInitAtomicCounters grCmdInitAtomicCounters;
    _grCmdLoadAtomicCounters grCmdLoadAtomicCounters;
    _grCmdSaveAtomicCounters grCmdSaveAtomicCounters;
} GrInstance;

extern GrInstance gri;

bool mantleLoaderInit();

#endif // MANTLE_LOADER_H_
