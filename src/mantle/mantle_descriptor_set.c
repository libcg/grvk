#include "mantle_internal.h"
#include "amdilc.h"
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
    GR_RESULT res = GR_SUCCESS;
    VkBufferCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .size = pCreateInfo->slots * sizeof(uint64_t),
        .queueFamilyIndexCount = 0,
        .flags = 0,
        .pQueueFamilyIndices = NULL,
    };

    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    //TODO: maybe handle memory binding or something
    if (VKD.vkCreateBuffer(grDevice->device, &ci, NULL,
                                   &buffer) != VK_SUCCESS) {
        LOGE("vkCreatebuffer failed\n");
        res = GR_ERROR_OUT_OF_MEMORY;
        goto bail;
    }
    VkMemoryRequirements reqs;
    VKD.vkGetBufferMemoryRequirements(grDevice->device, buffer, &reqs);
    VkMemoryAllocateFlagsInfo allocateFlags = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .pNext = NULL,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
        .deviceMask = 0,
    };
    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &allocateFlags,
        .allocationSize = reqs.size,
        .memoryTypeIndex = grDevice->vDescriptorSetMemoryTypeIndex,
    };
    if (VKD.vkAllocateMemory(grDevice->device, &allocInfo, NULL,//TODO: add custom allocator (enough to radv's VkDescriptorPool) to reduce overhead
                                   &memory) != VK_SUCCESS) {
        LOGE("descriptor allocation failed\n");
        res = GR_ERROR_OUT_OF_GPU_MEMORY;
        goto bail;
    }
    if (VKD.vkBindBufferMemory(grDevice->device, buffer, memory, 0) != VK_SUCCESS) {
        LOGE("descriptor buffer memory bind failed\n");
        res = GR_ERROR_OUT_OF_GPU_MEMORY;
        goto bail;
    }
    DescriptorSetSlot* slots = malloc(sizeof(DescriptorSetSlot) * pCreateInfo->slots);
    DescriptorSetSlot* tempSlots = malloc(sizeof(DescriptorSetSlot) * pCreateInfo->slots);
    memset(slots, 0, sizeof(DescriptorSetSlot) * pCreateInfo->slots);
    memset(tempSlots, 0, sizeof(DescriptorSetSlot) * pCreateInfo->slots);

    VkBufferDeviceAddressInfo bufferAddrInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer,
    };
    GrDescriptorSet* grDescriptorSet = malloc(sizeof(GrDescriptorSet));
    if (grDescriptorSet == NULL) {
        LOGE("descriptor set structure allocation failed\n");
        res = GR_ERROR_OUT_OF_MEMORY;
        goto bail;
    }
    *grDescriptorSet = (GrDescriptorSet) {
        .grObj = { GR_OBJ_TYPE_DESCRIPTOR_SET, grDevice},
        .slots = slots,
        .tempSlots = tempSlots,
        .slotCount = pCreateInfo->slots,
        .virtualDescriptorSet = buffer,
        .boundMemory = memory,
        .bufferDevicePtr = VKD.vkGetBufferDeviceAddress(grDevice->device, &bufferAddrInfo)
    };

    if (VKD.vkMapMemory(grDescriptorSet->grObj.grDevice->device, grDescriptorSet->boundMemory, 0, sizeof(uint64_t) * grDescriptorSet->slotCount, 0, (void**)&grDescriptorSet->boundMemoryHostPtr) != VK_SUCCESS) {
        res = GR_ERROR_OUT_OF_MEMORY;
        goto bail;
    }
    *pDescriptorSet = (GR_DESCRIPTOR_SET)grDescriptorSet;
    return GR_SUCCESS;
bail:
    if (grDescriptorSet != NULL) {
        free(grDescriptorSet);
    }
    if (buffer != VK_NULL_HANDLE) {
        VKD.vkDestroyBuffer(grDevice->device, buffer, NULL);
    }
    if (memory != VK_NULL_HANDLE) {
        VKD.vkFreeMemory(grDevice->device, memory, NULL);
    }
    return res;
}

