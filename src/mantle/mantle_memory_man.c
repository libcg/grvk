#include "mantle_internal.h"

void grGpuMemoryBindBuffer(
    GrGpuMemory* grGpuMemory)
{
    const GrDevice* grDevice = GET_OBJ_DEVICE(grGpuMemory);
    VkResult vkRes;

    if (grGpuMemory->buffer != VK_NULL_HANDLE) {
        // Already bound
        return;
    }

    // TODO recreate the buffer for specific usages
    const VkBufferCreateInfo bufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = grGpuMemory->deviceSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                 VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
                 VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
    };

    vkRes = VKD.vkCreateBuffer(grDevice->device, &bufferCreateInfo, NULL, &grGpuMemory->buffer);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkCreateBuffer failed (%d)\n", vkRes);
        assert(false);
    }

    vkRes = VKD.vkBindBufferMemory(grDevice->device, grGpuMemory->buffer,
                                   grGpuMemory->deviceMemory, 0);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkBindBufferMemory failed (%d)\n", vkRes);
        assert(false);
    }
}

// Memory Management Functions

GR_RESULT GR_STDCALL grGetMemoryHeapCount(
    GR_DEVICE device,
    GR_UINT* pCount)
{
    LOGT("%p %p\n", device, pCount);
    GrDevice* grDevice = (GrDevice*)device;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grDevice) != GR_OBJ_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pCount == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    *pCount = grDevice->memoryProperties.memoryTypeCount;
    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grGetMemoryHeapInfo(
    GR_DEVICE device,
    GR_UINT heapId,
    GR_ENUM infoType,
    GR_SIZE* pDataSize,
    GR_VOID* pData)
{
    LOGT("%p %u 0x%X %p %p\n", device, heapId, infoType, pDataSize, pData);
    GrDevice* grDevice = (GrDevice*)device;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grDevice) != GR_OBJ_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (infoType != GR_INFO_TYPE_MEMORY_HEAP_PROPERTIES) {
        return GR_ERROR_INVALID_VALUE;
    } else if (pDataSize == NULL) {
        return GR_ERROR_INVALID_POINTER;
    } else if (pData != NULL && *pDataSize < sizeof(GR_MEMORY_HEAP_PROPERTIES)) {
        return GR_ERROR_INVALID_MEMORY_SIZE;
    }

    if (heapId >= grDevice->memoryProperties.memoryTypeCount) {
        return GR_ERROR_INVALID_ORDINAL;
    }

    *pDataSize = sizeof(GR_MEMORY_HEAP_PROPERTIES);

    if (pData == NULL) {
        return GR_SUCCESS;
    }

    VkMemoryPropertyFlags flags = grDevice->memoryProperties.memoryTypes[heapId].propertyFlags;
    uint32_t vkHeapIndex = grDevice->memoryProperties.memoryTypes[heapId].heapIndex;
    bool deviceLocal = (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
    bool hostVisible = (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
    bool hostCoherent = (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
    bool hostCached = (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0;

    // FIXME heaps are out of order compared to 19.4.3
    // FIXME perf ratings need rework

    // https://www.basnieuwenhuizen.nl/the-catastrophe-of-reading-from-vram/
    // https://gpuopen.com/learn/vulkan-device-memory/
    *(GR_MEMORY_HEAP_PROPERTIES*)pData = (GR_MEMORY_HEAP_PROPERTIES) {
        .heapMemoryType = deviceLocal ? GR_HEAP_MEMORY_LOCAL : GR_HEAP_MEMORY_REMOTE,
        .heapSize = grDevice->memoryProperties.memoryHeaps[vkHeapIndex].size,
        .pageSize = 65536, // 19.4.3
        .flags = (hostVisible ? GR_MEMORY_HEAP_CPU_VISIBLE : 0) |
                 (hostCoherent ? GR_MEMORY_HEAP_CPU_GPU_COHERENT : 0) |
                 (hostVisible && !hostCached ? GR_MEMORY_HEAP_CPU_UNCACHED : 0) |
                 (hostCoherent && !hostCached ? GR_MEMORY_HEAP_CPU_WRITE_COMBINED : 0) |
                 (!deviceLocal && hostCached ? GR_MEMORY_HEAP_HOLDS_PINNED : 0) |
                 (!deviceLocal ? GR_MEMORY_HEAP_SHAREABLE : 0),
        .gpuReadPerfRating = 10.0f + 1000.0f * deviceLocal, // FIXME
        .gpuWritePerfRating = 10.0f + 1000.0f * deviceLocal, // FIXME
        // Mantle spec: "For heaps inaccessible by the CPU, the read and write performance rating
        //               of the CPU is reported as zero"
        .cpuReadPerfRating = !hostVisible ? 0.0f :
                             (1.0f + (100.0f * !deviceLocal) + (10000.0f * hostCached)),
        .cpuWritePerfRating = !hostVisible ? 0.0f :
                              (10000.0f + (1000.0f * !deviceLocal) + (7000.0f * hostCached)),
    };

    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grAllocMemory(
    GR_DEVICE device,
    const GR_MEMORY_ALLOC_INFO* pAllocInfo,
    GR_GPU_MEMORY* pMem)
{
    LOGT("%p %p %p\n", device, pAllocInfo, pMem);
    GrDevice* grDevice = (GrDevice*)device;
    VkResult vkRes;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grDevice) != GR_OBJ_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pAllocInfo == NULL || pMem == NULL) {
        return GR_ERROR_INVALID_POINTER;
    } else if ((pAllocInfo->heapCount != 0 && (pAllocInfo->flags & GR_MEMORY_ALLOC_VIRTUAL)) ||
               (pAllocInfo->heapCount == 0 && !(pAllocInfo->flags & GR_MEMORY_ALLOC_VIRTUAL))) {
        return GR_ERROR_INVALID_VALUE;
    }

    if (pAllocInfo->flags & GR_MEMORY_ALLOC_SHAREABLE) {
        LOGW("unhandled shareable flag\n");
    } else if (pAllocInfo->flags != 0) {
        LOGW("allocation flags %d are not supported\n", pAllocInfo->flags);
        return GR_ERROR_INVALID_FLAGS;
    }
    // TODO consider pAllocInfo->memPriority

    VkDeviceMemory vkMemory = VK_NULL_HANDLE;

    // Try to allocate from the best heap
    vkRes = VK_ERROR_UNKNOWN;
    for (int i = 0; i < pAllocInfo->heapCount; i++) {
        const VkMemoryAllocateInfo allocateInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = NULL,
            .allocationSize = pAllocInfo->size,
            .memoryTypeIndex = pAllocInfo->heaps[i],
        };

        vkRes = VKD.vkAllocateMemory(grDevice->device, &allocateInfo, NULL, &vkMemory);
        if (vkRes == VK_SUCCESS) {
            break;
        } else if (vkRes == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
            continue;
        } else {
            LOGE("vkAllocateMemory failed (%d)\n", vkRes);
            return getGrResult(vkRes);
        }
    }

    if (vkRes != VK_SUCCESS) {
        LOGE("no suitable heap was found\n");
        return GR_ERROR_OUT_OF_GPU_MEMORY;
    }

    GrGpuMemory* grGpuMemory = malloc(sizeof(GrGpuMemory));
    *grGpuMemory = (GrGpuMemory) {
        .grObj = { GR_OBJ_TYPE_GPU_MEMORY, grDevice },
        .deviceMemory = vkMemory,
        .deviceSize = pAllocInfo->size,
        .buffer = VK_NULL_HANDLE, // Created on demand
    };

    *pMem = (GR_GPU_MEMORY)grGpuMemory;
    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grFreeMemory(
    GR_GPU_MEMORY mem)
{
    LOGT("%p\n", mem);
    GrGpuMemory* grGpuMemory = (GrGpuMemory*)mem;

    if (grGpuMemory == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grGpuMemory) != GR_OBJ_TYPE_GPU_MEMORY) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    GrDevice* grDevice = GET_OBJ_DEVICE(grGpuMemory);

    VKD.vkDestroyBuffer(grDevice->device, grGpuMemory->buffer, NULL);
    VKD.vkFreeMemory(grDevice->device, grGpuMemory->deviceMemory, NULL);
    free(grGpuMemory);

    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grMapMemory(
    GR_GPU_MEMORY mem,
    GR_FLAGS flags,
    GR_VOID** ppData)
{
    LOGT("%p 0x%X %p\n", mem, flags, ppData);
    GrGpuMemory* grGpuMemory = (GrGpuMemory*)mem;

    if (grGpuMemory == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grGpuMemory) != GR_OBJ_TYPE_GPU_MEMORY) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (flags != 0) {
        return GR_ERROR_INVALID_FLAGS;
    } else if (ppData == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    GrDevice* grDevice = GET_OBJ_DEVICE(grGpuMemory);

    VkResult vkRes = VKD.vkMapMemory(grDevice->device, grGpuMemory->deviceMemory,
                                     0, VK_WHOLE_SIZE, 0, ppData);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkMapMemory failed (%d)\n", vkRes);
    }

    return getGrResult(vkRes);
}

GR_RESULT GR_STDCALL grUnmapMemory(
    GR_GPU_MEMORY mem)
{
    LOGT("%p\n", mem);
    GrGpuMemory* grGpuMemory = (GrGpuMemory*)mem;

    if (grGpuMemory == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grGpuMemory) != GR_OBJ_TYPE_GPU_MEMORY) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    GrDevice* grDevice = GET_OBJ_DEVICE(grGpuMemory);

    VKD.vkUnmapMemory(grDevice->device, grGpuMemory->deviceMemory);

    return GR_SUCCESS;
}
