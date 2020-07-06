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

    GrvkDescriptorSet* grvkDescriptorSet = malloc(sizeof(GrvkDescriptorSet));
    *grvkDescriptorSet = (GrvkDescriptorSet) {
        .sType = GRVK_STRUCT_TYPE_DESCRIPTOR_SET,
        .slotCount = pCreateInfo->slots,
    };

    *pDescriptorSet = (GR_DESCRIPTOR_SET)grvkDescriptorSet;
    return GR_SUCCESS;
}