static unsigned allocateRealDescriptorSet(GrGlobalDescriptorSet* descriptorSet, DescriptorSetSlot* slot)
{
    //TODO: make this atomic or locked at least
    unsigned index = 0xFFFFFFFF;
    if (slot->type == SLOT_TYPE_IMAGE_VIEW) {
        bool* currentPtr = descriptorSet->imagePtr;
        do {
            if (!*descriptorSet->imagePtr) {
                index = descriptorSet->imagePtr - descriptorSet->images;
                *descriptorSet->imagePtr = true;
            }
            if (descriptorSet->imagePtr >= descriptorSet->images + descriptorSet->descriptorCount) {
                descriptorSet->imagePtr = descriptorSet->images;
            }
            else {
                descriptorSet->imagePtr++;
            }
        } while (currentPtr != descriptorSet->imagePtr && index == 0xFFFFFFFF);
    }
    else if (slot->type == SLOT_TYPE_MEMORY_VIEW) {
        bool* currentPtr = descriptorSet->bufferViewPtr;
        do {
            if (!*descriptorSet->bufferViewPtr) {
                index = descriptorSet->bufferViewPtr - descriptorSet->bufferViews;
                *descriptorSet->bufferViewPtr = true;
            }
            if (descriptorSet->bufferViewPtr >= descriptorSet->bufferViews + descriptorSet->descriptorCount) {
                descriptorSet->bufferViewPtr = descriptorSet->bufferViews;
            }
            else {
                descriptorSet->bufferViewPtr++;
            }
        } while (currentPtr != descriptorSet->bufferViewPtr && index == 0xFFFFFFFF);
    }
    else if (slot->type == SLOT_TYPE_SAMPLER) {
        bool* currentPtr = descriptorSet->samplerPtr;
        do {
            if (!*descriptorSet->samplerPtr) {
                index = descriptorSet->samplerPtr - descriptorSet->samplers;
                *descriptorSet->samplerPtr = true;
            }
            if (descriptorSet->samplerPtr >= descriptorSet->samplers + descriptorSet->descriptorCount) {
                descriptorSet->samplerPtr = descriptorSet->samplers;
            }
            else {
                descriptorSet->samplerPtr++;
            }
        } while (currentPtr != descriptorSet->samplerPtr && index == 0xFFFFFFFF);
    }
    return index;//idk
}

GR_VOID grBeginDescriptorSetUpdate(
    GR_DESCRIPTOR_SET descriptorSet)
{
    LOGT("%p\n", descriptorSet);
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;
    // prepare the diff
    memcpy(grDescriptorSet->tempSlots, grDescriptorSet->slots, sizeof(DescriptorSetSlot) * grDescriptorSet->slotCount);
}

