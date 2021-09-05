#include "mantle_internal.h"

// Descriptor Set Functions

GR_RESULT GR_STDCALL grCreateDescriptorSet(
    GR_DEVICE device,
    const GR_DESCRIPTOR_SET_CREATE_INFO* pCreateInfo,
    GR_DESCRIPTOR_SET* pDescriptorSet)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pDescriptorSet);
    GrDevice* grDevice = (GrDevice*)device;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grDevice) != GR_OBJ_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pCreateInfo == NULL || pDescriptorSet == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    GrDescriptorSet* grDescriptorSet = malloc(sizeof(GrDescriptorSet));
    *grDescriptorSet = (GrDescriptorSet) {
        .grObj = { GR_OBJ_TYPE_DESCRIPTOR_SET, grDevice },
        .slotCount = pCreateInfo->slots,
        .slots = calloc(pCreateInfo->slots, sizeof(DescriptorSetSlot)),
    };

    *pDescriptorSet = (GR_DESCRIPTOR_SET)grDescriptorSet;
    return GR_SUCCESS;
}

GR_VOID grBeginDescriptorSetUpdate(
    GR_DESCRIPTOR_SET descriptorSet)
{
    LOGT("%p\n", descriptorSet);

    // TODO dirty tracking
}

GR_VOID grEndDescriptorSetUpdate(
    GR_DESCRIPTOR_SET descriptorSet)
{
    LOGT("%p\n", descriptorSet);

    // TODO update descriptors of bound pipelines
}

GR_VOID grAttachSamplerDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_SAMPLER* pSamplers)
{
    LOGT("%p %u %u %p\n", descriptorSet, startSlot, slotCount, pSamplers);
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;

    for (unsigned i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &grDescriptorSet->slots[startSlot + i];
        const GrSampler* grSampler = (GrSampler*)pSamplers[i];

        *slot = (DescriptorSetSlot) {
            .type = SLOT_TYPE_SAMPLER,
            .sampler.vkSampler = grSampler->sampler,
        };
    }
}

GR_VOID grAttachImageViewDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_IMAGE_VIEW_ATTACH_INFO* pImageViews)
{
    LOGT("%p %u %u %p\n", descriptorSet, startSlot, slotCount, pImageViews);
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;

    for (unsigned i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &grDescriptorSet->slots[startSlot + i];
        const GR_IMAGE_VIEW_ATTACH_INFO* info = &pImageViews[i];
        const GrImageView* grImageView = (GrImageView*)info->view;

        *slot = (DescriptorSetSlot) {
            .type = SLOT_TYPE_IMAGE_VIEW,
            .imageView = {
                .vkImageView = grImageView->imageView,
                .vkImageLayout = getVkImageLayout(info->state),
            },
        };
    }
}

GR_VOID grAttachMemoryViewDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_MEMORY_VIEW_ATTACH_INFO* pMemViews)
{
    LOGT("%p %u %u %p\n", descriptorSet, startSlot, slotCount, pMemViews);
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;

    for (unsigned i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &grDescriptorSet->slots[startSlot + i];
        const GR_MEMORY_VIEW_ATTACH_INFO* info = &pMemViews[i];
        GrGpuMemory* grGpuMemory = (GrGpuMemory*)info->mem;

        grGpuMemoryBindBuffer(grGpuMemory);

        *slot = (DescriptorSetSlot) {
            .type = SLOT_TYPE_MEMORY_VIEW,
            .memoryView = {
                .vkBuffer = grGpuMemory->buffer,
                .vkFormat = getVkFormat(info->format),
                .offset = info->offset,
                .range = info->range,
                .stride = info->stride,
            },
        };
    }
}

GR_VOID grAttachNestedDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_DESCRIPTOR_SET_ATTACH_INFO* pNestedDescriptorSets)
{
    LOGT("%p %u %u %p\n", descriptorSet, startSlot, slotCount, pNestedDescriptorSets);
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;

    for (unsigned i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &grDescriptorSet->slots[startSlot + i];
        const GR_DESCRIPTOR_SET_ATTACH_INFO* info = &pNestedDescriptorSets[i];

        *slot = (DescriptorSetSlot) {
            .type = SLOT_TYPE_NESTED,
            .nested = {
                .nextSet = (GrDescriptorSet*)info->descriptorSet,
                .slotOffset = info->slotOffset,
            },
        };
    }
}

GR_VOID grClearDescriptorSetSlots(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount)
{
    LOGT("%p %u %u\n", descriptorSet, startSlot, slotCount);
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;

    for (unsigned i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &grDescriptorSet->slots[startSlot + i];

        slot->type = SLOT_TYPE_NONE;
    }
}
