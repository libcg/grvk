#include "mantle_internal.h"

// Generic API Object Management functions

GR_RESULT grDestroyObject(
    GR_OBJECT object)
{
    LOGT("%p\n", object);
    GrObject* grObject = (GrObject*)object;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grObject);

    if (grObject == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }

    switch (grObject->grObjType) {
    case GR_OBJ_TYPE_COMMAND_BUFFER: {
        GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)grObject;

        VKD.vkDestroyCommandPool(grDevice->device, grCmdBuffer->commandPool, NULL);
        grCmdBufferResetState(grCmdBuffer);
    }   break;
    case GR_OBJ_TYPE_COLOR_TARGET_VIEW: {
        GrColorTargetView* grColorTargetView = (GrColorTargetView*)grObject;

        VKD.vkDestroyImageView(grDevice->device, grColorTargetView->imageView, NULL);
    }   break;
    case GR_OBJ_TYPE_IMAGE: {
        GrImage* grImage = (GrImage*)grObject;

        VKD.vkDestroyImage(grDevice->device, grImage->image, NULL);
        VKD.vkDestroyBuffer(grDevice->device, grImage->buffer, NULL);
    }   break;
    case GR_OBJ_TYPE_IMAGE_VIEW: {
        GrImageView* grImageView = (GrImageView*)grObject;

        VKD.vkDestroyImageView(grDevice->device, grImageView->imageView, NULL);
    }   break;
    case GR_OBJ_TYPE_SHADER:
        // FIXME actually destroy it?
        return GR_SUCCESS;
    default:
        LOGW("unsupported object type %u\n", grObject->grObjType);
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    free(grObject);
    return GR_SUCCESS;
}