GR_VOID grEndDescriptorSetUpdate(
    GR_DESCRIPTOR_SET descriptorSet)
{
    LOGT("%p\n", descriptorSet);
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;
    GrDevice* grDevice = GET_OBJ_DEVICE(grDescriptorSet);
    uint64_t* descriptorSetHostBuffer = (uint64_t*)grDescriptorSet->boundMemoryHostPtr;

    VkWriteDescriptorSet* writeDescriptorSet = (VkWriteDescriptorSet*)malloc(grDescriptorSet->slotCount * sizeof(VkWriteDescriptorSet) * 3);
    VkDescriptorImageInfo* imageInfos = (VkDescriptorImageInfo*)malloc(grDescriptorSet->slotCount * sizeof(VkDescriptorImageInfo));
    VkDescriptorBufferInfo* bufferInfos = (VkDescriptorBufferInfo*)malloc(grDescriptorSet->slotCount * sizeof(VkDescriptorBufferInfo));
    unsigned writeCount = 0;
    unsigned imageInfoCount = 0;
    unsigned bufferInfoCount = 0;
    for (int j = 0; j < grDescriptorSet->slotCount; j++) {
        DescriptorSetSlot* slot = &((DescriptorSetSlot*)grDescriptorSet->slots)[j];
        DescriptorSetSlot* tempSlot = &((DescriptorSetSlot*)grDescriptorSet->tempSlots)[j];
        bool freeExistingSlot = false;
        bool allocSlot = false;
        if (tempSlot->type != slot->type) {
            // allocate descriptor set anyway
            freeExistingSlot = (slot->type != SLOT_TYPE_NESTED && slot->type != SLOT_TYPE_NONE);
            allocSlot = (tempSlot->type != SLOT_TYPE_NESTED && tempSlot->type != SLOT_TYPE_NONE);
        }

        if (freeExistingSlot) {
            switch (slot->type) {
            case SLOT_TYPE_IMAGE_VIEW:
                grDescriptorSet->grObj.grDevice->globalDescriptorSet.images[slot->realDescriptorIndex] = false;
                slot->imageView = VK_NULL_HANDLE;
                break;
            case SLOT_TYPE_MEMORY_VIEW:
                if (slot->bufferView != VK_NULL_HANDLE) {
                    VKD.vkDestroyBufferView(grDescriptorSet->grObj.grDevice->device, slot->bufferView, NULL);
                    slot->bufferView = VK_NULL_HANDLE;
                    slot->bufferViewCreateInfo.buffer = VK_NULL_HANDLE;
                }
                grDescriptorSet->grObj.grDevice->globalDescriptorSet.bufferViews[slot->realDescriptorIndex] = false;
                break;
            case SLOT_TYPE_SAMPLER:
                grDescriptorSet->grObj.grDevice->globalDescriptorSet.samplers[slot->realDescriptorIndex] = false;
                slot->sampler = VK_NULL_HANDLE;
                break;
            default:
                break;
            }
        }

        slot->type = tempSlot->type;
        if (allocSlot) {
            slot->realDescriptorIndex = allocateRealDescriptorSet(&grDescriptorSet->grObj.grDevice->globalDescriptorSet, slot);
            descriptorSetHostBuffer[j] = (uint64_t)slot->realDescriptorIndex;
        }
        if (slot->type == SLOT_TYPE_NESTED) {
            slot->nestedDescriptorSet = tempSlot->nestedDescriptorSet;
            descriptorSetHostBuffer[j] = slot->nestedDescriptorSet;
        }
        else if (slot->type == SLOT_TYPE_IMAGE_VIEW) {
            slot->nestedDescriptorSet = 0;
            //update descriptor set
            if (slot->imageView != tempSlot->imageView || slot->imageLayout != tempSlot->imageLayout) {
                imageInfos[imageInfoCount] = (VkDescriptorImageInfo) {
                    .sampler = VK_NULL_HANDLE,
                    .imageView = tempSlot->imageView,
                    .imageLayout = tempSlot->imageLayout,
                };
                if (tempSlot->imageUsage & VK_IMAGE_USAGE_STORAGE_BIT) {
                    writeDescriptorSet[writeCount++] = (VkWriteDescriptorSet) {
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .pNext = NULL,
                        .dstSet = grDescriptorSet->grObj.grDevice->globalDescriptorSet.descriptorTable,
                        .dstBinding = TABLE_STORAGE_IMAGE,
                        .dstArrayElement = slot->realDescriptorIndex,
                        .descriptorCount = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                        .pImageInfo = &imageInfos[imageInfoCount],
                        .pBufferInfo = NULL,
                        .pTexelBufferView = NULL,
                    };
                }
                if (tempSlot->imageUsage & VK_IMAGE_USAGE_SAMPLED_BIT) {
                    writeDescriptorSet[writeCount++] = (VkWriteDescriptorSet) {
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .pNext = NULL,
                        .dstSet = grDescriptorSet->grObj.grDevice->globalDescriptorSet.descriptorTable,
                        .dstBinding = TABLE_SAMPLED_IMAGE,
                        .dstArrayElement = slot->realDescriptorIndex,
                        .descriptorCount = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                        .pImageInfo = &imageInfos[imageInfoCount],
                        .pBufferInfo = NULL,
                        .pTexelBufferView = NULL,
                    };
                }
                imageInfoCount++;
            }

            slot->imageView = tempSlot->imageView;
            slot->imageLayout = tempSlot->imageLayout;
            slot->imageUsage = tempSlot->imageUsage;
        }
        else if (slot->type == SLOT_TYPE_MEMORY_VIEW) {
            slot->nestedDescriptorSet = 0;
            if (slot->bufferView == VK_NULL_HANDLE || memcmp(&slot->bufferViewCreateInfo, &tempSlot->bufferViewCreateInfo, sizeof(VkBufferViewCreateInfo)) != 0) {
                if (slot->bufferView != VK_NULL_HANDLE) {
                    VKD.vkDestroyBufferView(grDescriptorSet->grObj.grDevice->device, slot->bufferView, NULL);
                }
                memcpy(&slot->bufferViewCreateInfo, &tempSlot->bufferViewCreateInfo, sizeof(VkBufferViewCreateInfo));
                if (tempSlot->bufferViewCreateInfo.format != VK_FORMAT_UNDEFINED) {
                    if (VKD.vkCreateBufferView(grDescriptorSet->grObj.grDevice->device, &tempSlot->bufferViewCreateInfo, NULL,
                                               &tempSlot->bufferView) != VK_SUCCESS) {
                        LOGE("vkCreateBufferView failed\n");
                        assert(false);
                    }
                    if (tempSlot->bufferUsage & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT) {
                        writeDescriptorSet[writeCount++] = (VkWriteDescriptorSet) {
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .pNext = NULL,
                            .dstSet = grDescriptorSet->grObj.grDevice->globalDescriptorSet.descriptorTable,
                            .dstBinding = TABLE_STORAGE_TEXEL_BUFFER,
                            .dstArrayElement = slot->realDescriptorIndex,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
                            .pImageInfo = NULL,
                            .pBufferInfo = NULL,
                            .pTexelBufferView = &tempSlot->bufferView,
                        };
                    }
                    writeDescriptorSet[writeCount++] = (VkWriteDescriptorSet) {
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .pNext = NULL,
                        .dstSet = grDescriptorSet->grObj.grDevice->globalDescriptorSet.descriptorTable,
                        .dstBinding = TABLE_UNIFORM_TEXEL_BUFFER,
                        .dstArrayElement = slot->realDescriptorIndex,
                        .descriptorCount = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                        .pImageInfo = NULL,
                        .pBufferInfo = NULL,
                        .pTexelBufferView = &tempSlot->bufferView,
                    };
                }
                if (tempSlot->bufferUsage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
                    bufferInfos[bufferInfoCount] = (VkDescriptorBufferInfo) {
                        .buffer = tempSlot->bufferViewCreateInfo.buffer,
                        .offset = tempSlot->bufferViewCreateInfo.offset,
                        .range  = tempSlot->bufferViewCreateInfo.range,
                    };
                    writeDescriptorSet[writeCount++] = (VkWriteDescriptorSet) {
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .pNext = NULL,
                        .dstSet = grDescriptorSet->grObj.grDevice->globalDescriptorSet.descriptorTable,
                        .dstBinding = TABLE_STORAGE_BUFFER,
                        .dstArrayElement = slot->realDescriptorIndex,
                        .descriptorCount = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        .pImageInfo = NULL,
                        .pBufferInfo = &bufferInfos[bufferInfoCount],
                        .pTexelBufferView = NULL,
                    };
                    bufferInfoCount++;
                }
                slot->bufferView = tempSlot->bufferView;
            }
        }
        else if (slot->type == SLOT_TYPE_SAMPLER) {
            slot->nestedDescriptorSet = 0;
            imageInfos[imageInfoCount] = (VkDescriptorImageInfo) {
                .sampler = tempSlot->sampler,
                .imageView = VK_NULL_HANDLE,
                .imageLayout = VK_NULL_HANDLE,
            };
            writeDescriptorSet[writeCount++] = (VkWriteDescriptorSet) {

                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = NULL,
                .dstSet = grDescriptorSet->grObj.grDevice->globalDescriptorSet.descriptorTable,
                .dstBinding = TABLE_SAMPLER,
                .dstArrayElement = slot->realDescriptorIndex,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                .pImageInfo = &imageInfos[imageInfoCount],
                .pBufferInfo = NULL,
                .pTexelBufferView = NULL,
            };
            imageInfoCount++;
            slot->sampler = tempSlot->sampler;
        }
    }

    VKD.vkUpdateDescriptorSets(grDescriptorSet->grObj.grDevice->device, writeCount, writeDescriptorSet,
                               0, NULL);
    free(writeDescriptorSet);
    free(imageInfos);
    free(bufferInfos);
}

