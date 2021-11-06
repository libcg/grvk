#include "mantle_internal.h"

inline static void releaseSlot(
    const GrDevice* grDevice,
    DescriptorSetSlot* slot)
{
    if (slot->type == SLOT_TYPE_BUFFER) {
        VKD.vkDestroyBufferView(grDevice->device, slot->buffer.bufferView, NULL);
    }
}

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

GR_VOID GR_STDCALL grBeginDescriptorSetUpdate(
    GR_DESCRIPTOR_SET descriptorSet)
{
    LOGT("%p\n", descriptorSet);

    // No-op
}

GR_VOID GR_STDCALL grEndDescriptorSetUpdate(
    GR_DESCRIPTOR_SET descriptorSet)
{
    LOGT("%p\n", descriptorSet);

    // No-op
}

GR_VOID GR_STDCALL grAttachSamplerDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_SAMPLER* pSamplers)
{
    LOGT("%p %u %u %p\n", descriptorSet, startSlot, slotCount, pSamplers);
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grDescriptorSet);

    for (unsigned i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &grDescriptorSet->slots[startSlot + i];
        const GrSampler* grSampler = (GrSampler*)pSamplers[i];

        releaseSlot(grDevice, slot);

        *slot = (DescriptorSetSlot) {
            .type = SLOT_TYPE_IMAGE,
            .image = {
                .imageInfo = {
                    .sampler = grSampler->sampler,
                    .imageView = VK_NULL_HANDLE,
                    .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                },
            },
        };
    }
}

GR_VOID GR_STDCALL grAttachImageViewDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_IMAGE_VIEW_ATTACH_INFO* pImageViews)
{
    LOGT("%p %u %u %p\n", descriptorSet, startSlot, slotCount, pImageViews);
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grDescriptorSet);

    for (unsigned i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &grDescriptorSet->slots[startSlot + i];
        const GR_IMAGE_VIEW_ATTACH_INFO* info = &pImageViews[i];
        const GrImageView* grImageView = (GrImageView*)info->view;

        releaseSlot(grDevice, slot);

        *slot = (DescriptorSetSlot) {
            .type = SLOT_TYPE_IMAGE,
            .image = {
                .imageInfo = {
                    .sampler = VK_NULL_HANDLE,
                    .imageView = grImageView->imageView,
                    .imageLayout = getVkImageLayout(info->state),
                },
            },
        };
    }
}

GR_VOID GR_STDCALL grAttachMemoryViewDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_MEMORY_VIEW_ATTACH_INFO* pMemViews)
{
    LOGT("%p %u %u %p\n", descriptorSet, startSlot, slotCount, pMemViews);
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grDescriptorSet);
    VkResult vkRes;

    for (unsigned i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &grDescriptorSet->slots[startSlot + i];
        const GR_MEMORY_VIEW_ATTACH_INFO* info = &pMemViews[i];
        GrGpuMemory* grGpuMemory = (GrGpuMemory*)info->mem;
        VkFormat vkFormat = getVkFormat(info->format);
        VkBufferView vkBufferView = VK_NULL_HANDLE;

        releaseSlot(grDevice, slot);

        if (vkFormat != VK_FORMAT_UNDEFINED) {
            // Create buffer view for typed buffers
            const VkBufferViewCreateInfo createInfo = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .buffer = grGpuMemory->buffer,
                .format = vkFormat,
                .offset = info->offset,
                .range = info->range,
            };

            vkRes = VKD.vkCreateBufferView(grDevice->device, &createInfo, NULL, &vkBufferView);
            if (vkRes != VK_SUCCESS) {
                LOGE("vkCreateBufferView failed (%d)\n", vkRes);
            }
        }

        *slot = (DescriptorSetSlot) {
            .type = SLOT_TYPE_BUFFER,
            .buffer = {
                .bufferView = vkBufferView,
                .bufferInfo = {
                    .buffer = grGpuMemory->buffer,
                    .offset = info->offset,
                    .range = info->range,
                },
                .stride = info->stride,
            },
        };
    }
}

GR_VOID GR_STDCALL grAttachNestedDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_DESCRIPTOR_SET_ATTACH_INFO* pNestedDescriptorSets)
{
    LOGT("%p %u %u %p\n", descriptorSet, startSlot, slotCount, pNestedDescriptorSets);
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grDescriptorSet);

    for (unsigned i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &grDescriptorSet->slots[startSlot + i];
        const GR_DESCRIPTOR_SET_ATTACH_INFO* info = &pNestedDescriptorSets[i];

        releaseSlot(grDevice, slot);

        *slot = (DescriptorSetSlot) {
            .type = SLOT_TYPE_NESTED,
            .nested = {
                .nextSet = (GrDescriptorSet*)info->descriptorSet,
                .slotOffset = info->slotOffset,
            },
        };
    }
}

GR_VOID GR_STDCALL grClearDescriptorSetSlots(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount)
{
    LOGT("%p %u %u\n", descriptorSet, startSlot, slotCount);
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grDescriptorSet);

    for (unsigned i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &grDescriptorSet->slots[startSlot + i];

        releaseSlot(grDevice, slot);

        slot->type = SLOT_TYPE_NONE;
    }
}
