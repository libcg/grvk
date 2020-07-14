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
        .device = grvkDevice->device,
        .descriptorPool = vkDescriptorPool,
        .slots = slots,
        .slotCount = pCreateInfo->slots,
        .descriptorSets = { VK_NULL_HANDLE },
    };

    *pDescriptorSet = (GR_DESCRIPTOR_SET)grvkDescriptorSet;
    return GR_SUCCESS;
}

GR_VOID grBeginDescriptorSetUpdate(
    GR_DESCRIPTOR_SET descriptorSet)
{
    // TODO keep track of diff?
}

GR_VOID grEndDescriptorSetUpdate(
    GR_DESCRIPTOR_SET descriptorSet)
{
    GrvkDescriptorSet* grvkDescriptorSet = (GrvkDescriptorSet*)descriptorSet;

    // TODO free old descriptor sets and layouts if applicable

    VkDescriptorSetLayout vkLayouts[MAX_STAGE_COUNT];
    for (int i = 0; i < MAX_STAGE_COUNT; i++) {
        VkDescriptorSetLayoutBinding* bindings =
            malloc(sizeof(VkDescriptorSetLayoutBinding) * grvkDescriptorSet->slotCount);

        for (int j = 0; j < grvkDescriptorSet->slotCount; j++) {
            const DescriptorSetSlot* slot = &((DescriptorSetSlot*)grvkDescriptorSet->slots)[j];

            if (slot->type == SLOT_TYPE_NESTED ||
                slot->type == SLOT_TYPE_IMAGE_VIEW ||
                slot->type == SLOT_TYPE_SAMPLER) {
                // TODO support other types
                printf("%s: unsupported slot type %d\n", __func__, slot->type);
            }

            if (slot->type == SLOT_TYPE_NONE || slot->type == SLOT_TYPE_NESTED) {
                bindings[j] = (VkDescriptorSetLayoutBinding) {
                    .binding = j, // Ignored
                    .descriptorType = 0,
                    .descriptorCount = 0,
                    .stageFlags = 0,
                    .pImmutableSamplers = NULL,
                };
            } else {
                if (slot->type == SLOT_TYPE_MEMORY_VIEW) {
                    GR_MEMORY_VIEW_ATTACH_INFO* info = (GR_MEMORY_VIEW_ATTACH_INFO*)slot->info;

                    // TODO support other states
                    if (info->state != GR_MEMORY_STATE_GRAPHICS_SHADER_READ_ONLY) {
                        printf("%s: unsupported memory state 0x%x\n", __func__, info->state);
                    }
                }

                bindings[j] = (VkDescriptorSetLayoutBinding) {
                    .binding = j, // Ignored
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 1,
                    .stageFlags = getVkShaderStageFlags(i),
                    .pImmutableSamplers = NULL,
                };
            }
        }

        VkDescriptorSetLayoutCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .bindingCount = grvkDescriptorSet->slotCount,
            .pBindings = bindings,
        };

        vkCreateDescriptorSetLayout(grvkDescriptorSet->device, &createInfo, NULL, &vkLayouts[i]);

        free(bindings);
    }

    const VkDescriptorSetAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = NULL,
        .descriptorPool = grvkDescriptorSet->descriptorPool,
        .descriptorSetCount = MAX_STAGE_COUNT,
        .pSetLayouts = vkLayouts,
    };

    vkAllocateDescriptorSets(grvkDescriptorSet->device, &allocateInfo,
                             grvkDescriptorSet->descriptorSets);

    // TODO update descriptor sets
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
