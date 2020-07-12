#include "mantle_internal.h"

typedef enum _DescriptorSetSlotType
{
    SLOT_TYPE_NONE,
    SLOT_TYPE_IMAGE_VIEW,
    SLOT_TYPE_MEMORY_VIEW,
    SLOT_TYPE_SAMPLER,
    SLOT_TYPE_NESTED,
} DescriptorSetSlotType;

typedef struct _DescriptorSetSlot
{
    DescriptorSetSlotType type;
    void* info;
} DescriptorSetSlot;

static void clearDescriptorSetSlot(
    DescriptorSetSlot* slot)
{
    free(slot->info);
    slot->type = SLOT_TYPE_NONE;
    slot->info = NULL;
}

static void setDescriptorSetSlot(
    DescriptorSetSlot* slot,
    DescriptorSetSlotType type,
    const void* data,
    uint32_t size)
{
    clearDescriptorSetSlot(slot);

    slot->type = type;
    slot->info = malloc(size);
    memcpy(slot->info, data, size);
}

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

    DescriptorSetSlot* slots = malloc(sizeof(DescriptorSetSlot) * pCreateInfo->slots);
    for (int i = 0; i < pCreateInfo->slots; i++) {
        slots[i] = (DescriptorSetSlot) {
            .type = SLOT_TYPE_NONE,
            .info = NULL,
        };
    }

    GrvkDescriptorSet* grvkDescriptorSet = malloc(sizeof(GrvkDescriptorSet));
    *grvkDescriptorSet = (GrvkDescriptorSet) {
        .sType = GRVK_STRUCT_TYPE_DESCRIPTOR_SET,
        .descriptorPool = vkDescriptorPool,
        .slots = slots,
        .slotCount = pCreateInfo->slots,
    };

    *pDescriptorSet = (GR_DESCRIPTOR_SET)grvkDescriptorSet;
    return GR_SUCCESS;
}

GR_VOID grAttachSamplerDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_SAMPLER* pSamplers)
{
    GrvkDescriptorSet* grvkDescriptorSet = (GrvkDescriptorSet*)descriptorSet;

    for (int i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &((DescriptorSetSlot*)grvkDescriptorSet->slots)[startSlot + i];

        setDescriptorSetSlot(slot, SLOT_TYPE_SAMPLER, &pSamplers[i], sizeof(GR_SAMPLER));
    }
}

GR_VOID grAttachImageViewDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_IMAGE_VIEW_ATTACH_INFO* pImageViews)
{
    GrvkDescriptorSet* grvkDescriptorSet = (GrvkDescriptorSet*)descriptorSet;

    for (int i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &((DescriptorSetSlot*)grvkDescriptorSet->slots)[startSlot + i];

        setDescriptorSetSlot(slot, SLOT_TYPE_IMAGE_VIEW,
                             &pImageViews[i], sizeof(GR_IMAGE_VIEW_ATTACH_INFO));
    }
}

GR_VOID grAttachMemoryViewDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_MEMORY_VIEW_ATTACH_INFO* pMemViews)
{
    GrvkDescriptorSet* grvkDescriptorSet = (GrvkDescriptorSet*)descriptorSet;

    for (int i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &((DescriptorSetSlot*)grvkDescriptorSet->slots)[startSlot + i];

        setDescriptorSetSlot(slot, SLOT_TYPE_MEMORY_VIEW,
                             &pMemViews[i], sizeof(GR_MEMORY_VIEW_ATTACH_INFO));
    }
}

GR_VOID grAttachNestedDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_DESCRIPTOR_SET_ATTACH_INFO* pNestedDescriptorSets)
{
    GrvkDescriptorSet* grvkDescriptorSet = (GrvkDescriptorSet*)descriptorSet;

    for (int i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &((DescriptorSetSlot*)grvkDescriptorSet->slots)[startSlot + i];

        setDescriptorSetSlot(slot, SLOT_TYPE_NESTED,
                             &pNestedDescriptorSets[i], sizeof(GR_DESCRIPTOR_SET_ATTACH_INFO));
    }
}

GR_VOID grClearDescriptorSetSlots(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount)
{
    GrvkDescriptorSet* grvkDescriptorSet = (GrvkDescriptorSet*)descriptorSet;

    for (int i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &((DescriptorSetSlot*)grvkDescriptorSet->slots)[startSlot + i];

        clearDescriptorSetSlot(slot);
    }
}
