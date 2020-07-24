#include "mantle_internal.h"

// Query and Synchronization Functions

GR_RESULT grCreateFence(
    GR_DEVICE device,
    const GR_FENCE_CREATE_INFO* pCreateInfo,
    GR_FENCE* pFence)
{
    GrvkDevice* grvkDevice = (GrvkDevice*)device;

    VkFence vkFence = VK_NULL_HANDLE;

    const VkFenceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };

    if (vki.vkCreateFence(grvkDevice->device, &createInfo, NULL, &vkFence) != VK_SUCCESS) {
        printf("%s: vkCreateFence failed\n", __func__);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    GrvkFence* grvkFence = malloc(sizeof(GrvkFence));
    *grvkFence = (GrvkFence) {
        .sType = GRVK_STRUCT_TYPE_FENCE,
        .fence = vkFence,
    };

    *pFence = (GR_FENCE)grvkFence;

    return GR_SUCCESS;
}