GR_VOID grAttachSamplerDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_SAMPLER* pSamplers)
{
    LOGT("%p %u %u %p\n", descriptorSet, startSlot, slotCount, pSamplers);
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;

    for (int i = 0; i < slotCount; i++) {
        DescriptorSetSlot* tempSlot = &((DescriptorSetSlot*)grDescriptorSet->tempSlots)[startSlot + i];
        const GrSampler* sampler = (GrSampler*)pSamplers[i];
        if (sampler != NULL) {
            tempSlot->sampler = sampler->sampler;
        }
        else {
            tempSlot->sampler = VK_NULL_HANDLE;
        }
        tempSlot->type = SLOT_TYPE_SAMPLER;
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

    for (int i = 0; i < slotCount; i++) {
        DescriptorSetSlot* tempSlot = &((DescriptorSetSlot*)grDescriptorSet->tempSlots)[startSlot + i];
        tempSlot->imageLayout = getVkImageLayout(pImageViews[i].state);
        const GrImageView* imageView = (GrImageView*)pImageViews[i].view;
        if (imageView == NULL) {//don't want to cause segfault
            tempSlot->imageView = VK_NULL_HANDLE;
        }
        else {
            tempSlot->imageView = imageView->imageView;
            tempSlot->imageUsage = imageView->usage;
        }
        tempSlot->type = SLOT_TYPE_IMAGE_VIEW;
    }
}

VkBufferUsageFlags getVkBufferUsages(GR_MEMORY_STATE state) {
    switch (state) {
    case GR_MEMORY_STATE_GRAPHICS_SHADER_READ_ONLY:
    case GR_MEMORY_STATE_COMPUTE_SHADER_READ_ONLY:
        return VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    default:
        return VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
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

    for (int i = 0; i < slotCount; i++) {
        DescriptorSetSlot* tempSlot = &((DescriptorSetSlot*)grDescriptorSet->tempSlots)[startSlot + i];
        const GR_MEMORY_VIEW_ATTACH_INFO* attachInfo = pMemViews + i;
        GrGpuMemory* grGpuMemory = (GrGpuMemory*)attachInfo->mem;
        tempSlot->type = SLOT_TYPE_MEMORY_VIEW;
        grGpuMemoryBindBuffer(grGpuMemory);
        tempSlot->bufferUsage = getVkBufferUsages(attachInfo->state);
        tempSlot->bufferViewCreateInfo = (VkBufferViewCreateInfo){
            .sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .buffer = grGpuMemory->buffer,
            .format = getVkFormat(attachInfo->format),
            .offset = attachInfo->offset,
            .range = attachInfo->range,
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

    for (int i = 0; i < slotCount; i++) {
        DescriptorSetSlot* tempSlot = &((DescriptorSetSlot*)grDescriptorSet->tempSlots)[startSlot + i];
        const GrDescriptorSet* nestedSet = (GrDescriptorSet*) pNestedDescriptorSets[i].descriptorSet;
        tempSlot->type = SLOT_TYPE_NESTED;
        tempSlot->nestedDescriptorSet = nestedSet->bufferDevicePtr + sizeof(uint64_t) * pNestedDescriptorSets[i].slotOffset;
    }
}

GR_VOID grClearDescriptorSetSlots(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount)
{
    LOGT("%p %u %u\n", descriptorSet, startSlot, slotCount);
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;

    for (int i = 0; i < slotCount; i++) {
        DescriptorSetSlot* tempSlot = &((DescriptorSetSlot*)grDescriptorSet->tempSlots)[startSlot + i];

        tempSlot->type = SLOT_TYPE_NONE;
        tempSlot->bufferView = VK_NULL_HANDLE;
        tempSlot->nestedDescriptorSet = 0;
    }
}
