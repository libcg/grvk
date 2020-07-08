#include "mantle_internal.h"

// Descriptor Set Functions

GR_RESULT grCreateDescriptorSet(
    GR_DEVICE device,
    const GR_DESCRIPTOR_SET_CREATE_INFO* pCreateInfo,
    GR_DESCRIPTOR_SET* pDescriptorSet)
{
    GrvkDevice* grvkDevice = (GrvkDevice*)device;

    if (grvkDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (grvkDevice->sType != GRVK_STRUCT_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pCreateInfo == NULL || pDescriptorSet == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    // Create descriptor pool in a way that allows any type to fill all the requested slots
    const VkDescriptorPoolSize poolSize = {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // TODO support other types
        .descriptorCount = pCreateInfo->slots,
    };

    const VkDescriptorPoolCreateInfo poolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .maxSets = pCreateInfo->slots,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize,
    };

    VkDescriptorPool vkDescriptorPool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(grvkDevice->device, &poolCreateInfo, NULL,
                               &vkDescriptorPool) != VK_SUCCESS) {
        printf("%s: vkCreateDescriptorPool failed\n", __func__);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    GrvkDescriptorSet* grvkDescriptorSet = malloc(sizeof(GrvkDescriptorSet));
    *grvkDescriptorSet = (GrvkDescriptorSet) {
        .sType = GRVK_STRUCT_TYPE_DESCRIPTOR_SET,
        .descriptorPool = vkDescriptorPool,
    };

    *pDescriptorSet = (GR_DESCRIPTOR_SET)grvkDescriptorSet;
    return GR_SUCCESS;
}
