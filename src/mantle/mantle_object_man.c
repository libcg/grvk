#include "mantle_internal.h"

// Generic API Object Management functions

GR_RESULT grGetObjectInfo(
    GR_BASE_OBJECT object,
    GR_ENUM infoType,
    GR_SIZE* pDataSize,
    GR_VOID* pData)
{
    GrObject* grObject = (GrObject*)object;

    if (pDataSize == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    switch (infoType) {
    case GR_INFO_TYPE_MEMORY_REQUIREMENTS: {
        GR_MEMORY_REQUIREMENTS* memReqs = (GR_MEMORY_REQUIREMENTS*)pData;

        if (pData == NULL) {
            *pDataSize = sizeof(GR_MEMORY_REQUIREMENTS);
            return GR_SUCCESS;
        } else if (*pDataSize != sizeof(GR_MEMORY_REQUIREMENTS)) {
            return GR_ERROR_INVALID_MEMORY_SIZE;
        }

        if (grObject->sType == GR_STRUCT_TYPE_DESCRIPTOR_SET ||
            grObject->sType == GR_STRUCT_TYPE_PIPELINE) {
            // No memory requirements
            *memReqs = (GR_MEMORY_REQUIREMENTS) {
                .size = 0,
                .alignment = 0,
                .heapCount = 0,
            };
        } else {
            printf("%s: unsupported type %d for info type 0x%X\n", __func__,
                   grObject->sType, infoType);
            return GR_ERROR_INVALID_VALUE;
        }
    }   break;
    default:
        printf("%s: unsupported info type 0x%X\n", __func__, infoType);
        return GR_ERROR_INVALID_VALUE;
    }

    return GR_SUCCESS;
}

GR_RESULT grBindObjectMemory(
    GR_OBJECT object,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset)
{
    GrObject* grObject = (GrObject*)object;

    if (grObject == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }

    if (grObject->sType == GR_STRUCT_TYPE_DESCRIPTOR_SET ||
        grObject->sType == GR_STRUCT_TYPE_PIPELINE) {
        // Nothing to do
    } else {
        printf("%s: unsupported object type %d\n", __func__, grObject->sType);
        return GR_ERROR_UNAVAILABLE;
    }

    return GR_SUCCESS;
}