GR_RESULT grGetObjectInfo(
    GR_BASE_OBJECT object,
    GR_ENUM infoType,
    GR_SIZE* pDataSize,
    GR_VOID* pData)
{
    LOGT("%p 0x%X %p %p\n", object, infoType, pDataSize, pData);
    GrBaseObject* grBaseObject = (GrBaseObject*)object;

    if (grBaseObject == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (pDataSize == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    GrObjectType objType = GET_OBJ_TYPE(grBaseObject);
    unsigned expectedSize = 0;

    switch (infoType) {
    case GR_INFO_TYPE_MEMORY_REQUIREMENTS: {
        GR_MEMORY_REQUIREMENTS* grMemReqs = (GR_MEMORY_REQUIREMENTS*)pData;
        VkMemoryRequirements memReqs;

        expectedSize = sizeof(GR_MEMORY_REQUIREMENTS);

        if (pData == NULL) {
            break;
        } else if (*pDataSize < expectedSize) {
            LOGW("can't write GR_MEMORY_REQUIREMENTS, got size %d, expected %d\n",
                 *pDataSize, expectedSize);
            return GR_ERROR_INVALID_MEMORY_SIZE;
        }

        switch (objType) {
        case GR_OBJ_TYPE_IMAGE: {
            GrImage* grImage = (GrImage*)grBaseObject;
            GrDevice* grDevice = GET_OBJ_DEVICE(grBaseObject);

            if (grImage->image != VK_NULL_HANDLE) {
                VKD.vkGetImageMemoryRequirements(grDevice->device, grImage->image, &memReqs);
            } else {
                VKD.vkGetBufferMemoryRequirements(grDevice->device, grImage->buffer, &memReqs);
            }

            *grMemReqs = getGrMemoryRequirements(memReqs);
        }   break;
        case GR_OBJ_TYPE_BORDER_COLOR_PALETTE:
        case GR_OBJ_TYPE_DESCRIPTOR_SET:
        case GR_OBJ_TYPE_PIPELINE:
            // No memory requirements
            *grMemReqs = (GR_MEMORY_REQUIREMENTS) {
                .size = 4,
                .alignment = 4,
                .heapCount = 0,
            };
            break;
        default:
            LOGW("unsupported type %d for info type 0x%X\n", objType, infoType);
            return GR_ERROR_INVALID_VALUE;
        }
    }   break;
    case GR_WSI_WIN_INFO_TYPE_QUEUE_PROPERTIES: {
        GR_WSI_WIN_QUEUE_PROPERTIES* grQueueProps = (GR_WSI_WIN_QUEUE_PROPERTIES*)pData;

        expectedSize = sizeof(GR_WSI_WIN_QUEUE_PROPERTIES);

        if (pData == NULL) {
            break;
        } else if (*pDataSize < expectedSize) {
            LOGW("can't write GR_WSI_WIN_QUEUE_PROPERTIES, got size %d, expected %d\n",
                 *pDataSize, expectedSize);
            return GR_ERROR_INVALID_MEMORY_SIZE;
        }

        // FIXME check queue capability
        *grQueueProps = (GR_WSI_WIN_QUEUE_PROPERTIES) {
            .presentSupport = GR_WSI_WIN_FULLSCREEN_PRESENT_SUPPORTED |
                              GR_WSI_WIN_WINDOWED_PRESENT_SUPPORTED,
        };
    }   break;
    case GR_WSI_WIN_INFO_TYPE_DISPLAY_PROPERTIES: {
        GR_WSI_WIN_DISPLAY_PROPERTIES* grDisplayProps = (GR_WSI_WIN_DISPLAY_PROPERTIES*)pData;

        expectedSize = sizeof(GR_WSI_WIN_DISPLAY_PROPERTIES);

        if (pData == NULL) {
            break;
        } else if (*pDataSize < expectedSize) {
            LOGW("can't write GR_WSI_WIN_DISPLAY_PROPERTIES, got size %d, expected %d\n",
                 *pDataSize, expectedSize);
            return GR_ERROR_INVALID_MEMORY_SIZE;
        }

        // FIXME implement
        *grDisplayProps = (GR_WSI_WIN_DISPLAY_PROPERTIES) {
            .hMonitor = MonitorFromPoint((POINT){ 0, 0 }, MONITOR_DEFAULTTOPRIMARY),
            .displayName = "Display 0",
            .desktopCoordinates = {
                .offset = { 0, 0 },
                .extent = { 1920, 1080 },
            },
            .rotation = GR_WSI_WIN_ROTATION_ANGLE_0,
        };
    }   break;
    default:
        LOGW("unsupported info type 0x%X\n", infoType);
        return GR_ERROR_INVALID_VALUE;
    }

    assert(expectedSize != 0);
    *pDataSize = expectedSize;

    return GR_SUCCESS;
}

GR_RESULT grBindObjectMemory(
    GR_OBJECT object,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset)
{
    LOGT("%p %p %llu\n", object, mem, offset);
    GrObject* grObject = (GrObject*)object;
    GrGpuMemory* grGpuMemory = (GrGpuMemory*)mem;
    VkResult vkRes = VK_SUCCESS;

    if (grObject == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }

    GrObjectType objType = GET_OBJ_TYPE(grObject);

    switch (objType) {
    case GR_OBJ_TYPE_IMAGE: {
        GrImage* grImage = (GrImage*)grObject;
        GrDevice* grDevice = GET_OBJ_DEVICE(grObject);

        if (grImage->image != VK_NULL_HANDLE) {
            vkRes = VKD.vkBindImageMemory(grDevice->device, grImage->image,
                                          grGpuMemory->deviceMemory, offset);
        } else {
            vkRes = VKD.vkBindBufferMemory(grDevice->device, grImage->buffer,
                                           grGpuMemory->deviceMemory, offset);
        }
    }   break;
    case GR_OBJ_TYPE_BORDER_COLOR_PALETTE:
    case GR_OBJ_TYPE_DESCRIPTOR_SET:
    case GR_OBJ_TYPE_PIPELINE:
        // Nothing to do
        break;
    default:
        LOGW("unsupported object type %d\n", objType);
        return GR_ERROR_UNAVAILABLE;
    }

    EnterCriticalSection(&grGpuMemory->boundObjectsMutex);
    grGpuMemory->boundObjectCount++;
    grGpuMemory->boundObjects = realloc(grGpuMemory->boundObjects,
                                        grGpuMemory->boundObjectCount * sizeof(GrObject*));
    grGpuMemory->boundObjects[grGpuMemory->boundObjectCount - 1] = grObject;
    LeaveCriticalSection(&grGpuMemory->boundObjectsMutex);

    if (vkRes != VK_SUCCESS) {
        LOGW("binding failed (%d)\n", objType);
    }
    return getGrResult(vkRes);
}
