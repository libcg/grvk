#include "mantle_internal.h"

static void clearDescriptorSetSlot(
    GrDevice* grDevice,
    DescriptorSetSlot* slot)
{
    if (slot->type == SLOT_TYPE_MEMORY_VIEW) {
        VKD.vkDestroyBufferView(grDevice->device, slot->memoryView.vkBufferView, NULL);
    }

    slot->type = SLOT_TYPE_NONE;
}

// Descriptor Set Functions

GR_RESULT grCreateDescriptorSet(
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
    GrDevice* grDevice = GET_OBJ_DEVICE(grDescriptorSet);

    LOGW("unhandled sampler descriptors\n");

    for (unsigned i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &grDescriptorSet->slots[startSlot + i];

        clearDescriptorSetSlot(grDevice, slot);
        // TODO implement
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
    GrDevice* grDevice = GET_OBJ_DEVICE(grDescriptorSet);

    LOGW("unhandled image view descriptors\n");

    for (unsigned i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &grDescriptorSet->slots[startSlot + i];

        clearDescriptorSetSlot(grDevice, slot);
        // TODO implement
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
    GrDevice* grDevice = GET_OBJ_DEVICE(grDescriptorSet);

    for (unsigned i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &grDescriptorSet->slots[startSlot + i];
        const GR_MEMORY_VIEW_ATTACH_INFO* info = &pMemViews[i];
        GrGpuMemory* grGpuMemory = (GrGpuMemory*)info->mem;
        VkBufferView vkBufferView = VK_NULL_HANDLE;
        VkResult res;

        grGpuMemoryBindBuffer(grGpuMemory);

        // Mantle doesn't have memory view objects, create a buffer view
        // FIXME what is info->state for?
        const VkBufferViewCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .buffer = grGpuMemory->buffer,
            .format = getVkFormat(info->format),
            .offset = info->offset,
            .range = info->range,
        };

        res = VKD.vkCreateBufferView(grDevice->device, &createInfo, NULL, &vkBufferView);
        if (res != VK_SUCCESS) {
            LOGE("vkCreateBufferView failed (%d)\n", res);
        }

        clearDescriptorSetSlot(grDevice, slot);

        *slot = (DescriptorSetSlot) {
            .type = SLOT_TYPE_MEMORY_VIEW,
            .memoryView.vkBufferView = vkBufferView,
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
    GrDevice* grDevice = GET_OBJ_DEVICE(grDescriptorSet);

    LOGW("unhandled nested descriptors\n");

    for (unsigned i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &grDescriptorSet->slots[startSlot + i];

        clearDescriptorSetSlot(grDevice, slot);
        // TODO implement
    }
}

GR_VOID grClearDescriptorSetSlots(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount)
{
    LOGT("%p %u %u\n", descriptorSet, startSlot, slotCount);
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;
    GrDevice* grDevice = GET_OBJ_DEVICE(grDescriptorSet);

    for (unsigned i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &grDescriptorSet->slots[startSlot + i];

        clearDescriptorSetSlot(grDevice, slot);
    }
}
