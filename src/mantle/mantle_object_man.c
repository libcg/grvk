#include "mantle_internal.h"

void getGrImageMemoryProperties(const GrImage* grImage, GR_MEMORY_REQUIREMENTS* reqs)
{
    VkMemoryRequirements vkReqs;
    vki.vkGetImageMemoryRequirements(grImage->device->device, grImage->image, &vkReqs);
    reqs->size = vkReqs.size;
    reqs->alignment = vkReqs.alignment;
    reqs->heapCount = 0;
    const VkPhysicalDeviceMemoryProperties *memProps = &grImage->device->memoryProperties;
    //get heap info (actually memory types)
    for (uint32_t i = 0; i < memProps->memoryTypeCount && reqs->heapCount < GR_MAX_MEMORY_HEAPS;++i) {
        if (vkReqs.memoryTypeBits & (1 << i)) {
            reqs->heaps[reqs->heapCount] = i;
            reqs->heapCount++;
        }
    }
}

// Generic API Object Management functions

GR_RESULT grGetObjectInfo(
    GR_BASE_OBJECT object,
    GR_ENUM infoType,
    GR_SIZE* pDataSize,
    GR_VOID* pData)
{
    LOGT("%p 0x%X %p %p\n", object, infoType, pDataSize, pData);
    GrObject* grObject = (GrObject*)object;
    if (grObject == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }
    if (pDataSize == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    switch (infoType) {
    case GR_INFO_TYPE_MEMORY_REQUIREMENTS: {
        GR_MEMORY_REQUIREMENTS* memReqs = (GR_MEMORY_REQUIREMENTS*)pData;

        if (pData == NULL) {
            *pDataSize = sizeof(GR_MEMORY_REQUIREMENTS);
            return GR_SUCCESS;
        } else if (*pDataSize < sizeof(GR_MEMORY_REQUIREMENTS)) {
            return GR_ERROR_INVALID_MEMORY_SIZE;
        }
        switch (grObject->sType) {
        case GR_STRUCT_TYPE_SHADER:
        case GR_STRUCT_TYPE_DEVICE:
        case GR_STRUCT_TYPE_PHYSICAL_GPU:
        case GR_STRUCT_TYPE_QUEUE:
        case GR_STRUCT_TYPE_GPU_MEMORY:
            LOGW("unsupported type %d for info type 0x%X\n",
                 grObject->sType, infoType);
            return GR_ERROR_INVALID_VALUE;
        case GR_STRUCT_TYPE_IMAGE:
            getGrImageMemoryProperties((GrImage*)grObject, memReqs);
            break;
        default:
            //no memory requirements for other types, but it is valid to request such data
            *memReqs = (GR_MEMORY_REQUIREMENTS) {
                .size = 0,
                .alignment = 1,
                .heapCount = 0,
            };
        }
    }   break;
    default:
        LOGW("unsupported info type 0x%X\n", infoType);
        return GR_ERROR_INVALID_VALUE;
    }

    return GR_SUCCESS;
}

GR_RESULT grBindObjectMemory(
    GR_OBJECT object,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset)
{
    LOGT("%p %p %llu\n", object, mem, offset);
    GrObject* grObject = (GrObject*)object;
    GrGpuMemory* memory = (GrGpuMemory*)mem;
    if (grObject == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }
    VkResult res = VK_SUCCESS;
    switch (grObject->sType){
    case GR_STRUCT_TYPE_SHADER:
    case GR_STRUCT_TYPE_DEVICE:
    case GR_STRUCT_TYPE_PHYSICAL_GPU:
    case GR_STRUCT_TYPE_QUEUE:
    case GR_STRUCT_TYPE_GPU_MEMORY:
        LOGW("unsupported object type %d\n", grObject->sType);
        return GR_ERROR_INVALID_OBJECT_TYPE;
    case GR_STRUCT_TYPE_IMAGE:
        if (memory != NULL && memory->sType != GR_STRUCT_TYPE_GPU_MEMORY) {
            LOGW("unsupported object type %d for binding to\n", memory->sType);
            return GR_ERROR_INVALID_OBJECT_TYPE;
        }
        GrImage* img = (GrImage*)object;
        res = vki.vkBindImageMemory(
            img->device->device,
            img->image,
            memory == NULL ? NULL : memory->deviceMemory,
            (VkDeviceSize)offset);
        break;
    default:
        //maybe return GR_ERROR_UNAVAILABLE, since it doesn't really bind any memory
        break;
    }
    return getGrResult(res);
}
