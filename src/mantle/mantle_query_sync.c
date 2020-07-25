#include "mantle_internal.h"

// Query and Synchronization Functions

GR_RESULT grCreateFence(
    GR_DEVICE device,
    const GR_FENCE_CREATE_INFO* pCreateInfo,
    GR_FENCE* pFence)
{
    GrDevice* grDevice = (GrDevice*)device;

    VkFence vkFence = VK_NULL_HANDLE;

    const VkFenceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };

    if (vki.vkCreateFence(grDevice->device, &createInfo, NULL, &vkFence) != VK_SUCCESS) {
        printf("%s: vkCreateFence failed\n", __func__);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    GrFence* grFence = malloc(sizeof(GrFence));
    *grFence = (GrFence) {
        .sType = GR_STRUCT_TYPE_FENCE,
        .fence = vkFence,
    };

    *pFence = (GR_FENCE)grFence;

    return GR_SUCCESS;
}

GR_RESULT grWaitForFences(
    GR_DEVICE device,
    GR_UINT fenceCount,
    const GR_FENCE* pFences,
    GR_BOOL waitAll,
    GR_FLOAT timeout)
{
    GrDevice* grDevice = (GrDevice*)device;
    uint64_t vkTimeout = timeout * 1000000000ull; // Convert to nanoseconds
    VkResult res;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (grDevice->sType != GR_STRUCT_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (fenceCount == 0) {
        return GR_ERROR_INVALID_VALUE;
    } else if (pFences == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    VkFence* vkFences = malloc(sizeof(VkFence) * fenceCount);
    for (int i = 0; i < fenceCount; i++) {
        GrFence* grFence = (GrFence*)pFences[i];

        vkFences[i] = grFence->fence;
    }

    res = vki.vkWaitForFences(grDevice->device, fenceCount, vkFences, waitAll, vkTimeout);
    free(vkFences);

    if (res == VK_SUCCESS) {
        return GR_SUCCESS;
    } else if (res == VK_TIMEOUT) {
        return GR_TIMEOUT;
    } else {
        printf("%s: vkWaitForFences failed\n", __func__);
        return GR_ERROR_OUT_OF_MEMORY;
    }
}
