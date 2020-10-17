#include "mantle_internal.h"

// Memory Management Functions

GR_RESULT grGetMemoryHeapCount(
    GR_DEVICE device,
    GR_UINT* pCount)
{
    LOGT("%p %p\n", device, pCount);
    GrDevice* grDevice = (GrDevice*)device;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (grDevice->sType != GR_STRUCT_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pCount == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    *pCount = grDevice->memoryProperties.memoryTypeCount;
    return GR_SUCCESS;
}

GR_RESULT grGetMemoryHeapInfo(
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
    } else if (grDevice->sType != GR_STRUCT_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (infoType != GR_INFO_TYPE_MEMORY_HEAP_PROPERTIES) {
        return GR_ERROR_INVALID_VALUE;
    } else if (pDataSize == NULL) {
        return GR_ERROR_INVALID_POINTER;
    } else if (pData == NULL) {
        *pDataSize = sizeof(GR_MEMORY_HEAP_PROPERTIES);
        return GR_SUCCESS;
    } else if (pData != NULL && *pDataSize < sizeof(GR_MEMORY_HEAP_PROPERTIES)) {
        return GR_ERROR_INVALID_MEMORY_SIZE;
    }

    if (heapId >= grDevice->memoryProperties.memoryTypeCount) {
        return GR_ERROR_INVALID_ORDINAL;
    }

    VkMemoryPropertyFlags flags = grDevice->memoryProperties.memoryTypes[heapId].propertyFlags;
    uint32_t vkHeapIndex = grDevice->memoryProperties.memoryTypes[heapId].heapIndex;

    *(GR_MEMORY_HEAP_PROPERTIES*)pData = (GR_MEMORY_HEAP_PROPERTIES) {
        .heapMemoryType = (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0 ?
                          GR_HEAP_MEMORY_LOCAL : GR_HEAP_MEMORY_REMOTE,
        .heapSize = grDevice->memoryProperties.memoryHeaps[vkHeapIndex].size,
        .pageSize = 4096, // FIXME guessed
        .flags = ((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0 ?
                  GR_MEMORY_HEAP_CPU_VISIBLE : 0) |
                 ((flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0 ?
                  GR_MEMORY_HEAP_CPU_GPU_COHERENT : 0) |
                 ((flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) == 0 ?
                  GR_MEMORY_HEAP_CPU_UNCACHED : 0),
        .gpuReadPerfRating = 1.f, // TODO
        .gpuWritePerfRating = 1.f, // TODO
        .cpuReadPerfRating = (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0 ?
                             1.f : 0.f, // TODO
        .cpuWritePerfRating = (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0 ?
                              1.f : 0.f, // TODO
    };

    return GR_SUCCESS;
}

GR_RESULT grAllocMemory(
    GR_DEVICE device,
    const GR_MEMORY_ALLOC_INFO* pAllocInfo,
    GR_GPU_MEMORY* pMem)
{
    LOGT("%p %p %p\n", device, pAllocInfo, pMem);
    GrDevice* grDevice = (GrDevice*)device;
    VkResult vkRes;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (grDevice->sType != GR_STRUCT_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pAllocInfo == NULL || pMem == NULL) {
        return GR_ERROR_INVALID_POINTER;
    } else if ((pAllocInfo->heapCount != 0 && (pAllocInfo->flags & GR_MEMORY_ALLOC_VIRTUAL)) ||
               (pAllocInfo->heapCount == 0 && !(pAllocInfo->flags & GR_MEMORY_ALLOC_VIRTUAL))) {
        return GR_ERROR_INVALID_VALUE;
    }

    if (pAllocInfo->flags != 0) { // TODO
        LOGW("allocation flags %d are not supported\n", pAllocInfo->flags);
        return GR_ERROR_INVALID_FLAGS;
    }
    // TODO consider pAllocInfo->memPriority

    VkDeviceMemory vkMemory = VK_NULL_HANDLE;

    // Try to allocate from the best heap
    for (int i = 0; i < pAllocInfo->heapCount; i++) {
        const VkMemoryAllocateInfo allocateInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = NULL,
            .allocationSize = pAllocInfo->size,
            .memoryTypeIndex = pAllocInfo->heaps[i],
        };

        vkRes = vki.vkAllocateMemory(grDevice->device, &allocateInfo, NULL, &vkMemory);
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

    const VkBufferCreateInfo bufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = pAllocInfo->size,
        .usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, // FIXME incomplete
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
    };

    VkBuffer vkBuffer = VK_NULL_HANDLE;
    vkRes = vki.vkCreateBuffer(grDevice->device, &bufferCreateInfo, NULL, &vkBuffer);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkCreateBuffer failed (%d)\n", vkRes);
        vki.vkFreeMemory(grDevice->device, vkMemory, NULL);
        return getGrResult(vkRes);
    }

    vkRes = vki.vkBindBufferMemory(grDevice->device, vkBuffer, vkMemory, 0);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkBindBufferMemory failed (%d)\n", vkRes);
        vki.vkDestroyBuffer(grDevice->device, vkBuffer, NULL);
        vki.vkFreeMemory(grDevice->device, vkMemory, NULL);
        return getGrResult(vkRes);
    }

    GrGpuMemory* grGpuMemory = malloc(sizeof(GrGpuMemory));
    *grGpuMemory = (GrGpuMemory) {
        .sType = GR_STRUCT_TYPE_GPU_MEMORY,
        .deviceMemory = vkMemory,
        .device = grDevice->device,
        .buffer = vkBuffer,
    };

    *pMem = (GR_GPU_MEMORY)grGpuMemory;
    return GR_SUCCESS;
}

GR_RESULT grFreeMemory(
    GR_GPU_MEMORY mem)
{
    LOGT("%p\n", mem);
    GrGpuMemory* grGpuMemory = (GrGpuMemory*)mem;

    if (grGpuMemory == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (grGpuMemory->sType != GR_STRUCT_TYPE_GPU_MEMORY) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    vki.vkDestroyBuffer(grGpuMemory->device, grGpuMemory->buffer, NULL);
    vki.vkFreeMemory(grGpuMemory->device, grGpuMemory->deviceMemory, NULL);
    free(grGpuMemory);

    return GR_SUCCESS;
}

GR_RESULT grMapMemory(
    GR_GPU_MEMORY mem,
    GR_FLAGS flags,
    GR_VOID** ppData)
{
    LOGT("%p 0x%X %p\n", mem, flags, ppData);
    GrGpuMemory* grGpuMemory = (GrGpuMemory*)mem;

    if (grGpuMemory == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (grGpuMemory->sType != GR_STRUCT_TYPE_GPU_MEMORY) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (flags != 0) {
        return GR_ERROR_INVALID_FLAGS;
    } else if (ppData == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    VkResult vkRes = vki.vkMapMemory(grGpuMemory->device, grGpuMemory->deviceMemory,
                                     0, VK_WHOLE_SIZE, 0, ppData);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkMapMemory failed (%d)\n", vkRes);
    }

    return getGrResult(vkRes);
}

GR_RESULT grUnmapMemory(
    GR_GPU_MEMORY mem)
{
    LOGT("%p\n", mem);
    GrGpuMemory* grGpuMemory = (GrGpuMemory*)mem;

    if (grGpuMemory == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (grGpuMemory->sType != GR_STRUCT_TYPE_GPU_MEMORY) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    vki.vkUnmapMemory(grGpuMemory->device, grGpuMemory->deviceMemory);

    return GR_SUCCESS;
}
