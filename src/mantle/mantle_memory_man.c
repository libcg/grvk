#include "mantle_internal.h"

// Memory Management Functions

GR_RESULT grGetMemoryHeapCount(
    GR_DEVICE device,
    GR_UINT* pCount)
{
    GrvkDevice* grvkDevice = (GrvkDevice*)device;

    if (grvkDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (grvkDevice->sType != GRVK_STRUCT_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pCount == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    VkPhysicalDeviceMemoryProperties vkMemoryProperties;
    vkGetPhysicalDeviceMemoryProperties(grvkDevice->physicalDevice, &vkMemoryProperties);

    *pCount = vkMemoryProperties.memoryTypeCount;
    return GR_SUCCESS;
}

GR_RESULT grGetMemoryHeapInfo(
    GR_DEVICE device,
    GR_UINT heapId,
    GR_ENUM infoType,
    GR_SIZE* pDataSize,
    GR_VOID* pData)
{
    GrvkDevice* grvkDevice = (GrvkDevice*)device;

    if (grvkDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (grvkDevice->sType != GRVK_STRUCT_TYPE_DEVICE) {
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

    VkPhysicalDeviceMemoryProperties vkMemoryProperties;
    vkGetPhysicalDeviceMemoryProperties(grvkDevice->physicalDevice, &vkMemoryProperties);

    if (heapId >= vkMemoryProperties.memoryTypeCount) {
        return GR_ERROR_INVALID_ORDINAL;
    }

    VkMemoryPropertyFlags flags = vkMemoryProperties.memoryTypes[heapId].propertyFlags;
    uint32_t vkHeapIndex = vkMemoryProperties.memoryTypes[heapId].heapIndex;

    *(GR_MEMORY_HEAP_PROPERTIES*)pData = (GR_MEMORY_HEAP_PROPERTIES) {
        .heapMemoryType = (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0 ?
                          GR_HEAP_MEMORY_LOCAL : GR_HEAP_MEMORY_REMOTE,
        .heapSize = vkMemoryProperties.memoryHeaps[vkHeapIndex].size,
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
    GrvkDevice* grvkDevice = (GrvkDevice*)device;

    if (grvkDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (grvkDevice->sType != GRVK_STRUCT_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pAllocInfo == NULL || pMem == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    if (pAllocInfo->flags != 0) { // TODO
        printf("%s: allocation flags %d are not supported\n", __func__, pAllocInfo->flags);
        return GR_ERROR_INVALID_FLAGS;
    }
    if (pAllocInfo->heapCount > 1) { // TODO
        printf("%s: multi-heap allocation is not implemented\n", __func__);
        return GR_ERROR_INVALID_VALUE;
    }

    const VkMemoryAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = pAllocInfo->size,
        .memoryTypeIndex = pAllocInfo->heaps[0],
    };

    VkDeviceMemory vkMemory = VK_NULL_HANDLE;
    if (vkAllocateMemory(grvkDevice->device, &allocateInfo, NULL, &vkMemory) != VK_SUCCESS) {
        printf("%s: vkAllocateMemory failed\n", __func__);
        return GR_ERROR_OUT_OF_GPU_MEMORY;
    }

    GrvkGpuMemory* grvkGpuMemory = malloc(sizeof(GrvkGpuMemory));
    *grvkGpuMemory = (GrvkGpuMemory) {
        .sType = GRVK_STRUCT_TYPE_GPU_MEMORY,
        .deviceMemory = vkMemory,
        .device = grvkDevice->device,
    };

    *pMem = (GR_GPU_MEMORY)grvkGpuMemory;
    return GR_SUCCESS;
}

GR_RESULT grMapMemory(
    GR_GPU_MEMORY mem,
    GR_FLAGS flags,
    GR_VOID** ppData)
{
    GrvkGpuMemory* grvkGpuMemory = (GrvkGpuMemory*)mem;

    if (grvkGpuMemory == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (grvkGpuMemory->sType != GRVK_STRUCT_TYPE_GPU_MEMORY) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (flags != 0) {
        return GR_ERROR_INVALID_FLAGS;
    } else if (ppData == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    if (vkMapMemory(grvkGpuMemory->device, grvkGpuMemory->deviceMemory,
                    0, VK_WHOLE_SIZE, 0, ppData) != VK_SUCCESS) {
        printf("%s: vkMapMemory failed\n", __func__);
        return GR_ERROR_MEMORY_MAP_FAILED;
    }

    return GR_SUCCESS;
}

GR_RESULT grUnmapMemory(
    GR_GPU_MEMORY mem)
{
    GrvkGpuMemory* grvkGpuMemory = (GrvkGpuMemory*)mem;

    if (grvkGpuMemory == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (grvkGpuMemory->sType != GRVK_STRUCT_TYPE_GPU_MEMORY) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    vkUnmapMemory(grvkGpuMemory->device, grvkGpuMemory->deviceMemory);

    return GR_SUCCESS;
}
