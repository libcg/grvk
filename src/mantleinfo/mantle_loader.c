#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "mantle_loader.h"

#define LOAD_PROC(module, var) \
{ \
    gri.var = (void*)GetProcAddress(module, #var); \
    if (gri.var == NULL) return false; \
}

GrInstance gri;

bool mantleLoaderInit() {
    HMODULE module = LoadLibrary(TEXT("mantle64.dll"));

    if (module == NULL) {
        return false;
    }

    LOAD_PROC(module, grInitAndEnumerateGpus);
    LOAD_PROC(module, grGetGpuInfo);
    LOAD_PROC(module, grCreateDevice);
    LOAD_PROC(module, grDestroyDevice);

    LOAD_PROC(module, grGetExtensionSupport);

    LOAD_PROC(module, grGetDeviceQueue);
    LOAD_PROC(module, grQueueWaitIdle);
    LOAD_PROC(module, grDeviceWaitIdle);
    LOAD_PROC(module, grQueueSubmit);
    LOAD_PROC(module, grQueueSetGlobalMemReferences);

    LOAD_PROC(module, grGetMemoryHeapCount);
    LOAD_PROC(module, grGetMemoryHeapInfo);
    LOAD_PROC(module, grAllocMemory);
    LOAD_PROC(module, grFreeMemory);
    LOAD_PROC(module, grSetMemoryPriority);
    LOAD_PROC(module, grMapMemory);
    LOAD_PROC(module, grUnmapMemory);
    LOAD_PROC(module, grRemapVirtualMemoryPages);
    LOAD_PROC(module, grPinSystemMemory);

    LOAD_PROC(module, grDestroyObject);
    LOAD_PROC(module, grGetObjectInfo);
    LOAD_PROC(module, grBindObjectMemory);

    LOAD_PROC(module, grGetFormatInfo);
    LOAD_PROC(module, grCreateImage);
    LOAD_PROC(module, grGetImageSubresourceInfo);
    LOAD_PROC(module, grCreateSampler);

    LOAD_PROC(module, grCreateImageView);
    LOAD_PROC(module, grCreateColorTargetView);
    LOAD_PROC(module, grCreateDepthStencilView);

    LOAD_PROC(module, grCreateShader);
    LOAD_PROC(module, grCreateGraphicsPipeline);
    LOAD_PROC(module, grCreateComputePipeline);
    LOAD_PROC(module, grStorePipeline);
    LOAD_PROC(module, grLoadPipeline);

    LOAD_PROC(module, grCreateDescriptorSet);
    LOAD_PROC(module, grBeginDescriptorSetUpdate);
    LOAD_PROC(module, grEndDescriptorSetUpdate);
    LOAD_PROC(module, grAttachSamplerDescriptors);
    LOAD_PROC(module, grAttachImageViewDescriptors);
    LOAD_PROC(module, grAttachMemoryViewDescriptors);
    LOAD_PROC(module, grAttachNestedDescriptors);
    LOAD_PROC(module, grClearDescriptorSetSlots);

    LOAD_PROC(module, grCreateViewportState);
    LOAD_PROC(module, grCreateRasterState);
    LOAD_PROC(module, grCreateColorBlendState);
    LOAD_PROC(module, grCreateDepthStencilState);
    LOAD_PROC(module, grCreateMsaaState);

    LOAD_PROC(module, grCreateQueryPool);
    LOAD_PROC(module, grGetQueryPoolResults);
    LOAD_PROC(module, grCreateFence);
    LOAD_PROC(module, grGetFenceStatus);
    LOAD_PROC(module, grWaitForFences);
    LOAD_PROC(module, grCreateQueueSemaphore);
    LOAD_PROC(module, grSignalQueueSemaphore);
    LOAD_PROC(module, grWaitQueueSemaphore);
    LOAD_PROC(module, grCreateEvent);
    LOAD_PROC(module, grGetEventStatus);
    LOAD_PROC(module, grSetEvent);
    LOAD_PROC(module, grResetEvent);

    LOAD_PROC(module, grGetMultiGpuCompatibility);
    LOAD_PROC(module, grOpenSharedMemory);
    LOAD_PROC(module, grOpenSharedQueueSemaphore);
    LOAD_PROC(module, grOpenPeerMemory);
    LOAD_PROC(module, grOpenPeerImage);

    LOAD_PROC(module, grCreateCommandBuffer);
    LOAD_PROC(module, grBeginCommandBuffer);
    LOAD_PROC(module, grEndCommandBuffer);
    LOAD_PROC(module, grResetCommandBuffer);

    LOAD_PROC(module, grCmdBindPipeline);
    LOAD_PROC(module, grCmdBindStateObject);
    LOAD_PROC(module, grCmdBindDescriptorSet);
    LOAD_PROC(module, grCmdBindDynamicMemoryView);
    LOAD_PROC(module, grCmdBindIndexData);
    LOAD_PROC(module, grCmdBindTargets);
    LOAD_PROC(module, grCmdPrepareMemoryRegions);
    LOAD_PROC(module, grCmdPrepareImages);
    LOAD_PROC(module, grCmdDraw);
    LOAD_PROC(module, grCmdDrawIndexed);
    LOAD_PROC(module, grCmdDrawIndirect);
    LOAD_PROC(module, grCmdDrawIndexedIndirect);
    LOAD_PROC(module, grCmdDispatch);
    LOAD_PROC(module, grCmdDispatchIndirect);
    LOAD_PROC(module, grCmdCopyMemory);
    LOAD_PROC(module, grCmdCopyImage);
    LOAD_PROC(module, grCmdCopyMemoryToImage);
    LOAD_PROC(module, grCmdCopyImageToMemory);
    LOAD_PROC(module, grCmdResolveImage);
    LOAD_PROC(module, grCmdCloneImageData);
    LOAD_PROC(module, grCmdUpdateMemory);
    LOAD_PROC(module, grCmdFillMemory);
    LOAD_PROC(module, grCmdClearColorImage);
    LOAD_PROC(module, grCmdClearColorImageRaw);
    LOAD_PROC(module, grCmdClearDepthStencil);
    LOAD_PROC(module, grCmdSetEvent);
    LOAD_PROC(module, grCmdResetEvent);
    LOAD_PROC(module, grCmdMemoryAtomic);
    LOAD_PROC(module, grCmdBeginQuery);
    LOAD_PROC(module, grCmdEndQuery);
    LOAD_PROC(module, grCmdResetQueryPool);
    LOAD_PROC(module, grCmdWriteTimestamp);
    LOAD_PROC(module, grCmdInitAtomicCounters);
    LOAD_PROC(module, grCmdLoadAtomicCounters);
    LOAD_PROC(module, grCmdSaveAtomicCounters);

    return true;
}
